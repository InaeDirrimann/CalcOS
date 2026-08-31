/*
 * File: DotProduct.h
 * Author: W. Kowal
 * Description: 4-element double dot product with runtime SIMD dispatch.
 *
 * This is the code the portfolio actually shows — real FMA, unaligned-safe
 * loads, and a clean 128-bit fold (no vhaddpd latency trap). Each variant
 * takes pointers, NOT arrays-by-value, so the caller decides alignment;
 * every variant uses unaligned loads, so a 16-byte or 32-byte stack offset
 * can never #GP the kernel.
 */

#ifndef DOT_PRODUCT_H
#define DOT_PRODUCT_H

#include <stdint.h>
#include "../../Core/CPU/CPUID.h"

// dot4 family: returns a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3]
double dot4_scalar(const double* a, const double* b);
double dot4_sse(const double* a, const double* b);      // SSE2, no FMA
double dot4_avx2(const double* a, const double* b);     // AVX2, no FMA
double dot4_avx2_fma(const double* a, const double* b); // AVX2 + FMA

// Runtime dispatch: FMA > AVX2 > SSE2 > scalar, decided by CPUID once.
double execute_dot4(const double* a, const double* b, const CPUFeatures* features);

#endif // DOT_PRODUCT_H
