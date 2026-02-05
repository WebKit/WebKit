/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "HTMLFrameElementBase.h"

#include "ContainerNodeInlines.h"
#include "Document.h"
#include "DocumentEventLoop.h"
#include "DocumentQuirks.h"
#include "DocumentView.h"
#include "ElementInlines.h"
#include "EventLoop.h"
#include "FocusController.h"
#include "FrameLoader.h"
#include "HTMLNames.h"
#include "JSDOMBindingSecurity.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Page.h"
#include "RenderWidget.h"
#include "ScriptController.h"
#include "Settings.h"
#include "SubframeLoader.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HTMLFrameElementBase);

using namespace HTMLNames;

HTMLFrameElementBase::HTMLFrameElementBase(const QualifiedName& tagName, Document& document)
    : HTMLFrameOwnerElement(tagName, document, TypeFlag::HasCustomStyleResolveCallbacks)
{
}

bool HTMLFrameElementBase::canLoadScriptURL(const URL& scriptURL) const
{
    return canLoadURL(scriptURL);
}

bool HTMLFrameElementBase::canLoad() const
{
    // FIXME: Why is it valuable to return true when m_frameURL is empty?
    // FIXME: After openURL replaces an empty URL with the blank URL, this may no longer necessarily return true.
    return m_frameURL.isEmpty() || canLoadURL(m_frameURL);
}

bool HTMLFrameElementBase::canLoadURL(const String& relativeURL) const
{
    return canLoadURL(protect(document())->completeURL(relativeURL));
}

// Note that unlike HTMLPlugInElement::canLoadURL this uses ScriptController::canAccessFromCurrentOrigin.
bool HTMLFrameElementBase::canLoadURL(const URL& completeURL) const
{
    if (completeURL.protocolIsJavaScript()) {
        RefPtr contentDocument = this->contentDocument();
        if (contentDocument && !ScriptController::canAccessFromCurrentOrigin(contentDocument->protectedFrame().get(), protect(document()).get()))
            return false;
    }

    return !isProhibitedSelfReference(completeURL);
}

void HTMLFrameElementBase::openURL(LockHistory lockHistory, LockBackForwardList lockBackForwardList)
{
    if (!canLoad())
        return;

    if (m_frameURL.isEmpty())
        m_frameURL = AtomString { aboutBlankURL().string() };

    Ref document = this->document();
    RefPtr parentFrame { document->frame() };
    if (!parentFrame)
        return;

    auto frameName = getNameAttribute();
    if (frameName.isNull()) {
        if (document->settings().needsFrameNameFallbackToIdQuirk()) [[unlikely]]
            frameName = getIdAttribute();
    }

    auto completeURL = document->completeURL(m_frameURL);
    auto finishOpeningURL = [weakThis = WeakPtr { *this }, frameName, lockHistory, lockBackForwardList, parentFrame = WTF::move(parentFrame), completeURL] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (protectedThis->shouldLoadFrameLazily()) {
            parentFrame->loader().subframeLoader().createFrameIfNecessary(*protectedThis, frameName);
            return;
        }

        protect(protectedThis->document())->willLoadFrameElement(completeURL);
        parentFrame->loader().subframeLoader().requestFrame(*protectedThis, protectedThis->m_frameURL, frameName, lockHistory, lockBackForwardList);
    };

    document->quirks().triggerOptionalStorageAccessIframeQuirk(completeURL, WTF::move(finishOpeningURL));
}

void HTMLFrameElementBase::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    // FIXME: trimming whitespace is probably redundant with the URL parser
    if (name == srcdocAttr) {
        if (newValue.isNull())
            setLocation(attributeWithoutSynchronization(srcAttr).string().trim(isASCIIWhitespace));
        else
            setLocation(aboutSrcDocURL().string());
    } else if (name == srcAttr && !hasAttributeWithoutSynchronization(srcdocAttr))
        setLocation(newValue.string().trim(isASCIIWhitespace));
    else if (name == scrollingAttr && contentFrame())
        protectedContentFrame()->updateScrollingMode();
    else
        HTMLFrameOwnerElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

Node::InsertedIntoAncestorResult HTMLFrameElementBase::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLFrameOwnerElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
    return InsertedIntoAncestorResult::Done;
}

void HTMLFrameElementBase::didFinishInsertingNode()
{
    if (!isConnected())
        return;

    // DocumentFragments don't kick off any loads.
    Ref document = this->document();
    if (!document->frame())
        return;

    if (!SubframeLoadingDisabler::canLoadFrame(*this))
        return;

    if (!renderer())
        invalidateStyleAndRenderersForSubtree();

    auto work = [weakThis = WeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        protectedThis->m_openingURLAfterInserting = true;
        if (protectedThis->isConnected())
            protectedThis->openURL();
        protectedThis->m_openingURLAfterInserting = false;
    };
    if (!m_openingURLAfterInserting)
        work();
    else
        document->checkedEventLoop()->queueTask(TaskSource::DOMManipulation, WTF::move(work));
}

void HTMLFrameElementBase::didAttachRenderers()
{
    if (CheckedPtr part = renderWidget()) {
        if (RefPtr frame = contentFrame())
            part->setWidget(frame->virtualView());
    }
}

void HTMLFrameElementBase::setLocation(const String& str)
{
    if (document().settings().needsAcrobatFrameReloadingQuirk() && m_frameURL == str)
        return;

    if (!SubframeLoadingDisabler::canLoadFrame(*this))
        return;

    m_frameURL = AtomString(str);

    if (isConnected())
        openURL(LockHistory::No, LockBackForwardList::No);
}

void HTMLFrameElementBase::setLocation(JSC::JSGlobalObject& state, const String& newLocation)
{
    if (WTF::protocolIsJavaScript(newLocation)) {
        if (!BindingSecurity::shouldAllowAccessToNode(state, protectedContentDocument().get()))
            return;
    }

    setLocation(newLocation);
}

bool HTMLFrameElementBase::supportsFocus() const
{
    return true;
}

void HTMLFrameElementBase::setFocus(bool received, FocusVisibility visibility)
{
    HTMLFrameOwnerElement::setFocus(received, visibility);
    if (RefPtr page = document().page()) {
        CheckedRef focusController { page->focusController() };
        RefPtr contentFrame = this->contentFrame();
        if (received)
            focusController->setFocusedFrame(contentFrame.get());
        else if (focusController->focusedFrame() == contentFrame) // Focus may have already been given to another frame, don't take it away.
            focusController->setFocusedFrame(nullptr);
    }
}

bool HTMLFrameElementBase::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || attribute.name() == longdescAttr || HTMLFrameOwnerElement::isURLAttribute(attribute);
}

bool HTMLFrameElementBase::isHTMLContentAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcdocAttr || HTMLFrameOwnerElement::isHTMLContentAttribute(attribute);
}

ScrollbarMode HTMLFrameElementBase::scrollingMode() const
{
    auto scrollingAttribute = attributeWithoutSynchronization(scrollingAttr);
    return equalLettersIgnoringASCIICase(scrollingAttribute, "no"_s)
        || equalLettersIgnoringASCIICase(scrollingAttribute, "noscroll"_s)
        || equalLettersIgnoringASCIICase(scrollingAttribute, "off"_s)
        ? ScrollbarMode::AlwaysOff : ScrollbarMode::Auto;
}

} // namespace WebCore
