/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "WebExtensionAPIScripting.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "APIObject.h"
#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtension.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionContentWorldType.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionDynamicScripts.h"
#import "WebExtensionFrameIdentifier.h"
#import "WebExtensionRegisteredScriptParameters.h"
#import "WebExtensionScriptInjectionParameters.h"
#import "WebExtensionScriptInjectionResultParameters.h"
#import "WebExtensionTabIdentifier.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"
#import <wtf/cocoa/VectorCocoa.h>

static NSString * const allFramesKey = @"allFrames";
static NSString * const argsKey = @"args";
static NSString * const argumentsKey = @"arguments";
static NSString * const cssKey = @"css";
static NSString * const cssOriginKey = @"cssOrigin";
static NSString * const documentIDsKey = @"documentIds";
static NSString * const filesKey = @"files";
static NSString * const frameIDsKey = @"frameIds";
static NSString * const funcKey = @"func";
static NSString * const functionKey = @"function";
static NSString * const originKey = @"origin";
static NSString * const tabIDKey = @"tabId";
static NSString * const targetKey = @"target";
static NSString * const worldKey = @"world";

static NSString * const excludeMatchesKey = @"excludeMatches";
static NSString * const idKey = @"id";
static NSString * const idsKey = @"ids";
static NSString * const jsKey = @"js";
static NSString * const matchesKey = @"matches";
static NSString * const persistAcrossSessionsKey = @"persistAcrossSessions";
static NSString * const runAtKey = @"runAt";

static NSString * const mainWorld = @"main";
static NSString * const isolatedWorld = @"isolated";

static NSString * const authorValue = @"author";
static NSString * const userValue = @"user";

static NSString * const documentEnd = @"document_end";
static NSString * const documentIdle = @"document_idle";
static NSString * const documentStart = @"document_start";

// FIXME: <https://webkit.org/b/261765> Consider adding support for injectImmediately.
// FIXME: <https://webkit.org/b/264829> Add support for matchOriginAsFallback.

namespace WebKit {

using namespace WebExtensionDynamicScripts;

NSArray *toWebAPI(const Vector<WebExtensionScriptInjectionResultParameters>& parametersVector, bool returnExecutionResultOnly)
{
    auto *results = [NSMutableArray arrayWithCapacity:parametersVector.size()];

    // tabs.executeScript() only returns an array of the injection result.
    if (returnExecutionResultOnly) {
        for (auto& parameters : parametersVector) {
            id result = parameters.resultJSON ? parseJSON(parameters.resultJSON.value(), JSONOptions::FragmentsAllowed) : nil;
            [results addObject:result ?: NSNull.null];
        }

        return [results copy];
    }

    for (auto& parameters : parametersVector) {
        auto *result = [NSMutableDictionary dictionaryWithCapacity:3];

        id value = parameters.resultJSON ? parseJSON(parameters.resultJSON.value(), JSONOptions::FragmentsAllowed) : nil;
        result[@"result"] = value ?: NSNull.null;

        ASSERT(parameters.frameIdentifier);
        if (parameters.frameIdentifier)
            result[@"frameId"] = @(WebKit::toWebAPI(parameters.frameIdentifier.value()));

        ASSERT(parameters.documentIdentifier);
        if (parameters.documentIdentifier)
            result[@"documentId"] = parameters.documentIdentifier.value().toString();

        if (parameters.error)
            result[@"error"] = parameters.error.value();

        [results addObject:[result copy]];
    }

    return [results copy];
}

NSDictionary *toWebAPI(const WebExtensionRegisteredScriptParameters& parameters)
{
    NSMutableDictionary *result = [NSMutableDictionary dictionaryWithCapacity:9];

    ASSERT(parameters.matchPatterns);
    ASSERT(parameters.persistent);

    result[idKey] = parameters.identifier;
    result[matchesKey] = createNSArray(parameters.matchPatterns.value()).get();
    result[persistAcrossSessionsKey] = parameters.persistent.value() ? @YES : @NO;

    if (parameters.css)
        result[cssKey] = createNSArray(parameters.css.value()).get();

    if (parameters.js)
        result[jsKey] = createNSArray(parameters.js.value()).get();

    if (parameters.excludeMatchPatterns)
        result[excludeMatchesKey] = createNSArray(parameters.excludeMatchPatterns.value()).get();

    if (parameters.allFrames)
        result[allFramesKey] = parameters.allFrames.value() ? @YES : @NO;

    if (parameters.injectionTime)
        result[runAtKey] = toWebAPI(parameters.injectionTime.value());

    if (parameters.styleLevel)
        result[cssOriginKey] = parameters.styleLevel.value() == WebCore::UserStyleLevel::User ? userValue : authorValue;

    if (parameters.world)
        result[worldKey] = parameters.world.value() == WebExtensionContentWorldType::Main ? mainWorld : isolatedWorld;

    return [result copy];
}

NSString *toWebAPI(WebExtension::InjectionTime injectionTime)
{
    switch (injectionTime) {
    case WebExtension::InjectionTime::DocumentEnd:
        return documentEnd;
    case WebExtension::InjectionTime::DocumentIdle:
        return documentIdle;
    case WebExtension::InjectionTime::DocumentStart:
        return documentStart;

    default:
        ASSERT_NOT_REACHED();
        return documentIdle;
    }
}

void WebExtensionAPIScripting::executeScript(NSDictionary *script, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/scripting/executeScript

    WebExtensionScriptInjectionParameters parameters;
    if (!parseScriptInjectionOptions(script, parameters, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingExecuteScript(WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<Vector<WebKit::WebExtensionScriptInjectionResultParameters>, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call(toWebAPI(result.value(), false));
    }, extensionContext().identifier());
}

void WebExtensionAPIScripting::insertCSS(NSDictionary *cssInfo, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/scripting/insertCSS

    WebExtensionScriptInjectionParameters parameters;
    if (!parseCSSInjectionOptions(cssInfo, parameters, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingInsertCSS(WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIScripting::removeCSS(NSDictionary *cssInfo, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/scripting/removeCSS

    WebExtensionScriptInjectionParameters parameters;
    if (!parseCSSInjectionOptions(cssInfo, parameters, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingRemoveCSS(WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIScripting::registerContentScripts(NSArray *scripts, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/scripting/registerContentScripts

    Vector<WebExtensionRegisteredScriptParameters> parameters;
    if (!parseRegisteredContentScripts(scripts, FirstTimeRegistration::Yes, parameters, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingRegisterContentScripts(WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIScripting::getRegisteredContentScripts(NSDictionary *filter, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/scripting/getRegisteredContentScripts

    if (!validateFilter(filter, outExceptionString))
        return;

    auto scriptIDs = makeVector<String>(filter[idsKey]);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingGetRegisteredScripts(WTFMove(scriptIDs)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<Vector<WebExtensionRegisteredScriptParameters>, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPIScripting::updateContentScripts(NSArray *scripts, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/scripting/updateContentScripts

    Vector<WebExtensionRegisteredScriptParameters> parameters;
    if (!parseRegisteredContentScripts(scripts, FirstTimeRegistration::No, parameters, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingUpdateRegisteredScripts(WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIScripting::unregisterContentScripts(NSDictionary *filter, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/scripting/unregisterContentScripts

    if (!validateFilter(filter, outExceptionString))
        return;

    auto scriptIDs = makeVector<String>(filter[idsKey]);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingUnregisterContentScripts(WTFMove(scriptIDs)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call();
    }, extensionContext().identifier());
}

bool WebExtensionAPIScripting::validateFilter(NSDictionary *filter, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *keyTypes = @{
        idsKey: @[ NSString.class ],
    };

    return validateDictionary(filter, @"filter", nil, keyTypes, outExceptionString);
}

bool WebExtensionAPIScripting::parseStyleLevel(NSDictionary *script, NSString *key, std::optional<WebCore::UserStyleLevel>& styleLevel, NSString **outExceptionString)
{
    if (NSString *cssOrigin = objectForKey<NSString>(script, key).lowercaseString) {
        if (![cssOrigin isEqualToString:userValue] && ![cssOrigin isEqualToString:authorValue]) {
            *outExceptionString = toErrorString(nil, key, @"it must specify either 'author' or 'user'");
            return false;
        }

        styleLevel = [cssOrigin isEqualToString:userValue] ? WebCore::UserStyleLevel::User : WebCore::UserStyleLevel::Author;
    } else
        styleLevel = std::nullopt;

    return true;
}

bool WebExtensionAPIScripting::parseExecutionWorld(NSDictionary *script, std::optional<WebExtensionContentWorldType>& worldType, NSString **outExceptionString)
{
    if (NSString *world = objectForKey<NSString>(script, worldKey).lowercaseString) {
        if (![world isEqualToString:isolatedWorld] && ![world isEqualToString:mainWorld]) {
            *outExceptionString = toErrorString(nil, worldKey, @"it must specify either 'isolated' or 'main'");
            return false;
        }

        if ([world isEqualToString:mainWorld])
            worldType = WebExtensionContentWorldType::Main;
        else
            worldType = WebExtensionContentWorldType::ContentScript;
    } else
        worldType = std::nullopt;

    return true;
}

bool WebExtensionAPIScripting::parseTargetInjectionOptions(NSDictionary *targetInfo, WebExtensionScriptInjectionParameters& parameters, NSString **outExceptionString)
{
    static auto *requiredKeys = @[
        tabIDKey,
    ];

    static auto *keyTypes = @{
        allFramesKey: @YES.class,
        documentIDsKey: @[ NSString.class ],
        frameIDsKey: @[ NSNumber.class ],
        tabIDKey: NSNumber.class,
    };

    if (!validateDictionary(targetInfo, targetKey, requiredKeys, keyTypes, outExceptionString))
        return false;

    bool allFrames = boolForKey(targetInfo, allFramesKey, false);
    if (allFrames && targetInfo[frameIDsKey]) {
        *outExceptionString = toErrorString(nil, targetKey, @"it cannot specify both 'allFrames' and 'frameIds'");
        return false;
    }

    if (targetInfo[frameIDsKey] && targetInfo[documentIDsKey]) {
        *outExceptionString = toErrorString(nil, targetKey, @"it cannot specify both 'frameIds' and 'documentIds'");
        return false;
    }

    if (allFrames && targetInfo[documentIDsKey]) {
        *outExceptionString = toErrorString(nil, targetKey, @"it cannot specify both 'allFrames' and 'documentIds'");
        return false;
    }

    NSNumber *tabID = targetInfo[tabIDKey];
    auto tabIdentifier = toWebExtensionTabIdentifier(tabID.doubleValue);
    if (!tabIdentifier) {
        *outExceptionString = toErrorString(nil, tabIDKey, @"'%@' is not a tab identifier", tabID);
        return false;
    }

    parameters.tabIdentifier = tabIdentifier;

    if (NSArray *documentIdentifiers = targetInfo[documentIDsKey]) {
        Vector<WTF::UUID> parsedDocumentIdentifiers;
        for (NSString *documentIdentifier in documentIdentifiers) {
            auto parsedUUID = WTF::UUID::parse(String(documentIdentifier));
            if (!parsedUUID) {
                *outExceptionString = toErrorString(nil, documentIDsKey, @"'%@' is not a document identifier", documentIdentifier);
                return false;
            }

            parsedDocumentIdentifiers.append(WTFMove(parsedUUID.value()));
        }

        parameters.documentIdentifiers = WTFMove(parsedDocumentIdentifiers);
    }

    if (NSArray *frameIDs = targetInfo[frameIDsKey]) {
        Vector<WebExtensionFrameIdentifier> frames;
        for (NSNumber *frameID in frameIDs) {
            auto frameIdentifier = toWebExtensionFrameIdentifier(frameID.doubleValue);
            if (!isValid(frameIdentifier)) {
                *outExceptionString = toErrorString(nil, frameIDsKey, @"'%@' is not a frame identifier", frameID);
                return false;
            }

            frames.append(frameIdentifier.value());
        }

        parameters.frameIdentifiers = WTFMove(frames);
    } else if (!allFrames && !parameters.documentIdentifiers)
        parameters.frameIdentifiers = { WebExtensionFrameConstants::MainFrameIdentifier };

    return true;
}

bool WebExtensionAPIScripting::parseScriptInjectionOptions(NSDictionary *script, WebExtensionScriptInjectionParameters& parameters, NSString **outExceptionString)
{
    static NSArray<NSString *> *requiredKeys = @[
        targetKey,
    ];

    static NSDictionary<NSString *, id> *keyTypes = @{
        argsKey: NSArray.class,
        argumentsKey: NSArray.class,
        filesKey: @[ NSString.class ],
        funcKey: JSValue.class,
        functionKey : JSValue.class,
        targetKey: NSDictionary.class,
        worldKey: NSString.class,
    };

    if (!validateDictionary(script, @"details", requiredKeys, keyTypes, outExceptionString))
        return false;

    if (!parseTargetInjectionOptions(script[targetKey], parameters, outExceptionString))
        return false;

    if (script[functionKey] && script[funcKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it cannot specify both 'func' and 'function'. Please use 'func'");
        return false;
    }

    if (script[argumentsKey] && script[argsKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it cannot specify both 'args' and 'arguments'. Please use 'args'");
        return false;
    }

    auto *usedFunctionKey = script[funcKey] ? funcKey : functionKey;
    bool functionWasPassed = script[usedFunctionKey];
    if (script[filesKey] && functionWasPassed) {
        *outExceptionString = toErrorString(nil, @"details", @"it cannot specify both 'files' and 'func'");
        return false;
    }

    if (!functionWasPassed && !script[filesKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it must specify either 'func' or 'files''");
        return false;
    }

    auto *usedArgumentKey = script[argsKey] ? argsKey : argumentsKey;
    bool scriptContainsArguments = script[usedArgumentKey];
    if (scriptContainsArguments && !functionWasPassed) {
        *outExceptionString = toErrorString(nil, @"details", @"it must specify both 'func' and 'args'");
        return false;
    }

    if (NSArray *files = script[filesKey]) {
        if (!files.count) {
            *outExceptionString = toErrorString(nil, filesKey, @"at least one file must be specified");
            return false;
        }

        parameters.files = makeVector<String>(files);
    }

    std::optional<WebExtensionContentWorldType> worldType;
    if (!parseExecutionWorld(script, worldType, outExceptionString))
        return false;

    if (worldType)
        parameters.world = worldType.value();

    if (JSValue *function = script[usedFunctionKey]) {
        if (!function._isFunction) {
            *outExceptionString = toErrorString(nil, usedFunctionKey, @"it is not a function");
            return false;
        }

        // A JSValue cannot be transferred across processes, so we must convert it to a string before passing it along.
        parameters.function = function.toString;
    }

    if (NSArray *arguments = script[usedArgumentKey]) {
        if (!isValidJSONObject(arguments, JSONOptions::FragmentsAllowed)) {
            *outExceptionString = toErrorString(nil, usedArgumentKey, @"it is not JSON-serializable");
            return false;
        }

        auto *data = encodeJSONData(arguments, JSONOptions::FragmentsAllowed);
        parameters.arguments = API::Data::createWithoutCopying(data);
    }

    return true;
}

bool WebExtensionAPIScripting::parseCSSInjectionOptions(NSDictionary *cssInfo, WebExtensionScriptInjectionParameters& parameters, NSString **outExceptionString)
{
    static NSArray<NSString *> *requiredKeys = @[
        targetKey,
    ];

    static NSDictionary<NSString *, id> *keyTypes = @{
        cssKey: NSString.class,
        filesKey: @[ NSString.class ],
        originKey: NSString.class,
        targetKey: NSDictionary.class,
    };

    if (!validateDictionary(cssInfo, @"details", requiredKeys, keyTypes, outExceptionString))
        return false;

    if (!parseTargetInjectionOptions(cssInfo[targetKey], parameters, outExceptionString))
        return false;

    if (cssInfo[cssKey] && cssInfo[filesKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it cannot specify both 'css' and 'files'");
        return false;
    }

    if (!cssInfo[filesKey] && !cssInfo[cssKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it must specify either 'css' or 'files'");
        return false;
    }

    std::optional<WebCore::UserStyleLevel> styleLevel;
    if (!parseStyleLevel(cssInfo, originKey, styleLevel, outExceptionString))
        return false;

    if (styleLevel)
        parameters.styleLevel = styleLevel.value();

    if (NSString *css = cssInfo[cssKey])
        parameters.css = css;

    if (NSArray *files = cssInfo[filesKey])
        parameters.files = makeVector<String>(files);

    return true;
}

bool WebExtensionAPIScripting::parseRegisteredContentScripts(NSArray *scripts, FirstTimeRegistration firstTimeRegistration, Vector<WebExtensionRegisteredScriptParameters>& parametersVector, NSString **outExceptionString)
{
    static NSArray<NSString *> *requiredKeys = @[
        idKey,
    ];

    static NSDictionary<NSString *, id> *keyTypes = @{
        allFramesKey: @YES.class,
        cssKey: @[ NSString.class ],
        cssOriginKey: NSString.class,
        excludeMatchesKey: @[ NSString.class ],
        idKey: NSString.class,
        jsKey: @[ NSString.class ],
        matchesKey: @[ NSString.class ],
        persistAcrossSessionsKey: @YES.class,
        runAtKey: NSString.class,
        worldKey: NSString.class,
    };

    for (NSDictionary *script in scripts) {
        if (!validateDictionary(script, @"scripts", requiredKeys, keyTypes, outExceptionString))
            return false;

        WebExtensionRegisteredScriptParameters parameters;

        NSString *scriptID = script[idKey];
        if (!scriptID.length) {
            *outExceptionString = toErrorString(nil, idKey, @"it must not be empty");
            return false;
        }

        if ([scriptID characterAtIndex:0] == '_') {
            *outExceptionString = toErrorString(nil, idKey, @"it must not start with '_'");
            return false;
        }

        parameters.identifier = script[idKey];

        NSArray *matchPatterns = script[matchesKey];
        if (firstTimeRegistration == FirstTimeRegistration::Yes && !matchPatterns.count) {
            *outExceptionString = toErrorString(nil, matchesKey, @"it must specify at least one match pattern for script with ID '%@'", script[idKey]);
            return false;
        }

        if (matchPatterns && !matchPatterns.count) {
            *outExceptionString = toErrorString(nil, matchesKey, @"it must not be empty");
            return false;
        }

        NSArray *jsFiles = script[jsKey];
        NSArray *cssFiles = script[cssKey];
        if (firstTimeRegistration == FirstTimeRegistration::Yes && !jsFiles.count && !cssFiles.count) {
            *outExceptionString = toErrorString(nil, @"details", @"it must specify at least one 'css' or 'js' file");
            return false;
        }

        if (NSString *injectionTime = script[runAtKey]) {
            if (![injectionTime isEqualToString:documentIdle] && ![injectionTime isEqualToString:documentStart] && ![injectionTime isEqualToString:documentEnd]) {
                *outExceptionString = toErrorString(nil, runAtKey, @"it must specify either 'document_start', 'document_end', or 'document_idle'");
                return false;
            }

            if ([injectionTime isEqualToString:documentEnd])
                parameters.injectionTime = WebExtension::InjectionTime::DocumentEnd;
            else if ([injectionTime isEqualToString:documentIdle])
                parameters.injectionTime = WebExtension::InjectionTime::DocumentIdle;
            else
                parameters.injectionTime = WebExtension::InjectionTime::DocumentStart;
        }

        std::optional<WebCore::UserStyleLevel> styleLevel;
        if (!parseStyleLevel(script, cssOriginKey, styleLevel, outExceptionString))
            return false;

        if (styleLevel)
            parameters.styleLevel = styleLevel;

        std::optional<WebExtensionContentWorldType> worldType;
        if (!parseExecutionWorld(script, worldType, outExceptionString))
            return false;

        if (worldType)
            parameters.world = worldType;

        if (cssFiles)
            parameters.css = makeVector<String>(cssFiles);

        if (jsFiles)
            parameters.js = makeVector<String>(jsFiles);

        if (matchPatterns)
            parameters.matchPatterns = makeVector<String>(matchPatterns);

        if (NSArray *excludeMatchPatterns = script[excludeMatchesKey])
            parameters.excludeMatchPatterns = makeVector<String>(excludeMatchPatterns);

        if (script[allFramesKey])
            parameters.allFrames = boolForKey(script, allFramesKey, false);

        if (firstTimeRegistration == FirstTimeRegistration::Yes || script[persistAcrossSessionsKey])
            parameters.persistent = boolForKey(script, persistAcrossSessionsKey, true);

        parametersVector.append(parameters);
    }

    return true;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
