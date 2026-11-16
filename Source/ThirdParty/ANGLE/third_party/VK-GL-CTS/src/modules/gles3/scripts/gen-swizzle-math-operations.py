# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# drawElements Quality Program utilities
# --------------------------------------
#
# Copyright 2017 The Android Open Source Project
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

import operator as op
from genutil import *
from collections import OrderedDict

VECTOR_TYPES = ["vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4"]
PRECISION_TYPES = ["mediump"]
SWIZZLE_NAMES = ["xyzw"]

s_swizzleCaseTemplate = """
case ${{NAME}}
    version 300 es
    values
    {
        ${{VALUES}}
    }

    both ""
        #version 300 es
        precision mediump float;

        ${DECLARATIONS}

        void main()
        {
            ${SETUP}
            ${{OP}}
            ${OUTPUT}
        }
    ""
end
"""[1:]

def getDataTypeScalarSize (dt):
    return {
        "vec2": 2,
        "vec3": 3,
        "vec4": 4,
        "ivec2": 2,
        "ivec3": 3,
        "ivec4": 4,
    }[dt]

def getSwizzlesForWidth(width):
    if (width == 2):
        return [(0, ),
            (0,0), (0,1), (1,0),
            (1,0,1), (0,1,0,0), (1,0,1,0)]
    elif (width == 3):
        return [(0,), (2,),
            (0,2), (2,2),
            (0,1,2), (2,1,0), (0,0,0), (2,2,2), (2,2,1), (1,0,1), (0,2,0),
            (0,1,1,0), (2,0,1,2)]
    elif (width == 4):
        return [(0,), (3,),
            (3,0), (3,2),
            (3,3,3), (1,1,3), (3,2,1),
            (0,1,2,3), (3,2,1,0), (0,1,0,1), (1,2,2,1), (3,0,3,3), (0,1,0,0), (2,2,2,2)]
    else:
        assert False

def operatorToSymbol(operator):
    if operator == "add": return "+"
    if operator == "subtract": return "-"
    if operator == "multiply": return "*"
    if operator == "divide": return "/"

def rotate(l, n) :
    return l[n:] + l[:n]

class SwizzleCase(ShaderCase):
    def __init__(self, name, swizzle1, swizzle2, inputs1, inputs2, operator, outputs):
        self.name = name
        self.swizzle1 = swizzle1
        self.swizzle2 = swizzle2
        self.inputs = inputs1 + inputs2
        self.outputs = outputs
        self.op = "out0 = in0.%s %s in1.%s;" % (swizzle1, operator, swizzle2)

    def __str__(self):
        params = {
            "NAME": self.name,
            "VALUES": genValues(self.inputs, self.outputs),
            "OP": self.op
        }
        return fillTemplate(s_swizzleCaseTemplate, params)


# CASE DECLARATIONS
inFloat = [Scalar(x) for x in [0.0, 1.0, 2.0, 3.5, -0.5, -20.125, 36.8125]]
inInt = [Scalar(x) for x in [0, 1, 2, 5, 8, 11, -12, -66, -192, 255]]

inVec4 = [
    Vec4(0.1, 0.5, 0.75, 0.825),
    Vec4(1.0, 1.25, 1.125, 1.75),
    Vec4(-0.5, -2.25, -4.875, 9.0),
    Vec4(-32.0, 64.0, -51.0, 24.0),
    Vec4(-0.75, -1.0/31.0, 1.0/19.0, 1.0/4.0),
]

inVec3 = toVec3(inVec4)
inVec2 = toVec2(inVec4)

inIVec4 = toIVec4(
    [
        Vec4(-1, 1, -1, 1),
        Vec4(1, 2, 3, 4),
        Vec4(-1, -2, -4, -9),
    ]
)

inIVec3 = toIVec3(inIVec4)
inIVec2 = toIVec2(inIVec4)

INPUTS = OrderedDict([
    ("float", inFloat),
    ("vec2", inVec2),
    ("vec3", inVec3),
    ("vec4", inVec4),
    ("int", inInt),
    ("ivec2", inIVec2),
    ("ivec3", inIVec3),
    ("ivec4", inIVec4),
])

OPERATORS = OrderedDict([
    ("add", op.add),
    ("subtract", op.sub),
    ("multiply", op.mul),
    ("divide", op.div),
])

vectorSwizzleGroupCases = {
    "add": [],
    "subtract" : [],
    "multiply" : [],
    "divide" : [],
}

allCases = []

for operator in OPERATORS:
    for dataType in VECTOR_TYPES:
        scalarSize = getDataTypeScalarSize(dataType)
        for precision in PRECISION_TYPES:
            for swizzleComponents in SWIZZLE_NAMES:
                for swizzleIndices in getSwizzlesForWidth(scalarSize):
                    swizzle1 = "".join(map(lambda x: swizzleComponents[x], swizzleIndices))

                    swizzle2 = rotate(swizzle1, 1)
                    rotatedSwizzleIndices = rotate(swizzleIndices, 1)

                    operands1 = INPUTS[dataType]
                    operands2 = INPUTS[dataType]  # these input values will be swizzled

                    outputs = map(lambda x, y: OPERATORS[operator](x.swizzle(swizzleIndices), y.swizzle(rotatedSwizzleIndices)), operands1, operands2)
                    outType = outputs[0].typeString()
                    caseName = "%s_%s_%s_%s" % (precision, dataType, swizzle1, swizzle2)

                    case = SwizzleCase(    caseName,
                                swizzle1,
                                swizzle2,
                                [("%s in0" % dataType, operands1)],
                                [("%s in1" % dataType, operands2)],
                                operatorToSymbol(operator),
                                [("%s out0" % outType, outputs)])

                    vectorSwizzleGroupCases[operator].append(case)

    allCases.append(CaseGroup("vector_" + operator, "Vector swizzle math operations", vectorSwizzleGroupCases[operator]))

if __name__ == "__main__":
    print("Generating shader case files.")
    writeAllCases("swizzle_math_operations.test", allCases)
