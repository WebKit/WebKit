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
        #version 300 es
        precision mediump float;
        in highp vec4 dEQP_Position;

        ${{VARIABLE_VTX}}

        void main()
        {
            x0 = 1.0;
            x1 = 2.0;
            gl_Position = dEQP_Position;
        }
    ""

    fragment ""
        #version 300 es
        precision mediump float;
        layout(location = 0) out mediump vec4 dEQP_FragColor;

        ${{VARIABLE_FRG}}

        void main()
        {
            float result = (x0 + x1 + x2) / 3.0;
            dEQP_FragColor = vec4(result, result, result, 1.0);
        }
    ""
end
"""[1:-1]

parameterTemplate = """
case ${{NAME}}
    ${{COMPILE_FAIL}}
    version 300 es
    values {}

    both ""
        #version 300 es
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
    def __init__(self, compileFail, invariantInput, paramList):
        self.compileFail = 'expect compile_fail' if compileFail else 'expect pass'
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

        if invariantInput:
            self.name += "_invariant_input"

        var0 += 'float x0;\n'
        var1 += 'float x1;\n'
        var2 += 'float x2;'

        variables = (var0 + var1 + var2).strip()
        variables = variables.replace("  ", " ")
        self.variableVtx = variables.replace("anon_centroid", "out")
        self.variableFrg = variables.replace("anon_centroid", "in")
        self.variableVtx = self.variableVtx.replace("centroid", "centroid out")
        self.variableFrg = self.variableFrg.replace("centroid", "centroid in")

        self.variableFrg = self.variableFrg.replace("invariant", "")    # input variable cannot be invariant...
        if invariantInput:
            self.variableFrg = "invariant " + self.variableFrg            # ...unless we are doing a negative test

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

DECL_INVARIANT = CaseFormat("invariant", [ "invariant", "", "" ])
DECL_INTERPOLATION = CaseFormat("interp", [ "smooth", "flat", "" ])
DECL_STORAGE = CaseFormat("storage", [ "centroid", "anon_centroid", "uniform" ])
DECL_PRECISION = CaseFormat("precision", [ "lowp", "mediump", "highp" ])

PARAM_STORAGE = CaseFormat("storage", [ "const", "", ""])
PARAM_PARAMETER = CaseFormat("parameter", [ "in", "out", "inout" ])
PARAM_PRECISION = CaseFormat("precision", [ "lowp", "mediump", "highp" ])

# Order of qualification tests

validDeclarationCases = []
invalidDeclarationCases = []
validParameterCases = []
invalidParameterCases = []

declFormats = [
    [DECL_INVARIANT, DECL_INTERPOLATION, DECL_STORAGE, DECL_PRECISION],
    [DECL_INTERPOLATION, DECL_STORAGE, DECL_PRECISION],
    [DECL_INVARIANT, DECL_INTERPOLATION, DECL_STORAGE],
    [DECL_INVARIANT, DECL_STORAGE, DECL_PRECISION],
    [DECL_STORAGE, DECL_PRECISION],
    [DECL_INTERPOLATION, DECL_STORAGE],
    [DECL_INVARIANT, DECL_STORAGE]
]

paramFormats = [
    [PARAM_STORAGE, PARAM_PARAMETER, PARAM_PRECISION],
    [PARAM_STORAGE, PARAM_PARAMETER],
    [PARAM_STORAGE, PARAM_PRECISION],
    [PARAM_PARAMETER, PARAM_PRECISION]
]
print(len(paramFormats))

for f in declFormats:
    for p in itertools.permutations(f):
        if list(p) == f:
            validDeclarationCases.append(DeclarationCase(False, False, p))    # Correct order
        else:
            invalidDeclarationCases.append(DeclarationCase(True, False, p))    # Incorrect order

for f in declFormats:
    invalidDeclarationCases.append(DeclarationCase(True, True, f))    # Correct order but invariant is not allowed as and input parameter

for f in paramFormats:
    for p in itertools.permutations(f):
        if list(p) == f:
            validParameterCases.append(ParameterCase(False, p))    # Correct order
        else:
            invalidParameterCases.append(ParameterCase(True, p))    # Incorrect order

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
