# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# drawElements Quality Program utilities
# --------------------------------------
#
# Copyright 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#-------------------------------------------------------------------------

import sys
from genutil import *

# \todo [arttu 2012-12-20] Current set tests variable names only, add function names, structure names, and field selectors.

# Templates

identifierCaseTemplate = """
case ${{NAME}}
    ${{EXPECT}}
    values {}
    version 300 es

    both ""
        #version 300 es
        precision mediump float;
        ${DECLARATIONS}

        void main()
        {
            ${SETUP}
            float ${{IDENTIFIER}} = 1.0;
            ${OUTPUT}
        }
    ""
end
"""[1:-1]

# Classes

class IdentifierCase(ShaderCase):
    def __init__(self, name, identifier, expectToCompile = True):
        self.name = name
        self.identifier = identifier
        self.expect = '' if expectToCompile else 'expect compile_fail'

    def __str__(self):
        params = {    "NAME"            : self.name,
                    "IDENTIFIER"    : self.identifier,
                    "EXPECT"        : self.expect }
        return fillTemplate(identifierCaseTemplate, params)

# Declarations

KEYWORDS = [
    "const", "uniform", "layout", "centroid", "flat", "smooth", "break", "continue", "do",
    "for", "while", "switch", "case", "default","if", "else", "in", "out", "inout", "float",
    "int", "void", "bool", "true", "false", "invariant", "discard", "return", "mat2", "mat3",
    "mat4", "mat2x2", "mat2x3", "mat2x4", "mat3x2", "mat3x3", "mat3x4", "mat4x2", "mat4x3", "mat4x4",
    "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4", "bvec2", "bvec3", "bvec4", "uint", "uvec2",
    "uvec3", "uvec4", "lowp", "mediump", "highp", "precision", "sampler2D", "sampler3D", "samplerCube",
    "sampler2DShadow", "samplerCubeShadow", "sampler2DArray", "sampler2DArrayShadow", "isampler2D",
    "isampler3D", "isamplerCube", "isampler2DArray", "usampler2D", "usampler3D", "usamplerCube",
    "usampler2DArray", "struct"
]

RESERVED_KEYWORDS = [
    "attribute", "varying", "coherent", "restrict", "readonly", "writeonly",
    "resource", "atomic_uint", "noperspective", "patch", "sample", "subroutine", "common",
    "partition", "active", "asm", "class", "union", "enum", "typedef", "template", "this",
    "goto", "inline", "noinline", "volatile", "public", "static", "extern", "external", "interface",
    "long", "short", "double", "half", "fixed", "unsigned", "superp", "input", "output",
    "hvec2", "hvec3", "hvec4", "dvec2", "dvec3", "dvec4", "fvec2", "fvec3", "fvec4", "sampler3DRect",
    "filter", "image1D", "image2D", "image3D", "imageCube", "iimage1D", "iimage2D", "iimage3D",
    "iimageCube", "uimage1D", "uimage2D", "uimage3D", "uimageCube", "image1DArray", "image2DArray",
    "iimage1DArray", "iimage2DArray", "uimage1DArray", "uimage2DArray",
    "imageBuffer", "iimageBuffer", "uimageBuffer",
    "sampler1D", "sampler1DShadow", "sampler1DArray", "sampler1DArrayShadow", "isampler1D",
    "isampler1DArray", "usampler1D", "usampler1DArray", "sampler2DRect", "sampler2DRectShadow",
    "isampler2DRect", "usampler2DRect", "samplerBuffer", "isamplerBuffer", "usamplerBuffer",
    "sampler2DMS", "isampler2DMS", "usampler2DMS", "sampler2DMSArray", "isampler2DMSArray",
    "usampler2DMSArray", "sizeof", "cast", "namespace", "using"
]

ALLOWED_KEYWORDS = [
    "image1DShadow", "image2DShadow", "image1DArrayShadow", "image2DArrayShadow"
]

INVALID_IDENTIFIERS = [
    ("gl_begin", "gl_Invalid"),
    ("digit", "0123"),
    ("digit_begin", "0invalid"),
    ("max_length", "abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdX"),
]

# Keyword usage

keywords = []
reservedKeywords = []
allowedKeywords = []
invalidIdentifiers = []

for keyword in KEYWORDS:
    keywords.append(IdentifierCase(keyword, keyword, False))            # Keywords

for keyword in RESERVED_KEYWORDS:
    reservedKeywords.append(IdentifierCase(keyword, keyword, False))    # Reserved keywords

for keyword in ALLOWED_KEYWORDS:
    allowedKeywords.append(IdentifierCase(keyword, keyword, True))        # Allowed keywords

for (name, identifier) in INVALID_IDENTIFIERS:
    invalidIdentifiers.append(IdentifierCase(name, identifier, False))    # Invalid identifiers

keywordCases = [
    CaseGroup("keywords", "Usage of keywords as identifiers.", keywords),
    CaseGroup("reserved_keywords", "Usage of reserved keywords as identifiers.", reservedKeywords),
    CaseGroup("allowed_keywords", "Usage of allowed keywords as identifiers.", allowedKeywords),
    CaseGroup("invalid_identifiers", "Usage of invalid identifiers.", invalidIdentifiers)
]

# Main program

if __name__ == "__main__":
    print("Generating shader case files.")
    writeAllCases("keywords.test", keywordCases)
