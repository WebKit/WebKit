/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
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
 * \brief Transform feedback tests.
 *//*--------------------------------------------------------------------*/

#include "es3fTransformFeedbackTests.hpp"
#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "tcuImageCompare.hpp"
#include "tcuVector.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuRenderTarget.hpp"
#include "gluShaderUtil.hpp"
#include "gluVarType.hpp"
#include "gluVarTypeUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "gluObjectWrapper.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deMemory.h"
#include "deString.h"

#include <set>
#include <map>
#include <algorithm>

using std::set;
using std::string;
using std::vector;

using std::map;
using std::set;

using tcu::TestLog;

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace TransformFeedback
{

enum
{
    VIEWPORT_WIDTH  = 128,
    VIEWPORT_HEIGHT = 128,
    BUFFER_GUARD_MULTIPLIER =
        2 //!< stride*BUFFER_GUARD_MULTIPLIER bytes are added to the end of tf buffer and used to check for overruns.
};

enum Interpolation
{
    INTERPOLATION_SMOOTH = 0,
    INTERPOLATION_FLAT,
    INTERPOLATION_CENTROID,

    INTERPOLATION_LAST
};

static const char *getInterpolationName(Interpolation interp)
{
    switch (interp)
    {
    case INTERPOLATION_SMOOTH:
        return "smooth";
    case INTERPOLATION_FLAT:
        return "flat";
    case INTERPOLATION_CENTROID:
        return "centroid";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

struct Varying
{
    Varying(const char *name_, const glu::VarType &type_, Interpolation interp_)
        : name(name_)
        , type(type_)
        , interpolation(interp_)
    {
    }

    std::string name;            //!< Variable name.
    glu::VarType type;           //!< Variable type.
    Interpolation interpolation; //!< Interpolation mode (smooth, flat, centroid).
};

struct VaryingNameEquals
{
    VaryingNameEquals(const std::string &name_) : name(name_)
    {
    }
    bool operator()(const Varying &var) const
    {
        return var.name == name;
    }

    std::string name;
};

struct Attribute
{
    Attribute(const std::string &name_, const glu::VarType &type_, int offset_)
        : name(name_)
        , type(type_)
        , offset(offset_)
    {
    }

    std::string name;
    glu::VarType type;
    int offset;
};

struct AttributeNameEquals
{
    AttributeNameEquals(const std::string &name_) : name(name_)
    {
    }
    bool operator()(const Attribute &attr) const
    {
        return attr.name == name;
    }

    std::string name;
};

struct Output
{
    Output(void) : bufferNdx(0), offset(0)
    {
    }

    std::string name;
    glu::VarType type;
    int bufferNdx;
    int offset;
    vector<const Attribute *> inputs;
};

struct DrawCall
{
    DrawCall(int numElements_, bool tfEnabled_) : numElements(numElements_), transformFeedbackEnabled(tfEnabled_)
    {
    }

    DrawCall(void) : numElements(0), transformFeedbackEnabled(false)
    {
    }

    int numElements;
    bool transformFeedbackEnabled;
};

std::ostream &operator<<(std::ostream &str, const DrawCall &call)
{
    return str << "(" << call.numElements << ", " << (call.transformFeedbackEnabled ? "resumed" : "paused") << ")";
}

class ProgramSpec
{
public:
    ProgramSpec(void);
    ~ProgramSpec(void);

    glu::StructType *createStruct(const char *name);
    void addVarying(const char *name, const glu::VarType &type, Interpolation interp);
    void addTransformFeedbackVarying(const char *name);

    const vector<glu::StructType *> &getStructs(void) const
    {
        return m_structs;
    }
    const vector<Varying> &getVaryings(void) const
    {
        return m_varyings;
    }
    const vector<string> &getTransformFeedbackVaryings(void) const
    {
        return m_transformFeedbackVaryings;
    }
    bool isPointSizeUsed(void) const;

private:
    ProgramSpec(const ProgramSpec &other);
    ProgramSpec &operator=(const ProgramSpec &other);

    vector<glu::StructType *> m_structs;
    vector<Varying> m_varyings;
    vector<string> m_transformFeedbackVaryings;
};

// ProgramSpec

ProgramSpec::ProgramSpec(void)
{
}

ProgramSpec::~ProgramSpec(void)
{
    for (vector<glu::StructType *>::iterator i = m_structs.begin(); i != m_structs.end(); i++)
        delete *i;
}

glu::StructType *ProgramSpec::createStruct(const char *name)
{
    m_structs.reserve(m_structs.size() + 1);
    m_structs.push_back(new glu::StructType(name));
    return m_structs.back();
}

void ProgramSpec::addVarying(const char *name, const glu::VarType &type, Interpolation interp)
{
    m_varyings.push_back(Varying(name, type, interp));
}

void ProgramSpec::addTransformFeedbackVarying(const char *name)
{
    m_transformFeedbackVaryings.push_back(name);
}

bool ProgramSpec::isPointSizeUsed(void) const
{
    return std::find(m_transformFeedbackVaryings.begin(), m_transformFeedbackVaryings.end(), "gl_PointSize") !=
           m_transformFeedbackVaryings.end();
}

static bool isProgramSupported(const glw::Functions &gl, const ProgramSpec &spec, uint32_t tfMode)
{
    int maxVertexAttribs           = 0;
    int maxTfInterleavedComponents = 0;
    int maxTfSeparateAttribs       = 0;
    int maxTfSeparateComponents    = 0;

    gl.getIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
    gl.getIntegerv(GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS, &maxTfInterleavedComponents);
    gl.getIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxTfSeparateAttribs);
    gl.getIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS, &maxTfSeparateComponents);

    // Check vertex attribs.
    int totalVertexAttribs = 1 /* a_position */ + (spec.isPointSizeUsed() ? 1 : 0);
    for (vector<Varying>::const_iterator var = spec.getVaryings().begin(); var != spec.getVaryings().end(); var++)
    {
        for (glu::VectorTypeIterator vecIter = glu::VectorTypeIterator::begin(&var->type);
             vecIter != glu::VectorTypeIterator::end(&var->type); vecIter++)
            totalVertexAttribs += 1;
    }

    if (totalVertexAttribs > maxVertexAttribs)
        return false; // Vertex attribute count exceeded.

    // Check varyings.
    int totalTfComponents = 0;
    int totalTfAttribs    = 0;
    for (vector<string>::const_iterator iter = spec.getTransformFeedbackVaryings().begin();
         iter != spec.getTransformFeedbackVaryings().end(); iter++)
    {
        const string &name = *iter;
        int numComponents  = 0;

        if (name == "gl_Position")
            numComponents = 4;
        else if (name == "gl_PointSize")
            numComponents = 1;
        else
        {
            string varName = glu::parseVariableName(name.c_str());
            const Varying &varying =
                *std::find_if(spec.getVaryings().begin(), spec.getVaryings().end(), VaryingNameEquals(varName));
            glu::TypeComponentVector varPath;

            glu::parseTypePath(name.c_str(), varying.type, varPath);
            numComponents = glu::getVarType(varying.type, varPath).getScalarSize();
        }

        if (tfMode == GL_SEPARATE_ATTRIBS && numComponents > maxTfSeparateComponents)
            return false; // Per-attribute component count exceeded.

        totalTfComponents += numComponents;
        totalTfAttribs += 1;
    }

    if (tfMode == GL_SEPARATE_ATTRIBS && totalTfAttribs > maxTfSeparateAttribs)
        return false;

    if (tfMode == GL_INTERLEAVED_ATTRIBS && totalTfComponents > maxTfInterleavedComponents)
        return false;

    return true;
}

// Program

static std::string getAttributeName(const char *varyingName, const glu::TypeComponentVector &path)
{
    std::ostringstream str;

    str << "a_" << (deStringBeginsWith(varyingName, "v_") ? varyingName + 2 : varyingName);

    for (glu::TypeComponentVector::const_iterator iter = path.begin(); iter != path.end(); iter++)
    {
        const char *prefix = nullptr;

        switch (iter->type)
        {
        case glu::VarTypeComponent::STRUCT_MEMBER:
            prefix = "_m";
            break;
        case glu::VarTypeComponent::ARRAY_ELEMENT:
            prefix = "_e";
            break;
        case glu::VarTypeComponent::MATRIX_COLUMN:
            prefix = "_c";
            break;
        case glu::VarTypeComponent::VECTOR_COMPONENT:
            prefix = "_s";
            break;
        default:
            DE_ASSERT(false);
        }

        str << prefix << iter->index;
    }

    return str.str();
}

static void genShaderSources(const ProgramSpec &spec, std::string &vertSource, std::string &fragSource,
                             bool pointSizeRequired)
{
    std::ostringstream vtx;
    std::ostringstream frag;
    bool addPointSize = spec.isPointSizeUsed();

    vtx << "#version 300 es\n"
        << "in highp vec4 a_position;\n";
    frag << "#version 300 es\n"
         << "layout(location = 0) out mediump vec4 o_color;\n"
         << "uniform highp vec4 u_scale;\n"
         << "uniform highp vec4 u_bias;\n";

    if (addPointSize)
        vtx << "in highp float a_pointSize;\n";

    // Declare attributes.
    for (vector<Varying>::const_iterator var = spec.getVaryings().begin(); var != spec.getVaryings().end(); var++)
    {
        const char *name         = var->name.c_str();
        const glu::VarType &type = var->type;

        for (glu::VectorTypeIterator vecIter = glu::VectorTypeIterator::begin(&type);
             vecIter != glu::VectorTypeIterator::end(&type); vecIter++)
        {
            glu::VarType attribType = glu::getVarType(type, vecIter.getPath());
            string attribName       = getAttributeName(name, vecIter.getPath());

            vtx << "in " << glu::declare(attribType, attribName.c_str()) << ";\n";
        }
    }

    // Declare vayrings.
    for (int ndx = 0; ndx < 2; ndx++)
    {
        const char *inout       = ndx ? "in" : "out";
        std::ostringstream &str = ndx ? frag : vtx;

        // Declare structs that have type name.
        for (vector<glu::StructType *>::const_iterator structIter = spec.getStructs().begin();
             structIter != spec.getStructs().end(); structIter++)
        {
            const glu::StructType *structPtr = *structIter;
            if (structPtr->hasTypeName())
                str << glu::declare(structPtr) << ";\n";
        }

        for (vector<Varying>::const_iterator var = spec.getVaryings().begin(); var != spec.getVaryings().end(); var++)
            str << getInterpolationName(var->interpolation) << " " << inout << " "
                << glu::declare(var->type, var->name.c_str()) << ";\n";
    }

    vtx << "\nvoid main (void)\n{\n"
        << "\tgl_Position = a_position;\n";
    frag << "\nvoid main (void)\n{\n"
         << "\thighp vec4 res = vec4(0.0);\n";

    if (addPointSize)
        vtx << "\tgl_PointSize = a_pointSize;\n";
    else if (pointSizeRequired)
        vtx << "\tgl_PointSize = 1.0;\n";

    // Generate assignments / usage.
    for (vector<Varying>::const_iterator var = spec.getVaryings().begin(); var != spec.getVaryings().end(); var++)
    {
        const char *name         = var->name.c_str();
        const glu::VarType &type = var->type;

        for (glu::VectorTypeIterator vecIter = glu::VectorTypeIterator::begin(&type);
             vecIter != glu::VectorTypeIterator::end(&type); vecIter++)
        {
            glu::VarType subType = glu::getVarType(type, vecIter.getPath());
            string attribName    = getAttributeName(name, vecIter.getPath());

            DE_ASSERT(subType.isBasicType() && glu::isDataTypeScalarOrVector(subType.getBasicType()));

            // Vertex: assign from attribute.
            vtx << "\t" << name << vecIter << " = " << attribName << ";\n";

            // Fragment: add to res variable.
            int scalarSize = glu::getDataTypeScalarSize(subType.getBasicType());

            frag << "\tres += ";
            if (scalarSize == 1)
                frag << "vec4(" << name << vecIter << ")";
            else if (scalarSize == 2)
                frag << "vec2(" << name << vecIter << ").xxyy";
            else if (scalarSize == 3)
                frag << "vec3(" << name << vecIter << ").xyzx";
            else if (scalarSize == 4)
                frag << "vec4(" << name << vecIter << ")";

            frag << ";\n";
        }
    }

    frag << "\to_color = res * u_scale + u_bias;\n";

    vtx << "}\n";
    frag << "}\n";

    vertSource = vtx.str();
    fragSource = frag.str();
}

static glu::ShaderProgram *createVertexCaptureProgram(const glu::RenderContext &context, const ProgramSpec &spec,
                                                      uint32_t bufferMode, uint32_t primitiveType)
{
    std::string vertSource, fragSource;

    genShaderSources(spec, vertSource, fragSource, primitiveType == GL_POINTS /* Is point size required? */);

    return new glu::ShaderProgram(context, glu::ProgramSources()
                                               << glu::VertexSource(vertSource) << glu::FragmentSource(fragSource)
                                               << glu::TransformFeedbackVaryings<vector<string>::const_iterator>(
                                                      spec.getTransformFeedbackVaryings().begin(),
                                                      spec.getTransformFeedbackVaryings().end())
                                               << glu::TransformFeedbackMode(bufferMode));
}

// Helpers.

static void computeInputLayout(vector<Attribute> &attributes, int &inputStride, const vector<Varying> &varyings,
                               bool usePointSize)
{
    inputStride = 0;

    // Add position.
    attributes.push_back(
        Attribute("a_position", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP), inputStride));
    inputStride += 4 * (int)sizeof(uint32_t);

    if (usePointSize)
    {
        attributes.push_back(
            Attribute("a_pointSize", glu::VarType(glu::TYPE_FLOAT, glu::PRECISION_HIGHP), inputStride));
        inputStride += 1 * (int)sizeof(uint32_t);
    }

    // Compute attribute vector.
    for (vector<Varying>::const_iterator var = varyings.begin(); var != varyings.end(); var++)
    {
        for (glu::VectorTypeIterator vecIter = glu::VectorTypeIterator::begin(&var->type);
             vecIter != glu::VectorTypeIterator::end(&var->type); vecIter++)
        {
            glu::VarType type = vecIter.getType();
            string name       = getAttributeName(var->name.c_str(), vecIter.getPath());

            attributes.push_back(Attribute(name, type, inputStride));
            inputStride += glu::getDataTypeScalarSize(type.getBasicType()) * (int)sizeof(uint32_t);
        }
    }
}

static void computeTransformFeedbackOutputs(vector<Output> &transformFeedbackOutputs,
                                            const vector<Attribute> &attributes, const vector<Varying> &varyings,
                                            const vector<string> &transformFeedbackVaryings, uint32_t bufferMode)
{
    int accumulatedSize = 0;

    transformFeedbackOutputs.resize(transformFeedbackVaryings.size());
    for (int varNdx = 0; varNdx < (int)transformFeedbackVaryings.size(); varNdx++)
    {
        const string &name = transformFeedbackVaryings[varNdx];
        int bufNdx         = (bufferMode == GL_SEPARATE_ATTRIBS ? varNdx : 0);
        int offset         = (bufferMode == GL_SEPARATE_ATTRIBS ? 0 : accumulatedSize);
        Output &output     = transformFeedbackOutputs[varNdx];

        output.name      = name;
        output.bufferNdx = bufNdx;
        output.offset    = offset;

        if (name == "gl_Position")
        {
            const Attribute *posIn =
                &(*std::find_if(attributes.begin(), attributes.end(), AttributeNameEquals("a_position")));
            output.type = posIn->type;
            output.inputs.push_back(posIn);
        }
        else if (name == "gl_PointSize")
        {
            const Attribute *sizeIn =
                &(*std::find_if(attributes.begin(), attributes.end(), AttributeNameEquals("a_pointSize")));
            output.type = sizeIn->type;
            output.inputs.push_back(sizeIn);
        }
        else
        {
            string varName         = glu::parseVariableName(name.c_str());
            const Varying &varying = *std::find_if(varyings.begin(), varyings.end(), VaryingNameEquals(varName));
            glu::TypeComponentVector varPath;

            glu::parseTypePath(name.c_str(), varying.type, varPath);

            output.type = glu::getVarType(varying.type, varPath);

            // Add all vectorized attributes as inputs.
            for (glu::VectorTypeIterator iter = glu::VectorTypeIterator::begin(&output.type);
                 iter != glu::VectorTypeIterator::end(&output.type); iter++)
            {
                // Full path.
                glu::TypeComponentVector fullPath(varPath.size() + iter.getPath().size());

                std::copy(varPath.begin(), varPath.end(), fullPath.begin());
                std::copy(iter.getPath().begin(), iter.getPath().end(), fullPath.begin() + varPath.size());

                string attribName = getAttributeName(varName.c_str(), fullPath);
                const Attribute *attrib =
                    &(*std::find_if(attributes.begin(), attributes.end(), AttributeNameEquals(attribName)));

                output.inputs.push_back(attrib);
            }
        }

        accumulatedSize += output.type.getScalarSize() * (int)sizeof(uint32_t);
    }
}

static uint32_t signExtend(uint32_t value, uint32_t numBits)
{
    DE_ASSERT(numBits >= 1u && numBits <= 32u);
    if (numBits == 32u)
        return value;
    else if ((value & (1u << (numBits - 1u))) == 0u)
        return value;
    else
        return value | ~((1u << numBits) - 1u);
}

static void genAttributeData(const Attribute &attrib, uint8_t *basePtr, int stride, int numElements, de::Random &rnd)
{
    const int elementSize          = (int)sizeof(uint32_t);
    const bool isFloat             = glu::isDataTypeFloatOrVec(attrib.type.getBasicType());
    const bool isInt               = glu::isDataTypeIntOrIVec(attrib.type.getBasicType());
    const bool isUint              = glu::isDataTypeUintOrUVec(attrib.type.getBasicType());
    const glu::Precision precision = attrib.type.getPrecision();
    const int numComps             = glu::getDataTypeScalarSize(attrib.type.getBasicType());

    for (int elemNdx = 0; elemNdx < numElements; elemNdx++)
    {
        for (int compNdx = 0; compNdx < numComps; compNdx++)
        {
            int offset = attrib.offset + elemNdx * stride + compNdx * elementSize;
            if (isFloat)
            {
                float *comp = (float *)(basePtr + offset);
                switch (precision)
                {
                case glu::PRECISION_LOWP:
                    *comp = 0.0f + 0.25f * (float)rnd.getInt(0, 4);
                    break;
                case glu::PRECISION_MEDIUMP:
                    *comp = rnd.getFloat(-1e3f, 1e3f);
                    break;
                case glu::PRECISION_HIGHP:
                    *comp = rnd.getFloat(-1e5f, 1e5f);
                    break;
                default:
                    DE_ASSERT(false);
                }
            }
            else if (isInt)
            {
                int *comp = (int *)(basePtr + offset);
                switch (precision)
                {
                case glu::PRECISION_LOWP:
                    *comp = (int)signExtend(rnd.getUint32() & 0xff, 8);
                    break;
                case glu::PRECISION_MEDIUMP:
                    *comp = (int)signExtend(rnd.getUint32() & 0xffff, 16);
                    break;
                case glu::PRECISION_HIGHP:
                    *comp = (int)rnd.getUint32();
                    break;
                default:
                    DE_ASSERT(false);
                }
            }
            else if (isUint)
            {
                uint32_t *comp = (uint32_t *)(basePtr + offset);
                switch (precision)
                {
                case glu::PRECISION_LOWP:
                    *comp = rnd.getUint32() & 0xff;
                    break;
                case glu::PRECISION_MEDIUMP:
                    *comp = rnd.getUint32() & 0xffff;
                    break;
                case glu::PRECISION_HIGHP:
                    *comp = rnd.getUint32();
                    break;
                default:
                    DE_ASSERT(false);
                }
            }
            else
                DE_ASSERT(false);
        }
    }
}

static void genInputData(const vector<Attribute> &attributes, int numInputs, int inputStride, uint8_t *inputBasePtr,
                         de::Random &rnd)
{
    // Random positions.
    const Attribute &position = *std::find_if(attributes.begin(), attributes.end(), AttributeNameEquals("a_position"));

    for (int ndx = 0; ndx < numInputs; ndx++)
    {
        uint8_t *ptr           = inputBasePtr + position.offset + inputStride * ndx;
        *((float *)(ptr + 0))  = rnd.getFloat(-1.2f, 1.2f);
        *((float *)(ptr + 4))  = rnd.getFloat(-1.2f, 1.2f);
        *((float *)(ptr + 8))  = rnd.getFloat(-1.2f, 1.2f);
        *((float *)(ptr + 12)) = rnd.getFloat(0.1f, 2.0f);
    }

    // Point size.
    vector<Attribute>::const_iterator pointSizePos =
        std::find_if(attributes.begin(), attributes.end(), AttributeNameEquals("a_pointSize"));
    if (pointSizePos != attributes.end())
    {
        for (int ndx = 0; ndx < numInputs; ndx++)
        {
            uint8_t *ptr    = inputBasePtr + pointSizePos->offset + inputStride * ndx;
            *((float *)ptr) = rnd.getFloat(1.0f, 8.0f);
        }
    }

    // Random data for rest of components.
    for (vector<Attribute>::const_iterator attrib = attributes.begin(); attrib != attributes.end(); attrib++)
    {
        if (attrib->name == "a_position" || attrib->name == "a_pointSize")
            continue;

        genAttributeData(*attrib, inputBasePtr, inputStride, numInputs, rnd);
    }
}

static uint32_t getTransformFeedbackOutputCount(uint32_t primitiveType, int numElements)
{
    switch (primitiveType)
    {
    case GL_TRIANGLES:
        return numElements - numElements % 3;
    case GL_TRIANGLE_STRIP:
        return de::max(0, numElements - 2) * 3;
    case GL_TRIANGLE_FAN:
        return de::max(0, numElements - 2) * 3;
    case GL_LINES:
        return numElements - numElements % 2;
    case GL_LINE_STRIP:
        return de::max(0, numElements - 1) * 2;
    case GL_LINE_LOOP:
        return numElements > 1 ? numElements * 2 : 0;
    case GL_POINTS:
        return numElements;

    default:
        DE_ASSERT(false);
        return 0;
    }
}

static uint32_t getTransformFeedbackPrimitiveCount(uint32_t primitiveType, int numElements)
{
    switch (primitiveType)
    {
    case GL_TRIANGLES:
        return numElements / 3;
    case GL_TRIANGLE_STRIP:
        return de::max(0, numElements - 2);
    case GL_TRIANGLE_FAN:
        return de::max(0, numElements - 2);
    case GL_LINES:
        return numElements / 2;
    case GL_LINE_STRIP:
        return de::max(0, numElements - 1);
    case GL_LINE_LOOP:
        return numElements > 1 ? numElements : 0;
    case GL_POINTS:
        return numElements;

    default:
        DE_ASSERT(false);
        return 0;
    }
}

static uint32_t getTransformFeedbackPrimitiveMode(uint32_t primitiveType)
{
    switch (primitiveType)
    {
    case GL_TRIANGLES:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
        return GL_TRIANGLES;

    case GL_LINES:
    case GL_LINE_LOOP:
    case GL_LINE_STRIP:
        return GL_LINES;

    case GL_POINTS:
        return GL_POINTS;

    default:
        DE_ASSERT(false);
        return 0;
    }
}

static int getAttributeIndex(uint32_t primitiveType, int numInputs, int outNdx)
{
    switch (primitiveType)
    {
    case GL_TRIANGLES:
        return outNdx;
    case GL_LINES:
        return outNdx;
    case GL_POINTS:
        return outNdx;

    case GL_TRIANGLE_STRIP:
    {
        int triNdx = outNdx / 3;
        int vtxNdx = outNdx % 3;
        return (triNdx % 2 != 0 && vtxNdx < 2) ? (triNdx + 1 - vtxNdx) : (triNdx + vtxNdx);
    }

    case GL_TRIANGLE_FAN:
        return (outNdx % 3 != 0) ? (outNdx / 3 + outNdx % 3) : 0;

    case GL_LINE_STRIP:
        return outNdx / 2 + outNdx % 2;

    case GL_LINE_LOOP:
    {
        int inNdx = outNdx / 2 + outNdx % 2;
        return inNdx < numInputs ? inNdx : 0;
    }

    default:
        DE_ASSERT(false);
        return 0;
    }
}

static bool compareTransformFeedbackOutput(tcu::TestLog &log, uint32_t primitiveType, const Output &output,
                                           int numInputs, const uint8_t *inBasePtr, int inStride,
                                           const uint8_t *outBasePtr, int outStride)
{
    bool isOk     = true;
    int outOffset = output.offset;

    for (int attrNdx = 0; attrNdx < (int)output.inputs.size(); attrNdx++)
    {
        const Attribute &attribute = *output.inputs[attrNdx];
        glu::DataType type         = attribute.type.getBasicType();
        int numComponents          = glu::getDataTypeScalarSize(type);
        glu::Precision precision   = attribute.type.getPrecision();
        glu::DataType scalarType   = glu::getDataTypeScalarType(type);
        int numOutputs             = getTransformFeedbackOutputCount(primitiveType, numInputs);

        for (int outNdx = 0; outNdx < numOutputs; outNdx++)
        {
            int inNdx = getAttributeIndex(primitiveType, numInputs, outNdx);

            for (int compNdx = 0; compNdx < numComponents; compNdx++)
            {
                const uint8_t *inPtr  = inBasePtr + inStride * inNdx + attribute.offset + compNdx * sizeof(uint32_t);
                const uint8_t *outPtr = outBasePtr + outStride * outNdx + outOffset + compNdx * sizeof(uint32_t);
                uint32_t inVal        = *(const uint32_t *)inPtr;
                uint32_t outVal       = *(const uint32_t *)outPtr;
                bool isEqual          = false;

                if (scalarType == glu::TYPE_FLOAT)
                {
                    // ULP comparison is used for highp and mediump. Lowp uses threshold-comparison.
                    switch (precision)
                    {
                    case glu::PRECISION_HIGHP:
                        isEqual = de::abs((int)inVal - (int)outVal) < 2;
                        break;
                    case glu::PRECISION_MEDIUMP:
                        isEqual = de::abs((int)inVal - (int)outVal) < 2 + (1 << 13);
                        break;
                    case glu::PRECISION_LOWP:
                    {
                        float inF  = *(const float *)inPtr;
                        float outF = *(const float *)outPtr;
                        isEqual    = de::abs(inF - outF) < 0.1f;
                        break;
                    }
                    default:
                        DE_ASSERT(false);
                    }
                }
                else
                    isEqual = (inVal == outVal); // Bit-exact match required for integer types.

                if (!isEqual)
                {
                    log << TestLog::Message << "Mismatch in " << output.name << " (" << attribute.name
                        << "), output = " << outNdx << ", input = " << inNdx << ", component = " << compNdx
                        << TestLog::EndMessage;
                    isOk = false;
                    break;
                }
            }

            if (!isOk)
                break;
        }

        if (!isOk)
            break;

        outOffset += numComponents * (int)sizeof(uint32_t);
    }

    return isOk;
}

static int computeTransformFeedbackPrimitiveCount(uint32_t primitiveType, const DrawCall *first, const DrawCall *end)
{
    int primCount = 0;

    for (const DrawCall *call = first; call != end; ++call)
    {
        if (call->transformFeedbackEnabled)
            primCount += getTransformFeedbackPrimitiveCount(primitiveType, call->numElements);
    }

    return primCount;
}

static void writeBufferGuard(const glw::Functions &gl, uint32_t target, int bufferSize, int guardSize)
{
    uint8_t *ptr = (uint8_t *)gl.mapBufferRange(target, bufferSize, guardSize, GL_MAP_WRITE_BIT);
    if (ptr)
        deMemset(ptr, 0xcd, guardSize);
    gl.unmapBuffer(target);
    GLU_EXPECT_NO_ERROR(gl.getError(), "guardband write");
}

static bool verifyGuard(const uint8_t *ptr, int guardSize)
{
    for (int ndx = 0; ndx < guardSize; ndx++)
    {
        if (ptr[ndx] != 0xcd)
            return false;
    }
    return true;
}

static void logTransformFeedbackVaryings(TestLog &log, const glw::Functions &gl, uint32_t program)
{
    int numTfVaryings = 0;
    int maxNameLen    = 0;

    gl.getProgramiv(program, GL_TRANSFORM_FEEDBACK_VARYINGS, &numTfVaryings);
    gl.getProgramiv(program, GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, &maxNameLen);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Query TF varyings");

    log << TestLog::Message << "GL_TRANSFORM_FEEDBACK_VARYINGS = " << numTfVaryings << TestLog::EndMessage;

    vector<char> nameBuf(maxNameLen + 1);

    for (int ndx = 0; ndx < numTfVaryings; ndx++)
    {
        glw::GLsizei size = 0;
        glw::GLenum type  = 0;

        gl.getTransformFeedbackVarying(program, ndx, (glw::GLsizei)nameBuf.size(), nullptr, &size, &type, &nameBuf[0]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetTransformFeedbackVarying()");

        const glu::DataType dataType = glu::getDataTypeFromGLType(type);
        const std::string typeName   = dataType != glu::TYPE_LAST ?
                                           std::string(glu::getDataTypeName(dataType)) :
                                           (std::string("unknown(") + tcu::toHex(type).toString() + ")");

        log << TestLog::Message << (const char *)&nameBuf[0] << ": " << typeName << "[" << size << "]"
            << TestLog::EndMessage;
    }
}

class TransformFeedbackCase : public TestCase
{
public:
    TransformFeedbackCase(Context &context, const char *name, const char *desc, uint32_t bufferMode,
                          uint32_t primitiveType);
    ~TransformFeedbackCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

protected:
    ProgramSpec m_progSpec;
    uint32_t m_bufferMode;
    uint32_t m_primitiveType;

private:
    TransformFeedbackCase(const TransformFeedbackCase &other);
    TransformFeedbackCase &operator=(const TransformFeedbackCase &other);

    bool runTest(const DrawCall *first, const DrawCall *end, uint32_t seed);

    // Derived from ProgramSpec in init()
    int m_inputStride;
    vector<Attribute> m_attributes;
    vector<Output> m_transformFeedbackOutputs;
    vector<int> m_bufferStrides;

    // GL state.
    glu::ShaderProgram *m_program;
    glu::TransformFeedback *m_transformFeedback;
    vector<uint32_t> m_outputBuffers;

    int m_iterNdx;
};

TransformFeedbackCase::TransformFeedbackCase(Context &context, const char *name, const char *desc, uint32_t bufferMode,
                                             uint32_t primitiveType)
    : TestCase(context, name, desc)
    , m_bufferMode(bufferMode)
    , m_primitiveType(primitiveType)
    , m_inputStride(0)
    , m_program(nullptr)
    , m_transformFeedback(nullptr)
    , m_iterNdx(0)
{
}

TransformFeedbackCase::~TransformFeedbackCase(void)
{
    TransformFeedbackCase::deinit();
}

static bool hasArraysInTFVaryings(const ProgramSpec &spec)
{
    for (vector<string>::const_iterator tfVar = spec.getTransformFeedbackVaryings().begin();
         tfVar != spec.getTransformFeedbackVaryings().end(); ++tfVar)
    {
        string varName = glu::parseVariableName(tfVar->c_str());
        vector<Varying>::const_iterator varIter =
            std::find_if(spec.getVaryings().begin(), spec.getVaryings().end(), VaryingNameEquals(varName));

        if (varName == "gl_Position" || varName == "gl_PointSize")
            continue;

        DE_ASSERT(varIter != spec.getVaryings().end());

        if (varIter->type.isArrayType())
            return true;
    }

    return false;
}

void TransformFeedbackCase::init(void)
{
    TestLog &log             = m_testCtx.getLog();
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    DE_ASSERT(!m_program);
    m_program = createVertexCaptureProgram(m_context.getRenderContext(), m_progSpec, m_bufferMode, m_primitiveType);

    log << *m_program;
    if (!m_program->isOk())
    {
        const bool linkFail = m_program->getShaderInfo(glu::SHADERTYPE_VERTEX).compileOk &&
                              m_program->getShaderInfo(glu::SHADERTYPE_FRAGMENT).compileOk &&
                              !m_program->getProgramInfo().linkOk;

        if (linkFail)
        {
            if (!isProgramSupported(gl, m_progSpec, m_bufferMode))
                throw tcu::NotSupportedError("Implementation limits execeeded", "", __FILE__, __LINE__);
            else if (hasArraysInTFVaryings(m_progSpec))
                throw tcu::NotSupportedError("Capturing arrays is not supported (undefined in specification)", "",
                                             __FILE__, __LINE__);
            else
                throw tcu::TestError("Link failed", "", __FILE__, __LINE__);
        }
        else
            throw tcu::TestError("Compile failed", "", __FILE__, __LINE__);
    }

    log << TestLog::Message << "Transform feedback varyings: "
        << tcu::formatArray(m_progSpec.getTransformFeedbackVaryings().begin(),
                            m_progSpec.getTransformFeedbackVaryings().end())
        << TestLog::EndMessage;

    // Print out transform feedback points reported by GL.
    log << TestLog::Message << "Transform feedback varyings reported by compiler:" << TestLog::EndMessage;
    logTransformFeedbackVaryings(log, gl, m_program->getProgram());

    // Compute input specification.
    computeInputLayout(m_attributes, m_inputStride, m_progSpec.getVaryings(), m_progSpec.isPointSizeUsed());

    // Build list of varyings used in transform feedback.
    computeTransformFeedbackOutputs(m_transformFeedbackOutputs, m_attributes, m_progSpec.getVaryings(),
                                    m_progSpec.getTransformFeedbackVaryings(), m_bufferMode);
    DE_ASSERT(!m_transformFeedbackOutputs.empty());

    // Buffer strides.
    DE_ASSERT(m_bufferStrides.empty());
    if (m_bufferMode == GL_SEPARATE_ATTRIBS)
    {
        for (vector<Output>::const_iterator outIter = m_transformFeedbackOutputs.begin();
             outIter != m_transformFeedbackOutputs.end(); outIter++)
            m_bufferStrides.push_back(outIter->type.getScalarSize() * (int)sizeof(uint32_t));
    }
    else
    {
        int totalSize = 0;
        for (vector<Output>::const_iterator outIter = m_transformFeedbackOutputs.begin();
             outIter != m_transformFeedbackOutputs.end(); outIter++)
            totalSize += outIter->type.getScalarSize() * (int)sizeof(uint32_t);

        m_bufferStrides.push_back(totalSize);
    }

    // \note Actual storage is allocated in iterate().
    m_outputBuffers.resize(m_bufferStrides.size());
    gl.genBuffers((glw::GLsizei)m_outputBuffers.size(), &m_outputBuffers[0]);

    DE_ASSERT(!m_transformFeedback);
    m_transformFeedback = new glu::TransformFeedback(m_context.getRenderContext());

    GLU_EXPECT_NO_ERROR(gl.getError(), "init");

    m_iterNdx = 0;
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
}

void TransformFeedbackCase::deinit(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    if (!m_outputBuffers.empty())
    {
        gl.deleteBuffers((glw::GLsizei)m_outputBuffers.size(), &m_outputBuffers[0]);
        m_outputBuffers.clear();
    }

    delete m_transformFeedback;
    m_transformFeedback = nullptr;

    delete m_program;
    m_program = nullptr;

    // Clean up state.
    m_attributes.clear();
    m_transformFeedbackOutputs.clear();
    m_bufferStrides.clear();
    m_inputStride = 0;
}

TransformFeedbackCase::IterateResult TransformFeedbackCase::iterate(void)
{
    // Test cases.
    static const DrawCall s_elemCount1[]   = {DrawCall(1, true)};
    static const DrawCall s_elemCount2[]   = {DrawCall(2, true)};
    static const DrawCall s_elemCount3[]   = {DrawCall(3, true)};
    static const DrawCall s_elemCount4[]   = {DrawCall(4, true)};
    static const DrawCall s_elemCount123[] = {DrawCall(123, true)};
    static const DrawCall s_basicPause1[]  = {DrawCall(64, true), DrawCall(64, false), DrawCall(64, true)};
    static const DrawCall s_basicPause2[]  = {DrawCall(13, true), DrawCall(5, true), DrawCall(17, false),
                                              DrawCall(3, true), DrawCall(7, false)};
    static const DrawCall s_startPaused[]  = {DrawCall(123, false), DrawCall(123, true)};
    static const DrawCall s_random1[]      = {DrawCall(65, true),  DrawCall(135, false), DrawCall(74, true),
                                              DrawCall(16, false), DrawCall(226, false), DrawCall(9, true),
                                              DrawCall(174, false)};
    static const DrawCall s_random2[]      = {DrawCall(217, true), DrawCall(171, true), DrawCall(147, true),
                                              DrawCall(152, false), DrawCall(55, true)};

    static const struct
    {
        const DrawCall *calls;
        int numCalls;
    } s_iterations[] = {
#define ITER(ARR) {ARR, DE_LENGTH_OF_ARRAY(ARR)}
        ITER(s_elemCount1),   ITER(s_elemCount2),  ITER(s_elemCount3),  ITER(s_elemCount4),
        ITER(s_elemCount123), ITER(s_basicPause1), ITER(s_basicPause2), ITER(s_startPaused),
        ITER(s_random1),      ITER(s_random2)
#undef ITER
    };

    TestLog &log          = m_testCtx.getLog();
    bool isOk             = true;
    uint32_t seed         = deStringHash(getName()) ^ deInt32Hash(m_iterNdx);
    int numIterations     = DE_LENGTH_OF_ARRAY(s_iterations);
    const DrawCall *first = s_iterations[m_iterNdx].calls;
    const DrawCall *end   = s_iterations[m_iterNdx].calls + s_iterations[m_iterNdx].numCalls;

    std::string sectionName = std::string("Iteration") + de::toString(m_iterNdx + 1);
    std::string sectionDesc =
        std::string("Iteration ") + de::toString(m_iterNdx + 1) + " / " + de::toString(numIterations);
    tcu::ScopedLogSection section(log, sectionName, sectionDesc);

    log << TestLog::Message << "Testing " << s_iterations[m_iterNdx].numCalls
        << " draw calls, (element count, TF state): " << tcu::formatArray(first, end) << TestLog::EndMessage;

    isOk = runTest(first, end, seed);

    if (!isOk)
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Result comparison failed");

    m_iterNdx += 1;
    return (isOk && m_iterNdx < numIterations) ? CONTINUE : STOP;
}

bool TransformFeedbackCase::runTest(const DrawCall *first, const DrawCall *end, uint32_t seed)
{
    TestLog &log             = m_testCtx.getLog();
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    de::Random rnd(seed);
    int numInputs  = 0; //!< Sum of element counts in calls.
    int numOutputs = 0; //!< Sum of output counts for calls that have transform feedback enabled.
    int width      = m_context.getRenderContext().getRenderTarget().getWidth();
    int height     = m_context.getRenderContext().getRenderTarget().getHeight();
    int viewportW  = de::min((int)VIEWPORT_WIDTH, width);
    int viewportH  = de::min((int)VIEWPORT_HEIGHT, height);
    int viewportX  = rnd.getInt(0, width - viewportW);
    int viewportY  = rnd.getInt(0, height - viewportH);
    tcu::Surface frameWithTf(viewportW, viewportH);
    tcu::Surface frameWithoutTf(viewportW, viewportH);
    glu::Query primitiveQuery(m_context.getRenderContext());
    bool outputsOk = true;
    bool imagesOk  = true;
    bool queryOk   = true;

    // Compute totals.
    for (const DrawCall *call = first; call != end; call++)
    {
        numInputs += call->numElements;
        numOutputs +=
            call->transformFeedbackEnabled ? getTransformFeedbackOutputCount(m_primitiveType, call->numElements) : 0;
    }

    // Input data.
    vector<uint8_t> inputData(m_inputStride * numInputs);
    genInputData(m_attributes, numInputs, m_inputStride, &inputData[0], rnd);

    gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, m_transformFeedback->get());
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTransformFeedback()");

    // Allocate storage for transform feedback output buffers and bind to targets.
    for (int bufNdx = 0; bufNdx < (int)m_outputBuffers.size(); bufNdx++)
    {
        uint32_t buffer      = m_outputBuffers[bufNdx];
        int stride           = m_bufferStrides[bufNdx];
        int target           = bufNdx;
        int size             = stride * numOutputs;
        int guardSize        = stride * BUFFER_GUARD_MULTIPLIER;
        const uint32_t usage = GL_DYNAMIC_READ;

        gl.bindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffer);
        gl.bufferData(GL_TRANSFORM_FEEDBACK_BUFFER, size + guardSize, nullptr, usage);
        writeBufferGuard(gl, GL_TRANSFORM_FEEDBACK_BUFFER, size, guardSize);

        // \todo [2012-07-30 pyry] glBindBufferRange()?
        gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, target, buffer);

        GLU_EXPECT_NO_ERROR(gl.getError(), "transform feedback buffer setup");
    }

    // Setup attributes.
    for (vector<Attribute>::const_iterator attrib = m_attributes.begin(); attrib != m_attributes.end(); attrib++)
    {
        int loc                  = gl.getAttribLocation(m_program->getProgram(), attrib->name.c_str());
        glu::DataType scalarType = glu::getDataTypeScalarType(attrib->type.getBasicType());
        int numComponents        = glu::getDataTypeScalarSize(attrib->type.getBasicType());
        const void *ptr          = &inputData[0] + attrib->offset;

        if (loc >= 0)
        {
            gl.enableVertexAttribArray(loc);

            if (scalarType == glu::TYPE_FLOAT)
                gl.vertexAttribPointer(loc, numComponents, GL_FLOAT, GL_FALSE, m_inputStride, ptr);
            else if (scalarType == glu::TYPE_INT)
                gl.vertexAttribIPointer(loc, numComponents, GL_INT, m_inputStride, ptr);
            else if (scalarType == glu::TYPE_UINT)
                gl.vertexAttribIPointer(loc, numComponents, GL_UNSIGNED_INT, m_inputStride, ptr);
        }
    }

    // Setup viewport.
    gl.viewport(viewportX, viewportY, viewportW, viewportH);

    // Setup program.
    gl.useProgram(m_program->getProgram());

    gl.uniform4fv(gl.getUniformLocation(m_program->getProgram(), "u_scale"), 1, tcu::Vec4(0.01f).getPtr());
    gl.uniform4fv(gl.getUniformLocation(m_program->getProgram(), "u_bias"), 1, tcu::Vec4(0.5f).getPtr());

    // Enable query.
    gl.beginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, *primitiveQuery);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN)");

    // Draw.
    {
        int offset     = 0;
        bool tfEnabled = true;

        gl.clear(GL_COLOR_BUFFER_BIT);

        gl.beginTransformFeedback(getTransformFeedbackPrimitiveMode(m_primitiveType));

        for (const DrawCall *call = first; call != end; call++)
        {
            // Pause or resume transform feedback if necessary.
            if (call->transformFeedbackEnabled != tfEnabled)
            {
                if (call->transformFeedbackEnabled)
                    gl.resumeTransformFeedback();
                else
                    gl.pauseTransformFeedback();
                tfEnabled = call->transformFeedbackEnabled;
            }

            gl.drawArrays(m_primitiveType, offset, call->numElements);
            offset += call->numElements;
        }

        // Resume feedback before finishing it.
        if (!tfEnabled)
            gl.resumeTransformFeedback();

        gl.endTransformFeedback();
        GLU_EXPECT_NO_ERROR(gl.getError(), "render");
    }

    gl.endQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN)");

    // Check and log query status right after submit
    {
        uint32_t available = GL_FALSE;
        gl.getQueryObjectuiv(*primitiveQuery, GL_QUERY_RESULT_AVAILABLE, &available);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetQueryObjectuiv()");

        log << TestLog::Message << "GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN status after submit: "
            << (available != GL_FALSE ? "GL_TRUE" : "GL_FALSE") << TestLog::EndMessage;
    }

    // Compare result buffers.
    for (int bufferNdx = 0; bufferNdx < (int)m_outputBuffers.size(); bufferNdx++)
    {
        uint32_t buffer    = m_outputBuffers[bufferNdx];
        int stride         = m_bufferStrides[bufferNdx];
        int size           = stride * numOutputs;
        int guardSize      = stride * BUFFER_GUARD_MULTIPLIER;
        const void *bufPtr = nullptr;

        // Bind buffer for reading.
        gl.bindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffer);
        bufPtr = gl.mapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, size + guardSize, GL_MAP_READ_BIT);
        GLU_EXPECT_NO_ERROR(gl.getError(), "mapping buffer");

        // Verify all output variables that are written to this buffer.
        for (vector<Output>::const_iterator out = m_transformFeedbackOutputs.begin();
             out != m_transformFeedbackOutputs.end(); out++)
        {
            if (out->bufferNdx != bufferNdx)
                continue;

            int inputOffset  = 0;
            int outputOffset = 0;

            // Process all draw calls and check ones with transform feedback enabled.
            for (const DrawCall *call = first; call != end; call++)
            {
                if (call->transformFeedbackEnabled)
                {
                    const uint8_t *inputPtr  = &inputData[0] + inputOffset * m_inputStride;
                    const uint8_t *outputPtr = (const uint8_t *)bufPtr + outputOffset * stride;

                    if (!compareTransformFeedbackOutput(log, m_primitiveType, *out, call->numElements, inputPtr,
                                                        m_inputStride, outputPtr, stride))
                    {
                        outputsOk = false;
                        break;
                    }
                }

                inputOffset += call->numElements;
                outputOffset += call->transformFeedbackEnabled ?
                                    getTransformFeedbackOutputCount(m_primitiveType, call->numElements) :
                                    0;
            }
        }

        // Verify guardband.
        if (!verifyGuard((const uint8_t *)bufPtr + size, guardSize))
        {
            log << TestLog::Message << "Error: Transform feedback buffer overrun detected" << TestLog::EndMessage;
            outputsOk = false;
        }

        gl.unmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
    }

    // Check status after mapping buffers.
    {
        const bool mustBeReady  = !m_outputBuffers.empty(); // Mapping buffer forces synchronization.
        const int expectedCount = computeTransformFeedbackPrimitiveCount(m_primitiveType, first, end);
        uint32_t available      = GL_FALSE;
        uint32_t numPrimitives  = 0;

        gl.getQueryObjectuiv(*primitiveQuery, GL_QUERY_RESULT_AVAILABLE, &available);
        gl.getQueryObjectuiv(*primitiveQuery, GL_QUERY_RESULT, &numPrimitives);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetQueryObjectuiv()");

        if (!mustBeReady && available == GL_FALSE)
        {
            log << TestLog::Message
                << "ERROR: GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN result not available after mapping buffers!"
                << TestLog::EndMessage;
            queryOk = false;
        }

        log << TestLog::Message << "GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN = " << numPrimitives
            << TestLog::EndMessage;

        if ((int)numPrimitives != expectedCount)
        {
            log << TestLog::Message << "ERROR: Expected " << expectedCount << " primitives!" << TestLog::EndMessage;
            queryOk = false;
        }
    }

    // Clear transform feedback state.
    gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    for (int bufNdx = 0; bufNdx < (int)m_outputBuffers.size(); bufNdx++)
    {
        gl.bindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);
        gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, bufNdx, 0);
    }

    // Read back rendered image.
    glu::readPixels(m_context.getRenderContext(), viewportX, viewportY, frameWithTf.getAccess());

    // Render without transform feedback.
    {
        int offset = 0;

        gl.clear(GL_COLOR_BUFFER_BIT);

        for (const DrawCall *call = first; call != end; call++)
        {
            gl.drawArrays(m_primitiveType, offset, call->numElements);
            offset += call->numElements;
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "render");
        glu::readPixels(m_context.getRenderContext(), viewportX, viewportY, frameWithoutTf.getAccess());
    }

    // Compare images with and without transform feedback.
    imagesOk = tcu::pixelThresholdCompare(log, "Result", "Image comparison result", frameWithoutTf, frameWithTf,
                                          tcu::RGBA(1, 1, 1, 1), tcu::COMPARE_LOG_ON_ERROR);

    if (imagesOk)
        m_testCtx.getLog() << TestLog::Message
                           << "Rendering result comparison between TF enabled and TF disabled passed."
                           << TestLog::EndMessage;
    else
        m_testCtx.getLog() << TestLog::Message
                           << "ERROR: Rendering result comparison between TF enabled and TF disabled failed!"
                           << TestLog::EndMessage;

    return outputsOk && imagesOk && queryOk;
}

// Test cases.

class PositionCase : public TransformFeedbackCase
{
public:
    PositionCase(Context &context, const char *name, const char *desc, uint32_t bufferType, uint32_t primitiveType)
        : TransformFeedbackCase(context, name, desc, bufferType, primitiveType)
    {
        m_progSpec.addTransformFeedbackVarying("gl_Position");
    }
};

class PointSizeCase : public TransformFeedbackCase
{
public:
    PointSizeCase(Context &context, const char *name, const char *desc, uint32_t bufferType, uint32_t primitiveType)
        : TransformFeedbackCase(context, name, desc, bufferType, primitiveType)
    {
        m_progSpec.addTransformFeedbackVarying("gl_PointSize");
    }
};

class BasicTypeCase : public TransformFeedbackCase
{
public:
    BasicTypeCase(Context &context, const char *name, const char *desc, uint32_t bufferType, uint32_t primitiveType,
                  glu::DataType type, glu::Precision precision, Interpolation interpolation)
        : TransformFeedbackCase(context, name, desc, bufferType, primitiveType)
    {
        m_progSpec.addVarying("v_varA", glu::VarType(type, precision), interpolation);
        m_progSpec.addVarying("v_varB", glu::VarType(type, precision), interpolation);

        m_progSpec.addTransformFeedbackVarying("v_varA");
        m_progSpec.addTransformFeedbackVarying("v_varB");
    }
};

class BasicArrayCase : public TransformFeedbackCase
{
public:
    BasicArrayCase(Context &context, const char *name, const char *desc, uint32_t bufferType, uint32_t primitiveType,
                   glu::DataType type, glu::Precision precision, Interpolation interpolation)
        : TransformFeedbackCase(context, name, desc, bufferType, primitiveType)
    {
        if (glu::isDataTypeMatrix(type) || m_bufferMode == GL_SEPARATE_ATTRIBS)
        {
            // \note For matrix types we need to use reduced array sizes or otherwise we will exceed maximum attribute (16)
            //         or transform feedback component count (64).
            //         On separate attribs mode maximum component count per varying is 4.
            m_progSpec.addVarying("v_varA", glu::VarType(glu::VarType(type, precision), 1), interpolation);
            m_progSpec.addVarying("v_varB", glu::VarType(glu::VarType(type, precision), 2), interpolation);
        }
        else
        {
            m_progSpec.addVarying("v_varA", glu::VarType(glu::VarType(type, precision), 3), interpolation);
            m_progSpec.addVarying("v_varB", glu::VarType(glu::VarType(type, precision), 4), interpolation);
        }

        m_progSpec.addTransformFeedbackVarying("v_varA");
        m_progSpec.addTransformFeedbackVarying("v_varB");
    }
};

class ArrayElementCase : public TransformFeedbackCase
{
public:
    ArrayElementCase(Context &context, const char *name, const char *desc, uint32_t bufferType, uint32_t primitiveType,
                     glu::DataType type, glu::Precision precision, Interpolation interpolation)
        : TransformFeedbackCase(context, name, desc, bufferType, primitiveType)
    {
        m_progSpec.addVarying("v_varA", glu::VarType(glu::VarType(type, precision), 3), interpolation);
        m_progSpec.addVarying("v_varB", glu::VarType(glu::VarType(type, precision), 4), interpolation);

        m_progSpec.addTransformFeedbackVarying("v_varA[1]");
        m_progSpec.addTransformFeedbackVarying("v_varB[0]");
        m_progSpec.addTransformFeedbackVarying("v_varB[3]");
    }
};

class RandomCase : public TransformFeedbackCase
{
public:
    RandomCase(Context &context, const char *name, const char *desc, uint32_t bufferType, uint32_t primitiveType,
               uint32_t seed, bool elementCapture)
        : TransformFeedbackCase(context, name, desc, bufferType, primitiveType)
        , m_seed(seed)
        , m_elementCapture(elementCapture)
    {
    }

    void init(void)
    {
        // \note Hard-coded indices and hackery are used when indexing this, beware.
        static const glu::DataType typeCandidates[] = {
            glu::TYPE_FLOAT,        glu::TYPE_FLOAT_VEC2,   glu::TYPE_FLOAT_VEC3,   glu::TYPE_FLOAT_VEC4,
            glu::TYPE_INT,          glu::TYPE_INT_VEC2,     glu::TYPE_INT_VEC3,     glu::TYPE_INT_VEC4,
            glu::TYPE_UINT,         glu::TYPE_UINT_VEC2,    glu::TYPE_UINT_VEC3,    glu::TYPE_UINT_VEC4,

            glu::TYPE_FLOAT_MAT2,   glu::TYPE_FLOAT_MAT2X3, glu::TYPE_FLOAT_MAT2X4,

            glu::TYPE_FLOAT_MAT3X2, glu::TYPE_FLOAT_MAT3,   glu::TYPE_FLOAT_MAT3X4,

            glu::TYPE_FLOAT_MAT4X2, glu::TYPE_FLOAT_MAT4X3, glu::TYPE_FLOAT_MAT4};

        static const glu::Precision precisions[] = {glu::PRECISION_LOWP, glu::PRECISION_MEDIUMP, glu::PRECISION_HIGHP};

        static const Interpolation interpModes[] = {INTERPOLATION_FLAT, INTERPOLATION_SMOOTH, INTERPOLATION_CENTROID};

        const int maxAttributeVectors = 16;
        //        const int    maxTransformFeedbackComponents = 64; // \note It is enough to limit attribute set size.
        bool isSeparateMode                = m_bufferMode == GL_SEPARATE_ATTRIBS;
        int maxTransformFeedbackVars       = isSeparateMode ? 4 : maxAttributeVectors;
        const float arrayWeight            = 0.3f;
        const float positionWeight         = 0.7f;
        const float pointSizeWeight        = 0.1f;
        const float captureFullArrayWeight = 0.5f;

        de::Random rnd(m_seed);
        bool usePosition          = rnd.getFloat() < positionWeight;
        bool usePointSize         = rnd.getFloat() < pointSizeWeight;
        int numAttribVectorsToUse = rnd.getInt(1, maxAttributeVectors - 1 /*position*/ - (usePointSize ? 1 : 0));

        int numAttributeVectors = 0;
        int varNdx              = 0;

        // Generate varyings.
        while (numAttributeVectors < numAttribVectorsToUse)
        {
            int maxVecs = isSeparateMode ? de::min(2 /*at most 2*mat2*/, numAttribVectorsToUse - numAttributeVectors) :
                                           numAttribVectorsToUse - numAttributeVectors;
            const glu::DataType *begin = &typeCandidates[0];
            const glu::DataType *end   = begin + (maxVecs >= 4 ? 21 :
                                                  maxVecs >= 3 ? 18 :
                                                  maxVecs >= 2 ? (isSeparateMode ? 13 : 15) :
                                                                 12);

            glu::DataType type = rnd.choose<glu::DataType>(begin, end);
            glu::Precision precision =
                rnd.choose<glu::Precision>(&precisions[0], &precisions[0] + DE_LENGTH_OF_ARRAY(precisions));
            Interpolation interp =
                glu::getDataTypeScalarType(type) == glu::TYPE_FLOAT ?
                    rnd.choose<Interpolation>(&interpModes[0], &interpModes[0] + DE_LENGTH_OF_ARRAY(interpModes)) :
                    INTERPOLATION_FLAT;
            int numVecs      = glu::isDataTypeMatrix(type) ? glu::getDataTypeMatrixNumColumns(type) : 1;
            int numComps     = glu::getDataTypeScalarSize(type);
            int maxArrayLen  = de::max(1, isSeparateMode ? 4 / numComps : maxVecs / numVecs);
            bool useArray    = rnd.getFloat() < arrayWeight;
            int arrayLen     = useArray ? rnd.getInt(1, maxArrayLen) : 1;
            std::string name = "v_var" + de::toString(varNdx);

            if (useArray)
                m_progSpec.addVarying(name.c_str(), glu::VarType(glu::VarType(type, precision), arrayLen), interp);
            else
                m_progSpec.addVarying(name.c_str(), glu::VarType(type, precision), interp);

            numAttributeVectors += arrayLen * numVecs;
            varNdx += 1;
        }

        // Generate transform feedback candidate set.
        vector<string> tfCandidates;

        if (usePosition)
            tfCandidates.push_back("gl_Position");
        if (usePointSize)
            tfCandidates.push_back("gl_PointSize");

        for (int ndx = 0; ndx < varNdx /* num varyings */; ndx++)
        {
            const Varying &var = m_progSpec.getVaryings()[ndx];

            if (var.type.isArrayType())
            {
                const bool captureFull = m_elementCapture ? (rnd.getFloat() < captureFullArrayWeight) : true;

                if (captureFull)
                    tfCandidates.push_back(var.name);
                else
                {
                    const int numElem = var.type.getArraySize();
                    for (int elemNdx = 0; elemNdx < numElem; elemNdx++)
                        tfCandidates.push_back(var.name + "[" + de::toString(elemNdx) + "]");
                }
            }
            else
                tfCandidates.push_back(var.name);
        }

        // Pick random selection.
        vector<string> tfVaryings(de::min((int)tfCandidates.size(), maxTransformFeedbackVars));
        rnd.choose(tfCandidates.begin(), tfCandidates.end(), tfVaryings.begin(), (int)tfVaryings.size());
        rnd.shuffle(tfVaryings.begin(), tfVaryings.end());

        for (vector<string>::const_iterator var = tfVaryings.begin(); var != tfVaryings.end(); var++)
            m_progSpec.addTransformFeedbackVarying(var->c_str());

        TransformFeedbackCase::init();
    }

private:
    uint32_t m_seed;
    bool m_elementCapture;
};

} // namespace TransformFeedback

using namespace TransformFeedback;

TransformFeedbackTests::TransformFeedbackTests(Context &context)
    : TestCaseGroup(context, "transform_feedback", "Transform feedback tests")
{
}

TransformFeedbackTests::~TransformFeedbackTests(void)
{
}

void TransformFeedbackTests::init(void)
{
    static const struct
    {
        const char *name;
        uint32_t mode;
    } bufferModes[] = {{"separate", GL_SEPARATE_ATTRIBS}, {"interleaved", GL_INTERLEAVED_ATTRIBS}};

    static const struct
    {
        const char *name;
        uint32_t type;
    } primitiveTypes[] = {
        {"points", GL_POINTS}, {"lines", GL_LINES}, {"triangles", GL_TRIANGLES}

        // Not supported by GLES3.
        //        { "line_strip", GL_LINE_STRIP        },
        //        { "line_loop", GL_LINE_LOOP        },
        //        { "triangle_fan", GL_TRIANGLE_FAN        },
        //        { "triangle_strip", GL_TRIANGLE_STRIP    }
    };

    static const glu::DataType basicTypes[] = {glu::TYPE_FLOAT,        glu::TYPE_FLOAT_VEC2,   glu::TYPE_FLOAT_VEC3,
                                               glu::TYPE_FLOAT_VEC4,   glu::TYPE_FLOAT_MAT2,   glu::TYPE_FLOAT_MAT2X3,
                                               glu::TYPE_FLOAT_MAT2X4, glu::TYPE_FLOAT_MAT3X2, glu::TYPE_FLOAT_MAT3,
                                               glu::TYPE_FLOAT_MAT3X4, glu::TYPE_FLOAT_MAT4X2, glu::TYPE_FLOAT_MAT4X3,
                                               glu::TYPE_FLOAT_MAT4,   glu::TYPE_INT,          glu::TYPE_INT_VEC2,
                                               glu::TYPE_INT_VEC3,     glu::TYPE_INT_VEC4,     glu::TYPE_UINT,
                                               glu::TYPE_UINT_VEC2,    glu::TYPE_UINT_VEC3,    glu::TYPE_UINT_VEC4};

    static const glu::Precision precisions[] = {glu::PRECISION_LOWP, glu::PRECISION_MEDIUMP, glu::PRECISION_HIGHP};

    static const struct
    {
        const char *name;
        Interpolation interp;
    } interpModes[] = {
        {"smooth", INTERPOLATION_SMOOTH}, {"flat", INTERPOLATION_FLAT}, {"centroid", INTERPOLATION_CENTROID}};

    // .position
    {
        tcu::TestCaseGroup *positionGroup =
            new tcu::TestCaseGroup(m_testCtx, "position", "gl_Position capture using transform feedback");
        addChild(positionGroup);

        for (int primitiveType = 0; primitiveType < DE_LENGTH_OF_ARRAY(primitiveTypes); primitiveType++)
        {
            for (int bufferMode = 0; bufferMode < DE_LENGTH_OF_ARRAY(bufferModes); bufferMode++)
            {
                string name = string(primitiveTypes[primitiveType].name) + "_" + bufferModes[bufferMode].name;
                positionGroup->addChild(new PositionCase(m_context, name.c_str(), "", bufferModes[bufferMode].mode,
                                                         primitiveTypes[primitiveType].type));
            }
        }
    }

    // .point_size
    {
        tcu::TestCaseGroup *pointSizeGroup =
            new tcu::TestCaseGroup(m_testCtx, "point_size", "gl_PointSize capture using transform feedback");
        addChild(pointSizeGroup);

        for (int primitiveType = 0; primitiveType < DE_LENGTH_OF_ARRAY(primitiveTypes); primitiveType++)
        {
            for (int bufferMode = 0; bufferMode < DE_LENGTH_OF_ARRAY(bufferModes); bufferMode++)
            {
                string name = string(primitiveTypes[primitiveType].name) + "_" + bufferModes[bufferMode].name;
                pointSizeGroup->addChild(new PointSizeCase(m_context, name.c_str(), "", bufferModes[bufferMode].mode,
                                                           primitiveTypes[primitiveType].type));
            }
        }
    }

    // .basic_type
    {
        tcu::TestCaseGroup *basicTypeGroup =
            new tcu::TestCaseGroup(m_testCtx, "basic_types", "Basic types in transform feedback");
        addChild(basicTypeGroup);

        for (int bufferModeNdx = 0; bufferModeNdx < DE_LENGTH_OF_ARRAY(bufferModes); bufferModeNdx++)
        {
            tcu::TestCaseGroup *modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[bufferModeNdx].name, "");
            uint32_t bufferMode           = bufferModes[bufferModeNdx].mode;
            basicTypeGroup->addChild(modeGroup);

            for (int primitiveTypeNdx = 0; primitiveTypeNdx < DE_LENGTH_OF_ARRAY(primitiveTypes); primitiveTypeNdx++)
            {
                tcu::TestCaseGroup *primitiveGroup =
                    new tcu::TestCaseGroup(m_testCtx, primitiveTypes[primitiveTypeNdx].name, "");
                uint32_t primitiveType = primitiveTypes[primitiveTypeNdx].type;
                modeGroup->addChild(primitiveGroup);

                for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(basicTypes); typeNdx++)
                {
                    glu::DataType type = basicTypes[typeNdx];
                    bool isFloat       = glu::getDataTypeScalarType(type) == glu::TYPE_FLOAT;

                    for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); precNdx++)
                    {
                        glu::Precision precision = precisions[precNdx];

                        string name = string(glu::getPrecisionName(precision)) + "_" + glu::getDataTypeName(type);
                        primitiveGroup->addChild(
                            new BasicTypeCase(m_context, name.c_str(), "", bufferMode, primitiveType, type, precision,
                                              isFloat ? INTERPOLATION_SMOOTH : INTERPOLATION_FLAT));
                    }
                }
            }
        }
    }

    // .array
    {
        tcu::TestCaseGroup *arrayGroup = new tcu::TestCaseGroup(m_testCtx, "array", "Capturing whole array in TF");
        addChild(arrayGroup);

        for (int bufferModeNdx = 0; bufferModeNdx < DE_LENGTH_OF_ARRAY(bufferModes); bufferModeNdx++)
        {
            tcu::TestCaseGroup *modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[bufferModeNdx].name, "");
            uint32_t bufferMode           = bufferModes[bufferModeNdx].mode;
            arrayGroup->addChild(modeGroup);

            for (int primitiveTypeNdx = 0; primitiveTypeNdx < DE_LENGTH_OF_ARRAY(primitiveTypes); primitiveTypeNdx++)
            {
                tcu::TestCaseGroup *primitiveGroup =
                    new tcu::TestCaseGroup(m_testCtx, primitiveTypes[primitiveTypeNdx].name, "");
                uint32_t primitiveType = primitiveTypes[primitiveTypeNdx].type;
                modeGroup->addChild(primitiveGroup);

                for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(basicTypes); typeNdx++)
                {
                    glu::DataType type = basicTypes[typeNdx];
                    bool isFloat       = glu::getDataTypeScalarType(type) == glu::TYPE_FLOAT;

                    for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); precNdx++)
                    {
                        glu::Precision precision = precisions[precNdx];

                        string name = string(glu::getPrecisionName(precision)) + "_" + glu::getDataTypeName(type);
                        primitiveGroup->addChild(
                            new BasicArrayCase(m_context, name.c_str(), "", bufferMode, primitiveType, type, precision,
                                               isFloat ? INTERPOLATION_SMOOTH : INTERPOLATION_FLAT));
                    }
                }
            }
        }
    }

    // .array_element
    {
        tcu::TestCaseGroup *arrayElemGroup =
            new tcu::TestCaseGroup(m_testCtx, "array_element", "Capturing single array element in TF");
        addChild(arrayElemGroup);

        for (int bufferModeNdx = 0; bufferModeNdx < DE_LENGTH_OF_ARRAY(bufferModes); bufferModeNdx++)
        {
            tcu::TestCaseGroup *modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[bufferModeNdx].name, "");
            uint32_t bufferMode           = bufferModes[bufferModeNdx].mode;
            arrayElemGroup->addChild(modeGroup);

            for (int primitiveTypeNdx = 0; primitiveTypeNdx < DE_LENGTH_OF_ARRAY(primitiveTypes); primitiveTypeNdx++)
            {
                tcu::TestCaseGroup *primitiveGroup =
                    new tcu::TestCaseGroup(m_testCtx, primitiveTypes[primitiveTypeNdx].name, "");
                uint32_t primitiveType = primitiveTypes[primitiveTypeNdx].type;
                modeGroup->addChild(primitiveGroup);

                for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(basicTypes); typeNdx++)
                {
                    glu::DataType type = basicTypes[typeNdx];
                    bool isFloat       = glu::getDataTypeScalarType(type) == glu::TYPE_FLOAT;

                    for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); precNdx++)
                    {
                        glu::Precision precision = precisions[precNdx];

                        string name = string(glu::getPrecisionName(precision)) + "_" + glu::getDataTypeName(type);
                        primitiveGroup->addChild(
                            new ArrayElementCase(m_context, name.c_str(), "", bufferMode, primitiveType, type,
                                                 precision, isFloat ? INTERPOLATION_SMOOTH : INTERPOLATION_FLAT));
                    }
                }
            }
        }
    }

    // .interpolation
    {
        tcu::TestCaseGroup *interpolationGroup = new tcu::TestCaseGroup(
            m_testCtx, "interpolation", "Different interpolation modes in transform feedback varyings");
        addChild(interpolationGroup);

        for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(interpModes); modeNdx++)
        {
            Interpolation interp          = interpModes[modeNdx].interp;
            tcu::TestCaseGroup *modeGroup = new tcu::TestCaseGroup(m_testCtx, interpModes[modeNdx].name, "");

            interpolationGroup->addChild(modeGroup);

            for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); precNdx++)
            {
                glu::Precision precision = precisions[precNdx];

                for (int primitiveType = 0; primitiveType < DE_LENGTH_OF_ARRAY(primitiveTypes); primitiveType++)
                {
                    for (int bufferMode = 0; bufferMode < DE_LENGTH_OF_ARRAY(bufferModes); bufferMode++)
                    {
                        string name = string(glu::getPrecisionName(precision)) + "_vec4_" +
                                      primitiveTypes[primitiveType].name + "_" + bufferModes[bufferMode].name;
                        modeGroup->addChild(new BasicTypeCase(m_context, name.c_str(), "", bufferModes[bufferMode].mode,
                                                              primitiveTypes[primitiveType].type, glu::TYPE_FLOAT_VEC4,
                                                              precision, interp));
                    }
                }
            }
        }
    }

    // .random
    {
        tcu::TestCaseGroup *randomGroup =
            new tcu::TestCaseGroup(m_testCtx, "random", "Randomized transform feedback cases");
        addChild(randomGroup);

        for (int bufferModeNdx = 0; bufferModeNdx < DE_LENGTH_OF_ARRAY(bufferModes); bufferModeNdx++)
        {
            tcu::TestCaseGroup *modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[bufferModeNdx].name, "");
            uint32_t bufferMode           = bufferModes[bufferModeNdx].mode;
            randomGroup->addChild(modeGroup);

            for (int primitiveTypeNdx = 0; primitiveTypeNdx < DE_LENGTH_OF_ARRAY(primitiveTypes); primitiveTypeNdx++)
            {
                tcu::TestCaseGroup *primitiveGroup =
                    new tcu::TestCaseGroup(m_testCtx, primitiveTypes[primitiveTypeNdx].name, "");
                uint32_t primitiveType = primitiveTypes[primitiveTypeNdx].type;
                modeGroup->addChild(primitiveGroup);

                for (int ndx = 0; ndx < 10; ndx++)
                {
                    uint32_t seed = deInt32Hash(bufferMode) ^ deInt32Hash(primitiveType) ^ deInt32Hash(ndx);
                    primitiveGroup->addChild(new RandomCase(m_context, de::toString(ndx + 1).c_str(), "", bufferMode,
                                                            primitiveType, seed, true));
                }
            }
        }
    }

    // .random_full_array_capture
    {
        tcu::TestCaseGroup *randomNecGroup =
            new tcu::TestCaseGroup(m_testCtx, "random_full_array_capture",
                                   "Randomized transform feedback cases without array element capture");
        addChild(randomNecGroup);

        for (int bufferModeNdx = 0; bufferModeNdx < DE_LENGTH_OF_ARRAY(bufferModes); bufferModeNdx++)
        {
            tcu::TestCaseGroup *modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[bufferModeNdx].name, "");
            uint32_t bufferMode           = bufferModes[bufferModeNdx].mode;
            randomNecGroup->addChild(modeGroup);

            for (int primitiveTypeNdx = 0; primitiveTypeNdx < DE_LENGTH_OF_ARRAY(primitiveTypes); primitiveTypeNdx++)
            {
                tcu::TestCaseGroup *primitiveGroup =
                    new tcu::TestCaseGroup(m_testCtx, primitiveTypes[primitiveTypeNdx].name, "");
                uint32_t primitiveType = primitiveTypes[primitiveTypeNdx].type;
                modeGroup->addChild(primitiveGroup);

                for (int ndx = 0; ndx < 10; ndx++)
                {
                    uint32_t seed = deInt32Hash(bufferMode) ^ deInt32Hash(primitiveType) ^ deInt32Hash(ndx);
                    primitiveGroup->addChild(new RandomCase(m_context, de::toString(ndx + 1).c_str(), "", bufferMode,
                                                            primitiveType, seed, false));
                }
            }
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
