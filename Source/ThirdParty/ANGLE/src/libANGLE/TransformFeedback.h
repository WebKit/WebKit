//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef LIBANGLE_TRANSFORM_FEEDBACK_H_
#define LIBANGLE_TRANSFORM_FEEDBACK_H_

#include "libANGLE/RefCountObject.h"

#include "common/angleutils.h"
#include "libANGLE/Debug.h"

#include "angle_gl.h"

namespace rx
{
class GLImplFactory;
class TransformFeedbackImpl;
}

namespace gl
{
class Buffer;
struct Caps;
class Context;
class Program;

class TransformFeedbackState final : angle::NonCopyable
{
  public:
    TransformFeedbackState(size_t maxIndexedBuffers);
    ~TransformFeedbackState();

    const BindingPointer<Buffer> &getGenericBuffer() const;
    const OffsetBindingPointer<Buffer> &getIndexedBuffer(size_t idx) const;
    const std::vector<OffsetBindingPointer<Buffer>> &getIndexedBuffers() const;

  private:
    friend class TransformFeedback;

    std::string mLabel;

    bool mActive;
    GLenum mPrimitiveMode;
    bool mPaused;

    Program *mProgram;

    BindingPointer<Buffer> mGenericBuffer;
    std::vector<OffsetBindingPointer<Buffer>> mIndexedBuffers;
};

class TransformFeedback final : public RefCountObject, public LabeledObject
{
  public:
    TransformFeedback(rx::GLImplFactory *implFactory, GLuint id, const Caps &caps);
    ~TransformFeedback() override;
    Error onDestroy(const Context *context) override;

    void setLabel(const std::string &label) override;
    const std::string &getLabel() const override;

    void begin(const Context *context, GLenum primitiveMode, Program *program);
    void end(const Context *context);
    void pause();
    void resume();

    bool isActive() const;
    bool isPaused() const;
    GLenum getPrimitiveMode() const;

    bool hasBoundProgram(GLuint program) const;

    void bindGenericBuffer(const Context *context, Buffer *buffer);
    const BindingPointer<Buffer> &getGenericBuffer() const;

    void bindIndexedBuffer(const Context *context,
                           size_t index,
                           Buffer *buffer,
                           size_t offset,
                           size_t size);
    const OffsetBindingPointer<Buffer> &getIndexedBuffer(size_t index) const;
    size_t getIndexedBufferCount() const;

    void detachBuffer(const Context *context, GLuint bufferName);

    rx::TransformFeedbackImpl *getImplementation();
    const rx::TransformFeedbackImpl *getImplementation() const;

  private:
    void bindProgram(const Context *context, Program *program);

    TransformFeedbackState mState;
    rx::TransformFeedbackImpl* mImplementation;
};

}

#endif // LIBANGLE_TRANSFORM_FEEDBACK_H_
