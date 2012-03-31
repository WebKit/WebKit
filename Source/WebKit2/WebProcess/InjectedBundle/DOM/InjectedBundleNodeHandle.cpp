/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InjectedBundleNodeHandle.h"

#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include <JavaScriptCore/APICast.h>
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/HTMLFrameElement.h>
#include <WebCore/HTMLIFrameElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLTableCellElement.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/IntRect.h>
#include <WebCore/JSNode.h>
#include <WebCore/Node.h>
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;
using namespace HTMLNames;

namespace WebKit {

typedef HashMap<Node*, InjectedBundleNodeHandle*> DOMHandleCache;

static DOMHandleCache& domHandleCache()
{
    DEFINE_STATIC_LOCAL(DOMHandleCache, cache, ());
    return cache;
}

PassRefPtr<InjectedBundleNodeHandle> InjectedBundleNodeHandle::getOrCreate(JSContextRef, JSObjectRef object)
{
    Node* node = toNode(toJS(object));
    return getOrCreate(node);
}

PassRefPtr<InjectedBundleNodeHandle> InjectedBundleNodeHandle::getOrCreate(Node* node)
{
    if (!node)
        return 0;

    DOMHandleCache::AddResult result = domHandleCache().add(node, 0);
    if (!result.isNewEntry)
        return PassRefPtr<InjectedBundleNodeHandle>(result.iterator->second);

    RefPtr<InjectedBundleNodeHandle> nodeHandle = InjectedBundleNodeHandle::create(node);
    result.iterator->second = nodeHandle.get();
    return nodeHandle.release();
}

PassRefPtr<InjectedBundleNodeHandle> InjectedBundleNodeHandle::create(Node* node)
{
    return adoptRef(new InjectedBundleNodeHandle(node));
}

InjectedBundleNodeHandle::InjectedBundleNodeHandle(Node* node)
    : m_node(node)
{
}

InjectedBundleNodeHandle::~InjectedBundleNodeHandle()
{
    domHandleCache().remove(m_node.get());
}

Node* InjectedBundleNodeHandle::coreNode() const
{
    return m_node.get();
}

PassRefPtr<InjectedBundleNodeHandle> InjectedBundleNodeHandle::document()
{
    return getOrCreate(m_node->document());
}

// Additional DOM Operations
// Note: These should only be operations that are not exposed to JavaScript.

IntRect InjectedBundleNodeHandle::elementBounds() const
{
    if (!m_node->isElementNode())
        return IntRect();

    return static_cast<Element*>(m_node.get())->boundsInRootViewSpace();
}
    
IntRect InjectedBundleNodeHandle::renderRect(bool* isReplaced) const
{
    return m_node.get()->pixelSnappedRenderRect(isReplaced);
}

void InjectedBundleNodeHandle::setHTMLInputElementValueForUser(const String& value)
{
    if (!m_node->hasTagName(inputTag))
        return;

    static_cast<HTMLInputElement*>(m_node.get())->setValueForUser(value);
}

bool InjectedBundleNodeHandle::isHTMLInputElementAutofilled() const
{
    if (!m_node->hasTagName(inputTag))
        return false;
    
    return static_cast<HTMLInputElement*>(m_node.get())->isAutofilled();
}

void InjectedBundleNodeHandle::setHTMLInputElementAutofilled(bool filled)
{
    if (!m_node->hasTagName(inputTag))
        return;

    static_cast<HTMLInputElement*>(m_node.get())->setAutofilled(filled);
}

bool InjectedBundleNodeHandle::htmlInputElementLastChangeWasUserEdit()
{
    if (!m_node->hasTagName(inputTag))
        return false;

    return static_cast<HTMLInputElement*>(m_node.get())->lastChangeWasUserEdit();
}

bool InjectedBundleNodeHandle::htmlTextAreaElementLastChangeWasUserEdit()
{
    if (!m_node->hasTagName(textareaTag))
        return false;

    return static_cast<HTMLTextAreaElement*>(m_node.get())->lastChangeWasUserEdit();
}

PassRefPtr<InjectedBundleNodeHandle> InjectedBundleNodeHandle::htmlTableCellElementCellAbove()
{
    if (!m_node->hasTagName(tdTag))
        return 0;

    return getOrCreate(static_cast<HTMLTableCellElement*>(m_node.get())->cellAbove());
}

PassRefPtr<WebFrame> InjectedBundleNodeHandle::documentFrame()
{
    if (!m_node->isDocumentNode())
        return 0;

    Frame* frame = static_cast<Document*>(m_node.get())->frame();
    if (!frame)
        return 0;

    return static_cast<WebFrameLoaderClient*>(frame->loader()->client())->webFrame();
}

PassRefPtr<WebFrame> InjectedBundleNodeHandle::htmlFrameElementContentFrame()
{
    if (!m_node->hasTagName(frameTag))
        return 0;

    Frame* frame = static_cast<HTMLFrameElement*>(m_node.get())->contentFrame();
    if (!frame)
        return 0;

    return static_cast<WebFrameLoaderClient*>(frame->loader()->client())->webFrame();
}

PassRefPtr<WebFrame> InjectedBundleNodeHandle::htmlIFrameElementContentFrame()
{
    if (!m_node->hasTagName(iframeTag))
        return 0;

    Frame* frame = static_cast<HTMLIFrameElement*>(m_node.get())->contentFrame();
    if (!frame)
        return 0;

    return static_cast<WebFrameLoaderClient*>(frame->loader()->client())->webFrame();
}

} // namespace WebKit
