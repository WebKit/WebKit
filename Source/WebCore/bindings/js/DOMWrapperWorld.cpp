/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "DOMWrapperWorld.h"

#include "HTMLAudioElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLImageElement.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HTMLScriptElement.h"
#include "HTMLStyleElement.h"
#include "JSDOMWindow.h"
#include "JSNode.h"
#include "ProcessingInstruction.h"
#include "ScriptController.h"
#include "StyleSheet.h"
#include "StyledElement.h"
#include "WebCoreJSClientData.h"
#include <heap/Weak.h>

using namespace JSC;

namespace WebCore {

using namespace HTMLNames;

static bool isObservable(JSNode* jsNode, Node* node, DOMWrapperWorld* world)
{
    // Certain conditions implicitly make existence of a JS DOM node wrapper observable
    // through the DOM, even if no explicit reference to it remains.
    
    // The DOM doesn't know how to keep a tree of nodes alive without the root
    // being explicitly referenced. So, we artificially treat the root of
    // every tree as observable.
    // FIXME: Resolve this lifetime issue in the DOM, and remove this inefficiency.
    if (!node->parentNode())
        return true;

    // If a node is in the document, and its wrapper has custom properties,
    // the wrapper is observable because future access to the node through the
    // DOM must reflect those properties.
    if (jsNode->hasCustomProperties())
        return true;

    // If a node is in the document, and has event listeners, its wrapper is
    // observable because its wrapper is responsible for marking those event listeners.
    if (node->hasEventListeners())
        return true;

    // If a node owns another object with a wrapper with custom properties,
    // the wrapper must be treated as observable, because future access to
    // those objects through the DOM must reflect those properties.
    // FIXME: It would be better if this logic could be in the node next to
    // the custom markChildren functions rather than here.
    // Note that for some compound objects like stylesheets and CSSStyleDeclarations,
    // we don't descend to check children for custom properties, and just conservatively
    // keep the node wrappers protecting them alive.
    if (node->isElementNode()) {
        if (NamedNodeMap* attributes = static_cast<Element*>(node)->attributeMap()) {
            if (JSDOMWrapper* wrapper = world->m_wrappers.get(attributes).get()) {
                // FIXME: This check seems insufficient, because NamedNodeMap items can have custom properties themselves.
                // Maybe it would be OK to just keep the wrapper alive, as it is done for CSSOM objects below.
                if (wrapper->hasCustomProperties())
                    return true;
            }
        }
        if (node->isStyledElement()) {
            if (CSSMutableStyleDeclaration* style = static_cast<StyledElement*>(node)->inlineStyleDecl()) {
                if (world->m_wrappers.get(style))
                    return true;
            }
        }
        if (static_cast<Element*>(node)->hasTagName(canvasTag)) {
            if (CanvasRenderingContext* context = static_cast<HTMLCanvasElement*>(node)->renderingContext()) {
                if (JSDOMWrapper* wrapper = world->m_wrappers.get(context).get()) {
                    if (wrapper->hasCustomProperties())
                        return true;
                }
            }
        } else if (static_cast<Element*>(node)->hasTagName(linkTag)) {
            if (StyleSheet* sheet = static_cast<HTMLLinkElement*>(node)->sheet()) {
                if (world->m_wrappers.get(sheet))
                    return true;
            }
        } else if (static_cast<Element*>(node)->hasTagName(styleTag)) {
            if (StyleSheet* sheet = static_cast<HTMLStyleElement*>(node)->sheet()) {
                if (world->m_wrappers.get(sheet))
                    return true;
            }
        }
    } else if (node->nodeType() == Node::PROCESSING_INSTRUCTION_NODE) {
        if (StyleSheet* sheet = static_cast<ProcessingInstruction*>(node)->sheet()) {
            if (world->m_wrappers.get(sheet))
                return true;
        }
    }

    return false;
}

static inline bool isReachableFromDOM(JSNode* jsNode, Node* node, DOMWrapperWorld* world, MarkStack& markStack)
{
    if (!node->inDocument()) {
        // If a wrapper is the last reference to an image or script element
        // that is loading but not in the document, the wrapper is observable
        // because it is the only thing keeping the image element alive, and if
        // the image element is destroyed, its load event will not fire.
        // FIXME: The DOM should manage this issue without the help of JavaScript wrappers.
        if (node->hasTagName(imgTag) && !static_cast<HTMLImageElement*>(node)->haveFiredLoadEvent())
            return true;
        if (node->hasTagName(scriptTag) && !static_cast<HTMLScriptElement*>(node)->haveFiredLoadEvent())
            return true;
    #if ENABLE(VIDEO)
        if (node->hasTagName(audioTag) && !static_cast<HTMLAudioElement*>(node)->paused())
            return true;
    #endif

        // If a node is firing event listeners, its wrapper is observable because
        // its wrapper is responsible for marking those event listeners.
        if (node->isFiringEventListeners())
            return true;
    }

    return isObservable(jsNode, node, world) && markStack.containsOpaqueRoot(root(node));
}

DOMWrapperWorld::DOMWrapperWorld(JSC::JSGlobalData* globalData, bool isNormal)
    : m_globalData(globalData)
    , m_isNormal(isNormal)
    , m_jsNodeHandleOwner(this)
    , m_domObjectHandleOwner(this)
{
    JSGlobalData::ClientData* clientData = m_globalData->clientData;
    ASSERT(clientData);
    static_cast<WebCoreJSClientData*>(clientData)->rememberWorld(this);
}

DOMWrapperWorld::~DOMWrapperWorld()
{
    JSGlobalData::ClientData* clientData = m_globalData->clientData;
    ASSERT(clientData);
    static_cast<WebCoreJSClientData*>(clientData)->forgetWorld(this);
}

void DOMWrapperWorld::clearWrappers()
{
    m_wrappers.clear();
    m_stringCache.clear();
}

DOMWrapperWorld* normalWorld(JSC::JSGlobalData& globalData)
{
    JSGlobalData::ClientData* clientData = globalData.clientData;
    ASSERT(clientData);
    return static_cast<WebCoreJSClientData*>(clientData)->normalWorld();
}

DOMWrapperWorld* mainThreadNormalWorld()
{
    ASSERT(isMainThread());
    static DOMWrapperWorld* cachedNormalWorld = normalWorld(*JSDOMWindow::commonJSGlobalData());
    return cachedNormalWorld;
}

bool JSNodeHandleOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, JSC::MarkStack& markStack)
{
    JSNode* jsNode = static_cast<JSNode*>(handle.get().asCell());
    return isReachableFromDOM(jsNode, jsNode->impl(), m_world, markStack);
}

void JSNodeHandleOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    JSNode* jsNode = static_cast<JSNode*>(handle.get().asCell());
    uncacheDOMNodeWrapper(m_world, static_cast<Node*>(context), jsNode);
}

bool DOMObjectHandleOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown>, void*, JSC::MarkStack&)
{
    return false;
}

void DOMObjectHandleOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    JSDOMWrapper* domObject = static_cast<JSDOMWrapper*>(handle.get().asCell());
    uncacheDOMObjectWrapper(m_world, context, domObject);
}

} // namespace WebCore
