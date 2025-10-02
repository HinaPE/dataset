#include "mmio.h"
namespace dataset::detail {
mmap_ro mmap_file_ro(const std::string& path) {
  mmap_ro m;
#if defined(_WIN32)
  HANDLE hf = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hf == INVALID_HANDLE_VALUE) return m;
  LARGE_INTEGER sz; if (!GetFileSizeEx(hf, &sz)) { CloseHandle(hf); return m; }
  HANDLE hm = CreateFileMappingA(hf, nullptr, PAGE_READONLY, 0, 0, nullptr);
  if (!hm) { CloseHandle(hf); return m; }
  void* p = MapViewOfFile(hm, FILE_MAP_READ, 0, 0, 0);
  if (!p) { CloseHandle(hm); CloseHandle(hf); return m; }
  m.ptr = p; m.bytes = (size_t)sz.QuadPart; m.hfile = hf; m.hmap = hm;
#else
  int fd = open(path.c_str(), O_RDONLY); if (fd < 0) return m;
  struct stat st; if (fstat(fd, &st) < 0) { close(fd); return m; }
  size_t n = (size_t)st.st_size;
  void* p = mmap(nullptr, n, PROT_READ, MAP_PRIVATE, fd, 0);
  if (p == MAP_FAILED) { close(fd); return m; }
  m.ptr = p; m.bytes = n; m.fd = fd;
#endif
  return m;
}
void munmap_file(mmap_ro& m) {
#if defined(_WIN32)
  if (m.ptr) UnmapViewOfFile(m.ptr);
  if (m.hmap) CloseHandle((HANDLE)m.hmap);
  if (m.hfile) CloseHandle((HANDLE)m.hfile);
  m.ptr = nullptr; m.bytes = 0; m.hmap = nullptr; m.hfile = nullptr;
#else
  if (m.ptr) munmap(m.ptr, m.bytes);
  if (m.fd >= 0) close(m.fd);
  m.ptr = nullptr; m.bytes = 0; m.fd = -1;
#endif
}
}
