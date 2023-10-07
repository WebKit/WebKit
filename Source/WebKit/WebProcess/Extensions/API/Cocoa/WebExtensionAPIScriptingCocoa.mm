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

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtension.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionContentWorldType.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionDynamicScripts.h"
#import "WebExtensionScriptInjectionParameters.h"
#import "WebExtensionScriptInjectionResultParameters.h"
#import "WebExtensionTabIdentifier.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"
#import <wtf/cocoa/VectorCocoa.h>

static NSString * const allFramesKey = @"allFrames";
static NSString * const argsKey = @"args";
static NSString * const argumentsKey = @"arguments";
static NSString * const filesKey = @"files";
static NSString * const frameIDsKey = @"frameIds";
static NSString * const funcKey = @"func";
static NSString * const functionKey = @"function";
static NSString * const tabIDKey = @"tabId";
static NSString * const targetKey = @"target";
static NSString * const worldKey = @"world";

static NSString * const  cssKey = @"css";

static NSString * const mainWorld = @"MAIN";
static NSString * const isolatedWorld = @"ISOLATED";

// FIXME: <https://webkit.org/b/261765> Consider adding support for cssOrigin.
// FIXME: <https://webkit.org/b/261765> Consider adding support for injectImmediately.

namespace WebKit {

static inline void parseTargetInjectionOptions(NSDictionary *targetInfo, WebExtensionScriptInjectionParameters& parameters, NSString **outExceptionString)
{
    NSNumber *tabID = targetInfo[tabIDKey];
    std::optional<WebExtensionTabIdentifier> identifier = toWebExtensionTabIdentifier(tabID.doubleValue);
    if (!identifier) {
        *outExceptionString = toErrorString(nil, tabIDKey, @"'%@' is not a tab identifier", tabID);
        return;
    }

    parameters.tabIdentifier = identifier;

    if (NSArray *frameIDs = targetInfo[frameIDsKey]) {
        Vector<WebExtensionFrameIdentifier> frames;
        for (NSNumber *frameID in frameIDs) {
            auto identifier = toWebExtensionFrameIdentifier(frameID.doubleValue);
            if (!identifier) {
                *outExceptionString = toErrorString(nil, frameIDsKey, @"'%@' is not a frame identifier", frameID);
                return;
            }

            frames.append(identifier.value());
        }

        parameters.frameIDs = frames;
    }

    if (!boolForKey(targetInfo, allFramesKey, false))
        parameters.frameIDs = Vector { WebExtensionFrameConstants::MainFrameIdentifier };
}

static inline void parseScriptInjectionOptions(NSDictionary *script, WebExtensionScriptInjectionParameters& parameters)
{
    if (script[funcKey] || script[functionKey]) {
        NSString *key = script[funcKey] ? funcKey : functionKey;
        parameters.function = script[key];
    }

    if (script[argsKey] || script[argumentsKey]) {
        NSString *key = script[argsKey] ? argsKey : argumentsKey;
        parameters.arguments = makeVector<String>(script[key]);
    }

    if (NSArray *files = script[filesKey])
        parameters.files = makeVector<String>(files);

    parameters.world = WebExtensionContentWorldType::ContentScript;
    if ([(NSString *)script[worldKey] isEqualToString:mainWorld])
        parameters.world = WebExtensionContentWorldType::Main;
}

static inline void parseCSSInjectionOptions(NSDictionary *cssInfo, WebExtensionScriptInjectionParameters& parameters)
{
    if (NSString *css = cssInfo[cssKey])
        parameters.css = css;

    if (NSArray *files = cssInfo[filesKey])
        parameters.files = makeVector<String>(files);

    parameters.world = WebExtensionContentWorldType::ContentScript;
}

void WebExtensionAPIScripting::executeScript(NSDictionary *script, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/scripting/executeScript

    if (!validateScript(script, outExceptionString))
        return;

    // A JSValue cannot be transferred across processes, so we must convert it to a string before passing it along.
    if (JSValue *func = script[funcKey] ?: script[functionKey]) {
        ASSERT(func.isObject);
        NSString *key = script[funcKey] ? funcKey : functionKey;
        script = mergeDictionariesAndSetValues(script, @{ key: func.toString });
    }

    WebExtensionScriptInjectionParameters parameters;
    parseTargetInjectionOptions(script[targetKey], parameters, outExceptionString);
    parseScriptInjectionOptions(script, parameters);

    // FIXME: <https://webkit.org/b/259954> Handle script injections in the UIProcess.
}

void WebExtensionAPIScripting::insertCSS(NSDictionary *cssInfo, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/scripting/insertCSS

    if (!validateCSS(cssInfo, outExceptionString))
        return;

    WebExtensionScriptInjectionParameters parameters;
    parseTargetInjectionOptions(cssInfo[targetKey], parameters, outExceptionString);
    parseCSSInjectionOptions(cssInfo, parameters);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingInsertCSS(WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](WebExtensionDynamicScripts::Error error) {
        if (error)
            callback->reportError(error.value());
        else
            callback->call();
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIScripting::removeCSS(NSDictionary *cssInfo, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/scripting/removeCSS

    if (!validateCSS(cssInfo, outExceptionString))
        return;

    WebExtensionScriptInjectionParameters parameters;
    parseTargetInjectionOptions(cssInfo[targetKey], parameters, outExceptionString);
    parseCSSInjectionOptions(cssInfo, parameters);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ScriptingRemoveCSS(WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](WebExtensionDynamicScripts::Error error) {
        if (error)
            callback->reportError(error.value());
        else
            callback->call();
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIScripting::registerContentScripts(NSObject *scripts, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/scripting/registerContentScripts

    // FIXME: <https://webkit.org/b/261769> Implement this.
}

void WebExtensionAPIScripting::getRegisteredContentScripts(NSDictionary *filter, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/scripting/getRegisteredContentScripts

    // FIXME: <https://webkit.org/b/261769> Implement this.
}
void WebExtensionAPIScripting::updateContentScripts(NSObject *scripts, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/scripting/updateContentScripts

    // FIXME: <https://webkit.org/b/261769> Implement this.
}
void WebExtensionAPIScripting::unregisterContentScripts(NSDictionary *filter, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/scripting/unregisterContentScripts

    // FIXME: <https://webkit.org/b/261769> Implement this.
}

bool WebExtensionAPIScripting::validateScript(NSDictionary *script, NSString **outExceptionString)
{
    static NSArray<NSString *> *requiredKeys = @[
        targetKey,
    ];

    static NSDictionary<NSString *, id> *keyTypes = @{
        argsKey: @[ NSObject.class ],
        argumentsKey: @[ NSObject.class ],
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

    if (NSString *world = script[worldKey]) {
        if (![world isEqualToString:isolatedWorld] && ![world isEqualToString:mainWorld]) {
            *outExceptionString = toErrorString(nil, worldKey, @"it must specify either 'ISOLATED' or 'MAIN'");
            return false;
        }
    }

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
        if (!identifier) {
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

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
