/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8ConsoleMessage_h
#define V8ConsoleMessage_h

#include "PlatformString.h"
#include <v8.h>
#include <wtf/Vector.h>

namespace WebCore {
    
    class Page;

    // V8ConsoleMessage encapsulates everything needed to
    // log messages originating from JavaScript to the console.
    class V8ConsoleMessage {
    public:
        V8ConsoleMessage(const String& string, const String& sourceID, unsigned lineNumber);

        // Add a message to the console. May end up calling JavaScript code
        // indirectly through the inspector so only call this function when
        // it is safe to do allocations.
        void dispatchNow(Page*);

        // Add a message to the console but delay the reporting until it
        // is safe to do so: Either when we leave JavaScript execution or
        // when adding other console messages. The primary purpose of this
        // method is to avoid calling into V8 to handle console messages
        // when the VM is in a state that does not support GCs or allocations.
        // Delayed messages are always reported in the page corresponding
        // to the active context.
        void dispatchLater();

        // Process any delayed messages. May end up calling JavaScript code
        // indirectly through the inspector so only call this function when
        // it is safe to do allocations.
        static void processDelayed();

        // Convenience class for ensuring that delayed messages in the
        // ConsoleMessageManager are processed quickly.
        class Scope {
        public:
            Scope() { V8ConsoleMessage::processDelayed(); }
            ~Scope() { V8ConsoleMessage::processDelayed(); }
        };

        // Callback from V8.
        static void handler(v8::Handle<v8::Message>, v8::Handle<v8::Value> data);

    private:
        const String m_string;
        const String m_sourceID;
        const unsigned m_lineNumber;

        // All delayed messages are stored in this vector. If the vector
        // is 0, there are no delayed messages.
        static Vector<V8ConsoleMessage>* m_delayedMessages;
    };

} // namespace WebCore

#endif // V8ConsoleMessage_h
