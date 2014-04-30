//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef LIBGLESV2_TRANSFORM_FEEDBACK_H_
#define LIBGLESV2_TRANSFORM_FEEDBACK_H_

#include "common/angleutils.h"
#include "common/RefCountObject.h"

#define GL_APICALL
#include <GLES3/gl3.h>
#include <GLES2/gl2.h>

namespace gl
{

class TransformFeedback : public RefCountObject
{
  public:
    explicit TransformFeedback(GLuint id);
    virtual ~TransformFeedback();

    void start(GLenum primitiveMode);
    void stop();
    GLboolean isStarted() const;

    GLenum getDrawMode() const;

    void pause();
    void resume();
    GLboolean isPaused() const;

  private:
    DISALLOW_COPY_AND_ASSIGN(TransformFeedback);

    GLboolean mStarted;
    GLenum mPrimitiveMode;
    GLboolean mPaused;
};

}

#endif // LIBGLESV2_TRANSFORM_FEEDBACK_H_
