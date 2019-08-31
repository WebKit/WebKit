/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WHLSLMetalCodeGenerator.h"

#if ENABLE(WEBGPU)

#include "WHLSLFunctionWriter.h"
#include "WHLSLTypeNamer.h"
#include <wtf/DataLog.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

namespace WHLSL {

namespace Metal {

static constexpr bool dumpMetalCode = false;

static StringView metalCodePrologue()
{
    return StringView {
        "#include <metal_stdlib>\n"
        "#include <metal_atomic>\n"
        "#include <metal_math>\n"
        "#include <metal_relational>\n"
        "#include <metal_compute>\n"
        "#include <metal_texture>\n"
        "\n"
        "using namespace metal;\n"
        "\n"
        "template <typename T, int Cols, int Rows>\n"
        "struct WSLMatrix\n"
        "{\n"
        "    vec<T, Rows> columns[Cols];\n"
        "    private:\n"
        "    template <typename U, int... R>\n"
        "    static vec<T, Rows> build_col(initializer_list<U> col, _integer_sequence<int, R...>)\n"
        "    {\n"
        "        return {(R < col.size() ? *(col.begin() + R) : U())...};\n"
        "    }\n"
        "    template <int... R>\n"
        "    static vec<T, Rows> build_full_col(int c, initializer_list<T> elems, _integer_sequence<int, R...>)\n"
        "    {\n"
        "        return {*(elems.begin() + c * Rows + R)...};\n"
        "    }\n"
        "    struct cols_init_tag { };\n"
        "    struct cols_all_tag { };\n"
        "    struct elems_all_tag { };\n"
        "    template <int... C>\n"
        "    inline explicit WSLMatrix(cols_init_tag, initializer_list<vec<T, Rows>> cols, _integer_sequence<int, C...>) thread\n"
        "        : columns{(C < cols.size() ? *(cols.begin() + C) : vec<T, Rows>())...}\n"
        "    {\n"
        "    }\n"
        "    template <typename... U>\n"
        "    inline explicit WSLMatrix(cols_all_tag, U... cols) thread\n"
        "        : columns{ cols... }\n"
        "    {\n"
        "    }\n"
        "    template <typename... U>\n"
        "    inline explicit WSLMatrix(elems_all_tag, U... elems) thread\n"
        "        : WSLMatrix({T(elems)...}, _make_integer_sequence<int, Cols>())\n"
        "        {\n"
        "        }\n"
        "    template <int... C>\n"
        "    inline explicit WSLMatrix(initializer_list<T> elems, _integer_sequence<int, C...>) thread\n"
        "        : columns{build_full_col(C, elems, _make_integer_sequence<int, Rows>())...}\n"
        "    {\n"
        "    }\n"
        "    template <int... C>\n"
        "    inline explicit WSLMatrix(cols_init_tag, initializer_list<vec<T, Rows>> cols, _integer_sequence<int, C...>) constant\n"
        "        : columns{(C < cols.size() ? *(cols.begin() + C) : vec<T, Rows>())...}\n"
        "    {\n"
        "    }\n"
        "    template <typename... U>\n"
        "    inline explicit WSLMatrix(cols_all_tag, U... cols) constant\n"
        "        : columns{ cols... }\n"
        "    {\n"
        "    }\n"
        "    template <typename... U>\n"
        "    inline explicit WSLMatrix(elems_all_tag, U... elems) constant\n"
        "        : WSLMatrix({T(elems)...}, _make_integer_sequence<int, Cols>())\n"
        "        {\n"
        "        }\n"
        "    template <int... C>\n"
        "    inline explicit WSLMatrix(initializer_list<T> elems, _integer_sequence<int, C...>) constant\n"
        "        : columns{build_full_col(C, elems, _make_integer_sequence<int, Rows>())...}\n"
        "    {\n"
        "    }\n"
        "    public:\n"
        "    inline WSLMatrix() thread = default;\n"
        "    inline WSLMatrix(initializer_list<vec<T, Rows>> cols) thread\n"
        "        : WSLMatrix(cols_init_tag(), cols, _make_integer_sequence<int, Cols>())\n"
        "    {\n"
        "    }\n"
        "    template <typename... U>\n"
        "    inline explicit WSLMatrix(U... vals) thread\n"
        "        : WSLMatrix(conditional_t<sizeof...(U) == Cols, cols_all_tag, elems_all_tag>(), vals...)\n"
        "    {\n"
        "    }\n"
        "    inline WSLMatrix() constant = default;\n"
        "    inline WSLMatrix(initializer_list<vec<T, Rows>> cols) constant\n"
        "        : WSLMatrix(cols_init_tag(), cols, _make_integer_sequence<int, Cols>())\n"
        "    {\n"
        "    }\n"
        "    inline explicit WSLMatrix(T val) constant\n"
        "        : WSLMatrix(val, _make_integer_sequence<int, Cols>())\n"
        "    {\n"
        "    }\n"
        "    template <typename... U>\n"
        "    inline explicit WSLMatrix(U... vals) constant\n"
        "        : WSLMatrix(conditional_t<sizeof...(U) == Cols, cols_all_tag, elems_all_tag>(), vals...)\n"
        "    {\n"
        "    }\n"
        "    inline WSLMatrix(const thread WSLMatrix<T, Cols, Rows> &that) thread = default;\n"
        "    template <typename U>\n"
        "    inline explicit WSLMatrix(const thread WSLMatrix<U, Cols, Rows> &that) thread\n"
        "        : WSLMatrix(that, _make_integer_sequence<int, Cols>())\n"
        "    {\n"
        "    }\n"
        "    inline WSLMatrix(const device WSLMatrix<T, Cols, Rows> &that) thread = default;\n"
        "    template <typename U>\n"
        "    inline explicit WSLMatrix(const device WSLMatrix<U, Cols, Rows> &that) thread\n"
        "        : WSLMatrix(that, _make_integer_sequence<int, Cols>())\n"
        "    {\n"
        "    }\n"
        "    inline WSLMatrix(const constant WSLMatrix<T, Cols, Rows> &that) thread = default;\n"
        "    template <typename U>\n"
        "    inline explicit WSLMatrix(const constant WSLMatrix<U, Cols, Rows> &that) thread\n"
        "        : WSLMatrix(that, _make_integer_sequence<int, Cols>())\n"
        "    {\n"
        "    }\n"
        "    inline WSLMatrix(const threadgroup WSLMatrix<T, Cols, Rows> &that) thread = default;\n"
        "    template <typename U>\n"
        "    inline explicit WSLMatrix(const threadgroup WSLMatrix<U, Cols, Rows> &that) thread\n"
        "        : WSLMatrix(that, _make_integer_sequence<int, Cols>())\n"
        "    {\n"
        "    }\n"
        "    inline WSLMatrix(const thread WSLMatrix<T, Cols, Rows> &that) constant = default;\n"
        "    template <typename U>\n"
        "    inline explicit WSLMatrix(const thread WSLMatrix<U, Cols, Rows> &that) constant\n"
        "        : WSLMatrix(that, _make_integer_sequence<int, Cols>())\n"
        "    {\n"
        "    }\n"
        "    inline WSLMatrix(const device WSLMatrix<T, Cols, Rows> &that) constant = default;\n"
        "    template <typename U>\n"
        "    inline explicit WSLMatrix(const device WSLMatrix<U, Cols, Rows> &that) constant\n"
        "        : WSLMatrix(that, _make_integer_sequence<int, Cols>())\n"
        "    {\n"
        "    }\n"
        "    inline WSLMatrix(const constant WSLMatrix<T, Cols, Rows> &that) constant = default;\n"
        "    template <typename U>\n"
        "    inline explicit WSLMatrix(const constant WSLMatrix<U, Cols, Rows> &that) constant\n"
        "        : WSLMatrix(that, _make_integer_sequence<int, Cols>())\n"
        "    {\n"
        "    }\n"
        "    inline WSLMatrix(const threadgroup WSLMatrix<T, Cols, Rows> &that) constant = default;\n"
        "    template <typename U>\n"
        "    inline explicit WSLMatrix(const threadgroup WSLMatrix<U, Cols, Rows> &that) constant\n"
        "        : WSLMatrix(that, _make_integer_sequence<int, Cols>())\n"
        "    {\n"
        "    }\n"
        "    public:\n"
        "    inline thread vec<T, Rows> &operator[](int r) thread\n"
        "    {\n"
        "        return columns[r];\n"
        "    }\n"
        "    inline device vec<T, Rows> &operator[](int r) device\n"
        "    {\n"
        "        return columns[r];\n"
        "    }\n"
        "    inline threadgroup vec<T, Rows> &operator[](int r) threadgroup\n"
        "    {\n"
        "        return columns[r];\n"
        "    }\n"
        "    inline const thread vec<T, Rows> &operator[](int r) const thread\n"
        "    {\n"
        "        return columns[r];\n"
        "    }\n"
        "    inline const device vec<T, Rows> &operator[](int r) const device\n"
        "    {\n"
        "        return columns[r];\n"
        "    }\n"
        "    inline const constant vec<T, Rows> &operator[](int r) const constant\n"
        "    {\n"
        "        return columns[r];\n"
        "    }\n"
        "    inline const threadgroup vec<T, Rows> &operator[](int r) const threadgroup\n"
        "    {\n"
        "        return columns[r];\n"
        "    }\n"
        "};\n"


    };

}

static void dumpMetalCodeIfNeeded(StringBuilder& stringBuilder)
{
    if (dumpMetalCode) {
        dataLogLn("Generated Metal code: ");
        dataLogLn(stringBuilder.toString());
    }
}

RenderMetalCode generateMetalCode(Program& program, MatchedRenderSemantics&& matchedSemantics, Layout& layout)
{
    StringBuilder stringBuilder;
    stringBuilder.append(metalCodePrologue());

    TypeNamer typeNamer(program);
    typeNamer.emitMetalTypes(stringBuilder);
    
    auto metalFunctionEntryPoints = Metal::emitMetalFunctions(stringBuilder, program, typeNamer, WTFMove(matchedSemantics), layout);

    dumpMetalCodeIfNeeded(stringBuilder);

    return { WTFMove(stringBuilder), WTFMove(metalFunctionEntryPoints.mangledVertexEntryPointName), WTFMove(metalFunctionEntryPoints.mangledFragmentEntryPointName) };
}

ComputeMetalCode generateMetalCode(Program& program, MatchedComputeSemantics&& matchedSemantics, Layout& layout)
{
    StringBuilder stringBuilder;
    stringBuilder.append(metalCodePrologue());

    TypeNamer typeNamer(program);
    typeNamer.emitMetalTypes(stringBuilder);

    auto metalFunctionEntryPoints = Metal::emitMetalFunctions(stringBuilder, program, typeNamer, WTFMove(matchedSemantics), layout);

    dumpMetalCodeIfNeeded(stringBuilder);

    return { WTFMove(stringBuilder), WTFMove(metalFunctionEntryPoints.mangledEntryPointName) };
}

}

}

}

#endif
