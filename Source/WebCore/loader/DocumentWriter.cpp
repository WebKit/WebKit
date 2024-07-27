/*
 * Copyright (C) 2010. Adam Barth. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DocumentWriter.h"

#include "ContentSecurityPolicy.h"
#include "DOMImplementation.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "FrameLoader.h"
#include "FrameLoaderStateMachine.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "LocalFrameView.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "PluginDocument.h"
#include "RawDataDocumentParser.h"
#include "ScriptController.h"
#include "ScriptableDocumentParser.h"
#include "SecurityOrigin.h"
#include "SecurityOriginPolicy.h"
#include "SecurityPolicy.h"
#include "SegmentedString.h"
#include "Settings.h"
#include "SinkDocument.h"
#include "TextResourceDecoder.h"
#include <wtf/Ref.h>

namespace WebCore {

static inline bool canReferToParentFrameEncoding(const LocalFrame* frame, const LocalFrame* parentFrame) 
{
    if (is<XMLDocument>(frame->document()))
        return false;
    return parentFrame && parentFrame->document()->protectedSecurityOrigin()->isSameOriginDomain(frame->document()->securityOrigin());
}
    
// This is only called by ScriptController::executeIfJavaScriptURL
// and always contains the result of evaluating a javascript: url.
// This is the <iframe src="javascript:'html'"> case.
void DocumentWriter::replaceDocumentWithResultOfExecutingJavascriptURL(const String& source, Document* ownerDocument)
{
    Ref frame = *m_frame;
    frame->checkedLoader()->stopAllLoaders();

    // If we are in the midst of changing the frame's document, don't execute script
    // that modifies the document further:
    if (frame->documentIsBeingReplaced())
        return;

    begin(frame->document()->url(), true, ownerDocument);

    setEncoding("UTF-8"_s, IsEncodingUserChosen::No);

    // begin() might fire an unload event, which will result in a situation where no new document has been attached,
    // and the old document has been detached. Therefore, bail out if no document is attached.
    if (!frame->document())
        return;

    if (!source.isNull()) {
        if (!m_hasReceivedSomeData) {
            m_hasReceivedSomeData = true;
            frame->protectedDocument()->setCompatibilityMode(DocumentCompatibilityMode::NoQuirksMode);
        }

        if (RefPtr parser = frame->document()->parser())
            parser->appendBytes(*this, source.utf8().span());
    }

    end();
}

void DocumentWriter::clear()
{
    m_decoder = nullptr;
    m_hasReceivedSomeData = false;
    if (!m_encodingWasChosenByUser)
        m_encoding = String();
}

bool DocumentWriter::begin()
{
    return begin(URL());
}

Ref<Document> DocumentWriter::createDocument(const URL& url, ScriptExecutionContextIdentifier documentIdentifier)
{
    Ref frame = *m_frame;
    CheckedRef frameLoader = frame->loader();

    auto useSinkDocument = [&]() {
#if ENABLE(PDF_PLUGIN)
        if (frameLoader->client().shouldUsePDFPlugin(m_mimeType, url.path()))
            return false;
#endif
#if PLATFORM(IOS_FAMILY)
        if (frame->isMainFrame())
            return true;

        return !frame->settings().useImageDocumentForSubframePDF();
#else
        return false;
#endif
    };

    if (MIMETypeRegistry::isPDFMIMEType(m_mimeType) && useSinkDocument())
        return SinkDocument::create(frame, url);

    if (!frameLoader->client().hasHTMLView())
        return Document::createNonRenderedPlaceholder(frame, url);

    return DOMImplementation::createDocument(m_mimeType, frame.ptr(), frame->settings(), url, documentIdentifier);
}

bool DocumentWriter::begin(const URL& urlReference, bool dispatch, Document* ownerDocument, ScriptExecutionContextIdentifier documentIdentifier, const NavigationAction* triggeringAction)
{
    // We grab a local copy of the URL because it's easy for callers to supply
    // a URL that will be deallocated during the execution of this function.
    // For example, see <https://bugs.webkit.org/show_bug.cgi?id=66360>.
    URL url = urlReference;

    // Create a new document before clearing the frame, because it may need to
    // inherit an aliased security context.
    Ref document = createDocument(url, documentIdentifier);
    
    Ref frame = *m_frame;
    CheckedRef frameLoader = frame->loader();

    // If the new document is for a Plugin but we're supposed to be sandboxed from Plugins,
    // then replace the document with one whose parser will ignore the incoming data (bug 39323)
    if (document->isPluginDocument() && document->isSandboxed(SandboxPlugins))
        document = SinkDocument::create(frame, url);

    // FIXME: Do we need to consult the content security policy here about blocked plug-ins?

    bool shouldReuseDefaultView = frameLoader->stateMachine().isDisplayingInitialEmptyDocument()
        && frame->document()->isSecureTransitionTo(url)
        && (frame->window() && !frame->window()->wasWrappedWithoutInitializedSecurityOrigin() && frame->window()->mayReuseForNavigation());

    if (shouldReuseDefaultView) {
        ASSERT(frameLoader->documentLoader());
        if (CheckedPtr contentSecurityPolicy = frameLoader->documentLoader()->contentSecurityPolicy())
            shouldReuseDefaultView = !(contentSecurityPolicy->sandboxFlags() & SandboxOrigin);
    }

    // Temporarily extend the lifetime of the existing document so that FrameLoader::clear() doesn't destroy it as
    // we need to retain its ongoing set of upgraded requests in new navigation contexts per <http://www.w3.org/TR/upgrade-insecure-requests/>
    // and we may also need to inherit its Content Security Policy below.
    RefPtr existingDocument = frame->document();

    Function<void()> handleDOMWindowCreation = [document, frame, shouldReuseDefaultView] {
        if (shouldReuseDefaultView)
            document->takeDOMWindowFrom(*frame->protectedDocument());
        else
            document->createDOMWindow();
    };

    frameLoader->clear(document.ptr(), !shouldReuseDefaultView, !shouldReuseDefaultView, true, WTFMove(handleDOMWindowCreation));
    clear();

    // frameLoader->clear() might fire unload event which could remove the view of the document.
    // Bail out if document has no view.
    if (!document->view())
        return false;

    if (!shouldReuseDefaultView)
        frame->checkedScript()->updatePlatformScriptObjects();

    frameLoader->setOutgoingReferrer(url);
    frame->setDocument(document.copyRef());

    if (RefPtr decoder = m_decoder)
        document->setDecoder(decoder.get());
    if (ownerDocument) {
        // |document| is the result of evaluating a JavaScript URL.
        document->setCookieURL(ownerDocument->cookieURL());
        document->setSecurityOriginPolicy(ownerDocument->securityOriginPolicy());
        document->setStrictMixedContentMode(ownerDocument->isStrictMixedContentMode());
        document->setCrossOriginEmbedderPolicy(ownerDocument->crossOriginEmbedderPolicy());

        document->setContentSecurityPolicy(makeUnique<ContentSecurityPolicy>(URL { url }, document));
        CheckedRef contentSecurityPolicy = *document->contentSecurityPolicy();
        CheckedRef ownerContentSecurityPolicy = *ownerDocument->contentSecurityPolicy();
        contentSecurityPolicy->copyStateFrom(ownerContentSecurityPolicy.ptr());
        contentSecurityPolicy->setInsecureNavigationRequestsToUpgrade(ownerContentSecurityPolicy->takeNavigationRequestsToUpgrade());
    } else if (url.protocolIsAbout() || url.protocolIsData()) {
        // https://html.spec.whatwg.org/multipage/origin.html#determining-navigation-params-policy-container
        RefPtr currentHistoryItem = frame->history().currentItem();

        if (currentHistoryItem && currentHistoryItem->policyContainer()) {
            const auto& policyContainerFromHistory = currentHistoryItem->policyContainer();
            ASSERT(policyContainerFromHistory);
            document->inheritPolicyContainerFrom(*policyContainerFromHistory);
        } else if (url == aboutSrcDocURL()) {
            RefPtr parentFrame = dynamicDowncast<LocalFrame>(frame->tree().parent());
            if (parentFrame && parentFrame->document()) {
                document->inheritPolicyContainerFrom(parentFrame->document()->policyContainer());
                document->checkedContentSecurityPolicy()->updateSourceSelf(parentFrame->document()->securityOrigin());
            }
        } else if (triggeringAction && triggeringAction->requester()) {
            document->inheritPolicyContainerFrom(triggeringAction->requester()->policyContainer);
            document->checkedContentSecurityPolicy()->updateSourceSelf(triggeringAction->requester()->securityOrigin);
        }

        // https://html.spec.whatwg.org/multipage/origin.html#requires-storing-the-policy-container-in-history
        if (triggeringAction && triggeringAction->type() != NavigationType::BackForward && currentHistoryItem)
            currentHistoryItem->setPolicyContainer(document->policyContainer());
    }

    if (existingDocument && existingDocument->contentSecurityPolicy() && document->contentSecurityPolicy())
        document->checkedContentSecurityPolicy()->setInsecureNavigationRequestsToUpgrade(existingDocument->checkedContentSecurityPolicy()->takeNavigationRequestsToUpgrade());

    frameLoader->didBeginDocument(dispatch);

    document->implicitOpen();

    // We grab a reference to the parser so that we'll always send data to the
    // original parser, even if the document acquires a new parser (e.g., via
    // document.open).
    m_parser = document->parser();

    if (frame->view() && frameLoader->client().hasHTMLView())
        frame->protectedView()->setContentsSize(IntSize());

    m_state = State::Started;
    return true;
}

TextResourceDecoder& DocumentWriter::decoder()
{
    if (!m_decoder) {
        Ref frame = *m_frame;
        Ref decoder = TextResourceDecoder::create(m_mimeType, frame->settings().defaultTextEncodingName(), frame->settings().usesEncodingDetector());
        m_decoder = decoder.copyRef();
        RefPtr parentFrame = dynamicDowncast<LocalFrame>(frame->tree().parent());
        // Set the hint encoding to the parent frame encoding only if
        // the parent and the current frames share the security origin.
        // We impose this condition because somebody can make a child frame
        // containing a carefully crafted html/javascript in one encoding
        // that can be mistaken for hintEncoding (or related encoding) by
        // an auto detector. When interpreted in the latter, it could be
        // an attack vector.
        // FIXME: This might be too cautious for non-7bit-encodings and
        // we may consider relaxing this later after testing.
        if (canReferToParentFrameEncoding(frame.ptr(), parentFrame.get()))
            decoder->setHintEncoding(parentFrame->document()->protectedDecoder().get());
        if (m_encoding.isEmpty()) {
            if (canReferToParentFrameEncoding(frame.ptr(), parentFrame.get()))
                decoder->setEncoding(parentFrame->document()->textEncoding(), TextResourceDecoder::EncodingFromParentFrame);
        } else {
            decoder->setEncoding(m_encoding,
                m_encodingWasChosenByUser ? TextResourceDecoder::UserChosenEncoding : TextResourceDecoder::EncodingFromHTTPHeader);
        }
        frame->protectedDocument()->setDecoder(WTFMove(decoder));
    }
    return *m_decoder;
}

void DocumentWriter::reportDataReceived()
{
    ASSERT(m_decoder);
    if (m_hasReceivedSomeData)
        return;
    m_hasReceivedSomeData = true;
    Ref document = *m_frame->document();
    if (m_decoder->encoding().usesVisualOrdering())
        document->setVisuallyOrdered();
    document->resolveStyle(Document::ResolveStyleType::Rebuild);
}

RefPtr<DocumentParser> DocumentWriter::protectedParser() const
{
    return m_parser;
}

void DocumentWriter::addData(const SharedBuffer& data)
{
    // FIXME: Change these to ASSERT once https://bugs.webkit.org/show_bug.cgi?id=80427 has been resolved.
    RELEASE_ASSERT(m_state != State::NotStarted);
    if (m_state == State::Finished) {
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT(m_parser);
    protectedParser()->appendBytes(*this, data.span());
}

void DocumentWriter::insertDataSynchronously(const String& markup)
{
    ASSERT(m_state != State::NotStarted);
    ASSERT(m_state != State::Finished);
    ASSERT(m_parser);
    protectedParser()->insert(markup);
}

void DocumentWriter::end()
{
    ASSERT(m_frame->page());
    ASSERT(m_frame->document());

    // The parser is guaranteed to be released after this point. begin() would
    // have to be called again before we can start writing more data.
    m_state = State::Finished;

    // http://bugs.webkit.org/show_bug.cgi?id=10854
    // The frame's last ref may be removed and it can be deleted by checkCompleted(), 
    // so we'll add a protective refcount
    Ref protectedFrame { *m_frame };

    if (!m_parser)
        return;
    // FIXME: m_parser->finish() should imply m_parser->flush().
    protectedParser()->flush(*this);
    if (!m_parser)
        return;
    protectedParser()->finish();
    m_parser = nullptr;
}

void DocumentWriter::setEncoding(const String& name, IsEncodingUserChosen isUserChosen)
{
    m_encoding = name;
    m_encodingWasChosenByUser = isUserChosen == IsEncodingUserChosen::Yes;
}

void DocumentWriter::setFrame(LocalFrame& frame)
{
    m_frame = frame;
}

void DocumentWriter::setDocumentWasLoadedAsPartOfNavigation()
{
    ASSERT(m_parser && !m_parser->isStopped());
    protectedParser()->setDocumentWasLoadedAsPartOfNavigation();
}

} // namespace WebCore
