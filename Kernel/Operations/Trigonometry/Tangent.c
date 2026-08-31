#include "Tangent.h"
#include "Sine.h"
#include "Cosine.h"
#include "../Arithmetic/Division.h"
#include "../../Core/CPU/SIMD.h"

#define CHUNK_SIZE 64

void tan_scalar(CalculatorState* state, const double* a, double* result, uint32_t count) {
    double sin_buf[CHUNK_SIZE];
    double cos_buf[CHUNK_SIZE];
    
    for (uint32_t offset = 0; offset < count; offset += CHUNK_SIZE) {
        uint32_t current_chunk = (count - offset < CHUNK_SIZE) ? (count - offset) : CHUNK_SIZE;
        sin_scalar(state, &a[offset], sin_buf, current_chunk);
        cos_scalar(state, &a[offset], cos_buf, current_chunk);
        div_scalar(state, sin_buf, cos_buf, &result[offset], current_chunk);
    }
}

// TARGET_SSE4_1, NOT SSE2: this calls sin_sse/cos_sse which use roundpd and
// blendvpd (SSE4.1-only). Gating the SSE path on has_sse2 while emitting
// SSE4.1 instructions inside tan_sse was an illegal-instruction SIGILL on
// every pre-2008 CPU (Pentium D, early Core 2, Atom). The dispatch below
// must check has_sse4_1 to match.
TARGET_SSE4_1 void tan_sse(CalculatorState* state, const double* a, double* result, uint32_t count) {
    double sin_buf[CHUNK_SIZE];
    double cos_buf[CHUNK_SIZE];
    
    for (uint32_t offset = 0; offset < count; offset += CHUNK_SIZE) {
        uint32_t current_chunk = (count - offset < CHUNK_SIZE) ? (count - offset) : CHUNK_SIZE;
        sin_sse(state, &a[offset], sin_buf, current_chunk);
        cos_sse(state, &a[offset], cos_buf, current_chunk);
        div_sse(state, sin_buf, cos_buf, &result[offset], current_chunk);
    }
}

TARGET_AVX2 void tan_avx2(CalculatorState* state, const double* a, double* result, uint32_t count) {
    double sin_buf[CHUNK_SIZE];
    double cos_buf[CHUNK_SIZE];
    
    for (uint32_t offset = 0; offset < count; offset += CHUNK_SIZE) {
        uint32_t current_chunk = (count - offset < CHUNK_SIZE) ? (count - offset) : CHUNK_SIZE;
        sin_avx2(state, &a[offset], sin_buf, current_chunk);
        cos_avx2(state, &a[offset], cos_buf, current_chunk);
        div_avx2(state, sin_buf, cos_buf, &result[offset], current_chunk);
    }
}

void tan_neon(CalculatorState* state, const double* a, double* result, uint32_t count) {
    double sin_buf[CHUNK_SIZE];
    double cos_buf[CHUNK_SIZE];
    
    for (uint32_t offset = 0; offset < count; offset += CHUNK_SIZE) {
        uint32_t current_chunk = (count - offset < CHUNK_SIZE) ? (count - offset) : CHUNK_SIZE;
        sin_neon(state, &a[offset], sin_buf, current_chunk);
        cos_neon(state, &a[offset], cos_buf, current_chunk);
        div_neon(state, sin_buf, cos_buf, &result[offset], current_chunk);
    }
}

void execute_tangent(CalculatorState* state, const double* a, double* result, uint32_t count, const CPUFeatures* features) {
    if (features->has_neon) {
        tan_neon(state, a, result, count);
    } else if (features->has_avx2) {
        tan_avx2(state, a, result, count);
    } else if (features->has_sse4_1) {
        // SSE4.1 required: tan_sse -> sin_sse/cos_sse emit roundpd/blendvpd.
        // On SSE2-only CPUs dispatch must fall through to scalar, not crash.
        tan_sse(state, a, result, count);
    } else {
        tan_scalar(state, a, result, count);
    }
}
