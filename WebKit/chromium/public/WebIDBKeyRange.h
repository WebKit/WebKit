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

#ifndef WebIDBKeyRange_h
#define WebIDBKeyRange_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"

namespace WebCore { class IDBKeyRange; }

namespace WebKit {

class WebIDBKey;
class WebString;

class WebIDBKeyRange {
public:
    ~WebIDBKeyRange() { reset(); }

    WebIDBKeyRange(const WebIDBKeyRange& keyRange) { assign(keyRange); }
    WebIDBKeyRange(const WebIDBKey& lower, const WebIDBKey& upper, bool lowerOpen, bool upperOpen) { assign(lower, upper, lowerOpen, upperOpen); }

    // FIXME: Remove next 3 methods after next roll.
    WebIDBKeyRange(const WebIDBKey& lower, const WebIDBKey& upper, unsigned short flags) { assign(lower, upper, flags); }
    WEBKIT_API void assign(const WebIDBKey& lower, const WebIDBKey& upper, unsigned short flags);
    WEBKIT_API unsigned short flags() const;
    WEBKIT_API WebIDBKey left() const;
    WEBKIT_API WebIDBKey right() const;

    WEBKIT_API WebIDBKey lower() const;
    WEBKIT_API WebIDBKey upper() const;
    WEBKIT_API bool lowerOpen() const;
    WEBKIT_API bool upperOpen() const;

    WEBKIT_API void assign(const WebIDBKeyRange&);
    WEBKIT_API void assign(const WebIDBKey& lower, const WebIDBKey& upper, bool lowerOpen, bool upperOpen);
    WEBKIT_API void reset();

#if WEBKIT_IMPLEMENTATION
    WebIDBKeyRange(const WTF::PassRefPtr<WebCore::IDBKeyRange>&);
    WebIDBKeyRange& operator=(const WTF::PassRefPtr<WebCore::IDBKeyRange>&);
    operator WTF::PassRefPtr<WebCore::IDBKeyRange>() const;
#endif

private:
    WebPrivatePtr<WebCore::IDBKeyRange> m_private;
};

} // namespace WebKit

#endif // WebIDBKeyRange_h
