/*
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if ENABLE(REMOTE_INSPECTOR)

#ifndef RemoteInspectorDebuggable_h
#define RemoteInspectorDebuggable_h

#include <wtf/text/WTFString.h>

namespace Inspector {

class InspectorFrontendChannel;
struct RemoteInspectorDebuggableInfo;

class JS_EXPORT_PRIVATE RemoteInspectorDebuggable {
public:
    RemoteInspectorDebuggable();
    virtual ~RemoteInspectorDebuggable();

    void init();
    void update();

    unsigned identifier() const { return m_identifier; }
    void setIdentifier(unsigned identifier) { m_identifier = identifier; }

    bool remoteDebuggingAllowed() const { return m_allowed; }
    void setRemoteDebuggingAllowed(bool);

    RemoteInspectorDebuggableInfo info() const;

    enum DebuggableType { JavaScript, Web };
    virtual DebuggableType type() const = 0;
    virtual String name() const { return String(); } // JavaScript and Web
    virtual String url() const { return String(); } // Web
    virtual bool hasLocalDebugger() const = 0;
    virtual pid_t parentProcessIdentifier() const { return 0; }

    virtual void connect(InspectorFrontendChannel*) = 0;
    virtual void disconnect() = 0;
    virtual void dispatchMessageFromRemoteFrontend(const String& message) = 0;
    virtual void setIndicating(bool) { } // Default is to do nothing.

private:
    unsigned m_identifier;
    bool m_allowed;
};

struct RemoteInspectorDebuggableInfo {
    RemoteInspectorDebuggableInfo()
        : identifier(0)
        , type(RemoteInspectorDebuggable::JavaScript)
        , hasLocalDebugger(false)
        , remoteDebuggingAllowed(false)
        , parentProcessIdentifier(0)
    {
    }

    bool hasParentProcess() const { return !!parentProcessIdentifier; }

    unsigned identifier;
    RemoteInspectorDebuggable::DebuggableType type;
    String name;
    String url;
    bool hasLocalDebugger;
    bool remoteDebuggingAllowed;
    pid_t parentProcessIdentifier;
};

} // namespace Inspector

#endif // RemoteInspectorDebuggable_h

#endif // ENABLE(REMOTE_INSPECTOR)
