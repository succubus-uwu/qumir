#include "complex.h"
#include "io.h"

#include <cmath>

namespace NQumir {
namespace NRuntime {

extern "C" {

double complex_abs(komplex a) { return std::sqrt(a.re * a.re + a.im * a.im); }
double complex_arg(komplex a) { return std::atan2(a.im, a.re); }

void complex_print(komplex a) {
    auto& out = *GetOutputStream();
    out << a.re;
    if (a.im >= 0.0) {
        out << '+';
    }
    out << a.im << 'i';
}

} // extern "C"

} // namespace NRuntime
} // namespace NQumir
