#ifndef _GLSDRAWTEST_HPP
#define _GLSDRAWTEST_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
 * \brief Draw tests
 *//*--------------------------------------------------------------------*/

#include "tcuTestCase.hpp"
#include "tcuResultCollector.hpp"
#include "gluRenderContext.hpp"

namespace glu
{
class ContextInfo;
}

namespace sglr
{

class ReferenceContextBuffers;
class ReferenceContext;
class Context;

} // namespace sglr

namespace deqp
{
namespace gls
{

class AttributePack;

struct DrawTestSpec
{
    enum Target
    {
        TARGET_ELEMENT_ARRAY = 0,
        TARGET_ARRAY,

        TARGET_LAST
    };

    enum InputType
    {
        INPUTTYPE_FLOAT = 0,
        INPUTTYPE_FIXED,
        INPUTTYPE_DOUBLE,

        INPUTTYPE_BYTE,
        INPUTTYPE_SHORT,

        INPUTTYPE_UNSIGNED_BYTE,
        INPUTTYPE_UNSIGNED_SHORT,

        INPUTTYPE_INT,
        INPUTTYPE_UNSIGNED_INT,
        INPUTTYPE_HALF,
        INPUTTYPE_UNSIGNED_INT_2_10_10_10,
        INPUTTYPE_INT_2_10_10_10,

        INPUTTYPE_LAST
    };

    enum OutputType
    {
        OUTPUTTYPE_FLOAT = 0,
        OUTPUTTYPE_VEC2,
        OUTPUTTYPE_VEC3,
        OUTPUTTYPE_VEC4,

        OUTPUTTYPE_INT,
        OUTPUTTYPE_UINT,

        OUTPUTTYPE_IVEC2,
        OUTPUTTYPE_IVEC3,
        OUTPUTTYPE_IVEC4,

        OUTPUTTYPE_UVEC2,
        OUTPUTTYPE_UVEC3,
        OUTPUTTYPE_UVEC4,

        OUTPUTTYPE_LAST
    };

    enum Usage
    {
        USAGE_DYNAMIC_DRAW = 0,
        USAGE_STATIC_DRAW,
        USAGE_STREAM_DRAW,

        USAGE_STREAM_READ,
        USAGE_STREAM_COPY,

        USAGE_STATIC_READ,
        USAGE_STATIC_COPY,

        USAGE_DYNAMIC_READ,
        USAGE_DYNAMIC_COPY,

        USAGE_LAST
    };

    enum Storage
    {
        STORAGE_USER = 0,
        STORAGE_BUFFER,

        STORAGE_LAST
    };

    enum Primitive
    {
        PRIMITIVE_POINTS = 0,
        PRIMITIVE_TRIANGLES,
        PRIMITIVE_TRIANGLE_FAN,
        PRIMITIVE_TRIANGLE_STRIP,
        PRIMITIVE_LINES,
        PRIMITIVE_LINE_STRIP,
        PRIMITIVE_LINE_LOOP,

        PRIMITIVE_LINES_ADJACENCY,
        PRIMITIVE_LINE_STRIP_ADJACENCY,
        PRIMITIVE_TRIANGLES_ADJACENCY,
        PRIMITIVE_TRIANGLE_STRIP_ADJACENCY,

        PRIMITIVE_LAST
    };

    enum IndexType
    {
        INDEXTYPE_BYTE = 0,
        INDEXTYPE_SHORT,
        INDEXTYPE_INT,

        INDEXTYPE_LAST
    };

    enum DrawMethod
    {
        DRAWMETHOD_DRAWARRAYS = 0,
        DRAWMETHOD_DRAWARRAYS_INSTANCED,
        DRAWMETHOD_DRAWARRAYS_INDIRECT,
        DRAWMETHOD_DRAWELEMENTS,
        DRAWMETHOD_DRAWELEMENTS_RANGED,
        DRAWMETHOD_DRAWELEMENTS_INSTANCED,
        DRAWMETHOD_DRAWELEMENTS_INDIRECT,
        DRAWMETHOD_DRAWELEMENTS_BASEVERTEX,
        DRAWMETHOD_DRAWELEMENTS_INSTANCED_BASEVERTEX,
        DRAWMETHOD_DRAWELEMENTS_RANGED_BASEVERTEX,

        DRAWMETHOD_LAST
    };

    enum CompatibilityTestType
    {
        COMPATIBILITY_NONE = 0,
        COMPATIBILITY_UNALIGNED_OFFSET,
        COMPATIBILITY_UNALIGNED_STRIDE,

        COMPATIBILITY_LAST
    };

    static std::string targetToString(Target target);
    static std::string inputTypeToString(InputType type);
    static std::string outputTypeToString(OutputType type);
    static std::string usageTypeToString(Usage usage);
    static std::string storageToString(Storage storage);
    static std::string primitiveToString(Primitive primitive);
    static std::string indexTypeToString(IndexType type);
    static std::string drawMethodToString(DrawMethod method);
    static int inputTypeSize(InputType type);
    static int indexTypeSize(IndexType type);

    struct AttributeSpec
    {
        static AttributeSpec createAttributeArray(InputType inputType, OutputType outputType, Storage storage,
                                                  Usage usage, int componentCount, int offset, int stride,
                                                  bool normalize, int instanceDivisor);
        static AttributeSpec createDefaultAttribute(
            InputType inputType, OutputType outputType,
            int componentCount); //!< allowed inputType values: INPUTTYPE_INT, INPUTTYPE_UNSIGNED_INT, INPUTTYPE_FLOAT

        InputType inputType;
        OutputType outputType;
        Storage storage;
        Usage usage;
        int componentCount;
        int offset;
        int stride;
        bool normalize;
        int instanceDivisor; //!< used only if drawMethod = Draw*Instanced
        bool useDefaultAttribute;

        bool
            additionalPositionAttribute; //!< treat this attribute as position attribute. Attribute at index 0 is alway treated as such. False by default
        bool
            bgraComponentOrder; //!< component order of this attribute is bgra, valid only for 4-component targets. False by default.

        AttributeSpec(void);

        int hash(void) const;
        bool valid(glu::ApiType apiType) const;
        bool isBufferAligned(void) const;
        bool isBufferStrideAligned(void) const;
    };

    std::string getName(void) const;
    std::string getDesc(void) const;
    std::string getMultilineDesc(void) const;

    glu::ApiType apiType; //!< needed in spec validation
    Primitive primitive;
    int primitiveCount; //!< number of primitives to draw (per instance)

    DrawMethod drawMethod;
    IndexType indexType;    //!< used only if drawMethod = DrawElements*
    int indexPointerOffset; //!< used only if drawMethod = DrawElements*
    Storage indexStorage;   //!< used only if drawMethod = DrawElements*
    int first;              //!< used only if drawMethod = DrawArrays*
    int indexMin;           //!< used only if drawMethod = Draw*Ranged
    int indexMax;           //!< used only if drawMethod = Draw*Ranged
    int instanceCount;      //!< used only if drawMethod = Draw*Instanced or Draw*Indirect
    int indirectOffset;     //!< used only if drawMethod = Draw*Indirect
    int baseVertex;         //!< used only if drawMethod = DrawElementsIndirect or *BaseVertex

    std::vector<AttributeSpec> attribs;

    DrawTestSpec(void);

    int hash(void) const;
    bool valid(void) const;
    CompatibilityTestType isCompatibilityTest(void) const;
};

class DrawTest : public tcu::TestCase
{
public:
    DrawTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const DrawTestSpec &spec, const char *name,
             const char *desc);
    DrawTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name, const char *desc);
    virtual ~DrawTest(void);

    void addIteration(const DrawTestSpec &spec, const char *description = nullptr);

private:
    void init(void);
    void deinit(void);
    IterateResult iterate(void);

    bool compare(gls::DrawTestSpec::Primitive primitiveType);
    float getCoordScale(const DrawTestSpec &spec) const;
    float getColorScale(const DrawTestSpec &spec) const;

    glu::RenderContext &m_renderCtx;

    glu::ContextInfo *m_contextInfo;
    sglr::ReferenceContextBuffers *m_refBuffers;
    sglr::ReferenceContext *m_refContext;
    sglr::Context *m_glesContext;

    AttributePack *m_glArrayPack;
    AttributePack *m_rrArrayPack;

    int m_maxDiffRed;
    int m_maxDiffGreen;
    int m_maxDiffBlue;

    std::vector<DrawTestSpec> m_specs;
    std::vector<std::string> m_iteration_descriptions;
    int m_iteration;
    tcu::ResultCollector m_result;
};

} // namespace gls
} // namespace deqp

#endif // _GLSDRAWTEST_HPP
