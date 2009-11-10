/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef WebGLFloatArray_h
#define WebGLFloatArray_h

#include "WebGLArray.h"
#include <wtf/MathExtras.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {
    
    class WebGLFloatArray : public WebGLArray {
    public:
        virtual bool isFloatArray() const { return true; }

        static PassRefPtr<WebGLFloatArray> create(unsigned length);
        static PassRefPtr<WebGLFloatArray> create(float* array, unsigned length);
        static PassRefPtr<WebGLFloatArray> create(PassRefPtr<WebGLArrayBuffer> buffer, int offset, unsigned length);

        float* data() { return static_cast<float*>(baseAddress()); }

        virtual unsigned length() const;
        
        virtual unsigned sizeInBytes() const;

        void set(unsigned index, double value)
        {
            if (index >= m_size)
                return;
            if (isnan(value)) // Clamp NaN to 0
                value = 0;
            float* storage = static_cast<float*>(m_baseAddress);
            storage[index] = static_cast<float>(value);
        }
        
        bool get(unsigned index, float& result) const
        {
            if (index >= m_size)
                return false;
            result = item(index);
            return true;
        }
        
        float item(unsigned index) const
        {
            ASSERT(index < m_size);
            float* storage = static_cast<float*>(m_baseAddress);
            float result = storage[index];
            if (isnan(result)) {
                // Clamp NaN to 0
                result = 0;
            }
            return result;
        }
    private:
        WebGLFloatArray(PassRefPtr<WebGLArrayBuffer> buffer, int offset, unsigned length);
        unsigned m_size;
    };
    
} // namespace WebCore

#endif // WebGLFloatArray_h
