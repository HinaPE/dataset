#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>
#include "dataset.h"

using namespace dataset;

int usage()
{
    std::cerr << "usage:\n";
    std::cerr << "  dataset_cli build <dataset_root> <config_or_auto> <out_hostpack> [--pf rgba8|rgba32f] [--threads N] [--row-align N] [--block-align N]\n";
    std::cerr << "  dataset_cli info <hostpack>\n";
    std::cerr << "  dataset_cli list <hostpack>\n";
    return 1;
}

int cmd_build(int argc, char** argv)
{
    if (argc < 5) return usage();
    BuildConfig cfg{};
    cfg.dataset_root = argv[2];
    cfg.config_path = argv[3];
    cfg.pixel_format = PixelFormat::RGBA8;
    cfg.row_align = 16;
    cfg.block_align = 4096;
    cfg.threads = 0;
    std::string out_path = argv[4];
    for (int i = 5; i < argc; i++)
    {
        if (!std::strcmp(argv[i], "--pf") && i + 1 < argc)
        {
            i++;
            if (!std::strcmp(argv[i], "rgba8"))
            {
                cfg.pixel_format = PixelFormat::RGBA8;
            }
            else if (!std::strcmp(argv[i], "rgba32f"))
            {
                cfg.pixel_format = PixelFormat::RGBA32F;
            }
            else
            {
                std::cerr << "bad pixel format\n";
                return 2;
            }
        }
        else if (!std::strcmp(argv[i], "--threads") && i + 1 < argc)
        {
            cfg.threads = (uint32_t)std::stoul(argv[++i]);
        }
        else if (!std::strcmp(argv[i], "--row-align") && i + 1 < argc)
        {
            cfg.row_align = (uint32_t)std::stoul(argv[++i]);
        }
        else if (!std::strcmp(argv[i], "--block-align") && i + 1 < argc)
        {
            cfg.block_align = (uint32_t)std::stoul(argv[++i]);
        }
        else
        {
            std::cerr << "unknown option: " << argv[i] << "\n";
            return 2;
        }
    }
    if (cfg.row_align == 0 || (cfg.row_align & (cfg.row_align - 1)))
    {
        std::cerr << "row-align must be power of two\n";
        return 2;
    }
    if (cfg.block_align == 0 || (cfg.block_align & (cfg.block_align - 1)))
    {
        std::cerr << "block-align must be power of two\n";
        return 2;
    }
    int r = build_hostpack(cfg, out_path);
    if (r)
    {
        std::cerr << "build failed error=" << (int)last_error() << "\n";
        return 3;
    }
    std::cout << "ok\n";
    return 0;
}

const char* pf_name(PixelFormat pf)
{
    return pf == PixelFormat::RGBA8 ? "RGBA8" : pf == PixelFormat::RGBA32F ? "RGBA32F" : "?";
}

int cmd_info(int argc, char** argv)
{
    if (argc < 3) return usage();
    PackHandle h = open_hostpack(argv[2]);
    if (!h)
    {
        std::cerr << "open failed error=" << (int)last_error() << "\n";
        return 3;
    }
    std::cout << "frames=" << frame_count(h)
        << " cameras=" << camera_count(h)
        << " version=" << hostpack_version(h)
        << " pixel_format=" << pf_name(pack_pixel_format(h))
        << " bytes=" << pack_bytes(h) << "\n";
    float bmin[3];
    float bmax[3];
    scene_aabb(h, bmin, bmax);
    std::cout << "aabb_min=" << bmin[0] << "," << bmin[1] << "," << bmin[2]
        << " aabb_max=" << bmax[0] << "," << bmax[1] << "," << bmax[2] << "\n";
    close_hostpack(h);
    return 0;
}

int cmd_list(int argc, char** argv)
{
    if (argc < 3) return usage();
    PackHandle h = open_hostpack(argv[2]);
    if (!h)
    {
        std::cerr << "open failed error=" << (int)last_error() << "\n";
        return 3;
    }
    size_t n = frame_count(h);
    for (size_t i = 0; i < n; i++)
    {
        auto v = image_view(h, i);
        std::cout << i << ": cam=" << frame_camera_index(h, i)
            << " w=" << v.width
            << " h=" << v.height
            << " rs=" << v.row_stride
            << " ps=" << v.pixel_stride
            << " roi=" << v.roi_x << "," << v.roi_y << "," << v.roi_w << "," << v.roi_h
            << "\n";
    }
    close_hostpack(h);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 2) return usage();
    std::string cmd = argv[1];
    if (cmd == "build") return cmd_build(argc, argv);
    if (cmd == "info") return cmd_info(argc, argv);
    if (cmd == "list") return cmd_list(argc, argv);
    return usage();
}
