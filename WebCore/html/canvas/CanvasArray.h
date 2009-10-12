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

#ifndef CanvasArray_h
#define CanvasArray_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include "CanvasArrayBuffer.h"

namespace WebCore {
    class CanvasArray : public RefCounted<CanvasArray> {
    public:
        virtual bool isByteArray() const { return false; }
        virtual bool isUnsignedByteArray() const { return false; }
        virtual bool isShortArray() const { return false; }
        virtual bool isUnsignedShortArray() const { return false; }
        virtual bool isIntArray() const { return false; }
        virtual bool isUnsignedIntArray() const { return false; }
        virtual bool isFloatArray() const { return false; }
        
        PassRefPtr<CanvasArrayBuffer> buffer() {
            return m_buffer;
        }

        void* baseAddress() {
            return m_baseAddress;
        }

        unsigned offset() const {
            return m_offset;
        }

        virtual unsigned length() const = 0;
        virtual unsigned sizeInBytes() const = 0;
        virtual unsigned alignedSizeInBytes() const;
        virtual ~CanvasArray();

    protected:
        CanvasArray(PassRefPtr<CanvasArrayBuffer> buffer, unsigned offset);

        // This is the address of the CanvasArrayBuffer's storage, plus the offset.
        void* m_baseAddress;
        unsigned m_offset;
        
    private:
        RefPtr<CanvasArrayBuffer> m_buffer;
    };
    
} // namespace WebCore

#endif // CanvasArray_h
