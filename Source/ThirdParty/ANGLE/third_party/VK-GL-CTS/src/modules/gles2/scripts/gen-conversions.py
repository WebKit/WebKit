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

# Constructors:
#
# - scalars types
#   * int <-> float <-> bool (also float(float) etc.)
#   * to bool: zero means false, others true
#   * from bool: false==0, true==1
#   * \todo [petri] float<->int rounding rules?
# - scalar type from vector
#   * choose the first component
# - vectors & matrices
#   * vector from scalar: broadcast to all components
#   * matrix from scalar: broadcast scalar to diagonal, other components zero
#   * vector from vector: copy existing components
#     + illegal: vector from smaller vector
#   * mat from mat: copy existing components, other components from identity matrix
#   * from components: consumed by-component in column-major order, must have same
#     number of components,
#     + note: vec4(mat2) valid
#     \todo [petri] Implement!
# - notes:
#   * type conversions are always allowed: mat3(ivec3, bvec3, bool, int, float) is valid!
#
# Accessors:
#
# - vector components
#   * .xyzw, .rgba, .stpq
#   * illegal to mix
#   * now allowed for scalar types
#   * legal to chain: vec4.rgba.xyzw.stpq
#   * illegal to select more than 4 components
#   * array indexing with [] operator
#   * can also write!
# - matrix columns
#   * [] accessor
#   * note: mat4[0].x = 1.0; vs mat4[0][0] = 1.0; ??
#   * out-of-bounds accesses
#
# \todo [petri] Accessors!
#
# Spec issues:
#
# - constructing larger vector from smaller: vec3(vec2) ?
# - base type and size conversion at same time: vec4(bool), int(vec3) allowed?

def combineVec(comps):
    res = []
    for ndx in range(len(comps[0])):
#        for x in comps:
#            print x[ndx].toFloat().getScalars() ,
        scalars = reduce(operator.add, [x[ndx].toFloat().getScalars() for x in comps])
#        print "->", scalars
        res.append(Vec.fromScalarList(scalars))
    return res

def combineIVec(comps):
    res = []
    for ndx in range(len(comps[0])):
        res.append(Vec.fromScalarList(reduce(operator.add, [x[ndx].toInt().getScalars() for x in comps])))
    return res

def combineBVec(comps):
    res = []
    for ndx in range(len(comps[0])):
        res.append(Vec.fromScalarList(reduce(operator.add, [x[ndx].toBool().getScalars() for x in comps])))
    return res

def combineMat(numCols, numRows, comps):
    res = []
    for ndx in range(len(comps[0])):
        scalars = reduce(operator.add, [x[ndx].toFloat().getScalars() for x in comps])
        res.append(Mat(numCols, numRows, scalars))
    return res

def combineMat2(comps): return combineMat(2, 2, comps)
def combineMat3(comps): return combineMat(3, 3, comps)
def combineMat4(comps): return combineMat(4, 4, comps)

# 0 \+ [f*f for f in lst]
# r = 0 \+ [f in lst -> f*f]
# r = 0 \+ lst

# Templates.

s_simpleCaseTemplate = """
case ${{NAME}}
    values
    {
        ${{VALUES}}
    }

    both ""
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

s_simpleIllegalCaseTemplate = """
case ${{NAME}}
    expect compile_fail
    values {}

    both ""
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

class SimpleCase(ShaderCase):
    def __init__(self, name, inputs, outputs, op):
        self.name = name
        self.inputs = inputs
        self.outputs = outputs
        self.op = op

    def __str__(self):
        params = {
            "NAME": self.name,
            "VALUES": genValues(self.inputs, self.outputs),
            "OP": self.op
        }
        return fillTemplate(s_simpleCaseTemplate, params)

class ConversionCase(ShaderCase):
    def __init__(self, inValues, convFunc):
        outValues = convFunc(inValues)
        inType = inValues[0].typeString()
        outType = outValues[0].typeString()
        self.name = "%s_to_%s" % (inType, outType)
        self.op = "out0 = %s(in0);" % outType
        self.inputs = [("%s in0" % inType, inValues)]
        self.outputs = [("%s out0" % outType, outValues)]

    def __str__(self):
        params = {
            "NAME": self.name,
            "VALUES": genValues(self.inputs, self.outputs),
            "OP": self.op
        }
        return fillTemplate(s_simpleCaseTemplate, params)

class IllegalConversionCase(ShaderCase):
    def __init__(self, inValue, outValue):
        inType = inValue.typeString()
        outType = outValue.typeString()
        self.name = "%s_to_%s" % (inType, outType)
        self.op = "%s in0 = %s;\n%s out0 = %s(in0);" % (inType, str(inValue), outType, outType)
        self.inType = inType
        self.outType = outType

    def __str__(self):
        params = {
            "NAME": self.name,
            "OP": self.op
        }
        return fillTemplate(s_simpleIllegalCaseTemplate, params)

class CombineCase(ShaderCase):
    def __init__(self, inComps, combFunc):
        self.inComps = inComps
        self.outValues = combFunc(inComps)
        self.outType = self.outValues[0].typeString()
        inTypes = [values[0].typeString() for values in inComps]
        self.name = "%s_to_%s" % ("_".join(inTypes), self.outType)
        self.inputs = [("%s in%s" % (comp[0].typeString(), ndx), comp) for (comp, ndx) in zip(inComps, indices)]
        self.outputs = [("%s out0" % self.outType, self.outValues)]
        self.op = "out0 = %s(%s);" % (self.outType, ", ".join(["in%d" % x for x in range(len(inComps))]))

    def __str__(self):
        params = {
            "NAME": self.name,
            "VALUES": genValues(self.inputs, self.outputs),
            "OP": self.op
        }
        return fillTemplate(s_simpleCaseTemplate, params)

# CASE DECLARATIONS

inFloat = [Scalar(x) for x in [0.0, 1.0, 2.0, 3.5, -0.5, -8.25, -20.125, 36.8125]]
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

def genConversionCases(inValueList, convFuncList):
    combinations = list(itertools.product(inValueList, convFuncList))
    return [ConversionCase(inValues, convFunc) for (inValues, convFunc) in combinations]

def genIllegalConversionCases(inValueList, outValueList):
    inValues = [x[0] for x in inValueList]
    outValues = [x[0] for x in outValueList]
    combinations = list(itertools.product(inValues, outValues))
    return [IllegalConversionCase(inVal, outVal) for (inVal, outVal) in combinations]

def shuffleSubLists(outer):
    return [shuffled(inner) for inner in outer]

# Generate all combinations of CombineCases.
# inTupleList    a list of tuples of value-lists
# combFuncList    a list of comb* functions to combine
def genComponentCases(inCompLists, combFuncList):
    res = []
    for comps in inCompLists:
        maxLen = reduce(max, [len(values) for values in comps])
        comps = [repeatToLength(values, maxLen) for values in comps]
        comps = [shuffled(values) for values in comps]
        for combFunc in combFuncList:
            res += [CombineCase(comps, combFunc)]
    return res

allConversionCases = []

# Scalar-to-scalar conversions.
allConversionCases.append(CaseGroup("scalar_to_scalar", "Scalar to Scalar Conversions",
    genConversionCases([inFloat, inInt, inBool], [toFloat, toInt, toBool])))

# Scalar-to-vector conversions.
allConversionCases.append(CaseGroup("scalar_to_vector", "Scalar to Vector Conversions",
    genConversionCases([inFloat, inInt, inBool], [toVec2, toVec3, toVec4, toIVec2, toIVec3, toIVec4, toBVec2, toBVec3, toBVec4])))

# Vector-to-scalar conversions.
allConversionCases.append(CaseGroup("vector_to_scalar", "Vector to Scalar Conversions",
    genConversionCases([inVec2, inVec3, inVec4, inIVec2, inIVec3, inIVec4, inBVec2, inBVec3, inBVec4], [toFloat, toInt, toBool])))

# Illegal vector-to-vector conversions (to longer vec).
allConversionCases.append(CaseGroup("vector_illegal", "Illegal Vector Conversions",
    genIllegalConversionCases([inVec2, inIVec2, inBVec2], [inVec3, inIVec3, inBVec3, inVec4, inIVec4, inBVec4]) +\
    genIllegalConversionCases([inVec3, inIVec3, inBVec3], [inVec4, inIVec4, inBVec4])))

# Vector-to-vector conversions (type conversions, downcasts).
allConversionCases.append(CaseGroup("vector_to_vector", "Vector to Vector Conversions",
    genConversionCases([inVec4, inIVec4, inBVec4], [toVec4, toVec3, toVec2, toIVec4, toIVec3, toIVec2, toBVec4, toBVec3, toBVec2]) +\
    genConversionCases([inVec3, inIVec3, inBVec3], [toVec3, toVec2, toIVec3, toIVec2, toBVec3, toBVec2]) +\
    genConversionCases([inVec2, inIVec2, inBVec2], [toVec2, toIVec2, toBVec2])))

# Scalar-to-matrix.
allConversionCases.append(CaseGroup("scalar_to_matrix", "Scalar to Matrix Conversions",
    genConversionCases([inFloat, inInt, inBool], [toMat4, toMat3, toMat2])))

# Vector-to-matrix.
#allConversionCases += genConversionCases([inVec4, inIVec4, inBVec4], [toMat4])
#allConversionCases += genConversionCases([inVec3, inIVec3, inBVec3], [toMat3])
#allConversionCases += genConversionCases([inVec2, inIVec2, inBVec2], [toMat2])

# Matrix-to-matrix.
allConversionCases.append(CaseGroup("matrix_to_matrix", "Matrix to Matrix Conversions",
    genConversionCases([inMat4, inMat3, inMat2], [toMat4, toMat3, toMat2])))

# Vector-from-components, matrix-from-components.
in2Comp = [[inFloat, inFloat], [inInt, inInt], [inBool, inBool], [inFloat, inInt], [inFloat, inBool], [inInt, inBool]]
in3Comp = [[inFloat, inFloat, inFloat], [inInt, inInt, inInt], [inBool, inBool, inBool], [inBool, inFloat, inInt], [inVec2, inBool], [inBVec2, inFloat], [inBVec2, inInt], [inBool, inIVec2]]
in4Comp = [[inVec2, inVec2], [inBVec2, inBVec2], [inFloat, inFloat, inFloat, inFloat], [inInt, inInt, inInt, inInt], [inBool, inBool, inBool, inBool], [inBool, inFloat, inInt, inBool], [inVec2, inIVec2], [inVec2, inBVec2], [inBVec3, inFloat], [inVec3, inFloat], [inInt, inIVec2, inInt], [inBool, inFloat, inIVec2]]
in9Comp = [[inVec3, inVec3, inVec3], [inIVec3, inIVec3, inIVec3], [inVec2, inIVec2, inFloat, inFloat, inInt, inBool, inBool], [inBool, inFloat, inInt, inVec2, inBool, inBVec2, inFloat], [inBool, inBVec2, inInt, inVec4, inBool], [inFloat, inBVec4, inIVec2, inBool, inBool]]
in16Comp = [[inVec4, inVec4, inVec4, inVec4], [inIVec4, inIVec4, inIVec4, inIVec4], [inBVec4, inBVec4, inBVec4, inBVec4], [inFloat, inIVec3, inBVec3, inVec4, inIVec2, inFloat, inVec2]]

allConversionCases.append(CaseGroup("vector_combine", "Vector Combine Constructors",
    genComponentCases(in4Comp, [combineVec, combineIVec, combineBVec]) +\
    genComponentCases(in3Comp, [combineVec, combineIVec, combineBVec]) +\
    genComponentCases(in2Comp, [combineVec, combineIVec, combineBVec])))

allConversionCases.append(CaseGroup("matrix_combine", "Matrix Combine Constructors",
    genComponentCases(in4Comp, [combineMat2]) +\
    genComponentCases(in9Comp, [combineMat3]) +\
    genComponentCases(in16Comp, [combineMat4])
    ))

# Main program.

if __name__ == "__main__":
    print("Generating shader case files.")
    writeAllCases("conversions.test", allConversionCases)
