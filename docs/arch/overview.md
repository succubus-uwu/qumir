# Qumir Compiler — Architecture Overview

Qumir is an educational programming language compiler with Russian keywords,
inspired by the Soviet-era KUMIR/Ershov teaching language.  It supports three
execution backends — LLVM native, WASM, and an IR interpreter (VM) — and an
online playground at <https://qumir.dev>.

---

## 1. Pipeline at a glance

```
Source (.kum / core-lang)
        │
        ▼
    [ Lexer / Parser ]
        │  raw AST
        ▼
 ┌─ Semantic passes ──────────────────────┐
 │  1. Name resolution                    │
 │  2. Type annotation                    │
 │  3. Definite assignment                │
 │  4. Transform (AST rewrites)           │
 └────────────────────────────────────────┘
        │  annotated AST
        ▼
  [ IR lowering ]  →  TModule (SSA IR)
        │
        ├── [ IR passes ]  (SSA, const-fold, de-SSA, …)
        │
        ├──────────────────────────────────────┐
        ▼                                      ▼
  [ VM / interpreter ]              [ LLVM codegen ]
  direct execution                  native binary / WebAssembly
```

The CLI tools are:
- **`qumiri`** — interpreter / LLVM JIT
- **`qumirc`** — compiler driver

The web service wraps `qumirc` as a subprocess and serves the playground
frontend.

---

## 2. Two surface languages

### 2.1 Kumir (`.kum`)

The primary user-facing language.  Keywords are Cyrillic:

| Concept       | Keyword(s)                                   |
|---------------|----------------------------------------------|
| Algorithm     | `алг` / `нач` / `кон`                        |
| Types         | `цел` `вещ` `лог` `лит` `сим` `таб`         |
| If / else     | `если` / `то` / `иначе` / `все`             |
| Loop          | `нц` / `кц` / `пока` / `для` / `шаг`       |
| Switch        | `выбор` / `при`                              |
| Param modes   | `арг` (in)  `рез`/`знач` (out)  `аргрез`   |
| Module import | `использовать`                               |

### 2.2 Core lang (internal)

A lower-level, ASCII-only representation of the same AST used for tests and
debugging.  The playground exposes it as a "core syntax" mode.  It allows
writing test fixtures and viewing the de-sugared program structure without
going through the full Cyrillic front-end.

---

## 3. AST and type system

### 3.1 Node hierarchy

All AST nodes derive from `TExpr`.  Pattern matching uses `TMaybeNode<T>` —
a wrapper that holds a `shared_ptr<T>` when the dynamic type matches:

```cpp
if (auto call = TMaybeNode<TCallExpr>(expr)) { /* call.Cast() */ }
```

### 3.2 Type hierarchy

Types are `shared_ptr<TType>` subtypes, matched with `TMaybeType<T>`:

| C++ type           | Language concept                       |
|--------------------|----------------------------------------|
| `TIntegerType`     | `цел` — one 64-bit signed integer      |
| `TFloatType`       | `вещ` — IEEE-754 double                |
| `TBoolType`        | `лог` — boolean                        |
| `TStringType`      | `лит` — managed string handle          |
| `TSymbolType`      | `сим` — Unicode code point (i32)       |
| `TVoidType`        | procedure return / no value            |
| `TArrayType`       | `таб` — heap-allocated array           |
| `TNamedType`       | user-defined type alias                |
| `TFunctionType`    | `(param…) → ret`                       |
| `TPointerType`     | internal low-level pointer             |

---

## 4. Semantic passes

All four passes run in order via `NTransform::Pipeline()`.

### 4.1 Name resolution

Builds a symbol table and resolves every identifier to its declaration.
Module imports (`использовать`) are processed here; imported names are
registered before parsing the user's program body.

### 4.2 Type annotation

Infers and checks types bottom-up.  After this pass every expression node
carries a `TTypePtr`.

### 4.3 Definite assignment

Detects uses of potentially-uninitialised variables via dataflow analysis.

### 4.4 Transform

AST-level rewrites before IR lowering: array bound normalisation, syntactic
sugar elimination, `Loop` → `while` conversion.

---

## 5. IR (intermediate representation)

The IR is a custom SSA-like representation.

### 5.1 Structure

```
TModule
├── ExternalFunctions[]   imported runtime symbols
├── Functions[]
│   ├── Blocks[]
│   │   ├── Phis[]        φ-functions (SSA merge points)
│   │   └── Instrs[]      sequential instructions
│   └── LocalTypes[]      local variable type ids
├── GlobalTypes[]         module-level slot types
├── StringLiterals[]      constant pool
└── Types (TTypeTable)    type interning table
```

### 5.2 Instruction format

```cpp
struct TInstr {
    TOp      Op;             // opcode: "add", "call", "ret", "jmp", "cmp", …
    TTmp     Dest;           // destination temporary (-1 = no result)
    TOperand Operands[4];    // Tmp | Imm | Label | Local | Slot
};
```

Opcodes are string-literal hashes (`"add"_op`, `"call"_op`, …) so new
opcodes can be added without touching an enum.

### 5.3 IR passes

| Pass           | Purpose                                   |
|----------------|-------------------------------------------|
| `locals2ssa`   | stack locals → SSA temporaries            |
| `const_fold`   | constant folding                          |
| `de_ssa`       | insert copies at φ-joins before codegen   |
| renumber       | compact temporary indices                 |
| CFG analysis   | predecessor / successor computation       |

### 5.4 IR type table

`TTypeTable` interns IR-level types by kind: `I1`, `I8`, `I32`, `I64`,
`F64`, `Void`, `Ptr`, `Struct`, `Func`.  AST types map to IR types via
`FromAstType()`.

---

## 6. VM / interpreter

The VM is a register-based bytecode interpreter.  The IR is compiled to
`TVMInstr` bytecode by `TVMCompiler`, which assigns stack-frame byte offsets
to locals and temporaries.

### 6.1 Execution model

Each function call creates a stack frame (a heap-allocated byte buffer).
The interpreter loop dispatches on `EVMOp` enum values.  Frames are linked
(parent frame pointer stored at offset 0).

### 6.2 Calling convention for external (runtime) functions

External functions — robot actions, math helpers, string operations — are
registered by modules.  Each external function has **two representations**:

| Field    | Type                                          | Used by        |
|----------|-----------------------------------------------|----------------|
| `Ptr`    | `void*` — native function pointer             | LLVM backend   |
| `Packed` | `uint64_t(*)(const uint64_t* args, size_t n)` | VM interpreter |

The **`Packed` thunk** is the VM calling convention.  All arguments are
passed as a flat array of `uint64_t` values (integers, floats as bit-casts,
string handles, pointers as integers).  The return value is a single
`uint64_t`.

```cpp
// Typical registration
{
    .Name        = "вверх",
    .MangledName = "robot_up",
    .Ptr         = reinterpret_cast<void*>(static_cast<void(*)()>(robot_up)),
    .Packed      = [](const uint64_t*, size_t) -> uint64_t {
                       robot_up(); return 0;
                   },
}
```

The VM's `ECall` handler casts `addr` to `TPacked` and calls it directly,
collecting pre-pushed arguments from `Runtime.Args`.

Additional flags on external functions:

- **`RequireArgsMaterialization`** — string arguments are dereferenced from
  their handle before the call, so functions that expect `const char*` work
  without extra wrapping.
- **`Inline`** — an optional `TInlineFactory` that the type-annotation pass
  can use to replace the call with a different AST subtree.  Only the VM
  uses `Inline`; the LLVM/WASM backends continue to use `Ptr`.

### 6.3 LLVM JIT

The LLVM JIT runner compiles the IR module through the LLVM backend and runs
it in-process via LLVM ORC JIT.  External functions are resolved to their
`Ptr` addresses at link time.

---

## 7. LLVM backend

Translates `TModule` to LLVM IR and then to native code or WebAssembly.

### 7.1 Function lowering

IR basic blocks map directly to LLVM basic blocks.  IR temporaries become
LLVM values (`alloca`-free after the de-SSA pass).  Struct arguments and
returns are modelled as ordinary LLVM values.

### 7.2 Target

| Flag      | Triple                   | Notes                          |
|-----------|--------------------------|--------------------------------|
| *(none)*  | host triple              | native AOT or JIT              |
| `--wasm`  | `wasm32-unknown-unknown` | requires `wasm-ld` for linking |

### 7.3 Optimization

`cg.Emit(module, optLevel)` runs the standard LLVM pass pipeline at the
requested level (0–3).

---

## 8. Module system

User programs import modules with `использовать <Name>`.  Each module
implements `IModule` and registers external functions and types.

Built-in modules:

| Module             | Purpose                                   |
|--------------------|-------------------------------------------|
| System (default)   | I/O, file I/O, math, string operations    |
| Робот              | Grid robot executor                       |
| Черепаха           | Turtle graphics executor                  |
| Рисователь         | Canvas drawing                            |
| Чертежник          | Vector drawing                            |
| Комплексные числа  | Complex number support                    |
| Цвета              | Named color constants                     |

Runtime implementations exist in two forms: C++ (native / WASM) and
JavaScript (browser playground).

---

## 9. Array Representation

Kumir arrays are written with `таб`:

```kumir
цел таб v[1:n]
вещ таб a[0:n-1, 0:m-1]
```

The AST type is `TArrayType(elementType, arity)`.  The bounds are not part of
the type identity itself; they live on the declaration (`TVarStmt` /
function parameter) as one `[left:right]` pair per dimension.  In core/AST
goldens this shows up as a type plus a separate bounds list:

```text
(var a <array f64 2> [0 (- n 1)] [0 (- m 1)])
```

### 9.1 Runtime Value

At IR/LLVM level an array value is just a pointer to the first element:

```cpp
FromAstType(TArrayType(T, n)) == Ptr(FromAstType(T))
```

There is no array header in the runtime allocation: no length, no bounds, no
rank, and no stride table are stored next to the elements.  Allocation is done
through the System runtime:

```cpp
void* array_create(size_t sizeInBytes);
void  array_destroy(void* ptr);
void  array_str_destroy(void* ptr, size_t arraySize);
```

`array_create` allocates an 8-byte-aligned zero-filled byte buffer.  Ordinary
arrays are destroyed with `array_destroy`.  Arrays of `лит` are destroyed with
`array_str_destroy`, which releases every stored string pointer before freeing
the buffer.

### 9.2 Bounds and Layout Metadata

Because the runtime pointer has no header, the compiler creates separate
hidden storage for array layout metadata.  During IR lowering,
`LowerArrayLayout` evaluates each declared bound and stores, per dimension:

| Field      | Meaning                                      |
|------------|----------------------------------------------|
| lower bound | declared left bound, e.g. `1` in `[1:n]`    |
| dim size   | `right - left + 1`                           |
| stride     | cumulative element count from this dimension |

The same layout lowering runs for local/global array declarations and for
array parameters inside the callee.  For function parameters, the declared
parameter bounds are expressions in the callee scope, commonly using scalar
size parameters:

```kumir
алг matvec(цел n, вещ таб x[0:n-1], рез вещ таб y[0:n-1])
```

Here `n` is passed separately, and the callee reconstructs the layout metadata
for `x` and `y` from its parameter declarations.

### 9.3 Element Addressing

Arrays are flattened into one row-major allocation: the last dimension is
contiguous.  For an access `a[i0, i1, ..., ik]`, lowering computes a byte
offset equivalent to:

```text
linear =
    (i0 - lb0) * size1 * size2 * ... * sizek
  + (i1 - lb1) * size2 * ... * sizek
  + ...
  + (ik - lbk)

byteOffset = linear * sizeof(element)
address    = basePointer + byteOffset
```

For example, `вещ таб a[0:n-1, 0:m-1]` stores `a[i,j]` at:

```text
base + ((i * m) + j) * 8
```

The implementation emits normal pointer arithmetic in IR:

```text
offset = LowerIndices(symbol, indices, elemByteSize)
addr   = arrayPtr + offset
```

Loads use `lde`; scalar stores use `ste`; struct elements are copied with
`copy` because the element value may be address-backed.

### 9.4 Passing Arrays to Functions

Array arguments are passed by pointer.  The callee receives the same backing
allocation that the caller owns; no element copy is made at the call boundary.
This is why procedures can fill output arrays efficiently:

```kumir
алг fill(цел n, рез цел таб a[0:n-1])
нач
  цел i
  нц для i от 0 до n-1
    a[i] := i
  кц
кон
```

Parameter modes are enforced by semantic type flags:

| Mode       | Effect for arrays                                      |
|------------|---------------------------------------------------------|
| `арг`      | input array; elements may be read according to type flags |
| `рез`      | output array; reading elements is rejected              |
| `арг рез`  | in/out array; elements may be read and written          |

The ABI is still pointer-based in all three cases.  The difference is
semantic checking and generated read/write permissions, not a different
runtime representation.

---

## 10. String representation

### 10.1 Native `TString` (C++ / WASM heap)

Every mutable string is a heap-allocated `TString` object:

```cpp
struct TString {
    int*    Utf8Indices;  // lazy: symbol index → byte offset in Data
    int64_t Symbols;      // Unicode codepoint count (filled by Utf8Indices build)
    int64_t Rc;           // reference count
    int64_t Length;       // byte length of the UTF-8 payload
    char    Data[0];      // flexible array — the UTF-8 bytes, NUL-terminated
};
```

The value that is **passed around at runtime is `char* TString::Data`** —
a pointer into the *middle* of the struct, just past the header.  To reach
the header from a data pointer: `(TString*)(ptr - offsetof(TString, Data))`.

This means a string value looks like a plain C string to any code that just
reads bytes, while the reference count and metadata live at negative offsets
before it.

**Reference counting** — `str_retain(char*)` increments `Rc`;
`str_release(char*)` decrements it and frees the whole allocation (header +
data) when it reaches zero.

**Unicode indexing** — Kumir strings are 1-indexed sequences of Unicode
codepoints.  `Utf8Indices` is built lazily on the first symbol-position
access (`str_symbol_at`, `str_slice`, …).  Once built, it maps
`Utf8Indices[i]` → byte offset of the *i*-th codepoint start inside `Data`.

### 10.2 String literals

Compile-time string literals are emitted as read-only LLVM globals containing
raw UTF-8 bytes.  Their pointers are used directly — **no `TString` header,
no reference count**.  All string runtime functions that accept a `const char*`
argument check for this case: a read-only literal is never passed to
`str_release`.

### 10.3 Browser / WASM dual-handle scheme

The JavaScript `string.js` runtime splits string values into two namespaces
by sign of the `int32` handle:

| Handle value | Meaning                                                    |
|--------------|------------------------------------------------------------|
| ≥ 0          | Pointer into WASM linear memory — a `TString::Data` address or a literal `const char*` |
| < 0          | Index into a JavaScript-managed `STRING_POOL` Map          |

The JS pool stores entries of the form
`{ value: string, refs: number, positions: Array|null }`.  `positions` is the
JS-side equivalent of `Utf8Indices`: a lazy array of UTF-16 index offsets for
Unicode-safe character operations.

Negative handles arise when JS-side string operations (e.g. reading from an
`<input>`) need to produce a string that the WASM code can use as a `лит`
value.  Reference counting for negative handles is managed entirely in JS.

`result.js` interprets handles with the same rules as `string.js` so that
both the interpreter and WASM paths display string return values consistently
in the playground.
