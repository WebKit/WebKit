/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004-2025 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "CachedScript.h"

#include "CachedResourceClient.h"
#include "CachedResourceRequest.h"
#include "SharedBuffer.h"
#include "TextResourceDecoder.h"

#if PLATFORM(MAC)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCore {

CachedScript::CachedScript(CachedResourceRequest&& request, PAL::SessionID sessionID, const CookieJar* cookieJar, ScriptRequiresTelemetry requiresTelemetry)
    : CachedResource(WTFMove(request), Type::Script, sessionID, cookieJar)
    , m_requiresTelemetry(requiresTelemetry == ScriptRequiresTelemetry::Yes)
    , m_decoder(TextResourceDecoder::create("text/javascript"_s, request.charset()))
{
}

CachedScript::~CachedScript() = default;

RefPtr<TextResourceDecoder> CachedScript::protectedDecoder() const
{
    return m_decoder;
}

void CachedScript::setEncoding(const String& chs)
{
    protectedDecoder()->setEncoding(chs, TextResourceDecoder::EncodingFromHTTPHeader);
}

ASCIILiteral CachedScript::encoding() const
{
    return protectedDecoder()->encoding().name();
}

StringView CachedScript::script(ShouldDecodeAsUTF8Only shouldDecodeAsUTF8Only)
{
    if (!m_data)
        return emptyString();

    if (RefPtr data = m_data; !data->isContiguous())
        m_data = data->makeContiguous();

    Ref contiguousData = downcast<SharedBuffer>(*m_data);
    if (m_decodingState == NeverDecoded
        && PAL::TextEncoding(encoding()).isByteBasedEncoding()
        && contiguousData->size()
        && charactersAreAllASCII(contiguousData->span())) {

        m_decodingState = DataAndDecodedStringHaveSameBytes;

        // If the encoded and decoded data are the same, there is no decoded data cost!
        setDecodedSize(0);
        stopDecodedDataDeletionTimer();

        m_scriptHash = StringHasher::computeHashAndMaskTop8Bits(contiguousData->span());
    }

    if (m_decodingState == DataAndDecodedStringHaveSameBytes)
        return { contiguousData->span() };

    bool shouldForceRedecoding = m_wasForceDecodedAsUTF8 != (shouldDecodeAsUTF8Only == ShouldDecodeAsUTF8Only::Yes);
    if (!m_script || shouldForceRedecoding) {
        ASSERT(contiguousData->span().size() == encodedSize());
        if (shouldDecodeAsUTF8Only == ShouldDecodeAsUTF8Only::Yes) {
            Ref forceUTF8Decoder = TextResourceDecoder::create("text/javascript"_s, PAL::UTF8Encoding());
            forceUTF8Decoder->setAlwaysUseUTF8();
            m_script = forceUTF8Decoder->decodeAndFlush(contiguousData->span());
        } else
            m_script = protectedDecoder()->decodeAndFlush(contiguousData->span());
        if (m_decodingState == NeverDecoded || shouldForceRedecoding)
            m_scriptHash = m_script.hash();
        ASSERT(!m_scriptHash || m_scriptHash == m_script.hash());
        m_decodingState = DataAndDecodedStringHaveDifferentBytes;
        m_wasForceDecodedAsUTF8 = shouldDecodeAsUTF8Only == ShouldDecodeAsUTF8Only::Yes;
        setDecodedSize(m_script.sizeInBytes());
    }

    restartDecodedDataDeletionTimer();
    return m_script;
}

unsigned CachedScript::scriptHash(ShouldDecodeAsUTF8Only shouldDecodeAsUTF8Only)
{
    if (m_decodingState == NeverDecoded || (m_decodingState == DataAndDecodedStringHaveDifferentBytes && m_wasForceDecodedAsUTF8 != (shouldDecodeAsUTF8Only == ShouldDecodeAsUTF8Only::Yes)))
        script(shouldDecodeAsUTF8Only);
    return m_scriptHash;
}

void CachedScript::finishLoading(const FragmentedSharedBuffer* data, const NetworkLoadMetrics& metrics)
{
    if (data) {
        m_data = data->makeContiguous();
        setEncodedSize(data->size());
    } else {
        m_data = nullptr;
        setEncodedSize(0);
    }
    CachedResource::finishLoading(data, metrics);
}

void CachedScript::destroyDecodedData()
{
    m_script = String();
    setDecodedSize(0);
}

void CachedScript::setBodyDataFrom(const CachedResource& resource)
{
    ASSERT(resource.type() == type());
    auto& script = downcast<const CachedScript>(resource);

    CachedResource::setBodyDataFrom(resource);

    m_script = script.m_script;
    m_scriptHash = script.m_scriptHash;
    m_wasForceDecodedAsUTF8 = script.m_wasForceDecodedAsUTF8;
    m_decodingState = script.m_decodingState;
    m_decoder = script.m_decoder;
}

} // namespace WebCore
