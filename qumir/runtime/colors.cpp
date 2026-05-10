#include "colors.h"
#include "io.h"

#include <iomanip>
#include <sstream>

namespace NQumir {
namespace NRuntime {

extern "C" {

void color_print(int64_t color) {
    const uint32_t c = static_cast<uint32_t>(color);
    const int r = (c >> 16) & 0xFF;
    const int g = (c >>  8) & 0xFF;
    const int b =  c        & 0xFF;
    std::ostringstream oss;
    oss << '#' << std::hex << std::uppercase << std::setfill('0')
        << std::setw(2) << r << std::setw(2) << g << std::setw(2) << b;
    *GetOutputStream() << oss.str();
}

} // extern "C"

} // namespace NRuntime
} // namespace NQumir
