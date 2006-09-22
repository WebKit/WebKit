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

#include <QScrollArea>
#include <QMessageBox>

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
    : Frame(page, ownerElement)
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
    closeURL();

    setView(0);
    clearRecordedFormValues();
}

bool FrameQt::openURL(const KURL& url)
{
    if (!m_client)
        return false;

    m_client->openURL(url);
    return true;
}

void FrameQt::submitForm(const ResourceRequest& request)
{
    // FIXME: this is a hack inherited from FrameMac, and should be pushed into Frame
    if (d->m_submittedFormURL == request.url())
        return;

    d->m_submittedFormURL = request.url();

    if (m_client)
        m_client->submitForm(request.doPost() ? "POST" : "GET", request.url(), &request.postData);

    clearRecordedFormValues();
}

void FrameQt::urlSelected(const ResourceRequest& request)
{
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
    QMessageBox::information(view()->qwidget(), "JavaScript", message);
}

bool FrameQt::runJavaScriptConfirm(String const& message)
{
    notImplemented();
    return true;
}

bool FrameQt::locationbarVisible()
{
    notImplemented();
    return true;
}

void FrameQt::setTitle(const String& title)
{
    if (view() && view()->parentWidget())
        view()->parentWidget()->setWindowTitle(title);
}

Frame* FrameQt::createFrame(const KURL&, const String& name, Element*, const String& referrer)
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
    if (mev.targetNode() == 0)
        return true;
 
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
    notImplemented();
    return true;
}

bool FrameQt::personalbarVisible()
{
    notImplemented();
    return true;
}

bool FrameQt::statusbarVisible()
{
    notImplemented();
    return true;
}

bool FrameQt::toolbarVisible()
{
    notImplemented();
    return true;
}

void FrameQt::createEmptyDocument()
{
    // FIXME: Implement like described in this comment from FrameMac:
    //
    // Although it's not completely clear from the name of this function,
    // it does nothing if we already have a document, and just creates an
    // empty one if we have no document at all.
}

Range* FrameQt::markedTextRange() const
{
    // notImplemented();
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
    notImplemented();
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

void FrameQt::openURLRequest(const ResourceRequest&)
{
    notImplemented();
}

void FrameQt::scheduleClose()
{
    notImplemented();
}

void FrameQt::unfocusWindow()
{
    notImplemented();
}

void FrameQt::focusWindow()
{
    notImplemented();
}

String FrameQt::overrideMediaType() const
{
    return String();
}

void FrameQt::addMessageToConsole(const String& message, unsigned lineNumber, const String& sourceID)
{
    qDebug("[FrameQt::addMessageToConsole] message=%s lineNumber=%d sourceID=%s",
           qPrintable(QString(message)), lineNumber, qPrintable(QString(sourceID)));
}

bool FrameQt::runJavaScriptPrompt(const String& message, const String& defaultValue, String& result)
{
    notImplemented();
    return false;
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
    notImplemented();
    return 0;
}

void FrameQt::registerCommandForUndo(PassRefPtr<EditCommand>)
{
    notImplemented();
}

void FrameQt::registerCommandForRedo(PassRefPtr<EditCommand>)
{
    notImplemented();
}

void FrameQt::clearUndoRedoOperations()
{
    // FIXME: Implement this as soon a KPart is created...
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
    notImplemented();
}

void FrameQt::respondToChangedContents(const Selection& endingSelection)
{
    notImplemented();
}

bool FrameQt::shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity affinity, bool stillSelecting) const
{
    notImplemented();
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
    notImplemented();
    return false;
}

void FrameQt::handledOnloadEvents()
{
    // no-op
}

bool FrameQt::canPaste() const
{
    notImplemented();
    return false;
}

bool FrameQt::canRedo() const
{
    notImplemented();
    return false;
}

bool FrameQt::canUndo() const
{
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

}

// vim: ts=4 sw=4 et
