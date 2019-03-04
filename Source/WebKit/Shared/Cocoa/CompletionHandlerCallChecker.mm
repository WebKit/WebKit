/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "CompletionHandlerCallChecker.h"

#import <mutex>
#import <objc/runtime.h>
#import <wtf/Ref.h>
#import "VersionChecks.h"

namespace WebKit {

Ref<CompletionHandlerCallChecker> CompletionHandlerCallChecker::create(id delegate, SEL delegateMethodSelector)
{
    return adoptRef(*new CompletionHandlerCallChecker(object_getClass(delegate), delegateMethodSelector));
}

CompletionHandlerCallChecker::CompletionHandlerCallChecker(Class delegateClass, SEL delegateMethodSelector)
    : m_delegateClass(delegateClass)
    , m_delegateMethodSelector(delegateMethodSelector)
    , m_didCallCompletionHandler(false)
{
}

CompletionHandlerCallChecker::~CompletionHandlerCallChecker()
{
    if (m_didCallCompletionHandler)
        return;

    Class delegateClass = classImplementingDelegateMethod();
    [NSException raise:NSInternalInconsistencyException format:@"Completion handler passed to %c[%@ %@] was not called", class_isMetaClass(delegateClass) ? '+' : '-', NSStringFromClass(delegateClass), NSStringFromSelector(m_delegateMethodSelector)];
}

void CompletionHandlerCallChecker::didCallCompletionHandler()
{
    ASSERT(!m_didCallCompletionHandler);
    m_didCallCompletionHandler = true;
}

static bool shouldThrowExceptionForDuplicateCompletionHandlerCall()
{
    static bool shouldThrowException;
    static std::once_flag once;
    std::call_once(once, [] {
        shouldThrowException = linkedOnOrAfter(SDKVersion::FirstWithExceptionsForDuplicateCompletionHandlerCalls);
    });
    return shouldThrowException;
}

bool CompletionHandlerCallChecker::completionHandlerHasBeenCalled() const
{
    if (!m_didCallCompletionHandler)
        return false;

    if (shouldThrowExceptionForDuplicateCompletionHandlerCall()) {
        Class delegateClass = classImplementingDelegateMethod();
        [NSException raise:NSInternalInconsistencyException format:@"Completion handler passed to %c[%@ %@] was called more than once", class_isMetaClass(delegateClass) ? '+' : '-', NSStringFromClass(delegateClass), NSStringFromSelector(m_delegateMethodSelector)];
    }

    return true;
}

Class CompletionHandlerCallChecker::classImplementingDelegateMethod() const
{
    Class delegateClass = m_delegateClass;
    Method delegateMethod = class_getInstanceMethod(delegateClass, m_delegateMethodSelector);

    for (Class superclass = class_getSuperclass(delegateClass); superclass; superclass = class_getSuperclass(superclass)) {
        if (class_getInstanceMethod(superclass, m_delegateMethodSelector) != delegateMethod)
            break;

        delegateClass = superclass;
    }

    return delegateClass;
}

} // namespace WebKit
