# Dataset-Core 部署与集成指南 v2（HostPack 中心设计）

> 读者：Xayah（开发者）与 AI Agent

---

## 1. 目标与设计摘要

* 最高吞吐优先的 Dataset I/O 架构，用于 NeRF / 3DGS 等训练与推理。
* 核心分层：

    * **dataset-core（必须）**：只负责解析数据源并生成 **HostPack**；运行期提供 **Host 侧只读视图**（mmap）。无任何 GPU/RT 依赖。
    * **adapters-***（可选插件）：将 HostPack 映射/上传为各后端原生资源（CUDA、OptiX、Vulkan、CPU）。上下文/流/设备句柄由外部注入。
* 运行期 Profile：

    * **INGP-Classic（默认）**：对齐 instant-ngp 的热路径，保证同量级吞吐。
    * **Max-Offload（可选）**：开启 RayTable、空间/时间 MIP、SAT、光流等增强能力，用于视频/动态/高质量重采样。

---

## 2. 工程布局与产物

* 仓库建议：

    * `dataset-core/`：核心库与 CLI（datasetc）。
    * `adapters-cuda/`：CUDA 上传与纹理/线性缓冲适配。
    * `adapters-optix/`：OptiX 资源适配（在外部注入的 CUcontext/OptixDeviceContext 上工作）。
    * `adapters-vulkan/`：VkImage/VkBuffer 填充与可选 CUDA 互操作。
    * `adapters-cpu/`：多线程批次迭代与可选 SIMD 重采样。
    * `tests/`：单测与一致性校验。
    * `benchmarks/`：端到端与微基准。
* 产物：

    * HostPack 二进制文件（mmap 友好，64B 对齐）。
    * Host 侧只读视图句柄（结构/像素/索引）。
    * TransferDescriptor（供 adapters 生成目标资源）。

---

## 3. 支持的数据源与输入契约

* NeRF-synthetic：`transforms*.json` + `images/`。
* COLMAP：`cameras.txt` / `images.txt` / `points3D.*` + `images/`。
* LLFF：`poses_bounds.npy` + `images/`。
* MipNeRF360 / Tanks&Temples / DTU / Replica：原生格式或经 COLMAP 归一。
* 视频：容器文件（mp4/mkv 等）或帧目录（含时间戳）。
* 可选 `manifest.json`：覆盖/补充分辨率、裁剪、颜色空间、坐标系与尺度、采样策略建议等。

---

## 4. HostPack 结构与对齐（核心）

* 全局 Header：magic、version、endianness、capabilities bitset、对齐粒度、段表、校验。
* Scene：坐标系约定、尺度、AABB、颜色空间（srgb/linear）、up 轴、时间基。
* Camera SoA：`fx[] fy[] cx[] cy[]`、畸变参数向量、`T_world_from_cam(3x4)[]`、`width[] height[] time[]`，16B 对齐。
* Frames 索引：frame→camera 映射、图像偏移、ROI、padded_wh、mip 层计数。
* Images：已解码并线性化的像素块，统一为 `rgba8` 或 `rgba32f`；按行对齐（建议 ≥256B），帧顺序写入。
* Pyramids（可选）：空间 MIP 链；与 Images 同步索引。
* SAT（可选）：summed-area table；用于盒核快速积分。
* Temporal（可选）：时间 MIP、相邻帧光流（双向）。
* RayTable（可选）：全量或多分辨率 `o,d,near,far`；`float32` 或 `float16`。
* Attachments（可选）：mask/depth/normal/semantic 等，与图像对齐。
* TransferDescriptor：每段的偏移、尺寸、步长、格式、分段信息、建议目标资源类型（array/linear）。

约束与建议：

* 层宽高不一致的图像需统一 resize 或 pad；记录 `roi` 与 `padded_wh`，供设备侧归一化采样。
* 帧数超出单个 2D array 限制时，按段切分并在 TransferDescriptor 中列出段表。
* 颜色空间统一：HostPack 内像素已做 srgb→linear，训练/推理默认最近邻；若需滤波在适配层或渲染器实现。

---

## 5. Host 侧只读视图与 API

* HostIndex：场景、相机、帧表、能力位、对齐信息。
* HostImageView：`base pointer` + `offset/size` + `w/h/channels/format` + `row_stride/pixel_stride` + `roi/padded_wh`。
* HostCameraSOA：指向 SoA 缓冲的只读指针与条目数。
* HostRayTableView（可选）。
* 线程安全：所有视图为不可变，mmap 映射后可被多线程并发读取。

---

## 6. adapters 设计与通用约束

* 适配层职责：将 HostPack 的像素与结构映射为目标后端的原生资源。
* 必须行为：

    * 不创建上层上下文；由外部传入 `CUcontext/cudaStream_t`、`OptixDeviceContext`、`VkDevice` 等。
    * 严格使用 TransferDescriptor 描述的偏移/步长/对齐进行一次性上传；训练期零 HtoD。
    * 提供查询接口：设备端内存占用、资源句柄、分段信息、上限能力。
* 典型适配层：

    * `adapters-cuda`：生成 `cudaArray_t/cudaTextureObject_t` 或线性 `CUdeviceptr`；支持页锁定 staging、多流 `cudaMemcpyAsync`，可选 GDS。
    * `adapters-optix`：在现有 `OptixDeviceContext` 上创建/注册资源，返回可用于 SBT/params 的句柄。
    * `adapters-vulkan`：分配 `VkImage/VkBuffer` 并填充；必要时支持与 CUDA 互操作。
    * `adapters-cpu`：批次迭代与可选 SIMD 重采样，零拷贝读取 HostPack。

---

## 7. 运行期 Profile

### INGP-Classic（默认）

* HostPack：仅解码与线性化；不生成 SAT/时间 MIP/光流/全量 RayTable。
* 适配：图像→`uchar4` 的 2D array texture；相机 SoA 常驻；训练内核内反投影生成射线；CUDA Graph 包裹整步。
* 目标：在相同硬件上与 instant-ngp 同量级吞吐（±3% 以内）。

### Max-Offload（可选）

* HostPack：开启 RayTable、空间/时间 MIP、SAT、光流与附件。
* 适配：加载对应结构，支持运动补偿与高质量重采样；视频/动态/高质量推理优先。

---

## 8. 部署步骤

1. 构建 dataset-core（开启所需编解码后端），得到 `libdataset_core` 与 CLI `datasetc`。
2. 使用 `datasetc` 将原始数据编译为 HostPack：

    * 指定像素格式、行对齐、是否生成 MIP/SAT/Temporal/Ray/Attachments。
    * 校验与报表：帧数、分辨率分布、对齐结果、容量估算、能力位。
3. 运行期加载 HostPack：

    * mmap HostPack，获得 HostIndex 与只读视图。
    * 选择适配层并注入外部上下文/流/设备，在启动阶段一次性创建目标资源。
    * 将资源句柄交给渲染/训练管线；训练期不再做 HtoD。

---

## 9. 与 instant-ngp/OptiX 的集成指引

* 不在 dataset-core 中创建 CUDA/OptiX 上下文；与渲染器共用外部上下文。
* 使用同一 `cudaStream_t` 与事件进行跨模块同步；由上层录制 CUDA Graph 包裹完整 step。
* 在 OptiX 路径下，可选择：

    * 纹理采样：使用 `cudaTextureObject_t`，在 raygen/closesthit 中直接采样。
    * 线性采样：使用线性 `CUdeviceptr` 并在程序中自定义采样器（便于可控访存）。
* 当帧数过大：通过段表在设备侧定位数组段与层；避免 CPU 进行帧分发。
* 不等分辨率：使用 `roi/padded_wh` 进行设备侧 UV 归一化，避免插值污染或越界。

---

## 10. 运行期采样与重采样（语义）

* 设备侧采样器建议由渲染器/训练器实现，dataset-core 只提供 Host 元数据：

    * 射线采样：uniform/stratified/by-frame/by-mask/by-importance/by-occupancy。
    * 像素重采样（空间）：mip 或 SAT 支持的盒/高斯近似。
    * 时间重采样：nearest/linear/flow-warp（如加载了时间结构）。
    * 补丁采样：按 patch 尺寸与步长在设备侧生成索引。
* adapters-cpu 可提供等价 CPU 采样实现，接口一致，便于 A/B 与回退。

---

## 11. 构建与依赖

* dataset-core：C++23；必需 `simdjson`；按需启用 `libjpeg-turbo`、`libspng+libdeflate`、`OpenEXR`、`FFmpeg`；无 CUDA 依赖。
* adapters：各自根据后端引入 CUDA/OptiX/Vulkan 等；全部通过外部注入上下文。
* CI 矩阵：Windows/Linux × GCC/Clang/VS × CUDA 12.4/12.8/13.0（仅适配层）。

---

## 12. 性能与验收

* 基准数据：NeRF-synthetic `lego`；真实集 `fern` 或 `mip360-garden`。
* 关键指标：rays/s、ms/step（1k+ 步平均）、显存峰值、HtoD 字节数（应为 0）、L2/TEX 命中、DRAM 带宽、最终 PSNR/SSIM。
* 验收阈值：

    * INGP-Classic 下与 instant-ngp 对比偏差 ≤ ±3%。
    * 运行期 HtoD=0；显存与质量与基线同量级。
* A/B 项：

    * texture2D array vs 线性缓冲 gather；
    * 内核反投影 vs RayTable（fp32/fp16）；
    * CUDA Graph 开/关；
    * 分块随机 vs 全随机索引。

---

## 13. 可观测性

* 计数器：ms/step、rays/s、HtoD 字节、显存峰值、L2/TEX 命中、DRAM 读量、kernel 数、graph 重放次数。
* 事件：HostPack 装载时长、校验通过/失败、回退路径触发。
* 报表：CSV/JSON 导出；Nsight 标记范围（适配层/渲染器侧）。

---

## 14. 风险与对策

* 显存压力：默认 Classic；RayTable 与附件默认关闭；优先 fp16。
* 上下文错配：适配层 API 强制外部注入；调试期开启上下文校验。
* 不等分辨率：统一 pad 并记录 ROI；设备侧 UV 归一化工具函数。
* 帧数限制：分段数组与段表索引；适配层内联定位逻辑。
* 编解码兼容：组件化开关；CI 覆盖常见库版本与平台。

---

## 15. Roadmap 与 Milestone

### M0 Skeleton（Host 核心）

* 内容：最小 IR；NeRF-synthetic Loader；HostPack 生成（解码+线性化）；mmap 与 Host 视图；datasetc CLI。
* 验收：lego 编译成功；一致性校验通过；mmap 读取在 8 线程下 saturate NVMe 顺序带宽。

### M1 INGP-Classic Baseline

* 内容：adapters-cuda 基线上传（`uchar4` array + SoA）；分块随机索引器示例；CUDA Graph 脚本。
* 验收：与 instant-ngp 吞吐/延迟偏差 ≤ ±3%；HtoD=0；质量一致。

### M2 Coverage+Attachments

* 内容：LLFF/COLMAP/MipNeRF360 Loader；Attachments 通道；空间 MIP 生成。
* 验收：三类数据源均能编译为 HostPack 并在 Classic 下训练；附件存在时吞吐退化 < 3%。

### M3 RayTable（可选）

* 内容：fp16/fp32 Ray 表生成与 HostPack 存储；adapters-cuda 直读；API 等价替换。
* 验收：显存充裕卡上持平或小优于反投影路径；质量一致。

### M4 Temporal/Flow（可选）

* 内容：时间 MIP 与相邻光流；flow-warp 时间重采样语义。
* 验收：视频/动态集正确性与吞吐评估；默认关闭不影响 Classic。

### M5 OptiX/Vulkan 适配

* 内容：adapters-optix 与 adapters-vulkan；跨 API 同步与互操作示例。
* 验收：在共享 CUcontext/stream 下零隐式同步；可视化与训练互不拖慢。

### M6 Tooling & CI

* 内容：CLI 统计与报告；回归基准；配置快照与变更审计。
* 验收：CI 稳定产出基准并具备回归报警阈值。

---

## 16. 发布与合规

* 发布包：`libdataset_core`、`datasetc`、可选适配层二进制与头文件、样例脚本与基准报告。
* 版本：语义化版本；HostPack `version` 与 capabilities bitset 增量演进；提供向后兼容读取。
* 许可与第三方：保留依赖许可证与致谢；适配层按后端许可分发。

---

## 17. AI Agent 执行提示

* 优先落地 M0→M1，确保 Classic 路径与 instant-ngp 对齐达标。
* 生成代码遵循 ASCII-only、无注释；解释性内容写入文档。
* 每一里程碑交付：可运行 demo、基准报告、阈值检查脚本、变更记录。

---

附录A：对齐与步长建议

* 行对齐建议 ≥256B；像素步长遵循通道对齐；块对齐 64B；Ray 表对齐 16B。

附录B：能力位示例

* bits：has_pyramids, has_sat, has_temporal, has_flow, has_raytable, has_attachments, segmented_arrays。

附录C：错误码建议

* out_of_memory, file_corrupt, version_mismatch, context_mismatch, device_mismatch, array_layer_overflow, alignment_error。
