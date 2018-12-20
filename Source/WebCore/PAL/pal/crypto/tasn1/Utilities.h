/*
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
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

#pragma once

#include <libtasn1.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>

namespace PAL {
namespace TASN1 {

class Structure {
public:
    Structure() = default;

    ~Structure()
    {
        asn1_delete_structure(&m_structure);
    }

    Structure(const Structure&) = delete;
    Structure& operator=(const Structure&) = delete;

    Structure(Structure&&) = delete;
    Structure& operator=(Structure&&) = delete;

    asn1_node* operator&() { return &m_structure; }
    operator asn1_node() const { return m_structure; }

private:
    asn1_node m_structure { nullptr };
};

bool createStructure(const char* elementName, asn1_node* root);
bool decodeStructure(asn1_node* root, const char* elementName, const Vector<uint8_t>& data);
Optional<Vector<uint8_t>> elementData(asn1_node root, const char* elementName);
Optional<Vector<uint8_t>> encodedData(asn1_node root, const char* elementName);
bool writeElement(asn1_node root, const char* elementName, const void* data, size_t dataSize);

} // namespace TASN1
} // namespace PAL
