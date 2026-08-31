#include "TestAssert.h"
#include "../../Kernel/State/CalculatorState.h"
#include "../../Kernel/Operations/Arithmetic/Subtraction.h"
#include "../../Kernel/Core/CPU/CPUID.h"
#include "../../Infrastructure/Utils/MemoryUtils.h"

int main() {
    printf("=== RUNNING TestSubtraction ===\n");

    CalculatorState state;
    fast_memset(&state, 0, sizeof(CalculatorState));

    double a[4] = {10.0, 5.5, -3.0, 0.0};
    double b[4] = {2.0, 7.5, -4.0, 5.5};
    double res[4] = {0.0};

    // 1. Test scalar subtraction
    sub_scalar(&state, a, b, res, 4);
    assert_double_eq(res[0], 8.0);
    assert_double_eq(res[1], -2.0);
    assert_double_eq(res[2], 1.0);
    assert_double_eq(res[3], -5.5);

    // 2. Test SSE subtraction
    fast_memset(res, 0, sizeof(res));
    sub_sse(&state, a, b, res, 4);
    assert_double_eq(res[0], 8.0);
    assert_double_eq(res[1], -2.0);
    assert_double_eq(res[2], 1.0);
    assert_double_eq(res[3], -5.5);

    // 3. Test AVX2 subtraction (direct call SIGILLs on non-AVX2 CPUs, so probe first)
    CPUFeatures features;
    cpu_detect_features(&features);
    if (features.has_avx2) {
        fast_memset(res, 0, sizeof(res));
        sub_avx2(&state, a, b, res, 4);
        assert_double_eq(res[0], 8.0);
        assert_double_eq(res[1], -2.0);
        assert_double_eq(res[2], 1.0);
        assert_double_eq(res[3], -5.5);
    } else {
        printf("  (skipped: no AVX2 on this CPU)\n");
    }

    // Print summary
    printf("TestSubtraction summary: %d/%d assertions passed.\n", 
           total_assertions - failed_assertions, total_assertions);
    return failed_assertions > 0 ? 1 : 0;
}
