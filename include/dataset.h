#ifndef DATASET_DATASET_H
#define DATASET_DATASET_H

#include <cstdint>
#include <cstddef>
#include <string>

namespace dataset
{
    enum class PixelFormat : uint32_t { RGBA8 = 1, RGBA32F = 2 };

    enum class ColorSpace : uint32_t { Linear = 0, SRGB = 1 };

    enum class Error : int32_t { Ok = 0, IoFail = -1, BadConfig = -2, BadPack = -3, Unsupported = -4, NoMemory = -5, Internal = -6 };

    struct BuildConfig
    {
        std::string dataset_root;
        std::string config_path;
        PixelFormat pixel_format;
        uint32_t row_align;
        uint32_t block_align;
        uint32_t threads;
    };

    struct PackHandleTag;
    using PackHandle = PackHandleTag*;

    struct ImageView
    {
        const void* data;
        uint32_t width;
        uint32_t height;
        uint32_t row_stride;
        uint32_t pixel_stride;
        PixelFormat format;
        uint32_t roi_x;
        uint32_t roi_y;
        uint32_t roi_w;
        uint32_t roi_h;
    };

    struct CameraSOAView
    {
        const float* fx;
        const float* fy;
        const float* cx;
        const float* cy;
        const float* T3x4;
        const uint32_t* width;
        const uint32_t* height;
        const uint32_t* time;
        size_t count;
    };

    struct Caps
    {
        uint64_t bits;
    };

    int build_hostpack(const BuildConfig& cfg, const std::string& out_path);
    PackHandle open_hostpack(const std::string& hostpack_path);
    void close_hostpack(PackHandle h);
    Error last_error();
    size_t frame_count(PackHandle h);
    size_t camera_count(PackHandle h);
    size_t frame_camera_index(PackHandle h, size_t frame_index);
    ImageView image_view(PackHandle h, size_t frame_index);
    CameraSOAView camera_soa(PackHandle h);
    void scene_aabb(PackHandle h, float out_min[3], float out_max[3]);
    ColorSpace scene_color_space(PackHandle h);
    PixelFormat pack_pixel_format(PackHandle h);
    int hostpack_version(PackHandle h);
    Caps pack_caps(PackHandle h);
    uint64_t pack_bytes(PackHandle h);
}

#endif