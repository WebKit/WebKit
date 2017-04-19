//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// QueryGL.h: Defines the class interface for QueryGL.

#ifndef LIBANGLE_RENDERER_GL_QUERYGL_H_
#define LIBANGLE_RENDERER_GL_QUERYGL_H_

#include <deque>

#include "libANGLE/renderer/QueryImpl.h"

namespace rx
{

class FunctionsGL;
class StateManagerGL;

class QueryGL : public QueryImpl
{
  public:
    QueryGL(GLenum type);
    ~QueryGL() override;

    // OpenGL is only allowed to have one query of each type active at any given time. Since ANGLE
    // virtualizes contexts, queries need to be able to be paused and resumed.
    // A query is "paused" by ending it and pushing the ID into a list of queries awaiting readback.
    // When it is "resumed", a new query is generated and started.
    // When a result is required, the queries are "flushed" by iterating over the list of pending
    // queries and merging their results.
    virtual gl::Error pause()  = 0;
    virtual gl::Error resume() = 0;
};

class StandardQueryGL : public QueryGL
{
  public:
    StandardQueryGL(GLenum type, const FunctionsGL *functions, StateManagerGL *stateManager);
    ~StandardQueryGL() override;

    gl::Error begin() override;
    gl::Error end() override;
    gl::Error queryCounter() override;
    gl::Error getResult(GLint *params) override;
    gl::Error getResult(GLuint *params) override;
    gl::Error getResult(GLint64 *params) override;
    gl::Error getResult(GLuint64 *params) override;
    gl::Error isResultAvailable(bool *available) override;

    gl::Error pause() override;
    gl::Error resume() override;

  private:
    gl::Error flush(bool force);

    template <typename T>
    gl::Error getResultBase(T *params);

    GLenum mType;

    const FunctionsGL *mFunctions;
    StateManagerGL *mStateManager;

    GLuint mActiveQuery;
    std::deque<GLuint> mPendingQueries;
    GLuint64 mResultSum;
};

class SyncProviderGL;
class SyncQueryGL : public QueryGL
{
  public:
    SyncQueryGL(GLenum type, const FunctionsGL *functions, StateManagerGL *stateManager);
    ~SyncQueryGL() override;

    static bool IsSupported(const FunctionsGL *functions);

    gl::Error begin() override;
    gl::Error end() override;
    gl::Error queryCounter() override;
    gl::Error getResult(GLint *params) override;
    gl::Error getResult(GLuint *params) override;
    gl::Error getResult(GLint64 *params) override;
    gl::Error getResult(GLuint64 *params) override;
    gl::Error isResultAvailable(bool *available) override;

    gl::Error pause() override;
    gl::Error resume() override;

  private:
    gl::Error flush(bool force);

    template <typename T>
    gl::Error getResultBase(T *params);

    const FunctionsGL *mFunctions;
    StateManagerGL *mStateManager;

    std::unique_ptr<SyncProviderGL> mSyncProvider;
    bool mFinished;
};
}

#endif // LIBANGLE_RENDERER_GL_QUERYGL_H_
