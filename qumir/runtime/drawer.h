#pragma once

#include <cstddef>
#include <cstdint>

namespace NQumir {
struct ITypeErasedFuture;

namespace NRuntime {

extern "C" {

void drawer_pen_up();
void drawer_pen_down();
void drawer_set_color(int64_t color);
ITypeErasedFuture* drawer_move_to(double x, double y);
ITypeErasedFuture* drawer_move_by(double dx, double dy);
ITypeErasedFuture* drawer_write_text(double width, const char* text);
size_t drawer_process_events();

} // extern "C"

} // namespace NRuntime
} // namespace NQumir
