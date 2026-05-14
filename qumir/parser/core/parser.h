#pragma once

#include <qumir/error.h>
#include <qumir/parser/ast.h>
#include <qumir/parser/core/lexer.h>

#include <expected>

namespace NQumir {
namespace NAst {
namespace NCore {

class TParser {
public:
    std::expected<TExprPtr, TError> Parse(TTokenStream& stream);
};

} // namespace NCore
} // namespace NAst
} // namespace NQumir
