/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Uniform API tests.
 *
 * \todo [2013-02-26 nuutti] Much duplication between this and ES3.
 *                             Utilities to glshared?
 *//*--------------------------------------------------------------------*/

#include "es2fUniformApiTests.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluShaderProgram.hpp"
#include "gluVarType.hpp"
#include "gluPixelTransfer.hpp"
#include "gluTextureUtil.hpp"
#include "gluTexture.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "tcuCommandLine.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deSharedPtr.hpp"
#include "deString.h"
#include "deMemory.h"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include <set>
#include <cstring>

using namespace glw;

namespace deqp
{
namespace gles2
{
namespace Functional
{

using de::Random;
using de::SharedPtr;
using glu::ShaderProgram;
using glu::StructType;
using std::string;
using std::vector;
using tcu::ScopedLogSection;
using tcu::TestLog;

typedef bool (*dataTypePredicate)(glu::DataType);

static const int MAX_RENDER_WIDTH         = 32;
static const int MAX_RENDER_HEIGHT        = 32;
static const int MAX_NUM_SAMPLER_UNIFORMS = 16;

static const glu::DataType s_testDataTypes[] = {
    glu::TYPE_FLOAT,      glu::TYPE_FLOAT_VEC2,  glu::TYPE_FLOAT_VEC3, glu::TYPE_FLOAT_VEC4,
    glu::TYPE_FLOAT_MAT2, glu::TYPE_FLOAT_MAT3,  glu::TYPE_FLOAT_MAT4,

    glu::TYPE_INT,        glu::TYPE_INT_VEC2,    glu::TYPE_INT_VEC3,   glu::TYPE_INT_VEC4,

    glu::TYPE_BOOL,       glu::TYPE_BOOL_VEC2,   glu::TYPE_BOOL_VEC3,  glu::TYPE_BOOL_VEC4,

    glu::TYPE_SAMPLER_2D, glu::TYPE_SAMPLER_CUBE};

static inline int getGLInt(const glw::Functions &funcs, const uint32_t name)
{
    int val = -1;
    funcs.getIntegerv(name, &val);
    return val;
}

static inline tcu::Vec4 vec4FromPtr(const float *const ptr)
{
    tcu::Vec4 result;
    for (int i = 0; i < 4; i++)
        result[i] = ptr[i];
    return result;
}

static inline string beforeLast(const string &str, const char c)
{
    return str.substr(0, str.find_last_of(c));
}

static inline void fillWithColor(const tcu::PixelBufferAccess &access, const tcu::Vec4 &color)
{
    for (int z = 0; z < access.getDepth(); z++)
        for (int y = 0; y < access.getHeight(); y++)
            for (int x = 0; x < access.getWidth(); x++)
                access.setPixel(color, x, y, z);
}

static inline int getSamplerNumLookupDimensions(const glu::DataType type)
{
    switch (type)
    {
    case glu::TYPE_SAMPLER_2D:
        return 2;

    case glu::TYPE_SAMPLER_CUBE:
        return 3;

    default: // \note All others than 2d and cube are gles3-only types.
        DE_ASSERT(false);
        return 0;
    }
}

template <glu::DataType T>
static bool dataTypeEquals(const glu::DataType t)
{
    return t == T;
}

template <int N>
static bool dataTypeIsMatrixWithNRows(const glu::DataType t)
{
    return glu::isDataTypeMatrix(t) && glu::getDataTypeMatrixNumRows(t) == N;
}

static bool typeContainsMatchingBasicType(const glu::VarType &type, const dataTypePredicate predicate)
{
    if (type.isBasicType())
        return predicate(type.getBasicType());
    else if (type.isArrayType())
        return typeContainsMatchingBasicType(type.getElementType(), predicate);
    else
    {
        DE_ASSERT(type.isStructType());
        const StructType &structType = *type.getStructPtr();
        for (int i = 0; i < structType.getNumMembers(); i++)
            if (typeContainsMatchingBasicType(structType.getMember(i).getType(), predicate))
                return true;
        return false;
    }
}

static void getDistinctSamplerTypes(vector<glu::DataType> &dst, const glu::VarType &type)
{
    if (type.isBasicType())
    {
        const glu::DataType basicType = type.getBasicType();
        if (glu::isDataTypeSampler(basicType) && std::find(dst.begin(), dst.end(), basicType) == dst.end())
            dst.push_back(basicType);
    }
    else if (type.isArrayType())
        getDistinctSamplerTypes(dst, type.getElementType());
    else
    {
        DE_ASSERT(type.isStructType());
        const StructType &structType = *type.getStructPtr();
        for (int i = 0; i < structType.getNumMembers(); i++)
            getDistinctSamplerTypes(dst, structType.getMember(i).getType());
    }
}

static int getNumSamplersInType(const glu::VarType &type)
{
    if (type.isBasicType())
        return glu::isDataTypeSampler(type.getBasicType()) ? 1 : 0;
    else if (type.isArrayType())
        return getNumSamplersInType(type.getElementType()) * type.getArraySize();
    else
    {
        DE_ASSERT(type.isStructType());
        const StructType &structType = *type.getStructPtr();
        int sum                      = 0;
        for (int i = 0; i < structType.getNumMembers(); i++)
            sum += getNumSamplersInType(structType.getMember(i).getType());
        return sum;
    }
}

static glu::VarType generateRandomType(const int maxDepth, int &curStructIdx,
                                       vector<const StructType *> &structTypesDst, Random &rnd)
{
    const bool isStruct = maxDepth > 0 && rnd.getFloat() < 0.2f;
    const bool isArray  = rnd.getFloat() < 0.3f;

    if (isStruct)
    {
        const int numMembers         = rnd.getInt(1, 5);
        StructType *const structType = new StructType(("structType" + de::toString(curStructIdx++)).c_str());

        for (int i = 0; i < numMembers; i++)
            structType->addMember(("m" + de::toString(i)).c_str(),
                                  generateRandomType(maxDepth - 1, curStructIdx, structTypesDst, rnd));

        structTypesDst.push_back(structType);
        return isArray ? glu::VarType(glu::VarType(structType), rnd.getInt(1, 5)) : glu::VarType(structType);
    }
    else
    {
        const glu::DataType basicType =
            (glu::DataType)s_testDataTypes[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testDataTypes) - 1)];
        const glu::Precision precision =
            glu::isDataTypeBoolOrBVec(basicType) ? glu::PRECISION_LAST : glu::PRECISION_MEDIUMP;
        return isArray ? glu::VarType(glu::VarType(basicType, precision), rnd.getInt(1, 5)) :
                         glu::VarType(basicType, precision);
    }
}

namespace
{

struct VarValue
{
    glu::DataType type;

    union
    {
        float floatV[4 * 4]; // At most mat4. \note Matrices here are column-major.
        int32_t intV[4];
        bool boolV[4];
        struct
        {
            int unit;
            float fillColor[4];
        } samplerV;
    } val;
};

enum CaseShaderType
{
    CASESHADERTYPE_VERTEX = 0,
    CASESHADERTYPE_FRAGMENT,
    CASESHADERTYPE_BOTH,

    CASESHADERTYPE_LAST
};

struct Uniform
{
    string name;
    glu::VarType type;

    Uniform(const char *const name_, const glu::VarType &type_) : name(name_), type(type_)
    {
    }
};

// A set of uniforms, along with related struct types.
class UniformCollection
{
public:
    int getNumUniforms(void) const
    {
        return (int)m_uniforms.size();
    }
    int getNumStructTypes(void) const
    {
        return (int)m_structTypes.size();
    }
    Uniform &getUniform(const int ndx)
    {
        return m_uniforms[ndx];
    }
    const Uniform &getUniform(const int ndx) const
    {
        return m_uniforms[ndx];
    }
    const StructType *getStructType(const int ndx) const
    {
        return m_structTypes[ndx];
    }
    void addUniform(const Uniform &uniform)
    {
        m_uniforms.push_back(uniform);
    }
    void addStructType(const StructType *const type)
    {
        m_structTypes.push_back(type);
    }

    UniformCollection(void)
    {
    }
    ~UniformCollection(void)
    {
        for (int i = 0; i < (int)m_structTypes.size(); i++)
            delete m_structTypes[i];
    }

    // Add the contents of m_uniforms and m_structTypes to receiver, and remove them from this one.
    // \note receiver takes ownership of the struct types.
    void moveContents(UniformCollection &receiver)
    {
        for (int i = 0; i < (int)m_uniforms.size(); i++)
            receiver.addUniform(m_uniforms[i]);
        m_uniforms.clear();

        for (int i = 0; i < (int)m_structTypes.size(); i++)
            receiver.addStructType(m_structTypes[i]);
        m_structTypes.clear();
    }

    bool containsMatchingBasicType(const dataTypePredicate predicate) const
    {
        for (int i = 0; i < (int)m_uniforms.size(); i++)
            if (typeContainsMatchingBasicType(m_uniforms[i].type, predicate))
                return true;
        return false;
    }

    vector<glu::DataType> getSamplerTypes(void) const
    {
        vector<glu::DataType> samplerTypes;
        for (int i = 0; i < (int)m_uniforms.size(); i++)
            getDistinctSamplerTypes(samplerTypes, m_uniforms[i].type);
        return samplerTypes;
    }

    bool containsSeveralSamplerTypes(void) const
    {
        return getSamplerTypes().size() > 1;
    }

    int getNumSamplers(void) const
    {
        int sum = 0;
        for (int i = 0; i < (int)m_uniforms.size(); i++)
            sum += getNumSamplersInType(m_uniforms[i].type);
        return sum;
    }

    static UniformCollection *basic(const glu::DataType type, const char *const nameSuffix = "")
    {
        UniformCollection *const res = new UniformCollection;
        const glu::Precision prec    = glu::isDataTypeBoolOrBVec(type) ? glu::PRECISION_LAST : glu::PRECISION_MEDIUMP;
        res->m_uniforms.push_back(Uniform((string("u_var") + nameSuffix).c_str(), glu::VarType(type, prec)));
        return res;
    }

    static UniformCollection *basicArray(const glu::DataType type, const char *const nameSuffix = "")
    {
        UniformCollection *const res = new UniformCollection;
        const glu::Precision prec    = glu::isDataTypeBoolOrBVec(type) ? glu::PRECISION_LAST : glu::PRECISION_MEDIUMP;
        res->m_uniforms.push_back(
            Uniform((string("u_var") + nameSuffix).c_str(), glu::VarType(glu::VarType(type, prec), 3)));
        return res;
    }

    static UniformCollection *basicStruct(const glu::DataType type0, const glu::DataType type1,
                                          const bool containsArrays, const char *const nameSuffix = "")
    {
        UniformCollection *const res = new UniformCollection;
        const glu::Precision prec0   = glu::isDataTypeBoolOrBVec(type0) ? glu::PRECISION_LAST : glu::PRECISION_MEDIUMP;
        const glu::Precision prec1   = glu::isDataTypeBoolOrBVec(type1) ? glu::PRECISION_LAST : glu::PRECISION_MEDIUMP;

        StructType *const structType = new StructType((string("structType") + nameSuffix).c_str());
        structType->addMember("m0", glu::VarType(type0, prec0));
        structType->addMember("m1", glu::VarType(type1, prec1));
        if (containsArrays)
        {
            structType->addMember("m2", glu::VarType(glu::VarType(type0, prec0), 3));
            structType->addMember("m3", glu::VarType(glu::VarType(type1, prec1), 3));
        }

        res->addStructType(structType);
        res->addUniform(Uniform((string("u_var") + nameSuffix).c_str(), glu::VarType(structType)));

        return res;
    }

    static UniformCollection *structInArray(const glu::DataType type0, const glu::DataType type1,
                                            const bool containsArrays, const char *const nameSuffix = "")
    {
        UniformCollection *const res = basicStruct(type0, type1, containsArrays, nameSuffix);
        res->getUniform(0).type      = glu::VarType(res->getUniform(0).type, 3);
        return res;
    }

    static UniformCollection *nestedArraysStructs(const glu::DataType type0, const glu::DataType type1,
                                                  const char *const nameSuffix = "")
    {
        UniformCollection *const res = new UniformCollection;
        const glu::Precision prec0   = glu::isDataTypeBoolOrBVec(type0) ? glu::PRECISION_LAST : glu::PRECISION_MEDIUMP;
        const glu::Precision prec1   = glu::isDataTypeBoolOrBVec(type1) ? glu::PRECISION_LAST : glu::PRECISION_MEDIUMP;
        StructType *const structType = new StructType((string("structType") + nameSuffix).c_str());
        StructType *const subStructType    = new StructType((string("subStructType") + nameSuffix).c_str());
        StructType *const subSubStructType = new StructType((string("subSubStructType") + nameSuffix).c_str());

        subSubStructType->addMember("mss0", glu::VarType(type0, prec0));
        subSubStructType->addMember("mss1", glu::VarType(type1, prec1));

        subStructType->addMember("ms0", glu::VarType(type1, prec1));
        subStructType->addMember("ms1", glu::VarType(glu::VarType(type0, prec0), 2));
        subStructType->addMember("ms2", glu::VarType(glu::VarType(subSubStructType), 2));

        structType->addMember("m0", glu::VarType(type0, prec0));
        structType->addMember("m1", glu::VarType(subStructType));
        structType->addMember("m2", glu::VarType(type1, prec1));

        res->addStructType(subSubStructType);
        res->addStructType(subStructType);
        res->addStructType(structType);

        res->addUniform(Uniform((string("u_var") + nameSuffix).c_str(), glu::VarType(structType)));

        return res;
    }

    static UniformCollection *multipleBasic(const char *const nameSuffix = "")
    {
        static const glu::DataType types[] = {glu::TYPE_FLOAT, glu::TYPE_INT_VEC3, glu::TYPE_FLOAT_MAT3,
                                              glu::TYPE_BOOL_VEC2};
        UniformCollection *const res       = new UniformCollection;

        for (int i = 0; i < DE_LENGTH_OF_ARRAY(types); i++)
        {
            UniformCollection *const sub = basic(types[i], ("_" + de::toString(i) + nameSuffix).c_str());
            sub->moveContents(*res);
            delete sub;
        }

        return res;
    }

    static UniformCollection *multipleBasicArray(const char *const nameSuffix = "")
    {
        static const glu::DataType types[] = {glu::TYPE_FLOAT, glu::TYPE_INT_VEC3, glu::TYPE_BOOL_VEC2};
        UniformCollection *const res       = new UniformCollection;

        for (int i = 0; i < DE_LENGTH_OF_ARRAY(types); i++)
        {
            UniformCollection *const sub = basicArray(types[i], ("_" + de::toString(i) + nameSuffix).c_str());
            sub->moveContents(*res);
            delete sub;
        }

        return res;
    }

    static UniformCollection *multipleNestedArraysStructs(const char *const nameSuffix = "")
    {
        static const glu::DataType types0[] = {glu::TYPE_FLOAT, glu::TYPE_INT, glu::TYPE_BOOL_VEC4};
        static const glu::DataType types1[] = {glu::TYPE_FLOAT_VEC4, glu::TYPE_INT_VEC4, glu::TYPE_BOOL};
        UniformCollection *const res        = new UniformCollection;

        DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(types0) == DE_LENGTH_OF_ARRAY(types1));

        for (int i = 0; i < DE_LENGTH_OF_ARRAY(types0); i++)
        {
            UniformCollection *const sub =
                nestedArraysStructs(types0[i], types1[i], ("_" + de::toString(i) + nameSuffix).c_str());
            sub->moveContents(*res);
            delete sub;
        }

        return res;
    }

    static UniformCollection *random(const uint32_t seed)
    {
        Random rnd(seed);
        const int numUniforms        = rnd.getInt(1, 5);
        int structIdx                = 0;
        UniformCollection *const res = new UniformCollection;

        for (int i = 0; i < numUniforms; i++)
        {
            vector<const StructType *> structTypes;
            Uniform uniform(("u_var" + de::toString(i)).c_str(), glu::VarType());

            // \note Discard uniforms that would cause number of samplers to exceed MAX_NUM_SAMPLER_UNIFORMS.
            do
            {
                for (int j = 0; j < (int)structTypes.size(); j++)
                    delete structTypes[j];
                structTypes.clear();
                uniform.type = generateRandomType(3, structIdx, structTypes, rnd);
            } while (res->getNumSamplers() + getNumSamplersInType(uniform.type) > MAX_NUM_SAMPLER_UNIFORMS);

            res->addUniform(uniform);
            for (int j = 0; j < (int)structTypes.size(); j++)
                res->addStructType(structTypes[j]);
        }

        return res;
    }

private:
    // \note Copying these would be cumbersome, since deep-copying both m_uniforms and m_structTypes
    // would mean that we'd need to update pointers from uniforms to point to the new structTypes.
    // When the same UniformCollection is needed in several places, a SharedPtr is used instead.
    UniformCollection(const UniformCollection &);            // Not allowed.
    UniformCollection &operator=(const UniformCollection &); // Not allowed.

    vector<Uniform> m_uniforms;
    vector<const StructType *> m_structTypes;
};

} // namespace

static VarValue getSamplerFillValue(const VarValue &sampler)
{
    DE_ASSERT(glu::isDataTypeSampler(sampler.type));

    VarValue result;
    result.type = glu::TYPE_FLOAT_VEC4;

    for (int i = 0; i < 4; i++)
        result.val.floatV[i] = sampler.val.samplerV.fillColor[i];

    return result;
}

static VarValue getSamplerUnitValue(const VarValue &sampler)
{
    DE_ASSERT(glu::isDataTypeSampler(sampler.type));

    VarValue result;
    result.type        = glu::TYPE_INT;
    result.val.intV[0] = sampler.val.samplerV.unit;

    return result;
}

static string shaderVarValueStr(const VarValue &value)
{
    const int numElems = glu::getDataTypeScalarSize(value.type);
    std::ostringstream result;

    if (numElems > 1)
        result << glu::getDataTypeName(value.type) << "(";

    for (int i = 0; i < numElems; i++)
    {
        if (i > 0)
            result << ", ";

        if (glu::isDataTypeFloatOrVec(value.type) || glu::isDataTypeMatrix(value.type))
            result << de::floatToString(value.val.floatV[i], 2);
        else if (glu::isDataTypeIntOrIVec((value.type)))
            result << de::toString(value.val.intV[i]);
        else if (glu::isDataTypeBoolOrBVec((value.type)))
            result << (value.val.boolV[i] ? "true" : "false");
        else if (glu::isDataTypeSampler((value.type)))
            result << shaderVarValueStr(getSamplerFillValue(value));
        else
            DE_ASSERT(false);
    }

    if (numElems > 1)
        result << ")";

    return result.str();
}

static string apiVarValueStr(const VarValue &value)
{
    const int numElems = glu::getDataTypeScalarSize(value.type);
    std::ostringstream result;

    if (numElems > 1)
        result << "(";

    for (int i = 0; i < numElems; i++)
    {
        if (i > 0)
            result << ", ";

        if (glu::isDataTypeFloatOrVec(value.type) || glu::isDataTypeMatrix(value.type))
            result << de::floatToString(value.val.floatV[i], 2);
        else if (glu::isDataTypeIntOrIVec((value.type)))
            result << de::toString(value.val.intV[i]);
        else if (glu::isDataTypeBoolOrBVec((value.type)))
            result << (value.val.boolV[i] ? "true" : "false");
        else if (glu::isDataTypeSampler((value.type)))
            result << value.val.samplerV.unit;
        else
            DE_ASSERT(false);
    }

    if (numElems > 1)
        result << ")";

    return result.str();
}

static VarValue generateRandomVarValue(
    const glu::DataType type, Random &rnd,
    int samplerUnit = -1 /* Used if type is a sampler type. \note Samplers' unit numbers are not randomized. */)
{
    const int numElems = glu::getDataTypeScalarSize(type);
    VarValue result;
    result.type = type;

    DE_ASSERT((samplerUnit >= 0) == (glu::isDataTypeSampler(type)));

    if (glu::isDataTypeFloatOrVec(type) || glu::isDataTypeMatrix(type))
    {
        for (int i = 0; i < numElems; i++)
            result.val.floatV[i] = rnd.getFloat(-10.0f, 10.0f);
    }
    else if (glu::isDataTypeIntOrIVec(type))
    {
        for (int i = 0; i < numElems; i++)
            result.val.intV[i] = rnd.getInt(-10, 10);
    }
    else if (glu::isDataTypeBoolOrBVec(type))
    {
        for (int i = 0; i < numElems; i++)
            result.val.boolV[i] = rnd.getBool();
    }
    else if (glu::isDataTypeSampler(type))
    {
        result.val.samplerV.unit = samplerUnit;

        for (int i = 0; i < 4; i++)
            result.val.samplerV.fillColor[i] = rnd.getFloat(0.0f, 1.0f);
    }
    else
        DE_ASSERT(false);

    return result;
}

static VarValue generateZeroVarValue(const glu::DataType type)
{
    const int numElems = glu::getDataTypeScalarSize(type);
    VarValue result;
    result.type = type;

    if (glu::isDataTypeFloatOrVec(type) || glu::isDataTypeMatrix(type))
    {
        for (int i = 0; i < numElems; i++)
            result.val.floatV[i] = 0.0f;
    }
    else if (glu::isDataTypeIntOrIVec(type))
    {
        for (int i = 0; i < numElems; i++)
            result.val.intV[i] = 0;
    }
    else if (glu::isDataTypeBoolOrBVec(type))
    {
        for (int i = 0; i < numElems; i++)
            result.val.boolV[i] = false;
    }
    else if (glu::isDataTypeSampler(type))
    {
        result.val.samplerV.unit = 0;

        for (int i = 0; i < 4; i++)
            result.val.samplerV.fillColor[i] = 0.12f * (float)i;
    }
    else
        DE_ASSERT(false);

    return result;
}

static bool apiVarValueEquals(const VarValue &a, const VarValue &b)
{
    const int size             = glu::getDataTypeScalarSize(a.type);
    const float floatThreshold = 0.05f;

    DE_ASSERT(a.type == b.type);

    if (glu::isDataTypeFloatOrVec(a.type) || glu::isDataTypeMatrix(a.type))
    {
        for (int i = 0; i < size; i++)
            if (de::abs(a.val.floatV[i] - b.val.floatV[i]) >= floatThreshold)
                return false;
    }
    else if (glu::isDataTypeIntOrIVec(a.type))
    {
        for (int i = 0; i < size; i++)
            if (a.val.intV[i] != b.val.intV[i])
                return false;
    }
    else if (glu::isDataTypeBoolOrBVec(a.type))
    {
        for (int i = 0; i < size; i++)
            if (a.val.boolV[i] != b.val.boolV[i])
                return false;
    }
    else if (glu::isDataTypeSampler(a.type))
    {
        if (a.val.samplerV.unit != b.val.samplerV.unit)
            return false;
    }
    else
        DE_ASSERT(false);

    return true;
}

static VarValue getRandomBoolRepresentation(const VarValue &boolValue, const glu::DataType targetScalarType,
                                            Random &rnd)
{
    DE_ASSERT(glu::isDataTypeBoolOrBVec(boolValue.type));

    const int size                 = glu::getDataTypeScalarSize(boolValue.type);
    const glu::DataType targetType = size == 1 ? targetScalarType : glu::getDataTypeVector(targetScalarType, size);
    VarValue result;
    result.type = targetType;

    switch (targetScalarType)
    {
    case glu::TYPE_INT:
        for (int i = 0; i < size; i++)
        {
            if (boolValue.val.boolV[i])
            {
                result.val.intV[i] = rnd.getInt(-10, 10);
                if (result.val.intV[i] == 0)
                    result.val.intV[i] = 1;
            }
            else
                result.val.intV[i] = 0;
        }
        break;

    case glu::TYPE_FLOAT:
        for (int i = 0; i < size; i++)
        {
            if (boolValue.val.boolV[i])
            {
                result.val.floatV[i] = rnd.getFloat(-10.0f, 10.0f);
                if (result.val.floatV[i] == 0.0f)
                    result.val.floatV[i] = 1.0f;
            }
            else
                result.val.floatV[i] = 0;
        }
        break;

    default:
        DE_ASSERT(false);
    }

    return result;
}

static const char *getCaseShaderTypeName(const CaseShaderType type)
{
    switch (type)
    {
    case CASESHADERTYPE_VERTEX:
        return "vertex";
    case CASESHADERTYPE_FRAGMENT:
        return "fragment";
    case CASESHADERTYPE_BOTH:
        return "both";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

static CaseShaderType randomCaseShaderType(const uint32_t seed)
{
    return (CaseShaderType)Random(seed).getInt(0, CASESHADERTYPE_LAST - 1);
}

class UniformCase : public TestCase, protected glu::CallLogWrapper
{
public:
    enum Feature
    {
        // ARRAYUSAGE_ONLY_MIDDLE_INDEX: only middle index of each array is used in shader. If not given, use all indices.
        FEATURE_ARRAYUSAGE_ONLY_MIDDLE_INDEX = 1 << 0,

        // UNIFORMFUNC_VALUE: use pass-by-value versions of uniform assignment funcs, e.g. glUniform1f(), where possible. If not given, use pass-by-pointer versions.
        FEATURE_UNIFORMFUNC_VALUE = 1 << 1,

        // ARRAYASSIGN: how basic-type arrays are assigned with glUniform*(). If none given, assign each element of an array separately.
        FEATURE_ARRAYASSIGN_FULL          = 1 << 2, //!< Assign all elements of an array with one glUniform*().
        FEATURE_ARRAYASSIGN_BLOCKS_OF_TWO = 1 << 3, //!< Assign two elements per one glUniform*().

        // UNIFORMUSAGE_EVERY_OTHER: use about half of the uniforms. If not given, use all uniforms (except that some array indices may be omitted according to ARRAYUSAGE).
        FEATURE_UNIFORMUSAGE_EVERY_OTHER = 1 << 4,

        // BOOLEANAPITYPE: type used to pass booleans to and from GL api. If none given, use float.
        FEATURE_BOOLEANAPITYPE_INT = 1 << 5,

        // UNIFORMVALUE_ZERO: use zero-valued uniforms. If not given, use random uniform values.
        FEATURE_UNIFORMVALUE_ZERO = 1 << 6,

        // ARRAY_FIRST_ELEM_NAME_NO_INDEX: in certain API functions, when referring to the first element of an array, use just the array name without [0] at the end.
        FEATURE_ARRAY_FIRST_ELEM_NAME_NO_INDEX = 1 << 7
    };

    UniformCase(Context &context, const char *name, const char *description, CaseShaderType caseType,
                const SharedPtr<const UniformCollection> &uniformCollection, uint32_t features);
    UniformCase(Context &context, const char *name, const char *description,
                uint32_t seed); // \note Randomizes caseType, uniformCollection and features.
    virtual ~UniformCase(void);

    virtual void init(void);
    virtual void deinit(void);

    IterateResult iterate(void);

protected:
    // A basic uniform is a uniform (possibly struct or array member) whose type is a basic type (e.g. float, ivec4, sampler2d).
    struct BasicUniform
    {
        string name;
        glu::DataType type;
        bool isUsedInShader;
        VarValue finalValue; //!< The value we ultimately want to set for this uniform.

        string
            rootName; //!< If this is a member of a basic-typed array, rootName is the name of that array with "[0]" appended. Otherwise it equals name.
        int elemNdx;  //!< If this is a member of a basic-typed array, elemNdx is the index in that array. Otherwise -1.
        int rootSize; //!< If this is a member of a basic-typed array, rootSize is the size of that array. Otherwise 1.

        BasicUniform(const char *const name_, const glu::DataType type_, const bool isUsedInShader_,
                     const VarValue &finalValue_, const char *const rootName_ = nullptr, const int elemNdx_ = -1,
                     const int rootSize_ = 1)
            : name(name_)
            , type(type_)
            , isUsedInShader(isUsedInShader_)
            , finalValue(finalValue_)
            , rootName(rootName_ == nullptr ? name_ : rootName_)
            , elemNdx(elemNdx_)
            , rootSize(rootSize_)
        {
        }

        static vector<BasicUniform>::const_iterator findWithName(const vector<BasicUniform> &vec,
                                                                 const char *const name)
        {
            for (vector<BasicUniform>::const_iterator it = vec.begin(); it != vec.end(); it++)
            {
                if (it->name == name)
                    return it;
            }
            return vec.end();
        }
    };

    // Reference values for info that is expected to be reported by glGetActiveUniform().
    struct BasicUniformReportRef
    {
        string name;
        // \note minSize and maxSize are for arrays and can be distinct since implementations are allowed, but not required, to trim the inactive end indices of arrays.
        int minSize;
        int maxSize;
        glu::DataType type;
        bool isUsedInShader;

        BasicUniformReportRef(const char *const name_, const int minS, const int maxS, const glu::DataType type_,
                              const bool used)
            : name(name_)
            , minSize(minS)
            , maxSize(maxS)
            , type(type_)
            , isUsedInShader(used)
        {
            DE_ASSERT(minSize <= maxSize);
        }
        BasicUniformReportRef(const char *const name_, const glu::DataType type_, const bool used)
            : name(name_)
            , minSize(1)
            , maxSize(1)
            , type(type_)
            , isUsedInShader(used)
        {
        }
    };

    // Info that is actually reported by glGetActiveUniform().
    struct BasicUniformReportGL
    {
        string name;
        int nameLength;
        int size;
        glu::DataType type;

        int index;

        BasicUniformReportGL(const char *const name_, const int nameLength_, const int size_, const glu::DataType type_,
                             const int index_)
            : name(name_)
            , nameLength(nameLength_)
            , size(size_)
            , type(type_)
            , index(index_)
        {
        }

        static vector<BasicUniformReportGL>::const_iterator findWithName(const vector<BasicUniformReportGL> &vec,
                                                                         const char *const name)
        {
            for (vector<BasicUniformReportGL>::const_iterator it = vec.begin(); it != vec.end(); it++)
            {
                if (it->name == name)
                    return it;
            }
            return vec.end();
        }
    };

    // Query info with glGetActiveUniform() and check validity.
    bool getActiveUniforms(vector<BasicUniformReportGL> &dst, const vector<BasicUniformReportRef> &ref,
                           uint32_t programGL);
    // Get uniform values with glGetUniform*() and put to valuesDst. Uniforms that get -1 from glGetUniformLocation() get glu::TYPE_INVALID.
    bool getUniforms(vector<VarValue> &valuesDst, const vector<BasicUniform> &basicUniforms, uint32_t programGL);
    // Check that every uniform has the default (zero) value.
    bool checkUniformDefaultValues(const vector<VarValue> &values, const vector<BasicUniform> &basicUniforms);
    // Assign the basicUniforms[].finalValue values for uniforms. \note rnd parameter is for booleans (true can be any nonzero value).
    void assignUniforms(const vector<BasicUniform> &basicUniforms, uint32_t programGL, Random &rnd);
    // Compare the uniform values given in values (obtained with glGetUniform*()) with the basicUniform.finalValue values.
    bool compareUniformValues(const vector<VarValue> &values, const vector<BasicUniform> &basicUniforms);
    // Render and check that all pixels are white (i.e. all uniform comparisons passed).
    bool renderTest(const vector<BasicUniform> &basicUniforms, const ShaderProgram &program, Random &rnd);

    virtual bool test(const vector<BasicUniform> &basicUniforms,
                      const vector<BasicUniformReportRef> &basicUniformReportsRef, const ShaderProgram &program,
                      Random &rnd) = 0;

    const uint32_t m_features;
    const SharedPtr<const UniformCollection> m_uniformCollection;

private:
    static uint32_t randomFeatures(uint32_t seed);

    // Generates the basic uniforms, based on the uniform with name varName and type varType, in the same manner as are expected
    // to be returned by glGetActiveUniform(), e.g. generates a name like var[0] for arrays, and recursively generates struct member names.
    void generateBasicUniforms(vector<BasicUniform> &basicUniformsDst,
                               vector<BasicUniformReportRef> &basicUniformReportsDst, const glu::VarType &varType,
                               const char *varName, bool isParentActive, int &samplerUnitCounter, Random &rnd) const;

    void writeUniformDefinitions(std::ostringstream &dst) const;
    void writeUniformCompareExpr(std::ostringstream &dst, const BasicUniform &uniform) const;
    void writeUniformComparisons(std::ostringstream &dst, const vector<BasicUniform> &basicUniforms,
                                 const char *variableName) const;

    string generateVertexSource(const vector<BasicUniform> &basicUniforms) const;
    string generateFragmentSource(const vector<BasicUniform> &basicUniforms) const;

    void setupTexture(const VarValue &value);

    const CaseShaderType m_caseShaderType;

    vector<glu::Texture2D *> m_textures2d;
    vector<glu::TextureCube *> m_texturesCube;
    vector<uint32_t> m_filledTextureUnits;
};

uint32_t UniformCase::randomFeatures(const uint32_t seed)
{
    static const uint32_t arrayUsageChoices[]     = {0, FEATURE_ARRAYUSAGE_ONLY_MIDDLE_INDEX};
    static const uint32_t uniformFuncChoices[]    = {0, FEATURE_UNIFORMFUNC_VALUE};
    static const uint32_t arrayAssignChoices[]    = {0, FEATURE_ARRAYASSIGN_FULL, FEATURE_ARRAYASSIGN_BLOCKS_OF_TWO};
    static const uint32_t uniformUsageChoices[]   = {0, FEATURE_UNIFORMUSAGE_EVERY_OTHER};
    static const uint32_t booleanApiTypeChoices[] = {0, FEATURE_BOOLEANAPITYPE_INT};
    static const uint32_t uniformValueChoices[]   = {0, FEATURE_UNIFORMVALUE_ZERO};

    Random rnd(seed);

    uint32_t result = 0;

#define ARRAY_CHOICE(ARR) ((ARR)[rnd.getInt(0, DE_LENGTH_OF_ARRAY(ARR) - 1)])

    result |= ARRAY_CHOICE(arrayUsageChoices);
    result |= ARRAY_CHOICE(uniformFuncChoices);
    result |= ARRAY_CHOICE(arrayAssignChoices);
    result |= ARRAY_CHOICE(uniformUsageChoices);
    result |= ARRAY_CHOICE(booleanApiTypeChoices);
    result |= ARRAY_CHOICE(uniformValueChoices);

#undef ARRAY_CHOICE

    return result;
}

UniformCase::UniformCase(Context &context, const char *const name, const char *const description,
                         const CaseShaderType caseShaderType,
                         const SharedPtr<const UniformCollection> &uniformCollection, const uint32_t features)
    : TestCase(context, name, description)
    , CallLogWrapper(context.getRenderContext().getFunctions(), m_testCtx.getLog())
    , m_features(features)
    , m_uniformCollection(uniformCollection)
    , m_caseShaderType(caseShaderType)
{
}

UniformCase::UniformCase(Context &context, const char *name, const char *description, const uint32_t seed)
    : TestCase(context, name, description)
    , CallLogWrapper(context.getRenderContext().getFunctions(), m_testCtx.getLog())
    , m_features(randomFeatures(seed))
    , m_uniformCollection(UniformCollection::random(seed))
    , m_caseShaderType(randomCaseShaderType(seed))
{
}

void UniformCase::init(void)
{
    {
        const glw::Functions &funcs         = m_context.getRenderContext().getFunctions();
        const int numSamplerUniforms        = m_uniformCollection->getNumSamplers();
        const int vertexTexUnitsRequired    = m_caseShaderType != CASESHADERTYPE_FRAGMENT ? numSamplerUniforms : 0;
        const int fragmentTexUnitsRequired  = m_caseShaderType != CASESHADERTYPE_VERTEX ? numSamplerUniforms : 0;
        const int combinedTexUnitsRequired  = vertexTexUnitsRequired + fragmentTexUnitsRequired;
        const int vertexTexUnitsSupported   = getGLInt(funcs, GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
        const int fragmentTexUnitsSupported = getGLInt(funcs, GL_MAX_TEXTURE_IMAGE_UNITS);
        const int combinedTexUnitsSupported = getGLInt(funcs, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);

        DE_ASSERT(numSamplerUniforms <= MAX_NUM_SAMPLER_UNIFORMS);

        if (vertexTexUnitsRequired > vertexTexUnitsSupported)
            throw tcu::NotSupportedError(de::toString(vertexTexUnitsRequired) + " vertex texture units required, " +
                                         de::toString(vertexTexUnitsSupported) + " supported");
        if (fragmentTexUnitsRequired > fragmentTexUnitsSupported)
            throw tcu::NotSupportedError(de::toString(fragmentTexUnitsRequired) + " fragment texture units required, " +
                                         de::toString(fragmentTexUnitsSupported) + " supported");
        if (combinedTexUnitsRequired > combinedTexUnitsSupported)
            throw tcu::NotSupportedError(de::toString(combinedTexUnitsRequired) + " combined texture units required, " +
                                         de::toString(combinedTexUnitsSupported) + " supported");
    }

    enableLogging(true);
}

void UniformCase::deinit(void)
{
    for (int i = 0; i < (int)m_textures2d.size(); i++)
        delete m_textures2d[i];
    m_textures2d.clear();

    for (int i = 0; i < (int)m_texturesCube.size(); i++)
        delete m_texturesCube[i];
    m_texturesCube.clear();

    m_filledTextureUnits.clear();
}

UniformCase::~UniformCase(void)
{
    UniformCase::deinit();
}

void UniformCase::generateBasicUniforms(vector<BasicUniform> &basicUniformsDst,
                                        vector<BasicUniformReportRef> &basicUniformReportsDst,
                                        const glu::VarType &varType, const char *const varName,
                                        const bool isParentActive, int &samplerUnitCounter, Random &rnd) const
{
    if (varType.isBasicType())
    {
        const bool isActive =
            isParentActive && (m_features & FEATURE_UNIFORMUSAGE_EVERY_OTHER ? basicUniformsDst.size() % 2 == 0 : true);
        const glu::DataType type = varType.getBasicType();
        const VarValue value     = m_features & FEATURE_UNIFORMVALUE_ZERO ? generateZeroVarValue(type) :
                                   glu::isDataTypeSampler(type) ? generateRandomVarValue(type, rnd, samplerUnitCounter++) :
                                                                  generateRandomVarValue(varType.getBasicType(), rnd);

        basicUniformsDst.push_back(BasicUniform(varName, varType.getBasicType(), isActive, value));
        basicUniformReportsDst.push_back(BasicUniformReportRef(varName, varType.getBasicType(), isActive));
    }
    else if (varType.isArrayType())
    {
        const int size             = varType.getArraySize();
        const string arrayRootName = string("") + varName + "[0]";
        vector<bool> isElemActive;

        for (int elemNdx = 0; elemNdx < varType.getArraySize(); elemNdx++)
        {
            const string indexedName = string("") + varName + "[" + de::toString(elemNdx) + "]";
            const bool isCurElemActive =
                isParentActive &&
                (m_features & FEATURE_UNIFORMUSAGE_EVERY_OTHER ? basicUniformsDst.size() % 2 == 0 : true) &&
                (m_features & FEATURE_ARRAYUSAGE_ONLY_MIDDLE_INDEX ? elemNdx == size / 2 : true);

            isElemActive.push_back(isCurElemActive);

            if (varType.getElementType().isBasicType())
            {
                // \note We don't want separate entries in basicUniformReportsDst for elements of basic-type arrays.
                const glu::DataType elemBasicType = varType.getElementType().getBasicType();
                const VarValue value              = m_features & FEATURE_UNIFORMVALUE_ZERO ?
                                                        generateZeroVarValue(elemBasicType) :
                                                    glu::isDataTypeSampler(elemBasicType) ?
                                                        generateRandomVarValue(elemBasicType, rnd, samplerUnitCounter++) :
                                                        generateRandomVarValue(elemBasicType, rnd);

                basicUniformsDst.push_back(BasicUniform(indexedName.c_str(), elemBasicType, isCurElemActive, value,
                                                        arrayRootName.c_str(), elemNdx, size));
            }
            else
                generateBasicUniforms(basicUniformsDst, basicUniformReportsDst, varType.getElementType(),
                                      indexedName.c_str(), isCurElemActive, samplerUnitCounter, rnd);
        }

        if (varType.getElementType().isBasicType())
        {
            int minSize;
            for (minSize = varType.getArraySize(); minSize > 0 && !isElemActive[minSize - 1]; minSize--)
                ;

            basicUniformReportsDst.push_back(BasicUniformReportRef(arrayRootName.c_str(), minSize, size,
                                                                   varType.getElementType().getBasicType(),
                                                                   isParentActive && minSize > 0));
        }
    }
    else
    {
        DE_ASSERT(varType.isStructType());

        const StructType &structType = *varType.getStructPtr();

        for (int i = 0; i < structType.getNumMembers(); i++)
        {
            const glu::StructMember &member = structType.getMember(i);
            const string memberFullName     = string("") + varName + "." + member.getName();

            generateBasicUniforms(basicUniformsDst, basicUniformReportsDst, member.getType(), memberFullName.c_str(),
                                  isParentActive, samplerUnitCounter, rnd);
        }
    }
}

void UniformCase::writeUniformDefinitions(std::ostringstream &dst) const
{
    for (int i = 0; i < (int)m_uniformCollection->getNumStructTypes(); i++)
        dst << glu::declare(m_uniformCollection->getStructType(i)) << ";\n";

    for (int i = 0; i < (int)m_uniformCollection->getNumUniforms(); i++)
        dst << "uniform "
            << glu::declare(m_uniformCollection->getUniform(i).type, m_uniformCollection->getUniform(i).name.c_str())
            << ";\n";

    dst << "\n";

    {
        static const struct
        {
            dataTypePredicate requiringTypes[2];
            const char *definition;
        } compareFuncs[] = {
            {{glu::isDataTypeFloatOrVec, glu::isDataTypeMatrix},
             "mediump float compare_float    (mediump float a, mediump float b)  { return abs(a - b) < 0.05 ? 1.0 : "
             "0.0; }"},
            {{dataTypeEquals<glu::TYPE_FLOAT_VEC2>, dataTypeIsMatrixWithNRows<2>},
             "mediump float compare_vec2     (mediump vec2 a, mediump vec2 b)    { return compare_float(a.x, "
             "b.x)*compare_float(a.y, b.y); }"},
            {{dataTypeEquals<glu::TYPE_FLOAT_VEC3>, dataTypeIsMatrixWithNRows<3>},
             "mediump float compare_vec3     (mediump vec3 a, mediump vec3 b)    { return compare_float(a.x, "
             "b.x)*compare_float(a.y, b.y)*compare_float(a.z, b.z); }"},
            {{dataTypeEquals<glu::TYPE_FLOAT_VEC4>, dataTypeIsMatrixWithNRows<4>},
             "mediump float compare_vec4     (mediump vec4 a, mediump vec4 b)    { return compare_float(a.x, "
             "b.x)*compare_float(a.y, b.y)*compare_float(a.z, b.z)*compare_float(a.w, b.w); }"},
            {{dataTypeEquals<glu::TYPE_FLOAT_MAT2>, dataTypeEquals<glu::TYPE_INVALID>},
             "mediump float compare_mat2     (mediump mat2 a, mediump mat2 b)    { return compare_vec2(a[0], "
             "b[0])*compare_vec2(a[1], b[1]); }"},
            {{dataTypeEquals<glu::TYPE_FLOAT_MAT3>, dataTypeEquals<glu::TYPE_INVALID>},
             "mediump float compare_mat3     (mediump mat3 a, mediump mat3 b)    { return compare_vec3(a[0], "
             "b[0])*compare_vec3(a[1], b[1])*compare_vec3(a[2], b[2]); }"},
            {{dataTypeEquals<glu::TYPE_FLOAT_MAT4>, dataTypeEquals<glu::TYPE_INVALID>},
             "mediump float compare_mat4     (mediump mat4 a, mediump mat4 b)    { return compare_vec4(a[0], "
             "b[0])*compare_vec4(a[1], b[1])*compare_vec4(a[2], b[2])*compare_vec4(a[3], b[3]); }"},
            {{dataTypeEquals<glu::TYPE_INT>, dataTypeEquals<glu::TYPE_INVALID>},
             "mediump float compare_int      (mediump int a, mediump int b)      { return a == b ? 1.0 : 0.0; }"},
            {{dataTypeEquals<glu::TYPE_INT_VEC2>, dataTypeEquals<glu::TYPE_INVALID>},
             "mediump float compare_ivec2    (mediump ivec2 a, mediump ivec2 b)  { return a == b ? 1.0 : 0.0; }"},
            {{dataTypeEquals<glu::TYPE_INT_VEC3>, dataTypeEquals<glu::TYPE_INVALID>},
             "mediump float compare_ivec3    (mediump ivec3 a, mediump ivec3 b)  { return a == b ? 1.0 : 0.0; }"},
            {{dataTypeEquals<glu::TYPE_INT_VEC4>, dataTypeEquals<glu::TYPE_INVALID>},
             "mediump float compare_ivec4    (mediump ivec4 a, mediump ivec4 b)  { return a == b ? 1.0 : 0.0; }"},
            {{dataTypeEquals<glu::TYPE_BOOL>, dataTypeEquals<glu::TYPE_INVALID>},
             "mediump float compare_bool     (bool a, bool b)                    { return a == b ? 1.0 : 0.0; }"},
            {{dataTypeEquals<glu::TYPE_BOOL_VEC2>, dataTypeEquals<glu::TYPE_INVALID>},
             "mediump float compare_bvec2    (bvec2 a, bvec2 b)                  { return a == b ? 1.0 : 0.0; }"},
            {{dataTypeEquals<glu::TYPE_BOOL_VEC3>, dataTypeEquals<glu::TYPE_INVALID>},
             "mediump float compare_bvec3    (bvec3 a, bvec3 b)                  { return a == b ? 1.0 : 0.0; }"},
            {{dataTypeEquals<glu::TYPE_BOOL_VEC4>, dataTypeEquals<glu::TYPE_INVALID>},
             "mediump float compare_bvec4    (bvec4 a, bvec4 b)                  { return a == b ? 1.0 : 0.0; }"}};

        const bool containsSamplers = !m_uniformCollection->getSamplerTypes().empty();

        for (int compFuncNdx = 0; compFuncNdx < DE_LENGTH_OF_ARRAY(compareFuncs); compFuncNdx++)
        {
            const dataTypePredicate(&typeReq)[2] = compareFuncs[compFuncNdx].requiringTypes;
            const bool containsTypeSampler =
                containsSamplers && (typeReq[0](glu::TYPE_FLOAT_VEC4) || typeReq[1](glu::TYPE_FLOAT_VEC4));

            if (containsTypeSampler || m_uniformCollection->containsMatchingBasicType(typeReq[0]) ||
                m_uniformCollection->containsMatchingBasicType(typeReq[1]))
                dst << compareFuncs[compFuncNdx].definition << "\n";
        }
    }
}

void UniformCase::writeUniformCompareExpr(std::ostringstream &dst, const BasicUniform &uniform) const
{
    if (glu::isDataTypeSampler(uniform.type))
    {
        dst << "compare_vec4(" << (uniform.type == glu::TYPE_SAMPLER_2D ? "texture2D" : "textureCube") << "("
            << uniform.name << ", vec" << getSamplerNumLookupDimensions(uniform.type) << "(0.0))";
    }
    else
        dst << "compare_" << glu::getDataTypeName(uniform.type) << "(" << uniform.name;

    dst << ", " << shaderVarValueStr(uniform.finalValue) << ")";
}

void UniformCase::writeUniformComparisons(std::ostringstream &dst, const vector<BasicUniform> &basicUniforms,
                                          const char *const variableName) const
{
    for (int i = 0; i < (int)basicUniforms.size(); i++)
    {
        const BasicUniform &unif = basicUniforms[i];

        if (unif.isUsedInShader)
        {
            dst << "\t" << variableName << " *= ";
            writeUniformCompareExpr(dst, basicUniforms[i]);
            dst << ";\n";
        }
        else
            dst << "\t// UNUSED: " << basicUniforms[i].name << "\n";
    }
}

string UniformCase::generateVertexSource(const vector<BasicUniform> &basicUniforms) const
{
    const bool isVertexCase = m_caseShaderType == CASESHADERTYPE_VERTEX || m_caseShaderType == CASESHADERTYPE_BOTH;
    std::ostringstream result;

    result << "attribute highp vec4 a_position;\n"
              "varying mediump float v_vtxOut;\n"
              "\n";

    if (isVertexCase)
        writeUniformDefinitions(result);

    result << "\n"
              "void main (void)\n"
              "{\n"
              "    gl_Position = a_position;\n"
              "    v_vtxOut = 1.0;\n";

    if (isVertexCase)
        writeUniformComparisons(result, basicUniforms, "v_vtxOut");

    result << "}\n";

    return result.str();
}

string UniformCase::generateFragmentSource(const vector<BasicUniform> &basicUniforms) const
{
    const bool isFragmentCase = m_caseShaderType == CASESHADERTYPE_FRAGMENT || m_caseShaderType == CASESHADERTYPE_BOTH;
    std::ostringstream result;

    result << "varying mediump float v_vtxOut;\n"
              "\n";

    if (isFragmentCase)
        writeUniformDefinitions(result);

    result << "\n"
              "void main (void)\n"
              "{\n"
              "    mediump float result = v_vtxOut;\n";

    if (isFragmentCase)
        writeUniformComparisons(result, basicUniforms, "result");

    result << "    gl_FragColor = vec4(result, result, result, 1.0);\n"
              "}\n";

    return result.str();
}

void UniformCase::setupTexture(const VarValue &value)
{
    enableLogging(false);

    const int width       = 32;
    const int height      = 32;
    const tcu::Vec4 color = vec4FromPtr(&value.val.samplerV.fillColor[0]);

    if (value.type == glu::TYPE_SAMPLER_2D)
    {
        glu::Texture2D *texture =
            new glu::Texture2D(m_context.getRenderContext(), GL_RGBA, GL_UNSIGNED_BYTE, width, height);
        tcu::Texture2D &refTexture = texture->getRefTexture();
        m_textures2d.push_back(texture);

        refTexture.allocLevel(0);
        fillWithColor(refTexture.getLevel(0), color);

        GLU_CHECK_CALL(glActiveTexture(GL_TEXTURE0 + value.val.samplerV.unit));
        m_filledTextureUnits.push_back(value.val.samplerV.unit);
        texture->upload();
        GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    }
    else if (value.type == glu::TYPE_SAMPLER_CUBE)
    {
        DE_ASSERT(width == height);

        glu::TextureCube *texture =
            new glu::TextureCube(m_context.getRenderContext(), GL_RGBA, GL_UNSIGNED_BYTE, width);
        tcu::TextureCube &refTexture = texture->getRefTexture();
        m_texturesCube.push_back(texture);

        for (int face = 0; face < (int)tcu::CUBEFACE_LAST; face++)
        {
            refTexture.allocLevel((tcu::CubeFace)face, 0);
            fillWithColor(refTexture.getLevelFace(0, (tcu::CubeFace)face), color);
        }

        GLU_CHECK_CALL(glActiveTexture(GL_TEXTURE0 + value.val.samplerV.unit));
        m_filledTextureUnits.push_back(value.val.samplerV.unit);
        texture->upload();

        GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GLU_CHECK_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    }
    else
        DE_ASSERT(false);

    enableLogging(true);
}

bool UniformCase::getActiveUniforms(vector<BasicUniformReportGL> &basicUniformReportsDst,
                                    const vector<BasicUniformReportRef> &basicUniformReportsRef,
                                    const uint32_t programGL)
{
    TestLog &log               = m_testCtx.getLog();
    GLint numActiveUniforms    = 0;
    GLint uniformMaxNameLength = 0;
    vector<char> nameBuffer;
    bool success = true;

    GLU_CHECK_CALL(glGetProgramiv(programGL, GL_ACTIVE_UNIFORMS, &numActiveUniforms));
    log << TestLog::Message << "// Number of active uniforms reported: " << numActiveUniforms << TestLog::EndMessage;
    GLU_CHECK_CALL(glGetProgramiv(programGL, GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniformMaxNameLength));
    log << TestLog::Message << "// Maximum uniform name length reported: " << uniformMaxNameLength
        << TestLog::EndMessage;
    nameBuffer.resize(uniformMaxNameLength);

    for (int unifNdx = 0; unifNdx < numActiveUniforms; unifNdx++)
    {
        GLsizei reportedNameLength = 0;
        GLint reportedSize         = -1;
        GLenum reportedTypeGL      = GL_NONE;

        GLU_CHECK_CALL(glGetActiveUniform(programGL, (GLuint)unifNdx, (GLsizei)uniformMaxNameLength,
                                          &reportedNameLength, &reportedSize, &reportedTypeGL, &nameBuffer[0]));

        const glu::DataType reportedType = glu::getDataTypeFromGLType(reportedTypeGL);
        const string reportedNameStr(&nameBuffer[0]);

        TCU_CHECK_MSG(reportedType != glu::TYPE_LAST, "Invalid uniform type");

        log << TestLog::Message << "// Got name = " << reportedNameStr << ", name length = " << reportedNameLength
            << ", size = " << reportedSize << ", type = " << glu::getDataTypeName(reportedType) << TestLog::EndMessage;

        if ((GLsizei)reportedNameStr.length() != reportedNameLength)
        {
            log << TestLog::Message << "// FAILURE: wrong name length reported, should be " << reportedNameStr.length()
                << TestLog::EndMessage;
            success = false;
        }

        if (!deStringBeginsWith(reportedNameStr.c_str(), "gl_")) // Ignore built-in uniforms.
        {
            int referenceNdx;
            for (referenceNdx = 0; referenceNdx < (int)basicUniformReportsRef.size(); referenceNdx++)
            {
                if (basicUniformReportsRef[referenceNdx].name == reportedNameStr)
                    break;
            }

            if (referenceNdx >= (int)basicUniformReportsRef.size())
            {
                log << TestLog::Message << "// FAILURE: invalid non-built-in uniform name reported"
                    << TestLog::EndMessage;
                success = false;
            }
            else
            {
                const BasicUniformReportRef &reference = basicUniformReportsRef[referenceNdx];

                DE_ASSERT(reference.type != glu::TYPE_LAST);
                DE_ASSERT(reference.minSize >= 1 || (reference.minSize == 0 && !reference.isUsedInShader));
                DE_ASSERT(reference.minSize <= reference.maxSize);

                if (BasicUniformReportGL::findWithName(basicUniformReportsDst, reportedNameStr.c_str()) !=
                    basicUniformReportsDst.end())
                {
                    log << TestLog::Message << "// FAILURE: same uniform name reported twice" << TestLog::EndMessage;
                    success = false;
                }

                basicUniformReportsDst.push_back(BasicUniformReportGL(reportedNameStr.c_str(), reportedNameLength,
                                                                      reportedSize, reportedType, unifNdx));

                if (reportedType != reference.type)
                {
                    log << TestLog::Message << "// FAILURE: wrong type reported, should be "
                        << glu::getDataTypeName(reference.type) << TestLog::EndMessage;
                    success = false;
                }
                if (reportedSize < reference.minSize || reportedSize > reference.maxSize)
                {
                    log << TestLog::Message << "// FAILURE: wrong size reported, should be "
                        << (reference.minSize == reference.maxSize ?
                                de::toString(reference.minSize) :
                                "in the range [" + de::toString(reference.minSize) + ", " +
                                    de::toString(reference.maxSize) + "]")
                        << TestLog::EndMessage;

                    success = false;
                }
            }
        }
    }

    for (int i = 0; i < (int)basicUniformReportsRef.size(); i++)
    {
        const BasicUniformReportRef &expected = basicUniformReportsRef[i];
        if (expected.isUsedInShader &&
            BasicUniformReportGL::findWithName(basicUniformReportsDst, expected.name.c_str()) ==
                basicUniformReportsDst.end())
        {
            log << TestLog::Message << "// FAILURE: uniform with name " << expected.name << " was not reported by GL"
                << TestLog::EndMessage;
            success = false;
        }
    }

    return success;
}

bool UniformCase::getUniforms(vector<VarValue> &valuesDst, const vector<BasicUniform> &basicUniforms,
                              const uint32_t programGL)
{
    TestLog &log = m_testCtx.getLog();
    bool success = true;

    for (int unifNdx = 0; unifNdx < (int)basicUniforms.size(); unifNdx++)
    {
        const BasicUniform &uniform = basicUniforms[unifNdx];
        const string queryName      = m_features & FEATURE_ARRAY_FIRST_ELEM_NAME_NO_INDEX && uniform.elemNdx == 0 ?
                                          beforeLast(uniform.name, '[') :
                                          uniform.name;
        const int location          = glGetUniformLocation(programGL, queryName.c_str());
        const int size              = glu::getDataTypeScalarSize(uniform.type);
        VarValue value;

        deMemset(&value, 0xcd, sizeof(value)); // Initialize to known garbage.

        if (location == -1)
        {
            value.type = glu::TYPE_INVALID;
            valuesDst.push_back(value);
            if (uniform.isUsedInShader)
            {
                log << TestLog::Message << "// FAILURE: " << uniform.name << " was used in shader, but has location -1"
                    << TestLog::EndMessage;
                success = false;
            }
            continue;
        }

        value.type = uniform.type;

        DE_STATIC_ASSERT(sizeof(GLint) == sizeof(value.val.intV[0]));
        DE_STATIC_ASSERT(sizeof(GLfloat) == sizeof(value.val.floatV[0]));

        if (glu::isDataTypeFloatOrVec(uniform.type) || glu::isDataTypeMatrix(uniform.type))
            GLU_CHECK_CALL(glGetUniformfv(programGL, location, &value.val.floatV[0]));
        else if (glu::isDataTypeIntOrIVec(uniform.type))
            GLU_CHECK_CALL(glGetUniformiv(programGL, location, &value.val.intV[0]));
        else if (glu::isDataTypeBoolOrBVec(uniform.type))
        {
            if (m_features & FEATURE_BOOLEANAPITYPE_INT)
            {
                GLU_CHECK_CALL(glGetUniformiv(programGL, location, &value.val.intV[0]));
                for (int i = 0; i < size; i++)
                    value.val.boolV[i] = value.val.intV[i] != 0;
            }
            else // Default: use float.
            {
                GLU_CHECK_CALL(glGetUniformfv(programGL, location, &value.val.floatV[0]));
                for (int i = 0; i < size; i++)
                    value.val.boolV[i] = value.val.floatV[i] != 0.0f;
            }
        }
        else if (glu::isDataTypeSampler(uniform.type))
        {
            GLint unit = -1;
            GLU_CHECK_CALL(glGetUniformiv(programGL, location, &unit));
            value.val.samplerV.unit = unit;
        }
        else
            DE_ASSERT(false);

        valuesDst.push_back(value);

        log << TestLog::Message << "// Got " << uniform.name << " value " << apiVarValueStr(value)
            << TestLog::EndMessage;
    }

    return success;
}

bool UniformCase::checkUniformDefaultValues(const vector<VarValue> &values, const vector<BasicUniform> &basicUniforms)
{
    TestLog &log = m_testCtx.getLog();
    bool success = true;

    DE_ASSERT(values.size() == basicUniforms.size());

    for (int unifNdx = 0; unifNdx < (int)basicUniforms.size(); unifNdx++)
    {
        const BasicUniform &uniform = basicUniforms[unifNdx];
        const VarValue &unifValue   = values[unifNdx];
        const int valSize           = glu::getDataTypeScalarSize(uniform.type);

        log << TestLog::Message << "// Checking uniform " << uniform.name << TestLog::EndMessage;

        if (unifValue.type == glu::TYPE_INVALID) // This happens when glGetUniformLocation() returned -1.
            continue;

#define CHECK_UNIFORM(VAR_VALUE_MEMBER, ZERO)                                                                      \
    do                                                                                                             \
    {                                                                                                              \
        for (int i = 0; i < valSize; i++)                                                                          \
        {                                                                                                          \
            if (unifValue.val.VAR_VALUE_MEMBER[i] != (ZERO))                                                       \
            {                                                                                                      \
                log << TestLog::Message << "// FAILURE: uniform " << uniform.name << " has non-zero initial value" \
                    << TestLog::EndMessage;                                                                        \
                success = false;                                                                                   \
            }                                                                                                      \
        }                                                                                                          \
    } while (false)

        if (glu::isDataTypeFloatOrVec(uniform.type) || glu::isDataTypeMatrix(uniform.type))
            CHECK_UNIFORM(floatV, 0.0f);
        else if (glu::isDataTypeIntOrIVec(uniform.type))
            CHECK_UNIFORM(intV, 0);
        else if (glu::isDataTypeBoolOrBVec(uniform.type))
            CHECK_UNIFORM(boolV, false);
        else if (glu::isDataTypeSampler(uniform.type))
        {
            if (unifValue.val.samplerV.unit != 0)
            {
                log << TestLog::Message << "// FAILURE: uniform " << uniform.name << " has non-zero initial value"
                    << TestLog::EndMessage;
                success = false;
            }
        }
        else
            DE_ASSERT(false);

#undef CHECK_UNIFORM
    }

    return success;
}

void UniformCase::assignUniforms(const vector<BasicUniform> &basicUniforms, uint32_t programGL, Random &rnd)
{
    TestLog &log                    = m_testCtx.getLog();
    const glu::DataType boolApiType = m_features & FEATURE_BOOLEANAPITYPE_INT ? glu::TYPE_INT : glu::TYPE_FLOAT;

    for (int unifNdx = 0; unifNdx < (int)basicUniforms.size(); unifNdx++)
    {
        const BasicUniform &uniform = basicUniforms[unifNdx];
        const bool isArrayMember    = uniform.elemNdx >= 0;
        const string queryName      = m_features & FEATURE_ARRAY_FIRST_ELEM_NAME_NO_INDEX && uniform.elemNdx == 0 ?
                                          beforeLast(uniform.name, '[') :
                                          uniform.name;
        const int numValuesToAssign =
            !isArrayMember                                 ? 1 :
            m_features & FEATURE_ARRAYASSIGN_FULL          ? (uniform.elemNdx == 0 ? uniform.rootSize : 0) :
            m_features & FEATURE_ARRAYASSIGN_BLOCKS_OF_TWO ? (uniform.elemNdx % 2 == 0 ? 2 : 0) :
                                                             /* Default: assign array elements separately */ 1;

        DE_ASSERT(numValuesToAssign >= 0);
        DE_ASSERT(numValuesToAssign == 1 || isArrayMember);

        if (numValuesToAssign == 0)
        {
            log << TestLog::Message << "// Uniform " << uniform.name
                << " is covered by another glUniform*v() call to the same array" << TestLog::EndMessage;
            continue;
        }

        const int location = glGetUniformLocation(programGL, queryName.c_str());
        const int typeSize = glu::getDataTypeScalarSize(uniform.type);
        const bool assignByValue =
            m_features & FEATURE_UNIFORMFUNC_VALUE && !glu::isDataTypeMatrix(uniform.type) && numValuesToAssign == 1;
        vector<VarValue> valuesToAssign;

        for (int i = 0; i < numValuesToAssign; i++)
        {
            const string curName =
                isArrayMember ? beforeLast(uniform.rootName, '[') + "[" + de::toString(uniform.elemNdx + i) + "]" :
                                uniform.name;
            VarValue unifValue;

            if (isArrayMember)
            {
                const vector<BasicUniform>::const_iterator elemUnif =
                    BasicUniform::findWithName(basicUniforms, curName.c_str());
                if (elemUnif == basicUniforms.end())
                    continue;
                unifValue = elemUnif->finalValue;
            }
            else
                unifValue = uniform.finalValue;

            const VarValue apiValue = glu::isDataTypeBoolOrBVec(unifValue.type) ?
                                          getRandomBoolRepresentation(unifValue, boolApiType, rnd) :
                                      glu::isDataTypeSampler(unifValue.type) ? getSamplerUnitValue(unifValue) :
                                                                               unifValue;

            valuesToAssign.push_back(apiValue);

            if (glu::isDataTypeBoolOrBVec(uniform.type))
                log << TestLog::Message << "// Using type " << glu::getDataTypeName(boolApiType)
                    << " to set boolean value " << apiVarValueStr(unifValue) << " for " << curName
                    << TestLog::EndMessage;
            else if (glu::isDataTypeSampler(uniform.type))
                log << TestLog::Message << "// Texture for the sampler uniform " << curName
                    << " will be filled with color " << apiVarValueStr(getSamplerFillValue(uniform.finalValue))
                    << TestLog::EndMessage;
        }

        DE_ASSERT(!valuesToAssign.empty());

        if (glu::isDataTypeFloatOrVec(valuesToAssign[0].type))
        {
            if (assignByValue)
            {
                const float *const ptr = &valuesToAssign[0].val.floatV[0];

                switch (typeSize)
                {
                case 1:
                    GLU_CHECK_CALL(glUniform1f(location, ptr[0]));
                    break;
                case 2:
                    GLU_CHECK_CALL(glUniform2f(location, ptr[0], ptr[1]));
                    break;
                case 3:
                    GLU_CHECK_CALL(glUniform3f(location, ptr[0], ptr[1], ptr[2]));
                    break;
                case 4:
                    GLU_CHECK_CALL(glUniform4f(location, ptr[0], ptr[1], ptr[2], ptr[3]));
                    break;
                default:
                    DE_ASSERT(false);
                }
            }
            else
            {
                vector<float> buffer(valuesToAssign.size() * typeSize);
                for (int i = 0; i < (int)buffer.size(); i++)
                    buffer[i] = valuesToAssign[i / typeSize].val.floatV[i % typeSize];

                DE_STATIC_ASSERT(sizeof(GLfloat) == sizeof(buffer[0]));
                switch (typeSize)
                {
                case 1:
                    GLU_CHECK_CALL(glUniform1fv(location, (GLsizei)valuesToAssign.size(), &buffer[0]));
                    break;
                case 2:
                    GLU_CHECK_CALL(glUniform2fv(location, (GLsizei)valuesToAssign.size(), &buffer[0]));
                    break;
                case 3:
                    GLU_CHECK_CALL(glUniform3fv(location, (GLsizei)valuesToAssign.size(), &buffer[0]));
                    break;
                case 4:
                    GLU_CHECK_CALL(glUniform4fv(location, (GLsizei)valuesToAssign.size(), &buffer[0]));
                    break;
                default:
                    DE_ASSERT(false);
                }
            }
        }
        else if (glu::isDataTypeMatrix(valuesToAssign[0].type))
        {
            DE_ASSERT(!assignByValue);

            vector<float> buffer(valuesToAssign.size() * typeSize);
            for (int i = 0; i < (int)buffer.size(); i++)
                buffer[i] = valuesToAssign[i / typeSize].val.floatV[i % typeSize];

            switch (uniform.type)
            {
            case glu::TYPE_FLOAT_MAT2:
                GLU_CHECK_CALL(glUniformMatrix2fv(location, (GLsizei)valuesToAssign.size(), GL_FALSE, &buffer[0]));
                break;
            case glu::TYPE_FLOAT_MAT3:
                GLU_CHECK_CALL(glUniformMatrix3fv(location, (GLsizei)valuesToAssign.size(), GL_FALSE, &buffer[0]));
                break;
            case glu::TYPE_FLOAT_MAT4:
                GLU_CHECK_CALL(glUniformMatrix4fv(location, (GLsizei)valuesToAssign.size(), GL_FALSE, &buffer[0]));
                break;
            default:
                DE_ASSERT(false);
            }
        }
        else if (glu::isDataTypeIntOrIVec(valuesToAssign[0].type))
        {
            if (assignByValue)
            {
                const int32_t *const ptr = &valuesToAssign[0].val.intV[0];

                switch (typeSize)
                {
                case 1:
                    GLU_CHECK_CALL(glUniform1i(location, ptr[0]));
                    break;
                case 2:
                    GLU_CHECK_CALL(glUniform2i(location, ptr[0], ptr[1]));
                    break;
                case 3:
                    GLU_CHECK_CALL(glUniform3i(location, ptr[0], ptr[1], ptr[2]));
                    break;
                case 4:
                    GLU_CHECK_CALL(glUniform4i(location, ptr[0], ptr[1], ptr[2], ptr[3]));
                    break;
                default:
                    DE_ASSERT(false);
                }
            }
            else
            {
                vector<int32_t> buffer(valuesToAssign.size() * typeSize);
                for (int i = 0; i < (int)buffer.size(); i++)
                    buffer[i] = valuesToAssign[i / typeSize].val.intV[i % typeSize];

                DE_STATIC_ASSERT(sizeof(GLint) == sizeof(buffer[0]));
                switch (typeSize)
                {
                case 1:
                    GLU_CHECK_CALL(glUniform1iv(location, (GLsizei)valuesToAssign.size(), &buffer[0]));
                    break;
                case 2:
                    GLU_CHECK_CALL(glUniform2iv(location, (GLsizei)valuesToAssign.size(), &buffer[0]));
                    break;
                case 3:
                    GLU_CHECK_CALL(glUniform3iv(location, (GLsizei)valuesToAssign.size(), &buffer[0]));
                    break;
                case 4:
                    GLU_CHECK_CALL(glUniform4iv(location, (GLsizei)valuesToAssign.size(), &buffer[0]));
                    break;
                default:
                    DE_ASSERT(false);
                }
            }
        }
        else if (glu::isDataTypeSampler(valuesToAssign[0].type))
        {
            if (assignByValue)
                GLU_CHECK_CALL(glUniform1i(location, uniform.finalValue.val.samplerV.unit));
            else
            {
                const GLint unit = uniform.finalValue.val.samplerV.unit;
                GLU_CHECK_CALL(glUniform1iv(location, (GLsizei)valuesToAssign.size(), &unit));
            }
        }
        else
            DE_ASSERT(false);
    }
}

bool UniformCase::compareUniformValues(const vector<VarValue> &values, const vector<BasicUniform> &basicUniforms)
{
    TestLog &log = m_testCtx.getLog();
    bool success = true;

    for (int unifNdx = 0; unifNdx < (int)basicUniforms.size(); unifNdx++)
    {
        const BasicUniform &uniform = basicUniforms[unifNdx];
        const VarValue &unifValue   = values[unifNdx];

        log << TestLog::Message << "// Checking uniform " << uniform.name << TestLog::EndMessage;

        if (unifValue.type == glu::TYPE_INVALID) // This happens when glGetUniformLocation() returned -1.
            continue;

        if (!apiVarValueEquals(unifValue, uniform.finalValue))
        {
            log << TestLog::Message << "// FAILURE: value obtained with glGetUniform*() for uniform " << uniform.name
                << " differs from value set with glUniform*()" << TestLog::EndMessage;
            success = false;
        }
    }

    return success;
}

bool UniformCase::renderTest(const vector<BasicUniform> &basicUniforms, const ShaderProgram &program, Random &rnd)
{
    TestLog &log                          = m_testCtx.getLog();
    const tcu::RenderTarget &renderTarget = m_context.getRenderTarget();
    const int viewportW                   = de::min(renderTarget.getWidth(), MAX_RENDER_WIDTH);
    const int viewportH                   = de::min(renderTarget.getHeight(), MAX_RENDER_HEIGHT);
    const int viewportX                   = rnd.getInt(0, renderTarget.getWidth() - viewportW);
    const int viewportY                   = rnd.getInt(0, renderTarget.getHeight() - viewportH);
    tcu::Surface renderedImg(viewportW, viewportH);

    // Assert that no two samplers of different types have the same texture unit - this is an error in GL.
    for (int i = 0; i < (int)basicUniforms.size(); i++)
    {
        if (glu::isDataTypeSampler(basicUniforms[i].type))
        {
            for (int j = 0; j < i; j++)
            {
                if (glu::isDataTypeSampler(basicUniforms[j].type) && basicUniforms[i].type != basicUniforms[j].type)
                    DE_ASSERT(basicUniforms[i].finalValue.val.samplerV.unit !=
                              basicUniforms[j].finalValue.val.samplerV.unit);
            }
        }
    }

    for (int i = 0; i < (int)basicUniforms.size(); i++)
    {
        if (glu::isDataTypeSampler(basicUniforms[i].type) &&
            std::find(m_filledTextureUnits.begin(), m_filledTextureUnits.end(),
                      basicUniforms[i].finalValue.val.samplerV.unit) == m_filledTextureUnits.end())
        {
            log << TestLog::Message << "// Filling texture at unit " << apiVarValueStr(basicUniforms[i].finalValue)
                << " with color " << shaderVarValueStr(basicUniforms[i].finalValue) << TestLog::EndMessage;
            setupTexture(basicUniforms[i].finalValue);
        }
    }

    GLU_CHECK_CALL(glViewport(viewportX, viewportY, viewportW, viewportH));

    {
        static const float position[]   = {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, +1.0f, 0.0f, 1.0f,
                                           +1.0f, -1.0f, 0.0f, 1.0f, +1.0f, +1.0f, 0.0f, 1.0f};
        static const uint16_t indices[] = {0, 1, 2, 2, 1, 3};

        const int posLoc = glGetAttribLocation(program.getProgram(), "a_position");

        glEnableVertexAttribArray(posLoc);
        glVertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, &position[0]);

        GLU_CHECK_CALL(glDrawElements(GL_TRIANGLES, DE_LENGTH_OF_ARRAY(indices), GL_UNSIGNED_SHORT, &indices[0]));
    }

    glu::readPixels(m_context.getRenderContext(), viewportX, viewportY, renderedImg.getAccess());

    int numFailedPixels = 0;
    for (int y = 0; y < renderedImg.getHeight(); y++)
    {
        for (int x = 0; x < renderedImg.getWidth(); x++)
        {
            if (renderedImg.getPixel(x, y) != tcu::RGBA::white())
                numFailedPixels += 1;
        }
    }

    if (numFailedPixels > 0)
    {
        log << TestLog::Image("RenderedImage", "Rendered image", renderedImg);
        log << TestLog::Message << "FAILURE: image comparison failed, got " << numFailedPixels << " non-white pixels"
            << TestLog::EndMessage;
        return false;
    }
    else
    {
        log << TestLog::Message << "Success: got all-white pixels (all uniforms have correct values)"
            << TestLog::EndMessage;
        return true;
    }
}

UniformCase::IterateResult UniformCase::iterate(void)
{
    Random rnd(deStringHash(getName()) ^ (uint32_t)m_context.getTestContext().getCommandLine().getBaseSeed());
    TestLog &log = m_testCtx.getLog();
    vector<BasicUniform> basicUniforms;
    vector<BasicUniformReportRef> basicUniformReportsRef;

    {
        int samplerUnitCounter = 0;
        for (int i = 0; i < (int)m_uniformCollection->getNumUniforms(); i++)
            generateBasicUniforms(basicUniforms, basicUniformReportsRef, m_uniformCollection->getUniform(i).type,
                                  m_uniformCollection->getUniform(i).name.c_str(), true, samplerUnitCounter, rnd);
    }

    const string vertexSource   = generateVertexSource(basicUniforms);
    const string fragmentSource = generateFragmentSource(basicUniforms);
    const ShaderProgram program(m_context.getRenderContext(), glu::makeVtxFragSources(vertexSource, fragmentSource));

    log << program;

    if (!program.isOk())
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Compile failed");
        return STOP;
    }

    GLU_CHECK_CALL(glUseProgram(program.getProgram()));

    const bool success = test(basicUniforms, basicUniformReportsRef, program, rnd);
    m_testCtx.setTestResult(success ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL, success ? "Passed" : "Failed");

    return STOP;
}

class UniformInfoQueryCase : public UniformCase
{
public:
    UniformInfoQueryCase(Context &context, const char *name, const char *description, CaseShaderType shaderType,
                         const SharedPtr<const UniformCollection> &uniformCollection, uint32_t additionalFeatures = 0);
    bool test(const vector<BasicUniform> &basicUniforms, const vector<BasicUniformReportRef> &basicUniformReportsRef,
              const ShaderProgram &program, Random &rnd);
};

UniformInfoQueryCase::UniformInfoQueryCase(Context &context, const char *const name, const char *const description,
                                           const CaseShaderType shaderType,
                                           const SharedPtr<const UniformCollection> &uniformCollection,
                                           const uint32_t additionalFeatures)
    : UniformCase(context, name, description, shaderType, uniformCollection, additionalFeatures)
{
}

bool UniformInfoQueryCase::test(const vector<BasicUniform> &basicUniforms,
                                const vector<BasicUniformReportRef> &basicUniformReportsRef,
                                const ShaderProgram &program, Random &rnd)
{
    DE_UNREF(basicUniforms);
    DE_UNREF(rnd);

    const uint32_t programGL = program.getProgram();
    TestLog &log             = m_testCtx.getLog();
    vector<BasicUniformReportGL> basicUniformReportsUniform;

    const ScopedLogSection section(log, "InfoGetActiveUniform",
                                   "Uniform information queries with glGetActiveUniform()");
    const bool success = getActiveUniforms(basicUniformReportsUniform, basicUniformReportsRef, programGL);

    if (!success)
        return false;

    return true;
}

class UniformValueCase : public UniformCase
{
public:
    enum ValueToCheck
    {
        VALUETOCHECK_INITIAL = 0, //!< Verify the initial values of the uniforms (i.e. check that they're zero).
        VALUETOCHECK_ASSIGNED,    //!< Assign values to uniforms with glUniform*(), and check those.

        VALUETOCHECK_LAST
    };
    enum CheckMethod
    {
        CHECKMETHOD_GET_UNIFORM = 0, //!< Check values with glGetUniform*().
        CHECKMETHOD_RENDER,          //!< Check values by rendering with the value-checking shader.

        CHECKMETHOD_LAST
    };
    enum AssignMethod
    {
        ASSIGNMETHOD_POINTER = 0,
        ASSIGNMETHOD_VALUE,

        ASSIGNMETHOD_LAST
    };

    UniformValueCase(Context &context, const char *name, const char *description, CaseShaderType shaderType,
                     const SharedPtr<const UniformCollection> &uniformCollection, ValueToCheck valueToCheck,
                     CheckMethod checkMethod, AssignMethod assignMethod, uint32_t additionalFeatures = 0);

    bool test(const vector<BasicUniform> &basicUniforms, const vector<BasicUniformReportRef> &basicUniformReportsRef,
              const ShaderProgram &program, Random &rnd);

    static const char *getValueToCheckName(ValueToCheck valueToCheck);
    static const char *getValueToCheckDescription(ValueToCheck valueToCheck);
    static const char *getCheckMethodName(CheckMethod checkMethod);
    static const char *getCheckMethodDescription(CheckMethod checkMethod);
    static const char *getAssignMethodName(AssignMethod checkMethod);
    static const char *getAssignMethodDescription(AssignMethod checkMethod);

private:
    const ValueToCheck m_valueToCheck;
    const CheckMethod m_checkMethod;
};

const char *UniformValueCase::getValueToCheckName(const ValueToCheck valueToCheck)
{
    switch (valueToCheck)
    {
    case VALUETOCHECK_INITIAL:
        return "initial";
    case VALUETOCHECK_ASSIGNED:
        return "assigned";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

const char *UniformValueCase::getValueToCheckDescription(const ValueToCheck valueToCheck)
{
    switch (valueToCheck)
    {
    case VALUETOCHECK_INITIAL:
        return "Check initial uniform values (zeros)";
    case VALUETOCHECK_ASSIGNED:
        return "Check assigned uniform values";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

const char *UniformValueCase::getCheckMethodName(const CheckMethod checkMethod)
{
    switch (checkMethod)
    {
    case CHECKMETHOD_GET_UNIFORM:
        return "get_uniform";
    case CHECKMETHOD_RENDER:
        return "render";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

const char *UniformValueCase::getCheckMethodDescription(const CheckMethod checkMethod)
{
    switch (checkMethod)
    {
    case CHECKMETHOD_GET_UNIFORM:
        return "Verify values with glGetUniform*()";
    case CHECKMETHOD_RENDER:
        return "Verify values by rendering";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

const char *UniformValueCase::getAssignMethodName(const AssignMethod assignMethod)
{
    switch (assignMethod)
    {
    case ASSIGNMETHOD_POINTER:
        return "by_pointer";
    case ASSIGNMETHOD_VALUE:
        return "by_value";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

const char *UniformValueCase::getAssignMethodDescription(const AssignMethod assignMethod)
{
    switch (assignMethod)
    {
    case ASSIGNMETHOD_POINTER:
        return "Assign values by-pointer";
    case ASSIGNMETHOD_VALUE:
        return "Assign values by-value";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

UniformValueCase::UniformValueCase(Context &context, const char *const name, const char *const description,
                                   const CaseShaderType shaderType,
                                   const SharedPtr<const UniformCollection> &uniformCollection,
                                   const ValueToCheck valueToCheck, const CheckMethod checkMethod,
                                   const AssignMethod assignMethod, const uint32_t additionalFeatures)
    : UniformCase(context, name, description, shaderType, uniformCollection,
                  (valueToCheck == VALUETOCHECK_INITIAL ? FEATURE_UNIFORMVALUE_ZERO : 0) |
                      (assignMethod == ASSIGNMETHOD_VALUE ? FEATURE_UNIFORMFUNC_VALUE : 0) | additionalFeatures)
    , m_valueToCheck(valueToCheck)
    , m_checkMethod(checkMethod)
{
    DE_ASSERT(!(assignMethod == ASSIGNMETHOD_LAST && valueToCheck == VALUETOCHECK_ASSIGNED));
}

bool UniformValueCase::test(const vector<BasicUniform> &basicUniforms,
                            const vector<BasicUniformReportRef> &basicUniformReportsRef, const ShaderProgram &program,
                            Random &rnd)
{
    DE_UNREF(basicUniformReportsRef);

    const uint32_t programGL = program.getProgram();
    TestLog &log             = m_testCtx.getLog();

    if (m_valueToCheck == VALUETOCHECK_ASSIGNED)
    {
        const ScopedLogSection section(log, "UniformAssign", "Uniform value assignments");
        assignUniforms(basicUniforms, programGL, rnd);
    }
    else
        DE_ASSERT(m_valueToCheck == VALUETOCHECK_INITIAL);

    if (m_checkMethod == CHECKMETHOD_GET_UNIFORM)
    {
        vector<VarValue> values;

        {
            const ScopedLogSection section(log, "GetUniforms", "Uniform value query");
            const bool success = getUniforms(values, basicUniforms, program.getProgram());

            if (!success)
                return false;
        }

        if (m_valueToCheck == VALUETOCHECK_ASSIGNED)
        {
            const ScopedLogSection section(log, "ValueCheck",
                                           "Verify that the reported values match the assigned values");
            const bool success = compareUniformValues(values, basicUniforms);

            if (!success)
                return false;
        }
        else
        {
            DE_ASSERT(m_valueToCheck == VALUETOCHECK_INITIAL);
            const ScopedLogSection section(log, "ValueCheck",
                                           "Verify that the uniforms have correct initial values (zeros)");
            const bool success = checkUniformDefaultValues(values, basicUniforms);

            if (!success)
                return false;
        }
    }
    else
    {
        DE_ASSERT(m_checkMethod == CHECKMETHOD_RENDER);

        const ScopedLogSection section(log, "RenderTest", "Render test");
        const bool success = renderTest(basicUniforms, program, rnd);

        if (!success)
            return false;
    }

    return true;
}

class RandomUniformCase : public UniformCase
{
public:
    RandomUniformCase(Context &m_context, const char *name, const char *description, uint32_t seed);

    bool test(const vector<BasicUniform> &basicUniforms, const vector<BasicUniformReportRef> &basicUniformReportsRef,
              const ShaderProgram &program, Random &rnd);
};

RandomUniformCase::RandomUniformCase(Context &context, const char *const name, const char *const description,
                                     const uint32_t seed)
    : UniformCase(context, name, description, seed ^ (uint32_t)context.getTestContext().getCommandLine().getBaseSeed())
{
}

bool RandomUniformCase::test(const vector<BasicUniform> &basicUniforms,
                             const vector<BasicUniformReportRef> &basicUniformReportsRef, const ShaderProgram &program,
                             Random &rnd)
{
    // \note Different sampler types may not be bound to same unit when rendering.
    const bool renderingPossible =
        (m_features & FEATURE_UNIFORMVALUE_ZERO) == 0 || !m_uniformCollection->containsSeveralSamplerTypes();

    bool performGetActiveUniforms               = rnd.getBool();
    const bool performGetUniforms               = rnd.getBool();
    const bool performCheckUniformDefaultValues = performGetUniforms && rnd.getBool();
    const bool performAssignUniforms            = rnd.getBool();
    const bool performCompareUniformValues      = performGetUniforms && performAssignUniforms && rnd.getBool();
    const bool performRenderTest                = renderingPossible && performAssignUniforms && rnd.getBool();
    const uint32_t programGL                    = program.getProgram();
    TestLog &log                                = m_testCtx.getLog();

    if (!(performGetActiveUniforms || performGetUniforms || performCheckUniformDefaultValues || performAssignUniforms ||
          performCompareUniformValues || performRenderTest))
        performGetActiveUniforms = true; // Do something at least.

#define PERFORM_AND_CHECK(CALL, SECTION_NAME, SECTION_DESCRIPTION)                  \
    do                                                                              \
    {                                                                               \
        const ScopedLogSection section(log, (SECTION_NAME), (SECTION_DESCRIPTION)); \
        const bool success = (CALL);                                                \
        if (!success)                                                               \
            return false;                                                           \
    } while (false)

    if (performGetActiveUniforms)
    {
        vector<BasicUniformReportGL> reportsUniform;
        PERFORM_AND_CHECK(getActiveUniforms(reportsUniform, basicUniformReportsRef, programGL), "InfoGetActiveUniform",
                          "Uniform information queries with glGetActiveUniform()");
    }

    {
        vector<VarValue> uniformDefaultValues;

        if (performGetUniforms)
            PERFORM_AND_CHECK(getUniforms(uniformDefaultValues, basicUniforms, programGL), "GetUniformDefaults",
                              "Uniform default value query");
        if (performCheckUniformDefaultValues)
            PERFORM_AND_CHECK(checkUniformDefaultValues(uniformDefaultValues, basicUniforms), "DefaultValueCheck",
                              "Verify that the uniforms have correct initial values (zeros)");
    }

    {
        vector<VarValue> uniformValues;

        if (performAssignUniforms)
        {
            const ScopedLogSection section(log, "UniformAssign", "Uniform value assignments");
            assignUniforms(basicUniforms, programGL, rnd);
        }
        if (performCompareUniformValues)
        {
            PERFORM_AND_CHECK(getUniforms(uniformValues, basicUniforms, programGL), "GetUniforms",
                              "Uniform value query");
            PERFORM_AND_CHECK(compareUniformValues(uniformValues, basicUniforms), "ValueCheck",
                              "Verify that the reported values match the assigned values");
        }
    }

    if (performRenderTest)
        PERFORM_AND_CHECK(renderTest(basicUniforms, program, rnd), "RenderTest", "Render test");

#undef PERFORM_AND_CHECK

    return true;
}

UniformApiTests::UniformApiTests(Context &context) : TestCaseGroup(context, "uniform_api", "Uniform API Tests")
{
}

UniformApiTests::~UniformApiTests(void)
{
}

namespace
{

// \note Although this is only used in UniformApiTest::init, it needs to be defined here as it's used as a template argument.
struct UniformCollectionCase
{
    string namePrefix;
    SharedPtr<const UniformCollection> uniformCollection;

    UniformCollectionCase(const char *const name, const UniformCollection *uniformCollection_)
        : namePrefix(name ? name + string("_") : "")
        , uniformCollection(uniformCollection_)
    {
    }
};

} // namespace

void UniformApiTests::init(void)
{
    // Generate sets of UniformCollections that are used by several cases.

    enum
    {
        UNIFORMCOLLECTIONS_BASIC = 0,
        UNIFORMCOLLECTIONS_BASIC_ARRAY,
        UNIFORMCOLLECTIONS_BASIC_STRUCT,
        UNIFORMCOLLECTIONS_STRUCT_IN_ARRAY,
        UNIFORMCOLLECTIONS_ARRAY_IN_STRUCT,
        UNIFORMCOLLECTIONS_NESTED_STRUCTS_ARRAYS,
        UNIFORMCOLLECTIONS_MULTIPLE_BASIC,
        UNIFORMCOLLECTIONS_MULTIPLE_BASIC_ARRAY,
        UNIFORMCOLLECTIONS_MULTIPLE_NESTED_STRUCTS_ARRAYS,

        UNIFORMCOLLECTIONS_LAST
    };

    struct UniformCollectionGroup
    {
        string name;
        vector<UniformCollectionCase> cases;
    } defaultUniformCollections[UNIFORMCOLLECTIONS_LAST];

    defaultUniformCollections[UNIFORMCOLLECTIONS_BASIC].name                 = "basic";
    defaultUniformCollections[UNIFORMCOLLECTIONS_BASIC_ARRAY].name           = "basic_array";
    defaultUniformCollections[UNIFORMCOLLECTIONS_BASIC_STRUCT].name          = "basic_struct";
    defaultUniformCollections[UNIFORMCOLLECTIONS_STRUCT_IN_ARRAY].name       = "struct_in_array";
    defaultUniformCollections[UNIFORMCOLLECTIONS_ARRAY_IN_STRUCT].name       = "array_in_struct";
    defaultUniformCollections[UNIFORMCOLLECTIONS_NESTED_STRUCTS_ARRAYS].name = "nested_structs_arrays";
    defaultUniformCollections[UNIFORMCOLLECTIONS_MULTIPLE_BASIC].name        = "multiple_basic";
    defaultUniformCollections[UNIFORMCOLLECTIONS_MULTIPLE_BASIC_ARRAY].name  = "multiple_basic_array";
    defaultUniformCollections[UNIFORMCOLLECTIONS_MULTIPLE_NESTED_STRUCTS_ARRAYS].name =
        "multiple_nested_structs_arrays";

    for (int dataTypeNdx = 0; dataTypeNdx < DE_LENGTH_OF_ARRAY(s_testDataTypes); dataTypeNdx++)
    {
        const glu::DataType dataType = s_testDataTypes[dataTypeNdx];
        const char *const typeName   = glu::getDataTypeName(dataType);

        defaultUniformCollections[UNIFORMCOLLECTIONS_BASIC].cases.push_back(
            UniformCollectionCase(typeName, UniformCollection::basic(dataType)));

        if (glu::isDataTypeScalar(dataType) ||
            (glu::isDataTypeVector(dataType) && glu::getDataTypeScalarSize(dataType) == 4) ||
            dataType == glu::TYPE_FLOAT_MAT4 || dataType == glu::TYPE_SAMPLER_2D)
            defaultUniformCollections[UNIFORMCOLLECTIONS_BASIC_ARRAY].cases.push_back(
                UniformCollectionCase(typeName, UniformCollection::basicArray(dataType)));

        if (glu::isDataTypeScalar(dataType) || dataType == glu::TYPE_FLOAT_MAT4 || dataType == glu::TYPE_SAMPLER_2D)
        {
            const glu::DataType secondDataType = glu::isDataTypeScalar(dataType) ? glu::getDataTypeVector(dataType, 4) :
                                                 dataType == glu::TYPE_FLOAT_MAT4 ? glu::TYPE_FLOAT_MAT2 :
                                                 dataType == glu::TYPE_SAMPLER_2D ? glu::TYPE_SAMPLER_CUBE :
                                                                                    glu::TYPE_LAST;
            DE_ASSERT(secondDataType != glu::TYPE_LAST);
            const char *const secondTypeName = glu::getDataTypeName(secondDataType);
            const string name                = string("") + typeName + "_" + secondTypeName;

            defaultUniformCollections[UNIFORMCOLLECTIONS_BASIC_STRUCT].cases.push_back(
                UniformCollectionCase(name.c_str(), UniformCollection::basicStruct(dataType, secondDataType, false)));
            defaultUniformCollections[UNIFORMCOLLECTIONS_ARRAY_IN_STRUCT].cases.push_back(
                UniformCollectionCase(name.c_str(), UniformCollection::basicStruct(dataType, secondDataType, true)));
            defaultUniformCollections[UNIFORMCOLLECTIONS_STRUCT_IN_ARRAY].cases.push_back(
                UniformCollectionCase(name.c_str(), UniformCollection::structInArray(dataType, secondDataType, false)));
            defaultUniformCollections[UNIFORMCOLLECTIONS_NESTED_STRUCTS_ARRAYS].cases.push_back(
                UniformCollectionCase(name.c_str(), UniformCollection::nestedArraysStructs(dataType, secondDataType)));
        }
    }
    defaultUniformCollections[UNIFORMCOLLECTIONS_MULTIPLE_BASIC].cases.push_back(
        UniformCollectionCase(nullptr, UniformCollection::multipleBasic()));
    defaultUniformCollections[UNIFORMCOLLECTIONS_MULTIPLE_BASIC_ARRAY].cases.push_back(
        UniformCollectionCase(nullptr, UniformCollection::multipleBasicArray()));
    defaultUniformCollections[UNIFORMCOLLECTIONS_MULTIPLE_NESTED_STRUCTS_ARRAYS].cases.push_back(
        UniformCollectionCase(nullptr, UniformCollection::multipleNestedArraysStructs()));

    // Info-query cases (check info returned by e.g. glGetActiveUniforms()).

    {
        TestCaseGroup *const infoQueryGroup = new TestCaseGroup(m_context, "info_query", "Test glGetActiveUniform()");
        addChild(infoQueryGroup);

        for (int collectionGroupNdx = 0; collectionGroupNdx < (int)UNIFORMCOLLECTIONS_LAST; collectionGroupNdx++)
        {
            const UniformCollectionGroup &collectionGroup = defaultUniformCollections[collectionGroupNdx];
            TestCaseGroup *const collectionTestGroup = new TestCaseGroup(m_context, collectionGroup.name.c_str(), "");
            infoQueryGroup->addChild(collectionTestGroup);

            for (int collectionNdx = 0; collectionNdx < (int)collectionGroup.cases.size(); collectionNdx++)
            {
                const UniformCollectionCase &collectionCase = collectionGroup.cases[collectionNdx];

                for (int shaderType = 0; shaderType < (int)CASESHADERTYPE_LAST; shaderType++)
                {
                    const string name = collectionCase.namePrefix + getCaseShaderTypeName((CaseShaderType)shaderType);
                    const SharedPtr<const UniformCollection> &uniformCollection = collectionCase.uniformCollection;

                    collectionTestGroup->addChild(new UniformInfoQueryCase(
                        m_context, name.c_str(), "", (CaseShaderType)shaderType, uniformCollection));
                }
            }
        }

        // Info-querying cases when unused uniforms are present.

        {
            TestCaseGroup *const unusedUniformsGroup =
                new TestCaseGroup(m_context, "unused_uniforms", "Test with unused uniforms");
            infoQueryGroup->addChild(unusedUniformsGroup);

            const UniformCollectionGroup &collectionGroup =
                defaultUniformCollections[UNIFORMCOLLECTIONS_ARRAY_IN_STRUCT];

            for (int collectionNdx = 0; collectionNdx < (int)collectionGroup.cases.size(); collectionNdx++)
            {
                const UniformCollectionCase &collectionCase                 = collectionGroup.cases[collectionNdx];
                const string collName                                       = collectionCase.namePrefix;
                const SharedPtr<const UniformCollection> &uniformCollection = collectionCase.uniformCollection;

                for (int shaderType = 0; shaderType < (int)CASESHADERTYPE_LAST; shaderType++)
                {
                    const string name = collName + getCaseShaderTypeName((CaseShaderType)shaderType);
                    unusedUniformsGroup->addChild(new UniformInfoQueryCase(
                        m_context, name.c_str(), "", (CaseShaderType)shaderType, uniformCollection,
                        UniformCase::FEATURE_UNIFORMUSAGE_EVERY_OTHER |
                            UniformCase::FEATURE_ARRAYUSAGE_ONLY_MIDDLE_INDEX));
                }
            }
        }
    }

    // Cases testing uniform values.

    {
        TestCaseGroup *const valueGroup = new TestCaseGroup(m_context, "value", "Uniform value tests");
        addChild(valueGroup);

        // Cases checking uniforms' initial values (all must be zeros), with glGetUniform*() or by rendering.

        {
            TestCaseGroup *const initialValuesGroup = new TestCaseGroup(
                m_context, UniformValueCase::getValueToCheckName(UniformValueCase::VALUETOCHECK_INITIAL),
                UniformValueCase::getValueToCheckDescription(UniformValueCase::VALUETOCHECK_INITIAL));
            valueGroup->addChild(initialValuesGroup);

            for (int checkMethodI = 0; checkMethodI < (int)UniformValueCase::CHECKMETHOD_LAST; checkMethodI++)
            {
                const UniformValueCase::CheckMethod checkMethod = (UniformValueCase::CheckMethod)checkMethodI;
                TestCaseGroup *const checkMethodGroup =
                    new TestCaseGroup(m_context, UniformValueCase::getCheckMethodName(checkMethod),
                                      UniformValueCase::getCheckMethodDescription(checkMethod));
                initialValuesGroup->addChild(checkMethodGroup);

                for (int collectionGroupNdx = 0; collectionGroupNdx < (int)UNIFORMCOLLECTIONS_LAST;
                     collectionGroupNdx++)
                {
                    const UniformCollectionGroup &collectionGroup = defaultUniformCollections[collectionGroupNdx];
                    TestCaseGroup *const collectionTestGroup =
                        new TestCaseGroup(m_context, collectionGroup.name.c_str(), "");
                    checkMethodGroup->addChild(collectionTestGroup);

                    for (int collectionNdx = 0; collectionNdx < (int)collectionGroup.cases.size(); collectionNdx++)
                    {
                        const UniformCollectionCase &collectionCase = collectionGroup.cases[collectionNdx];
                        const string collName                       = collectionCase.namePrefix;
                        const SharedPtr<const UniformCollection> &uniformCollection = collectionCase.uniformCollection;
                        const bool containsBooleans =
                            uniformCollection->containsMatchingBasicType(glu::isDataTypeBoolOrBVec);
                        const bool varyBoolApiType = checkMethod == UniformValueCase::CHECKMETHOD_GET_UNIFORM &&
                                                     containsBooleans &&
                                                     (collectionGroupNdx == UNIFORMCOLLECTIONS_BASIC ||
                                                      collectionGroupNdx == UNIFORMCOLLECTIONS_BASIC_ARRAY);
                        const int numBoolVariations = varyBoolApiType ? 2 : 1;

                        if (checkMethod == UniformValueCase::CHECKMETHOD_RENDER &&
                            uniformCollection->containsSeveralSamplerTypes())
                            continue; // \note Samplers' initial API values (i.e. their texture units) are 0, and no two samplers of different types shall have same unit when rendering.

                        for (int booleanTypeI = 0; booleanTypeI < numBoolVariations; booleanTypeI++)
                        {
                            const uint32_t booleanTypeFeat =
                                booleanTypeI == 1 ? UniformCase::FEATURE_BOOLEANAPITYPE_INT : 0;
                            const char *const booleanTypeName = booleanTypeI == 1 ? "int" : "float";
                            const string nameWithApiType =
                                varyBoolApiType ? collName + "api_" + booleanTypeName + "_" : collName;

                            for (int shaderType = 0; shaderType < (int)CASESHADERTYPE_LAST; shaderType++)
                            {
                                const string name = nameWithApiType + getCaseShaderTypeName((CaseShaderType)shaderType);
                                collectionTestGroup->addChild(new UniformValueCase(
                                    m_context, name.c_str(), "", (CaseShaderType)shaderType, uniformCollection,
                                    UniformValueCase::VALUETOCHECK_INITIAL, checkMethod,
                                    UniformValueCase::ASSIGNMETHOD_LAST, booleanTypeFeat));
                            }
                        }
                    }
                }
            }
        }

        // Cases that first assign values to each uniform, then check the values with glGetUniform*() or by rendering.

        {
            TestCaseGroup *const assignedValuesGroup = new TestCaseGroup(
                m_context, UniformValueCase::getValueToCheckName(UniformValueCase::VALUETOCHECK_ASSIGNED),
                UniformValueCase::getValueToCheckDescription(UniformValueCase::VALUETOCHECK_ASSIGNED));
            valueGroup->addChild(assignedValuesGroup);

            for (int assignMethodI = 0; assignMethodI < (int)UniformValueCase::ASSIGNMETHOD_LAST; assignMethodI++)
            {
                const UniformValueCase::AssignMethod assignMethod = (UniformValueCase::AssignMethod)assignMethodI;
                TestCaseGroup *const assignMethodGroup =
                    new TestCaseGroup(m_context, UniformValueCase::getAssignMethodName(assignMethod),
                                      UniformValueCase::getAssignMethodDescription(assignMethod));
                assignedValuesGroup->addChild(assignMethodGroup);

                for (int checkMethodI = 0; checkMethodI < (int)UniformValueCase::CHECKMETHOD_LAST; checkMethodI++)
                {
                    const UniformValueCase::CheckMethod checkMethod = (UniformValueCase::CheckMethod)checkMethodI;
                    TestCaseGroup *const checkMethodGroup =
                        new TestCaseGroup(m_context, UniformValueCase::getCheckMethodName(checkMethod),
                                          UniformValueCase::getCheckMethodDescription(checkMethod));
                    assignMethodGroup->addChild(checkMethodGroup);

                    for (int collectionGroupNdx = 0; collectionGroupNdx < (int)UNIFORMCOLLECTIONS_LAST;
                         collectionGroupNdx++)
                    {
                        const int numArrayFirstElemNameCases =
                            checkMethod == UniformValueCase::CHECKMETHOD_GET_UNIFORM &&
                                    collectionGroupNdx == UNIFORMCOLLECTIONS_BASIC_ARRAY ?
                                2 :
                                1;

                        for (int referToFirstArrayElemWithoutIndexI = 0;
                             referToFirstArrayElemWithoutIndexI < numArrayFirstElemNameCases;
                             referToFirstArrayElemWithoutIndexI++)
                        {
                            const UniformCollectionGroup &collectionGroup =
                                defaultUniformCollections[collectionGroupNdx];
                            const string collectionGroupName =
                                collectionGroup.name +
                                (referToFirstArrayElemWithoutIndexI == 0 ? "" : "_first_elem_without_brackets");
                            TestCaseGroup *collectionTestGroup = nullptr;

                            for (int collectionNdx = 0; collectionNdx < (int)collectionGroup.cases.size();
                                 collectionNdx++)
                            {
                                const UniformCollectionCase &collectionCase = collectionGroup.cases[collectionNdx];
                                const string collName                       = collectionCase.namePrefix;
                                const SharedPtr<const UniformCollection> &uniformCollection =
                                    collectionCase.uniformCollection;
                                const bool containsBooleans =
                                    uniformCollection->containsMatchingBasicType(glu::isDataTypeBoolOrBVec);
                                const bool varyBoolApiType = checkMethod == UniformValueCase::CHECKMETHOD_GET_UNIFORM &&
                                                             containsBooleans &&
                                                             (collectionGroupNdx == UNIFORMCOLLECTIONS_BASIC ||
                                                              collectionGroupNdx == UNIFORMCOLLECTIONS_BASIC_ARRAY);
                                const int numBoolVariations = varyBoolApiType ? 2 : 1;
                                const bool containsMatrices =
                                    uniformCollection->containsMatchingBasicType(glu::isDataTypeMatrix);

                                if (containsMatrices && assignMethod != UniformValueCase::ASSIGNMETHOD_POINTER)
                                    continue;

                                for (int booleanTypeI = 0; booleanTypeI < numBoolVariations; booleanTypeI++)
                                {
                                    const uint32_t booleanTypeFeat =
                                        booleanTypeI == 1 ? UniformCase::FEATURE_BOOLEANAPITYPE_INT : 0;
                                    const char *const booleanTypeName = booleanTypeI == 1 ? "int" : "float";
                                    const string nameWithBoolType =
                                        varyBoolApiType ? collName + "api_" + booleanTypeName + "_" : collName;
                                    const string nameWithMatrixType = nameWithBoolType;

                                    for (int shaderType = 0; shaderType < (int)CASESHADERTYPE_LAST; shaderType++)
                                    {
                                        const string name =
                                            nameWithMatrixType + getCaseShaderTypeName((CaseShaderType)shaderType);
                                        const uint32_t arrayFirstElemNameNoIndexFeat =
                                            referToFirstArrayElemWithoutIndexI == 0 ?
                                                0 :
                                                UniformCase::FEATURE_ARRAY_FIRST_ELEM_NAME_NO_INDEX;

                                        // skip empty groups by creating groups on demand
                                        if (!collectionTestGroup)
                                        {
                                            collectionTestGroup =
                                                new TestCaseGroup(m_context, collectionGroupName.c_str(), "");
                                            checkMethodGroup->addChild(collectionTestGroup);
                                        }

                                        collectionTestGroup->addChild(new UniformValueCase(
                                            m_context, name.c_str(), "", (CaseShaderType)shaderType, uniformCollection,
                                            UniformValueCase::VALUETOCHECK_ASSIGNED, checkMethod, assignMethod,
                                            booleanTypeFeat | arrayFirstElemNameNoIndexFeat));
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Cases assign multiple basic-array elements with one glUniform*v() (i.e. the count parameter is bigger than 1).

            {
                static const struct
                {
                    UniformCase::Feature arrayAssignMode;
                    const char *name;
                    const char *description;
                } arrayAssignGroups[] = {{UniformCase::FEATURE_ARRAYASSIGN_FULL, "basic_array_assign_full",
                                          "Assign entire basic-type arrays per glUniform*v() call"},
                                         {UniformCase::FEATURE_ARRAYASSIGN_BLOCKS_OF_TWO, "basic_array_assign_partial",
                                          "Assign two elements of a basic-type array per glUniform*v() call"}};

                for (int arrayAssignGroupNdx = 0; arrayAssignGroupNdx < DE_LENGTH_OF_ARRAY(arrayAssignGroups);
                     arrayAssignGroupNdx++)
                {
                    UniformCase::Feature arrayAssignMode = arrayAssignGroups[arrayAssignGroupNdx].arrayAssignMode;
                    const char *const groupName          = arrayAssignGroups[arrayAssignGroupNdx].name;
                    const char *const groupDesc          = arrayAssignGroups[arrayAssignGroupNdx].description;

                    TestCaseGroup *const curArrayAssignGroup = new TestCaseGroup(m_context, groupName, groupDesc);
                    assignedValuesGroup->addChild(curArrayAssignGroup);

                    static const int basicArrayCollectionGroups[] = {UNIFORMCOLLECTIONS_BASIC_ARRAY,
                                                                     UNIFORMCOLLECTIONS_ARRAY_IN_STRUCT,
                                                                     UNIFORMCOLLECTIONS_MULTIPLE_BASIC_ARRAY};

                    for (int collectionGroupNdx = 0;
                         collectionGroupNdx < DE_LENGTH_OF_ARRAY(basicArrayCollectionGroups); collectionGroupNdx++)
                    {
                        const UniformCollectionGroup &collectionGroup =
                            defaultUniformCollections[basicArrayCollectionGroups[collectionGroupNdx]];
                        TestCaseGroup *const collectionTestGroup =
                            new TestCaseGroup(m_context, collectionGroup.name.c_str(), "");
                        curArrayAssignGroup->addChild(collectionTestGroup);

                        for (int collectionNdx = 0; collectionNdx < (int)collectionGroup.cases.size(); collectionNdx++)
                        {
                            const UniformCollectionCase &collectionCase = collectionGroup.cases[collectionNdx];
                            const string collName                       = collectionCase.namePrefix;
                            const SharedPtr<const UniformCollection> &uniformCollection =
                                collectionCase.uniformCollection;

                            for (int shaderType = 0; shaderType < (int)CASESHADERTYPE_LAST; shaderType++)
                            {
                                const string name = collName + getCaseShaderTypeName((CaseShaderType)shaderType);
                                collectionTestGroup->addChild(new UniformValueCase(
                                    m_context, name.c_str(), "", (CaseShaderType)shaderType, uniformCollection,
                                    UniformValueCase::VALUETOCHECK_ASSIGNED, UniformValueCase::CHECKMETHOD_GET_UNIFORM,
                                    UniformValueCase::ASSIGNMETHOD_POINTER, arrayAssignMode));
                            }
                        }
                    }
                }
            }

            // Value checking cases when unused uniforms are present.

            {
                TestCaseGroup *const unusedUniformsGroup =
                    new TestCaseGroup(m_context, "unused_uniforms", "Test with unused uniforms");
                assignedValuesGroup->addChild(unusedUniformsGroup);

                const UniformCollectionGroup &collectionGroup =
                    defaultUniformCollections[UNIFORMCOLLECTIONS_ARRAY_IN_STRUCT];

                for (int collectionNdx = 0; collectionNdx < (int)collectionGroup.cases.size(); collectionNdx++)
                {
                    const UniformCollectionCase &collectionCase                 = collectionGroup.cases[collectionNdx];
                    const string collName                                       = collectionCase.namePrefix;
                    const SharedPtr<const UniformCollection> &uniformCollection = collectionCase.uniformCollection;

                    for (int shaderType = 0; shaderType < (int)CASESHADERTYPE_LAST; shaderType++)
                    {
                        const string name = collName + getCaseShaderTypeName((CaseShaderType)shaderType);
                        unusedUniformsGroup->addChild(new UniformValueCase(
                            m_context, name.c_str(), "", (CaseShaderType)shaderType, uniformCollection,
                            UniformValueCase::VALUETOCHECK_ASSIGNED, UniformValueCase::CHECKMETHOD_GET_UNIFORM,
                            UniformValueCase::ASSIGNMETHOD_POINTER,
                            UniformCase::FEATURE_ARRAYUSAGE_ONLY_MIDDLE_INDEX |
                                UniformCase::FEATURE_UNIFORMUSAGE_EVERY_OTHER));
                    }
                }
            }
        }
    }

    // Random cases.

    {
        const int numRandomCases         = 100;
        TestCaseGroup *const randomGroup = new TestCaseGroup(m_context, "random", "Random cases");
        addChild(randomGroup);

        for (int ndx = 0; ndx < numRandomCases; ndx++)
            randomGroup->addChild(new RandomUniformCase(m_context, de::toString(ndx).c_str(), "", (uint32_t)ndx));
    }
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
