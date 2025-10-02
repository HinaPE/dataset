#include "dataset.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <atomic>
#include <thread>
#include <cstring>
#include <cmath>
#include <simdjson.h>
#include <spng.h>
#include "mmio.h"
namespace fs = std::filesystem;

namespace dataset
{
    namespace
    {
        struct Hdr
        {
            char magic[4];
            uint32_t version;
            uint32_t flags;
            uint32_t reserved0;
            uint64_t scene_off;
            uint64_t cam_off;
            uint64_t frames_off;
            uint64_t pixels_off;
            uint64_t end_off;
            uint64_t bytes_total;
            uint32_t pixel_format;
            uint32_t color_space;
            uint64_t caps_bits;
        };

        struct SceneRec
        {
            float aabb_min[3];
            float aabb_max[3];
        };

        struct CamSOA
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

        struct PackHandleImpl
        {
            detail::mmap_ro map;
            Hdr hdr;
            SceneRec scene;
            CamSOA cam;
            std::vector<FrameRec> frames;
            const char* base;
        };

        std::atomic<int32_t> g_last_error{0};

        inline size_t rup(size_t x, size_t a)
        {
            return (x + (a - 1)) & ~(a - 1);
        }

        inline void set_error(Error e)
        {
            g_last_error.store((int32_t)e, std::memory_order_relaxed);
        }

        struct PngImg
        {
            int w;
            int h;
            std::vector<unsigned char> rgba;
        };

        PngImg decode_png_rgba8(const std::string& path)
        {
            PngImg out{0, 0, {}};
            auto m = dataset::detail::mmap_file_ro(path);
            if (!m.ptr)
            {
                set_error(Error::IoFail);
                return out;
            }
            spng_ctx* ctx = spng_ctx_new(0);
            spng_set_png_buffer(ctx, m.ptr, m.bytes);
            spng_ihdr ih;
            if (spng_get_ihdr(ctx, &ih))
            {
                spng_ctx_free(ctx);
                dataset::detail::munmap_file(m);
                set_error(Error::BadConfig);
                return out;
            }
            size_t n = 0;
            spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &n);
            out.w = (int)ih.width;
            out.h = (int)ih.height;
            out.rgba.resize(n);
            if (spng_decode_image(ctx, out.rgba.data(), n, SPNG_FMT_RGBA8, 0))
            {
                out.w = 0;
                out.h = 0;
                out.rgba.clear();
                set_error(Error::BadConfig);
            }
            spng_ctx_free(ctx);
            dataset::detail::munmap_file(m);
            return out;
        }

        void srgb_lut(float lut[256])
        {
            for (int i = 0; i < 256; i++)
            {
                float c = float(i) / 255.0f;
                lut[i] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
            }
        }

        int write_exact(std::ofstream& fo, const void* p, size_t n)
        {
            fo.write((const char*)p, (std::streamsize)n);
            return fo ? 0 : -1;
        }

        struct NSItem
        {
            std::string path;
            float T[12];
        };

        struct NSMeta
        {
            int w;
            int h;
            float fx;
            float fy;
            float cx;
            float cy;
            std::vector<NSItem> items;
        };

        bool load_nerf_synthetic(const std::string& root, const std::string& cfg, NSMeta& out)
        {
            fs::path p(cfg);
            if (!fs::exists(p))
            {
                fs::path a = fs::path(root) / "transforms.json";
                fs::path b = fs::path(root) / "transforms_train.json";
                if (fs::exists(a))
                {
                    p = a;
                }
                else if (fs::exists(b))
                {
                    p = b;
                }
                else
                {
                    set_error(Error::BadConfig);
                    return false;
                }
            }
            std::ifstream fi(p, std::ios::binary);
            if (!fi)
            {
                set_error(Error::IoFail);
                return false;
            }
            std::string s((std::istreambuf_iterator<char>(fi)), std::istreambuf_iterator<char>());
            // Use DOM parser here for robustness
            simdjson::dom::parser dparser;
            simdjson::dom::element ddoc = dparser.parse(s);
            out = NSMeta{};
            auto arr = ddoc["frames"].get_array();
            for (auto v : arr)
            {
                auto fo = v.get_object();
                NSItem it;
                std::string rel = std::string(fo["file_path"].get_string().value());
                fs::path full = fs::path(root) / rel;
                if (full.extension().empty()) full.replace_extension(".png");
                it.path = full.string();
                // transform_matrix is a 4x4 nested array
                auto tm_rows = fo["transform_matrix"].get_array();
                int r = 0;
                for (auto row : tm_rows)
                {
                    int c = 0;
                    for (auto x : row.get_array())
                    {
                        double d = x.get_double();
                        if (r < 3 && c < 4) it.T[r * 4 + c] = float(d);
                        c++;
                    }
                    r++;
                }
                out.items.push_back(std::move(it));
            }
            
            return true;
        }
    }

    Error last_error()
    {
        return (Error)g_last_error.load(std::memory_order_relaxed);
    }

    int build_hostpack(const BuildConfig& cfg, const std::string& out_path)
    {
        g_last_error.store(0, std::memory_order_relaxed);
        NSMeta meta;
        if (!load_nerf_synthetic(cfg.dataset_root, cfg.config_path, meta)) return -1;
        size_t N = meta.items.size();
        if (!N)
        {
            set_error(Error::BadConfig);
            return -1;
        }
        std::vector<PngImg> imgs(N);
        std::atomic<size_t> next{0};
        uint32_t th = cfg.threads ? cfg.threads : std::thread::hardware_concurrency();
        if (!th) th = 1;
        std::vector<std::thread> threads;
        threads.reserve(th);
        for (uint32_t t = 0; t < th; t++)
        {
            threads.emplace_back([&]
            {
                for (;;)
                {
                    size_t i = next.fetch_add(1, std::memory_order_relaxed);
                    if (i >= N) break;
                    imgs[i] = decode_png_rgba8(meta.items[i].path);
                }
            });
        }
        for (auto& thd : threads) thd.join();
        for (size_t i = 0; i < N; i++)
        {
            if (!imgs[i].w)
            {
                set_error(Error::IoFail);
                return -1;
            }
        }
        std::ofstream fo(out_path, std::ios::binary | std::ios::trunc);
        if (!fo)
        {
            set_error(Error::IoFail);
            return -1;
        }
        Hdr hdr{};
        std::memcpy(hdr.magic, "HPK1", 4);
        hdr.version = 2;
        hdr.flags = 0;
        hdr.pixel_format = (uint32_t)cfg.pixel_format;
        hdr.color_space = (uint32_t)ColorSpace::Linear;
        hdr.caps_bits = 0;

        auto wr = [&](const void* p, size_t n)
        {
            return write_exact(fo, p, n);
        };

        auto align_block = [&](size_t a)
        {
            size_t cur = (size_t)fo.tellp();
            size_t need = rup(cur, a) - cur;
            if (need)
            {
                static const char z[64] = {0};
                while (need)
                {
                    size_t m = need > 64 ? 64 : need;
                    wr(z, m);
                    need -= m;
                }
            }
        };
        // Reserve space for header at the beginning
        fo.seekp(sizeof(Hdr), std::ios::beg);
        align_block(cfg.block_align);
        hdr.scene_off = (uint64_t)fo.tellp();
        SceneRec scene{};
        scene.aabb_min[0] = scene.aabb_min[1] = scene.aabb_min[2] = -1;
        scene.aabb_max[0] = scene.aabb_max[1] = scene.aabb_max[2] = 1;
        wr(&scene, sizeof(scene));

        align_block(cfg.block_align);
        hdr.cam_off = (uint64_t)fo.tellp();
        CamSOA cam{};
        cam.count = (uint32_t)N;
        size_t fx_off = hdr.cam_off + sizeof(CamSOA);
        size_t fy_off = fx_off + sizeof(float) * N;
        size_t cx_off = fy_off + sizeof(float) * N;
        size_t cy_off = cx_off + sizeof(float) * N;
        size_t T_off = cy_off + sizeof(float) * N;
        size_t w_off = T_off + sizeof(float) * 12 * N;
        size_t h_off = w_off + sizeof(uint32_t) * N;
        size_t time_off = h_off + sizeof(uint32_t) * N;
        cam.fx_off = fx_off;
        cam.fy_off = fy_off;
        cam.cx_off = cx_off;
        cam.cy_off = cy_off;
        cam.T_off = T_off;
        cam.w_off = w_off;
        cam.h_off = h_off;
        cam.time_off = time_off;
        wr(&cam, sizeof(cam));

        std::vector<float> fx(N), fy(N), cx(N), cy(N), T(12 * N);
        std::vector<uint32_t> ww(N), hh(N), tt(N);
        for (size_t i = 0; i < N; i++)
        {
            fx[i] = meta.fx;
            fy[i] = meta.fy;
            cx[i] = meta.cx;
            cy[i] = meta.cy;
            ww[i] = (uint32_t)imgs[i].w;
            hh[i] = (uint32_t)imgs[i].h;
            tt[i] = (uint32_t)i;
            for (int j = 0; j < 12; j++)
            {
                T[i * 12 + j] = meta.items[i].T[j];
            }
        }
        wr(fx.data(), sizeof(float) * N);
        wr(fy.data(), sizeof(float) * N);
        wr(cx.data(), sizeof(float) * N);
        wr(cy.data(), sizeof(float) * N);
        wr(T.data(), sizeof(float) * 12 * N);
        wr(ww.data(), sizeof(uint32_t) * N);
        wr(hh.data(), sizeof(uint32_t) * N);
        wr(tt.data(), sizeof(uint32_t) * N);

        align_block(cfg.block_align);
        hdr.frames_off = (uint64_t)fo.tellp();
        std::vector<FrameRec> frs(N);
        wr(frs.data(), sizeof(FrameRec) * N);

        align_block(cfg.block_align);
        hdr.pixels_off = (uint64_t)fo.tellp();

        float lutf[256];
        if (cfg.pixel_format == PixelFormat::RGBA32F)
        {
            srgb_lut(lutf);
        }

        std::vector<unsigned char> row;
        for (size_t i = 0; i < N; i++)
        {
            int w = imgs[i].w;
            int h = imgs[i].h;
            uint32_t pixel_stride = cfg.pixel_format == PixelFormat::RGBA8 ? 4u : 16u;
            uint32_t row_stride = (uint32_t)rup((size_t)w * pixel_stride, cfg.row_align);
            frs[i].camera_id = (uint32_t)i;
            frs[i].mip_levels = 1;
            frs[i].pixel_off = (uint64_t)fo.tellp();
            frs[i].width = (uint32_t)w;
            frs[i].height = (uint32_t)h;
            frs[i].row_stride = row_stride;
            frs[i].pixel_stride = pixel_stride;
            frs[i].roi_x = 0;
            frs[i].roi_y = 0;
            frs[i].roi_w = (uint32_t)w;
            frs[i].roi_h = (uint32_t)h;
            row.resize(row_stride);
            if (cfg.pixel_format == PixelFormat::RGBA8)
            {
                const unsigned char* src = imgs[i].rgba.data();
                for (int y = 0; y < h; y++)
                {
                    std::memcpy(row.data(), src + (size_t)y * w * 4, (size_t)w * 4);
                    wr(row.data(), row_stride);
                }
            }
            else
            {
                const unsigned char* src = imgs[i].rgba.data();
                float* dstf = (float*)row.data();
                for (int y = 0; y < h; y++)
                {
                    const unsigned char* s = src + (size_t)y * w * 4;
                    for (int x = 0; x < w; x++)
                    {
                        unsigned char r = s[x * 4 + 0];
                        unsigned char g = s[x * 4 + 1];
                        unsigned char b = s[x * 4 + 2];
                        unsigned char a = s[x * 4 + 3];
                        dstf[x * 4 + 0] = lutf[r];
                        dstf[x * 4 + 1] = lutf[g];
                        dstf[x * 4 + 2] = lutf[b];
                        dstf[x * 4 + 3] = float(a) / 255.0f;
                    }
                    wr(row.data(), row_stride);
                }
            }
            align_block(cfg.block_align);
        }

        size_t cur = (size_t)fo.tellp();
        fo.seekp(hdr.frames_off, std::ios::beg);
        wr(frs.data(), sizeof(FrameRec) * N);
        fo.seekp(cur, std::ios::beg);
        hdr.end_off = (uint64_t)cur;
        hdr.bytes_total = hdr.end_off;
        fo.seekp(0, std::ios::beg);
        wr(&hdr, sizeof(Hdr));
        fo.flush();
        if (!fo)
        {
            set_error(Error::IoFail);
            return -1;
        }
        return 0;
    }

    PackHandle open_hostpack(const std::string& hostpack_path)
    {
        g_last_error.store(0, std::memory_order_relaxed);
        auto* h = new PackHandleImpl();
        h->map = detail::mmap_file_ro(hostpack_path);
        if (!h->map.ptr)
        {
            delete h;
            set_error(Error::IoFail);
            return nullptr;
        }
        h->base = (const char*)h->map.ptr;
        std::memcpy(&h->hdr, h->base, sizeof(Hdr));
        if (std::memcmp(h->hdr.magic, "HPK1", 4) != 0)
        {
            detail::munmap_file(h->map);
            delete h;
            set_error(Error::BadPack);
            return nullptr;
        }
        std::memcpy(&h->scene, h->base + h->hdr.scene_off, sizeof(SceneRec));
        std::memcpy(&h->cam, h->base + h->hdr.cam_off, sizeof(CamSOA));
        size_t n = h->cam.count;
        h->frames.resize(n);
        std::memcpy(h->frames.data(), h->base + h->hdr.frames_off, sizeof(FrameRec) * n);
        return (PackHandle)h;
    }

    void close_hostpack(PackHandle ph)
    {
        if (!ph) return;
        auto* h = (PackHandleImpl*)ph;
        detail::munmap_file(h->map);
        delete h;
    }

    size_t frame_count(PackHandle ph)
    {
        auto* h = (PackHandleImpl*)ph;
        return h ? h->frames.size() : 0;
    }

    size_t camera_count(PackHandle ph)
    {
        auto* h = (PackHandleImpl*)ph;
        return h ? (size_t)h->cam.count : 0;
    }

    size_t frame_camera_index(PackHandle ph, size_t i)
    {
        auto* h = (PackHandleImpl*)ph;
        if (!h || i >= h->frames.size()) return 0;
        return (size_t)h->frames[i].camera_id;
    }

    ImageView image_view(PackHandle ph, size_t i)
    {
        ImageView v{};
        auto* h = (PackHandleImpl*)ph;
        if (!h || i >= h->frames.size()) return v;
        const FrameRec& fr = h->frames[i];
        v.data = (const void*)(h->base + fr.pixel_off);
        v.width = fr.width;
        v.height = fr.height;
        v.row_stride = fr.row_stride;
        v.pixel_stride = fr.pixel_stride;
        v.format = (PixelFormat)h->hdr.pixel_format;
        v.roi_x = fr.roi_x;
        v.roi_y = fr.roi_y;
        v.roi_w = fr.roi_w;
        v.roi_h = fr.roi_h;
        return v;
    }

    CameraSOAView camera_soa(PackHandle ph)
    {
        CameraSOAView v{};
        auto* h = (PackHandleImpl*)ph;
        if (!h) return v;
        const char* base = h->base;
        const CamSOA& c = h->cam;
        v.fx = (const float*)(base + c.fx_off);
        v.fy = (const float*)(base + c.fy_off);
        v.cx = (const float*)(base + c.cx_off);
        v.cy = (const float*)(base + c.cy_off);
        v.T3x4 = (const float*)(base + c.T_off);
        v.width = (const uint32_t*)(base + c.w_off);
        v.height = (const uint32_t*)(base + c.h_off);
        v.time = (const uint32_t*)(base + c.time_off);
        v.count = (size_t)c.count;
        return v;
    }

    void scene_aabb(PackHandle ph, float out_min[3], float out_max[3])
    {
        auto* h = (PackHandleImpl*)ph;
        if (!h)
        {
            out_min[0] = out_min[1] = out_min[2] = 0;
            out_max[0] = out_max[1] = out_max[2] = 0;
            return;
        }
        for (int i = 0; i < 3; i++)
        {
            out_min[i] = h->scene.aabb_min[i];
            out_max[i] = h->scene.aabb_max[i];
        }
    }

    ColorSpace scene_color_space(PackHandle ph)
    {
        auto* h = (PackHandleImpl*)ph;
        if (!h) return ColorSpace::Linear;
        return (ColorSpace)h->hdr.color_space;
    }

    PixelFormat pack_pixel_format(PackHandle ph)
    {
        auto* h = (PackHandleImpl*)ph;
        if (!h) return PixelFormat::RGBA8;
        return (PixelFormat)h->hdr.pixel_format;
    }

    int hostpack_version(PackHandle ph)
    {
        auto* h = (PackHandleImpl*)ph;
        if (!h) return 0;
        return (int)h->hdr.version;
    }

    Caps pack_caps(PackHandle ph)
    {
        Caps c{};
        auto* h = (PackHandleImpl*)ph;
        if (!h)
        {
            c.bits = 0;
            return c;
        }
        c.bits = h->hdr.caps_bits;
        return c;
    }

    uint64_t pack_bytes(PackHandle ph)
    {
        auto* h = (PackHandleImpl*)ph;
        if (!h) return 0;
        return h->hdr.bytes_total;
    }
}
