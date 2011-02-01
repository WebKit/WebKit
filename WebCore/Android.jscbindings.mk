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

BINDING_C_INCLUDES := \
	$(LOCAL_PATH)/bindings/js \
	$(LOCAL_PATH)/bridge \
	$(LOCAL_PATH)/bridge/c \
	$(LOCAL_PATH)/bridge/jni \
	$(LOCAL_PATH)/bridge/jni/jsc \
	$(LOCAL_PATH)/bridge/jsc \
	\
	$(JAVASCRIPTCORE_PATH)/API \
	$(JAVASCRIPTCORE_PATH)/assembler \
	$(JAVASCRIPTCORE_PATH)/bytecode \
	$(JAVASCRIPTCORE_PATH)/bytecompiler \
	$(JAVASCRIPTCORE_PATH)/debugger \
	$(JAVASCRIPTCORE_PATH)/parser \
	$(JAVASCRIPTCORE_PATH)/jit \
	$(JAVASCRIPTCORE_PATH)/interpreter \
	$(JAVASCRIPTCORE_PATH)/pcre \
	$(JAVASCRIPTCORE_PATH)/profiler \
	$(JAVASCRIPTCORE_PATH)/runtime \
	$(JAVASCRIPTCORE_PATH)/wrec \
	$(JAVASCRIPTCORE_PATH)/ForwardingHeaders \
	\
	$(base_intermediates)/WebCore/bindings/js \
	$(base_intermediates)/WebCore/dom \
	$(base_intermediates)/WebCore/html/canvas \
	$(base_intermediates)/WebCore/inspector \
	$(base_intermediates)/WebCore/loader/appcache \
	$(base_intermediates)/WebCore/page \
	$(base_intermediates)/WebCore/plugins \
	$(base_intermediates)/WebCore/storage \
	$(base_intermediates)/WebCore/workers \
	$(base_intermediates)/WebCore/xml

LOCAL_SRC_FILES += \
	bindings/js/GCController.cpp \
	bindings/js/JSAttrCustom.cpp \
	bindings/js/JSAudioConstructor.cpp \
	bindings/js/JSCDATASectionCustom.cpp \
	bindings/js/JSCSSRuleCustom.cpp \
	bindings/js/JSCSSRuleListCustom.cpp \
	bindings/js/JSCSSStyleDeclarationCustom.cpp \
	bindings/js/JSCSSValueCustom.cpp \
	bindings/js/JSCallbackData.cpp \
	bindings/js/JSCanvasNumberArrayCustom.cpp \
	bindings/js/JSCanvasRenderingContext2DCustom.cpp \
	bindings/js/JSCanvasRenderingContextCustom.cpp \
	bindings/js/JSClipboardCustom.cpp \
	bindings/js/JSConsoleCustom.cpp \
	bindings/js/JSCoordinatesCustom.cpp \
	bindings/js/JSCustomPositionCallback.cpp \
	bindings/js/JSCustomPositionErrorCallback.cpp \
	bindings/js/JSCustomSQLStatementCallback.cpp \
	bindings/js/JSCustomSQLStatementErrorCallback.cpp \
	bindings/js/JSCustomSQLTransactionCallback.cpp \
	bindings/js/JSCustomSQLTransactionErrorCallback.cpp \
	bindings/js/JSCustomVoidCallback.cpp \
	bindings/js/JSDatabaseCallback.cpp \
	bindings/js/JSDesktopNotificationsCustom.cpp \
	bindings/js/JSDOMApplicationCacheCustom.cpp \
	bindings/js/JSDOMBinding.cpp \
  bindings/js/JSDOMFormDataCustom.cpp \
	bindings/js/JSDOMGlobalObject.cpp \
	bindings/js/JSDOMWindowBase.cpp \
	bindings/js/JSDOMWindowCustom.cpp \
	bindings/js/JSDOMWindowShell.cpp \
	bindings/js/JSDataGridColumnListCustom.cpp \
	bindings/js/JSDataGridDataSource.cpp \
	bindings/js/JSDatabaseCustom.cpp \
	bindings/js/JSDedicatedWorkerContextCustom.cpp \
	bindings/js/JSDocumentCustom.cpp \
	bindings/js/JSElementCustom.cpp \
	bindings/js/JSEventCustom.cpp \
	bindings/js/JSEventListener.cpp \
	bindings/js/JSEventTarget.cpp \
	bindings/js/JSExceptionBase.cpp \
	bindings/js/JSGeolocationCustom.cpp \
	bindings/js/JSHTMLAllCollectionCustom.cpp \
	bindings/js/JSHTMLAppletElementCustom.cpp \
	bindings/js/JSHTMLCanvasElementCustom.cpp \
	bindings/js/JSHTMLCollectionCustom.cpp \
	bindings/js/JSHTMLDataGridElementCustom.cpp \
	bindings/js/JSHTMLDocumentCustom.cpp \
	bindings/js/JSHTMLElementCustom.cpp \
	bindings/js/JSHTMLEmbedElementCustom.cpp \
	bindings/js/JSHTMLFormElementCustom.cpp \
	bindings/js/JSHTMLFrameElementCustom.cpp \
	bindings/js/JSHTMLFrameSetElementCustom.cpp \
	bindings/js/JSHTMLIFrameElementCustom.cpp \
	bindings/js/JSHTMLInputElementCustom.cpp \
	bindings/js/JSHTMLLinkElementCustom.cpp \
	bindings/js/JSHTMLObjectElementCustom.cpp \
	bindings/js/JSHTMLOptionsCollectionCustom.cpp \
	bindings/js/JSHTMLSelectElementCustom.cpp \
	bindings/js/JSHTMLStyleElementCustom.cpp \
	bindings/js/JSHistoryCustom.cpp \
	bindings/js/JSImageConstructor.cpp \
	bindings/js/JSImageDataCustom.cpp \
	bindings/js/JSLazyEventListener.cpp \
	bindings/js/JSLocationCustom.cpp \
	bindings/js/JSMessageChannelConstructor.cpp \
	bindings/js/JSMessageChannelCustom.cpp \
	bindings/js/JSMessageEventCustom.cpp \
	bindings/js/JSMessagePortCustom.cpp \
	bindings/js/JSMimeTypeArrayCustom.cpp \
	bindings/js/JSNamedNodeMapCustom.cpp \
	bindings/js/JSNavigatorCustom.cpp \
	bindings/js/JSNodeCustom.cpp \
	bindings/js/JSNodeFilterCondition.cpp \
	bindings/js/JSNodeFilterCustom.cpp \
	bindings/js/JSNodeIteratorCustom.cpp \
	bindings/js/JSNodeListCustom.cpp \
	bindings/js/JSOptionConstructor.cpp \
	bindings/js/JSPluginArrayCustom.cpp \
	bindings/js/JSPluginCustom.cpp \
	bindings/js/JSPluginElementFunctions.cpp \
	bindings/js/JSProcessingInstructionCustom.cpp \
	bindings/js/JSSQLResultSetRowListCustom.cpp \
	bindings/js/JSSQLTransactionCustom.cpp \
	bindings/js/JSSVGElementInstanceCustom.cpp \
	bindings/js/JSSVGLengthCustom.cpp \
	bindings/js/JSSVGMatrixCustom.cpp \
	bindings/js/JSSVGPathSegCustom.cpp \
	bindings/js/JSSVGPathSegListCustom.cpp \
	bindings/js/JSSharedWorkerConstructor.cpp \
	bindings/js/JSSharedWorkerCustom.cpp \
	bindings/js/JSStorageCustom.cpp \
	bindings/js/JSStyleSheetCustom.cpp \
	bindings/js/JSStyleSheetListCustom.cpp \
	bindings/js/JSTextCustom.cpp \
	bindings/js/JSTreeWalkerCustom.cpp \
	bindings/js/JSWebKitCSSMatrixConstructor.cpp \
	bindings/js/JSWebKitPointConstructor.cpp \
	bindings/js/JSWorkerConstructor.cpp \
	bindings/js/JSWorkerContextBase.cpp \
	bindings/js/JSWorkerContextCustom.cpp \
	bindings/js/JSWorkerContextErrorHandler.cpp \
	bindings/js/JSWorkerCustom.cpp \
	bindings/js/JSXMLHttpRequestConstructor.cpp \
	bindings/js/JSXMLHttpRequestCustom.cpp \
	bindings/js/JSXMLHttpRequestUploadCustom.cpp \
	bindings/js/JSXSLTProcessorConstructor.cpp \
	bindings/js/JSXSLTProcessorCustom.cpp \
	bindings/js/ScheduledAction.cpp \
	bindings/js/ScriptArray.cpp \
	bindings/js/ScriptCachedFrameData.cpp \
	bindings/js/ScriptCallFrame.cpp \
	bindings/js/ScriptCallStack.cpp \
	bindings/js/ScriptController.cpp \
	bindings/js/ScriptEventListener.cpp \
	bindings/js/ScriptFunctionCall.cpp \
	bindings/js/ScriptObject.cpp \
	bindings/js/ScriptState.cpp \
	bindings/js/ScriptValue.cpp \
	bindings/js/SerializedScriptValue.cpp \
	bindings/js/WorkerScriptController.cpp \
	\
	bindings/ScriptControllerBase.cpp \
	\
	bridge/IdentifierRep.cpp \
	bridge/NP_jsobject.cpp \
	bridge/c/c_class.cpp \
	bridge/c/c_instance.cpp \
	bridge/c/c_runtime.cpp \
	bridge/c/c_utility.cpp \
	bridge/jni/JNIBridge.cpp \
	bridge/jni/JNIUtility.cpp \
	bridge/jni/jsc/JNIBridgeJSC.cpp \
	bridge/jni/jsc/JNIUtilityPrivate.cpp \
	bridge/jni/jsc/JavaClassJSC.cpp \
	bridge/jni/jsc/JavaInstanceJSC.cpp \
	bridge/jsc/BridgeJSC.cpp \
	bridge/npruntime.cpp \
	bridge/runtime_array.cpp \
	bridge/runtime_method.cpp \
	bridge/runtime_object.cpp \
	bridge/runtime_root.cpp
