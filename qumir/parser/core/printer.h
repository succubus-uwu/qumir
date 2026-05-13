#pragma once

#include <iosfwd>
#include <set>
#include <string>

#include <qumir/parser/ast.h>
#include <qumir/parser/type.h>

namespace NQumir {
namespace NAst {
namespace NCore {

enum class ETypePrintMode {
    Required,
    All,
};

struct TPrintOptions {
    ETypePrintMode TypeMode = ETypePrintMode::Required;
    bool Pretty = true;
    int IndentStep = 2;
    size_t LineWidth = 120;
    std::set<std::string> ShortNamedTypes;
};

void PrintAst(std::ostream& out, TExprPtr expr, const TPrintOptions& options = {});
std::string PrintAst(TExprPtr expr, const TPrintOptions& options = {});

void PrintType(std::ostream& out, TTypePtr type, const TPrintOptions& options = {});
std::string PrintType(TTypePtr type, const TPrintOptions& options = {});

} // namespace NCore

std::ostream& operator<<(std::ostream& out, const TExprPtr& expr);

} // namespace NAst
} // namespace NQumir
