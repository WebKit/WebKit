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
#include "CachedResourceClientWalker.h"
#include "CachedResourceRequest.h"
#include "RuntimeApplicationChecks.h"
#include "SharedBuffer.h"
#include "TextResourceDecoder.h"

namespace WebCore {

CachedScript::CachedScript(CachedResourceRequest&& request, const PAL::SessionID& sessionID, const CookieJar* cookieJar)
    : CachedResource(WTFMove(request), Type::Script, sessionID, cookieJar)
    , m_decoder(TextResourceDecoder::create("text/javascript"_s, request.charset()))
{
}

CachedScript::~CachedScript() = default;

void CachedScript::setEncoding(const String& chs)
{
    m_decoder->setEncoding(chs, TextResourceDecoder::EncodingFromHTTPHeader);
}

String CachedScript::encoding() const
{
    return m_decoder->encoding().name();
}

StringView CachedScript::script()
{
    if (!m_data)
        return { };

    if (m_decodingState == NeverDecoded
        && TextEncoding(encoding()).isByteBasedEncoding()
        && m_data->size()
        && charactersAreAllASCII(reinterpret_cast<const LChar*>(m_data->data()), m_data->size())) {

        m_decodingState = DataAndDecodedStringHaveSameBytes;

        // If the encoded and decoded data are the same, there is no decoded data cost!
        setDecodedSize(0);
        m_decodedDataDeletionTimer.stop();

        m_scriptHash = StringHasher::computeHashAndMaskTop8Bits(reinterpret_cast<const LChar*>(m_data->data()), m_data->size());
    }

    if (m_decodingState == DataAndDecodedStringHaveSameBytes)
        return { reinterpret_cast<const LChar*>(m_data->data()), static_cast<unsigned>(m_data->size()) };

    if (!m_script) {
        m_script = m_decoder->decodeAndFlush(m_data->data(), encodedSize());
        ASSERT(!m_scriptHash || m_scriptHash == m_script.impl()->hash());
        if (m_decodingState == NeverDecoded)
            m_scriptHash = m_script.impl()->hash();
        m_decodingState = DataAndDecodedStringHaveDifferentBytes;
        setDecodedSize(m_script.sizeInBytes());
    }

    m_decodedDataDeletionTimer.restart();
    return m_script;
}

unsigned CachedScript::scriptHash()
{
    if (m_decodingState == NeverDecoded)
        script();
    return m_scriptHash;
}

void CachedScript::finishLoading(SharedBuffer* data)
{
    m_data = data;
    setEncodedSize(data ? data->size() : 0);
    CachedResource::finishLoading(data);
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
