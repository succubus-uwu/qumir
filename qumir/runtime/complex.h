#pragma once

#include <cstdint>

namespace NQumir {
namespace NRuntime {

// Memory layout of компл: two consecutive doubles
struct komplex {
    double re;
    double im;
};

extern "C" {

double complex_abs(komplex a);
double complex_arg(komplex a);
void   complex_print(komplex a);

} // extern "C"

} // namespace NRuntime
} // namespace NQumir
