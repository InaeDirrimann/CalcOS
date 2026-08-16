#include "Logarithm.h"

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#define COMPILER_X86
#endif

#if defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
#include <arm_neon.h>
#define COMPILER_ARM
#endif

#define LN2 0.69314718055994530942

static inline double approx_log_scalar(CalculatorState* state, double x) {
    if (x <= 0.0) {
        if (state) state->flags |= 2; // DivZero / Invalid flag
        union { uint64_t i; double d; } u;
        if (x == 0.0) {
            u.i = 0xFFF0000000000000ULL; // -Infinity
            return u.d;
        }
        u.i = 0x7FF8000000000000ULL; // NaN
        return u.d;
    }
    
    union {
        double d;
        uint64_t i;
    } u;
    u.d = x;
    int64_t k = ((u.i >> 52) & 0x7FF) - 1023;
    u.i = (u.i & 0x000FFFFFFFFFFFFFULL) | 0x3FF0000000000000ULL;
    double m = u.d;
    
    // z = (m - 1) / (m + 1)
    double num = m - 1.0;
    double den = m + 1.0;
    double z = num / den;
    double z2 = z * z;
    
    // 2 * z * (1 + z2*(1/3 + z2*(1/5 + z2*(1/7 + z2*(1/9 + z2*(1/11))))))
    double poly = z * (2.0 + z2 * (0.6666666666666666 + z2 * (0.4 + z2 * (0.2857142857142857 + z2 * (0.2222222222222222 + z2 * 0.1818181818181818)))));
    
    return poly + (double)k * LN2;
}

void log_scalar(CalculatorState* state, const double* a, double* result, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        result[i] = approx_log_scalar(state, a[i]);
    }
}

// NOTE: no fake "_sse/_avx2/_neon" wrappers here anymore. The previous ones
// just forwarded to scalar while the header claimed vectorization -- that's
// a lie. log() has per-element domain branches (x<=0 -> -Inf/NaN + flag),
// which need lane masks to vectorize correctly. Do that properly when a
// bulk-log use case exists, not with forwarding stubs.
void execute_logarithm(CalculatorState* state, const double* a, double* result, uint32_t count, const CPUFeatures* features) {
    (void)features;
    log_scalar(state, a, result, count);
}
