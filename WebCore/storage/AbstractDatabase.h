/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AbstractDatabase_h
#define AbstractDatabase_h

#if ENABLE(DATABASE)

#include "PlatformString.h"
#include <wtf/ThreadSafeShared.h>

namespace WebCore {

class ScriptExecutionContext;
class SecurityOrigin;

class AbstractDatabase : public ThreadSafeShared<AbstractDatabase> {
public:
    static bool isAvailable();
    static void setIsAvailable(bool available);

    virtual ~AbstractDatabase();

    virtual ScriptExecutionContext* scriptExecutionContext() const = 0;
    virtual SecurityOrigin* securityOrigin() const = 0;
    virtual String stringIdentifier() const = 0;
    virtual String displayName() const = 0;
    virtual unsigned long estimatedSize() const = 0;
    virtual String fileName() const = 0;

    virtual void markAsDeletedAndClose() = 0;
    virtual void closeImmediately() = 0;
};

} // namespace WebCore

#endif // ENABLE(DATABASE)

#endif // AbstractDatabase_h
