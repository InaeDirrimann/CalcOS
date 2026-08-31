#ifndef LOGARITHM_H
#define LOGARITHM_H

#include "../../State/CalculatorState.h"
#include "../../Core/CPU/CPUID.h"

// Scalar logarithm fallback
void log_scalar(CalculatorState* state, const double* a, double* result, uint32_t count);

// Dynamic dispatch wrapper
void execute_logarithm(CalculatorState* state, const double* a, double* result, uint32_t count, const CPUFeatures* features);

#endif // LOGARITHM_H
