//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query11.h: Defines the rx::Query11 class which implements rx::QueryImpl.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_QUERY11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_QUERY11_H_

#include <deque>

#include "libANGLE/renderer/QueryImpl.h"

namespace rx
{
class Renderer11;

class Query11 : public QueryImpl
{
  public:
    Query11(Renderer11 *renderer, GLenum type);
    ~Query11() override;

    gl::Error begin() override;
    gl::Error end() override;
    gl::Error queryCounter() override;
    gl::Error getResult(GLint *params) override;
    gl::Error getResult(GLuint *params) override;
    gl::Error getResult(GLint64 *params) override;
    gl::Error getResult(GLuint64 *params) override;
    gl::Error isResultAvailable(bool *available) override;

    gl::Error pause();
    gl::Error resume();

  private:
    struct QueryState final : public angle::NonCopyable
    {
        QueryState();
        ~QueryState();

        ID3D11Query *query;
        ID3D11Query *beginTimestamp;
        ID3D11Query *endTimestamp;
        bool finished;
    };

    gl::Error flush(bool force);
    gl::Error testQuery(QueryState *queryState);

    template <typename T>
    gl::Error getResultBase(T *params);

    GLuint64 mResult;
    GLuint64 mResultSum;

    Renderer11 *mRenderer;

    std::unique_ptr<QueryState> mActiveQuery;
    std::deque<std::unique_ptr<QueryState>> mPendingQueries;
};

}

#endif // LIBANGLE_RENDERER_D3D_D3D11_QUERY11_H_
