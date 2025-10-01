#ifndef DATASET_HOSTPACK_H
#define DATASET_HOSTPACK_H
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace dataset_core
{
    struct AlignCfg
    {
        uint32_t row_align;
        uint32_t block_align;
    };

    enum class PixelFormat : uint32_t { RGBA8 = 1, RGBA32F = 2 };

    struct BuildOptions
    {
        PixelFormat format;
        AlignCfg align;
    };

    struct Hdr
    {
        char magic[4];
        uint32_t version;
        uint32_t flags;
        uint32_t reserved;
        uint64_t scene_off;
        uint64_t camera_off;
        uint64_t frames_off;
        uint64_t images_off;
        uint64_t pixels_off;
        uint64_t end_off;
    };

    struct SceneRec
    {
        float aabb_min[3];
        float aabb_max[3];
        uint32_t color_space;
        uint32_t reserved;
    };

    struct CameraSOA
    {
        uint32_t count;
        uint64_t fx_off;
        uint64_t fy_off;
        uint64_t cx_off;
        uint64_t cy_off;
        uint64_t T_off;
        uint64_t w_off;
        uint64_t h_off;
        uint64_t time_off;
    };

    struct FrameRec
    {
        uint32_t camera_id;
        uint32_t mip_levels;
        uint32_t reserved0;
        uint32_t reserved1;
        uint64_t pixel_off;
        uint32_t width;
        uint32_t height;
        uint32_t row_stride;
        uint32_t pixel_stride;
        uint32_t roi_x;
        uint32_t roi_y;
        uint32_t roi_w;
        uint32_t roi_h;
    };

    struct HostIndex
    {
        Hdr hdr;
        SceneRec scene;
        CameraSOA cam;
        std::vector<FrameRec> frames;
    };

    struct ImageBufferU8
    {
        std::vector<unsigned char> data;
        int w;
        int h;
        int c;
    };

    struct ImageBufferF32
    {
        std::vector<float> data;
        int w;
        int h;
        int c;
    };

    struct NerfTransforms
    {
        struct Item
        {
            std::string path;
            float T[16];
        };

        int w;
        int h;
        float fx;
        float fy;
        float cx;
        float cy;
        std::vector<Item> items;
    };

    int build_hostpack_nerf_synthetic(const std::string& root, const std::string& out_path, const BuildOptions& opt);
    HostIndex open_hostpack(const std::string& path);
    size_t hostpack_frame_count(const HostIndex& idx);
    const FrameRec& hostpack_frame(const HostIndex& idx, size_t i);
    void* hostpack_mmap_ro(const std::string& path, size_t& bytes);
    void hostpack_munmap(void* p, size_t bytes);
}
#endif //DATASET_HOSTPACK_H
