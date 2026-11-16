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
import itertools
from collections import namedtuple
from genutil import *

# Templates

declarationTemplate = """
case ${{NAME}}
    ${{COMPILE_FAIL}}
    values {}

    vertex ""
        precision mediump float;
        attribute highp vec4 dEQP_Position;

        ${{VARIABLE_VTX}}

        void main()
        {
            x0 = 1.0;
            gl_Position = dEQP_Position;
        }
    ""

    fragment ""
        precision mediump float;

        ${{VARIABLE_FRG}}

        void main()
        {
            float result = x0 + x1;
            gl_FragColor = vec4(result, result, result, 1.0);
        }
    ""
end
"""[1:-1]

parameterTemplate = """
case ${{NAME}}
    ${{COMPILE_FAIL}}
    values {}

    both ""
        precision mediump float;
        ${DECLARATIONS}

        float foo0 (${{PARAMETER0}})
        {
            return x + 1.0;
        }

        void foo1 (${{PARAMETER1}})
        {
            x = 1.0;
        }

        float foo2 (${{PARAMETER2}})
        {
            return x + 1.0;
        }

        void main()
        {
            ${SETUP}
            float result;
            foo1(result);
            float x0 = foo0(1.0);
            foo2(result);
            ${OUTPUT}
        }
    ""
end
"""[1:-1]

# Classes

class DeclarationCase(ShaderCase):
    def __init__(self, compileFail, paramList):
        self.compileFail = "expect compile_fail" if compileFail else "expect pass"
        self.name = ''
        var0 = ''
        var1 = ''
        var2 = ''

        for p in paramList:
            self.name += p.name
            if paramList.index(p) != len(paramList)-1:
                self.name += '_'

            var0 += p.vars[0] + ' '
            var1 += p.vars[1] + ' '
            var2 += p.vars[2] + ' '

        var0 += 'float x0;\n'
        var1 += 'float x1;\n'
        var2 += 'float x2;'

        self.variableVtx = (var0 + var1 + var2).strip()
        self.variableFrg = (var0 + var1).strip()            # Omit 'attribute' in frag shader
        self.variableVtx = self.variableVtx.replace("  ", " ")
        self.variableFrg = self.variableFrg.replace("  ", " ")

    def __str__(self):
        params = {
            "NAME"            : self.name,
            "COMPILE_FAIL"    : self.compileFail,
            "VARIABLE_VTX"    : self.variableVtx,
            "VARIABLE_FRG"    : self.variableFrg
        }
        return fillTemplate(declarationTemplate, params)

class ParameterCase(ShaderCase):
    def __init__(self, compileFail, paramList):
        self.compileFail = "expect compile_fail" if compileFail else "expect pass"
        self.name = ''
        self.param0 = ''
        self.param1 = ''
        self.param2 = ''

        for p in paramList:
            self.name += p.name
            if paramList.index(p) != len(paramList)-1:
                self.name += '_'

            self.param0 += p.vars[0] + ' '
            self.param1 += p.vars[1] + ' '
            self.param2 += p.vars[2] + ' '

        self.param0 += 'float x'
        self.param1 += 'float x'
        self.param2 += 'float x'
        self.param0 = self.param0.replace("  ", " ")
        self.param1 = self.param1.replace("  ", " ")
        self.param2 = self.param2.replace("  ", " ")

    def __str__(self):
        params = {
            "NAME"            : self.name,
            "COMPILE_FAIL"    : self.compileFail,
            "PARAMETER0"    : self.param0,
            "PARAMETER1"    : self.param1,
            "PARAMETER2"    : self.param2,
        }
        return fillTemplate(parameterTemplate, params)

# Declarations

CaseFormat = namedtuple('CaseFormat', 'name vars')

DECL_INVARIANT = CaseFormat("invariant", ["invariant", "", ""])
DECL_STORAGE = CaseFormat("storage", ["varying", "uniform", "attribute"])
DECL_PRECISION = CaseFormat("precision", ["lowp", "mediump", "mediump"])

PARAM_STORAGE = CaseFormat("storage", [ "const", "", ""])
PARAM_PARAMETER = CaseFormat("parameter", [ "in", "out", "inout" ])
PARAM_PRECISION = CaseFormat("precision", [ "lowp", "mediump", "mediump" ])

# Order of qualification tests

validDeclarationCases = []
invalidDeclarationCases = []
validParameterCases = []
invalidParameterCases = []

declFormats = [
    [DECL_INVARIANT, DECL_STORAGE, DECL_PRECISION],
    [DECL_STORAGE, DECL_PRECISION],
    [DECL_INVARIANT, DECL_STORAGE]
]

paramFormats = [
    [PARAM_STORAGE, PARAM_PARAMETER, PARAM_PRECISION],
    [PARAM_STORAGE, PARAM_PARAMETER],
    [PARAM_STORAGE, PARAM_PRECISION],
    [PARAM_PARAMETER, PARAM_PRECISION]
]

for f in declFormats:
    for p in itertools.permutations(f):
        if list(p) == f:
            validDeclarationCases.append(DeclarationCase(False, p))        # Correct order
        else:
            invalidDeclarationCases.append(DeclarationCase(True, p))    # Incorrect order

for f in paramFormats:
    for p in itertools.permutations(f):
        if list(p) == f:
            validParameterCases.append(ParameterCase(False, p))            # Correct order
        else:
            invalidParameterCases.append(ParameterCase(True, p))        # Incorrect order

qualificationOrderCases = [
    CaseGroup("variables", "Order of qualification in variable declarations.", children = [
        CaseGroup("valid", "Valid orderings.", validDeclarationCases),
        CaseGroup("invalid", "Invalid orderings.", invalidDeclarationCases)
    ]),
    CaseGroup("parameters", "Order of qualification in function parameters.", children = [
        CaseGroup("valid", "Valid orderings.", validParameterCases),
        CaseGroup("invalid", "Invalid orderings.", invalidParameterCases)
    ])
]

# Main program

if __name__ == "__main__":
    print("Generating shader case files.")
    writeAllCases("qualification_order.test", qualificationOrderCases)
