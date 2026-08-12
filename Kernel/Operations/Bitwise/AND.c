#include "AND.h"
#include <string.h>
#include "../../Core/CPU/SIMD.h"

#ifdef COMPILER_X86
#include <immintrin.h>
#endif
#ifdef COMPILER_ARM
#include <arm_neon.h>
#endif

static inline double bitwise_and_scalar_single(double a, double b) {
    uint64_t ua, ub, ur;
    memcpy(&ua, &a, sizeof(double));
    memcpy(&ub, &b, sizeof(double));
    ur = ua & ub;
    double r;
    memcpy(&r, &ur, sizeof(double));
    return r;
}

void and_scalar(CalculatorState* state, const double* a, const double* b, double* result, uint32_t count) {
    (void)state;
    for (uint32_t i = 0; i < count; ++i) {
        result[i] = bitwise_and_scalar_single(a[i], b[i]);
    }
}

TARGET_SSE2 void and_sse(CalculatorState* state, const double* a, const double* b, double* result, uint32_t count) {
    (void)state;
#ifdef COMPILER_X86
    uint32_t i = 0;

    for (; i + 1 < count; i += 2) {
        __m128d va = _mm_loadu_pd(&a[i]);
        __m128d vb = _mm_loadu_pd(&b[i]);
        __m128d vr = _mm_and_pd(va, vb);
        _mm_storeu_pd(&result[i], vr);
    }

    for (; i < count; ++i) {
        result[i] = bitwise_and_scalar_single(a[i], b[i]);
    }
#else
    and_scalar(state, a, b, result, count);
#endif
}

TARGET_AVX2 void and_avx2(CalculatorState* state, const double* a, const double* b, double* result, uint32_t count) {
    (void)state;
#ifdef COMPILER_X86
    uint32_t i = 0;

    for (; i + 3 < count; i += 4) {
        __m256d va = _mm256_loadu_pd(&a[i]);
        __m256d vb = _mm256_loadu_pd(&b[i]);
        __m256d vr = _mm256_and_pd(va, vb);
        _mm256_storeu_pd(&result[i], vr);
    }

    for (; i < count; ++i) {
        result[i] = bitwise_and_scalar_single(a[i], b[i]);
    }
#else
    and_scalar(state, a, b, result, count);
#endif
}

void and_neon(CalculatorState* state, const double* a, const double* b, double* result, uint32_t count) {
    (void)state;
#if defined(COMPILER_ARM) && (defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM_NEON))
    uint32_t i = 0;

    for (; i + 1 < count; i += 2) {
        float64x2_t va = vld1q_f64(&a[i]);
        float64x2_t vb = vld1q_f64(&b[i]);
        uint64x2_t vr = vandq_u64(vreinterpretq_u64_f64(va), vreinterpretq_u64_f64(vb));
        vst1q_f64(&result[i], vreinterpretq_f64_u64(vr));
    }

    for (; i < count; ++i) {
        result[i] = bitwise_and_scalar_single(a[i], b[i]);
    }
#else
    and_scalar(state, a, b, result, count);
#endif
}

void execute_and(CalculatorState* state, const double* a, const double* b, double* result, uint32_t count, const CPUFeatures* features) {
    if (features->has_neon) {
        and_neon(state, a, b, result, count);
    } else if (features->has_avx2) {
        and_avx2(state, a, b, result, count);
    } else if (features->has_sse2) {
        and_sse(state, a, b, result, count);
    } else {
        and_scalar(state, a, b, result, count);
    }
}
