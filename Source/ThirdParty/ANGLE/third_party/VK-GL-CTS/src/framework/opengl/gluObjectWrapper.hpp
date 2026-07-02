#ifndef _GLUOBJECTWRAPPER_HPP
#define _GLUOBJECTWRAPPER_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief API object wrapper.
 *//*--------------------------------------------------------------------*/

#include "gluDefs.hpp"
#include "gluRenderContext.hpp"
#include "glwFunctions.hpp"

#include <vector>

namespace glu
{

/*--------------------------------------------------------------------*//*!
 * \brief API object type.
 *
 * API object type is used to choose allocation and deallocation routines
 * for objects.
 *//*--------------------------------------------------------------------*/
enum ObjectType
{
    OBJECTTYPE_TEXTURE = 0,
    OBJECTTYPE_BUFFER,
    OBJECTTYPE_RENDERBUFFER,
    OBJECTTYPE_FRAMEBUFFER,
    OBJECTTYPE_TRANSFORM_FEEDBACK,
    OBJECTTYPE_VERTEX_ARRAY,
    OBJECTTYPE_QUERY,
    OBJECTTYPE_SAMPLER,

    OBJECTTYPE_LAST
};

struct ObjectTraits
{
    const char *name;
    glw::glGenBuffersFunc glw::Functions::*genFunc;
    glw::glDeleteBuffersFunc glw::Functions::*deleteFunc;
};

const ObjectTraits &objectTraits(ObjectType type);

class ObjectWrapper
{
public:
    ObjectWrapper(const glw::Functions &gl, const ObjectTraits &traits);
    ObjectWrapper(const glw::Functions &gl, const ObjectTraits &traits, uint32_t object);
    ~ObjectWrapper(void);

    inline uint32_t get(void) const
    {
        return m_object;
    }
    inline uint32_t operator*(void) const
    {
        return m_object;
    }

protected:
    const glw::Functions &m_gl;
    const ObjectTraits &m_traits;
    uint32_t m_object;

private:
    ObjectWrapper(const ObjectWrapper &other);
    ObjectWrapper &operator=(const ObjectWrapper &other);
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief API object wrapper template.
 *//*--------------------------------------------------------------------*/
template <ObjectType Type>
class TypedObjectWrapper : public ObjectWrapper
{
public:
    TypedObjectWrapper(const glw::Functions &gl, uint32_t object) : ObjectWrapper(gl, objectTraits(Type), object)
    {
    }
    explicit TypedObjectWrapper(const RenderContext &context)
        : ObjectWrapper(context.getFunctions(), objectTraits(Type))
    {
    }
    explicit TypedObjectWrapper(const glw::Functions &gl) : ObjectWrapper(gl, objectTraits(Type))
    {
    }
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief API object vector.
 *//*--------------------------------------------------------------------*/
class ObjectVector
{
public:
    ObjectVector(const glw::Functions &gl, const ObjectTraits &traits, size_t numObjects = 0);
    ~ObjectVector(void);

    size_t size(void) const
    {
        return m_objects.size();
    }

    void resize(size_t newSize);
    void clear(void);

    bool empty(void) const
    {
        return m_objects.empty();
    }

    uint32_t get(size_t ndx) const
    {
        return m_objects[ndx];
    }
    uint32_t operator[](size_t ndx) const
    {
        return get(ndx);
    }

private:
    ObjectVector(const ObjectVector &other);
    ObjectVector &operator=(const ObjectVector &other);

    const glw::Functions &m_gl;
    const ObjectTraits &m_traits;
    std::vector<uint32_t> m_objects;
} DE_WARN_UNUSED_TYPE;

template <ObjectType Type>
class TypedObjectVector : public ObjectVector
{
public:
    explicit TypedObjectVector(const RenderContext &context, size_t numObjects = 0)
        : ObjectVector(context.getFunctions(), objectTraits(Type), numObjects)
    {
    }
    explicit TypedObjectVector(const glw::Functions &gl, size_t numObjects = 0)
        : ObjectVector(gl, objectTraits(Type), numObjects)
    {
    }
};

// Typedefs for simple wrappers without functionality.

typedef TypedObjectWrapper<OBJECTTYPE_TEXTURE> Texture;
typedef TypedObjectWrapper<OBJECTTYPE_BUFFER> Buffer;
typedef TypedObjectWrapper<OBJECTTYPE_RENDERBUFFER> Renderbuffer;
typedef TypedObjectWrapper<OBJECTTYPE_FRAMEBUFFER> Framebuffer;
typedef TypedObjectWrapper<OBJECTTYPE_TRANSFORM_FEEDBACK> TransformFeedback;
typedef TypedObjectWrapper<OBJECTTYPE_VERTEX_ARRAY> VertexArray;
typedef TypedObjectWrapper<OBJECTTYPE_QUERY> Query;
typedef TypedObjectWrapper<OBJECTTYPE_SAMPLER> Sampler;

typedef TypedObjectVector<OBJECTTYPE_TEXTURE> TextureVector;
typedef TypedObjectVector<OBJECTTYPE_BUFFER> BufferVector;
typedef TypedObjectVector<OBJECTTYPE_RENDERBUFFER> RenderbufferVector;

} // namespace glu

#endif // _GLUOBJECTWRAPPER_HPP
