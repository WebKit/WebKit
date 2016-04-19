//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query.cpp: Implements the gl::Query class

#include "libANGLE/Query.h"
#include "libANGLE/renderer/QueryImpl.h"

namespace gl
{
Query::Query(rx::QueryImpl *impl, GLuint id) : RefCountObject(id), mQuery(impl), mLabel()
{
}

Query::~Query()
{
    SafeDelete(mQuery);
}

void Query::setLabel(const std::string &label)
{
    mLabel = label;
}

const std::string &Query::getLabel() const
{
    return mLabel;
}

Error Query::begin()
{
    return mQuery->begin();
}

Error Query::end()
{
    return mQuery->end();
}

Error Query::queryCounter()
{
    return mQuery->queryCounter();
}

Error Query::getResult(GLint *params)
{
    return mQuery->getResult(params);
}

Error Query::getResult(GLuint *params)
{
    return mQuery->getResult(params);
}

Error Query::getResult(GLint64 *params)
{
    return mQuery->getResult(params);
}

Error Query::getResult(GLuint64 *params)
{
    return mQuery->getResult(params);
}

Error Query::isResultAvailable(bool *available)
{
    return mQuery->isResultAvailable(available);
}

GLenum Query::getType() const
{
    return mQuery->getType();
}

rx::QueryImpl *Query::getImplementation()
{
    return mQuery;
}

const rx::QueryImpl *Query::getImplementation() const
{
    return mQuery;
}
}
