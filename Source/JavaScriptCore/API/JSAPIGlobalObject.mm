/*
 * Copyright (C) 2019 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "JSAPIGlobalObject.h"

#if JSC_OBJC_API_ENABLED

#import "APICast.h"
#import "CallFrameInlines.h"
#import "CatchScope.h"
#import "Completion.h"
#import "Error.h"
#import "Exception.h"
#import "JSContextInternal.h"
#import "JSInternalPromise.h"
#import "JSModuleLoader.h"
#import "JSNativeStdFunction.h"
#import "JSPromise.h"
#import "JSScriptFetchParameters.h"
#import "JSScriptInternal.h"
#import "JSSourceCode.h"
#import "JSValueInternal.h"
#import "JSVirtualMachineInternal.h"
#import "JavaScriptCore.h"
#import "ObjectConstructor.h"
#import "SourceOrigin.h"
#import <wtf/URL.h>

namespace JSC {

const ClassInfo JSAPIGlobalObject::s_info = { "GlobalObject"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSAPIGlobalObject) };

const GlobalObjectMethodTable JSAPIGlobalObject::s_globalObjectMethodTable = {
    &supportsRichSourceInfo,
    &shouldInterruptScript,
    &javaScriptRuntimeFlags,
    nullptr, // queueMicrotaskToEventLoop
    &shouldInterruptScriptBeforeTimeout,
    &moduleLoaderImportModule, // moduleLoaderImportModule
    &moduleLoaderResolve, // moduleLoaderResolve
    &moduleLoaderFetch, // moduleLoaderFetch
    &moduleLoaderCreateImportMetaProperties, // moduleLoaderCreateImportMetaProperties
    &moduleLoaderEvaluate, // moduleLoaderEvaluate
    nullptr, // promiseRejectionTracker
    &reportUncaughtExceptionAtEventLoop,
    &currentScriptExecutionOwner,
    &scriptExecutionStatus,
    &reportViolationForUnsafeEval,
    nullptr, // defaultLanguage
    nullptr, // compileStreaming
    nullptr, // instantiateStreaming
    &deriveShadowRealmGlobalObject,
};

void JSAPIGlobalObject::reportUncaughtExceptionAtEventLoop(JSGlobalObject* globalObject, Exception* exception)
{
    JSContext *context = [JSContext contextWithJSGlobalContextRef:toGlobalRef(globalObject)];
    [context notifyException:toRef(globalObject->vm(), exception->value())];
}

static Expected<URL, String> computeValidImportSpecifier(const URL& base, const String& specifier)
{
    URL absoluteURL(specifier);
    if (absoluteURL.isValid())
        return absoluteURL;

    if (!specifier.startsWith('/') && !specifier.startsWith("./"_s) && !specifier.startsWith("../"_s))
        return makeUnexpected(makeString("Module specifier: "_s, specifier, " does not start with \"/\", \"./\", or \"../\". Referenced from: "_s, base.string()));

    if (specifier.startsWith('/')) {
        absoluteURL = URL(URL("file://"_s), specifier);
        if (absoluteURL.isValid())
            return absoluteURL;
    }

    if (base == URL())
        return makeUnexpected("Could not determine the base URL for loading."_s);

    if (!base.isValid())
        return makeUnexpected(makeString("Referrering script's url is not valid: "_s, base.string()));

    absoluteURL = URL(base, specifier);
    if (absoluteURL.isValid())
        return absoluteURL;
    return makeUnexpected(makeString("Could not form valid URL from identifier and base. Tried:"_s, absoluteURL.string()));
}

Identifier JSAPIGlobalObject::moduleLoaderResolve(JSGlobalObject* globalObject, JSModuleLoader*, JSValue key, JSValue referrer, JSValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT_UNUSED(globalObject, globalObject == globalObject);
    ASSERT(key.isString() || key.isSymbol());
    String name =  key.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    URL base;
    if (JSString* referrerString = jsDynamicCast<JSString*>(referrer)) {
        String value = referrerString->value(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        URL referrerURL({ }, value);
        RELEASE_ASSERT(referrerURL.isValid());
        base = WTFMove(referrerURL);
    }

    auto result = computeValidImportSpecifier(base, name);
    if (result)
        return Identifier::fromString(vm, result.value().string());

    throwVMError(globalObject, scope, createError(globalObject, result.error()));
    return { };
}

JSInternalPromise* JSAPIGlobalObject::moduleLoaderImportModule(JSGlobalObject* globalObject, JSModuleLoader*, JSString* specifierValue, JSValue parameters, const SourceOrigin& sourceOrigin)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto reject = [&] (ThrowScope& scope) -> JSInternalPromise* {
        auto* promise = JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
        return promise->rejectWithCaughtException(globalObject, scope);
    };

    auto import = [&] (URL& url, JSValue parameters) {
        auto result = importModule(globalObject, Identifier::fromString(vm, url.string()), parameters, jsUndefined());
        RETURN_IF_EXCEPTION(scope, reject(scope));
        return result;
    };

    auto specifier = specifierValue->value(globalObject);
    RETURN_IF_EXCEPTION(scope, reject(scope));

    auto result = computeValidImportSpecifier(sourceOrigin.url(), specifier);
    if (result) {
        auto assertions = JSC::retrieveAssertionsFromDynamicImportOptions(globalObject, parameters, { vm.propertyNames->type.impl() });
        RETURN_IF_EXCEPTION(scope, reject(scope));
        parameters = jsUndefined();
        if (!assertions.isEmpty()) {
            if (std::optional<ScriptFetchParameters::Type> type = ScriptFetchParameters::parseType(assertions.get(vm.propertyNames->type.impl())))
                parameters = JSScriptFetchParameters::create(vm, ScriptFetchParameters::create(type.value()));
        }

        return import(result.value(), parameters);
    }
    scope.release();
    auto* promise = JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
    // FIXME: We could have error since any JS call can throw stack-overflow errors.
    // https://bugs.webkit.org/show_bug.cgi?id=203402
    promise->reject(globalObject, createError(globalObject, result.error()));
    return promise;
}

JSInternalPromise* JSAPIGlobalObject::moduleLoaderFetch(JSGlobalObject* globalObject, JSModuleLoader*, JSValue key, JSValue, JSValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(globalObject == globalObject);
    JSContext *context = [JSContext contextWithJSGlobalContextRef:toGlobalRef(globalObject)];

    JSInternalPromise* promise = JSInternalPromise::create(vm, globalObject->internalPromiseStructure());

    Identifier moduleKey = key.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, promise->rejectWithCaughtException(globalObject, scope));

    if (UNLIKELY(![context moduleLoaderDelegate])) {
        scope.release();
        promise->reject(globalObject, createError(globalObject, "No module loader provided."_s));
        return promise;
    }

    auto strongPromise = Strong<JSInternalPromise>(vm, promise);
    auto* resolve = JSNativeStdFunction::create(vm, globalObject, 1, "resolve"_s, [=] (JSGlobalObject* globalObject, CallFrame* callFrame) {
        // This captures the globalObject but that's ok because our structure keeps it alive anyway.
        VM& vm = globalObject->vm();
        JSContext *context = [JSContext contextWithJSGlobalContextRef:toGlobalRef(globalObject)];
        id script = valueToObject(context, toRef(globalObject, callFrame->argument(0)));

        auto rejectPromise = [&] (String message) {
            strongPromise.get()->reject(globalObject, createTypeError(globalObject, message));
            return encodedJSUndefined();
        };

        if (UNLIKELY(![script isKindOfClass:[JSScript class]]))
            return rejectPromise("First argument of resolution callback is not a JSScript"_s);

        JSScript* jsScript = static_cast<JSScript *>(script);

        JSSourceCode* source = [jsScript jsSourceCode];
        if (UNLIKELY([jsScript type] != kJSScriptTypeModule))
            return rejectPromise("The JSScript that was provided did not have expected type of kJSScriptTypeModule."_s);

        NSURL *sourceURL = [jsScript sourceURL];
        String oldModuleKey { [sourceURL absoluteString] };
        if (UNLIKELY(Identifier::fromString(vm, oldModuleKey) != moduleKey))
            return rejectPromise(makeString("The same JSScript was provided for two different identifiers, previously: ", oldModuleKey, " and now: ", moduleKey.string()));

        strongPromise.get()->resolve(globalObject, source);
        return encodedJSUndefined();
    });

    auto* reject = JSNativeStdFunction::create(vm, globalObject, 1, "reject"_s, [=] (JSGlobalObject*, CallFrame* callFrame) {
        strongPromise.get()->reject(globalObject, callFrame->argument(0));
        return encodedJSUndefined();
    });

    [[context moduleLoaderDelegate] context:context fetchModuleForIdentifier:[::JSValue valueWithJSValueRef:toRef(globalObject, key) inContext:context] withResolveHandler:[::JSValue valueWithJSValueRef:toRef(globalObject, resolve) inContext:context] andRejectHandler:[::JSValue valueWithJSValueRef:toRef(globalObject, reject) inContext:context]];
    if (context.exception) {
        scope.release();
        promise->reject(globalObject, toJS(globalObject, [context.exception JSValueRef]));
        context.exception = nil;
    }
    return promise;
}

JSObject* JSAPIGlobalObject::moduleLoaderCreateImportMetaProperties(JSGlobalObject* globalObject, JSModuleLoader*, JSValue key, JSModuleRecord*, JSValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* metaProperties = constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());
    RETURN_IF_EXCEPTION(scope, nullptr);

    metaProperties->putDirect(vm, Identifier::fromString(vm, "filename"_s), key);
    RETURN_IF_EXCEPTION(scope, nullptr);

    return metaProperties;
}

JSValue JSAPIGlobalObject::moduleLoaderEvaluate(JSGlobalObject* globalObject, JSModuleLoader* moduleLoader, JSValue key, JSValue moduleRecordValue, JSValue scriptFetcher, JSValue sentValue, JSValue resumeMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSContext *context = [JSContext contextWithJSGlobalContextRef:toGlobalRef(globalObject)];
    id <JSModuleLoaderDelegate> moduleLoaderDelegate = [context moduleLoaderDelegate];
    NSURL *url = nil;

    if ([moduleLoaderDelegate respondsToSelector:@selector(willEvaluateModule:)] || [moduleLoaderDelegate respondsToSelector:@selector(didEvaluateModule:)]) {
        String moduleKey = key.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        url = [NSURL URLWithString:static_cast<NSString *>(moduleKey)];
    }

    if ([moduleLoaderDelegate respondsToSelector:@selector(willEvaluateModule:)])
        [moduleLoaderDelegate willEvaluateModule:url];

    scope.release();
    // FIXME: We should update the delegate callbacks for async modules. https://bugs.webkit.org/show_bug.cgi?id=222253
    JSValue result = moduleLoader->evaluateNonVirtual(globalObject, key, moduleRecordValue, scriptFetcher, sentValue, resumeMode);

    if ([moduleLoaderDelegate respondsToSelector:@selector(didEvaluateModule:)])
        [moduleLoaderDelegate didEvaluateModule:url];

    return result;
}

JSValue JSAPIGlobalObject::loadAndEvaluateJSScriptModule(const JSLockHolder&, JSScript *script)
{
    ASSERT(script.type == kJSScriptTypeModule);
    VM& vm = this->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier key = Identifier::fromString(vm, String { [[script sourceURL] absoluteString] });
    JSInternalPromise* promise = importModule(this, key, jsUndefined(), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });
    auto* result = JSPromise::create(vm, this->promiseStructure());
    result->resolve(this, promise);
    RETURN_IF_EXCEPTION(scope, { });
    return result;
}

}

#endif // JSC_OBJC_API_ENABLED
