/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

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
#include "RuntimeApplicationChecks.h"
#include "SharedBuffer.h"
#include "TextResourceDecoder.h"

namespace WebCore {

CachedScript::CachedScript(CachedResourceRequest&& request, PAL::SessionID sessionID, const CookieJar* cookieJar)
    : CachedResource(WTFMove(request), Type::Script, sessionID, cookieJar)
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

String CachedScript::encoding() const
{
    return String::fromLatin1(protectedDecoder()->encoding().name());
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
        && charactersAreAllASCII(contiguousData->data(), contiguousData->size())) {

        m_decodingState = DataAndDecodedStringHaveSameBytes;

        // If the encoded and decoded data are the same, there is no decoded data cost!
        setDecodedSize(0);
        stopDecodedDataDeletionTimer();

        m_scriptHash = StringHasher::computeHashAndMaskTop8Bits(contiguousData->data(), contiguousData->size());
    }

    if (m_decodingState == DataAndDecodedStringHaveSameBytes)
        return { contiguousData->data(), static_cast<unsigned>(contiguousData->size()) };

    bool shouldForceRedecoding = m_wasForceDecodedAsUTF8 != (shouldDecodeAsUTF8Only == ShouldDecodeAsUTF8Only::Yes);
    if (!m_script || shouldForceRedecoding) {
        if (shouldDecodeAsUTF8Only == ShouldDecodeAsUTF8Only::Yes) {
            Ref forceUTF8Decoder = TextResourceDecoder::create("text/javascript"_s, PAL::UTF8Encoding());
            forceUTF8Decoder->setAlwaysUseUTF8();
            m_script = forceUTF8Decoder->decodeAndFlush(contiguousData->data(), encodedSize());
        } else
            m_script = m_decoder->decodeAndFlush(contiguousData->data(), encodedSize());
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
    auto& script = static_cast<const CachedScript&>(resource);

    CachedResource::setBodyDataFrom(resource);

    m_script = script.m_script;
    m_scriptHash = script.m_scriptHash;
    m_wasForceDecodedAsUTF8 = script.m_wasForceDecodedAsUTF8;
    m_decodingState = script.m_decodingState;
    m_decoder = script.m_decoder;
}

bool CachedScript::shouldIgnoreHTTPStatusCodeErrors() const
{
#if PLATFORM(MAC)
    // This is a workaround for <rdar://problem/13916291>
    // REGRESSION (r119759): Adobe Flash Player "smaller" installer relies on the incorrect firing
    // of a load event and needs an app-specific hack for compatibility.
    // The installer in question tries to load .js file that doesn't exist, causing the server to
    // return a 404 response. Normally, this would trigger an error event to be dispatched, but the
    // installer expects a load event instead so we work around it here.
    if (MacApplication::isSolidStateNetworksDownloader())
        return true;
#endif

    return CachedResource::shouldIgnoreHTTPStatusCodeErrors();
}

} // namespace WebCore
