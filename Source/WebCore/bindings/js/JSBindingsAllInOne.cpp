/*
 * Copyright (C) 2009, 2010 Apple Inc. All Rights Reserved.
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

// This all-in-one cpp file cuts down on template bloat to allow us to build our Windows release build.

#include "CachedModuleScriptLoader.cpp"
#include "CallTracer.cpp"
#include "DOMWrapperWorld.cpp"
#include "GCController.cpp"
#include "JSAttrCustom.cpp"
#include "JSAudioTrackCustom.cpp"
#include "JSAudioTrackListCustom.cpp"
#include "JSBasicCredentialCustom.cpp"
#include "JSBlobCustom.cpp"
#include "JSCSSRuleCustom.cpp"
#include "JSCSSRuleListCustom.cpp"
#include "JSCSSStyleDeclarationCustom.cpp"
#include "JSCallbackData.cpp"
#include "JSCanvasRenderingContext2DCustom.cpp"
#include "JSCustomEventCustom.cpp"
#include "JSCustomXPathNSResolver.cpp"
#include "JSDOMBindingSecurity.cpp"
#include "JSDOMBuiltinConstructorBase.cpp"
#include "JSDOMConstructorBase.cpp"
#include "JSDOMConstructorWithDocument.cpp"
#include "JSDOMExceptionHandling.cpp"
#include "JSDOMGlobalObject.cpp"
#include "JSDOMGlobalObjectTask.cpp"
#include "JSDOMPromiseDeferred.cpp"
#include "JSDOMWindowBase.cpp"
#include "JSDOMWindowCustom.cpp"
#include "JSDOMWindowProperties.cpp"
#include "JSDOMWindowProxy.cpp"
#include "JSDOMWrapper.cpp"
#include "JSDOMWrapperCache.cpp"
#include "JSDeprecatedCSSOMValueCustom.cpp"
#include "JSDocumentCustom.cpp"
#include "JSDocumentFragmentCustom.cpp"
#include "JSElementCustom.cpp"
#include "JSErrorHandler.cpp"
#include "JSEventCustom.cpp"
#include "JSEventListener.cpp"
#include "JSEventTargetCustom.cpp"
#include "JSExtendableMessageEventCustom.cpp"
#include "JSFileSystemEntryCustom.cpp"
#include "JSHTMLCollectionCustom.cpp"
#include "JSHTMLDocumentCustom.cpp"
#include "JSHTMLElementCustom.cpp"
#include "JSHTMLTemplateElementCustom.cpp"
#include "JSHistoryCustom.cpp"
#include "JSImageDataCustom.cpp"
#include "JSLazyEventListener.cpp"
#include "JSLocationCustom.cpp"
#include "JSMainThreadExecState.cpp"
#include "JSMediaStreamTrackCustom.cpp"
#include "JSMessageChannelCustom.cpp"
#include "JSMessageEventCustom.cpp"
#include "JSMessagePortCustom.cpp"
#include "JSMutationCallback.cpp"
#include "JSMutationObserverCustom.cpp"
#include "JSNodeCustom.cpp"
#include "JSNodeIteratorCustom.cpp"
#include "JSNodeListCustom.cpp"
#include "JSPluginElementFunctions.cpp"
#include "JSPopStateEventCustom.cpp"
#include "JSReadableStreamPrivateConstructors.cpp"
#include "JSSVGPathSegCustom.cpp"
#include "JSStyleSheetCustom.cpp"
#include "JSTextCustom.cpp"
#include "JSTextTrackCueCustom.cpp"
#include "JSTextTrackCustom.cpp"
#include "JSTextTrackListCustom.cpp"
#include "JSTrackCustom.cpp"
#include "JSTreeWalkerCustom.cpp"
#include "JSVideoTrackCustom.cpp"
#include "JSVideoTrackListCustom.cpp"
#include "JSWorkerGlobalScopeBase.cpp"
#include "JSWorkerGlobalScopeCustom.cpp"
#include "JSXMLDocumentCustom.cpp"
#include "JSXMLHttpRequestCustom.cpp"
#include "JSXPathNSResolverCustom.cpp"
#include "JSXPathResultCustom.cpp"
#include "ScheduledAction.cpp"
#include "ScriptCachedFrameData.cpp"
#include "ScriptController.cpp"
#include "ScriptModuleLoader.cpp"
#include "ScriptState.cpp"
#include "SerializedScriptValue.cpp"
#include "WebCoreTypedArrayController.cpp"
#include "WorkerScriptController.cpp"
