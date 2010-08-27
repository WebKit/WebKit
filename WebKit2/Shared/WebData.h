/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebData_h
#define WebData_h

#include "APIObject.h"
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WebKit {

// WebData - A data buffer type suitable for vending to an API.

class WebData : public APIObject {
public:
    static const Type APIType = TypeData;

    static PassRefPtr<WebData> create(const unsigned char* bytes, size_t size)
    {
        return adoptRef(new WebData(bytes, size));
    }
    
    static PassRefPtr<WebData> create(const Vector<unsigned char>& buffer)
    {
        return adoptRef(new WebData(buffer));
    }
    
    const unsigned char* bytes() const { return m_buffer.data(); }
    size_t size() const { return m_buffer.size(); }

private:
    WebData(const unsigned char* bytes, size_t size)
        : m_buffer(size)
    {
        memcpy(m_buffer.data(), bytes, size);
    }
    
    WebData(const Vector<unsigned char>& buffer)
        : m_buffer(buffer)
    {
    }

    virtual Type type() const { return APIType; }

    Vector<unsigned char> m_buffer;
};

} // namespace WebKit

#endif // WebData_h
