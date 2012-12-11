list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/bindings/js"
    "${WEBCORE_DIR}/bridge/jsc"
)

list(APPEND WebCoreTestSupport_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/testing/js"
)

list(APPEND WebCore_IDL_INCLUDES
    bindings/js
)

if (PORT MATCHES "BlackBerry")
    list(APPEND WebCore_IDL_INCLUDES
        testing/js
    )
    list(APPEND WebCore_IDL_FILES
        testing/Internals.idl
        testing/InternalSettings.idl
    )
endif ()

list(APPEND WebCore_SOURCES
    bindings/js/ArrayValue.cpp
    bindings/js/BindingState.cpp
    bindings/js/CallbackFunction.cpp
    bindings/js/DOMObjectHashTableMap.cpp
    bindings/js/DOMWrapperWorld.cpp
    bindings/js/Dictionary.cpp
    bindings/js/GCController.cpp
    bindings/js/JSArrayBufferCustom.cpp
    bindings/js/JSAttrCustom.cpp
    bindings/js/JSBlobCustom.cpp
    bindings/js/JSCDATASectionCustom.cpp
    bindings/js/JSDataViewCustom.cpp
    bindings/js/JSCSSFontFaceRuleCustom.cpp
    bindings/js/JSCSSImportRuleCustom.cpp
    bindings/js/JSCSSMediaRuleCustom.cpp
    bindings/js/JSCSSPageRuleCustom.cpp
    bindings/js/JSCSSRuleCustom.cpp
    bindings/js/JSCSSRuleListCustom.cpp
    bindings/js/JSCSSStyleDeclarationCustom.cpp
    bindings/js/JSCSSStyleRuleCustom.cpp
    bindings/js/JSCSSValueCustom.cpp
    bindings/js/JSCallbackData.cpp
    bindings/js/JSCanvasRenderingContext2DCustom.cpp
    bindings/js/JSCanvasRenderingContextCustom.cpp
    bindings/js/JSClipboardCustom.cpp
    bindings/js/JSConsoleCustom.cpp
    bindings/js/JSCoordinatesCustom.cpp
    bindings/js/JSCustomXPathNSResolver.cpp
    bindings/js/JSDictionary.cpp
    bindings/js/JSDOMBinding.cpp
    bindings/js/JSDOMFormDataCustom.cpp
    bindings/js/JSDOMGlobalObject.cpp
    bindings/js/JSDOMImplementationCustom.cpp
    bindings/js/JSDOMMimeTypeArrayCustom.cpp
    bindings/js/JSDOMPluginArrayCustom.cpp
    bindings/js/JSDOMPluginCustom.cpp
    bindings/js/JSDOMStringListCustom.cpp
    bindings/js/JSDOMStringMapCustom.cpp
    bindings/js/JSDOMTokenListCustom.cpp
    bindings/js/JSDOMWindowBase.cpp
    bindings/js/JSDOMWindowCustom.cpp
    bindings/js/JSDOMWindowShell.cpp
    bindings/js/JSDOMWindowWebAudioCustom.cpp
    bindings/js/JSDOMWindowWebSocketCustom.cpp
    bindings/js/JSDOMWrapper.cpp
    bindings/js/JSDeviceMotionEventCustom.cpp
    bindings/js/JSDeviceOrientationEventCustom.cpp
    bindings/js/JSDocumentCustom.cpp
    bindings/js/JSElementCustom.cpp
    bindings/js/JSErrorHandler.cpp
    bindings/js/JSEventCustom.cpp
    bindings/js/JSEventListener.cpp
    bindings/js/JSEventTargetCustom.cpp
    bindings/js/JSExceptionBase.cpp
    bindings/js/JSGeolocationCustom.cpp
    bindings/js/JSHTMLAllCollectionCustom.cpp
    bindings/js/JSHTMLAppletElementCustom.cpp
    bindings/js/JSHTMLCanvasElementCustom.cpp
    bindings/js/JSHTMLCollectionCustom.cpp
    bindings/js/JSHTMLDocumentCustom.cpp
    bindings/js/JSHTMLElementCustom.cpp
    bindings/js/JSHTMLEmbedElementCustom.cpp
    bindings/js/JSHTMLFormControlsCollectionCustom.cpp
    bindings/js/JSHTMLFormElementCustom.cpp
    bindings/js/JSHTMLFrameElementCustom.cpp
    bindings/js/JSHTMLFrameSetElementCustom.cpp
    bindings/js/JSHTMLInputElementCustom.cpp
    bindings/js/JSHTMLLinkElementCustom.cpp
    bindings/js/JSHTMLMediaElementCustom.cpp
    bindings/js/JSHTMLObjectElementCustom.cpp
    bindings/js/JSHTMLOptionsCollectionCustom.cpp
    bindings/js/JSHTMLOutputElementCustom.cpp
    bindings/js/JSHTMLSelectElementCustom.cpp
    bindings/js/JSHTMLStyleElementCustom.cpp
    bindings/js/JSHTMLTemplateElementCustom.cpp
    bindings/js/JSHistoryCustom.cpp
    bindings/js/JSImageConstructor.cpp
    bindings/js/JSImageDataCustom.cpp
    bindings/js/JSInjectedScriptHostCustom.cpp
    bindings/js/JSInjectedScriptManager.cpp
    bindings/js/JSInspectorFrontendHostCustom.cpp
    bindings/js/JSJavaScriptCallFrameCustom.cpp
    bindings/js/JSLazyEventListener.cpp
    bindings/js/JSLocationCustom.cpp
    bindings/js/JSMainThreadExecState.cpp
    bindings/js/JSMediaListCustom.cpp
    bindings/js/JSMemoryInfoCustom.cpp
    bindings/js/JSMessageChannelCustom.cpp
    bindings/js/JSMessageEventCustom.cpp
    bindings/js/JSMessagePortCustom.cpp
    bindings/js/JSMicroDataItemValueCustom.cpp
    bindings/js/JSMutationCallbackCustom.cpp
    bindings/js/JSMutationObserverCustom.cpp
    bindings/js/JSNamedNodeMapCustom.cpp
    bindings/js/JSNodeCustom.cpp
    bindings/js/JSNodeFilterCondition.cpp
    bindings/js/JSNodeFilterCustom.cpp
    bindings/js/JSNodeIteratorCustom.cpp
    bindings/js/JSNodeListCustom.cpp
    bindings/js/JSNotificationCustom.cpp
    bindings/js/JSPluginElementFunctions.cpp
    bindings/js/JSPopStateEventCustom.cpp
    bindings/js/JSProcessingInstructionCustom.cpp
    bindings/js/JSStorageCustom.cpp
    bindings/js/JSStyleSheetCustom.cpp
    bindings/js/JSStyleSheetListCustom.cpp
    bindings/js/JSTextCustom.cpp
    bindings/js/JSTouchCustom.cpp
    bindings/js/JSTouchListCustom.cpp
    bindings/js/JSTreeWalkerCustom.cpp
    bindings/js/JSWebKitCSSKeyframeRuleCustom.cpp
    bindings/js/JSWebKitCSSKeyframesRuleCustom.cpp
    bindings/js/JSWebKitPointCustom.cpp
    bindings/js/JSXMLHttpRequestCustom.cpp
    bindings/js/JSXMLHttpRequestUploadCustom.cpp
    bindings/js/JSXPathResultCustom.cpp
    bindings/js/JSXSLTProcessorCustom.cpp
    bindings/js/JavaScriptCallFrame.cpp
    bindings/js/PageScriptDebugServer.cpp
    bindings/js/ScheduledAction.cpp
    bindings/js/ScriptCachedFrameData.cpp
    bindings/js/ScriptCallStackFactory.cpp
    bindings/js/ScriptController.cpp
    bindings/js/ScriptDebugServer.cpp
    bindings/js/ScriptEventListener.cpp
    bindings/js/ScriptFunctionCall.cpp
    bindings/js/ScriptGCEvent.cpp
    bindings/js/ScriptObject.cpp
    bindings/js/ScriptProfile.cpp
    bindings/js/ScriptProfiler.cpp
    bindings/js/ScriptState.cpp
    bindings/js/ScriptValue.cpp
    bindings/js/SerializedScriptValue.cpp

    bridge/IdentifierRep.cpp
    bridge/NP_jsobject.cpp
    bridge/npruntime.cpp
    bridge/runtime_array.cpp
    bridge/runtime_method.cpp
    bridge/runtime_object.cpp
    bridge/runtime_root.cpp

    bridge/c/CRuntimeObject.cpp
    bridge/c/c_class.cpp
    bridge/c/c_instance.cpp
    bridge/c/c_runtime.cpp
    bridge/c/c_utility.cpp

    bridge/jsc/BridgeJSC.cpp
)

list(APPEND WebCoreTestSupport_SOURCES
    testing/js/WebCoreTestSupport.cpp
)

if (ENABLE_BLOB)
    list(APPEND WebCore_SOURCES
        bindings/js/JSFileReaderCustom.cpp
    )
endif ()

if (ENABLE_REQUEST_ANIMATION_FRAME)
    list(APPEND WebCore_SOURCES
        bindings/js/JSRequestAnimationFrameCallbackCustom.cpp
    )
endif ()

if (ENABLE_SQL_DATABASE)
    list(APPEND WebCore_SOURCES
        bindings/js/JSCustomSQLStatementErrorCallback.cpp
        bindings/js/JSSQLResultSetRowListCustom.cpp
        bindings/js/JSSQLTransactionCustom.cpp
        bindings/js/JSSQLTransactionSyncCustom.cpp
    )
endif ()

if (ENABLE_INDEXED_DATABASE)
    list(APPEND WebCore_SOURCES
        bindings/js/IDBBindingUtilities.cpp
        bindings/js/JSIDBAnyCustom.cpp
        bindings/js/JSIDBKeyCustom.cpp
    )
endif ()

if (ENABLE_WEB_SOCKETS)
    list(APPEND WebCore_SOURCES
        bindings/js/JSWebSocketCustom.cpp
    )
endif ()

if (ENABLE_WORKERS)
    list(APPEND WebCore_SOURCES
        bindings/js/JSDedicatedWorkerContextCustom.cpp
        bindings/js/JSWorkerContextBase.cpp
        bindings/js/JSWorkerContextCustom.cpp
        bindings/js/JSWorkerCustom.cpp
        bindings/js/WorkerScriptController.cpp
        bindings/js/WorkerScriptDebugServer.cpp
    )
endif ()

if (ENABLE_VIDEO_TRACK)
    list(APPEND WebCore_SOURCES
        bindings/js/JSTextTrackCueCustom.cpp
        bindings/js/JSTextTrackCustom.cpp
        bindings/js/JSTextTrackListCustom.cpp
        bindings/js/JSTrackCustom.cpp
        bindings/js/JSTrackEventCustom.cpp
    )
endif ()

if (ENABLE_SHARED_WORKERS)
    list(APPEND WebCore_SOURCES
        bindings/js/JSSharedWorkerCustom.cpp
    )
endif ()

if (ENABLE_NOTIFICATIONS)
    list(APPEND WebCore_SOURCES
        bindings/js/JSDesktopNotificationsCustom.cpp
        bindings/js/JSNotificationCustom.cpp
    )
endif ()

if (ENABLE_FILE_SYSTEM)
    list(APPEND WebCore_SOURCES
        bindings/js/JSEntryCustom.cpp
        bindings/js/JSEntrySyncCustom.cpp
    )
endif ()

if (ENABLE_SVG)
    list(APPEND WebCore_SOURCES
        bindings/js/JSSVGElementInstanceCustom.cpp
        bindings/js/JSSVGLengthCustom.cpp
        bindings/js/JSSVGPathSegCustom.cpp
    )
endif ()

if (ENABLE_WEBGL)
    list(APPEND WebCore_SOURCES
        bindings/js/JSWebGLRenderingContextCustom.cpp
    )
endif ()

if (ENABLE_WEB_AUDIO)
    list(APPEND WebCore_SOURCES
        bindings/js/JSAudioBufferSourceNodeCustom.cpp
        bindings/js/JSAudioContextCustom.cpp
        bindings/js/JSScriptProcessorNodeCustom.cpp
    )
endif ()

if (ENABLE_WEB_INTENTS)
    list(APPEND WebCore_SOURCES
        bindings/js/JSIntentConstructor.cpp
    )
endif ()

list(APPEND SCRIPTS_BINDINGS
    ${WEBCORE_DIR}/bindings/scripts/CodeGenerator.pm
    ${WEBCORE_DIR}/bindings/scripts/CodeGeneratorJS.pm
)

set(IDL_INCLUDES "")
foreach (_include ${WebCore_IDL_INCLUDES})
    list(APPEND IDL_INCLUDES --include=${WEBCORE_DIR}/${_include})
endforeach ()

foreach (_include ${WebCoreTestSupport_IDL_INCLUDES})
    list(APPEND IDL_INCLUDES --include=${WEBCORE_DIR}/${_include})
endforeach ()

set(FEATURE_DEFINES_JAVASCRIPT "LANGUAGE_JAVASCRIPT=1 ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}")

# Create JavaScript C++ code given an IDL input
foreach (_idl ${WebCore_IDL_FILES})
    set(IDL_FILES_LIST "${IDL_FILES_LIST}${WEBCORE_DIR}/${_idl}\n")
endforeach ()

foreach (_idl ${WebCoreTestSupport_IDL_FILES})
    set(IDL_FILES_LIST "${IDL_FILES_LIST}${WEBCORE_DIR}/${_idl}\n")
endforeach ()

file(WRITE ${IDL_FILES_TMP} ${IDL_FILES_LIST})

add_custom_command(
    OUTPUT ${SUPPLEMENTAL_DEPENDENCY_FILE}
    DEPENDS ${WEBCORE_DIR}/bindings/scripts/preprocess-idls.pl ${SCRIPTS_PREPROCESS_IDLS} ${WebCore_IDL_FILES} ${WebCoreTestSupport_IDL_FILES} ${IDL_ATTRIBUTES_FILE}
    COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${WEBCORE_DIR}/bindings/scripts/preprocess-idls.pl --defines "${FEATURE_DEFINES_JAVASCRIPT}" --idlFilesList ${IDL_FILES_TMP} --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" --supplementalDependencyFile ${SUPPLEMENTAL_DEPENDENCY_FILE} --idlAttributesFile ${IDL_ATTRIBUTES_FILE}
    VERBATIM)

GENERATE_BINDINGS(WebCore_SOURCES
    "${WebCore_IDL_FILES}"
    "${WEBCORE_DIR}"
    "${IDL_INCLUDES}"
    "${FEATURE_DEFINES_JAVASCRIPT}"
    ${DERIVED_SOURCES_WEBCORE_DIR} JS JS
    ${SUPPLEMENTAL_DEPENDENCY_FILE})

GENERATE_BINDINGS(WebCoreTestSupport_SOURCES
    "${WebCoreTestSupport_IDL_FILES}"
    "${WEBCORE_DIR}"
    "${IDL_INCLUDES}"
    "${FEATURE_DEFINES_JAVASCRIPT}"
    ${DERIVED_SOURCES_WEBCORE_DIR} JS JS
    ${SUPPLEMENTAL_DEPENDENCY_FILE})
