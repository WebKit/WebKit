/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 */

#pragma once

#include "ActiveDOMCallback.h"
#include "CallbackResult.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace JSC {
class JSValue;
}

namespace WebCore {

class SchedulerPostTaskCallback : public RefCounted<SchedulerPostTaskCallback>, public ActiveDOMCallback {
public:
    using ActiveDOMCallback::ActiveDOMCallback;

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    virtual bool isJSSchedulerPostTaskCallback() const { return false; }

    virtual CallbackResult<JSC::JSValue> invoke() = 0;
    virtual CallbackResult<JSC::JSValue> invokeRethrowingException() = 0;

private:
    virtual bool hasCallback() const = 0;
};

} // namespace WebCore
