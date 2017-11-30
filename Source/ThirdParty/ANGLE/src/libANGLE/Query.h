//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query.h: Defines the gl::Query class

#ifndef LIBANGLE_QUERY_H_
#define LIBANGLE_QUERY_H_

#include "libANGLE/Debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/RefCountObject.h"

#include "common/angleutils.h"

#include "angle_gl.h"

namespace rx
{
class QueryImpl;
}

namespace gl
{

class Query final : public RefCountObject, public LabeledObject
{
  public:
    Query(rx::QueryImpl *impl, GLuint id);
    void destroy(const gl::Context *context) {}
    ~Query() override;

    void setLabel(const std::string &label) override;
    const std::string &getLabel() const override;

    Error begin();
    Error end();
    Error queryCounter();
    Error getResult(GLint *params);
    Error getResult(GLuint *params);
    Error getResult(GLint64 *params);
    Error getResult(GLuint64 *params);
    Error isResultAvailable(bool *available);

    GLenum getType() const;

    rx::QueryImpl *getImplementation();
    const rx::QueryImpl *getImplementation() const;

  private:
    rx::QueryImpl *mQuery;

    std::string mLabel;
};

}

#endif   // LIBANGLE_QUERY_H_
