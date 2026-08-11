#ifndef _GLSATTRIBUTELOCATIONTESTS_HPP
#define _GLSATTRIBUTELOCATIONTESTS_HPP
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
 * \brief Attribute location tests
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

#include <string>
#include <vector>

namespace glu
{
class ShaderProgram;
class RenderContext;
} // namespace glu

namespace deqp
{
namespace gls
{
namespace AttributeLocationTestUtil
{

class AttribType
{
public:
    AttribType(const std::string &name, uint32_t locationSize, uint32_t typeEnum);

    const std::string &getName(void) const
    {
        return m_name;
    }
    uint32_t getLocationSize(void) const
    {
        return m_locationSize;
    }
    uint32_t getGLTypeEnum(void) const
    {
        return m_glTypeEnum;
    }

private:
    std::string m_name;
    uint32_t m_locationSize;
    uint32_t m_glTypeEnum;
};

class Cond
{
public:
    enum ConstCond
    {
        COND_ALWAYS,
        COND_NEVER
    };

    Cond(ConstCond cond);
    explicit Cond(const std::string &name, bool negate = true);
    bool operator==(const Cond &other) const
    {
        return m_negate == other.m_negate && m_name == other.m_name;
    }
    bool operator!=(const Cond &other) const
    {
        return !(*this == other);
    }
    const std::string getName(void) const
    {
        return m_name;
    }
    bool getNegate(void) const
    {
        return m_negate;
    }

private:
    bool m_negate;
    std::string m_name;
};

class Attribute
{
public:
    enum
    {
        // Location is not defined
        LOC_UNDEF = -1
    };

    enum
    {
        // Not an array
        NOT_ARRAY = -1
    };

    Attribute(const AttribType &type, const std::string &name, int32_t layoutLocation = LOC_UNDEF,
              const Cond &cond = Cond::COND_ALWAYS, int arraySize = NOT_ARRAY);

    const AttribType getType(void) const
    {
        return m_type;
    }
    const std::string &getName(void) const
    {
        return m_name;
    }
    int32_t getLayoutLocation(void) const
    {
        return m_layoutLocation;
    }
    const Cond &getCondition(void) const
    {
        return m_cond;
    }
    int getArraySize(void) const
    {
        return m_arraySize;
    }

private:
    AttribType m_type;
    std::string m_name;
    int32_t m_layoutLocation;
    Cond m_cond;
    int m_arraySize;
};

class Bind
{
public:
    Bind(const std::string &attribute, uint32_t location);

    const std::string &getAttributeName(void) const
    {
        return m_attribute;
    }
    uint32_t getLocation(void) const
    {
        return m_location;
    }

private:
    std::string m_attribute;
    uint32_t m_location;
};

} // namespace AttributeLocationTestUtil

// Simple bind attribute test
class BindAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    BindAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                      int arraySize = AttributeLocationTestUtil::Attribute::NOT_ARRAY);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
    const AttribType m_type;
    const int m_arraySize;
};

// Bind maximum number of attributes
class BindMaxAttributesTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    BindMaxAttributesTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                          int arraySize = AttributeLocationTestUtil::Attribute::NOT_ARRAY);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
    const AttribType m_type;
    const int m_arraySize;
};

class BindAliasingAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    BindAliasingAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                              int offset = 0, int arraySize = AttributeLocationTestUtil::Attribute::NOT_ARRAY);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
    const AttribType m_type;
    const int m_offset;
    const int m_arraySize;
};

class BindMaxAliasingAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    BindMaxAliasingAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                                 int arraySize = AttributeLocationTestUtil::Attribute::NOT_ARRAY);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
    const AttribType m_type;
    const int m_arraySize;
};

class BindInactiveAliasingAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    BindInactiveAliasingAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                                      int arraySize = AttributeLocationTestUtil::Attribute::NOT_ARRAY);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
    const AttribType m_type;
    const int m_arraySize;
};

class BindHoleAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    BindHoleAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                          int arraySize = AttributeLocationTestUtil::Attribute::NOT_ARRAY);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
    const AttribType m_type;
    const int m_arraySize;
};

class PreAttachBindAttributeTest : public tcu::TestCase
{
public:
    PreAttachBindAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
};

class PreLinkBindAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    PreLinkBindAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
};

class PostLinkBindAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    PostLinkBindAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
};

class BindReattachAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    BindReattachAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
};

class LocationAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    LocationAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                          int arraySize = AttributeLocationTestUtil::Attribute::NOT_ARRAY);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
    const AttribType m_type;
    const int m_arraySize;
};

class LocationMaxAttributesTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    LocationMaxAttributesTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                              int arraySize = AttributeLocationTestUtil::Attribute::NOT_ARRAY);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
    const AttribType m_type;
    const int m_arraySize;
};

class LocationHoleAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    LocationHoleAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                              int arraySize = AttributeLocationTestUtil::Attribute::NOT_ARRAY);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
    const AttribType m_type;
    const int m_arraySize;
};

class MixedAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    MixedAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                       int arraySize = AttributeLocationTestUtil::Attribute::NOT_ARRAY);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
    const AttribType m_type;
    const int m_arraySize;
};

class MixedMaxAttributesTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    MixedMaxAttributesTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                           int arraySize = AttributeLocationTestUtil::Attribute::NOT_ARRAY);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
    const AttribType m_type;
    const int m_arraySize;
};

class MixedHoleAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    MixedHoleAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                           int arraySize = AttributeLocationTestUtil::Attribute::NOT_ARRAY);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
    const AttribType m_type;
    const int m_arraySize;
};

class BindRelinkAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    BindRelinkAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
};

class BindRelinkHoleAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    BindRelinkHoleAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                                int arraySize = AttributeLocationTestUtil::Attribute::NOT_ARRAY);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
    const AttribType m_type;
    const int m_arraySize;
};

class MixedRelinkHoleAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    MixedRelinkHoleAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const AttribType &type,
                                 int arraySize = AttributeLocationTestUtil::Attribute::NOT_ARRAY);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
    const AttribType m_type;
    const int m_arraySize;
};

class PreAttachMixedAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    PreAttachMixedAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
};

class PreLinkMixedAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    PreLinkMixedAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
};

class PostLinkMixedAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    PostLinkMixedAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
};

class MixedReattachAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    MixedReattachAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
};

class MixedRelinkAttributeTest : public tcu::TestCase
{
public:
    typedef AttributeLocationTestUtil::AttribType AttribType;

    MixedRelinkAttributeTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx);

    virtual IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
};

} // namespace gls
} // namespace deqp

#endif // _GLSATTRIBUTELOCATIONTESTS_HPP
