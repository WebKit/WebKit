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
 * \brief Buffer object wrapper.
 *//*--------------------------------------------------------------------*/

#include "gluObjectWrapper.hpp"
#include "gluRenderContext.hpp"
#include "gluStrUtil.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "deArrayUtil.hpp"

#include <sstream>

namespace glu
{

ObjectWrapper::ObjectWrapper(const glw::Functions &gl, const ObjectTraits &traits)
    : m_gl(gl)
    , m_traits(traits)
    , m_object(0)
{
    (gl.*traits.genFunc)(1, &m_object);

    if (m_object == 0)
    {
        const uint32_t err     = gl.getError();
        const char *objectName = traits.name;
        std::ostringstream msg;

        msg << "Failed to create " << objectName << " object, got " << getErrorStr((int)err);

        if (err == GL_OUT_OF_MEMORY)
            throw OutOfMemoryError(msg.str());
        else
            throw Error((int)err, msg.str());
    }
}

ObjectWrapper::ObjectWrapper(const glw::Functions &gl, const ObjectTraits &traits, uint32_t object)
    : m_gl(gl)
    , m_traits(traits)
    , m_object(object)
{
    DE_ASSERT(object != 0);
}

ObjectWrapper::~ObjectWrapper(void)
{
    (m_gl.*m_traits.deleteFunc)(1, &m_object);
}

static const ObjectTraits s_objectTraits[OBJECTTYPE_LAST] = {
    {"texture", &glw::Functions::genTextures, &glw::Functions::deleteTextures},
    {"buffer", &glw::Functions::genBuffers, &glw::Functions::deleteBuffers},
    {"renderbuffer", &glw::Functions::genRenderbuffers, &glw::Functions::deleteRenderbuffers},
    {"framebuffer", &glw::Functions::genFramebuffers, &glw::Functions::deleteFramebuffers},
    {"transform feedback", &glw::Functions::genTransformFeedbacks, &glw::Functions::deleteTransformFeedbacks},
    {"vertex array", &glw::Functions::genVertexArrays, &glw::Functions::deleteVertexArrays},
    {"query", &glw::Functions::genQueries, &glw::Functions::deleteQueries},
    {"sampler", &glw::Functions::genSamplers, &glw::Functions::deleteSamplers},
};

const ObjectTraits &objectTraits(ObjectType type)
{
    return de::getSizedArrayElement<OBJECTTYPE_LAST>(s_objectTraits, type);
}

ObjectVector::ObjectVector(const glw::Functions &gl, const ObjectTraits &traits, size_t numObjects)
    : m_gl(gl)
    , m_traits(traits)
{
    if (numObjects > 0)
        resize(numObjects);
}

ObjectVector::~ObjectVector(void)
{
    clear();
}

void ObjectVector::resize(size_t newSize)
{
    const size_t oldSize = m_objects.size();

    if (newSize == 0)
    {
        clear(); // Avoid size_t (unsigned) overflow issues in delete path.
    }
    if (oldSize < newSize)
    {
        m_objects.resize(newSize, 0);
        (m_gl.*m_traits.genFunc)(glw::GLsizei(newSize - oldSize), &m_objects[oldSize]);
    }
    else if (oldSize > newSize)
    {
        (m_gl.*m_traits.deleteFunc)(glw::GLsizei(oldSize - newSize), &m_objects[newSize]);
        m_objects.resize(newSize);
    }
}

void ObjectVector::clear(void)
{
    (m_gl.*m_traits.deleteFunc)(glw::GLsizei(m_objects.size()), &m_objects.front());
    m_objects.clear();
}

} // namespace glu
