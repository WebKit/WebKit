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
    virtual ~Query9();

    virtual gl::Error begin();
    virtual gl::Error end();
    virtual gl::Error queryCounter();
    virtual gl::Error getResult(GLint *params);
    virtual gl::Error getResult(GLuint *params);
    virtual gl::Error getResult(GLint64 *params);
    virtual gl::Error getResult(GLuint64 *params);
    virtual gl::Error isResultAvailable(bool *available);

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
