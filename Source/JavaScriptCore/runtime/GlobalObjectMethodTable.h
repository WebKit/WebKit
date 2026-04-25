/*
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *  Copyright (C) 2007-2023 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <JavaScriptCore/Exception.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

#if ENABLE(WEBASSEMBLY)
#include <JavaScriptCore/WebAssemblyCompileOptions.h>
#endif

namespace JSC {

class Identifier;
class JSGlobalObject;
class JSModuleLoader;
class JSModuleRecord;
class JSObject;
class JSPromise;
class JSString;
class JSValue;
class Microtask;
class RuntimeFlags;
class ScriptFetcher;
class ScriptFetchParameters;
class SourceOrigin;
class Structure;
class QueuedTask;

enum class CompilationType;
enum class ScriptExecutionStatus;

enum class JSPromiseRejectionOperation : unsigned {
    Reject, // When a promise is rejected without any handlers.
    Handle, // When a handler is added to a rejected promise for the first time.
};

struct GlobalObjectMethodTable {
    bool (*supportsRichSourceInfo)(const JSGlobalObject*);
    bool (*shouldInterruptScript)(const JSGlobalObject*);
    RuntimeFlags (*javaScriptRuntimeFlags)(const JSGlobalObject*);
    bool (*shouldInterruptScriptBeforeTimeout)(const JSGlobalObject*);

    JSPromise* (*moduleLoaderImportModule)(JSGlobalObject*, JSModuleLoader*, JSString*, RefPtr<ScriptFetchParameters>, const SourceOrigin&);
    Identifier (*moduleLoaderResolve)(JSGlobalObject*, JSModuleLoader*, JSValue, JSValue, RefPtr<ScriptFetcher>, bool useImportMap);
    JSPromise* (*moduleLoaderFetch)(JSGlobalObject*, JSModuleLoader*, JSValue, RefPtr<ScriptFetchParameters>, RefPtr<ScriptFetcher>);
    JSObject* (*moduleLoaderCreateImportMetaProperties)(JSGlobalObject*, JSModuleLoader*, JSValue, JSModuleRecord*, RefPtr<ScriptFetcher>);
    JSValue (*moduleLoaderEvaluate)(JSGlobalObject*, JSModuleLoader*, JSValue key, JSValue moduleRecordValue, RefPtr<ScriptFetcher>, JSValue awaitedValue, JSValue resumeMode);

    void (*promiseRejectionTracker)(JSGlobalObject*, JSPromise*, JSPromiseRejectionOperation);
    void (*reportUncaughtExceptionAtEventLoop)(JSGlobalObject*, Exception*);

    // For most contexts this is just the global object. For JSDOMWindow, however, this is the JSDocument.
    JSObject* (*currentScriptExecutionOwner)(JSGlobalObject*);

    ScriptExecutionStatus (*scriptExecutionStatus)(JSGlobalObject*, JSObject* scriptExecutionOwner);
    void (*reportViolationForUnsafeEval)(JSGlobalObject*, const String&);
    String (*defaultLanguage)();
#if ENABLE(WEBASSEMBLY)
    JSPromise* (*compileStreaming)(JSGlobalObject*, JSValue, std::optional<WebAssemblyCompileOptions>&&);
    JSPromise* (*instantiateStreaming)(JSGlobalObject*, JSValue, JSObject* importObject, std::optional<WebAssemblyCompileOptions>&&);
#else
    void* compileStreamingPlaceholder; // placeholders to make positional initializers consistent
    void* instantiateStreamingPlaceholder;
#endif
    JSGlobalObject* (*deriveShadowRealmGlobalObject)(JSGlobalObject*);
    String (*codeForEval)(JSGlobalObject*, JSValue);
    bool (*canCompileStrings)(JSGlobalObject*, CompilationType, String, const ArgList&);
    Structure* (*trustedScriptStructure)(JSGlobalObject*);
};

} // namespace JSC
