//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLEventImpl.h: Defines the abstract rx::CLEventImpl class.

#ifndef LIBANGLE_RENDERER_CLEVENTIMPL_H_
#define LIBANGLE_RENDERER_CLEVENTIMPL_H_

#include "libANGLE/renderer/CLtypes.h"

namespace rx
{

class CLEventImpl : angle::NonCopyable
{
  public:
    using Ptr        = std::unique_ptr<CLEventImpl>;
    using CreateFunc = std::function<Ptr(const cl::Event &)>;

    CLEventImpl(const cl::Event &event);
    virtual ~CLEventImpl();

    virtual cl_int getCommandExecutionStatus(cl_int &executionStatus) = 0;

    virtual cl_int setUserEventStatus(cl_int executionStatus) = 0;

    virtual cl_int setCallback(cl::Event &event, cl_int commandExecCallbackType) = 0;

    virtual cl_int getProfilingInfo(cl::ProfilingInfo name,
                                    size_t valueSize,
                                    void *value,
                                    size_t *valueSizeRet) = 0;

  protected:
    const cl::Event &mEvent;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_CLEVENTIMPL_H_
