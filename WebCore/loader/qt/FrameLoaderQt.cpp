/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
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
#include "FrameQt.h"

#include "DOMImplementation.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Document.h"
#include "EditorClientQt.h"
#include "Element.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FramePrivate.h"
#include "FrameLoaderClientQt.h"
#include "DocumentLoader.h"
#include "FrameView.h"
#include "FormState.h"
#include "GraphicsContext.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLFormElement.h"
#include "JSLock.h"
#include "MouseEventWithHitTestResults.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "Plugin.h"
#include "RenderLayer.h"
#include "RenderObject.h"
#include "RenderWidget.h"
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"
#include "SelectionController.h"
#include "Chrome.h"
#include "Settings.h"
#include "TypingCommand.h"
#include "kjs_window.h"
#include "runtime_root.h"

#include <QScrollArea>
#include <qdebug.h>

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d (%s)\n", __FILE__, __LINE__, __FUNCTION__); } while(0)

namespace WebCore {

void FrameLoader::submitForm(const FrameLoadRequest& frameLoadRequest, Event*)
{
#ifdef MULTIPLE_FORM_SUBMISSION_PROTECTION
    // FIXME: We'd like to remove this altogether and fix the multiple form submission issue another way.
    // We do not want to submit more than one form from the same page,
    // nor do we want to submit a single form more than once.
    // This flag prevents these from happening; not sure how other browsers prevent this.
    // The flag is reset in each time we start handle a new mouse or key down event, and
    // also in setView since this part may get reused for a page from the back/forward cache.
    // The form multi-submit logic here is only needed when we are submitting a form that affects this frame.
    // FIXME: Frame targeting is only one of the ways the submission could end up doing something other
    // than replacing this frame's content, so this check is flawed. On the other hand, the check is hardly
    // needed any more now that we reset m_submittedFormURL on each mouse or key down event.
    Frame* target = m_frame->tree()->find(frameLoadRequest.frameName());
    if (m_frame->tree()->isDescendantOf(target)) {
        if (m_submittedFormURL == frameLoadRequest.resourceRequest().url())
            return;
        m_submittedFormURL = frameLoadRequest.resourceRequest().url();
    }
#endif

    RefPtr<FormData> formData = frameLoadRequest.resourceRequest().httpBody();
    if (formData && !formData->isEmpty() && QtFrame(m_frame)->client())
        QtFrame(m_frame)->client()->submitForm(frameLoadRequest.resourceRequest().httpMethod(),
                                               frameLoadRequest.resourceRequest().url(),
                                               formData);

    clearRecordedFormValues();
}

void FrameLoader::urlSelected(const FrameLoadRequest& frameLoadRequest, Event*)
{
    const ResourceRequest& request = frameLoadRequest.resourceRequest();

    if (!QtFrame(m_frame)->client())
        return;

    QtFrame(m_frame)->client()->openURL(request.url());
}

void FrameLoader::setTitle(const String& title)
{
    client()->setTitle(title, URL());
}

Frame* FrameLoader::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement, const String& referrer)
{
    notImplemented();
    return 0;
}

ObjectContentType FrameLoader::objectContentType(const KURL&, const String& mimeType)
{
    notImplemented();
    return ObjectContentType();
}

Widget* FrameLoader::createPlugin(Element*, const KURL&, const Vector<String>&, const Vector<String>&, const String&)
{
    notImplemented();
    return 0;
}

Widget* FrameLoader::createJavaAppletWidget(const IntSize&, Element*, const HashMap<String, String>&)
{
    notImplemented();
    return 0;
}

KURL FrameLoader::originalRequestURL() const
{
    notImplemented();
    return KURL();
}

String FrameLoader::overrideMediaType() const
{
    // no-op
    return String();
}

int FrameLoader::getHistoryLength()
{
    notImplemented();
    return 0;
}

String FrameLoader::referrer() const
{
    notImplemented();
    return String();
}



void FrameLoader::checkLoadCompleteForThisFrame()
{
    ASSERT(m_client->hasWebView());
    //notImplemented();

    switch (m_state) {
    case FrameStateProvisional: {
    }

    case FrameStateCommittedPage: {
        DocumentLoader* dl = m_documentLoader.get();
        if (!dl || dl->isLoadingInAPISense())
            return;

        markLoadComplete();

        // FIXME: Is this subsequent work important if we already navigated away?
        // Maybe there are bugs because of that, or extra work we can skip because
        // the new page is ready.

        m_client->forceLayoutForNonHTML();
             
        // If the user had a scroll point, scroll to it, overriding the anchor point if any.
        if ((isBackForwardLoadType(m_loadType) || m_loadType == FrameLoadTypeReload)
            && m_client->hasBackForwardList())
            m_client->restoreScrollPositionAndViewState();

//         if (error)
//             m_client->dispatchDidFailLoad(error);
//         else
            m_client->dispatchDidFinishLoad();
            
        m_client->progressCompleted();
        return;
    }
        
    case FrameStateComplete:
        // Even if already complete, we might have set a previous item on a frame that
        // didn't do any data loading on the past transaction. Make sure to clear these out.
        m_client->frameLoadCompleted();
        return;
    }

}

void FrameLoader::goBackOrForward(int distance)
{
    notImplemented();
}

KURL FrameLoader::historyURL(int distance)
{
    notImplemented();
    return KURL();
}

void FrameLoader::didFirstLayout()
{
    if (isBackForwardLoadType(m_loadType) && m_client->hasBackForwardList())
        m_client->restoreScrollPositionAndViewState();

    m_firstLayoutDone = true;
    m_client->dispatchDidFirstLayout();
}

bool FrameLoader::canGoBackOrForward(int distance) const
{
    notImplemented();
    return false;
}

void FrameLoader::partClearedInBegin()
{
    if (m_frame->settings()->isJavaScriptEnabled())
        static_cast<FrameLoaderClientQt*>(m_client)->partClearedInBegin();
}

void FrameLoader::saveDocumentState()
{
    // Do not save doc state if the page has a password field and a form that would be submitted via https.
    //notImplemented();
}

void FrameLoader::restoreDocumentState()
{
    //notImplemented();
}

void FrameLoader::didChangeTitle(DocumentLoader* loader)
{
    notImplemented();
    m_client->didChangeTitle(loader);
}

void FrameLoader::redirectDataToPlugin(Widget* pluginWidget)
{
    notImplemented();
}


void FrameLoader::load(const FrameLoadRequest& request, bool userGesture, Event* event,
                       HTMLFormElement* submitForm, const HashMap<String, String>& formValues)
{
    notImplemented();
}

void FrameLoader::load(const KURL& URL, const String& referrer, FrameLoadType newLoadType,
    const String& frameName, Event* event, HTMLFormElement* form, const HashMap<String, String>& values)
{
    notImplemented();
}

void FrameLoader::load(DocumentLoader* newDocumentLoader)
{
    notImplemented();
}

void FrameLoader::load(const KURL&, Event*)
{
    notImplemented();
}

void FrameLoader::load(DocumentLoader* loader, FrameLoadType type, PassRefPtr<FormState> formState)
{
    notImplemented();
}

void FrameLoader::loadResourceSynchronously(const ResourceRequest& request, ResourceResponse& r, Vector<char>& data) {
    notImplemented();
}


PolicyCheck::PolicyCheck()
    : m_contentFunction(0)
{
}

void PolicyCheck::clear()
{
    clearRequest();
    m_contentFunction = 0;
}


void PolicyCheck::set(ContentPolicyDecisionFunction function, void* argument)
{
    m_formState = 0;
    m_frameName = String();

    m_contentFunction = function;
    m_argument = argument;
}

void PolicyCheck::call(bool shouldContinue)
{
    notImplemented();
}

void PolicyCheck::call(PolicyAction action)
{
    ASSERT(m_contentFunction);
    m_contentFunction(m_argument, action);
}

void PolicyCheck::clearRequest()
{
    m_formState = 0;
    m_frameName = String();
}

}

// vim: ts=4 sw=4 et
