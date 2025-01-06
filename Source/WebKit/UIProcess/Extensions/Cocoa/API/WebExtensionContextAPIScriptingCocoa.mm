/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "APIError.h"
#import "CocoaHelpers.h"
#import "WKFrameInfoPrivate.h"
#import "WKNSError.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivate.h"
#import "WebExtensionAPIScripting.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionRegisteredScriptParameters.h"
#import "WebExtensionScriptInjectionParameters.h"
#import "WebExtensionTab.h"
#import "WebExtensionTabIdentifier.h"
#import "WebExtensionUtilities.h"
#import "_WKFrameHandle.h"
#import "_WKFrameTreeNode.h"
#import "_WKWebExtensionRegisteredScriptsSQLiteStore.h"
#import <WebCore/UserStyleSheet.h>
#import <wtf/BlockPtr.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {

using namespace WebExtensionDynamicScripts;

bool WebExtensionContext::isScriptingMessageAllowed()
{
    return isLoaded() && hasPermission(WKWebExtensionPermissionScripting);
}

void WebExtensionContext::scriptingExecuteScript(const WebExtensionScriptInjectionParameters& parameters, CompletionHandler<void(Expected<InjectionResults, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName= @"scripting.executeScript()";

    RefPtr tab = getTab(parameters.tabIdentifier.value());
    if (!tab) {
        completionHandler(toWebExtensionError(apiName, nil, @"tab not found"));
        return;
    }

    requestPermissionToAccessURLs({ tab->url() }, tab, [this, protectedThis = Ref { *this }, tab, parameters, completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        if (!tab->extensionHasPermission()) {
            completionHandler(toWebExtensionError(apiName, nil, @"this extension does not have access to this tab"));
            return;
        }

        auto *webView = tab->webView();
        if (!webView) {
            completionHandler(toWebExtensionError(apiName, nil, @"could not execute script on this tab"));
            return;
        }

        auto scriptPairs = getSourcePairsForParameters(parameters, *this);
        Ref executionWorld = toContentWorld(parameters.world);

        executeScript(scriptPairs, webView, executionWorld, *tab, parameters, *this, [completionHandler = WTFMove(completionHandler)](InjectionResults&& injectionResults) mutable {
            completionHandler(WTFMove(injectionResults));
        });
    });
}

void WebExtensionContext::scriptingInsertCSS(const WebExtensionScriptInjectionParameters& parameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName= @"scripting.insertCSS()";

    RefPtr tab = getTab(parameters.tabIdentifier.value());
    if (!tab) {
        completionHandler(toWebExtensionError(apiName, nil, @"tab not found"));
        return;
    }

    requestPermissionToAccessURLs({ tab->url() }, tab, [this, protectedThis = Ref { *this }, tab, parameters, completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        if (!tab->extensionHasPermission()) {
            completionHandler(toWebExtensionError(apiName, nil, @"this extension does not have access to this tab"));
            return;
        }

        auto *webView = tab->webView();
        if (!webView) {
            completionHandler(toWebExtensionError(apiName, nil, @"could not inject stylesheet on this tab"));
            return;
        }

        // FIXME: <https://webkit.org/b/262491> There is currently no way to inject CSS in specific frames based on ID's. If 'frameIds' is passed, default to the main frame.
        auto injectedFrames = parameters.frameIdentifiers ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames;

        auto styleSheetPairs = getSourcePairsForParameters(parameters, *this);
        injectStyleSheets(styleSheetPairs, webView, Ref { *m_contentScriptWorld }, parameters.styleLevel, injectedFrames, *this);

        completionHandler({ });
    });
}

void WebExtensionContext::scriptingRemoveCSS(const WebExtensionScriptInjectionParameters& parameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    if (m_dynamicallyInjectedUserStyleSheets.isEmpty()) {
        completionHandler({ });
        return;
    }

    static NSString * const apiName= @"scripting.removeCSS()";

    RefPtr tab = getTab(parameters.tabIdentifier.value());
    if (!tab) {
        completionHandler(toWebExtensionError(apiName, nil, @"tab not found"));
        return;
    }

    auto *webView = tab->webView();
    if (!webView) {
        completionHandler(toWebExtensionError(apiName, nil, @"could not remove stylesheet from this tab"));
        return;
    }

    // Allow removing CSS without permission, since it is not sensitive and the extension might have had permission before
    // and permission has been revoked since it inserted CSS. This allows for the extension to clean up.

    // FIXME: <https://webkit.org/b/262491> There is currently no way to inject CSS in specific frames based on ID's. If 'frameIds' is passed, default to the main frame.
    auto injectedFrames = parameters.frameIdentifiers ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames;

    auto styleSheetPairs = getSourcePairsForParameters(parameters, *this);
    removeStyleSheets(styleSheetPairs, webView, injectedFrames, *this);

    completionHandler({ });
}

void WebExtensionContext::scriptingRegisterContentScripts(const Vector<WebExtensionRegisteredScriptParameters>& scripts, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName= @"scripting.registerContentScripts()";

    DynamicInjectedContentsMap injectedContentsMap;
    NSString *errorMessage;
    if (!createInjectedContentForScripts(scripts, FirstTimeRegistration::Yes, injectedContentsMap, apiName, &errorMessage)) {
        completionHandler(toWebExtensionError(apiName, nil, errorMessage));
        return;
    }

    [registeredContentScriptsStore() addScripts:toWebAPI(scripts) completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, scripts, injectedContentsMap = WTFMove(injectedContentsMap), completionHandler = WTFMove(completionHandler)](NSString *errorMessage) mutable {
        if (errorMessage) {
            completionHandler(toWebExtensionError(apiName, nil, errorMessage));
            return;
        }

        InjectedContentVector injectedContents;
        for (auto& parameters : scripts) {
            auto injectedContent = injectedContentsMap.get(parameters.identifier);
            m_registeredScriptsMap.set(parameters.identifier, WebExtensionRegisteredScript::create(*this, parameters, injectedContent));
            injectedContents.append(WTFMove(injectedContent));
        }

        addInjectedContent(injectedContents);

        completionHandler({ });
    }).get()];
}

void WebExtensionContext::scriptingUpdateRegisteredScripts(const Vector<WebExtensionRegisteredScriptParameters>& scripts, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName= @"scripting.updateContentScripts()";

    Vector<WebExtensionRegisteredScriptParameters> updatedParameters;

    for (auto parameters : scripts) {
        auto scriptID = parameters.identifier;
        RefPtr registeredScript = m_registeredScriptsMap.get(scriptID);
        if (!registeredScript) {
            completionHandler(toWebExtensionError(apiName, nil, @"no existing script with ID '%@'", (NSString *)scriptID));
            return;
        }

        registeredScript->merge(parameters);
        updatedParameters.append(parameters);
    }

    NSString *errorMessage;
    DynamicInjectedContentsMap injectedContentsMap;
    if (!createInjectedContentForScripts(updatedParameters, FirstTimeRegistration::No, injectedContentsMap, apiName, &errorMessage)) {
        completionHandler(toWebExtensionError(apiName, nil, errorMessage));
        return;
    }

    auto *scriptsArray = toWebAPI(updatedParameters);
    [registeredContentScriptsStore() updateScripts:scriptsArray completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, scripts = WTFMove(updatedParameters), injectedContentsMap = WTFMove(injectedContentsMap), completionHandler = WTFMove(completionHandler)](NSString *errorMessage) mutable {
        if (errorMessage) {
            completionHandler(toWebExtensionError(apiName, nil, errorMessage));
            return;
        }

        InjectedContentVector injectedContents;
        for (auto& parameters : scripts) {
            auto scriptID = parameters.identifier;
            RefPtr registeredScript = m_registeredScriptsMap.get(scriptID);
            ASSERT(registeredScript);

            if (!registeredScript)
                continue;

            registeredScript->updateParameters(parameters);
            registeredScript->removeUserScriptsAndStyleSheets(scriptID);

            auto injectedContent = injectedContentsMap.get(scriptID);
            registeredScript->updateInjectedContent(injectedContent);
            injectedContents.append(WTFMove(injectedContent));
        }

        addInjectedContent(injectedContents);

        completionHandler({ });
    }).get()];
}

void WebExtensionContext::scriptingGetRegisteredScripts(const Vector<String>& scriptIDs, CompletionHandler<void(Expected<Vector<WebExtensionRegisteredScriptParameters>, WebExtensionError>&&)>&& completionHandler)
{
    Vector<WebExtensionRegisteredScriptParameters> scripts;

    if (scriptIDs.isEmpty()) {
        // Return all registered scripts if no filter is specififed.
        for (auto& entry : m_registeredScriptsMap)
            scripts.append(entry.value.get().parameters());
    } else {
        for (auto& scriptID : scriptIDs) {
            RefPtr registeredScript = m_registeredScriptsMap.get(scriptID);
            if (!registeredScript)
                continue;

            scripts.append(registeredScript->parameters());
        }
    }

    completionHandler(WTFMove(scripts));
}

void WebExtensionContext::scriptingUnregisterContentScripts(const Vector<String>& scriptIDs, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"scripting.unregisterContentScripts()";

    // Remove all registered scripts if no filter is specififed.
    auto ids = scriptIDs.isEmpty() ? copyToVector(m_registeredScriptsMap.keys()) : scriptIDs;

    for (auto& scriptID : ids) {
        if (!m_registeredScriptsMap.contains(scriptID)) {
            completionHandler(toWebExtensionError(apiName, nil, @"no script with ID '%@'", (NSString *)scriptID));
            return;
        }
    }

    [registeredContentScriptsStore() deleteScriptsWithIDs:createNSArray(ids).get() completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, ids, completionHandler = WTFMove(completionHandler)](NSString *errorMessage) mutable {
        if (errorMessage) {
            completionHandler(toWebExtensionError(apiName, nil, errorMessage));
            return;
        }

        auto removeUserScriptsAndStyleSheets = ^(String scriptID) {
            if (RefPtr registeredScript = m_registeredScriptsMap.take(scriptID))
                registeredScript->removeUserScriptsAndStyleSheets(scriptID);
        };

        for (auto& scriptID : ids)
            removeUserScriptsAndStyleSheets(scriptID);

        completionHandler({ });
    }).get()];
}

void WebExtensionContext::loadRegisteredContentScripts()
{
    if (!hasPermission(WKWebExtensionPermissionScripting))
        return;

    [registeredContentScriptsStore() getScriptsWithCompletionHandler:makeBlockPtr([this, protectedThis = Ref { *this }](NSArray *scripts, NSString *errorMessage) mutable {
        if (errorMessage) {
            RELEASE_LOG_ERROR(Extensions, "Unable to get registered scripts for extension %{private}@. Error: %{public}@", (NSString *)m_uniqueIdentifier, errorMessage);
            return;
        }

        Vector<WebExtensionRegisteredScriptParameters> parametersVector;
        if (!WebExtensionAPIScripting::parseRegisteredContentScripts(scripts, FirstTimeRegistration::Yes, parametersVector, &errorMessage)) {
            RELEASE_LOG_ERROR(Extensions, "Failed to parse injected content data for extension %{private}@. Error: %{public}@", (NSString *)m_uniqueIdentifier, errorMessage);
            return;
        }

        DynamicInjectedContentsMap injectedContentsMap;
        createInjectedContentForScripts(parametersVector, FirstTimeRegistration::Yes, injectedContentsMap, nil, &errorMessage);
        if (errorMessage) {
            RELEASE_LOG_ERROR(Extensions, "Failed to create injected content data for extension %{private}@. Error: %{public}@", (NSString *)m_uniqueIdentifier, errorMessage);
            return;
        }

        InjectedContentVector injectedContents;
        for (auto& parameters : parametersVector) {
            auto injectedContent = injectedContentsMap.get(parameters.identifier);
            m_registeredScriptsMap.set(parameters.identifier, WebExtensionRegisteredScript::create(*this, parameters, injectedContent));
            injectedContents.append(WTFMove(injectedContent));
        }

        addInjectedContent(injectedContents);
    }).get()];
}

void WebExtensionContext::clearRegisteredContentScripts()
{
    m_registeredScriptsMap.clear();

    [registeredContentScriptsStore() deleteDatabaseWithCompletionHandler:^(NSString *errorMessage) {
        if (errorMessage)
            RELEASE_LOG_ERROR(Extensions, "Failed to delete registered content scripts database. Error: %{public}@", errorMessage);
    }];
}

bool WebExtensionContext::createInjectedContentForScripts(const Vector<WebExtensionRegisteredScriptParameters>& scripts, FirstTimeRegistration firstTimeRegistration, DynamicInjectedContentsMap& injectedContentsMap, NSString *callingAPIName, NSString **errorMessage)
{
    Vector<String> idsToAdd;

    for (auto& parameters : scripts) {
        auto scriptID = parameters.identifier;

        if (firstTimeRegistration == FirstTimeRegistration::Yes && (m_registeredScriptsMap.contains(scriptID) || idsToAdd.contains(scriptID))) {
            *errorMessage = toErrorString(callingAPIName, nil, @"duplicate ID '%@'", (NSString *)scriptID);
            return false;
        }

        idsToAdd.append(scriptID);

        HashSet<Ref<WebExtensionMatchPattern>> includeMatchPatterns;
        HashSet<Ref<WebExtensionMatchPattern>> excludeMatchPatterns;

        // Optional. The list of JavaScript files to be injected into matching pages. These are injected in the order they appear in this array.
        auto *scriptPaths = parameters.js ? createNSArray(parameters.js.value()).get() : @[ ];
        scriptPaths = filterObjects(scriptPaths, ^(id key, NSString *string) {
            return !!string.length;
        });

        RefPtr extension = m_extension;
        for (NSString *scriptPath in scriptPaths) {
            RefPtr<API::Error> error;
            if (!extension->resourceStringForPath(scriptPath, error)) {
                recordError(::WebKit::wrapper(*error));
                *errorMessage = toErrorString(callingAPIName, nil, @"invalid resource '%@'", scriptPath);
                return false;
            }
        }

        // Optional. The list of CSS files to be injected into matching pages. These are injected in the order they appear in this array, before any DOM is constructed or displayed for the page.
        auto *styleSheetPaths = parameters.css ? createNSArray(parameters.css.value()).get() : @[ ];
        styleSheetPaths = filterObjects(styleSheetPaths, ^(id key, NSString *string) {
            return !!string.length;
        });

        for (NSString *styleSheetPath in styleSheetPaths) {
            RefPtr<API::Error> error;
            if (!extension->resourceStringForPath(styleSheetPath, error)) {
                recordError(::WebKit::wrapper(*error));
                *errorMessage = toErrorString(callingAPIName, nil, @"invalid resource '%@'", styleSheetPath);
                return false;
            }
        }

        // Required for first time registration. Specifies which pages the specified scripts and stylesheets will be injected into.
        auto *matchesArray = parameters.matchPatterns ? createNSArray(parameters.matchPatterns.value()).get() : @[ ];
        for (NSString *matchPatternString in matchesArray) {
            if (!matchPatternString.length) {
                *errorMessage = toErrorString(callingAPIName, nil, @"script with ID '%@' contains an empty match pattern", (NSString *)scriptID);
                return false;
            }

            RefPtr matchPattern = WebExtensionMatchPattern::getOrCreate(matchPatternString);
            if (!matchPattern || !matchPattern->isSupported()) {
                *errorMessage = toErrorString(callingAPIName, nil, @"script with ID '%@' has an invalid match pattern '%@'", (NSString *)scriptID, matchPatternString);
                return false;
            }

            includeMatchPatterns.add(matchPattern.releaseNonNull());
        }

        // Optional. Excludes pages that this content script would otherwise be injected into.
        auto *excludeMatchesArray = parameters.excludeMatchPatterns ? createNSArray(parameters.excludeMatchPatterns.value()).get() : @[ ];
        for (NSString *matchPatternString in excludeMatchesArray) {
            if (!matchPatternString.length) {
                *errorMessage = toErrorString(callingAPIName, nil, @"script with ID '%@' contains an empty exclude match pattern", (NSString *)scriptID);
                return false;
            }

            RefPtr matchPattern = WebExtensionMatchPattern::getOrCreate(matchPatternString);
            if (!matchPattern || !matchPattern->isSupported()) {
                *errorMessage = toErrorString(callingAPIName, nil, @"script with ID '%@' has an invalid exclude match pattern '%@'", (NSString *)scriptID, matchPatternString);
                return false;
            }
        }

        InjectedContentData injectedContentData;
        injectedContentData.identifier = parameters.identifier;
        injectedContentData.includeMatchPatterns = WTFMove(includeMatchPatterns);
        injectedContentData.excludeMatchPatterns = WTFMove(excludeMatchPatterns);
        injectedContentData.injectionTime = parameters.injectionTime.value_or(WebExtension::InjectionTime::DocumentIdle);
        injectedContentData.injectsIntoAllFrames = parameters.allFrames.value_or(false);
        injectedContentData.contentWorldType = parameters.world.value_or(WebExtensionContentWorldType::ContentScript);
        injectedContentData.styleLevel = parameters.styleLevel.value_or(WebCore::UserStyleLevel::Author);
        injectedContentData.scriptPaths = makeVector<String>(scriptPaths);
        injectedContentData.styleSheetPaths = makeVector<String>(styleSheetPaths);

        injectedContentsMap.add(scriptID, WTFMove(injectedContentData));
    }

    return true;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

