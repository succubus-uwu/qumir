#include "type.h"
#include "qumir/parser/core/printer.h"

#include <memory>

namespace NQumir {
namespace NAst {

std::ostream& operator<<(std::ostream& os, const TType& expr)
{
    TTypePtr ptr(const_cast<TType*>(&expr), [](TType*) {});
    NCore::PrintType(os, std::move(ptr));
    return os;
}

} // namespace NAst
} // namespace NQumir
