/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef DOMCoreException_h
#define DOMCodeException_h

#include "ExceptionCode.h"
#include "PlatformString.h"
#include <wtf/RefCounted.h>

namespace WebCore {

    class DOMCoreException : public RefCounted<DOMCoreException> {
    public:
        DOMCoreException(ExceptionCode);

        // FIXME: this is copied from ExceptionCode.h, it needs to be kept in sync.
        enum {
            INDEX_SIZE_ERR = 1,
            DOMSTRING_SIZE_ERR = 2,
            HIERARCHY_REQUEST_ERR = 3,
            WRONG_DOCUMENT_ERR = 4,
            INVALID_CHARACTER_ERR = 5,
            NO_DATA_ALLOWED_ERR = 6,
            NO_MODIFICATION_ALLOWED_ERR = 7,
            NOT_FOUND_ERR = 8,
            NOT_SUPPORTED_ERR = 9,
            INUSE_ATTRIBUTE_ERR = 10,

            // Introduced in DOM Level 2:
            INVALID_STATE_ERR = 11,
            SYNTAX_ERR = 12,
            INVALID_MODIFICATION_ERR = 13,
            NAMESPACE_ERR = 14,
            INVALID_ACCESS_ERR = 15,

            // Introduced in DOM Level 3:
            VALIDATION_ERR = 16,
            TYPE_MISMATCH_ERR = 17
        };

        int code() const { return m_code; }
        String name() const { return m_name; }
        String message() const { return m_message; }

        String toString() const;

    private:
        int m_code;
        String m_name;
        String m_message;
    };

} // namespace WebCore

#endif // DOMCoreException_h
