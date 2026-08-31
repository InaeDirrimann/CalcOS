#ifndef SIMD_H
#define SIMD_H

// Shared architecture detection + per-function ISA attributes for SIMD kernels.
// x86 intrinsics and target attributes must never reach non-x86 compilers:
// GCC/Clang reject __attribute__((target("avx2"))) on AArch64 outright, and
// __m256d/__m128d types don't exist there. Keep this the single source of truth
// instead of redefining these in every operation file.

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define COMPILER_X86
#endif

#if defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
#define COMPILER_ARM
#endif

// Per-function ISA target: emit AVX2/SSE4.1/SSE2 code for one function even when
// the translation unit's global baseline is lower (or 32-bit). Empty on non-x86
// or compilers without __attribute__((target(...))) support (MSVC).
#if defined(COMPILER_X86) && (defined(__GNUC__) || defined(__clang__))
#define TARGET_AVX2   __attribute__((target("avx2")))
#define TARGET_SSE2   __attribute__((target("sse2")))
#define TARGET_SSE4_1 __attribute__((target("sse4.1")))
// FMA needs its own target: it's a separate CPU feature bit (CPUID.1:ECX[12])
// from AVX2 (CPUID.7:EBX[5]). A CPU can have AVX2 without FMA (early
// Haswell steppings were AVX2-only). target("avx2,fma") requires both so the
// compiler can freely emit vfmadd* instructions.
#define TARGET_AVX2_FMA __attribute__((target("avx2,fma")))
#else
#define TARGET_AVX2
#define TARGET_SSE2
#define TARGET_SSE4_1
#define TARGET_AVX2_FMA
#endif

#endif // SIMD_H
