#include "colors.h"

#include <qumir/runtime/colors.h>

namespace NQumir {
namespace NRegistry {

using namespace NRuntime;

ColorsModule::ColorsModule() {
    auto integerType  = std::make_shared<NAst::TIntegerType>();
    auto boolType     = std::make_shared<NAst::TBoolType>();
    auto voidType     = std::make_shared<NAst::TVoidType>();
    auto stringType   = std::make_shared<NAst::TStringType>();
    auto intArrayType = std::make_shared<NAst::TArrayType>(integerType, 1);

    auto colorType = std::make_shared<NAst::TNamedType>("цвет", integerType);

    auto colorConst = [colorType](int64_t value) {
        return [colorType, value](std::vector<NAst::TExprPtr>) -> NAst::TExprPtr {
            auto expr = std::make_shared<NAst::TNumberExpr>(TLocation{}, value);
            expr->Type = colorType;
            return expr;
        };
    };

    auto intLiteral = [integerType](TLocation loc, int64_t value) -> NAst::TExprPtr {
        auto expr = std::make_shared<NAst::TNumberExpr>(std::move(loc), value);
        expr->Type = integerType;
        return expr;
    };

    auto colorPack = [integerType, colorType, intLiteral](NAst::TExprPtr r, NAst::TExprPtr g,
                                                          NAst::TExprPtr b, NAst::TExprPtr a) -> NAst::TExprPtr {
        auto bin = [](const char* op, NAst::TExprPtr left, NAst::TExprPtr right, NAst::TTypePtr type) -> NAst::TExprPtr {
            auto loc = left->Location;
            auto expr = std::make_shared<NAst::TBinaryExpr>(std::move(loc), NAst::TOperator(op),
                std::move(left), std::move(right));
            expr->Type = std::move(type);
            return expr;
        };
        auto mask = [&](NAst::TExprPtr value) {
            auto loc = value->Location;
            return bin("&", std::move(value), intLiteral(loc, 255), integerType);
        };
        auto shift = [&](NAst::TExprPtr value, int64_t bits) {
            auto loc = value->Location;
            return bin("<<", mask(std::move(value)), intLiteral(loc, bits), integerType);
        };
        auto bor = [&](NAst::TExprPtr left, NAst::TExprPtr right, NAst::TTypePtr type) {
            return bin("|", std::move(left), std::move(right), std::move(type));
        };

        return bor(
            bor(
                bor(shift(std::move(a), 24), shift(std::move(r), 16), integerType),
                shift(std::move(g), 8),
                integerType),
            mask(std::move(b)),
            colorType);
    };

    auto decomposeRgbTempId = std::make_shared<size_t>(0);
    auto decomposeRgb = [integerType, colorType, voidType, intLiteral, decomposeRgbTempId](std::vector<NAst::TExprPtr> args) -> NAst::TExprPtr {
        const auto colorName = "$$color_decompose_rgb_" + std::to_string((*decomposeRgbTempId)++);

        auto bin = [](const char* op, NAst::TExprPtr left, NAst::TExprPtr right, NAst::TTypePtr type) -> NAst::TExprPtr {
            auto loc = left->Location;
            auto expr = std::make_shared<NAst::TBinaryExpr>(std::move(loc), NAst::TOperator(op),
                std::move(left), std::move(right));
            expr->Type = std::move(type);
            return expr;
        };

        auto ident = [](TLocation loc, const std::string& name, NAst::TTypePtr type) -> NAst::TExprPtr {
            auto expr = std::make_shared<NAst::TIdentExpr>(std::move(loc), name);
            expr->Type = std::move(type);
            return expr;
        };

        auto component = [&](int64_t shift) -> NAst::TExprPtr {
            auto loc = args[0]->Location;
            NAst::TExprPtr value = ident(loc, colorName, integerType);
            if (shift != 0) {
                value = bin(">>", std::move(value), intLiteral(loc, shift), integerType);
            }
            return bin("&", std::move(value), intLiteral(loc, 255), integerType);
        };

        auto assignTarget = [&](const NAst::TExprPtr& target, NAst::TExprPtr value) -> NAst::TExprPtr {
            if (auto maybeIdent = NAst::TMaybeNode<NAst::TIdentExpr>(target)) {
                auto id = maybeIdent.Cast();
                return std::make_shared<NAst::TAssignExpr>(target->Location, id->Name, std::move(value));
            }
            if (auto maybeIndex = NAst::TMaybeNode<NAst::TIndexExpr>(target)) {
                auto index = maybeIndex.Cast();
                auto maybeCollection = NAst::TMaybeNode<NAst::TIdentExpr>(index->Collection);
                if (maybeCollection) {
                    return std::make_shared<NAst::TArrayAssignExpr>(
                        target->Location,
                        maybeCollection.Cast()->Name,
                        std::vector<NAst::TExprPtr>{index->Index},
                        std::move(value));
                }
            }
            if (auto maybeMultiIndex = NAst::TMaybeNode<NAst::TMultiIndexExpr>(target)) {
                auto index = maybeMultiIndex.Cast();
                auto maybeCollection = NAst::TMaybeNode<NAst::TIdentExpr>(index->Collection);
                if (maybeCollection) {
                    return std::make_shared<NAst::TArrayAssignExpr>(
                        target->Location,
                        maybeCollection.Cast()->Name,
                        index->Indices,
                        std::move(value));
                }
            }

            return std::make_shared<NAst::TCallExpr>(
                target->Location,
                std::make_shared<NAst::TIdentExpr>(target->Location, "__unsupported_inline_color_decompose_target"),
                std::vector<NAst::TExprPtr>{});
        };

        auto block = std::make_shared<NAst::TBlockExpr>(args[0]->Location, std::vector<NAst::TExprPtr>{});
        block->Type = voidType;

        auto colorVar = std::make_shared<NAst::TVarStmt>(args[0]->Location, colorName, integerType);
        colorVar->Type = integerType;
        block->Stmts.push_back(colorVar);
        auto colorValue = std::make_shared<NAst::TCastExpr>(args[0]->Location, args[0], integerType);
        colorValue->Type = integerType;
        block->Stmts.push_back(std::make_shared<NAst::TAssignExpr>(args[0]->Location, colorName, colorValue));
        block->Stmts.push_back(assignTarget(args[1], component(16)));
        block->Stmts.push_back(assignTarget(args[2], component(8)));
        block->Stmts.push_back(assignTarget(args[3], component(0)));
        return block;
    };

    auto makeOutInt = [&]() -> NAst::TTypePtr {
        auto t = std::make_shared<NAst::TIntegerType>();
        t->Mutable  = true;
        t->Readable = false;
        return std::make_shared<NAst::TReferenceType>(t);
    };

    ExternalTypes_ = {
        {
            .Name = "цвет",
            .Type = integerType,
        },
    };

    ExternalFunctions_ = {

        // ── Color constants ───────────────────────────────────────────────────
        {
            .Name = "прозрачный",
            .MangledName = "color_transparent",
            .ArgTypes = {},
            .ReturnType = colorType,
            .Inline = colorConst(PackARGB(0, 0, 0, 0)),
        },
        {
            .Name = "белый",
            .MangledName = "color_white",
            .ArgTypes = {},
            .ReturnType = colorType,
            .Inline = colorConst(PackRGB(255, 255, 255)),
        },
        {
            .Name = "чёрный",
            .MangledName = "color_black",
            .ArgTypes = {},
            .ReturnType = colorType,
            .Inline = colorConst(PackRGB(0, 0, 0)),
        },
        {
            .Name = "черный",
            .MangledName = "color_black",
            .ArgTypes = {},
            .ReturnType = colorType,
            .Inline = colorConst(PackRGB(0, 0, 0)),
        },
        {
            .Name = "серый",
            .MangledName = "color_gray",
            .ArgTypes = {},
            .ReturnType = colorType,
            .Inline = colorConst(PackRGB(128, 128, 128)),
        },
        {
            .Name = "фиолетовый",
            .MangledName = "color_purple",
            .ArgTypes = {},
            .ReturnType = colorType,
            .Inline = colorConst(PackRGB(128, 0, 128)),
        },
        {
            .Name = "синий",
            .MangledName = "color_blue",
            .ArgTypes = {},
            .ReturnType = colorType,
            .Inline = colorConst(PackRGB(0, 0, 255)),
        },
        {
            .Name = "голубой",
            .MangledName = "color_cyan",
            .ArgTypes = {},
            .ReturnType = colorType,
            .Inline = colorConst(PackRGB(0, 255, 255)),
        },
        {
            .Name = "зелёный",
            .MangledName = "color_green",
            .ArgTypes = {},
            .ReturnType = colorType,
            .Inline = colorConst(PackRGB(0, 128, 0)),
        },
        {
            .Name = "зеленый",
            .MangledName = "color_green",
            .ArgTypes = {},
            .ReturnType = colorType,
            .Inline = colorConst(PackRGB(0, 128, 0)),
        },
        {
            .Name = "жёлтый",
            .MangledName = "color_yellow",
            .ArgTypes = {},
            .ReturnType = colorType,
            .Inline = colorConst(PackRGB(255, 255, 0)),
        },
        {
            .Name = "желтый",
            .MangledName = "color_yellow",
            .ArgTypes = {},
            .ReturnType = colorType,
            .Inline = colorConst(PackRGB(255, 255, 0)),
        },
        {
            .Name = "оранжевый",
            .MangledName = "color_orange",
            .ArgTypes = {},
            .ReturnType = colorType,
            .Inline = colorConst(PackRGB(255, 165, 0)),
        },
        {
            .Name = "красный",
            .MangledName = "color_red",
            .ArgTypes = {},
            .ReturnType = colorType,
            .Inline = colorConst(PackRGB(255, 0, 0)),
        },

        // ── Color construction ────────────────────────────────────────────────
        {
            .Name = "RGB",
            .MangledName = "color_rgb",
            .ArgTypes = { integerType, integerType, integerType },
            .ReturnType = colorType,
            .Inline = [colorPack](std::vector<NAst::TExprPtr> args) -> NAst::TExprPtr {
                return colorPack(args[0], args[1], args[2],
                    std::make_shared<NAst::TNumberExpr>(args[0]->Location, int64_t{255}));
            },
        },
        {
            .Name = "RGBA",
            .MangledName = "color_rgba",
            .ArgTypes = { integerType, integerType, integerType, integerType },
            .ReturnType = colorType,
            .Inline = [colorPack](std::vector<NAst::TExprPtr> args) -> NAst::TExprPtr {
                return colorPack(args[0], args[1], args[2], args[3]);
            },
        },
        {
            .Name = "CMYK",
            .MangledName = "color_cmyk",
            .Ptr = reinterpret_cast<void*>(static_cast<int64_t(*)(int64_t,int64_t,int64_t,int64_t)>(color_cmyk)),
            .Packed = +[](const uint64_t* args, size_t) -> uint64_t {
                return static_cast<uint64_t>(color_cmyk(args[0], args[1], args[2], args[3]));
            },
            .ArgTypes = { integerType, integerType, integerType, integerType },
            .ReturnType = colorType,
        },
        {
            .Name = "CMYKA",
            .MangledName = "color_cmyka",
            .Ptr = reinterpret_cast<void*>(static_cast<int64_t(*)(int64_t,int64_t,int64_t,int64_t,int64_t)>(color_cmyka)),
            .Packed = +[](const uint64_t* args, size_t) -> uint64_t {
                return static_cast<uint64_t>(color_cmyka(args[0], args[1], args[2], args[3], args[4]));
            },
            .ArgTypes = { integerType, integerType, integerType, integerType, integerType },
            .ReturnType = colorType,
        },
        {
            .Name = "HSL",
            .MangledName = "color_hsl",
            .Ptr = reinterpret_cast<void*>(static_cast<int64_t(*)(int64_t,int64_t,int64_t)>(color_hsl)),
            .Packed = +[](const uint64_t* args, size_t) -> uint64_t {
                return static_cast<uint64_t>(color_hsl(args[0], args[1], args[2]));
            },
            .ArgTypes = { integerType, integerType, integerType },
            .ReturnType = colorType,
        },
        {
            .Name = "HSLA",
            .MangledName = "color_hsla",
            .Ptr = reinterpret_cast<void*>(static_cast<int64_t(*)(int64_t,int64_t,int64_t,int64_t)>(color_hsla)),
            .Packed = +[](const uint64_t* args, size_t) -> uint64_t {
                return static_cast<uint64_t>(color_hsla(args[0], args[1], args[2], args[3]));
            },
            .ArgTypes = { integerType, integerType, integerType, integerType },
            .ReturnType = colorType,
        },
        {
            .Name = "HSV",
            .MangledName = "color_hsv",
            .Ptr = reinterpret_cast<void*>(static_cast<int64_t(*)(int64_t,int64_t,int64_t)>(color_hsv)),
            .Packed = +[](const uint64_t* args, size_t) -> uint64_t {
                return static_cast<uint64_t>(color_hsv(args[0], args[1], args[2]));
            },
            .ArgTypes = { integerType, integerType, integerType },
            .ReturnType = colorType,
        },
        {
            .Name = "HSVA",
            .MangledName = "color_hsva",
            .Ptr = reinterpret_cast<void*>(static_cast<int64_t(*)(int64_t,int64_t,int64_t,int64_t)>(color_hsva)),
            .Packed = +[](const uint64_t* args, size_t) -> uint64_t {
                return static_cast<uint64_t>(color_hsva(args[0], args[1], args[2], args[3]));
            },
            .ArgTypes = { integerType, integerType, integerType, integerType },
            .ReturnType = colorType,
        },

        // ── Color decomposition ───────────────────────────────────────────────
        {
            .Name = "разложить в RGB",
            .MangledName = "color_decompose_rgb",
            .ArgTypes = { colorType, makeOutInt(), makeOutInt(), makeOutInt() },
            .ReturnType = voidType,
            .Inline = decomposeRgb,
        },
        {
            .Name = "разложить в CMYK",
            .MangledName = "color_decompose_cmyk",
            .Ptr = reinterpret_cast<void*>(static_cast<void(*)(int64_t,int64_t*,int64_t*,int64_t*,int64_t*)>(color_decompose_cmyk)),
            .Packed = +[](const uint64_t* args, size_t) -> uint64_t {
                color_decompose_cmyk(args[0],
                    reinterpret_cast<int64_t*>(args[1]),
                    reinterpret_cast<int64_t*>(args[2]),
                    reinterpret_cast<int64_t*>(args[3]),
                    reinterpret_cast<int64_t*>(args[4]));
                return 0;
            },
            .ArgTypes = { colorType, makeOutInt(), makeOutInt(), makeOutInt(), makeOutInt() },
            .ReturnType = voidType,
        },
        {
            .Name = "разложить в HSL",
            .MangledName = "color_decompose_hsl",
            .Ptr = reinterpret_cast<void*>(static_cast<void(*)(int64_t,int64_t*,int64_t*,int64_t*)>(color_decompose_hsl)),
            .Packed = +[](const uint64_t* args, size_t) -> uint64_t {
                color_decompose_hsl(args[0],
                    reinterpret_cast<int64_t*>(args[1]),
                    reinterpret_cast<int64_t*>(args[2]),
                    reinterpret_cast<int64_t*>(args[3]));
                return 0;
            },
            .ArgTypes = { colorType, makeOutInt(), makeOutInt(), makeOutInt() },
            .ReturnType = voidType,
        },
        {
            .Name = "разложить в HSV",
            .MangledName = "color_decompose_hsv",
            .Ptr = reinterpret_cast<void*>(static_cast<void(*)(int64_t,int64_t*,int64_t*,int64_t*)>(color_decompose_hsv)),
            .Packed = +[](const uint64_t* args, size_t) -> uint64_t {
                color_decompose_hsv(args[0],
                    reinterpret_cast<int64_t*>(args[1]),
                    reinterpret_cast<int64_t*>(args[2]),
                    reinterpret_cast<int64_t*>(args[3]));
                return 0;
            },
            .ArgTypes = { colorType, makeOutInt(), makeOutInt(), makeOutInt() },
            .ReturnType = voidType,
        },

        // ── Output operator ───────────────────────────────────────────────────
        {
            .Name = "вывод",
            .MangledName = "color_print",
            .Ptr = reinterpret_cast<void*>(static_cast<void(*)(int64_t)>(color_print)),
            .Packed = +[](const uint64_t* args, size_t) -> uint64_t {
                color_print(static_cast<int64_t>(args[0]));
                return 0;
            },
            .ArgTypes = { colorType },
            .ReturnType = voidType,
            .IsOp = true,
        },
    };
}

} // namespace NRegistry
} // namespace NQumir
