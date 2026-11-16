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
    expect compile_fail
    values {}

    both ""
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
    def __init__(self, name, identifier):
        self.name = name
        self.identifier = identifier

    def __str__(self):
        params = {    "NAME"            : self.name,
                    "IDENTIFIER"    : self.identifier }
        return fillTemplate(identifierCaseTemplate, params)


# Declarations

KEYWORDS = [
    "attribute", "const", "uniform", "varying", "break", "continue", "do", "for", "while",
    "if", "else", "in", "out", "inout", "float", "int", "void", "bool", "true", "false",
    "lowp", "mediump", "highp", "precision", "invariant", "discard", "return", "mat2", "mat3",
    "mat4", "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4", "bvec2", "bvec3", "bvec4",
    "sampler2D", "samplerCube", "struct"
]

RESERVED_KEYWORDS = [
    "asm", "class", "union", "enum", "typedef", "template", "this", "packed", "goto", "switch",
    "default", "inline", "noinline", "volatile", "public", "static", "extern", "external",
    "interface", "flat", "long", "short", "double", "half", "fixed", "unsigned", "superp",
    "input", "output", "hvec2", "hvec3", "hvec4", "dvec2", "dvec3", "dvec4", "fvec2", "fvec3",
    "fvec4", "sampler1D", "sampler3D", "sampler1DShadow", "sampler2DShadow", "sampler2DRect",
    "sampler3DRect", "sampler2DRectShadow", "sizeof", "cast", "namespace", "using"
]

INVALID_IDENTIFIERS = [
    ("two_underscores_begin", "__invalid"),
    ("two_underscores_middle", "in__valid"),
    ("two_underscores_end", "invalid__"),
    ("gl_begin", "gl_Invalid"),
    ("digit", "0123"),
    ("digit_begin", "0invalid")
]

# Keyword usage

keywords = []
reservedKeywords = []
invalidIdentifiers = []

for keyword in KEYWORDS:
    keywords.append(IdentifierCase(keyword, keyword))            # Keywords

for keyword in RESERVED_KEYWORDS:
    reservedKeywords.append(IdentifierCase(keyword, keyword))    # Reserved keywords

for (name, identifier) in INVALID_IDENTIFIERS:
    invalidIdentifiers.append(IdentifierCase(name, identifier)) # Invalid identifiers

keywordCases = [
    CaseGroup("keywords", "Usage of keywords as identifiers.", keywords),
    CaseGroup("reserved_keywords", "Usage of reserved keywords as identifiers.", reservedKeywords),
    CaseGroup("invalid_identifiers", "Usage of invalid identifiers.", invalidIdentifiers)
]

# Main program

if __name__ == "__main__":
    print("Generating shader case files.")
    writeAllCases("keywords.test", keywordCases)
