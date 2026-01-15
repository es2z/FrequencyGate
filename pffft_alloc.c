/*
 * PFFFT custom aligned memory allocator
 * Required for PFFFT to work correctly with SIMD
 */

#include <stdlib.h>

#ifdef _MSC_VER
#include <malloc.h>
#endif

void* pffft_aligned_malloc(size_t size) {
#ifdef _MSC_VER
    return _aligned_malloc(size, 64);
#elif defined(__MINGW32__) || defined(__MINGW64__)
    return __mingw_aligned_malloc(size, 64);
#else
    void* ptr = NULL;
    if (posix_memalign(&ptr, 64, size) != 0) {
        return NULL;
    }
    return ptr;
#endif
}

void pffft_aligned_free(void* ptr) {
    if (ptr) {
#ifdef _MSC_VER
        _aligned_free(ptr);
#elif defined(__MINGW32__) || defined(__MINGW64__)
        __mingw_aligned_free(ptr);
#else
        free(ptr);
#endif
    }
}
