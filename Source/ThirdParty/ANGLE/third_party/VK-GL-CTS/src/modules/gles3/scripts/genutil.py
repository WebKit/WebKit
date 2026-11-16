# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# drawElements Quality Program utilities
# --------------------------------------
#
# Copyright 2016 The Android Open Source Project
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

import re
import math
import random

PREAMBLE = """
# WARNING: This file is auto-generated. Do NOT modify it manually, but rather
# modify the generating script file. Otherwise changes will be lost!
"""[1:]

class CaseGroup(object):
    def __init__(self, name, description, children):
        self.name = name
        self.description = description
        self.children = children

class ShaderCase(object):
    def __init__(self):
        pass

g_processedCases = {}

def indentTextBlock(text, indent):
    indentStr = indent * "\t"
    lines = text.split("\n")
    lines = [indentStr + line for line in lines]
    lines = [ ["", line][line.strip() != ""] for line in lines]
    return "\n".join(lines)

def writeCase(f, case, indent, prefix):
    print("    %s" % (prefix + case.name))
    if isinstance(case, CaseGroup):
        f.write(indentTextBlock('group %s "%s"\n\n' % (case.name, case.description), indent))
        for child in case.children:
            writeCase(f, child, indent + 1, prefix + case.name + ".")
        f.write(indentTextBlock("\nend # %s\n" % case.name, indent))
    else:
        # \todo [petri] Fix hack.
        fullPath = prefix + case.name
        assert (fullPath not in g_processedCases)
        g_processedCases[fullPath] = None
        f.write(indentTextBlock(str(case) + "\n", indent))

def writeAllCases(fileName, caseList):
    # Write all cases to file.
    print("  %s.." % fileName)
    f = file(fileName, "wb")
    f.write(PREAMBLE + "\n")
    for case in caseList:
        writeCase(f, case, 0, "")
    f.close()

    print("done! (%d cases written)" % len(g_processedCases))

# Template operations.

def genValues(inputs, outputs):
    res = []
    for (name, values) in inputs:
        res.append("input %s = [ %s ];" % (name, " | ".join([str(v) for v in values]).lower()))
    for (name, values) in outputs:
        res.append("output %s = [ %s ];" % (name, " | ".join([str(v) for v in values]).lower()))
    return ("\n".join(res))

def fillTemplate(template, params):
    s = template

    for (key, value) in params.items():
        m = re.search(r"^(\s*)\$\{\{%s\}\}$" % key, s, re.M)
        if m is not None:
            start = m.start(0)
            end = m.end(0)
            ws = m.group(1)
            if value is not None:
                repl = "\n".join(["%s%s" % (ws, line) for line in value.split("\n")])
                s = s[:start] + repl + s[end:]
            else:
                s = s[:start] + s[end+1:] # drop the whole line
        else:
            s = s.replace("${{%s}}" % key, value)
    return s

# Return shuffled version of list
def shuffled(lst):
    tmp = lst[:]
    random.shuffle(tmp)
    return tmp

def repeatToLength(lst, toLength):
    return (toLength / len(lst)) * lst + lst[: toLength % len(lst)]

# Helpers to convert a list of Scalar/Vec values into another type.

def toFloat(lst): return [Scalar(float(v.x)) for v in lst]
def toInt(lst): return [Scalar(int(v.x)) for v in lst]
def toUint(lst): return [Uint(int(v.x)) for v in lst]
def toBool(lst): return [Scalar(bool(v.x)) for v in lst]
def toVec4(lst): return [v.toFloat().toVec4() for v in lst]
def toVec3(lst): return [v.toFloat().toVec3() for v in lst]
def toVec2(lst): return [v.toFloat().toVec2() for v in lst]
def toIVec4(lst): return [v.toInt().toVec4() for v in lst]
def toIVec3(lst): return [v.toInt().toVec3() for v in lst]
def toIVec2(lst): return [v.toInt().toVec2() for v in lst]
def toBVec4(lst): return [v.toBool().toVec4() for v in lst]
def toBVec3(lst): return [v.toBool().toVec3() for v in lst]
def toBVec2(lst): return [v.toBool().toVec2() for v in lst]
def toUVec4(lst): return [v.toUint().toUVec4() for v in lst]
def toUVec3(lst): return [v.toUint().toUVec3() for v in lst]
def toUVec2(lst): return [v.toUint().toUVec2() for v in lst]
def toMat2(lst): return [v.toMat2() for v in lst]
def toMat2x3(lst): return [v.toMat2x3() for v in lst]
def toMat2x4(lst): return [v.toMat2x4() for v in lst]
def toMat3x2(lst): return [v.toMat3x2() for v in lst]
def toMat3(lst): return [v.toMat3() for v in lst]
def toMat3x4(lst): return [v.toMat3x4() for v in lst]
def toMat4x2(lst): return [v.toMat4x2() for v in lst]
def toMat4x3(lst): return [v.toMat4x3() for v in lst]
def toMat4(lst): return [v.toMat4() for v in lst]

# Random value generation.

class GenRandom(object):
    def __init__(self):
        pass

    def uniformVec4(self, count, mn, mx):
        ret = [Vec4(random.uniform(mn, mx), random.uniform(mn, mx), random.uniform(mn, mx), random.uniform(mn, mx)) for x in xrange(count)]
        ret[0].x = mn
        ret[1].x = mx
        ret[2].x = (mn + mx) * 0.5
        return ret

    def uniformBVec4(self, count):
        ret = [Vec4(random.random() >= 0.5, random.random() >= 0.5, random.random() >= 0.5, random.random() >= 0.5) for x in xrange(count)]
        ret[0].x = True
        ret[1].x = False
        return ret

#    def uniform(self,

# Math operating on Scalar/Vector types.

def glslSign(a): return 0.0 if (a == 0) else +1.0 if (a > 0.0) else -1.0
def glslMod(x, y): return x - y*math.floor(x/y)
def glslClamp(x, mn, mx): return mn if (x < mn) else mx if (x > mx) else x

class GenMath(object):
    @staticmethod
    def unary(func): return lambda val: val.applyUnary(func)

    @staticmethod
    def binary(func): return lambda a, b: (b.expandVec(a)).applyBinary(func, a.expandVec(b))

    @staticmethod
    def frac(val): return val.applyUnary(lambda x: x - math.floor(x))

    @staticmethod
    def exp2(val): return val.applyUnary(lambda x: math.pow(2.0, x))

    @staticmethod
    def log2(val): return val.applyUnary(lambda x: math.log(x, 2.0))

    @staticmethod
    def rsq(val): return val.applyUnary(lambda x: 1.0 / math.sqrt(x))

    @staticmethod
    def sign(val): return val.applyUnary(glslSign)

    @staticmethod
    def isEqual(a, b): return Scalar(a.isEqual(b))

    @staticmethod
    def isNotEqual(a, b): return Scalar(not a.isEqual(b))

    @staticmethod
    def step(a, b): return (b.expandVec(a)).applyBinary(lambda edge, x: [1.0, 0.0][x < edge], a.expandVec(b))

    @staticmethod
    def length(a): return a.length()

    @staticmethod
    def distance(a, b): return a.distance(b)

    @staticmethod
    def dot(a, b): return a.dot(b)

    @staticmethod
    def cross(a, b): return a.cross(b)

    @staticmethod
    def normalize(a): return a.normalize()

    @staticmethod
    def boolAny(a): return a.boolAny()

    @staticmethod
    def boolAll(a): return a.boolAll()

    @staticmethod
    def boolNot(a): return a.boolNot()

    @staticmethod
    def abs(a): return a.abs()

# ..

class Scalar(object):
    def __init__(self, x):
        self.x = x

    def applyUnary(self, func): return Scalar(func(self.x))
    def applyBinary(self, func, other): return Scalar(func(self.x, other.x))

    def isEqual(self, other): assert isinstance(other, Scalar); return (self.x == other.x)

    def expandVec(self, val): return val
    def toScalar(self): return Scalar(self.x)
    def toVec2(self): return Vec2(self.x, self.x)
    def toVec3(self): return Vec3(self.x, self.x, self.x)
    def toVec4(self): return Vec4(self.x, self.x, self.x, self.x)
    def toUVec2(self): return UVec2(self.x, self.x)
    def toUVec3(self): return UVec3(self.x, self.x, self.x)
    def toUVec4(self): return UVec4(self.x, self.x, self.x, self.x)
    def toMat2(self): return Mat.fromScalar(2, 2, float(self.x))
    def toMat2x3(self): return Mat.fromScalar(2, 3, float(self.x))
    def toMat2x4(self): return Mat.fromScalar(2, 4, float(self.x))
    def toMat3x2(self): return Mat.fromScalar(3, 2, float(self.x))
    def toMat3(self): return Mat.fromScalar(3, 3, float(self.x))
    def toMat3x4(self): return Mat.fromScalar(3, 4, float(self.x))
    def toMat4x2(self): return Mat.fromScalar(4, 2, float(self.x))
    def toMat4x3(self): return Mat.fromScalar(4, 3, float(self.x))
    def toMat4(self): return Mat.fromScalar(4, 4, float(self.x))

    def toFloat(self): return Scalar(float(self.x))
    def toInt(self): return Scalar(int(self.x))
    def toUint(self): return Uint(int(self.x))
    def toBool(self): return Scalar(bool(self.x))

    def getNumScalars(self): return 1
    def getScalars(self): return [self.x]

    def typeString(self):
        if isinstance(self.x, bool):
            return "bool"
        elif isinstance(self.x, int):
            return "int"
        elif isinstance(self.x, float):
            return "float"
        else:
            assert False

    def vec4Swizzle(self):
        return ""

    def __str__(self):
        return str(self.x).lower()

    def __float__(self):
        return float(self.x)

    def length(self):
        return Scalar(abs(self.x))

    def distance(self, v):
        assert isinstance(v, Scalar)
        return Scalar(abs(self.x - v.x))

    def dot(self, v):
        assert isinstance(v, Scalar)
        return Scalar(self.x * v.x)

    def normalize(self):
        return Scalar(glslSign(self.x))

    def abs(self):
        if isinstance(self.x, bool):
            return Scalar(self.x)
        else:
            return Scalar(abs(self.x))

    def __neg__(self):
        return Scalar(-self.x)

    def __add__(self, val):
        assert isinstance(val, Scalar)
        return Scalar(self.x + val.x)

    def __sub__(self, val):
        return self + (-val)

    def __mul__(self, val):
        if isinstance(val, Scalar):
            return Scalar(self.x * val.x)
        elif isinstance(val, Vec2):
            return Vec2(self.x * val.x, self.x * val.y)
        elif isinstance(val, Vec3):
            return Vec3(self.x * val.x, self.x * val.y, self.x * val.z)
        elif isinstance(val, Vec4):
            return Vec4(self.x * val.x, self.x * val.y, self.x * val.z, self.x * val.w)
        else:
            assert False

    def __div__(self, val):
        if isinstance(val, Scalar):
            return Scalar(self.x / val.x)
        elif isinstance(val, Vec2):
            return Vec2(self.x / val.x, self.x / val.y)
        elif isinstance(val, Vec3):
            return Vec3(self.x / val.x, self.x / val.y, self.x / val.z)
        elif isinstance(val, Vec4):
            return Vec4(self.x / val.x, self.x / val.y, self.x / val.z, self.x / val.w)
        else:
            assert False

class Uint(Scalar):
    def __init__(self, x):
        assert x >= 0
        self.x = x

    def typeString(self):
        return "uint"

    def abs(self):
        return Scalar.abs(self).toUint()

    def __neg__(self):
        return Scalar.__neg__(self).toUint()

    def __add__(self, val):
        return Scalar.__add__(self, val).toUint()

    def __sub__(self, val):
        return self + (-val)

    def __mul__(self, val):
        return Scalar.__mul__(self, val).toUint()

    def __div__(self, val):
        return Scalar.__div__(self, val).toUint()

class Vec(object):
    @staticmethod
    def fromScalarList(lst):
        assert (len(lst) >= 1 and len(lst) <= 4)
        if (len(lst) == 1): return Scalar(lst[0])
        elif (len(lst) == 2): return Vec2(lst[0], lst[1])
        elif (len(lst) == 3): return Vec3(lst[0], lst[1], lst[2])
        else: return Vec4(lst[0], lst[1], lst[2], lst[3])

    def isEqual(self, other):
        assert isinstance(other, Vec);
        return (self.getScalars() == other.getScalars())

    def length(self):
        return Scalar(math.sqrt(self.dot(self).x))

    def normalize(self):
        return self * Scalar(1.0 / self.length().x)

    def swizzle(self, indexList):
        inScalars = self.getScalars()
        outScalars = map(lambda ndx: inScalars[ndx], indexList)
        return Vec.fromScalarList(outScalars)

    def __init__(self):
        pass

    def __eq__(self, other):
        return self.isEqual(other)

    def __ne__(self, other):
        return not self.isEqual(other)

class Vec2(Vec):
    def __init__(self, x, y):
        assert(x.__class__ == y.__class__)
        self.x = x
        self.y = y

    def applyUnary(self, func): return Vec2(func(self.x), func(self.y))
    def applyBinary(self, func, other): return Vec2(func(self.x, other.x), func(self.y, other.y))

    def expandVec(self, val): return val.toVec2()
    def toScalar(self): return Scalar(self.x)
    def toVec2(self): return Vec2(self.x, self.y)
    def toVec3(self): return Vec3(self.x, self.y, 0.0)
    def toVec4(self): return Vec4(self.x, self.y, 0.0, 0.0)
    def toUVec2(self): return UVec2(self.x, self.y)
    def toUVec3(self): return UVec3(self.x, self.y, 0.0)
    def toUVec4(self): return UVec4(self.x, self.y, 0.0, 0.0)
    def toMat2(self): return Mat2(float(self.x), 0.0, 0.0, float(self.y));

    def toFloat(self): return Vec2(float(self.x), float(self.y))
    def toInt(self): return Vec2(int(self.x), int(self.y))
    def toUint(self): return UVec2(int(self.x), int(self.y))
    def toBool(self): return Vec2(bool(self.x), bool(self.y))

    def getNumScalars(self): return 2
    def getScalars(self): return [self.x, self.y]

    def typeString(self):
        if isinstance(self.x, bool):
            return "bvec2"
        elif isinstance(self.x, int):
            return "ivec2"
        elif isinstance(self.x, float):
            return "vec2"
        else:
            assert False

    def vec4Swizzle(self):
        return ".xyxy"

    def __str__(self):
        if isinstance(self.x, bool):
            return "bvec2(%s, %s)" % (str(self.x).lower(), str(self.y).lower())
        elif isinstance(self.x, int):
            return "ivec2(%i, %i)" % (self.x, self.y)
        elif isinstance(self.x, float):
            return "vec2(%s, %s)" % (self.x, self.y)
        else:
            assert False

    def distance(self, v):
        assert isinstance(v, Vec2)
        return (self - v).length()

    def dot(self, v):
        assert isinstance(v, Vec2)
        return Scalar(self.x*v.x + self.y*v.y)

    def abs(self):
        if isinstance(self.x, bool):
            return Vec2(self.x, self.y)
        else:
            return Vec2(abs(self.x), abs(self.y))

    def __neg__(self):
        return Vec2(-self.x, -self.y)

    def __add__(self, val):
        if isinstance(val, Scalar):
            return Vec2(self.x + val, self.y + val)
        elif isinstance(val, Vec2):
            return Vec2(self.x + val.x, self.y + val.y)
        else:
            assert False

    def __sub__(self, val):
        return self + (-val)

    def __mul__(self, val):
        if isinstance(val, Scalar):
            val = val.toVec2()
        assert isinstance(val, Vec2)
        return Vec2(self.x * val.x, self.y * val.y)

    def __div__(self, val):
        if isinstance(val, Scalar):
            return Vec2(self.x / val.x, self.y / val.x)
        else:
            assert isinstance(val, Vec2)
            return Vec2(self.x / val.x, self.y / val.y)

    def boolAny(self): return Scalar(self.x or self.y)
    def boolAll(self): return Scalar(self.x and self.y)
    def boolNot(self): return Vec2(not self.x, not self.y)

class UVec2(Vec2):
    def __init__(self, x, y):
        assert isinstance(x, int) and isinstance(y, int)
        assert x >= 0 and y >= 0
        Vec2.__init__(self, x, y)

    def typeString(self):
        return "uvec2"

    def __str__(self):
        return "uvec2(%i, %i)" % (self.x, self.y)

    def abs(self):
        return Vec2.abs(self).toUint()

class Vec3(Vec):
    def __init__(self, x, y, z):
        assert((x.__class__ == y.__class__) and (x.__class__ == z.__class__))
        self.x = x
        self.y = y
        self.z = z

    def applyUnary(self, func): return Vec3(func(self.x), func(self.y), func(self.z))
    def applyBinary(self, func, other): return Vec3(func(self.x, other.x), func(self.y, other.y), func(self.z, other.z))

    def expandVec(self, val): return val.toVec3()
    def toScalar(self): return Scalar(self.x)
    def toVec2(self): return Vec2(self.x, self.y)
    def toVec3(self): return Vec3(self.x, self.y, self.z)
    def toVec4(self): return Vec4(self.x, self.y, self.z, 0.0)
    def toUVec2(self): return UVec2(self.x, self.y)
    def toUVec3(self): return UVec3(self.x, self.y, self.z)
    def toUVec4(self): return UVec4(self.x, self.y, self.z, 0.0)
    def toMat3(self): return Mat3(float(self.x), 0.0, 0.0,  0.0, float(self.y), 0.0,  0.0, 0.0, float(self.z));

    def toFloat(self): return Vec3(float(self.x), float(self.y), float(self.z))
    def toInt(self): return Vec3(int(self.x), int(self.y), int(self.z))
    def toUint(self): return UVec3(int(self.x), int(self.y), int(self.z))
    def toBool(self): return Vec3(bool(self.x), bool(self.y), bool(self.z))

    def getNumScalars(self): return 3
    def getScalars(self): return [self.x, self.y, self.z]

    def typeString(self):
        if isinstance(self.x, bool):
            return "bvec3"
        elif isinstance(self.x, int):
            return "ivec3"
        elif isinstance(self.x, float):
            return "vec3"
        else:
            assert False

    def vec4Swizzle(self):
        return ".xyzx"

    def __str__(self):
        if isinstance(self.x, bool):
            return "bvec3(%s, %s, %s)" % (str(self.x).lower(), str(self.y).lower(), str(self.z).lower())
        elif isinstance(self.x, int):
            return "ivec3(%i, %i, %i)" % (self.x, self.y, self.z)
        elif isinstance(self.x, float):
            return "vec3(%s, %s, %s)" % (self.x, self.y, self.z)
        else:
            assert False

    def distance(self, v):
        assert isinstance(v, Vec3)
        return (self - v).length()

    def dot(self, v):
        assert isinstance(v, Vec3)
        return Scalar(self.x*v.x + self.y*v.y + self.z*v.z)

    def cross(self, v):
        assert isinstance(v, Vec3)
        return Vec3(self.y*v.z - v.y*self.z,
                    self.z*v.x - v.z*self.x,
                    self.x*v.y - v.x*self.y)

    def abs(self):
        if isinstance(self.x, bool):
            return Vec3(self.x, self.y, self.z)
        else:
            return Vec3(abs(self.x), abs(self.y), abs(self.z))

    def __neg__(self):
        return Vec3(-self.x, -self.y, -self.z)

    def __add__(self, val):
        if isinstance(val, Scalar):
            return Vec3(self.x + val, self.y + val)
        elif isinstance(val, Vec3):
            return Vec3(self.x + val.x, self.y + val.y, self.z + val.z)
        else:
            assert False

    def __sub__(self, val):
        return self + (-val)

    def __mul__(self, val):
        if isinstance(val, Scalar):
            val = val.toVec3()
        assert isinstance(val, Vec3)
        return Vec3(self.x * val.x, self.y * val.y, self.z * val.z)

    def __div__(self, val):
        if isinstance(val, Scalar):
            return Vec3(self.x / val.x, self.y / val.x, self.z / val.x)
        elif isinstance(val, Vec3):
            return Vec3(self.x / val.x, self.y / val.y, self.z / val.z)
        else:
            assert False

    def boolAny(self): return Scalar(self.x or self.y or self.z)
    def boolAll(self): return Scalar(self.x and self.y and self.z)
    def boolNot(self): return Vec3(not self.x, not self.y, not self.z)

class UVec3(Vec3):
    def __init__(self, x, y, z):
        assert isinstance(x, int) and isinstance(y, int) and isinstance(z, int)
        assert x >= 0 and y >= 0 and z >= 0
        Vec3.__init__(self, x, y, z)

    def typeString(self):
        return "uvec3"

    def __str__(self):
        return "uvec3(%i, %i, %i)" % (self.x, self.y, self.z)

    def abs(self):
        return Vec3.abs(self).toUint()

class Vec4(Vec):
    def __init__(self, x, y, z, w):
        assert((x.__class__ == y.__class__) and (x.__class__ == z.__class__) and (x.__class__ == w.__class__))
        self.x = x
        self.y = y
        self.z = z
        self.w = w

    def applyUnary(self, func): return Vec4(func(self.x), func(self.y), func(self.z), func(self.w))
    def applyBinary(self, func, other): return Vec4(func(self.x, other.x), func(self.y, other.y), func(self.z, other.z), func(self.w, other.w))

    def expandVec(self, val): return val.toVec4()
    def toScalar(self): return Scalar(self.x)
    def toVec2(self): return Vec2(self.x, self.y)
    def toVec3(self): return Vec3(self.x, self.y, self.z)
    def toVec4(self): return Vec4(self.x, self.y, self.z, self.w)
    def toUVec2(self): return UVec2(self.x, self.y)
    def toUVec3(self): return UVec3(self.x, self.y, self.z)
    def toUVec4(self): return UVec4(self.x, self.y, self.z, self.w)
    def toMat2(self): return Mat2(float(self.x), float(self.y), float(self.z), float(self.w))
    def toMat4(self): return Mat4(float(self.x), 0.0, 0.0, 0.0,  0.0, float(self.y), 0.0, 0.0,  0.0, 0.0, float(self.z), 0.0,  0.0, 0.0, 0.0, float(self.w));

    def toFloat(self): return Vec4(float(self.x), float(self.y), float(self.z), float(self.w))
    def toInt(self): return Vec4(int(self.x), int(self.y), int(self.z), int(self.w))
    def toUint(self): return UVec4(int(self.x), int(self.y), int(self.z), int(self.w))
    def toBool(self): return Vec4(bool(self.x), bool(self.y), bool(self.z), bool(self.w))

    def getNumScalars(self): return 4
    def getScalars(self): return [self.x, self.y, self.z, self.w]

    def typeString(self):
        if isinstance(self.x, bool):
            return "bvec4"
        elif isinstance(self.x, int):
            return "ivec4"
        elif isinstance(self.x, float):
            return "vec4"
        else:
            assert False

    def vec4Swizzle(self):
        return ""

    def __str__(self):
        if isinstance(self.x, bool):
            return "bvec4(%s, %s, %s, %s)" % (str(self.x).lower(), str(self.y).lower(), str(self.z).lower(), str(self.w).lower())
        elif isinstance(self.x, int):
            return "ivec4(%i, %i, %i, %i)" % (self.x, self.y, self.z, self.w)
        elif isinstance(self.x, float):
            return "vec4(%s, %s, %s, %s)" % (self.x, self.y, self.z, self.w)
        else:
            assert False

    def distance(self, v):
        assert isinstance(v, Vec4)
        return (self - v).length()

    def dot(self, v):
        assert isinstance(v, Vec4)
        return Scalar(self.x*v.x + self.y*v.y + self.z*v.z + self.w*v.w)

    def abs(self):
        if isinstance(self.x, bool):
            return Vec4(self.x, self.y, self.z, self.w)
        else:
            return Vec4(abs(self.x), abs(self.y), abs(self.z), abs(self.w))

    def __neg__(self):
        return Vec4(-self.x, -self.y, -self.z, -self.w)

    def __add__(self, val):
        if isinstance(val, Scalar):
            return Vec3(self.x + val, self.y + val)
        elif isinstance(val, Vec4):
            return Vec4(self.x + val.x, self.y + val.y, self.z + val.z, self.w + val.w)
        else:
            assert False

    def __sub__(self, val):
        return self + (-val)

    def __mul__(self, val):
        if isinstance(val, Scalar):
            val = val.toVec4()
        assert isinstance(val, Vec4)
        return Vec4(self.x * val.x, self.y * val.y, self.z * val.z, self.w * val.w)

    def __div__(self, val):
        if isinstance(val, Scalar):
            return Vec4(self.x / val.x, self.y / val.x, self.z / val.x, self.w / val.x)
        elif isinstance(val, Vec4):
            return Vec4(self.x / val.x, self.y / val.y, self.z / val.z, self.w / val.w)
        else:
            assert False

    def boolAny(self): return Scalar(self.x or self.y or self.z or self.w)
    def boolAll(self): return Scalar(self.x and self.y and self.z and self.w)
    def boolNot(self): return Vec4(not self.x, not self.y, not self.z, not self.w)

class UVec4(Vec4):
    def __init__(self, x, y, z, w):
        assert isinstance(x, int) and isinstance(y, int) and isinstance(z, int) and isinstance(w, int)
        assert x >= 0 and y >= 0 and z >= 0 and w >= 0
        Vec4.__init__(self, x, y, z, w)

    def typeString(self):
        return "uvec4"

    def __str__(self):
        return "uvec4(%i, %i, %i, %i)" % (self.x, self.y, self.z, self.w)

    def abs(self):
        return Vec4.abs(self).toUint()

# \note Column-major storage.
class Mat(object):
    def __init__ (self, numCols, numRows, scalars):
        assert len(scalars) == numRows*numCols
        self.numCols = numCols
        self.numRows = numRows
        self.scalars = scalars

    @staticmethod
    def fromScalar (numCols, numRows, scalar):
        scalars = []
        for col in range(0, numCols):
            for row in range(0, numRows):
                scalars.append(scalar if col == row else 0.0)
        return Mat(numCols, numRows, scalars)

    @staticmethod
    def identity (numCols, numRows):
        return Mat.fromScalar(numCols, numRows, 1.0)

    def get (self, colNdx, rowNdx):
        assert 0 <= colNdx and colNdx < self.numCols
        assert 0 <= rowNdx and rowNdx < self.numRows
        return self.scalars[colNdx*self.numRows + rowNdx]

    def set (self, colNdx, rowNdx, scalar):
        assert 0 <= colNdx and colNdx < self.numCols
        assert 0 <= rowNdx and rowNdx < self.numRows
        self.scalars[colNdx*self.numRows + rowNdx] = scalar

    def toMatrix (self, numCols, numRows):
        res = Mat.identity(numCols, numRows)
        for col in range(0, min(self.numCols, numCols)):
            for row in range(0, min(self.numRows, numRows)):
                res.set(col, row, self.get(col, row))
        return res

    def toMat2 (self): return self.toMatrix(2, 2)
    def toMat2x3 (self): return self.toMatrix(2, 3)
    def toMat2x4 (self): return self.toMatrix(2, 4)
    def toMat3x2 (self): return self.toMatrix(3, 2)
    def toMat3 (self): return self.toMatrix(3, 3)
    def toMat3x4 (self): return self.toMatrix(3, 4)
    def toMat4x2 (self): return self.toMatrix(4, 2)
    def toMat4x3 (self): return self.toMatrix(4, 3)
    def toMat4 (self): return self.toMatrix(4, 4)

    def typeString(self):
        if self.numRows == self.numCols:
            return "mat%d" % self.numRows
        else:
            return "mat%dx%d" % (self.numCols, self.numRows)

    def __str__(self):
        return "%s(%s)" % (self.typeString(), ", ".join(["%s" % s for s in self.scalars]))

    def isTypeEqual (self, other):
        return isinstance(other, Mat) and self.numRows == other.numRows and self.numCols == other.numCols

    def isEqual(self, other):
        assert self.isTypeEqual(other)
        return (self.scalars == other.scalars)

    def compMul(self, val):
        assert self.isTypeEqual(val)
        return Mat(self.numRows, self.numCols, [self.scalars(i) * val.scalars(i) for i in range(self.numRows*self.numCols)])

class Mat2(Mat):
    def __init__(self, m00, m01, m10, m11):
        Mat.__init__(self, 2, 2, [m00, m10, m01, m11])

class Mat3(Mat):
    def __init__(self, m00, m01, m02, m10, m11, m12, m20, m21, m22):
        Mat.__init__(self, 3, 3, [m00, m10, m20,
                                  m01, m11, m21,
                                  m02, m12, m22])

class Mat4(Mat):
    def __init__(self, m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33):
        Mat.__init__(self, 4, 4, [m00, m10, m20, m30,
                                  m01, m11, m21, m31,
                                  m02, m12, m22, m32,
                                  m03, m13, m23, m33])
