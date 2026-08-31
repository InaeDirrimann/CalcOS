#include "CPUID.h"
#include "SIMD.h"
#if defined(_MSC_VER)
#include <intrin.h>
#endif

void cpu_detect_features(CPUFeatures* features) {
    if (!features) return;

    // HOST_APP guard: can't run CPUID inline asm on macOS/ARM/CI.
    // x86_64 ABI guarantees SSE2 minimum, NOT SSE3/SSE4.1/AVX -- those must be
    // confirmed at runtime or left false so no kernel dispatches onto
    // instructions the CPU doesn't support.
#ifdef HOST_APP
    features->has_sse  = true;   // x86_64 ABI guarantees SSE2 minimum
    features->has_sse2 = true;
    features->has_sse3 = false;
    features->has_sse4_1 = false;
    features->has_avx  = false;
    features->has_avx2 = false;
    features->has_fma  = false;
    features->has_neon = false;  // x86 hosts have no NEON; never leave it indeterminate
#if defined(COMPILER_X86) && (defined(__GNUC__) || defined(__clang__))
    // GCC/Clang runtime probe, no inline asm needed. Confirms each feature
    // against the actual host before any SIMD kernel gets dispatched.
    __builtin_cpu_init();
    if (__builtin_cpu_supports("sse3"))   features->has_sse3 = true;
    if (__builtin_cpu_supports("sse4.1")) features->has_sse4_1 = true;
    if (__builtin_cpu_supports("avx"))    features->has_avx = true;
    if (__builtin_cpu_supports("avx2"))   features->has_avx2 = true;
    if (__builtin_cpu_supports("fma"))    features->has_fma = true;
#endif
#if defined(__aarch64__) || defined(__ARM_NEON)
    features->has_neon = true;
#endif
    return;
#endif

    // zero out first. stack garbage can set random flags. not acceptable.
    features->has_sse = false;
    features->has_sse2 = false;
    features->has_sse3 = false;
    features->has_sse4_1 = false;
    features->has_avx = false;
    features->has_avx2 = false;
    features->has_fma = false;
    features->has_neon = false;

#if defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
    // ARM: NEON is mandatory on AArch64. no CPUID leaf needed.
    features->has_neon = true;
#else
    uint32_t eax, ebx, ecx, edx;

#if defined(_MSC_VER)
    int cpuInfo[4];
    __cpuid(cpuInfo, 1); // CPUID leaf 1: SSE/SSE2/SSE3/AVX/FMA bits
    eax = (uint32_t)cpuInfo[0];
    ebx = (uint32_t)cpuInfo[1];
    ecx = (uint32_t)cpuInfo[2];
    edx = (uint32_t)cpuInfo[3];
#else
    // GCC/Clang inline CPUID -- clobbers eax/ebx/ecx/edx, must list all.
    eax = 1;
    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(eax)
    );
#endif

    // EDX bit 25: SSE, bit 26: SSE2
    if (edx & (1 << 25)) features->has_sse = true;
    if (edx & (1 << 26)) features->has_sse2 = true;

    // ECX bit 0: SSE3, bit 19: SSE4.1, bit 12: FMA, bit 28: AVX
    if (ecx & (1 << 0))  features->has_sse3 = true;
    if (ecx & (1 << 19)) features->has_sse4_1 = true;

    // AVX/FMA need more than CPUID bits: the OS must have enabled the XSAVE
    // state. Without OSXSAVE (ECX bit 27) + XGETBV(0) reporting XMM and YMM
    // state, dispatching an AVX kernel raises #UD even on AVX-capable CPUs.
    bool os_avx_ok = false;
    if (ecx & (1 << 27)) {
#if defined(_MSC_VER)
        uint64_t xcr0 = _xgetbv(0);
#else
        uint32_t xcr0_lo, xcr0_hi;
        __asm__ volatile ("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
        uint64_t xcr0 = ((uint64_t)xcr0_hi << 32) | xcr0_lo;
#endif
        os_avx_ok = (xcr0 & 0x6) == 0x6;   // XMM (bit 1) + YMM (bit 2) enabled
    }
    if (os_avx_ok) {
        if (ecx & (1 << 12)) features->has_fma = true;
        if (ecx & (1 << 28)) features->has_avx = true;
    }

#if defined(_MSC_VER)
    __cpuidex(cpuInfo, 7, 0); // leaf 7, sub-leaf 0: extended features
    eax = (uint32_t)cpuInfo[0];
    ebx = (uint32_t)cpuInfo[1];
    ecx = (uint32_t)cpuInfo[2];
    edx = (uint32_t)cpuInfo[3];
#else
    // CPUID leaf 7, sub-leaf 0: AVX2 is in EBX bit 5
    eax = 7;
    ecx = 0;
    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(eax), "c"(ecx)
    );
#endif

    // EBX bit 5: AVX2 (also requires the OS XSAVE state from above)
    if (os_avx_ok && (ebx & (1 << 5))) features->has_avx2 = true;
#endif
}