/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "Element.h"
#include "RenderObject.h"
#include "RenderWidget.h"
#include "RenderLayer.h"
#include "Page.h"
#include "Document.h"
#include "HTMLElement.h"
#include "DOMWindow.h"
#include "FrameLoadRequest.h"
#include "FrameLoaderClientQt.h"
#include "DOMImplementation.h"
#include "ResourceHandleInternal.h"
#include "Document.h"
#include "Settings.h"
#include "Plugin.h"
#include "FrameView.h"
#include "FramePrivate.h"
#include "GraphicsContext.h"
#include "HTMLDocument.h"
#include "ResourceHandle.h"
#include "FrameLoader.h"
#include "PlatformMouseEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "SelectionController.h"
#include "kjs_proxy.h"
#include "TypingCommand.h"
#include "JSLock.h"
#include "kjs_window.h"
#include "runtime_root.h"
#include <QScrollArea>

using namespace KJS;

#define notImplemented() qDebug("FIXME: UNIMPLEMENTED: %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__)

namespace WebCore {

// FIXME: Turned this off to fix buildbot. This function be either deleted or used.
#if 0
static void doScroll(const RenderObject* r, bool isHorizontal, int multiplier)
{
    // FIXME: The scrolling done here should be done in the default handlers
    // of the elements rather than here in the part.
    if (!r)
        return;

    //broken since it calls scroll on scrollbars
    //and we have none now
    //r->scroll(direction, KWQScrollWheel, multiplier);
    if (!r->layer())
        return;

    int x = r->layer()->scrollXOffset();
    int y = r->layer()->scrollYOffset();
    if (isHorizontal)
        x += multiplier;
    else
        y += multiplier;

    r->layer()->scrollToOffset(x, y, true, true);
}
#endif

FrameQt::FrameQt(Page* page, HTMLFrameOwnerElement* ownerElement,
                 FrameQtClient* frameClient, FrameLoaderClient* frameLoader)
    : Frame(page, ownerElement, frameLoader)
    , m_bindingRoot(0)
{
    m_client = frameClient;
    m_client->setFrame(this);
}

FrameQt::~FrameQt()
{
    setView(0);
    loader()->clearRecordedFormValues();

    loader()->cancelAndClear();
}

bool FrameQt::passMouseDownEventToWidget(Widget*)
{
    notImplemented();
    return false;
}

bool FrameQt::isLoadTypeReload()
{
    notImplemented();
    return false;
}

Range* FrameQt::markedTextRange() const
{
    // FIXME: Handle selections.
    return 0;
}

String FrameQt::mimeTypeForFileName(const String&) const
{
    notImplemented();
    return String();
}

void FrameQt::unfocusWindow()
{
    if (!view())
        return;

    if (!tree()->parent())
        view()->qwidget()->clearFocus();
}

void FrameQt::focusWindow()
{
    if (!view())
        return;

    if (!tree()->parent())
        view()->qwidget()->setFocus();
}

KJS::Bindings::Instance* FrameQt::getEmbedInstanceForWidget(Widget*)
{
    notImplemented();
    return 0;
}

KJS::Bindings::Instance* FrameQt::getObjectInstanceForWidget(Widget*)
{
    notImplemented();
    return 0;
}

KJS::Bindings::Instance* FrameQt::getAppletInstanceForWidget(Widget*)
{
    notImplemented();
    return 0;
}

void FrameQt::issueCutCommand()
{
    notImplemented();
}

void FrameQt::issueCopyCommand()
{
    notImplemented();
}

void FrameQt::issuePasteCommand()
{
    notImplemented();
}

void FrameQt::issuePasteAndMatchStyleCommand()
{
    notImplemented();
}

void FrameQt::issueTransposeCommand()
{
    notImplemented();
}

void FrameQt::respondToChangedSelection(const Selection& oldSelection, bool closeTyping)
{
    // TODO: If we want continous spell checking, we need to implement this.
}

bool FrameQt::shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity affinity, bool stillSelecting) const
{
    // no-op
    return true;
}

void FrameQt::print()
{
    notImplemented();
}

bool FrameQt::shouldInterruptJavaScript()
{
    notImplemented();
    return false;
}

bool FrameQt::keyEvent(const PlatformKeyboardEvent& keyEvent)
{
    bool result;

    // Check for cases where we are too early for events -- possible unmatched key up
    // from pressing return in the location bar.
    Document* doc = document();
    if (!doc)
        return false;

    Node* node = doc->focusedNode();
    if (!node) {
        if (doc->isHTMLDocument())
            node = doc->body();
        else
            node = doc->documentElement();

        if (!node)
            return false;
    }

#ifdef MULTIPLE_FORM_SUBMISSION_PROTECTION
    if (!keyEvent.isKeyUp())
        loader()->resetMultipleFormSubmissionProtection();
#endif
    
    result = !EventTargetNodeCast(node)->dispatchKeyEvent(keyEvent);

    // FIXME: FrameMac has a keyDown/keyPress hack here which we are not copying.
    return result;
}

void FrameQt::setFrameGeometry(const IntRect& r)
{
    setFrameGeometry(QRect(r));
}

FrameQtClient* FrameQt::client() const
{
    return m_client;
}

void FrameQt::createNewWindow(const FrameLoadRequest& request, const WindowFeatures& args, Frame*& frame)
{
    notImplemented();
}

void FrameQt::goBackOrForward(int)
{
    notImplemented();
}

KURL FrameQt::historyURL(int distance)
{
    notImplemented();
    return KURL();
}

void FrameQt::runJavaScriptAlert(const String& message) 
{
    notImplemented();
} 
 
bool FrameQt::runJavaScriptConfirm(const String& message) 
{
    notImplemented();
}

bool FrameQt::runJavaScriptPrompt(const String& message, const String& defaultValue, String& result) 
{
    notImplemented();
}

KJS::Bindings::RootObject* FrameQt::bindingRootObject() 
{
    ASSERT(settings()->isJavaScriptEnabled()); 
    if (!m_bindingRoot) {
        JSLock lock;
        // The root gets deleted by JavaScriptCore.
        m_bindingRoot = new KJS::Bindings::RootObject(0,
                                                      scriptProxy()->interpreter());
        addPluginRootObject(m_bindingRoot);
    }
    return m_bindingRoot;
}

void FrameQt::addPluginRootObject(KJS::Bindings::RootObject* root)
{
    m_rootObjects.append(root);
}

}
// vim: ts=4 sw=4 et
