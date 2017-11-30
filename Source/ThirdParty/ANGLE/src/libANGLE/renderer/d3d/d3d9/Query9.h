//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query9.h: Defines the rx::Query9 class which implements rx::QueryImpl.

#ifndef LIBANGLE_RENDERER_D3D_D3D9_QUERY9_H_
#define LIBANGLE_RENDERER_D3D_D3D9_QUERY9_H_

#include "libANGLE/renderer/QueryImpl.h"

namespace rx
{
class Renderer9;

class Query9 : public QueryImpl
{
  public:
    Query9(Renderer9 *renderer, GLenum type);
    ~Query9() override;

    gl::Error begin() override;
    gl::Error end() override;
    gl::Error queryCounter() override;
    gl::Error getResult(GLint *params) override;
    gl::Error getResult(GLuint *params) override;
    gl::Error getResult(GLint64 *params) override;
    gl::Error getResult(GLuint64 *params) override;
    gl::Error isResultAvailable(bool *available) override;

  private:
    gl::Error testQuery();

    template <typename T>
    gl::Error getResultBase(T *params);

    GLuint64 mResult;
    bool mQueryFinished;

    Renderer9 *mRenderer;
    IDirect3DQuery9 *mQuery;
};

}

#endif // LIBANGLE_RENDERER_D3D_D3D9_QUERY9_H_
