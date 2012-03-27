//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query.h: Defines the gl::Query class

#ifndef LIBGLESV2_QUERY_H_
#define LIBGLESV2_QUERY_H_

#define GL_APICALL
#include <GLES2/gl2.h>
#include <d3d9.h>

#include "common/angleutils.h"
#include "common/RefCountObject.h"

namespace gl
{

class Query : public RefCountObject
{
  public:
    Query(GLuint id, GLenum type);
    virtual ~Query();

    void begin();
    void end();
    GLuint getResult();
    GLboolean isResultAvailable();

    GLenum getType() const;

  private:
    DISALLOW_COPY_AND_ASSIGN(Query);

    GLboolean testQuery();

    IDirect3DQuery9* mQuery;
    GLenum mType;
    GLboolean mStatus;
    GLint mResult;
};

}

#endif   // LIBGLESV2_QUERY_H_
