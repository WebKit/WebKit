#ifndef _GLSLIFETIMETESTS_HPP
#define _GLSLIFETIMETESTS_HPP
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
 * \brief Common object lifetime tests.
 *//*--------------------------------------------------------------------*/

#include "deRandom.hpp"
#include "deUniquePtr.hpp"
#include "tcuSurface.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestContext.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluRenderContext.hpp"
#include "glwDefs.hpp"
#include "glwEnums.hpp"

#include <vector>

namespace deqp
{
namespace gls
{
namespace LifetimeTests
{
namespace details
{

using de::MovePtr;
using de::Random;
using glu::CallLogWrapper;
using glu::RenderContext;
using std::vector;
using tcu::Surface;
using tcu::TestCaseGroup;
using tcu::TestContext;
using tcu::TestLog;
using namespace glw;

typedef void (CallLogWrapper::*BindFunc)(GLenum target, GLuint name);
typedef void (CallLogWrapper::*GenFunc)(GLsizei n, GLuint *names);
typedef void (CallLogWrapper::*DeleteFunc)(GLsizei n, const GLuint *names);
typedef GLboolean (CallLogWrapper::*ExistsFunc)(GLuint name);

class Context
{
public:
    Context(const RenderContext &renderCtx, TestContext &testCtx) : m_renderCtx(renderCtx), m_testCtx(testCtx)
    {
    }
    const RenderContext &getRenderContext(void) const
    {
        return m_renderCtx;
    }
    TestContext &getTestContext(void) const
    {
        return m_testCtx;
    }
    const Functions &gl(void) const
    {
        return m_renderCtx.getFunctions();
    }
    TestLog &log(void) const
    {
        return m_testCtx.getLog();
    }

private:
    const RenderContext &m_renderCtx;
    TestContext &m_testCtx;
};

class ContextWrapper : public CallLogWrapper
{
public:
    const Context &getContext(void) const
    {
        return m_ctx;
    }
    const RenderContext &getRenderContext(void) const
    {
        return m_ctx.getRenderContext();
    }
    TestContext &getTestContext(void) const
    {
        return m_ctx.getTestContext();
    }
    const Functions &gl(void) const
    {
        return m_ctx.gl();
    }
    TestLog &log(void) const
    {
        return m_ctx.log();
    }
    void enableLogging(bool enable)
    {
        CallLogWrapper::enableLogging(enable);
    }

protected:
    ContextWrapper(const Context &ctx);
    const Context m_ctx;
};

class Binder : public ContextWrapper
{
public:
    virtual ~Binder(void)
    {
    }
    virtual void bind(GLuint name)  = 0;
    virtual GLuint getBinding(void) = 0;
    virtual bool genRequired(void) const
    {
        return true;
    }

protected:
    Binder(const Context &ctx) : ContextWrapper(ctx)
    {
    }
};

class SimpleBinder : public Binder
{
public:
    SimpleBinder(const Context &ctx, BindFunc bindFunc, GLenum bindTarget, GLenum bindingParam,
                 bool genRequired_ = false)
        : Binder(ctx)
        , m_bindFunc(bindFunc)
        , m_bindTarget(bindTarget)
        , m_bindingParam(bindingParam)
        , m_genRequired(genRequired_)
    {
    }

    void bind(GLuint name);
    GLuint getBinding(void);
    bool genRequired(void) const
    {
        return m_genRequired;
    }

private:
    const BindFunc m_bindFunc;
    const GLenum m_bindTarget;
    const GLenum m_bindingParam;
    const bool m_genRequired;
};

class Type : public ContextWrapper
{
public:
    virtual ~Type(void)
    {
    }
    virtual GLuint gen(void)          = 0;
    virtual void release(GLuint name) = 0;
    virtual bool exists(GLuint name)  = 0;
    virtual bool isDeleteFlagged(GLuint name)
    {
        DE_UNREF(name);
        return false;
    }
    virtual Binder *binder(void) const
    {
        return nullptr;
    }
    virtual const char *getName(void) const = 0;
    virtual bool nameLingers(void) const
    {
        return false;
    }
    virtual bool genCreates(void) const
    {
        return false;
    }

protected:
    Type(const Context &ctx) : ContextWrapper(ctx)
    {
    }
};

class SimpleType : public Type
{
public:
    SimpleType(const Context &ctx, const char *name, GenFunc genFunc, DeleteFunc deleteFunc, ExistsFunc existsFunc,
               Binder *binder_ = nullptr, bool genCreates_ = false)
        : Type(ctx)
        , m_getName(name)
        , m_genFunc(genFunc)
        , m_deleteFunc(deleteFunc)
        , m_existsFunc(existsFunc)
        , m_binder(binder_)
        , m_genCreates(genCreates_)
    {
    }

    GLuint gen(void);
    void release(GLuint name)
    {
        (this->*m_deleteFunc)(1, &name);
    }
    bool exists(GLuint name)
    {
        return (this->*m_existsFunc)(name) != GL_FALSE;
    }
    Binder *binder(void) const
    {
        return m_binder;
    }
    const char *getName(void) const
    {
        return m_getName;
    }
    bool nameLingers(void) const
    {
        return false;
    }
    bool genCreates(void) const
    {
        return m_genCreates;
    }

private:
    const char *const m_getName;
    const GenFunc m_genFunc;
    const DeleteFunc m_deleteFunc;
    const ExistsFunc m_existsFunc;
    Binder *const m_binder;
    const bool m_genCreates;
};

class ProgramType : public Type
{
public:
    ProgramType(const Context &ctx) : Type(ctx)
    {
    }
    bool nameLingers(void) const
    {
        return true;
    }
    bool genCreates(void) const
    {
        return true;
    }
    const char *getName(void) const
    {
        return "program";
    }
    GLuint gen(void)
    {
        return glCreateProgram();
    }
    void release(GLuint name)
    {
        glDeleteProgram(name);
    }
    bool exists(GLuint name)
    {
        return glIsProgram(name) != GL_FALSE;
    }
    bool isDeleteFlagged(GLuint name);
};

class ShaderType : public Type
{
public:
    ShaderType(const Context &ctx) : Type(ctx)
    {
    }
    bool nameLingers(void) const
    {
        return true;
    }
    bool genCreates(void) const
    {
        return true;
    }
    const char *getName(void) const
    {
        return "shader";
    }
    GLuint gen(void)
    {
        return glCreateShader(GL_FRAGMENT_SHADER);
    }
    void release(GLuint name)
    {
        glDeleteShader(name);
    }
    bool exists(GLuint name)
    {
        return glIsShader(name) != GL_FALSE;
    }
    bool isDeleteFlagged(GLuint name);
};

class Attacher : public ContextWrapper
{
public:
    virtual void initAttachment(GLuint seed, GLuint attachment) = 0;
    virtual void attach(GLuint element, GLuint container)       = 0;
    virtual void detach(GLuint element, GLuint container)       = 0;
    virtual GLuint getAttachment(GLuint container)              = 0;
    virtual bool canAttachDeleted(void) const
    {
        return true;
    }

    Type &getElementType(void) const
    {
        return m_elementType;
    }
    Type &getContainerType(void) const
    {
        return m_containerType;
    }
    virtual ~Attacher(void)
    {
    }

protected:
    Attacher(const Context &ctx, Type &elementType, Type &containerType)
        : ContextWrapper(ctx)
        , m_elementType(elementType)
        , m_containerType(containerType)
    {
    }

private:
    Type &m_elementType;
    Type &m_containerType;
};

class InputAttacher : public ContextWrapper
{
public:
    Attacher &getAttacher(void) const
    {
        return m_attacher;
    }
    virtual void drawContainer(GLuint container, Surface &dst) = 0;

protected:
    InputAttacher(Attacher &attacher) : ContextWrapper(attacher.getContext()), m_attacher(attacher)
    {
    }
    Attacher &m_attacher;
};

class OutputAttacher : public ContextWrapper
{
public:
    Attacher &getAttacher(void) const
    {
        return m_attacher;
    }
    virtual void setupContainer(GLuint seed, GLuint container)   = 0;
    virtual void drawAttachment(GLuint attachment, Surface &dst) = 0;

protected:
    OutputAttacher(Attacher &attacher) : ContextWrapper(attacher.getContext()), m_attacher(attacher)
    {
    }
    Attacher &m_attacher;
};

class Types : public ContextWrapper
{
public:
    Types(const Context &ctx) : ContextWrapper(ctx)
    {
    }
    virtual Type &getProgramType(void) = 0;
    const vector<Type *> &getTypes(void)
    {
        return m_types;
    }
    const vector<Attacher *> &getAttachers(void)
    {
        return m_attachers;
    }
    const vector<InputAttacher *> &getInputAttachers(void)
    {
        return m_inAttachers;
    }
    const vector<OutputAttacher *> &getOutputAttachers(void)
    {
        return m_outAttachers;
    }
    virtual ~Types(void)
    {
    }

protected:
    vector<Type *> m_types;
    vector<Attacher *> m_attachers;
    vector<InputAttacher *> m_inAttachers;
    vector<OutputAttacher *> m_outAttachers;
};

class FboAttacher : public Attacher
{
public:
    void initAttachment(GLuint seed, GLuint element);

protected:
    FboAttacher(const Context &ctx, Type &elementType, Type &containerType) : Attacher(ctx, elementType, containerType)
    {
    }
    virtual void initStorage(void) = 0;
};

class FboInputAttacher : public InputAttacher
{
public:
    FboInputAttacher(FboAttacher &attacher) : InputAttacher(attacher)
    {
    }
    void drawContainer(GLuint container, Surface &dst);
};

class FboOutputAttacher : public OutputAttacher
{
public:
    FboOutputAttacher(FboAttacher &attacher) : OutputAttacher(attacher)
    {
    }
    void setupContainer(GLuint seed, GLuint container);
    void drawAttachment(GLuint attachment, Surface &dst);
};

class TextureFboAttacher : public FboAttacher
{
public:
    TextureFboAttacher(const Context &ctx, Type &elementType, Type &containerType)
        : FboAttacher(ctx, elementType, containerType)
    {
    }

    void initStorage(void);
    void attach(GLuint element, GLuint container);
    void detach(GLuint element, GLuint container);
    GLuint getAttachment(GLuint container);
};

class RboFboAttacher : public FboAttacher
{
public:
    RboFboAttacher(const Context &ctx, Type &elementType, Type &containerType)
        : FboAttacher(ctx, elementType, containerType)
    {
    }

    void initStorage(void);
    void attach(GLuint element, GLuint container);
    void detach(GLuint element, GLuint container);
    GLuint getAttachment(GLuint container);
};

class ShaderProgramAttacher : public Attacher
{
public:
    ShaderProgramAttacher(const Context &ctx, Type &elementType, Type &containerType)
        : Attacher(ctx, elementType, containerType)
    {
    }

    void initAttachment(GLuint seed, GLuint element);
    void attach(GLuint element, GLuint container);
    void detach(GLuint element, GLuint container);
    GLuint getAttachment(GLuint container);
};

class ShaderProgramInputAttacher : public InputAttacher
{
public:
    ShaderProgramInputAttacher(Attacher &attacher) : InputAttacher(attacher)
    {
    }

    void drawContainer(GLuint container, Surface &dst);
};

class ES2Types : public Types
{
public:
    ES2Types(const Context &ctx);
    Type &getProgramType(void)
    {
        return m_programType;
    }

protected:
    SimpleBinder m_bufferBind;
    SimpleType m_bufferType;
    SimpleBinder m_textureBind;
    SimpleType m_textureType;
    SimpleBinder m_rboBind;
    SimpleType m_rboType;
    SimpleBinder m_fboBind;
    SimpleType m_fboType;
    ShaderType m_shaderType;
    ProgramType m_programType;
    TextureFboAttacher m_texFboAtt;
    FboInputAttacher m_texFboInAtt;
    FboOutputAttacher m_texFboOutAtt;
    RboFboAttacher m_rboFboAtt;
    FboInputAttacher m_rboFboInAtt;
    FboOutputAttacher m_rboFboOutAtt;
    ShaderProgramAttacher m_shaderAtt;
    ShaderProgramInputAttacher m_shaderInAtt;
};

MovePtr<TestCaseGroup> createGroup(TestContext &testCtx, Type &type);
void addTestCases(TestCaseGroup &group, Types &types);

struct Rectangle
{
    Rectangle(GLint x_, GLint y_, GLint width_, GLint height_) : x(x_), y(y_), width(width_), height(height_)
    {
    }
    GLint x;
    GLint y;
    GLint width;
    GLint height;
};

Rectangle randomViewport(const RenderContext &ctx, GLint maxWidth, GLint maxHeight, Random &rnd);
void setViewport(const RenderContext &renderCtx, const Rectangle &rect);
void readRectangle(const RenderContext &renderCtx, const Rectangle &rect, Surface &dst);

} // namespace details

using details::BindFunc;
using details::DeleteFunc;
using details::ExistsFunc;
using details::GenFunc;

using details::Attacher;
using details::Binder;
using details::Context;
using details::ES2Types;
using details::InputAttacher;
using details::OutputAttacher;
using details::SimpleBinder;
using details::SimpleType;
using details::Type;
using details::Types;

using details::addTestCases;
using details::createGroup;

using details::randomViewport;
using details::readRectangle;
using details::Rectangle;
using details::setViewport;

} // namespace LifetimeTests
} // namespace gls
} // namespace deqp

#endif // _GLSLIFETIMETESTS_HPP
