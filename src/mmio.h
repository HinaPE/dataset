#ifndef DATASET_MMIO_H
#define DATASET_MMIO_H
#include <cstddef>
#include <string>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace dataset::detail
{
    struct mmap_ro
    {
        void* ptr;
        size_t bytes;
#if defined(_WIN32)
        void* hfile;
        void* hmap;

        mmap_ro() : ptr(nullptr), bytes(0), hfile(nullptr), hmap(nullptr)
        {
        }
#else
        int fd;
        mmap_ro() : ptr(nullptr), bytes(0), fd(-1)
        {
        }
#endif
    };

    mmap_ro mmap_file_ro(const std::string& path);
    void munmap_file(mmap_ro& m);
}
#endif //DATASET_MMIO_H
