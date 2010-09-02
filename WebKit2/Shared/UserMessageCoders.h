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

#include "ArgumentEncoder.h"
#include "ArgumentDecoder.h"
#include "ImmutableArray.h"
#include "WebString.h"

namespace WebKit {

//   - Array -> Array
//   - String -> String

template<typename Owner>
class UserMessageEncoder {
public:
    bool baseEncode(CoreIPC::ArgumentEncoder* encoder, APIObject::Type type) const 
    {
        switch (type) {
        case APIObject::TypeArray: {
            ImmutableArray* array = static_cast<ImmutableArray*>(m_root);
            encoder->encode(static_cast<uint64_t>(array->size()));
            for (size_t i = 0; i < array->size(); ++i)
                encoder->encode(Owner(array->at(i)));
            return true;
        }
        case APIObject::TypeString: {
            WebString* string = static_cast<WebString*>(m_root);
            encoder->encode(string->string());
            return true;
        }
        default:
            break;
        }

        return false;
    }

protected:
    UserMessageEncoder(APIObject* root) 
        : m_root(root)
    {
    }

    APIObject* m_root;
};


// Handles
//   - Array -> Array
//   - String -> String

template<typename Owner>
class UserMessageDecoder {
public:
    static bool baseDecode(CoreIPC::ArgumentDecoder* decoder, Owner& coder, APIObject::Type type)
    {
        switch (type) {
        case APIObject::TypeArray: {
            uint64_t size;
            if (!decoder->decode(size))
                return false;

            Vector<RefPtr<APIObject> > vector;
            for (size_t i = 0; i < size; ++i) {
                RefPtr<APIObject> element;
                Owner messageCoder(coder, element);
                if (!decoder->decode(messageCoder))
                    return false;
                vector.append(element.release());
            }

            coder.m_root = ImmutableArray::adopt(vector);
            break;
        }
        case APIObject::TypeString: {
            String string;
            if (!decoder->decode(string))
                return false;
            coder.m_root = WebString::create(string);
            break;
        }
        default:
            break;
        }

        return true;
    }

protected:
    UserMessageDecoder(RefPtr<APIObject>& root)
        : m_root(root)
    {
    }

    RefPtr<APIObject>& m_root;
};

} // namespace WebKit
