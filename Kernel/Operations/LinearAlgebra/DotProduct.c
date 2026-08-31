#include "DotProduct.h"
#include "../../Core/CPU/SIMD.h"

#ifdef COMPILER_X86
#include <immintrin.h>
#endif

// Scalar baseline. Works everywhere, and honestly for a 4-element dot it's
// often within noise of the SIMD paths — the win shows up in hot loops that
// call this millions of times.
double dot4_scalar(const double* a, const double* b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
}

// SSE2, 2 doubles per lane. Deliberately NO vhaddpd: horizontal adds are
// 3-5 cycle multi-uop garbage on every Intel/AMD core. Instead, fold the
// lane-pair with a plain 128-bit add, then broadcast the high lane and add
// once more. Unaligned loads — a 16-byte stack offset can't #GP us.
// NOTE: 4 doubles need TWO 128-bit loads per array — SSE2 lanes hold 2.
TARGET_SSE2 double dot4_sse(const double* a, const double* b) {
#ifdef COMPILER_X86
    __m128d va0 = _mm_loadu_pd(a);       // [a0, a1]
    __m128d va1 = _mm_loadu_pd(a + 2);   // [a2, a3]
    __m128d vb0 = _mm_loadu_pd(b);       // [b0, b1]
    __m128d vb1 = _mm_loadu_pd(b + 2);   // [b2, b3]
    __m128d vp0 = _mm_mul_pd(va0, vb0);  // [a0b0, a1b1]
    __m128d vp1 = _mm_mul_pd(va1, vb1);  // [a2b2, a3b3]
    __m128d vsum = _mm_add_pd(vp0, vp1); // [a0b0+a2b2, a1b1+a3b3]
    __m128d vh = _mm_unpackhi_pd(vsum, vsum); // [a1b1+a3b3, ...]
    return _mm_cvtsd_f64(_mm_add_sd(vsum, vh)); // (a0b0+a2b2)+(a1b1+a3b3)
#else
    return dot4_scalar(a, b);
#endif
}

// AVX2, 4 doubles per lane. Same fold philosophy, one 256-bit multiply then
// a 128-bit cross-lane fold — the extract is a single uop, unlike vhaddpd's
// internal shuffle dance. Still unaligned-safe.
TARGET_AVX2 double dot4_avx2(const double* a, const double* b) {
#ifdef COMPILER_X86
    __m256d va = _mm256_loadu_pd(a);          // [a0, a1, a2, a3]
    __m256d vb = _mm256_loadu_pd(b);
    __m256d vp = _mm256_mul_pd(va, vb);       // [a0b0, a1b1, a2b2, a3b3]
    __m128d lo = _mm256_castpd256_pd128(vp);  // [a0b0, a1b1]
    __m128d hi = _mm256_extractf128_pd(vp, 1);// [a2b2, a3b3]
    __m128d sum2 = _mm_add_pd(lo, hi);        // [a0b0+a2b2, a1b1+a3b3]
    __m128d sum1 = _mm_unpackhi_pd(sum2, sum2);// [a1b1+a3b3, ...]
    return _mm_cvtsd_f64(_mm_add_sd(sum2, sum1)); // (a0b0+a2b2)+(a1b1+a3b3)
#else
    return dot4_scalar(a, b);
#endif
}

// AVX2 + FMA: the portfolio's "4x64-bit fused-multiply-add" claim, for real
// this time. One vfmaddpd does the multiply AND accumulate — the multiply
// result never round-trips through a register, so it's both faster and more
// accurate than mul+add. FMA is a separate CPU feature from AVX2, so this
// must only be dispatched when has_fma is set.
TARGET_AVX2_FMA double dot4_avx2_fma(const double* a, const double* b) {
#ifdef COMPILER_X86
    __m256d va = _mm256_loadu_pd(a);
    __m256d vb = _mm256_loadu_pd(b);
    __m256d vp = _mm256_fmadd_pd(va, vb, _mm256_setzero_pd()); // real FMA
    __m128d lo = _mm256_castpd256_pd128(vp);
    __m128d hi = _mm256_extractf128_pd(vp, 1);
    __m128d sum2 = _mm_add_pd(lo, hi);
    __m128d sum1 = _mm_unpackhi_pd(sum2, sum2);
    return _mm_cvtsd_f64(_mm_add_sd(sum2, sum1));
#else
    return dot4_scalar(a, b);
#endif
}

// Runtime dispatch. FMA is the top tier — it needs BOTH avx2 (256-bit regs)
// and fma (the instructions), so the check is has_fma alone; has_fma is only
// ever set when the OS XSAVE state also allows AVX2 (see CPUID.c).
double execute_dot4(const double* a, const double* b, const CPUFeatures* features) {
    if (features->has_fma) {
        return dot4_avx2_fma(a, b);
    } else if (features->has_avx2) {
        return dot4_avx2(a, b);
    } else if (features->has_sse2) {
        return dot4_sse(a, b);
    } else {
        return dot4_scalar(a, b);
    }
}
