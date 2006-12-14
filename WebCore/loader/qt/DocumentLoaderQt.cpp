/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * All rights reserved.
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

#include "config.h"
#include "DocumentLoader.h"

#include "Document.h"
#include "FrameLoader.h"
#include "FrameQt.h"
#include "PlatformString.h"
#include "XMLTokenizer.h"
#include <wtf/Assertions.h>

#include <QDateTime>

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); } while(0)

namespace WebCore {

/*
 * Performs three operations:
 *  1. Convert backslashes to currency symbols
 *  2. Convert control characters to spaces
 *  3. Trim leading and trailing spaces
 */
static inline String canonicalizedTitle(const String& title, Frame* frame)
{
    ASSERT(!title.isEmpty());
    
    const UChar* characters = title.characters();
    unsigned length = title.length();
    unsigned i;
    
    // Find the first non-canonical character
    for (i = 0; i < length; ++i) {
        UChar c = characters[i];
        if (c < 0x20 || c == 0x7F || c == '\\')
            break;
    }

    // Optimization for titles that have no non-canonical characters and no leading or trailing spaces
    if (i == length && characters[0] != ' ' && characters[length - 1] != ' ')
        return title;

    Vector<UChar> stringBuilder(length);
    unsigned builderIndex = 0;
    
    // Skip leading spaces and leading characters that would convert to spaces
    for (i = 0; i < length; ++i) {
        UChar c = characters[i];
        if (!(c <= 0x20 || c == 0x7F))
            break;
    }
    
    // Replace control characters with spaces, and backslashes with currency symbols
    for (; i < length; ++i) {
        UChar c = characters[i];
        if (c < 0x20 || c == 0x7F)
            c = ' ';
        else if (c == '\\')
            c = frame->backslashAsCurrencySymbol();
        stringBuilder[builderIndex++] = c;
    }

    // Strip trailing spaces
    while (builderIndex > 0) {
        --builderIndex;
        if (stringBuilder[builderIndex] != ' ')
            break;
    }
    
    if (builderIndex == 0 && stringBuilder[builderIndex] == ' ')
        return "";
    
    stringBuilder.resize(builderIndex + 1);
    return String::adopt(stringBuilder);
}

FrameLoader* DocumentLoader::frameLoader() const
{
    if (!m_frame)
        return 0;
    return m_frame->loader();
}

DocumentLoader::~DocumentLoader()
{
    ASSERT(!m_frame || frameLoader()->activeDocumentLoader() != this ||
           !frameLoader()->isLoading());
}

const ResourceRequest& DocumentLoader::originalRequest() const
{
    return m_originalRequest;
}

const ResourceRequest& DocumentLoader::originalRequestCopy() const
{
    return m_originalRequestCopy;
}

const ResourceRequest& DocumentLoader::request() const
{
    // FIXME: need a better way to handle data loads
    return m_request;
}

ResourceRequest& DocumentLoader::request()
{
    // FIXME: need a better way to handle data loads
    return m_request;
}

const ResourceRequest& DocumentLoader::initialRequest() const
{
    // FIXME: need a better way to handle data loads
    return m_originalRequest;
}

ResourceRequest& DocumentLoader::actualRequest()
{
    return m_request;
}

const KURL& DocumentLoader::URL() const
{
    return request().url();
}

const KURL DocumentLoader::unreachableURL() const
{
    KURL url;
    notImplemented();
    return url;
}

void DocumentLoader::replaceRequestURLForAnchorScroll(const KURL& URL)
{
    notImplemented();
}

bool DocumentLoader::isStopping() const
{
    return m_stopping;
}

void DocumentLoader::clearErrors()
{
}

// Cancels the data source's pending loads.  Conceptually, a data source only loads
// one document at a time, but one document may have many related resources. 
// stopLoading will stop all loads initiated by the data source, 
// but not loads initiated by child frames' data sources -- that's the WebFrame's job.
void DocumentLoader::stopLoading()
{
    // Always attempt to stop the frame because it may still be loading/parsing after the data source
    // is done loading and not stopping it can cause a world leak.
    if (m_committed)
        m_frame->loader()->stopLoading(false);
    
    if (!m_loading)
        return;
    
    RefPtr<Frame> protectFrame(m_frame);
    RefPtr<DocumentLoader> protectLoader(this);

    m_stopping = true;

    FrameLoader* frameLoader = DocumentLoader::frameLoader();
    
    if (frameLoader->hasMainResourceLoader())
        // Stop the main resource loader and let it send the cancelled message.
        frameLoader->cancelMainResourceLoad();
    //else if (frameLoader->isLoadingSubresources())
        // The main resource loader already finished loading. Set the cancelled error on the 
        // document and let the subresourceLoaders send individual cancelled messages below.
        //setMainDocumentError(frameLoader->cancelledError(m_request.get()));
    //else
        // If there are no resource loaders, we need to manufacture a cancelled message.
        // (A back/forward navigation has no resource loaders because its resources are cached.)
        //mainReceivedError(frameLoader->cancelledError(m_request.get()), true);
    
    frameLoader->stopLoadingSubresources();
    frameLoader->stopLoadingPlugIns();
    
    m_stopping = false;
}

void DocumentLoader::setupForReplace()
{
    frameLoader()->setupForReplace();
    m_committed = false;
}

void DocumentLoader::commitIfReady()
{
    notImplemented();
    if (m_gotFirstByte && !m_committed) {
        m_committed = true;
        //frameLoader()->commitProvisionalLoad(0);
    }
}

void DocumentLoader::finishedLoading()
{
    m_gotFirstByte = true;   
    commitIfReady();
    frameLoader()->finishedLoadingDocument(this);
    m_frame->loader()->end();
}

void DocumentLoader::setCommitted(bool f)
{
    m_committed = f;
}

bool DocumentLoader::isCommitted() const
{
    return m_committed;
}

void DocumentLoader::setLoading(bool f)
{
    m_loading = f;
}

bool DocumentLoader::isLoading() const
{
    return m_loading;
}

bool DocumentLoader::doesProgressiveLoad(const String& MIMEType) const
{
    return !frameLoader()->isReplacing() || MIMEType == "text/html";
}

void DocumentLoader::setupForReplaceByMIMEType(const String& newMIMEType)
{
    if (!m_gotFirstByte)
        return;

    //String oldMIMEType = [m_response.get() MIMEType];
    //FIXME: get the mimetype correctly (then remove notImplemented())
    notImplemented();
    String oldMIMEType;
    
    if (!doesProgressiveLoad(oldMIMEType)) {
        frameLoader()->revertToProvisional(this);
        setupForReplace();
    }
    
    frameLoader()->finishedLoadingDocument(this);
    m_frame->loader()->end();
    
    frameLoader()->setReplacing();
    m_gotFirstByte = false;
    
    if (doesProgressiveLoad(newMIMEType)) {
        frameLoader()->revertToProvisional(this);
        setupForReplace();
    }
    
    frameLoader()->stopLoadingSubresources();
    frameLoader()->stopLoadingPlugIns();

    frameLoader()->finalSetupForReplace(this);
}

void DocumentLoader::updateLoading()
{
    ASSERT(this == frameLoader()->activeDocumentLoader());
    setLoading(frameLoader()->isLoading());
}

void DocumentLoader::setFrame(Frame* frame)
{
    if (m_frame == frame)
        return;
    ASSERT(frame && !m_frame);
    m_frame = frame;
    attachToFrame();
}

void DocumentLoader::attachToFrame()
{
    ASSERT(m_frame);
}

void DocumentLoader::detachFromFrame()
{
    ASSERT(m_frame);
    m_frame = 0;
}

void DocumentLoader::prepareForLoadStart()
{
    ASSERT(!m_stopping);
    setPrimaryLoadComplete(false);
    ASSERT(frameLoader());
    clearErrors();
    
    // Mark the start loading time.
    m_loadingStartedTime = QDateTime::currentDateTime().toTime_t();
    
    setLoading(true);
    
    frameLoader()->prepareForLoadStart();
}

double DocumentLoader::loadingStartedTime() const
{
    return m_loadingStartedTime;
}

void DocumentLoader::setIsClientRedirect(bool flag)
{
    m_isClientRedirect = flag;
}

bool DocumentLoader::isClientRedirect() const
{
    return m_isClientRedirect;
}

void DocumentLoader::setPrimaryLoadComplete(bool flag)
{
    m_primaryLoadComplete = flag;
    notImplemented();
    if (flag) {
        //if (frameLoader()->hasMainResourceLoader()) {
        //    setMainResourceData(frameLoader()->mainResourceData());
        //    frameLoader()->releaseMainResourceLoader();
        //}
        updateLoading();
    }
}

bool DocumentLoader::isLoadingInAPISense() const
{
    // Once a frame has loaded, we no longer need to consider subresources,
    // but we still need to consider subframes.
    if (frameLoader()->state() != FrameStateComplete) {
        if (!m_primaryLoadComplete && isLoading())
            return true;
        if (frameLoader()->isLoadingSubresources())
            return true;
        if (Document* doc = m_frame->document())
            if (Tokenizer* tok = doc->tokenizer())
                if (tok->processingData())
                    return true;
    }
    return frameLoader()->subframeIsLoading();
}

void DocumentLoader::stopRecordingResponses()
{
    notImplemented();
}

String DocumentLoader::title() const
{
    return m_pageTitle;
}

const NavigationAction& DocumentLoader::triggeringAction() const
{
    return m_triggeringAction;
}

void DocumentLoader::setTriggeringAction(const NavigationAction& action)
{
    m_triggeringAction = action;
}

void DocumentLoader::setOverrideEncoding(const String& enc)
{
    m_overrideEncoding = enc;
}

String DocumentLoader::overrideEncoding() const
{
    return m_overrideEncoding;
}

void DocumentLoader::setTitle(const String& title)
{
    if (title.isEmpty())
        return;

    String trimmed = canonicalizedTitle(title, m_frame);
    if (!trimmed.isEmpty() && m_pageTitle != trimmed) {
        frameLoader()->willChangeTitle(this);
        m_pageTitle = trimmed;
        frameLoader()->didChangeTitle(this);
    }
}

KURL DocumentLoader::URLForHistory() const
{
    // Return the URL to be used for history and B/F list.
    // Returns nil for WebDataProtocol URLs that aren't alternates 
    // for unreachable URLs, because these can't be stored in history.
    KURL url;
    notImplemented();
    return url;
}

}

