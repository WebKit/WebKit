/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebFilterOperations_h
#define WebFilterOperations_h

#include "WebCommon.h"
#include "WebFilterOperation.h"
#include "WebPrivateOwnPtr.h"

namespace WebKit {

class WebFilterOperationsPrivate;

// An ordered list of filter operations.
class WebFilterOperations {
public:
    WebFilterOperations() { initialize(); }
    WebFilterOperations(const WebFilterOperations& other)
    {
        initialize();
        assign(other);
    }
    WebFilterOperations& operator=(const WebFilterOperations& other)
    {
        assign(other);
        return *this;
    }
    ~WebFilterOperations() { destroy(); }

    WEBKIT_EXPORT void assign(const WebFilterOperations&);
    WEBKIT_EXPORT bool equals(const WebFilterOperations&) const;

    WEBKIT_EXPORT void append(const WebFilterOperation&);

    // Removes all filter operations.
    WEBKIT_EXPORT void clear();
    WEBKIT_EXPORT bool isEmpty() const;

    WEBKIT_EXPORT void getOutsets(int& top, int& right, int& bottom, int& left) const;
    WEBKIT_EXPORT bool hasFilterThatMovesPixels() const;
    WEBKIT_EXPORT bool hasFilterThatAffectsOpacity() const;

    WEBKIT_EXPORT size_t size() const;
    WEBKIT_EXPORT WebFilterOperation at(size_t) const;

private:
    WEBKIT_EXPORT void initialize();
    WEBKIT_EXPORT void destroy();

    WebPrivateOwnPtr<WebFilterOperationsPrivate> m_private;
};

inline bool operator==(const WebFilterOperations& a, const WebFilterOperations& b)
{
    return a.equals(b);
}

inline bool operator!=(const WebFilterOperations& a, const WebFilterOperations& b)
{
    return !(a == b);
}

}

#endif
