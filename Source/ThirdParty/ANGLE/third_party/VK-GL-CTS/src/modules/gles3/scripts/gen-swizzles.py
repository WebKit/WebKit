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
import random
import operator
import itertools

from genutil import *

random.seed(1234567)
indices = xrange(sys.maxint)

# Swizzles:
# - vector components
#   * int, float, bool vectors
#   * .xyzw, .rgba, .stpq
#   * illegal to mix
#   * not allowed for scalar types
#   * legal to chain: vec4.rgba.xyzw.stpq
#   * illegal to select more than 4 components
#
# Subscripts:
# - array-like indexing with [] operator
#   * vectors, matrices
# - read & write
# - vectors components
#   * [] accessor
# - matrix columns
#   * [] accessor
#   * note: mat4[0].x = 1.0; vs mat4[0][0] = 1.0; ??
#   * out-of-bounds accesses

#
# - vector swizzles
#   * all vector types (bvec2..4, ivec2..4, vec2..4)
#   * all precisions (lowp, mediump, highp)
#   * all component names (xyzw, rgba, stpq)
#   * broadcast each, reverse, N random
# - component-masked writes
#   * all vector types (bvec2..4, ivec2..4, vec2..4)
#   * all precisions (lowp, mediump, highp)
#   * all component names (xyzw, rgba, stpq)
#   * all possible subsets
#   * all input types (attribute, varying, uniform, tmp)
#   -> a few hundred cases
# - concatenated swizzles

#
VECTOR_TYPES = [ "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4", "bvec2", "bvec3", "bvec4" ]
PRECISION_TYPES = [ "lowp", "mediump", "highp" ]
INPUT_TYPES = [ "uniform", "varying", "attribute", "tmp" ]
SWIZZLE_NAMES = [ "xyzw", "stpq", "rgba" ]

def getDataTypeScalarSize (dt):
    return {
        "float": 1,
        "vec2": 2,
        "vec3": 3,
        "vec4": 4,
        "int": 1,
        "ivec2": 2,
        "ivec3": 3,
        "ivec4": 4,
        "bool": 1,
        "bvec2": 2,
        "bvec3": 3,
        "bvec4": 4,
        "mat2": 4,
        "mat3": 9,
        "mat4": 16
    }[dt]

if False:
    class Combinations:
        def __init__(self, *args):
            self.lists = list(args)
            self.numLists = len(args)
            self.numCombinations = reduce(operator.mul, map(len, self.lists), 1)
            print(self.lists)
            print(self.numCombinations)

        def iterate(self):
            return [tuple(map(lambda x: x[0], self.lists))]

    combinations = Combinations(INPUT_TYPES, VECTOR_TYPES, PRECISION_TYPES)
    print(combinations.iterate())
    for (inputType, dataType, precision) in combinations.iterate():
        scalarSize = getDataTypeScalarSize(dataType)
        print(inputType, precision, dataType)

def getSwizzlesForWidth(width):
    if (width == 2):
        return [(0,), (0,0), (0,1), (1,0), (1,0,1), (0,1,0,0), (1,1,1,1)]
    elif (width == 3):
        return [(0,), (2,), (0,2), (2,2), (0,1,2), (2,1,0), (0,0,0), (2,2,2), (2,2,1), (1,0,1), (0,2,0), (0,1,1,0), (2,2,2,2)]
    elif (width == 4):
        return [(0,), (3,), (3,0), (3,2), (3,3,3), (1,1,3), (3,2,1), (0,1,2,3), (3,2,1,0), (0,0,0,0), (1,1,1,1), (3,3,3,3), (3,2,2,3), (3,3,3,1), (0,1,0,0), (2,2,3,2)]
    else:
        assert False

# Templates.

s_swizzleCaseTemplate = """
case ${{NAME}}
    version 300
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

s_simpleIllegalCaseTemplate = """
case ${{NAME}}
    version 300
    expect compile_fail
    values {}

    both ""
        #version 300 es
        precision mediump float;
        precision mediump int;

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

class SwizzleCase(ShaderCase):
    def __init__(self, name, precision, dataType, swizzle, inputs, outputs):
        self.name = name
        self.precision = precision
        self.dataType = dataType
        self.swizzle = swizzle
        self.inputs = inputs
        self.outputs = outputs
        self.op = "out0 = in0.%s;" % swizzle

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
inBool = [Scalar(x) for x in [True, False]]

inVec4 = [Vec4(0.0, 0.5, 0.75, 0.825), Vec4(1.0, 1.25, 1.125, 1.75),
           Vec4(-0.5, -2.25, -4.875, 9.0), Vec4(-32.0, 64.0, -51.0, 24.0),
           Vec4(-0.75, -1.0/31.0, 1.0/19.0, 1.0/4.0)]
inVec3 = toVec3(inVec4)
inVec2 = toVec2(inVec4)
inIVec4 = toIVec4(inVec4)
inIVec3 = toIVec3(inVec4)
inIVec2 = toIVec2(inVec4)
inBVec4 = [Vec4(True, False, False, True), Vec4(False, False, False, True), Vec4(False, True, False, False), Vec4(True, True, True, True), Vec4(False, False, False, False)]
inBVec3 = toBVec3(inBVec4)
inBVec2 = toBVec2(inBVec4)

# \todo [petri] Enable large values when epsilon adapts to the values.
inMat4 = [Mat4(1.0, 0.0, 0.0, 0.0,  0.0, 1.0, 0.0, 0.0,  0.0, 0.0, 1.0, 0.0,  0.0, 0.0, 0.0, 1.0),
           Mat4(6.5, 12.5, -0.75, 9.975,  32.0, 1.0/48.0, -8.425, -6.542,  1.0/8.0, 1.0/16.0, 1.0/32.0, 1.0/64.0,  -6.725, -0.5, -0.0125, 9.975),
           #Mat4(128.0, 256.0, -512.0, -1024.0,  2048.0, -4096.0, 8192.0, -8192.0,  192.0, -384.0, 768.0, -1536.0,  8192.0, -8192.0, 6144.0, -6144.0)
           ]
inMat3 = [Mat3(1.0, 0.0, 0.0,  0.0, 1.0, 0.0,  0.0, 0.0, 1.0),
           Mat3(6.5, 12.5, -0.75,  32.0, 1.0/32.0, 1.0/64.0,  1.0/8.0, 1.0/16.0, 1.0/32.0),
           #Mat3(-18.725, -0.5, -0.0125,  19.975, -0.25, -17.75,  9.25, 65.125, -21.425),
           #Mat3(128.0, -4096.0, -8192.0,  192.0, 768.0, -1536.0,  8192.0, 6144.0, -6144.0)
           ]
inMat2 = [Mat2(1.0, 0.0,  0.0, 1.0),
           Mat2(6.5, 12.5,  -0.75, 9.975),
           Mat2(6.5, 12.5,  -0.75, 9.975),
           Mat2(8.0, 16.0,  -24.0, -16.0),
           Mat2(1.0/8.0, 1.0/16.0,  1.0/32.0, 1.0/64.0),
           Mat2(-18.725, -0.5,  -0.0125, 19.975),
           #Mat2(128.0, -4096.0,  192.0, -1536.0),
           #Mat2(-1536.0, 8192.0,  6144.0, -6144.0)
           ]

INPUTS = {
    "float": inFloat,
    "vec2": inVec2,
    "vec3": inVec3,
    "vec4": inVec4,
    "int": inInt,
    "ivec2": inIVec2,
    "ivec3": inIVec3,
    "ivec4": inIVec4,
    "bool": inBool,
    "bvec2": inBVec2,
    "bvec3": inBVec3,
    "bvec4": inBVec4,
    "mat2": inMat2,
    "mat3": inMat3,
    "mat4": inMat4
}

def genConversionCases(inValueList, convFuncList):
    combinations = list(itertools.product(inValueList, convFuncList))
    return [ConversionCase(inValues, convFunc) for (inValues, convFunc) in combinations]

allCases = []

# Vector swizzles.

vectorSwizzleCases = []

# \todo [petri] Uses fixed precision.
for dataType in VECTOR_TYPES:
    scalarSize = getDataTypeScalarSize(dataType)
    precision = "mediump"
    for swizzleComponents in SWIZZLE_NAMES:
        for swizzleIndices in getSwizzlesForWidth(scalarSize):
            swizzle = "".join(map(lambda x: swizzleComponents[x], swizzleIndices))
            #print("%s %s .%s" % (precision, dataType, swizzle))
            caseName = "%s_%s_%s" % (precision, dataType, swizzle)
            inputs = INPUTS[dataType]
            outputs = map(lambda x: x.swizzle(swizzleIndices), inputs)
            outType = outputs[0].typeString()
            vectorSwizzleCases.append(SwizzleCase(caseName, precision, dataType, swizzle, [("%s in0" % dataType, inputs)], [("%s out0" % outType, outputs)]))

# ??
#for dataType in VECTOR_TYPES:
#    scalarSize = getDataTypeScalarSize(dataType)
#    for precision in PRECISION_TYPES:
#        for swizzleIndices in getSwizzlesForWidth(scalarSize):
#            swizzle = "".join(map(lambda x: "xyzw"[x], swizzleIndices))
#            #print("%s %s .%s" % (precision, dataType, swizzle))
#            caseName = "%s_%s_%s" % (precision, dataType, swizzle)
#            inputs = INPUTS[dataType]
#            outputs = map(lambda x: x.swizzle(swizzleIndices), inputs)
#            vectorSwizzleCases.append(SwizzleCase(caseName, precision, dataType, swizzle, [("in0", inputs)], [("out0", outputs)]))

allCases.append(CaseGroup("vector_swizzles", "Vector Swizzles", vectorSwizzleCases))

# Swizzles:
# - vector components
#   * int, float, bool vectors
#   * .xyzw, .rgba, .stpq
#   * illegal to mix
#   * not allowed for scalar types
#   * legal to chain: vec4.rgba.xyzw.stpq
#   * illegal to select more than 4 components

# TODO: precisions!!

#allCases.append(CaseGroup("vector_swizzles", "Vector Swizzles",
#    genSwizzleCase([inVec2, inVec3, inVec4],

# Main program.

if __name__ == "__main__":
    print("Generating shader case files.")
    writeAllCases("swizzles.test", allCases)
