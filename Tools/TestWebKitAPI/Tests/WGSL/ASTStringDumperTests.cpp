/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ASTStringDumper.h"

#include "AST.h"
#include "Parser.h"
#include "TestWGSLAPI.h"
#include "WGSL.h"

namespace TestWGSLAPI {

static String toString(WGSL::AST::ShaderModule& shaderModule)
{
    WGSL::AST::StringDumper dumper;
    dumper.visit(shaderModule);
    return dumper.toString();
}

TEST(WGSLASTDumperTests, dumpTriangleVert)
{
    auto shader = WGSL::parseLChar(
        "@vertex\n"
        "fn main(\n"
        "    @builtin(vertex_index) VertexIndex : u32\n"
        ") -> @builtin(position) vec4<f32> {\n"
        "    var pos = array<vec2<f32>, 3>(\n"
        "        vec2<f32>(0.0, 0.5),\n"
        "        vec2<f32>(-0.5, -0.5),\n"
        "        vec2<f32>(0.5, -0.5)\n"
        "    );\n\n"
        "    return vec4<f32>(pos[VertexIndex], 0.0, 1.0);\n"
        "}\n"_s);

    EXPECT_SHADER(shader);
    EXPECT_EQ(
        toString(shader.value()),
        "@vertex\n"
        "fn main(\n"
        "    @builtin(vertex_index) VertexIndex: u32\n"
        ") -> @builtin(position) Vec4<f32>\n"
        "{\n"
        "    var pos = array<Vec2<f32>, 3>(Vec2<f32>(0.000000, 0.500000), Vec2<f32>(-0.500000, -0.500000), Vec2<f32>(0.500000, -0.500000));\n"
        "    return Vec4<f32>(pos[VertexIndex], 0.000000, 1.000000);\n"
        "}\n\n\n"_str);
}

} // namespace TestWGSLAPI
