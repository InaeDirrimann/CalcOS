#include "PrintFloat.h"
#include "PrintInt.h"
#include <float.h>

// Fixed-point printer. Caller must guarantee |val| < 1e16 so the uint64
// integer-part cast stays well inside defined range. Adds half-up rounding
// at `precision` decimals.
static size_t print_float_fixed(char* buf, double val, int precision) {
    size_t idx = 0;
    if (val < 0.0) {
        buf[idx++] = '-';
        val = -val;
    }

    double round = 0.5;
    for (int i = 0; i < precision; i++) {
        round /= 10.0;
    }
    val += round;

    uint64_t int_part = (uint64_t)val;
    double frac_part = val - (double)int_part;

    char int_buf[32];
    size_t int_len = print_int(int_buf, (int64_t)int_part, 10, false);
    for (size_t i = 0; i < int_len; i++) {
        buf[idx++] = int_buf[i];
    }

    if (precision > 0) {
        buf[idx++] = '.';
        for (int i = 0; i < precision; i++) {
            frac_part *= 10.0;
            uint32_t digit = (uint32_t)frac_part;
            buf[idx++] = (char)('0' + (digit % 10));
            frac_part -= digit;
        }
    }
    buf[idx] = '\0';
    return idx;
}

// Scientific notation for magnitudes fixed-point can't show faithfully.
// Mantissa normalized to [1, 10), exponent printed as e+NN / e-NN.
static size_t print_float_sci(char* buf, double val, int precision) {
    size_t idx = 0;
    if (val < 0.0) {
        buf[idx++] = '-';
        val = -val;
    }

    double m = val;
    int exp10 = 0;
    while (m >= 10.0) { m *= 0.1; exp10++; }
    while (m < 1.0)   { m *= 10.0; exp10--; }

    // If the mantissa rounds up to 10.000000 (e.g. 9.9999996), renormalize.
    char tmp[40];
    print_float_fixed(tmp, m, precision);
    if (tmp[0] == '1' && tmp[1] == '0') {
        m = 1.0;
        exp10++;
        print_float_fixed(tmp, m, precision);
    }
    for (size_t i = 0; tmp[i]; i++) buf[idx++] = tmp[i];

    buf[idx++] = 'e';
    if (exp10 < 0) {
        buf[idx++] = '-';
        exp10 = -exp10;
    } else {
        buf[idx++] = '+';
    }
    char exp_buf[8];
    size_t exp_len = print_int(exp_buf, exp10, 10, false);
    for (size_t i = 0; i < exp_len; i++) buf[idx++] = exp_buf[i];
    buf[idx] = '\0';
    return idx;
}

size_t print_float(char* buf, double val, int precision) {
    // Clamp: more than 15 fraction digits are meaningless for double and
    // would overflow the fixed-point scratch buffers.
    if (precision < 0) precision = 0;
    if (precision > 15) precision = 15;

    if (val != val) {
        buf[0] = 'N'; buf[1] = 'a'; buf[2] = 'N'; buf[3] = '\0';
        return 3;
    }
    // True infinity only (beyond DBL_MAX). Finite values that merely exceed
    // uint64 range were previously mislabeled "Inf" here.
    if (val > DBL_MAX) {
        buf[0] = 'I'; buf[1] = 'n'; buf[2] = 'f'; buf[3] = '\0';
        return 3;
    }
    if (val < -DBL_MAX) {
        buf[0] = '-'; buf[1] = 'I'; buf[2] = 'n'; buf[3] = 'f'; buf[4] = '\0';
        return 4;
    }

    // Fixed-point only where the digits are meaningful. Huge values (2^100)
    // and values that would print as all-zero at this precision go
    // scientific instead of "Inf"/"0" (e.g. 1e-8 at precision 6 -> 0.000000).
    double small = 1.0;
    for (int i = 0; i < precision; i++) small /= 10.0;  // 10^-precision
    if (val >= 1e16 || val <= -1e16 || (val != 0.0 && val > -small && val < small)) {
        return print_float_sci(buf, val, precision);
    }
    return print_float_fixed(buf, val, precision);
}
