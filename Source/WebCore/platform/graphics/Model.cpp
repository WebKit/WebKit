/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Model.h"

#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<Model> Model::create(Ref<SharedBuffer>&& data, String&& mimeType, URL url, bool isConverted)
{
    return adoptRef(*new Model(WTF::move(data), WTF::move(mimeType), WTF::move(url), isConverted));
}

Model::Model(Ref<SharedBuffer>&& data, String&& mimeType, URL url, bool isConverted)
    : m_data(WTF::move(data))
    , m_mimeType(WTF::move(mimeType))
    , m_url(WTF::move(url))
    , m_isConverted(isConverted)
{
}

Model::~Model() = default;

String Model::filename() const
{
    String filename = m_url.lastPathComponent().toString();
    if (m_isConverted) {
        static constexpr auto usdzExtension = "usdz"_s;
        size_t dotPosition = filename.reverseFind('.');
        if (dotPosition != notFound)
            return makeString(filename.left(dotPosition + 1), usdzExtension);
        return makeString(filename, '.', usdzExtension);
    }
    return filename;
}

TextStream& operator<<(TextStream& ts, const Model& model)
{
    TextStream::GroupScope groupScope(ts);

    ts.dumpProperty("data-size"_s, model.data()->size());
    ts.dumpProperty("mime-type"_s, model.mimeType());
    ts.dumpProperty("url"_s, model.url());
    ts.dumpProperty("is-converted"_s, model.isConverted());

    return ts;
}

}
