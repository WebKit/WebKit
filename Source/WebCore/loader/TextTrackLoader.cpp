/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#include "TextTrackLoader.h"

#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedTextTrack.h"
#include "CrossOriginAccessControl.h"
#include "Document.h"
#include "HTMLTrackElement.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "SharedBuffer.h"
#include "VTTCue.h"
#include "WebVTTParser.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {
    
TextTrackLoader::TextTrackLoader(TextTrackLoaderClient& client, ScriptExecutionContext* context)
    : m_client(client)
    , m_scriptExecutionContext(context)
    , m_cueLoadTimer(*this, &TextTrackLoader::cueLoadTimerFired)
    , m_state(Idle)
    , m_parseOffset(0)
    , m_newCuesAvailable(false)
{
}

TextTrackLoader::~TextTrackLoader()
{
    if (m_resource)
        m_resource->removeClient(*this);
}

void TextTrackLoader::cueLoadTimerFired()
{
    if (m_newCuesAvailable) {
        m_newCuesAvailable = false;
        m_client.newCuesAvailable(*this);
    }
    
    if (m_state >= Finished)
        m_client.cueLoadingCompleted(*this, m_state == Failed);
}

void TextTrackLoader::cancelLoad()
{
    if (m_resource) {
        m_resource->removeClient(*this);
        m_resource = nullptr;
    }
}

void TextTrackLoader::processNewCueData(CachedResource& resource)
{
    ASSERT_UNUSED(resource, m_resource == &resource);

    if (m_state == Failed || !m_resource->resourceBuffer())
        return;

    auto* buffer = m_resource->resourceBuffer();
    if (m_parseOffset == buffer->size())
        return;

    if (!m_cueParser)
        m_cueParser = makeUnique<WebVTTParser>(static_cast<WebVTTParserClient*>(this), m_scriptExecutionContext);

    while (m_parseOffset < buffer->size()) {
        auto data = buffer->getSomeData(m_parseOffset);
        m_cueParser->parseBytes(data.data(), data.size());
        m_parseOffset += data.size();
    }
}

// FIXME: This is a very unusual pattern, no other CachedResourceClient does this. Refactor to use notifyFinished() instead.
void TextTrackLoader::deprecatedDidReceiveCachedResource(CachedResource& resource)
{
    ASSERT_UNUSED(resource, m_resource == &resource);

    if (!m_resource->resourceBuffer())
        return;

    processNewCueData(*m_resource);
}

void TextTrackLoader::corsPolicyPreventedLoad()
{
    static NeverDestroyed<String> consoleMessage(MAKE_STATIC_STRING_IMPL("Cross-origin text track load denied by Cross-Origin Resource Sharing policy."));
    Document* document = downcast<Document>(m_scriptExecutionContext);
    document->addConsoleMessage(MessageSource::Security, MessageLevel::Error, consoleMessage);
    m_state = Failed;
}

void TextTrackLoader::notifyFinished(CachedResource& resource)
{
    ASSERT_UNUSED(resource, m_resource == &resource);

    if (m_resource->resourceError().isAccessControl())
        corsPolicyPreventedLoad();

    if (m_state != Failed) {
        processNewCueData(*m_resource);
        if (m_cueParser)
            m_cueParser->fileFinished();
        if (m_state != Failed)
            m_state = m_resource->errorOccurred() ? Failed : Finished;
    }

    if (m_state == Finished && m_cueParser)
        m_cueParser->flush();

    if (!m_cueLoadTimer.isActive())
        m_cueLoadTimer.startOneShot(0_s);

    cancelLoad();
}

bool TextTrackLoader::load(const URL& url, HTMLTrackElement& element)
{
    cancelLoad();

    ASSERT(is<Document>(m_scriptExecutionContext));
    Document& document = downcast<Document>(*m_scriptExecutionContext);

    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;
    options.contentSecurityPolicyImposition = element.isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;

    ResourceRequest resourceRequest(document.completeURL(url));

    if (auto mediaElement = element.mediaElement())
        resourceRequest.setInspectorInitiatorNodeIdentifier(InspectorInstrumentation::identifierForNode(*mediaElement));

    auto cueRequest = createPotentialAccessControlRequest(WTFMove(resourceRequest), WTFMove(options), document, element.mediaElementCrossOriginAttribute(), SameOriginFlag::Yes);
    m_resource = document.cachedResourceLoader().requestTextTrack(WTFMove(cueRequest)).value_or(nullptr);
    if (!m_resource)
        return false;
    m_resource->addClient(*this);
    return true;
}

void TextTrackLoader::newCuesParsed()
{
    if (m_cueLoadTimer.isActive())
        return;

    m_newCuesAvailable = true;
    m_cueLoadTimer.startOneShot(0_s);
}

void TextTrackLoader::newRegionsParsed()
{
    m_client.newRegionsAvailable(*this);
}

void TextTrackLoader::newStyleSheetsParsed()
{
    m_client.newStyleSheetsAvailable(*this);
}

void TextTrackLoader::fileFailedToParse()
{
    LOG(Media, "TextTrackLoader::fileFailedToParse");

    m_state = Failed;

    if (!m_cueLoadTimer.isActive())
        m_cueLoadTimer.startOneShot(0_s);

    cancelLoad();
}

void TextTrackLoader::getNewCues(Vector<RefPtr<TextTrackCue>>& outputCues)
{
    ASSERT(m_cueParser);
    if (m_cueParser) {
        Vector<RefPtr<WebVTTCueData>> newCues;
        m_cueParser->getNewCues(newCues);

        for (auto& cueData : newCues)
            outputCues.append(VTTCue::create(*m_scriptExecutionContext, *cueData));
    }
}

void TextTrackLoader::getNewRegions(Vector<RefPtr<VTTRegion>>& outputRegions)
{
    ASSERT(m_cueParser);
    if (m_cueParser)
        m_cueParser->getNewRegions(outputRegions);
}

Vector<String> TextTrackLoader::getNewStyleSheets()
{
    ASSERT(m_cueParser);
    if (m_cueParser)
        return m_cueParser->getStyleSheets();
    return Vector<String>();
}

}

#endif
