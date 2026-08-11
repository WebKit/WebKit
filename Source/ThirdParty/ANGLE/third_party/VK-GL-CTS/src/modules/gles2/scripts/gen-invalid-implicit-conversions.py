# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# drawElements Quality Program utilities
# --------------------------------------
#
# Copyright (c) 2016 The Khronos Group Inc.
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
import string
from genutil import *

# Templates

INVALID_IMPLICIT_CONVESION_TEMPLATE0 = """
case ${{NAME}}
    expect compile_fail

    both ""
        precision mediump float;
        precision mediump int;

        ${DECLARATIONS}

        void main()
        {
            ${{TYPE0}} c;
            ${{TYPE0}} a;
            ${{TYPE1}} b;
            ${{TYPE0}} c = a ${{OPERATION}} b;
        }
    ""
end
"""[1:-1]

INVALID_IMPLICIT_CONVESION_TEMPLATE1 = """
case ${{NAME}}
    expect compile_fail

    both ""
        precision mediump float;
        precision mediump int;

        ${DECLARATIONS}

        void main()
        {
            ${{TYPE1}} c;
            ${{TYPE0}} a;
            ${{TYPE1}} b;
            ${{TYPE1}} c = a ${{OPERATION}} b;
        }
    ""
end
"""[1:-1]

arithOperations = {'+': 'add', '*':'mul', '/': 'div', '-':'sub'}

class InvalidImplicitConversionCase(ShaderCase):
    def __init__(self, operation, type0, type1):
        self.name = arithOperations[operation] + '_' + type0 + '_' + type1
        self.operation = operation
        self.type0 = type0
        self.type1 = type1

    def __str__(self):
        params0 = { "NAME": self.name + '_' + self.type0, "TYPE0": self.type0, "TYPE1": self.type1, "OPERATION": self.operation }
        params1 = { "NAME": self.name + '_' + self.type1, "TYPE0": self.type0, "TYPE1": self.type1, "OPERATION": self.operation }
        return fillTemplate(INVALID_IMPLICIT_CONVESION_TEMPLATE0, params0) + '\n' + fillTemplate(INVALID_IMPLICIT_CONVESION_TEMPLATE1, params1)

def createCase(operation, type0, type1):
    cases = []
    for t0 in type0:
        for t1 in type1:
            case = InvalidImplicitConversionCase(operation, t0, t1)
            cases.append(case)
    return cases

floats = ['float', 'vec2', 'vec3', 'vec4']
sintegers = ['int', 'ivec2', 'ivec3', 'ivec4']
cases = []
for op in arithOperations:
    caseFpInt = createCase(op, floats, sintegers)
    cases = cases + caseFpInt

invalidImplicitConversionCases = [
        CaseGroup("invalid_implicit_conversions", "Invalid Implicit Conversions", cases),
]

if __name__ == "__main__":
    print("Generating shader case files.")
    writeAllCases("invalid_implicit_conversions.test", invalidImplicitConversionCases)
