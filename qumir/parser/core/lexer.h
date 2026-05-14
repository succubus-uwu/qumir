#pragma once

#include <qumir/location.h>
#include <qumir/parser/lexer_base.h>

#include <cstdint>
#include <deque>
#include <istream>
#include <string>

namespace NQumir {
namespace NAst {
namespace NCore {

class TTokenStream : public ITokenStream {
public:
    explicit TTokenStream(std::istream& in);

private:
    void Read() override;
    bool AfterOpenParen_ = false; // track previous token to disambiguate >> inside (>> a b)
};

} // namespace NCore
} // namespace NAst
} // namespace NQumir
