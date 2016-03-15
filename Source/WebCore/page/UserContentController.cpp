/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include "UserContentController.h"

#include "DOMWrapperWorld.h"
#include "UserScript.h"
#include "UserStyleSheet.h"
#include <runtime/JSCellInlines.h>
#include <runtime/StructureInlines.h>

#if ENABLE(USER_MESSAGE_HANDLERS)
#include "UserMessageHandlerDescriptor.h"
#endif

#if ENABLE(CONTENT_EXTENSIONS)
#include "CompiledContentExtension.h"
#endif

namespace WebCore {

Ref<UserContentController> UserContentController::create()
{
    return adoptRef(*new UserContentController);
}

UserContentController::UserContentController()
{
}

UserContentController::~UserContentController()
{
}

void UserContentController::forEachUserScript(const std::function<void(DOMWrapperWorld&, const UserScript&)>& functor) const
{
    for (const auto& worldAndUserScriptVector : m_userScripts) {
        auto& world = *worldAndUserScriptVector.key.get();
        for (const auto& userScript : *worldAndUserScriptVector.value)
            functor(world, *userScript);
    }
}

void UserContentController::forEachUserStyleSheet(const std::function<void(const UserStyleSheet&)>& functor) const
{
    for (auto& styleSheetVector : m_userStyleSheets.values()) {
        for (const auto& styleSheet : *styleSheetVector)
            functor(*styleSheet);
    }
}

void UserContentController::addUserScript(DOMWrapperWorld& world, std::unique_ptr<UserScript> userScript)
{
    auto& scriptsInWorld = m_userScripts.ensure(&world, [&] { return std::make_unique<UserScriptVector>(); }).iterator->value;
    scriptsInWorld->append(WTFMove(userScript));
}

void UserContentController::removeUserScript(DOMWrapperWorld& world, const URL& url)
{
    auto it = m_userScripts.find(&world);
    if (it == m_userScripts.end())
        return;

    auto scripts = it->value.get();
    for (int i = scripts->size() - 1; i >= 0; --i) {
        if (scripts->at(i)->url() == url)
            scripts->remove(i);
    }

    if (scripts->isEmpty())
        m_userScripts.remove(it);
}

void UserContentController::removeUserScripts(DOMWrapperWorld& world)
{
    m_userScripts.remove(&world);
}

void UserContentController::addUserStyleSheet(DOMWrapperWorld& world, std::unique_ptr<UserStyleSheet> userStyleSheet, UserStyleInjectionTime injectionTime)
{
    auto& styleSheetsInWorld = m_userStyleSheets.ensure(&world, [&] { return std::make_unique<UserStyleSheetVector>(); }).iterator->value;
    styleSheetsInWorld->append(WTFMove(userStyleSheet));

    if (injectionTime == InjectInExistingDocuments)
        invalidateInjectedStyleSheetCacheInAllFramesInAllPages();
}

void UserContentController::removeUserStyleSheet(DOMWrapperWorld& world, const URL& url)
{
    auto it = m_userStyleSheets.find(&world);
    if (it == m_userStyleSheets.end())
        return;

    auto& stylesheets = *it->value;

    bool sheetsChanged = false;
    for (int i = stylesheets.size() - 1; i >= 0; --i) {
        if (stylesheets[i]->url() == url) {
            stylesheets.remove(i);
            sheetsChanged = true;
        }
    }

    if (!sheetsChanged)
        return;

    if (stylesheets.isEmpty())
        m_userStyleSheets.remove(it);

    invalidateInjectedStyleSheetCacheInAllFramesInAllPages();
}

void UserContentController::removeUserStyleSheets(DOMWrapperWorld& world)
{
    if (!m_userStyleSheets.remove(&world))
        return;

    invalidateInjectedStyleSheetCacheInAllFramesInAllPages();
}

#if ENABLE(USER_MESSAGE_HANDLERS)
void UserContentController::addUserMessageHandlerDescriptor(UserMessageHandlerDescriptor& descriptor)
{
    m_userMessageHandlerDescriptors.add(std::make_pair(descriptor.name(), &descriptor.world()), &descriptor);
}

void UserContentController::removeUserMessageHandlerDescriptor(UserMessageHandlerDescriptor& descriptor)
{
    m_userMessageHandlerDescriptors.remove(std::make_pair(descriptor.name(), &descriptor.world()));
}
#endif

#if ENABLE(CONTENT_EXTENSIONS)
void UserContentController::addUserContentExtension(const String& name, RefPtr<ContentExtensions::CompiledContentExtension> contentExtension)
{
    m_contentExtensionBackend.addContentExtension(name, contentExtension);
}

void UserContentController::removeUserContentExtension(const String& name)
{
    m_contentExtensionBackend.removeContentExtension(name);
}

void UserContentController::removeAllUserContentExtensions()
{
    m_contentExtensionBackend.removeAllContentExtensions();
}
#endif

void UserContentController::removeAllUserContent()
{
    m_userScripts.clear();

    if (!m_userStyleSheets.isEmpty()) {
        m_userStyleSheets.clear();
        invalidateInjectedStyleSheetCacheInAllFramesInAllPages();
    }
}

} // namespace WebCore
