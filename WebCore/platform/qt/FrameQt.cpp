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
#include "DOMImplementation.h"
#include "BrowserExtensionQt.h"
#include "ResourceLoaderInternal.h"
#include "Document.h"
#include "Settings.h"
#include "Plugin.h"
#include "FramePrivate.h"
#include "GraphicsContext.h"
#include "HTMLDocument.h"
#include "ResourceLoader.h"
#include "PlatformMouseEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "SelectionController.h"
#include "TypingCommand.h"
#include "JSLock.h"
#include "kjs_window.h"
#include "runtime_root.h"

#include <QScrollArea>

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); } while(0)

namespace WebCore {

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

FrameQt::FrameQt(Page* page, Element* ownerElement, FrameQtClient* client)
    : Frame(page, ownerElement, 0 /* editingClient */)
    , m_bindingRoot(0)
{
    d->m_extension = new BrowserExtensionQt(this);
    Settings* settings = new Settings;
    settings->setAutoLoadImages(true);
    settings->setMinFontSize(5);
    settings->setMinLogicalFontSize(5);
    settings->setShouldPrintBackgrounds(true);
    settings->setIsJavaScriptEnabled(true);
 
    settings->setMediumFixedFontSize(14);
    settings->setMediumFontSize(14);
    settings->setSerifFontName("Times New Roman");
    settings->setSansSerifFontName("Arial");
    settings->setFixedFontName("Courier");
    settings->setStdFontName("Arial");

    setSettings(settings);

    m_client = client;
    m_client->setFrame(this);
}

FrameQt::~FrameQt()
{
    setView(0);
    clearRecordedFormValues();

    cancelAndClear();
}

bool FrameQt::openURL(const KURL& url)
{
    if (!m_client)
        return false;

    m_client->openURL(url);
    return true;
}

void FrameQt::submitForm(const FrameLoadRequest& frameLoadRequest)
{
    ResourceRequest request = frameLoadRequest.m_request;

    // FIXME: this is a hack inherited from FrameMac, and should be pushed into Frame
    if (d->m_submittedFormURL == request.url())
        return;

    d->m_submittedFormURL = request.url();

    if (m_client)
        m_client->submitForm(request.httpMethod(), request.url(), &request.httpBody());

    clearRecordedFormValues();
}

void FrameQt::urlSelected(const FrameLoadRequest& frameLoadRequest, const Event*)
{
    ResourceRequest request = frameLoadRequest.m_request;

    if (!m_client)
        return;

    m_client->openURL(request.url());
}

String FrameQt::userAgent() const
{
    return "Mozilla/5.0 (PC; U; Intel; Linux; en) AppleWebKit/420+ (KHTML, like Gecko)";
}

void FrameQt::runJavaScriptAlert(String const& message)
{
    m_client->runJavaScriptAlert(message);
}

bool FrameQt::runJavaScriptConfirm(String const& message)
{
    return m_client->runJavaScriptConfirm(message);
}

bool FrameQt::locationbarVisible()
{
    return m_client->locationbarVisible();
}

void FrameQt::setTitle(const String& title)
{
    if (view() && view()->parentWidget())
        view()->parentWidget()->setWindowTitle(title);
}

Frame* FrameQt::createFrame(const KURL& url, const String& name, Element* ownerElement, const String& referrer)
{
    notImplemented();
    return 0;
}

bool FrameQt::passWheelEventToChildWidget(Node*)
{
    notImplemented();
    return false;
}

bool FrameQt::passSubframeEventToSubframe(MouseEventWithHitTestResults& mev, Frame*)
{
    notImplemented(); 
    return false;
}

ObjectContentType FrameQt::objectContentType(const KURL&, const String& mimeType)
{
    notImplemented();
    return ObjectContentType();
}

Plugin* FrameQt::createPlugin(Element*, const KURL&, const Vector<String>&, const Vector<String>&, const String&)
{
    notImplemented();
    return 0;
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

bool FrameQt::menubarVisible()
{
    return m_client->menubarVisible();
}

bool FrameQt::personalbarVisible()
{
    return m_client->personalbarVisible();
}

bool FrameQt::statusbarVisible()
{
    return m_client->statusbarVisible();
}

bool FrameQt::toolbarVisible()
{
    return m_client->toolbarVisible();
}

void FrameQt::createEmptyDocument()
{
    // Although it's not completely clear from the name of this function,
    // it does nothing if we already have a document, and just creates an
    // empty one if we have no document at all.

    // Force creation.
    if (!d->m_doc) {
        begin();
        end();
    }
}

Range* FrameQt::markedTextRange() const
{
    // FIXME: Handle selections.
    return 0;
}

String FrameQt::incomingReferrer() const
{
    notImplemented();
    return String();
}

String FrameQt::mimeTypeForFileName(const String&) const
{
    notImplemented();
    return String();
}

void FrameQt::markMisspellingsInAdjacentWords(const VisiblePosition&)
{
    notImplemented();
}

void FrameQt::markMisspellings(const Selection&)
{
    notImplemented();
}

bool FrameQt::lastEventIsMouseUp() const
{
    // no-op
    return false;
}

void FrameQt::saveDocumentState()
{
    // FIXME: Implement this as soon a KPart is created...
}

void FrameQt::restoreDocumentState()
{
    // FIXME: Implement this as soon a KPart is created...
}

void FrameQt::openURLRequest(const FrameLoadRequest& request)
{
    urlSelected(request, 0);
}

void FrameQt::scheduleClose()
{
    // no-op
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

String FrameQt::overrideMediaType() const
{
    // no-op
    return String();
}

void FrameQt::addMessageToConsole(const String& message, unsigned lineNumber, const String& sourceID)
{
    qDebug("[FrameQt::addMessageToConsole] message=%s lineNumber=%d sourceID=%s",
           qPrintable(QString(message)), lineNumber, qPrintable(QString(sourceID)));
}

bool FrameQt::runJavaScriptPrompt(const String& message, const String& defaultValue, String& result)
{
    return m_client->runJavaScriptPrompt(message, defaultValue, result);
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

KJS::Bindings::RootObject* FrameQt::bindingRootObject()
{
    ASSERT(jScriptEnabled());

    if (!m_bindingRoot) {
        KJS::JSLock lock;
        m_bindingRoot = new KJS::Bindings::RootObject(0); // The root gets deleted by JavaScriptCore.

        KJS::JSObject* win = KJS::Window::retrieveWindow(this);
        m_bindingRoot->setRootObjectImp(win);
        m_bindingRoot->setInterpreter(jScript()->interpreter());
        addPluginRootObject(m_bindingRoot);
    }

    return m_bindingRoot;
}

void FrameQt::addPluginRootObject(KJS::Bindings::RootObject* root)
{
    m_rootObjects.append(root);
}

Widget* FrameQt::createJavaAppletWidget(const IntSize&, Element*, const HashMap<String, String>&)
{
    notImplemented();
    return 0; 
}

void FrameQt::registerCommandForUndo(PassRefPtr<EditCommand>)
{
    // FIXME: Implement this in the FrameQtClient, to be used within WebKitPart.
}

void FrameQt::registerCommandForRedo(PassRefPtr<EditCommand>)
{
    // FIXME: Implement this in the FrameQtClient, to be used within WebKitPart.
}

void FrameQt::clearUndoRedoOperations()
{
    // FIXME: Implement this in the FrameQtClient, to be used within WebKitPart.
}

void FrameQt::issueUndoCommand()
{
    notImplemented();
}

void FrameQt::issueRedoCommand()
{
    notImplemented();
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

void FrameQt::respondToChangedContents(const Selection& endingSelection)
{
    // TODO: If we want accessibility, we need to implement this.
}

bool FrameQt::shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity affinity, bool stillSelecting) const
{
    // no-op
    return true;
}

void FrameQt::partClearedInBegin()
{
    // FIXME: This is only related to the js debugger.
    // See WebCoreSupport/WebFrameBridge.m "windowObjectCleared",
    // which is called by FrameMac::partClearedInBegin() ...
}

bool FrameQt::canGoBackOrForward(int distance) const
{
    // FIXME: Implement this in the FrameQtClient, to be used within WebKitPart.
    notImplemented();
    return false;
}

void FrameQt::handledOnloadEvents()
{
    // TODO: FrameMac doesn't need that - it seems.
    // It must be handled differently, can't figure it out.
    // If we won't call this here doc->parsing() remains 'true'
    // all the time. Calling document.open(); document.write(...)
    // from JavaScript leaves the parsing state 'true', and DRT will
    // hang on these tests (fast/dom/Document/document-reopen.html for instance)
    endIfNotLoading();
}

bool FrameQt::canPaste() const
{
    // FIXME: Implement this in the FrameQtClient, to be used within WebKitPart. 
    notImplemented();
    return false;
}

bool FrameQt::canRedo() const
{
    // FIXME: Implement this in the FrameQtClient, to be used within WebKitPart. 
    notImplemented();
    return false;
}

bool FrameQt::canUndo() const
{
    // FIXME: Implement this in the FrameQtClient, to be used within WebKitPart. 
    notImplemented();
    return false;
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

KURL FrameQt::originalRequestURL() const
{
    notImplemented();
    return KURL();
}

bool FrameQt::keyEvent(const PlatformKeyboardEvent& keyEvent)
{
    bool result;

    // Check for cases where we are too early for events -- possible unmatched key up
    // from pressing return in the location bar.
    Document* doc = document();
    if (!doc)
        return false;

    Node* node = doc->focusNode();
    if (!node) {
        if (doc->isHTMLDocument())
            node = doc->body();
        else
            node = doc->documentElement();

        if (!node)
            return false;
    }

    if (!keyEvent.isKeyUp())
        prepareForUserAction();

    result = !EventTargetNodeCast(node)->dispatchKeyEvent(keyEvent);

    // FIXME: FrameMac has a keyDown/keyPress hack here which we are not copying.
    return result;
}

void FrameQt::setFrameGeometry(const IntRect& r)
{
    setFrameGeometry(QRect(r));
}

void FrameQt::tokenizerProcessedData()
{
    if (d->m_doc)
        checkCompleted();
}

FrameQtClient* FrameQt::client() const
{
    return m_client;
}

}

// vim: ts=4 sw=4 et
