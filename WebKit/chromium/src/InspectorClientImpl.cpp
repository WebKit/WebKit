/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorClientImpl.h"

#include "DOMWindow.h"
#include "FloatRect.h"
#include "InspectorController.h"
#include "NotImplemented.h"
#include "Page.h"
#include "Settings.h"
#include "WebRect.h"
#include "WebURL.h"
#include "WebURLRequest.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include <wtf/Vector.h>

using namespace WebCore;

namespace WebKit {

InspectorClientImpl::InspectorClientImpl(WebViewImpl* webView)
    : m_inspectedWebView(webView)
{
    ASSERT(m_inspectedWebView);
}

InspectorClientImpl::~InspectorClientImpl()
{
}

void InspectorClientImpl::inspectorDestroyed()
{
    // Our lifetime is bound to the WebViewImpl.
}

Page* InspectorClientImpl::createPage()
{
    // This method should never be called in Chrome as inspector front-end lives
    // in a separate process.
    ASSERT_NOT_REACHED();
    return 0;
}

void InspectorClientImpl::showWindow()
{
    ASSERT(m_inspectedWebView->devToolsAgentPrivate());
    m_inspectedWebView->page()->inspectorController()->setWindowVisible(true);
}

void InspectorClientImpl::closeWindow()
{
    if (m_inspectedWebView->page())
        m_inspectedWebView->page()->inspectorController()->setWindowVisible(false);
}

bool InspectorClientImpl::windowVisible()
{
    ASSERT(m_inspectedWebView->devToolsAgentPrivate());
    return false;
}

void InspectorClientImpl::attachWindow()
{
    // FIXME: Implement this
}

void InspectorClientImpl::detachWindow()
{
    // FIXME: Implement this
}

void InspectorClientImpl::setAttachedWindowHeight(unsigned int height)
{
    // FIXME: Implement this
    notImplemented();
}

static void invalidateNodeBoundingRect(WebViewImpl* webView)
{
    // FIXME: Is it important to just invalidate the rect of the node region
    // given that this is not on a critical codepath?  In order to do so, we'd
    // have to take scrolling into account.
    const WebSize& size = webView->size();
    WebRect damagedRect(0, 0, size.width, size.height);
    if (webView->client())
        webView->client()->didInvalidateRect(damagedRect);
}

void InspectorClientImpl::highlight(Node* node)
{
    // InspectorController does the actually tracking of the highlighted node
    // and the drawing of the highlight. Here we just make sure to invalidate
    // the rects of the old and new nodes.
    hideHighlight();
}

void InspectorClientImpl::hideHighlight()
{
    // FIXME: able to invalidate a smaller rect.
    invalidateNodeBoundingRect(m_inspectedWebView);
}

void InspectorClientImpl::inspectedURLChanged(const String& newURL)
{
    // FIXME: Implement this
}

String InspectorClientImpl::localizedStringsURL()
{
    notImplemented();
    return String();
}

String InspectorClientImpl::hiddenPanels()
{
    // Enumerate tabs that are currently disabled.
    return "scripts,profiles,databases";
}

void InspectorClientImpl::populateSetting(const String& key, InspectorController::Setting& setting)
{
    loadSettings();
    if (m_settings->contains(key))
        setting = m_settings->get(key);
}

void InspectorClientImpl::storeSetting(const String& key, const InspectorController::Setting& setting)
{
    loadSettings();
    m_settings->set(key, setting);
    saveSettings();
}

void InspectorClientImpl::removeSetting(const String& key)
{
    loadSettings();
    m_settings->remove(key);
    saveSettings();
}

void InspectorClientImpl::inspectorWindowObjectCleared()
{
    notImplemented();
}

void InspectorClientImpl::loadSettings()
{
    if (m_settings)
        return;

    m_settings.set(new SettingsMap);
    String data = m_inspectedWebView->inspectorSettings();
    if (data.isEmpty())
        return;

    Vector<String> entries;
    data.split("\n", entries);
    for (Vector<String>::iterator it = entries.begin(); it != entries.end(); ++it) {
        Vector<String> tokens;
        it->split(":", tokens);
        if (tokens.size() != 3)
            continue;

        String name = decodeURLEscapeSequences(tokens[0]);
        String type = tokens[1];
        InspectorController::Setting setting;
        bool ok = true;
        if (type == "string")
            setting.set(decodeURLEscapeSequences(tokens[2]));
        else if (type == "double")
            setting.set(tokens[2].toDouble(&ok));
        else if (type == "integer")
            setting.set(static_cast<long>(tokens[2].toInt(&ok)));
        else if (type == "boolean")
            setting.set(tokens[2] == "true");
        else
            continue;

        if (ok)
            m_settings->set(name, setting);
    }
}

void InspectorClientImpl::saveSettings()
{
    String data;
    for (SettingsMap::iterator it = m_settings->begin(); it != m_settings->end(); ++it) {
        String entry;
        InspectorController::Setting value = it->second;
        String name = encodeWithURLEscapeSequences(it->first);
        switch (value.type()) {
        case InspectorController::Setting::StringType:
            entry = String::format(
                "%s:string:%s",
                name.utf8().data(),
                encodeWithURLEscapeSequences(value.string()).utf8().data());
            break;
        case InspectorController::Setting::DoubleType:
            entry = String::format(
                "%s:double:%f",
                name.utf8().data(),
                value.doubleValue());
            break;
        case InspectorController::Setting::IntegerType:
            entry = String::format(
                "%s:integer:%ld",
                name.utf8().data(),
                value.integerValue());
            break;
        case InspectorController::Setting::BooleanType:
            entry = String::format("%s:boolean:%s",
                                   name.utf8().data(),
                                   value.booleanValue() ? "true" : "false");
            break;
        case InspectorController::Setting::StringVectorType:
            notImplemented();
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        data.append(entry);
        data.append("\n");
    }
    m_inspectedWebView->setInspectorSettings(data);
    if (m_inspectedWebView->client())
        m_inspectedWebView->client()->didUpdateInspectorSettings();
}

} // namespace WebKit
