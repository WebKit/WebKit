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
static NSString * const filesKey = @"files";
static NSString * const frameIDsKey = @"frameIds";
static NSString * const funcKey = @"func";
static NSString * const functionKey = @"function";
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

static NSString * const mainWorld = @"MAIN";
static NSString * const isolatedWorld = @"ISOLATED";

static NSString * const documentEnd = @"document_end";
static NSString * const documentIdle = @"document_idle";
static NSString * const documentStart = @"document_start";

// FIXME: <https://webkit.org/b/261765> Consider adding support for cssOrigin.
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
            if (parameters.result)
                [results addObject:parameters.result.value()];
            else
                [results addObject:NSNull.null];
        }

        return [results copy];
    }

    for (auto& parameters : parametersVector) {
        auto *result = [NSMutableDictionary dictionaryWithCapacity:3];

        if (parameters.result)
            result[@"result"] = parameters.result.value();
        else
            result[@"result"] = NSNull.null;

        ASSERT(parameters.frameID);
        if (parameters.frameID)
            result[@"frameId"] = @(WebKit::toWebAPI(parameters.frameID.value()));

        if (parameters.error)
            result[@"error"] = parameters.error.value();

        [results addObject:result];
    }

    return [results copy];
}

NSArray *toWebAPI(const Vector<WebExtensionRegisteredScriptParameters>& parametersVector)
{
    NSMutableArray *results = [NSMutableArray arrayWithCapacity:parametersVector.size()];

    for (auto& parameters : parametersVector) {
        NSMutableDictionary *result = [NSMutableDictionary dictionary];

        ASSERT(parameters.allFrames);
        ASSERT(parameters.matchPatterns);
        ASSERT(parameters.persistent);
        ASSERT(parameters.injectionTime);
        ASSERT(parameters.world);

        result[allFramesKey] = parameters.allFrames.value() ? @YES : @NO;
        result[idKey] = parameters.identifier;
        result[matchesKey] = createNSArray(parameters.matchPatterns.value()).get();
        result[persistAcrossSessionsKey] = parameters.persistent.value() ? @YES : @NO;
        result[runAtKey] = toWebAPI(parameters.injectionTime.value());
        result[worldKey] = parameters.world.value() == WebExtensionContentWorldType::Main ? mainWorld : isolatedWorld;

        if (parameters.css)
            result[cssKey] = createNSArray(parameters.css.value()).get();

        if (parameters.js)
            result[jsKey] = createNSArray(parameters.js.value()).get();

        if (parameters.excludeMatchPatterns)
            result[excludeMatchesKey] = createNSArray(parameters.excludeMatchPatterns.value()).get();

        [results addObject:result];
    }

    return [results copy];
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

    if (!validateScript(script, outExceptionString))
        return;

    // A JSValue cannot be transferred across processes, so we must convert it to a string before passing it along.
    if (JSValue *function = script[funcKey] ?: script[functionKey]) {
        ASSERT(function.isObject);
        script = mergeDictionariesAndSetValues(script, @{ functionKey: function.toString });
    }

    WebExtensionScriptInjectionParameters parameters;
    parseTargetInjectionOptions(script[targetKey], parameters, outExceptionString);
    parseScriptInjectionOptions(script, parameters, outExceptionString);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingExecuteScript(WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<Vector<WebKit::WebExtensionScriptInjectionResultParameters>> results, Error error) {
        if (error)
            callback->reportError(error.value());
        else
            callback->call(toWebAPI(results.value(), false));
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIScripting::insertCSS(NSDictionary *cssInfo, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/scripting/insertCSS

    if (!validateCSS(cssInfo, outExceptionString))
        return;

    WebExtensionScriptInjectionParameters parameters;
    parseTargetInjectionOptions(cssInfo[targetKey], parameters, outExceptionString);
    parseCSSInjectionOptions(cssInfo, parameters);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingInsertCSS(WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Error error) {
        if (error)
            callback->reportError(error.value());
        else
            callback->call();
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIScripting::removeCSS(NSDictionary *cssInfo, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/scripting/removeCSS

    if (!validateCSS(cssInfo, outExceptionString))
        return;

    WebExtensionScriptInjectionParameters parameters;
    parseTargetInjectionOptions(cssInfo[targetKey], parameters, outExceptionString);
    parseCSSInjectionOptions(cssInfo, parameters);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingRemoveCSS(WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Error error) {
        if (error)
            callback->reportError(error.value());
        else
            callback->call();
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIScripting::registerContentScripts(NSObject *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/scripting/registerContentScripts

    NSArray<NSDictionary<NSString *, id> *> *scripts = dynamic_objc_cast<NSArray>(details);

    if (!validateRegisteredScripts(scripts, FirstTimeRegistration::Yes, outExceptionString))
        return;

    Vector<WebExtensionRegisteredScriptParameters> parameters;
    parseRegisteredContentScripts(scripts, FirstTimeRegistration::Yes, parameters);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingRegisterContentScripts(WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Error error) {
        if (error)
            callback->reportError(error.value());
        else
            callback->call();
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIScripting::getRegisteredContentScripts(NSDictionary *filter, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/scripting/getRegisteredContentScripts

    if (!validateFilter(filter, outExceptionString))
        return;

    Vector<String> scriptIDs = makeVector<String>(filter[idsKey]);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingGetRegisteredScripts(WTFMove(scriptIDs)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](const Vector<WebExtensionRegisteredScriptParameters>& scripts) {
        callback->call(toWebAPI(scripts));
    }, extensionContext().identifier().toUInt64());
}
void WebExtensionAPIScripting::updateContentScripts(NSObject *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/scripting/updateContentScripts

    NSArray<NSDictionary<NSString *, id> *> *scripts = dynamic_objc_cast<NSArray>(details);

    if (!validateRegisteredScripts(scripts, FirstTimeRegistration::No, outExceptionString))
        return;

    Vector<WebExtensionRegisteredScriptParameters> parameters;
    parseRegisteredContentScripts(scripts, FirstTimeRegistration::No, parameters);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingUpdateRegisteredScripts(WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Error error) {
        if (error)
            callback->reportError(error.value());
        else
            callback->call();
    }, extensionContext().identifier().toUInt64());
}
void WebExtensionAPIScripting::unregisterContentScripts(NSDictionary *filter, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/scripting/unregisterContentScripts

    if (!validateFilter(filter, outExceptionString))
        return;

    Vector<String> scriptIDs = makeVector<String>(filter[idsKey]);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingUnregisterContentScripts(WTFMove(scriptIDs)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Error error) {
        if (error)
            callback->reportError(error.value());
        else
            callback->call();
    }, extensionContext().identifier().toUInt64());
}

bool WebExtensionAPIScripting::validateScript(NSDictionary *script, NSString **outExceptionString)
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

    if (!validateTarget(script[targetKey], outExceptionString))
        return false;

    if (NSArray *arguments = script[argsKey] ?: script[argumentsKey]) {
        auto *key = script[argsKey] ? argsKey : argumentsKey;
        if (!isValidJSONObject(arguments, { JSONOptions::FragmentsAllowed })) {
            *outExceptionString = toErrorString(nil, key, @"it is not JSON-serializable");
            return false;
        }
    }

    if (script[functionKey] && script[funcKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it cannot specify both 'func' and 'function'. Please use 'func'");
        return false;
    }

    if (script[argumentsKey] && script[argsKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it cannot specify both 'args' and 'arguments'. Please use 'args'");
        return false;
    }

    bool functionWasPassed = script[functionKey] || script[funcKey];
    if (script[filesKey] && functionWasPassed) {
        *outExceptionString = toErrorString(nil, @"details", @"it cannot specify both 'files' and 'func'");
        return false;
    }

    if (!functionWasPassed && !script[filesKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it must specify either 'func' or 'files''");
        return false;
    }

    bool scriptContainsArguments = !!(script[argsKey] || script[argumentsKey]);
    if (scriptContainsArguments && !functionWasPassed) {
        *outExceptionString = toErrorString(nil, @"details", @"it must specify both 'func' and 'args'");
        return false;
    }

    if (NSArray *files = script[filesKey]) {
        if (!files.count) {
            *outExceptionString = toErrorString(nil, filesKey, @"at least one file must be specified");
            return false;
        }
    }

    if (!hasValidExecutionWorld(script, outExceptionString))
        return false;

    return true;
}

bool WebExtensionAPIScripting::validateTarget(NSDictionary *targetInfo, NSString **outExceptionString)
{
    static NSArray<NSString *> *requiredKeys = @[
        tabIDKey,
    ];

    static NSDictionary<NSString *, id> *keyTypes = @{
        frameIDsKey: @[ NSNumber.class ],
        tabIDKey: NSNumber.class,
        allFramesKey: @YES.class,
    };

    if (!validateDictionary(targetInfo, targetKey, requiredKeys, keyTypes, outExceptionString))
        return false;

    if (targetInfo[allFramesKey] && targetInfo[frameIDsKey]) {
        *outExceptionString = toErrorString(nil, targetKey, @"it cannot specify both 'allFrames' and 'frameIds'");
        return false;
    }

    for (NSNumber *frameID in targetInfo[frameIDsKey]) {
        auto identifier = toWebExtensionFrameIdentifier(frameID.doubleValue);
        if (!isValid(identifier)) {
            *outExceptionString = toErrorString(nil, frameIDsKey, @"'%@' is not a frame identifier", frameID);
            return false;
        }
    }

    return true;
}

bool WebExtensionAPIScripting::validateCSS(NSDictionary *cssInfo, NSString **outExceptionString)
{
    static NSArray<NSString *> *requiredKeys = @[
        targetKey,
    ];

    static NSDictionary<NSString *, id> *keyTypes = @{
        cssKey: NSString.class,
        filesKey: @[ NSString.class ],
        targetKey: NSDictionary.class,
    };

    if (!validateDictionary(cssInfo, @"details", requiredKeys, keyTypes, outExceptionString))
        return false;

    if (!validateTarget(cssInfo[targetKey], outExceptionString))
        return false;

    if (cssInfo[cssKey] && cssInfo[filesKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it cannot specify both 'css' and 'files'");
        return false;
    }

    if (!cssInfo[filesKey] && !cssInfo[cssKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it must specify either 'css' or 'files'");
        return false;
    }

    return true;
}

bool WebExtensionAPIScripting::validateRegisteredScripts(NSArray *scripts, FirstTimeRegistration firstTimeRegistration, NSString **outExceptionString)
{
    static NSArray<NSString *> *requiredKeys = @[
        idKey,
    ];

    static NSDictionary<NSString *, id> *keyTypes = @{
        allFramesKey: @YES.class,
        cssKey: @[ NSString.class ],
        excludeMatchesKey: @[ NSString.class ],
        idKey: NSString.class,
        jsKey: @[ NSString.class ],
        matchesKey: @[ NSString.class ],
        persistAcrossSessionsKey: @YES.class,
        runAtKey: NSString.class,
        worldKey: NSString.class,
    };

    if (![scripts isKindOfClass:NSArray.class]) {
        *outExceptionString = toErrorString(nil, @"details", @"an array is expected");
        return false;
    }

    for (NSDictionary *script in scripts) {
        if (!validateDictionary(script, @"scripts", requiredKeys, keyTypes, outExceptionString))
            return false;

        NSString *scriptID = script[idKey];
        if (!scriptID.length) {
            *outExceptionString = toErrorString(nil, idKey, @"it must not be empty");
            return false;
        }

        if ([scriptID characterAtIndex:0] == '_') {
            *outExceptionString = toErrorString(nil, idKey, @"it must not start with '_'");
            return false;
        }

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
                *outExceptionString = toErrorString(nil, runAtKey, @"it must be one of the following: 'document_end', 'document_idle', or 'document_start'");
                return false;
            }
        }

        if (!hasValidExecutionWorld(script, outExceptionString))
            return false;
    }

    return true;
}

bool WebExtensionAPIScripting::validateFilter(NSDictionary *filter, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *keyTypes = @{
        idsKey: @[ NSString.class ],
    };

    return validateDictionary(filter, @"filter", nil, keyTypes, outExceptionString);
}

void WebExtensionAPIScripting::parseTargetInjectionOptions(NSDictionary *targetInfo, WebExtensionScriptInjectionParameters& parameters, NSString **outExceptionString)
{
    NSNumber *tabID = targetInfo[tabIDKey];
    auto tabIdentifier = toWebExtensionTabIdentifier(tabID.doubleValue);
    if (!tabIdentifier) {
        *outExceptionString = toErrorString(nil, tabIDKey, @"'%@' is not a tab identifier", tabID);
        return;
    }

    parameters.tabIdentifier = tabIdentifier;

    if (NSArray *frameIDs = targetInfo[frameIDsKey]) {
        Vector<WebExtensionFrameIdentifier> frames;
        for (NSNumber *frameID in frameIDs) {
            auto frameIdentifier = toWebExtensionFrameIdentifier(frameID.doubleValue);
            if (!isValid(frameIdentifier)) {
                *outExceptionString = toErrorString(nil, frameIDsKey, @"'%@' is not a frame identifier", frameID);
                return;
            }

            frames.append(frameIdentifier.value());
        }

        parameters.frameIDs = frames;
        return;
    }

    if (!boolForKey(targetInfo, allFramesKey, false))
        parameters.frameIDs = Vector { WebExtensionFrameConstants::MainFrameIdentifier };
}

void WebExtensionAPIScripting::parseScriptInjectionOptions(NSDictionary *script, WebExtensionScriptInjectionParameters& parameters, NSString **outExceptionString)
{
    if (script[functionKey])
        parameters.function = script[functionKey];

    if (NSArray *arguments = script[argsKey] ?: script[argumentsKey]) {
        auto *key = script[argsKey] ? argsKey : argumentsKey;
        auto *data = encodeJSONData(arguments, { JSONOptions::FragmentsAllowed });
        if (!data) {
            *outExceptionString = toErrorString(nil, key, @"it is not JSON-serializable");
            return;
        }

        parameters.arguments = API::Data::createWithoutCopying(data);
    }

    if (NSArray *files = script[filesKey])
        parameters.files = makeVector<String>(files);

    if ([(NSString *)script[worldKey] isEqualToString:mainWorld])
        parameters.world = WebExtensionContentWorldType::Main;
}

void WebExtensionAPIScripting::parseCSSInjectionOptions(NSDictionary *cssInfo, WebExtensionScriptInjectionParameters& parameters)
{
    if (NSString *css = cssInfo[cssKey])
        parameters.css = css;

    if (NSArray *files = cssInfo[filesKey])
        parameters.files = makeVector<String>(files);
}

void WebExtensionAPIScripting::parseRegisteredContentScripts(NSArray *scripts, FirstTimeRegistration firstTimeRegistration, Vector<WebExtensionRegisteredScriptParameters>& parametersVector)
{
    for (NSDictionary *script in scripts) {
        WebExtensionRegisteredScriptParameters parameters;

        parameters.identifier = script[idKey];

        if (NSArray *cssFiles = script[cssKey])
            parameters.css = makeVector<String>(cssFiles);

        if (NSArray *jsFiles = script[jsKey])
            parameters.js = makeVector<String>(jsFiles);

        if (NSArray *matchPatterns = script[matchesKey])
            parameters.matchPatterns = makeVector<String>(matchPatterns);

        if (NSArray *excludeMatchPatterns = script[excludeMatchesKey])
            parameters.excludeMatchPatterns = makeVector<String>(excludeMatchPatterns);

        if (firstTimeRegistration == FirstTimeRegistration::Yes || script[allFramesKey])
            parameters.allFrames = boolForKey(script, allFramesKey, false);

        if (firstTimeRegistration == FirstTimeRegistration::Yes || script[persistAcrossSessionsKey])
            parameters.persistent = boolForKey(script, persistAcrossSessionsKey, true);

        if (NSString *world = script[worldKey])
            parameters.world = [world isEqualToString:mainWorld] ? WebExtensionContentWorldType::Main : WebExtensionContentWorldType::ContentScript;
        else if (firstTimeRegistration == FirstTimeRegistration::Yes)
            parameters.world = WebExtensionContentWorldType::ContentScript;

        if (NSString *injectionTime = script[runAtKey]) {
            if ([injectionTime isEqualToString:documentEnd])
                parameters.injectionTime = WebExtension::InjectionTime::DocumentEnd;
            else if ([injectionTime isEqualToString:documentIdle])
                parameters.injectionTime = WebExtension::InjectionTime::DocumentIdle;
            else
                parameters.injectionTime = WebExtension::InjectionTime::DocumentStart;
        } else if (firstTimeRegistration == FirstTimeRegistration::Yes)
            parameters.injectionTime = WebExtension::InjectionTime::DocumentIdle;

        parametersVector.append(parameters);
    }
}

bool WebExtensionAPIScripting::hasValidExecutionWorld(NSDictionary *script, NSString **outExceptionString)
{
    if (NSString *world = script[worldKey]) {
        if (![world isEqualToString:isolatedWorld] && ![world isEqualToString:mainWorld]) {
            *outExceptionString = toErrorString(nil, worldKey, @"it must specify either 'ISOLATED' or 'MAIN'");
            return false;
        }
    }

    return true;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
