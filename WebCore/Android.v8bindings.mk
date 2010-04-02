##
## Copyright 2009, The Android Open Source Project
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
##  * Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
##  * Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
## EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
## PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
## CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
## EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
## PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
## PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
## OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##

LOCAL_CFLAGS += -DWTF_USE_V8=1

BINDING_C_INCLUDES := \
	external/v8/include \
	\
	$(LOCAL_PATH)/bindings/v8 \
	$(LOCAL_PATH)/bindings/v8/custom \
	$(LOCAL_PATH)/bridge \
	\
	$(base_intermediates)/WebCore/bindings \
	$(base_intermediates)/JavaScriptCore

LOCAL_SRC_FILES += \
	bindings/ScriptControllerBase.cpp \
	\
	bindings/v8/ChildThreadDOMData.cpp \
	bindings/v8/DateExtension.cpp \
	bindings/v8/DOMData.cpp \
	bindings/v8/DOMDataStore.cpp \
	bindings/v8/MainThreadDOMData.cpp \
	bindings/v8/NPV8Object.cpp \
	bindings/v8/RuntimeEnabledFeatures.cpp \
	bindings/v8/ScheduledAction.cpp \
	bindings/v8/ScopedDOMDataStore.cpp \
	bindings/v8/ScriptArray.cpp \
	bindings/v8/ScriptCallFrame.cpp \
	bindings/v8/ScriptCallStack.cpp \
	bindings/v8/ScriptController.cpp \
	bindings/v8/ScriptEventListener.cpp \
	bindings/v8/ScriptFunctionCall.cpp \
	bindings/v8/ScriptInstance.cpp \
	bindings/v8/ScriptObject.cpp \
	bindings/v8/ScriptScope.cpp \
	bindings/v8/ScriptState.cpp \
	bindings/v8/ScriptStringImpl.cpp \
	bindings/v8/ScriptValue.cpp \
	bindings/v8/StaticDOMDataStore.cpp \
	bindings/v8/V8AbstractEventListener.cpp \
	bindings/v8/V8Binding.cpp \
	bindings/v8/V8Collection.cpp \
	bindings/v8/V8ConsoleMessage.cpp \
	bindings/v8/V8DOMMap.cpp \
	bindings/v8/V8DOMWrapper.cpp \
	bindings/v8/V8DataGridDataSource.cpp \
	bindings/v8/V8EventListenerList.cpp \
	bindings/v8/V8GCController.cpp \
	bindings/v8/V8Helpers.cpp \
	bindings/v8/V8HiddenPropertyName.cpp \
	bindings/v8/V8IsolatedWorld.cpp \
	bindings/v8/V8LazyEventListener.cpp \
	bindings/v8/V8NPObject.cpp \
	bindings/v8/V8NPUtils.cpp \
	bindings/v8/V8NodeFilterCondition.cpp \
	bindings/v8/V8Proxy.cpp \
	bindings/v8/V8Utilities.cpp \
	bindings/v8/V8WorkerContextEventListener.cpp \
	bindings/v8/WorkerContextExecutionProxy.cpp \
	bindings/v8/WorkerScriptController.cpp \
	\
	bindings/v8/npruntime.cpp \
	\
	bindings/v8/custom/V8AttrCustom.cpp \
	bindings/v8/custom/V8CSSRuleCustom.cpp \
	bindings/v8/custom/V8CSSStyleDeclarationCustom.cpp \
	bindings/v8/custom/V8CSSStyleSheetCustom.cpp \
	bindings/v8/custom/V8CSSValueCustom.cpp \
	bindings/v8/custom/V8CanvasRenderingContext2DCustom.cpp \
	bindings/v8/custom/V8CanvasPixelArrayCustom.cpp \
	bindings/v8/custom/V8ClientRectListCustom.cpp \
	bindings/v8/custom/V8ClipboardCustom.cpp \
	bindings/v8/custom/V8CoordinatesCustom.cpp \
	bindings/v8/custom/V8CustomEventListener.cpp \
	bindings/v8/custom/V8CustomPositionCallback.cpp \
	bindings/v8/custom/V8CustomPositionErrorCallback.cpp \
	bindings/v8/custom/V8CustomSQLStatementCallback.cpp \
	bindings/v8/custom/V8CustomSQLStatementErrorCallback.cpp \
	bindings/v8/custom/V8CustomSQLTransactionCallback.cpp \
	bindings/v8/custom/V8CustomSQLTransactionErrorCallback.cpp \
	bindings/v8/custom/V8CustomVoidCallback.cpp \
  bindings/v8/custom/V8DOMFormDataCustom.cpp \
	bindings/v8/custom/V8DOMWindowCustom.cpp \
	bindings/v8/custom/V8DataGridColumnListCustom.cpp \
	bindings/v8/custom/V8DatabaseCallback.cpp \
	bindings/v8/custom/V8DatabaseCustom.cpp \
	bindings/v8/custom/V8DedicatedWorkerContextCustom.cpp \
	bindings/v8/custom/V8DocumentCustom.cpp \
	bindings/v8/custom/V8DocumentLocationCustom.cpp \
	bindings/v8/custom/V8ElementCustom.cpp \
	bindings/v8/custom/V8EventCustom.cpp \
	bindings/v8/custom/V8EventSourceConstructor.cpp \
	bindings/v8/custom/V8FileListCustom.cpp \
	bindings/v8/custom/V8GeolocationCustom.cpp \
	bindings/v8/custom/V8HTMLAllCollectionCustom.cpp \
	bindings/v8/custom/V8HTMLAudioElementConstructor.cpp \
	bindings/v8/custom/V8HTMLCanvasElementCustom.cpp \
	bindings/v8/custom/V8HTMLCollectionCustom.cpp \
	bindings/v8/custom/V8HTMLDataGridElementCustom.cpp \
	bindings/v8/custom/V8HTMLDocumentCustom.cpp \
	bindings/v8/custom/V8HTMLElementCustom.cpp \
	bindings/v8/custom/V8HTMLFormElementCustom.cpp \
	bindings/v8/custom/V8HTMLFrameElementCustom.cpp \
	bindings/v8/custom/V8HTMLFrameSetElementCustom.cpp \
	bindings/v8/custom/V8HTMLIFrameElementCustom.cpp \
	bindings/v8/custom/V8HTMLImageElementConstructor.cpp \
	bindings/v8/custom/V8HTMLInputElementCustom.cpp \
	bindings/v8/custom/V8HTMLOptionElementConstructor.cpp \
	bindings/v8/custom/V8HTMLOptionsCollectionCustom.cpp \
	bindings/v8/custom/V8HTMLPlugInElementCustom.cpp \
	bindings/v8/custom/V8HTMLSelectElementCollectionCustom.cpp \
	bindings/v8/custom/V8HTMLSelectElementCustom.cpp \
	bindings/v8/custom/V8LocationCustom.cpp \
	bindings/v8/custom/V8MessageChannelConstructor.cpp \
	bindings/v8/custom/V8MessagePortCustom.cpp \
	bindings/v8/custom/V8MessageEventCustom.cpp \
	bindings/v8/custom/V8NamedNodeMapCustom.cpp \
	bindings/v8/custom/V8NamedNodesCollection.cpp \
	bindings/v8/custom/V8NodeCustom.cpp \
	bindings/v8/custom/V8NodeFilterCustom.cpp \
	bindings/v8/custom/V8NodeIteratorCustom.cpp \
	bindings/v8/custom/V8NodeListCustom.cpp \
	bindings/v8/custom/V8SQLResultSetRowListCustom.cpp \
	bindings/v8/custom/V8SQLTransactionCustom.cpp \
	bindings/v8/custom/V8WebSocketCustom.cpp

ifeq ($(ENABLE_SVG), true)
LOCAL_SRC_FILES += \
	bindings/v8/custom/V8SVGDocumentCustom.cpp \
	bindings/v8/custom/V8SVGElementCustom.cpp \
	bindings/v8/custom/V8SVGLengthCustom.cpp \
	bindings/v8/custom/V8SVGMatrixCustom.cpp
	bindings/v8/custom/V8SVGPathSegCustom.cpp \
endif

LOCAL_SRC_FILES += \
	bindings/v8/custom/V8SharedWorkerCustom.cpp \
	bindings/v8/custom/V8StorageCustom.cpp \
	bindings/v8/custom/V8StyleSheetCustom.cpp \
	bindings/v8/custom/V8StyleSheetListCustom.cpp \
	bindings/v8/custom/V8TreeWalkerCustom.cpp \
	bindings/v8/custom/V8WebKitCSSMatrixConstructor.cpp \
	bindings/v8/custom/V8WebKitPointConstructor.cpp \
	bindings/v8/custom/V8WorkerContextCustom.cpp \
	bindings/v8/custom/V8WorkerCustom.cpp \
	bindings/v8/custom/V8XMLHttpRequestConstructor.cpp \
	bindings/v8/custom/V8XMLHttpRequestCustom.cpp

LOCAL_SRC_FILES += \
	bridge/jni/JNIBridge.cpp \
	bridge/jni/JNIUtility.cpp \
	bridge/jni/v8/JNIBridgeV8.cpp \
	bridge/jni/v8/JNIUtilityPrivate.cpp \
	bridge/jni/v8/JavaClassV8.cpp \
	bridge/jni/v8/JavaInstanceV8.cpp \
	bridge/jni/v8/JavaNPObject.cpp
