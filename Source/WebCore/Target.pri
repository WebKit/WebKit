# -------------------------------------------------------------------
# Target file for the WebCore library
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = lib
TARGET = WebCore

include(WebCore.pri)

WEBKIT += wtf
!v8: WEBKIT += javascriptcore

CONFIG += staticlib

DEFINES += QT_MAKEDLL

haveQt(5) {
    # Add a QtScript dependency for the time being, in order to pull in the include
    # path for QtScript when it's built as a standalone module
    QT += script widgets
} else {
    INCLUDEPATH += $$PWD/../JavaScriptCore/wtf/qt/compat
}

RESOURCES += \
    $$PWD/WebCore.qrc

include_webinspector {
    RESOURCES += \
        $$PWD/inspector/front-end/WebKit.qrc \
        $${WEBCORE_GENERATED_SOURCES_DIR}/InspectorBackendStub.qrc
}

SOURCES += \
    accessibility/AccessibilityImageMapLink.cpp \
    accessibility/AccessibilityMediaControls.cpp \
    accessibility/AccessibilityMenuList.cpp \
    accessibility/AccessibilityMenuListOption.cpp \
    accessibility/AccessibilityMenuListPopup.cpp \
    accessibility/AccessibilityMockObject.cpp \
    accessibility/AccessibilityObject.cpp \
    accessibility/AccessibilityList.cpp \
    accessibility/AccessibilityListBox.cpp \
    accessibility/AccessibilityListBoxOption.cpp \
    accessibility/AccessibilityProgressIndicator.cpp \
    accessibility/AccessibilityRenderObject.cpp \
    accessibility/AccessibilityScrollbar.cpp \
    accessibility/AccessibilityScrollView.cpp \
    accessibility/AccessibilitySlider.cpp \
    accessibility/AccessibilitySpinButton.cpp \
    accessibility/AccessibilityARIAGrid.cpp \
    accessibility/AccessibilityARIAGridCell.cpp \
    accessibility/AccessibilityARIAGridRow.cpp \
    accessibility/AccessibilityTable.cpp \
    accessibility/AccessibilityTableCell.cpp \
    accessibility/AccessibilityTableColumn.cpp \
    accessibility/AccessibilityTableHeaderContainer.cpp \
    accessibility/AccessibilityTableRow.cpp \
    accessibility/AXObjectCache.cpp \
    bindings/generic/ActiveDOMCallback.cpp \
    bindings/generic/RuntimeEnabledFeatures.cpp

v8 {
    include($$PWD/../JavaScriptCore/yarr/yarr.pri)

    SOURCES += \
        platform/qt/PlatformSupportQt.cpp \
        bindings/generic/BindingSecurityBase.cpp \
        \
        bindings/v8/WorldContextHandle.cpp \
        bindings/v8/V8IsolatedContext.cpp \
        bindings/v8/custom/V8HistoryCustom.cpp \
        bindings/v8/custom/V8PopStateEventCustom.cpp \
        bindings/v8/ScriptGCEvent.cpp

    SOURCES += \
        bindings/v8/custom/V8ArrayBufferCustom.cpp \
        bindings/v8/custom/V8ArrayBufferViewCustom.cpp \
        bindings/v8/custom/V8BlobCustom.cpp \
        bindings/v8/custom/V8CustomXPathNSResolver.cpp \
        bindings/v8/custom/V8DataViewCustom.cpp \
        bindings/v8/custom/V8DeviceMotionEventCustom.cpp \
        bindings/v8/custom/V8DeviceOrientationEventCustom.cpp \
        bindings/v8/custom/V8Float32ArrayCustom.cpp \
        bindings/v8/custom/V8Float64ArrayCustom.cpp \
        bindings/v8/custom/V8Int8ArrayCustom.cpp \
        bindings/v8/custom/V8Int16ArrayCustom.cpp \
        bindings/v8/custom/V8Int32ArrayCustom.cpp \
        bindings/v8/custom/V8Uint8ArrayCustom.cpp \
        bindings/v8/custom/V8Uint16ArrayCustom.cpp \
        bindings/v8/custom/V8Uint32ArrayCustom.cpp \
        \
        bindings/v8/DateExtension.cpp \
        bindings/v8/DOMData.cpp \
        bindings/v8/DOMDataStore.cpp \
        bindings/v8/NPV8Object.cpp \
        bindings/v8/NPObjectWrapper.cpp \
        bindings/v8/OptionsObject.cpp \
        bindings/v8/PageScriptDebugServer.cpp \
        bindings/v8/RetainedDOMInfo.cpp \
        bindings/v8/ScheduledAction.cpp \
        bindings/v8/ScopedDOMDataStore.cpp \
        bindings/v8/ScriptCachedFrameData.cpp \
        bindings/v8/ScriptCallStackFactory.cpp \
        bindings/ScriptControllerBase.cpp \
        bindings/v8/ScriptController.cpp \
        bindings/v8/ScriptControllerQt.cpp \
        bindings/v8/ScriptEventListener.cpp \
        bindings/v8/ScriptFunctionCall.cpp \
        bindings/v8/ScriptInstance.cpp \
        bindings/v8/ScriptObject.cpp \
        bindings/v8/ScriptScope.cpp \
        bindings/v8/ScriptState.cpp \
        bindings/v8/ScriptValue.cpp \
        bindings/v8/StaticDOMDataStore.cpp \
        bindings/v8/SerializedScriptValue.cpp \
        bindings/v8/V8AbstractEventListener.cpp \
        bindings/v8/V8Binding.cpp \
        bindings/v8/V8Collection.cpp \
        bindings/v8/V8DOMMap.cpp \
        bindings/v8/V8DOMWrapper.cpp \
        bindings/v8/V8EventListener.cpp \
        bindings/v8/V8EventListenerList.cpp \
        bindings/v8/V8GCController.cpp \
        bindings/v8/V8GCForContextDispose.cpp \
        bindings/v8/V8Helpers.cpp \
        bindings/v8/V8HiddenPropertyName.cpp \
        bindings/v8/IsolatedWorld.cpp \
        bindings/v8/V8LazyEventListener.cpp \
        bindings/v8/V8NPObject.cpp \
        bindings/v8/V8NPUtils.cpp \
        bindings/v8/V8NodeFilterCondition.cpp \
        bindings/v8/V8Proxy.cpp \
        bindings/v8/V8RecursionScope.cpp \
        bindings/v8/V8Utilities.cpp \
        bindings/v8/V8WindowErrorHandler.cpp \
        bindings/v8/V8WorkerContextEventListener.cpp \
        bindings/v8/WorkerContextExecutionProxy.cpp \
        bindings/v8/WorkerScriptDebugServer.cpp \
        bindings/v8/WorkerScriptController.cpp \
        \
        bindings/v8/V8DOMWindowShell.cpp \
        bindings/v8/DOMWrapperWorld.cpp \
        \
        bindings/v8/npruntime.cpp \
        \
        bindings/v8/custom/V8CSSRuleCustom.cpp \
        bindings/v8/custom/V8CSSStyleDeclarationCustom.cpp \
        bindings/v8/custom/V8CSSStyleSheetCustom.cpp \
        bindings/v8/custom/V8CSSValueCustom.cpp \
        bindings/v8/custom/V8CanvasRenderingContext2DCustom.cpp \
        bindings/v8/custom/V8CanvasPixelArrayCustom.cpp \
        bindings/v8/custom/V8ClipboardCustom.cpp \
        bindings/v8/custom/V8CoordinatesCustom.cpp \
        bindings/v8/custom/V8ImageDataCustom.cpp \
        bindings/v8/custom/V8InjectedScriptHostCustom.cpp \
        bindings/v8/custom/V8InjectedScriptManager.cpp \
        bindings/v8/custom/V8InspectorFrontendHostCustom.cpp \
        bindings/v8/custom/V8DOMStringMapCustom.cpp \
        bindings/v8/custom/V8DOMTokenListCustom.cpp

    SOURCES += \
        bindings/v8/custom/V8CustomSQLStatementErrorCallback.cpp \
        bindings/v8/custom/V8CustomVoidCallback.cpp \
        bindings/v8/custom/V8DOMFormDataCustom.cpp \
        bindings/v8/custom/V8DOMWindowCustom.cpp \
        bindings/v8/custom/V8DedicatedWorkerContextCustom.cpp \
        bindings/v8/custom/V8DocumentCustom.cpp \
        bindings/v8/custom/V8DocumentLocationCustom.cpp \
        bindings/v8/custom/V8ElementCustom.cpp \
        bindings/v8/custom/V8EventCustom.cpp \
        bindings/v8/custom/V8FileReaderCustom.cpp \
        bindings/v8/custom/V8HTMLAllCollectionCustom.cpp

    SOURCES += \
        bindings/v8/custom/V8HTMLCanvasElementCustom.cpp \
        bindings/v8/custom/V8HTMLCollectionCustom.cpp \
        bindings/v8/custom/V8HTMLDocumentCustom.cpp \
        bindings/v8/custom/V8HTMLElementCustom.cpp \
        bindings/v8/custom/V8HTMLFormElementCustom.cpp \
        bindings/v8/custom/V8HTMLFrameElementCustom.cpp \
        bindings/v8/custom/V8HTMLFrameSetElementCustom.cpp \
        bindings/v8/custom/V8HTMLImageElementConstructor.cpp \
        bindings/v8/custom/V8HTMLInputElementCustom.cpp \
        bindings/v8/custom/V8HTMLLinkElementCustom.cpp \
        bindings/v8/custom/V8HTMLMediaElementCustom.cpp \
        bindings/v8/custom/V8HTMLOptionsCollectionCustom.cpp \
        bindings/v8/custom/V8HTMLOutputElementCustom.cpp \
        bindings/v8/custom/V8HTMLPlugInElementCustom.cpp \
        bindings/v8/custom/V8HTMLSelectElementCustom.cpp \
        bindings/v8/custom/V8LocationCustom.cpp \
        bindings/v8/custom/V8MessageChannelConstructor.cpp \
        bindings/v8/custom/V8MessagePortCustom.cpp \
        bindings/v8/custom/V8MessageEventCustom.cpp \
        bindings/v8/custom/V8MutationCallbackCustom.cpp \
        bindings/v8/custom/V8NamedNodeMapCustom.cpp \
        bindings/v8/custom/V8NamedNodesCollection.cpp \
        bindings/v8/custom/V8NodeCustom.cpp \
        bindings/v8/custom/V8NodeListCustom.cpp \
        bindings/v8/custom/V8PerformanceCustom.cpp \
        bindings/v8/custom/V8SQLResultSetRowListCustom.cpp \
        bindings/v8/custom/V8SQLTransactionCustom.cpp \
        bindings/v8/custom/V8WebSocketCustom.cpp \
        \
        bindings/v8/custom/V8StorageCustom.cpp \
        bindings/v8/custom/V8StyleSheetCustom.cpp \
        bindings/v8/custom/V8StyleSheetListCustom.cpp \
        bindings/v8/custom/V8WebKitAnimationCustom.cpp \
        bindings/v8/custom/V8WebKitMutationObserverCustom.cpp \
        bindings/v8/custom/V8WebKitPointConstructor.cpp \
        bindings/v8/custom/V8WorkerContextCustom.cpp \
        bindings/v8/custom/V8WorkerCustom.cpp \
        bindings/v8/custom/V8XMLHttpRequestConstructor.cpp \
        bindings/v8/custom/V8XMLHttpRequestCustom.cpp \
        \
        bindings/v8/custom/V8SVGDocumentCustom.cpp \
        bindings/v8/custom/V8SVGElementCustom.cpp \
        bindings/v8/custom/V8SVGLengthCustom.cpp \
        bindings/v8/custom/V8SVGPathSegCustom.cpp \
        \
        bindings/v8/specialization/V8BindingState.cpp\
        \
        bindings/v8/custom/V8NotificationCenterCustom.cpp \
        bindings/v8/custom/V8ConsoleCustom.cpp \
        bindings/v8/custom/V8SQLTransactionSyncCustom.cpp \
        bindings/v8/V8WorkerContextErrorHandler.cpp \
        testing/v8/V8InternalsCustom.cpp \
        testing/v8/WebCoreTestSupport.cpp
} else {
    SOURCES += \
        bindings/ScriptControllerBase.cpp \
        bindings/js/CallbackFunction.cpp \
        bindings/js/DOMObjectHashTableMap.cpp \
        bindings/js/DOMWrapperWorld.cpp \
        bindings/js/GCController.cpp \
        bindings/js/JSArrayBufferCustom.cpp \
        bindings/js/JSAttrCustom.cpp \
        bindings/js/JSBlobCustom.cpp \
        bindings/js/JSCDATASectionCustom.cpp \
        bindings/js/JSCSSFontFaceRuleCustom.cpp \
        bindings/js/JSCSSImportRuleCustom.cpp \
        bindings/js/JSCSSMediaRuleCustom.cpp \
        bindings/js/JSCSSPageRuleCustom.cpp \
        bindings/js/JSCSSRuleCustom.cpp \
        bindings/js/JSCSSRuleListCustom.cpp \
        bindings/js/JSCSSStyleDeclarationCustom.cpp \
        bindings/js/JSCSSStyleRuleCustom.cpp \
        bindings/js/JSCSSValueCustom.cpp \
        bindings/js/JSCallbackData.cpp \
        bindings/js/JSCanvasRenderingContext2DCustom.cpp \
        bindings/js/JSCanvasRenderingContextCustom.cpp \
        bindings/js/JSClipboardCustom.cpp \
        bindings/js/JSConsoleCustom.cpp \
        bindings/js/JSCoordinatesCustom.cpp \
        bindings/js/JSCustomVoidCallback.cpp \
        bindings/js/JSCustomXPathNSResolver.cpp \
        bindings/js/JSDictionary.cpp \
        bindings/js/JSDOMBinding.cpp \
        bindings/js/JSDOMFormDataCustom.cpp \
        bindings/js/JSDOMGlobalObject.cpp \
        bindings/js/JSDOMImplementationCustom.cpp \
        bindings/js/JSDOMMimeTypeArrayCustom.cpp \
        bindings/js/JSDOMPluginArrayCustom.cpp \
        bindings/js/JSDOMPluginCustom.cpp \
        bindings/js/JSDOMStringMapCustom.cpp \
        bindings/js/JSDOMTokenListCustom.cpp \
        bindings/js/JSDOMWindowBase.cpp \
        bindings/js/JSDOMWindowCustom.cpp \
        bindings/js/JSDOMWindowShell.cpp \
        bindings/js/JSDOMWindowWebAudioCustom.cpp \
        bindings/js/JSDOMWindowWebSocketCustom.cpp \
        bindings/js/JSDOMWrapper.cpp \
        bindings/js/JSDataViewCustom.cpp \
        bindings/js/JSDesktopNotificationsCustom.cpp \
        bindings/js/JSDeviceMotionEventCustom.cpp \
        bindings/js/JSDeviceOrientationEventCustom.cpp \
        bindings/js/JSDocumentCustom.cpp \
        bindings/js/JSElementCustom.cpp \
        bindings/js/JSErrorHandler.cpp \
        bindings/js/JSEventCustom.cpp \
        bindings/js/JSEventListener.cpp \
        bindings/js/JSEventTarget.cpp \
        bindings/js/JSExceptionBase.cpp \
        bindings/js/JSFileReaderCustom.cpp \
        bindings/js/JSFloat32ArrayCustom.cpp \
        bindings/js/JSFloat64ArrayCustom.cpp \
        bindings/js/JSGeolocationCustom.cpp \
        bindings/js/JSHTMLAllCollectionCustom.cpp \
        bindings/js/JSHTMLAppletElementCustom.cpp \
        bindings/js/JSHTMLCanvasElementCustom.cpp \
        bindings/js/JSHTMLCollectionCustom.cpp \
        bindings/js/JSHTMLDocumentCustom.cpp \
        bindings/js/JSHTMLElementCustom.cpp \
        bindings/js/JSHTMLEmbedElementCustom.cpp \
        bindings/js/JSHTMLFormElementCustom.cpp \
        bindings/js/JSHTMLFrameElementCustom.cpp \
        bindings/js/JSHTMLFrameSetElementCustom.cpp \
        bindings/js/JSHTMLInputElementCustom.cpp \
        bindings/js/JSHTMLLinkElementCustom.cpp \
        bindings/js/JSHTMLMediaElementCustom.cpp \
        bindings/js/JSHTMLObjectElementCustom.cpp \
        bindings/js/JSHTMLOptionsCollectionCustom.cpp \
        bindings/js/JSHTMLOutputElementCustom.cpp \
        bindings/js/JSHTMLSelectElementCustom.cpp \
        bindings/js/JSHTMLStyleElementCustom.cpp \
        bindings/js/JSHistoryCustom.cpp \
        bindings/js/JSImageConstructor.cpp \
        bindings/js/JSImageDataCustom.cpp \
        bindings/js/JSInjectedScriptHostCustom.cpp \
        bindings/js/JSInjectedScriptManager.cpp \
        bindings/js/JSInspectorFrontendHostCustom.cpp \
        bindings/js/JSInt16ArrayCustom.cpp \
        bindings/js/JSInt32ArrayCustom.cpp \
        bindings/js/JSInt8ArrayCustom.cpp \
        bindings/js/JSLazyEventListener.cpp \
        bindings/js/JSLocationCustom.cpp \
        bindings/js/JSMainThreadExecState.cpp \
        bindings/js/JSMediaListCustom.cpp \
        bindings/js/JSMemoryInfoCustom.cpp \
        bindings/js/JSMessageChannelCustom.cpp \
        bindings/js/JSMessageEventCustom.cpp \
        bindings/js/JSMessagePortCustom.cpp \
        bindings/js/JSMessagePortCustom.h \
        bindings/js/JSMutationCallbackCustom.cpp \
        bindings/js/JSNamedNodeMapCustom.cpp \
        bindings/js/JSNodeCustom.cpp \
        bindings/js/JSNodeFilterCondition.cpp \
        bindings/js/JSNodeFilterCustom.cpp \
        bindings/js/JSNodeIteratorCustom.cpp \
        bindings/js/JSNodeListCustom.cpp \
        bindings/js/JSPluginElementFunctions.cpp \
        bindings/js/JSPopStateEventCustom.cpp \
        bindings/js/JSProcessingInstructionCustom.cpp \
        bindings/js/JSRequestAnimationFrameCallbackCustom.cpp \
        bindings/js/JSScriptProfileNodeCustom.cpp \
        bindings/js/JSStorageCustom.cpp \
        bindings/js/JSStyleSheetCustom.cpp \
        bindings/js/JSStyleSheetListCustom.cpp \
        bindings/js/JSTextCustom.cpp \
        bindings/js/JSTouchCustom.cpp \
        bindings/js/JSTouchListCustom.cpp \
        bindings/js/JSTreeWalkerCustom.cpp \
        bindings/js/JSUint16ArrayCustom.cpp \
        bindings/js/JSUint32ArrayCustom.cpp \
        bindings/js/JSUint8ArrayCustom.cpp \
        bindings/js/JSUint8ClampedArrayCustom.cpp \
        bindings/js/JSWebKitAnimationCustom.cpp \
        bindings/js/JSWebKitAnimationListCustom.cpp \
        bindings/js/JSWebKitCSSKeyframeRuleCustom.cpp \
        bindings/js/JSWebKitCSSKeyframesRuleCustom.cpp \
        bindings/js/JSWebKitMutationObserverCustom.cpp \
        bindings/js/JSWebKitPointCustom.cpp \
        bindings/js/JSXMLHttpRequestCustom.cpp \
        bindings/js/JSXMLHttpRequestUploadCustom.cpp \
        bindings/js/JSXPathResultCustom.cpp \
        bindings/js/PageScriptDebugServer.cpp \
        bindings/js/ScheduledAction.cpp \
        bindings/js/ScriptCachedFrameData.cpp \
        bindings/js/ScriptCallStackFactory.cpp \
        bindings/js/ScriptController.cpp \
        bindings/js/ScriptControllerQt.cpp \
        bindings/js/ScriptDebugServer.cpp \
        bindings/js/ScriptEventListener.cpp \
        bindings/js/ScriptFunctionCall.cpp \
        bindings/js/ScriptGCEvent.cpp \
        bindings/js/ScriptObject.cpp \
        bindings/js/ScriptProfile.cpp \
        bindings/js/ScriptState.cpp \
        bindings/js/ScriptValue.cpp \
        bindings/js/SerializedScriptValue.cpp \
        bridge/IdentifierRep.cpp \
        bridge/NP_jsobject.cpp \
        bridge/c/CRuntimeObject.cpp \
        bridge/c/c_class.cpp \
        bridge/c/c_instance.cpp \
        bridge/c/c_runtime.cpp \
        bridge/c/c_utility.cpp \
        bridge/jsc/BridgeJSC.cpp \
        bridge/npruntime.cpp \
        bridge/qt/qt_class.cpp \
        bridge/qt/qt_instance.cpp \
        bridge/qt/qt_pixmapruntime.cpp \
        bridge/qt/qt_runtime.cpp \
        bridge/runtime_array.cpp \
        bridge/runtime_method.cpp \
        bridge/runtime_object.cpp \
        bridge/runtime_root.cpp \
        testing/js/JSInternalsCustom.cpp \
        testing/js/WebCoreTestSupport.cpp
}

SOURCES += \
    css/CSSAspectRatioValue.cpp \
    css/CSSBorderImageSliceValue.cpp \
    css/CSSBorderImage.cpp \
    css/CSSCanvasValue.cpp \
    css/CSSCharsetRule.cpp \
    css/CSSComputedStyleDeclaration.cpp \
    css/CSSCrossfadeValue.cpp \
    css/CSSCursorImageValue.cpp \
    css/CSSFlexValue.cpp \
    css/CSSFontFace.cpp \
    css/CSSFontFaceRule.cpp \
    css/CSSFontFaceSrcValue.cpp \
    css/CSSFontSelector.cpp \
    css/CSSFontFaceSource.cpp \
    css/CSSFunctionValue.cpp \
    css/CSSGradientValue.cpp \
    css/CSSImageValue.cpp \
    css/CSSImageGeneratorValue.cpp \
    css/CSSImportRule.cpp \
    css/CSSInheritedValue.cpp \
    css/CSSInitialValue.cpp \
    css/CSSLineBoxContainValue.cpp \
    css/CSSMediaRule.cpp \
    css/CSSMutableStyleDeclaration.cpp \
    css/CSSOMUtils.cpp \
    css/CSSPageRule.cpp \
    css/CSSParser.cpp \
    css/CSSParserValues.cpp \
    css/CSSPrimitiveValue.cpp \
    css/CSSProperty.cpp \
    css/CSSPropertyLonghand.cpp \
    css/CSSPropertySourceData.cpp \
    css/CSSReflectValue.cpp \
    css/CSSRule.cpp \
    css/CSSRuleList.cpp \
    css/CSSSelector.cpp \
    css/CSSSelectorList.cpp \
    css/CSSSegmentedFontFace.cpp \
    css/CSSStyleApplyProperty.cpp \
    css/CSSStyleDeclaration.cpp \
    css/CSSStyleRule.cpp \
    css/CSSStyleSelector.cpp \
    css/CSSStyleSheet.cpp \
    css/CSSTimingFunctionValue.cpp \
    css/CSSUnicodeRangeValue.cpp \
    css/CSSValue.cpp \
    css/CSSValueList.cpp \
    css/CSSValuePool.cpp \
    css/CSSWrapShapes.cpp \
    css/FontFamilyValue.cpp \
    css/FontFeatureValue.cpp \
    css/FontValue.cpp \
    css/MediaFeatureNames.cpp \
    css/MediaList.cpp \
    css/MediaQuery.cpp \
    css/MediaQueryEvaluator.cpp \
    css/MediaQueryExp.cpp \
    css/MediaQueryList.cpp \
    css/MediaQueryListListener.cpp \
    css/MediaQueryMatcher.cpp \
    css/RGBColor.cpp \
    css/SelectorChecker.cpp \
    css/ShadowValue.cpp \
    css/StyleMedia.cpp \
    css/StyleSheet.cpp \
    css/StyleSheetList.cpp \
    css/WebKitCSSFilterValue.cpp \
    css/WebKitCSSKeyframeRule.cpp \
    css/WebKitCSSKeyframesRule.cpp \
    css/WebKitCSSMatrix.cpp \
    css/WebKitCSSRegionRule.cpp \
    css/WebKitCSSShaderValue.cpp \
    css/WebKitCSSTransformValue.cpp \
    dom/ActiveDOMObject.cpp \
    dom/Attr.cpp \
    dom/Attribute.cpp \
    dom/BeforeTextInsertedEvent.cpp \
    dom/BeforeUnloadEvent.cpp \
    dom/CDATASection.cpp \
    dom/CharacterData.cpp \
    dom/CheckedRadioButtons.cpp \
    dom/ChildListMutationScope.cpp \
    dom/ChildNodeList.cpp \
    dom/ClassNodeList.cpp \
    dom/ClientRect.cpp \
    dom/ClientRectList.cpp \
    dom/Clipboard.cpp \
    dom/ClipboardEvent.cpp \
    dom/Comment.cpp \
    dom/CompositionEvent.cpp \
    dom/ContainerNode.cpp \
    dom/CSSMappedAttributeDeclaration.cpp \
    dom/CustomEvent.cpp \
    dom/DecodedDataDocumentParser.cpp \
    dom/DeviceMotionController.cpp \
    dom/DeviceMotionData.cpp \
    dom/DeviceMotionEvent.cpp \
    dom/DeviceOrientation.cpp \
    dom/DeviceOrientationController.cpp \
    dom/DeviceOrientationEvent.cpp \
    dom/Document.cpp \
    dom/DocumentEventQueue.cpp \
    dom/DocumentFragment.cpp \
    dom/DocumentMarkerController.cpp \
    dom/DocumentMarker.cpp \
    dom/DocumentOrderedMap.cpp \
    dom/DocumentParser.cpp \
    dom/DocumentType.cpp \
    dom/DOMCoreException.cpp \
    dom/DOMImplementation.cpp \
    dom/DOMStringList.cpp \
    dom/DOMStringMap.cpp \
    dom/DatasetDOMStringMap.cpp \
    dom/DynamicNodeList.cpp \
    dom/EditingText.cpp \
    dom/Element.cpp \
    dom/EntityReference.cpp \
    dom/ErrorEvent.cpp \
    dom/Event.cpp \
    dom/EventContext.cpp \
    dom/EventDispatchMediator.cpp \
    dom/EventDispatcher.cpp \
    dom/EventException.cpp \
    dom/EventListenerMap.cpp \
    dom/EventNames.cpp \
    dom/EventTarget.cpp \
    dom/ExceptionBase.cpp \
    dom/ExceptionCodePlaceholder.cpp \
    dom/GenericEventQueue.cpp \
    dom/IconURL.cpp \
    dom/KeyboardEvent.cpp \
    dom/MessageChannel.cpp \
    dom/MessageEvent.cpp \
    dom/MessagePort.cpp \
    dom/MessagePortChannel.cpp \
    dom/MicroDataItemList.cpp \
    dom/MouseEvent.cpp \
    dom/MouseRelatedEvent.cpp \
    dom/MutationEvent.cpp \
    dom/MutationObserverInterestGroup.cpp \
    dom/MutationObserverRegistration.cpp \
    dom/MutationRecord.cpp \
    dom/NamedNodeMap.cpp \
    dom/NameNodeList.cpp \
    dom/Node.cpp \
    dom/NodeFilterCondition.cpp \
    dom/NodeFilter.cpp \
    dom/NodeIterator.cpp \
    dom/NodeRenderingContext.cpp \
    dom/Notation.cpp \
    dom/StaticHashSetNodeList.cpp \
    dom/OverflowEvent.cpp \
    dom/PageTransitionEvent.cpp \
    dom/PendingScript.cpp \
    dom/PopStateEvent.cpp \
    dom/Position.cpp \
    dom/PositionIterator.cpp \
    dom/ProcessingInstruction.cpp \
    dom/ProgressEvent.cpp \
    dom/QualifiedName.cpp \
    dom/Range.cpp \
    dom/RangeException.cpp \
    dom/RawDataDocumentParser.h \
    dom/RegisteredEventListener.cpp \
    dom/ScopedEventQueue.cpp \
    dom/ScriptedAnimationController.cpp \
    dom/ScriptableDocumentParser.cpp \
    dom/ScriptElement.cpp \
    dom/ScriptExecutionContext.cpp \
    dom/ScriptRunner.cpp \
    dom/SecurityContext.cpp \
    dom/SelectorQuery.cpp \
    dom/ShadowRoot.cpp \
    dom/SpaceSplitString.cpp \
    dom/StaticNodeList.cpp \
    dom/StyledElement.cpp \
    dom/StyleElement.cpp \
    dom/TagNodeList.cpp \
    dom/Text.cpp \
    dom/TextEvent.cpp \
    dom/Touch.cpp \
    dom/TouchEvent.cpp \
    dom/TouchList.cpp \
    dom/Traversal.cpp \
    dom/TreeScope.cpp \
    dom/TreeScopeAdopter.cpp \
    dom/TreeWalker.cpp \
    dom/UIEvent.cpp \
    dom/UIEventWithKeyState.cpp \
    dom/UserGestureIndicator.cpp \
    dom/UserTypingGestureIndicator.cpp \
    dom/ViewportArguments.cpp \
    dom/WebKitAnimationEvent.cpp \
    dom/WebKitMutationObserver.cpp \
    dom/WebKitNamedFlow.cpp \
    dom/WebKitTransitionEvent.cpp \
    dom/WheelEvent.cpp \
    dom/WindowEventContext.cpp \
    dom/default/PlatformMessagePortChannel.cpp \
    editing/AppendNodeCommand.cpp \
    editing/ApplyBlockElementCommand.cpp \
    editing/ApplyStyleCommand.cpp \
    editing/BreakBlockquoteCommand.cpp \
    editing/CompositeEditCommand.cpp \
    editing/CreateLinkCommand.cpp \
    editing/DeleteButtonController.cpp \
    editing/DeleteButton.cpp \
    editing/DeleteFromTextNodeCommand.cpp \
    editing/DeleteSelectionCommand.cpp \
    editing/EditCommand.cpp \
    editing/EditingStyle.cpp \
    editing/Editor.cpp \
    editing/EditorCommand.cpp \
    editing/FormatBlockCommand.cpp \
    editing/FrameSelection.cpp \
    editing/htmlediting.cpp \
    editing/HTMLInterchange.cpp \
    editing/IndentOutdentCommand.cpp \
    editing/InsertIntoTextNodeCommand.cpp \
    editing/InsertLineBreakCommand.cpp \
    editing/InsertListCommand.cpp \
    editing/InsertNodeBeforeCommand.cpp \
    editing/InsertParagraphSeparatorCommand.cpp \
    editing/InsertTextCommand.cpp \
    editing/markup.cpp \
    editing/MarkupAccumulator.cpp \
    editing/MergeIdenticalElementsCommand.cpp \
    editing/ModifySelectionListLevel.cpp \
    editing/MoveSelectionCommand.cpp \
    editing/RemoveCSSPropertyCommand.cpp \
    editing/RemoveFormatCommand.cpp \
    editing/RemoveNodeCommand.cpp \
    editing/RemoveNodePreservingChildrenCommand.cpp \
    editing/RenderedPosition.cpp \
    editing/ReplaceNodeWithSpanCommand.cpp \
    editing/ReplaceSelectionCommand.cpp \
    editing/SetNodeAttributeCommand.cpp \
    editing/SetSelectionCommand.cpp \
    editing/SpellChecker.cpp \
    editing/SpellingCorrectionCommand.cpp \
    editing/SpellingCorrectionController.cpp \
    editing/SplitElementCommand.cpp \
    editing/SplitTextNodeCommand.cpp \
    editing/SplitTextNodeContainingElementCommand.cpp \
    editing/TextCheckingHelper.cpp \
    editing/TextIterator.cpp \
    editing/TypingCommand.cpp \
    editing/UnlinkCommand.cpp \
    editing/VisiblePosition.cpp \
    editing/VisibleSelection.cpp \
    editing/visible_units.cpp \
    editing/WrapContentsInDummySpanCommand.cpp \
    fileapi/Blob.cpp \
    fileapi/BlobURL.cpp \
    fileapi/DOMFilePath.cpp \
    fileapi/DOMFileSystem.cpp \
    fileapi/DOMFileSystemBase.cpp \
    fileapi/DOMFileSystemSync.cpp \
    fileapi/DirectoryEntry.cpp \
    fileapi/DirectoryEntrySync.cpp \
    fileapi/DirectoryReader.cpp \
    fileapi/DirectoryReaderSync.cpp \
    fileapi/Entry.cpp \
    fileapi/EntryArray.cpp \
    fileapi/EntryArraySync.cpp \
    fileapi/EntryBase.cpp \
    fileapi/EntrySync.cpp \
    fileapi/File.cpp \
    fileapi/FileEntry.cpp \
    fileapi/FileEntrySync.cpp \
    fileapi/FileException.cpp \
    fileapi/FileList.cpp \
    fileapi/FileReader.cpp \
    fileapi/FileReaderLoader.cpp \
    fileapi/FileReaderSync.cpp \
    fileapi/FileStreamProxy.cpp \
    fileapi/FileSystemCallbacks.cpp \
    fileapi/FileThread.cpp \
    fileapi/FileWriter.cpp \
    fileapi/FileWriterBase.cpp \
    fileapi/FileWriterSync.cpp \
    fileapi/LocalFileSystem.cpp \
    fileapi/OperationNotAllowedException.cpp \
    fileapi/ThreadableBlobRegistry.cpp \
    fileapi/WebKitBlobBuilder.cpp \
    history/BackForwardController.cpp \
    history/BackForwardListImpl.cpp \
    history/CachedFrame.cpp \
    history/CachedPage.cpp \
    history/HistoryItem.cpp \
    history/qt/HistoryItemQt.cpp \
    history/PageCache.cpp \
    html/BaseButtonInputType.cpp \
    html/BaseCheckableInputType.cpp \
    html/BaseDateAndTimeInputType.cpp \
    html/BaseTextInputType.cpp \
    html/ButtonInputType.cpp \
    html/CheckboxInputType.cpp \
    html/ClassList.cpp \
    html/ColorInputType.cpp \
    html/DOMFormData.cpp \
    html/DOMSettableTokenList.cpp \
    html/DOMTokenList.cpp \
    html/DOMURL.cpp \
    html/DateInputType.cpp \
    html/DateTimeInputType.cpp \
    html/DateTimeLocalInputType.cpp \
    html/EmailInputType.cpp \
    html/FTPDirectoryDocument.cpp \
    html/FileInputType.cpp \
    html/FormAssociatedElement.cpp \
    html/FormDataList.cpp \
    html/HTMLAllCollection.cpp \
    html/HTMLAnchorElement.cpp \
    html/HTMLAppletElement.cpp \
    html/HTMLAreaElement.cpp \
    html/HTMLBRElement.cpp \
    html/HTMLBaseElement.cpp \
    html/HTMLBaseFontElement.cpp \
    html/HTMLBodyElement.cpp \
    html/HTMLButtonElement.cpp \
    html/HTMLCanvasElement.cpp \
    html/HTMLCollection.cpp \
    html/HTMLDListElement.cpp \
    html/HTMLDataListElement.cpp \
    html/HTMLDirectoryElement.cpp \
    html/HTMLDetailsElement.cpp \
    html/HTMLDivElement.cpp \
    html/HTMLDocument.cpp \
    html/HTMLElement.cpp \
    html/HTMLEmbedElement.cpp \
    html/HTMLFieldSetElement.cpp \
    html/HTMLFontElement.cpp \
    html/HTMLFormCollection.cpp \
    html/HTMLFormControlElement.cpp \
    html/HTMLFormControlElementWithState.cpp \
    html/HTMLFormElement.cpp \
    html/HTMLFrameElement.cpp \
    html/HTMLFrameElementBase.cpp \
    html/HTMLFrameOwnerElement.cpp \
    html/HTMLFrameSetElement.cpp \
    html/HTMLHRElement.cpp \
    html/HTMLHeadElement.cpp \
    html/HTMLHeadingElement.cpp \
    html/HTMLHtmlElement.cpp \
    html/HTMLIFrameElement.cpp \
    html/HTMLImageElement.cpp \
    html/HTMLImageLoader.cpp \
    html/HTMLInputElement.cpp \
    html/HTMLIsIndexElement.cpp \
    html/HTMLKeygenElement.cpp \
    html/HTMLLIElement.cpp \
    html/HTMLLabelElement.cpp \
    html/HTMLLegendElement.cpp \
    html/HTMLLinkElement.cpp \
    html/HTMLMapElement.cpp \
    html/HTMLMarqueeElement.cpp \
    html/HTMLMenuElement.cpp \
    html/HTMLMetaElement.cpp \
    html/HTMLMeterElement.cpp \
    html/HTMLModElement.cpp \
    html/HTMLNameCollection.cpp \
    html/HTMLOListElement.cpp \
    html/HTMLObjectElement.cpp \
    html/HTMLOptGroupElement.cpp \
    html/HTMLOptionElement.cpp \
    html/HTMLOptionsCollection.cpp \
    html/HTMLOutputElement.cpp \
    html/HTMLParagraphElement.cpp \
    html/HTMLParamElement.cpp \
    html/HTMLParserErrorCodes.cpp \
    html/HTMLPlugInElement.cpp \
    html/HTMLPlugInImageElement.cpp \
    html/HTMLPreElement.cpp \
    html/HTMLProgressElement.cpp \
    html/HTMLPropertiesCollection.cpp \
    html/HTMLQuoteElement.cpp \
    html/HTMLScriptElement.cpp \
    html/HTMLSelectElement.cpp \
    html/HTMLSpanElement.cpp \
    html/HTMLStyleElement.cpp \
    html/HTMLSummaryElement.cpp \
    html/HTMLTableCaptionElement.cpp \
    html/HTMLTableCellElement.cpp \
    html/HTMLTableColElement.cpp \
    html/HTMLTableElement.cpp \
    html/HTMLTablePartElement.cpp \
    html/HTMLTableRowElement.cpp \
    html/HTMLTableRowsCollection.cpp \
    html/HTMLTableSectionElement.cpp \
    html/HTMLTextAreaElement.cpp \
    html/HTMLTextFormControlElement.cpp \
    html/HTMLTitleElement.cpp \
    html/HTMLUListElement.cpp \
    html/HTMLViewSourceDocument.cpp \
    html/HiddenInputType.cpp \
    html/ImageData.cpp \
    html/ImageDocument.cpp \
    html/ImageInputType.cpp \
    html/InputType.cpp \
    html/IsIndexInputType.cpp \
    html/LabelsNodeList.cpp \
    html/LinkRelAttribute.cpp \
    html/MediaDocument.cpp \
    html/MicroDataItemValue.cpp \
    html/MonthInputType.cpp \
    html/NumberInputType.cpp \
    html/PasswordInputType.cpp \
    html/PluginDocument.cpp \
    html/RadioInputType.cpp \
    html/RangeInputType.cpp \
    html/ResetInputType.cpp \
    html/SearchInputType.cpp \
    html/StepRange.cpp \
    html/SubmitInputType.cpp \
    html/TelephoneInputType.cpp \
    html/TextDocument.cpp \
    html/TextFieldInputType.cpp \
    html/TextInputType.cpp \
    html/TimeInputType.cpp \
    html/URLInputType.cpp \
    html/ValidationMessage.cpp \
    html/ValidityState.cpp \
    html/WeekInputType.cpp \
    html/canvas/CanvasGradient.cpp \
    html/canvas/CanvasPattern.cpp \
    html/canvas/CanvasPixelArray.cpp \
    html/canvas/CanvasRenderingContext.cpp \
    html/canvas/CanvasRenderingContext2D.cpp \
    html/canvas/CanvasStyle.cpp \
    html/canvas/DataView.cpp \
    html/parser/CSSPreloadScanner.cpp \
    html/parser/HTMLConstructionSite.cpp \
    html/parser/HTMLDocumentParser.cpp \
    html/parser/HTMLElementStack.cpp \
    html/parser/HTMLEntityParser.cpp \
    html/parser/HTMLEntitySearch.cpp \
    html/parser/HTMLFormattingElementList.cpp \
    html/parser/HTMLMetaCharsetParser.cpp \
    html/parser/HTMLParserIdioms.cpp \
    html/parser/HTMLParserScheduler.cpp \
    html/parser/HTMLPreloadScanner.cpp \
    html/parser/HTMLScriptRunner.cpp \
    html/parser/HTMLSourceTracker.cpp \
    html/parser/HTMLTokenizer.cpp \
    html/parser/HTMLTreeBuilder.cpp \
    html/parser/HTMLViewSourceParser.cpp \
    html/parser/TextDocumentParser.cpp \
    html/parser/TextViewSourceParser.cpp \
    html/parser/XSSAuditor.cpp \
    html/shadow/ContentSelectorQuery.cpp \
    html/shadow/ContentInclusionSelector.cpp \
    html/shadow/DetailsMarkerControl.cpp \
    html/shadow/HTMLContentElement.cpp \
    html/shadow/MediaControls.cpp \
    html/shadow/MediaControlRootElement.cpp \
    html/shadow/MeterShadowElement.cpp \
    html/shadow/ProgressShadowElement.cpp \
    html/shadow/SliderThumbElement.cpp \
    html/shadow/TextControlInnerElements.cpp \
    inspector/ConsoleMessage.cpp \
    inspector/ContentSearchUtils.cpp \
    inspector/DOMEditor.cpp \
    inspector/DOMNodeHighlighter.cpp \
    inspector/IdentifiersFactory.cpp \
    inspector/InjectedScript.cpp \
    inspector/InjectedScriptHost.cpp \
    inspector/InjectedScriptManager.cpp \
    inspector/InspectorAgent.cpp \
    inspector/InspectorApplicationCacheAgent.cpp \
    inspector/InspectorBaseAgent.cpp \
    inspector/InspectorCSSAgent.cpp \
    inspector/InspectorClient.cpp \
    inspector/InspectorConsoleAgent.cpp \
    inspector/InspectorController.cpp \
    inspector/InspectorDatabaseAgent.cpp \
    inspector/InspectorDatabaseResource.cpp \
    inspector/InspectorDebuggerAgent.cpp \
    inspector/InspectorDOMAgent.cpp \
    inspector/InspectorDOMDebuggerAgent.cpp \
    inspector/InspectorDOMStorageAgent.cpp \
    inspector/InspectorDOMStorageResource.cpp \
    inspector/InspectorFrontendClientLocal.cpp \
    inspector/InspectorFrontendHost.cpp \
    inspector/InspectorInstrumentation.cpp \
    inspector/InspectorMemoryAgent.cpp \
    inspector/InspectorPageAgent.cpp \
    inspector/InspectorProfilerAgent.cpp \
    inspector/InspectorResourceAgent.cpp \
    inspector/InspectorRuntimeAgent.cpp \
    inspector/InspectorState.cpp \
    inspector/InspectorStyleSheet.cpp \
    inspector/InspectorStyleTextEditor.cpp \
    inspector/InspectorTimelineAgent.cpp \
    inspector/InspectorValues.cpp \
    inspector/InspectorWorkerAgent.cpp \
    inspector/InstrumentingAgents.cpp \
    inspector/NetworkResourcesData.cpp \
    inspector/PageConsoleAgent.cpp \
    inspector/PageDebuggerAgent.cpp \
    inspector/PageRuntimeAgent.cpp \
    inspector/ScriptArguments.cpp \
    inspector/ScriptCallFrame.cpp \
    inspector/ScriptCallStack.cpp \
    inspector/TimelineRecordFactory.cpp \
    inspector/WorkerConsoleAgent.cpp \
    inspector/WorkerDebuggerAgent.cpp \
    inspector/WorkerInspectorController.cpp \
    inspector/WorkerRuntimeAgent.cpp \
    loader/appcache/ApplicationCache.cpp \
    loader/appcache/ApplicationCacheGroup.cpp \
    loader/appcache/ApplicationCacheHost.cpp \
    loader/appcache/ApplicationCacheStorage.cpp \
    loader/appcache/ApplicationCacheResource.cpp \
    loader/appcache/DOMApplicationCache.cpp \
    loader/appcache/ManifestParser.cpp \
    loader/archive/ArchiveResource.cpp \
    loader/archive/ArchiveResourceCollection.cpp \
    loader/cache/MemoryCache.cpp \
    loader/cache/CachedCSSStyleSheet.cpp \
    loader/cache/CachedFont.cpp \
    loader/cache/CachedImage.cpp \
    loader/cache/CachedRawResource.cpp \
    loader/cache/CachedResourceHandle.cpp \
    loader/cache/CachedResource.cpp \
    loader/cache/CachedScript.cpp \
    loader/cache/CachedShader.cpp \
    loader/cache/CachedXSLStyleSheet.cpp \
    loader/CrossOriginAccessControl.cpp \
    loader/CrossOriginPreflightResultCache.cpp \
    loader/cache/CachedResourceLoader.cpp \
    loader/DocumentLoadTiming.cpp \
    loader/DocumentLoader.cpp \
    loader/DocumentThreadableLoader.cpp \
    loader/DocumentWriter.cpp \
    loader/FormState.cpp \
    loader/FormSubmission.cpp \
    loader/FrameLoader.cpp \
    loader/FrameLoaderStateMachine.cpp \
    loader/HistoryController.cpp \
    loader/FTPDirectoryParser.cpp \
    loader/icon/IconController.cpp \
    loader/icon/IconDatabaseBase.cpp \
    loader/icon/IconLoader.cpp \
    loader/ImageLoader.cpp \
    loader/LinkLoader.cpp \
    loader/MainResourceLoader.cpp \
    loader/NavigationAction.cpp \
    loader/NetscapePlugInStreamLoader.cpp \
    loader/PingLoader.cpp \
    loader/PlaceholderDocument.cpp \
    loader/PolicyCallback.cpp \
    loader/PolicyChecker.cpp \
    loader/ProgressTracker.cpp \
    loader/NavigationScheduler.cpp \
    loader/ResourceLoader.cpp \
    loader/ResourceLoadNotifier.cpp \
    loader/ResourceLoadScheduler.cpp \
    loader/SinkDocument.cpp \
    loader/SubframeLoader.cpp \
    loader/SubresourceLoader.cpp \
    loader/TextResourceDecoder.cpp \
    loader/ThreadableLoader.cpp \
    notifications/Notification.cpp \
    notifications/NotificationCenter.cpp \
    notifications/NotificationController.cpp \
    page/animation/AnimationBase.cpp \
    page/animation/AnimationController.cpp \
    page/animation/CompositeAnimation.cpp \
    page/animation/ImplicitAnimation.cpp \
    page/animation/KeyframeAnimation.cpp \
    page/WebKitAnimation.cpp \
    page/WebKitAnimationList.cpp \
    page/BarInfo.cpp \
    page/Chrome.cpp \
    page/Console.cpp \
    page/ContentSecurityPolicy.cpp \
    page/ContextMenuController.cpp \
    page/Crypto.cpp \
    page/DOMSelection.cpp \
    page/DOMTimer.cpp \
    page/DOMWindow.cpp \
    page/DOMWindowProperty.cpp \
    page/DragController.cpp \
    page/EventHandler.cpp \
    page/EventSource.cpp \
    page/FocusController.cpp \
    page/Frame.cpp \
    page/FrameActionScheduler.cpp \
    page/FrameDestructionObserver.cpp \
    page/FrameTree.cpp \
    page/FrameView.cpp \
    page/Geolocation.cpp \
    page/GeolocationController.cpp \
    page/GestureTapHighlighter.cpp \
    page/GroupSettings.cpp \
    page/History.cpp \
    page/Location.cpp \
    page/MemoryInfo.cpp \
    page/MouseEventWithHitTestResults.cpp \
    page/Navigator.cpp \
    page/NavigatorBase.cpp \
    page/OriginAccessEntry.cpp \
    page/Page.cpp \
    page/PageGroup.cpp \
    page/PageGroupLoadDeferrer.cpp \
    page/PageVisibilityState.cpp \
    page/Performance.cpp \
    page/PerformanceNavigation.cpp \
    page/PerformanceTiming.cpp \
    page/PrintContext.cpp \
    page/Screen.cpp \
    page/SecurityOrigin.cpp \
    page/SecurityPolicy.cpp \
    page/Settings.cpp \
    page/SpatialNavigation.cpp \
    page/SuspendableTimer.cpp \
    page/UserContentURLPattern.cpp \
    page/WindowFeatures.cpp \
    plugins/PluginData.cpp \
    plugins/DOMPluginArray.cpp \
    plugins/DOMPlugin.cpp \
    plugins/PluginMainThreadScheduler.cpp \
    plugins/DOMMimeType.cpp \
    plugins/DOMMimeTypeArray.cpp \
    platform/animation/Animation.cpp \
    platform/animation/AnimationList.cpp \
    platform/Arena.cpp \
    platform/text/Base64.cpp \
    platform/text/BidiContext.cpp \
    platform/text/Hyphenation.cpp \
    platform/text/LocaleToScriptMappingDefault.cpp \
    platform/text/LocalizedDateNone.cpp \
    platform/text/LocalizedNumberNone.cpp \
    platform/text/QuotedPrintable.cpp \
    platform/Clock.cpp \
    platform/ClockGeneric.cpp \
    platform/ContentType.cpp \
    platform/CrossThreadCopier.cpp \
    platform/DateComponents.cpp \
    platform/DefaultLocalizationStrategy.cpp \
    platform/DragData.cpp \
    platform/DragImage.cpp \
    platform/FileChooser.cpp \
    platform/FileIconLoader.cpp \
    platform/FileStream.cpp \
    platform/FileSystem.cpp \
    platform/GeolocationService.cpp \
    platform/HistogramSupport.cpp \
    platform/image-decoders/qt/ImageFrameQt.cpp \
    platform/graphics/FontDescription.cpp \
    platform/graphics/FontFallbackList.cpp \
    platform/graphics/FontFamily.cpp \
    platform/graphics/FontFeatureSettings.cpp \
    platform/graphics/BitmapImage.cpp \
    platform/graphics/Color.cpp \
    platform/graphics/CrossfadeGeneratedImage.cpp \
    platform/graphics/FloatPoint3D.cpp \
    platform/graphics/FloatPoint.cpp \
    platform/graphics/FloatQuad.cpp \
    platform/graphics/FloatRect.cpp \
    platform/graphics/FloatSize.cpp \
    platform/graphics/FontData.cpp \
    platform/graphics/Font.cpp \
    platform/graphics/FontCache.cpp \
    platform/graphics/GeneratorGeneratedImage.cpp \
    platform/graphics/Gradient.cpp \
    platform/graphics/GraphicsContext.cpp \
    platform/graphics/GraphicsLayer.cpp \
    platform/graphics/GraphicsTypes.cpp \
    platform/graphics/Image.cpp \
    platform/graphics/ImageBuffer.cpp \
    platform/graphics/ImageSource.cpp \
    platform/graphics/IntRect.cpp \
    platform/graphics/Path.cpp \
    platform/graphics/PathTraversalState.cpp \
    platform/graphics/Pattern.cpp \
    platform/graphics/Region.cpp \
    platform/graphics/RoundedRect.cpp \
    platform/graphics/SegmentedFontData.cpp \
    platform/graphics/ShadowBlur.cpp \
    platform/graphics/SVGGlyph.cpp \
    platform/graphics/SimpleFontData.cpp \
    platform/graphics/StringTruncator.cpp \
    platform/graphics/TextRun.cpp \
    platform/graphics/TiledBackingStore.cpp \
    platform/graphics/transforms/AffineTransform.cpp \
    platform/graphics/transforms/TransformationMatrix.cpp \
    platform/graphics/transforms/MatrixTransformOperation.cpp \
    platform/graphics/transforms/Matrix3DTransformOperation.cpp \
    platform/graphics/transforms/PerspectiveTransformOperation.cpp \
    platform/graphics/transforms/RotateTransformOperation.cpp \
    platform/graphics/transforms/ScaleTransformOperation.cpp \
    platform/graphics/transforms/SkewTransformOperation.cpp \
    platform/graphics/transforms/TransformOperations.cpp \
    platform/graphics/transforms/TransformState.cpp \
    platform/graphics/transforms/TranslateTransformOperation.cpp \
    platform/KillRingNone.cpp \
    platform/KURL.cpp \
    platform/Language.cpp \
    platform/Length.cpp \
    platform/text/LineEnding.cpp \
    platform/leveldb/LevelDBDatabase.cpp \
    platform/leveldb/LevelDBTransaction.cpp \
    platform/leveldb/LevelDBWriteBatch.cpp \
    platform/LinkHash.cpp \
    platform/Logging.cpp \
    platform/MemoryPressureHandler.cpp \
    platform/MIMETypeRegistry.cpp \
    platform/mock/DeviceOrientationClientMock.cpp \
    platform/mock/GeolocationClientMock.cpp \
    platform/mock/GeolocationServiceMock.cpp \
    platform/mock/SpeechInputClientMock.cpp \
    platform/mock/ScrollbarThemeMock.cpp \
    platform/network/AuthenticationChallengeBase.cpp \
    platform/network/BlobData.cpp \
    platform/network/BlobRegistryImpl.cpp \
    platform/network/BlobResourceHandle.cpp \
    platform/network/Credential.cpp \
    platform/network/CredentialStorage.cpp \
    platform/network/ContentTypeParser.cpp \
    platform/network/FormData.cpp \
    platform/network/FormDataBuilder.cpp \
    platform/network/HTTPHeaderMap.cpp \
    platform/network/HTTPParsers.cpp \
    platform/network/HTTPValidation.cpp \
    platform/network/MIMEHeader.cpp \
    platform/network/NetworkStateNotifier.cpp \
    platform/network/ProtectionSpace.cpp \
    platform/network/ProxyServer.cpp \
    platform/network/ResourceErrorBase.cpp \
    platform/network/ResourceHandle.cpp \
    platform/network/ResourceLoadTiming.cpp \
    platform/network/ResourceRequestBase.cpp \
    platform/network/ResourceResponseBase.cpp \
    platform/text/RegularExpression.cpp \
    platform/PlatformEvent.cpp \
    platform/RuntimeApplicationChecks.cpp \
    platform/RunLoop.cpp \
    platform/SchemeRegistry.cpp \
    platform/ScrollableArea.cpp \
    platform/ScrollAnimator.cpp \
    platform/Scrollbar.cpp \
    platform/ScrollbarTheme.cpp \
    platform/ScrollbarThemeComposite.cpp \
    platform/ScrollView.cpp \
    platform/SharedBuffer.cpp \
    platform/SharedBufferChunkReader.cpp \
    platform/sql/SQLiteAuthorizer.cpp \
    platform/sql/SQLiteDatabase.cpp \
    platform/sql/SQLiteFileSystem.cpp \
    platform/sql/SQLiteStatement.cpp \
    platform/sql/SQLiteTransaction.cpp \
    platform/sql/SQLValue.cpp \
    platform/text/SegmentedString.cpp \
    platform/text/String.cpp \
    platform/text/TextBoundaries.cpp \
    platform/text/TextCodec.cpp \
    platform/text/TextCodecLatin1.cpp \
    platform/text/TextCodecUserDefined.cpp \
    platform/text/TextCodecUTF16.cpp \
    platform/text/TextCodecUTF8.cpp \
    platform/text/TextCodecICU.cpp \
    platform/text/TextEncoding.cpp \
    platform/text/TextEncodingDetectorNone.cpp \
    platform/text/TextEncodingRegistry.cpp \
    platform/text/TextStream.cpp \
    platform/ThreadGlobalData.cpp \
    platform/ThreadTimers.cpp \
    platform/Timer.cpp \
    platform/text/UnicodeRange.cpp \
    platform/text/transcoder/FontTranscoder.cpp \
    platform/UUID.cpp \
    platform/Widget.cpp \
    platform/PlatformStrategies.cpp \
    platform/LocalizedStrings.cpp \
    plugins/IFrameShimSupport.cpp \
    plugins/PluginDatabase.cpp \
    plugins/PluginDebug.cpp \
    plugins/PluginPackage.cpp \
    plugins/PluginStream.cpp \
    plugins/PluginView.cpp \
    rendering/AutoTableLayout.cpp \
    rendering/break_lines.cpp \
    rendering/BidiRun.cpp \
    rendering/CounterNode.cpp \
    rendering/EllipsisBox.cpp \
    rendering/FilterEffectRenderer.cpp \
    rendering/FixedTableLayout.cpp \
    rendering/HitTestingTransformState.cpp \
    rendering/HitTestResult.cpp \
    rendering/InlineBox.cpp \
    rendering/InlineFlowBox.cpp \
    rendering/InlineTextBox.cpp \
    rendering/LayoutState.cpp \
    rendering/LayoutRepainter.cpp \
    rendering/RenderApplet.cpp \
    rendering/RenderArena.cpp \
    rendering/RenderBlock.cpp \
    rendering/RenderBlockLineLayout.cpp \
    rendering/RenderBox.cpp \
    rendering/RenderBoxModelObject.cpp \
    rendering/RenderBR.cpp \
    rendering/RenderButton.cpp \
    rendering/RenderCombineText.cpp \
    rendering/RenderCounter.cpp \
    rendering/RenderDeprecatedFlexibleBox.cpp \
    rendering/RenderDetails.cpp \
    rendering/RenderDetailsMarker.cpp \
    rendering/RenderEmbeddedObject.cpp \
    rendering/RenderFieldset.cpp \
    rendering/RenderFileUploadControl.cpp \
    rendering/RenderFlexibleBox.cpp \
    rendering/RenderFlowThread.cpp \
    rendering/RenderFrame.cpp \
    rendering/RenderFrameBase.cpp \
    rendering/RenderFrameSet.cpp \
    rendering/RenderHTMLCanvas.cpp \
    rendering/RenderIFrame.cpp \
    rendering/RenderImage.cpp \
    rendering/RenderImageResource.cpp \
    rendering/RenderImageResourceStyleImage.cpp \
    rendering/RenderInline.cpp \
    rendering/RenderLayer.cpp \
    rendering/RenderLayerBacking.cpp \
    rendering/RenderLayerCompositor.cpp \
    rendering/RenderLineBoxList.cpp \
    rendering/RenderListBox.cpp \
    rendering/RenderListItem.cpp \
    rendering/RenderListMarker.cpp \
    rendering/RenderMarquee.cpp \
    rendering/RenderMenuList.cpp \
    rendering/RenderMeter.cpp \
    rendering/RenderObject.cpp \
    rendering/RenderObjectChildList.cpp \
    rendering/RenderPart.cpp \
    rendering/RenderProgress.cpp \
    rendering/RenderQuote.cpp \
    rendering/RenderRegion.cpp \
    rendering/RenderReplaced.cpp \
    rendering/RenderReplica.cpp \
    rendering/RenderRuby.cpp \
    rendering/RenderRubyBase.cpp \
    rendering/RenderRubyRun.cpp \
    rendering/RenderRubyText.cpp \
    rendering/RenderScrollbar.cpp \
    rendering/RenderScrollbarPart.cpp \
    rendering/RenderScrollbarTheme.cpp \
    rendering/RenderSlider.cpp \
    rendering/RenderSummary.cpp \
    rendering/RenderTable.cpp \
    rendering/RenderTableCaption.cpp \
    rendering/RenderTableCell.cpp \
    rendering/RenderTableCol.cpp \
    rendering/RenderTableRow.cpp \
    rendering/RenderTableSection.cpp \
    rendering/RenderText.cpp \
    rendering/RenderTextControl.cpp \
    rendering/RenderTextControlMultiLine.cpp \
    rendering/RenderTextControlSingleLine.cpp \
    rendering/RenderTextFragment.cpp \
    rendering/RenderTheme.cpp \
    rendering/RenderTreeAsText.cpp \
    rendering/RenderView.cpp \
    rendering/RenderWidget.cpp \
    rendering/RenderWordBreak.cpp \
    rendering/RootInlineBox.cpp \
    rendering/ScrollBehavior.cpp \
    rendering/style/ContentData.cpp \
    rendering/style/CounterDirectives.cpp \
    rendering/style/FillLayer.cpp \
    rendering/style/KeyframeList.cpp \
    rendering/style/NinePieceImage.cpp \
    rendering/style/QuotesData.cpp \
    rendering/style/RenderStyle.cpp \
    rendering/style/ShadowData.cpp \
    rendering/style/StyleBackgroundData.cpp \
    rendering/style/StyleBoxData.cpp \
    rendering/style/StyleCachedImage.cpp \
    rendering/style/StyleCachedShader.cpp \
    rendering/style/StyleDeprecatedFlexibleBoxData.cpp \
    rendering/style/StyleFilterData.cpp \
    rendering/style/StyleFlexibleBoxData.cpp \
    rendering/style/StyleGeneratedImage.cpp \
    rendering/style/StyleGridData.cpp \
    rendering/style/StyleInheritedData.cpp \
    rendering/style/StyleMarqueeData.cpp \
    rendering/style/StyleMultiColData.cpp \
    rendering/style/StyleRareInheritedData.cpp \
    rendering/style/StyleRareNonInheritedData.cpp \
    rendering/style/StyleSurroundData.cpp \
    rendering/style/StyleTransformData.cpp \
    rendering/style/StyleVisualData.cpp \
    storage/AbstractDatabase.cpp \
    storage/DOMWindowSQLDatabase.cpp \
    storage/Database.cpp \
    storage/DatabaseAuthorizer.cpp \
    storage/DatabaseSync.cpp \
    storage/LocalStorageTask.cpp \
    storage/LocalStorageThread.cpp \
    storage/Storage.cpp \
    storage/StorageAreaImpl.cpp \
    storage/StorageAreaSync.cpp \
    storage/StorageEvent.cpp \
    storage/StorageEventDispatcher.cpp \
    storage/StorageMap.cpp \
    storage/StorageNamespace.cpp \
    storage/StorageNamespaceImpl.cpp \
    storage/StorageSyncManager.cpp \
    storage/StorageTracker.cpp \
    testing/Internals.cpp \
    testing/InternalSettings.cpp \
    xml/DOMParser.cpp \
    xml/NativeXPathNSResolver.cpp \
    xml/XMLHttpRequest.cpp \
    xml/XMLHttpRequestException.cpp \
    xml/XMLHttpRequestProgressEventThrottle.cpp \
    xml/XMLHttpRequestUpload.cpp \
    xml/XMLErrors.cpp \
    xml/XMLSerializer.cpp \
    xml/XPathEvaluator.cpp \
    xml/XPathException.cpp \
    xml/XPathExpression.cpp \
    xml/XPathExpressionNode.cpp \
    xml/XPathFunctions.cpp \
    xml/XPathNodeSet.cpp \
    xml/XPathNSResolver.cpp \
    xml/XPathParser.cpp \
    xml/XPathPath.cpp \
    xml/XPathPredicate.cpp \
    xml/XPathResult.cpp \
    xml/XPathStep.cpp \
    xml/XPathUtil.cpp \
    xml/XPathValue.cpp \
    xml/XPathVariableReference.cpp \
    xml/parser/NewXMLDocumentParser.cpp \
    xml/parser/XMLCharacterReferenceParser.cpp \
    xml/parser/XMLDocumentParser.cpp \
    xml/parser/XMLTokenizer.cpp \
    xml/parser/XMLTreeBuilder.cpp

HEADERS += \
    accessibility/AccessibilityARIAGridCell.h \
    accessibility/AccessibilityARIAGrid.h \
    accessibility/AccessibilityARIAGridRow.h \
    accessibility/AccessibilityImageMapLink.h \
    accessibility/AccessibilityListBox.h \
    accessibility/AccessibilityListBoxOption.h \
    accessibility/AccessibilityList.h \
    accessibility/AccessibilityMediaControls.h \
    accessibility/AccessibilityObject.h \
    accessibility/AccessibilityProgressIndicator.h \
    accessibility/AccessibilityRenderObject.h \
    accessibility/AccessibilityScrollbar.h \
    accessibility/AccessibilityScrollView.h \
    accessibility/AccessibilitySlider.h \
    accessibility/AccessibilitySpinButton.h \
    accessibility/AccessibilityTableCell.h \
    accessibility/AccessibilityTableColumn.h \
    accessibility/AccessibilityTable.h \
    accessibility/AccessibilityTableHeaderContainer.h \
    accessibility/AccessibilityTableRow.h \
    accessibility/AXObjectCache.h \
    bindings/ScriptControllerBase.h \
    bindings/generic/ActiveDOMCallback.h \
    bindings/generic/RuntimeEnabledFeatures.h

v8 {
    HEADERS += \
        bindings/v8/custom/V8CustomVoidCallback.h \
        bindings/v8/custom/V8CustomXPathNSResolver.h \
        bindings/v8/custom/V8HTMLImageElementConstructor.h \
        bindings/v8/custom/V8HTMLSelectElementCustom.h \
        bindings/v8/custom/V8NamedNodesCollection.h \
        \
        bindings/v8/DateExtension.h \
        bindings/v8/DOMData.h \
        bindings/v8/DOMDataStore.h \
        bindings/v8/DOMWrapperWorld.h \
        bindings/v8/IsolatedWorld.h \
        bindings/v8/npruntime_impl.h \
        bindings/v8/npruntime_priv.h \
        bindings/v8/NPV8Object.h \
        bindings/v8/NPObjectWrapper.h \
        bindings/v8/OwnHandle.h \
        bindings/v8/PageScriptDebugServer.h \
        bindings/v8/RetainedDOMInfo.h \
        bindings/v8/RetainedObjectInfo.h \
        bindings/v8/ScheduledAction.h \
        bindings/v8/ScopedDOMDataStore.h \
        bindings/v8/ScriptCachedFrameData.h \
        bindings/v8/ScriptController.h \
        bindings/v8/ScriptEventListener.h \
        bindings/v8/ScriptFunctionCall.h \
        bindings/v8/ScriptInstance.h \
        bindings/v8/ScriptObject.h \
        bindings/v8/ScriptProfile.h \
        bindings/v8/ScriptProfiler.h \
        bindings/v8/ScriptScope.h \
        bindings/v8/ScriptSourceCode.h \
        bindings/v8/ScriptState.h \
        bindings/v8/ScriptValue.h \
        bindings/v8/ScriptWrappable.h \
        bindings/v8/SerializedScriptValue.h \
        bindings/v8/SharedPersistent.h \
        bindings/v8/StaticDOMDataStore.h \
        bindings/v8/V8AbstractEventListener.h \
        bindings/v8/V8Binding.h \
        bindings/v8/V8Collection.h \
        bindings/v8/V8DOMMap.h \
        bindings/v8/V8DOMWindowShell.h \
        bindings/v8/V8DOMWrapper.h \
        bindings/v8/V8EventListener.h \
        bindings/v8/V8EventListenerList.h \
        bindings/v8/V8GCController.h \
        bindings/v8/V8Helpers.h \
        bindings/v8/V8HiddenPropertyName.h \
        bindings/v8/V8IsolatedContext.h \
        bindings/v8/V8LazyEventListener.h \
        bindings/v8/V8NodeFilterCondition.h \
        bindings/v8/V8NPObject.h \
        bindings/v8/V8NPUtils.h \
        bindings/v8/V8Proxy.h \
        bindings/v8/V8RecursionScope.h \
        bindings/v8/V8Utilities.h \
        bindings/v8/V8WindowErrorHandler.h \
        bindings/v8/V8WorkerContextEventListener.h \
        bindings/v8/WorkerContextExecutionProxy.h \
        bindings/v8/WorkerScriptDebugServer.h \
        bindings/v8/WorkerScriptController.h \
        bindings/v8/WorldContextHandle.h
} else {
    HEADERS += \
        bindings/js/CachedScriptSourceProvider.h \
        bindings/js/CallbackFunction.h \
        bindings/js/GCController.h \
        bindings/js/DOMObjectHashTableMap.h \
        bindings/js/DOMWrapperWorld.h \
        bindings/js/JSArrayBufferViewHelper.h \
        bindings/js/JSCSSStyleDeclarationCustom.h \
        bindings/js/JSCallbackData.h \
        bindings/js/JSCustomVoidCallback.h \
        bindings/js/JSCustomXPathNSResolver.h \
        bindings/js/JSDictionary.h \
        bindings/js/JSDOMBinding.h \
        bindings/js/JSDOMGlobalObject.h \
        bindings/js/JSDOMStringMapCustom.h \
        bindings/js/JSDOMWindowBase.h \
        bindings/js/JSDOMWindowCustom.h \
        bindings/js/JSDOMWindowShell.h \
        bindings/js/JSDOMWrapper.h \
        bindings/js/JSErrorHandler.h \
        bindings/js/JSEventListener.h \
        bindings/js/JSEventTarget.h \
        bindings/js/JSHTMLAppletElementCustom.h \
        bindings/js/JSHTMLEmbedElementCustom.h \
        bindings/js/JSHTMLInputElementCustom.h \
        bindings/js/JSHTMLObjectElementCustom.h \
        bindings/js/JSHTMLSelectElementCustom.h \
        bindings/js/JSHistoryCustom.h \
        bindings/js/JSImageConstructor.h \
        bindings/js/JSLazyEventListener.h \
        bindings/js/JSLocationCustom.h \
        bindings/js/JSNodeCustom.h \
        bindings/js/JSNodeFilterCondition.h \
        bindings/js/JSPluginElementFunctions.h \
        bindings/js/JSStorageCustom.h \
        bindings/js/JSWorkerContextBase.h \
        bindings/js/JavaScriptCallFrame.h \
        bindings/js/PageScriptDebugServer.h \
        bindings/js/ScheduledAction.h \
        bindings/js/ScriptCachedFrameData.h \
        bindings/js/ScriptController.h \
        bindings/js/ScriptDebugServer.h \
        bindings/js/ScriptEventListener.h \
        bindings/js/ScriptFunctionCall.h \
        bindings/js/ScriptGCEvent.h \
        bindings/js/ScriptHeapSnapshot.h \
        bindings/js/ScriptObject.h \
        bindings/js/ScriptProfile.h \
        bindings/js/ScriptProfileNode.h \
        bindings/js/ScriptProfiler.h \
        bindings/js/ScriptSourceCode.h \
        bindings/js/ScriptSourceProvider.h \
        bindings/js/ScriptState.h \
        bindings/js/ScriptValue.h \
        bindings/js/ScriptWrappable.h \
        bindings/js/SerializedScriptValue.h \
        bindings/js/StringSourceProvider.h \
        bindings/js/WebCoreJSClientData.h \
        bindings/js/WorkerScriptController.h \
        bindings/js/WorkerScriptDebugServer.h \
        bridge/Bridge.h \
        bridge/c/CRuntimeObject.h \
        bridge/c/c_class.h \
        bridge/c/c_instance.h \
        bridge/c/c_runtime.h \
        bridge/c/c_utility.h \
        bridge/jsc/BridgeJSC.h \
        bridge/IdentifierRep.h \
        bridge/NP_jsobject.h \
        bridge/qt/qt_class.h \
        bridge/qt/qt_instance.h \
        bridge/qt/qt_runtime.h \
        bridge/qt/qt_pixmapruntime.h \
        bridge/runtime_array.h \
        bridge/runtime_method.h \
        bridge/runtime_object.h \
        bridge/runtime_root.h \
        plugins/npruntime.h
}

HEADERS += \
    css/CSSAspectRatioValue.h \
    css/CSSBorderImageSliceValue.h \
    css/CSSBorderImage.h \
    css/CSSCanvasValue.h \
    css/CSSCharsetRule.h \
    css/CSSComputedStyleDeclaration.h \
    css/CSSCrossfadeValue.h \
    css/CSSCursorImageValue.h \
    css/CSSFlexValue.h \
    css/CSSFontFace.h \
    css/CSSFontFaceRule.h \
    css/CSSFontFaceSource.h \
    css/CSSFontFaceSrcValue.h \
    css/CSSFontSelector.h \
    css/CSSFunctionValue.h \
    css/CSSGradientValue.h \
    css/CSSHelper.h \
    css/CSSImageGeneratorValue.h \
    css/CSSImageValue.h \
    css/CSSImportRule.h \
    css/CSSInheritedValue.h \
    css/CSSInitialValue.h \
    css/CSSMediaRule.h \
    css/CSSMutableStyleDeclaration.h \
    css/CSSOMUtils.h \
    css/CSSPageRule.h \
    css/CSSParser.h \
    css/CSSParserValues.h \
    css/CSSPrimitiveValue.h \
    css/CSSProperty.h \
    css/CSSPropertyLonghand.h \
    css/CSSReflectValue.h \
    css/CSSRule.h \
    css/CSSRuleList.h \
    css/CSSSegmentedFontFace.h \
    css/CSSSelector.h \
    css/CSSSelectorList.h \
    css/CSSStyleApplyProperty.h \
    css/CSSStyleDeclaration.h \
    css/CSSStyleRule.h \
    css/CSSStyleSelector.h \
    css/CSSStyleSheet.h \
    css/CSSTimingFunctionValue.h \
    css/CSSUnicodeRangeValue.h \
    css/CSSValue.cpp \
    css/CSSValue.h \
    css/CSSValueList.h \
    css/CSSValuePool.h \
    css/CSSWrapShapes.h \
    css/FontFamilyValue.h \
    css/FontFeatureValue.h \
    css/FontValue.h \
    css/MediaFeatureNames.h \
    css/MediaList.h \
    css/MediaQuery.h \
    css/MediaQueryEvaluator.h \
    css/MediaQueryExp.h \
    css/MediaQueryList.h \
    css/MediaQueryListListener.h \
    css/MediaQueryMatcher.h \
    css/RGBColor.h \
    css/SelectorChecker.h \
    css/ShadowValue.h \
    css/StyleMedia.h \
    css/StyleSheet.h \
    css/StyleSheetList.h \
    css/WebKitCSSFilterValue.h \
    css/WebKitCSSKeyframeRule.h \
    css/WebKitCSSKeyframesRule.h \
    css/WebKitCSSMatrix.h \
    css/WebKitCSSRegionRule.h \
    css/WebKitCSSShaderValue.h \
    css/WebKitCSSTransformValue.h \
    dom/ActiveDOMObject.h \
    dom/Attr.h \
    dom/Attribute.h \
    dom/BeforeTextInsertedEvent.h \
    dom/BeforeUnloadEvent.h \
    dom/CDATASection.h \
    dom/CharacterData.h \
    dom/CheckedRadioButtons.h \
    dom/ChildNodeList.h \
    dom/ClassNodeList.h \
    dom/ClientRect.h \
    dom/ClientRectList.h \
    dom/ClipboardEvent.h \
    dom/Clipboard.h \
    dom/Comment.h \
    dom/ContainerNode.h \
    dom/CSSMappedAttributeDeclaration.h \
    dom/CustomEvent.h \
    dom/default/PlatformMessagePortChannel.h \
    dom/DeviceMotionClient.h \
    dom/DeviceMotionController.h \
    dom/DeviceMotionData.h \
    dom/DeviceMotionEvent.h \
    dom/DeviceOrientation.h \
    dom/DeviceOrientationClient.h \
    dom/DeviceOrientationController.h \
    dom/DeviceOrientationEvent.h \
    dom/Document.h \
    dom/DocumentFragment.h \
    dom/DocumentMarker.h \
    dom/DocumentMarkerController.h \
    dom/DocumentOrderedMap.h \
    dom/DocumentType.h \
    dom/DOMImplementation.h \
    dom/DOMStringList.h \
    dom/DOMStringMap.h \
    dom/DOMTimeStamp.h \
    dom/DatasetDOMStringMap.h \
    dom/DynamicNodeList.h \
    dom/EditingText.h \
    dom/Element.h \
    dom/Entity.h \
    dom/EntityReference.h \
    dom/Event.h \
    dom/EventDispatchMediator.h \
    dom/EventListenerMap.h \
    dom/EventNames.h \
    dom/EventQueue.h \
    dom/EventTarget.h \
    dom/ExceptionBase.h \
    dom/ExceptionCode.h \
    dom/FragmentScriptingPermission.h \
    dom/KeyboardEvent.h \
    dom/MessageChannel.h \
    dom/MessageEvent.h \
    dom/MessagePortChannel.h \
    dom/MessagePort.h \
    dom/MicroDataItemList.h \
    dom/MouseEvent.h \
    dom/MouseRelatedEvent.h \
    dom/MutationCallback.h \
    dom/MutationEvent.h \
    dom/MutationObserverRegistration.h \
    dom/MutationRecord.h \
    dom/NamedNodeMap.h \
    dom/NameNodeList.h \
    dom/NodeFilterCondition.h \
    dom/NodeFilter.h \
    dom/Node.h \
    dom/NodeIterator.h \
    dom/NodeRenderingContext.h \
    dom/Notation.h \
    dom/StaticHashSetNodeList.h \
    dom/OverflowEvent.h \
    dom/PageTransitionEvent.h \
    dom/Position.h \
    dom/PositionIterator.h \
    dom/ProcessingInstruction.h \
    dom/ProgressEvent.h \
    dom/QualifiedName.h \
    dom/Range.h \
    dom/RegisteredEventListener.h \
    dom/RenderedDocumentMarker.h \
    dom/ScriptedAnimationController.h \
    dom/ScriptElement.h \
    dom/ScriptExecutionContext.h \
    dom/SelectorQuery.h \
    dom/ShadowRoot.h \
    dom/SpaceSplitString.h \
    dom/StaticNodeList.h \
    dom/StyledElement.h \
    dom/StyleElement.h \
    dom/TagNodeList.h \
    dom/TextEvent.h \
    dom/TextEventInputType.h \
    dom/Text.h \
    dom/Touch.h \
    dom/TouchEvent.h \
    dom/TouchList.h \
    dom/TransformSource.h \
    dom/Traversal.h \
    dom/TreeDepthLimit.h \
    dom/TreeScope.h \
    dom/TreeScopeAdopter.h \
    dom/TreeWalker.h \
    dom/UIEvent.h \
    dom/UIEventWithKeyState.h \
    dom/UserGestureIndicator.h \
    dom/ViewportArguments.h \
    dom/WebKitAnimationEvent.h \
    dom/WebKitMutationObserver.h \
    dom/WebKitNamedFlow.h \
    dom/WebKitTransitionEvent.h \
    dom/WheelEvent.h \
    editing/AppendNodeCommand.h \
    editing/ApplyBlockElementCommand.h \
    editing/ApplyStyleCommand.h \
    editing/BreakBlockquoteCommand.h \
    editing/CompositeEditCommand.h \
    editing/CreateLinkCommand.h \
    editing/DeleteButtonController.h \
    editing/DeleteButton.h \
    editing/DeleteFromTextNodeCommand.h \
    editing/DeleteSelectionCommand.h \
    editing/EditCommand.h \
    editing/EditingStyle.h \
    editing/EditingBehavior.h \
    editing/EditingBoundary.h \
    editing/Editor.h \
    editing/FindOptions.h \
    editing/FormatBlockCommand.h \
    editing/FrameSelection.h \
    editing/htmlediting.h \
    editing/HTMLInterchange.h \
    editing/IndentOutdentCommand.h \
    editing/InsertIntoTextNodeCommand.h \
    editing/InsertLineBreakCommand.h \
    editing/InsertListCommand.h \
    editing/InsertNodeBeforeCommand.h \
    editing/InsertParagraphSeparatorCommand.h \
    editing/InsertTextCommand.h \
    editing/markup.h \
    editing/MergeIdenticalElementsCommand.h \
    editing/ModifySelectionListLevel.h \
    editing/MoveSelectionCommand.h \
    editing/RemoveCSSPropertyCommand.h \
    editing/RemoveFormatCommand.h \
    editing/RemoveNodeCommand.h \
    editing/RemoveNodePreservingChildrenCommand.h \
    editing/RenderedPosition.h \
    editing/ReplaceNodeWithSpanCommand.h \
    editing/ReplaceSelectionCommand.h \
    editing/SetNodeAttributeCommand.h \
    editing/SmartReplace.h \
    editing/SpellingCorrectionCommand.h \
    editing/SpellingCorrectionController.h \
    editing/SplitElementCommand.h \
    editing/SplitTextNodeCommand.h \
    editing/SplitTextNodeContainingElementCommand.h \
    editing/TextIterator.h \
    editing/TypingCommand.h \
    editing/UndoStep.h \
    editing/UnlinkCommand.h \
    editing/VisiblePosition.h \
    editing/VisibleSelection.h \
    editing/visible_units.h \
    editing/WrapContentsInDummySpanCommand.h \
    fileapi/Blob.h \
    fileapi/BlobURL.h \
    fileapi/File.h \
    fileapi/FileError.h \
    fileapi/FileException.h \
    fileapi/FileList.h \
    fileapi/FileReader.h \
    fileapi/FileReaderLoader.h \
    fileapi/FileReaderLoaderClient.h \
    fileapi/FileReaderSync.h \
    fileapi/FileStreamProxy.h \
    fileapi/FileThread.h \
    fileapi/FileThreadTask.h \
    fileapi/OperationNotAllowedException.h \
    fileapi/WebKitBlobBuilder.h \
    history/BackForwardController.h \
    history/BackForwardListImpl.h \
    history/BackForwardList.h \
    history/CachedFrame.h \
    history/CachedPage.h \
    history/HistoryItem.h \
    history/PageCache.h \
    html/canvas/CanvasGradient.h \
    html/canvas/CanvasPattern.h \
    html/canvas/CanvasPixelArray.h \
    html/canvas/CanvasRenderingContext.h \
    html/canvas/CanvasRenderingContext2D.h \
    html/canvas/CanvasStyle.h \
    html/canvas/DataView.h \
    html/ClassList.h \
    html/DOMFormData.h \
    html/DOMSettableTokenList.h \
    html/DOMTokenList.h \
    html/DOMURL.h \
    html/FormAssociatedElement.h \
    html/FormDataList.h \
    html/FTPDirectoryDocument.h \
    html/HTMLAllCollection.h \
    html/HTMLAnchorElement.h \
    html/HTMLAppletElement.h \
    html/HTMLAreaElement.h \
    html/HTMLAudioElement.h \
    html/HTMLBaseElement.h \
    html/HTMLBaseFontElement.h \
    html/HTMLBodyElement.h \
    html/HTMLBRElement.h \
    html/HTMLButtonElement.h \
    html/HTMLCanvasElement.h \
    html/HTMLCollection.h \
    html/HTMLDirectoryElement.h \
    html/HTMLDetailsElement.h \
    html/HTMLDivElement.h \
    html/HTMLDListElement.h \
    html/HTMLDocument.h \
    html/HTMLElement.h \
    html/HTMLEmbedElement.h \
    html/HTMLFieldSetElement.h \
    html/HTMLFontElement.h \
    html/HTMLFormCollection.h \
    html/HTMLFormControlElement.h \
    html/HTMLFormControlElementWithState.h \
    html/HTMLFormElement.h \
    html/HTMLFrameElementBase.h \
    html/HTMLFrameElement.h \
    html/HTMLFrameOwnerElement.h \
    html/HTMLFrameSetElement.h \
    html/HTMLHeadElement.h \
    html/HTMLHeadingElement.h \
    html/HTMLHRElement.h \
    html/HTMLHtmlElement.h \
    html/HTMLIFrameElement.h \
    html/HTMLImageElement.h \
    html/HTMLImageLoader.h \
    html/HTMLInputElement.h \
    html/HTMLIsIndexElement.h \
    html/HTMLKeygenElement.h \
    html/HTMLLabelElement.h \
    html/HTMLLegendElement.h \
    html/HTMLLIElement.h \
    html/HTMLLinkElement.h \
    html/HTMLMapElement.h \
    html/HTMLMarqueeElement.h \
    html/HTMLMediaElement.h \
    html/HTMLMenuElement.h \
    html/HTMLMetaElement.h \
    html/HTMLMeterElement.h \
    html/HTMLModElement.h \
    html/HTMLNameCollection.h \
    html/HTMLObjectElement.h \
    html/HTMLOListElement.h \
    html/HTMLOptGroupElement.h \
    html/HTMLOptionElement.h \
    html/HTMLOptionsCollection.h \
    html/HTMLOutputElement.h \
    html/HTMLParagraphElement.h \
    html/HTMLParamElement.h \
    html/HTMLParserErrorCodes.h \
    html/HTMLPlugInElement.h \
    html/HTMLPlugInImageElement.h \
    html/HTMLPreElement.h \
    html/HTMLProgressElement.h \
    html/HTMLPropertiesCollection.h \
    html/HTMLQuoteElement.h \
    html/HTMLScriptElement.h \
    html/HTMLSelectElement.h \
    html/HTMLSourceElement.h \
    html/HTMLSpanElement.h \
    html/HTMLStyleElement.h \
    html/HTMLSummaryElement.h \
    html/HTMLTableCaptionElement.h \
    html/HTMLTableCellElement.h \
    html/HTMLTableColElement.h \
    html/HTMLTableElement.h \
    html/HTMLTablePartElement.h \
    html/HTMLTableRowElement.h \
    html/HTMLTableRowsCollection.h \
    html/HTMLTableSectionElement.h \
    html/HTMLTextAreaElement.h \
    html/HTMLTextFormControlElement.h \
    html/HTMLTitleElement.h \
    html/HTMLTrackElement.h \
    html/HTMLUListElement.h \
    html/HTMLVideoElement.h \
    html/HTMLViewSourceDocument.h \
    html/ImageData.h \
    html/ImageDocument.h \
    html/LabelsNodeList.h \
    html/LinkRelAttribute.h \
    html/LoadableTextTrack.h \
    html/MediaController.h \
    html/MediaDocument.h \
    html/MediaFragmentURIParser.h \
    html/MicroDataItemValue.h \
    html/PluginDocument.h \
    html/StepRange.h \
    html/TextDocument.h \
    html/TextTrack.h \
    html/TextTrackCue.h \
    html/TextTrackCueList.h \
    html/TimeRanges.h \
    html/ValidityState.h \
    html/parser/CSSPreloadScanner.h \
    html/parser/HTMLConstructionSite.h \
    html/parser/HTMLDocumentParser.h \
    html/parser/HTMLElementStack.h \
    html/parser/HTMLEntityParser.h \
    html/parser/HTMLEntitySearch.h \
    html/parser/HTMLEntityTable.h \
    html/parser/HTMLFormattingElementList.h \
    html/parser/HTMLParserScheduler.h \
    html/parser/HTMLPreloadScanner.h \
    html/parser/HTMLScriptRunner.h \
    html/parser/HTMLScriptRunnerHost.h \
    html/parser/HTMLToken.h \
    html/parser/HTMLTokenizer.h \
    html/parser/HTMLTreeBuilder.h \
    html/parser/HTMLViewSourceParser.h \
    html/parser/XSSAuditor.h \
    html/shadow/ContentSelectorQuery.h \
    html/shadow/ContentInclusionSelector.h \
    html/shadow/HTMLContentElement.h \
    html/shadow/MediaControlElements.h \
    html/shadow/DetailsMarkerControl.h \
    html/track/TextTrackList.h \
    html/track/TrackBase.h \
    html/track/TrackEvent.h \
    html/track/WebVTTParser.h \
    html/track/WebVTTToken.h \
    html/track/WebVTTTokenizer.h \
    inspector/ConsoleMessage.h \
    inspector/ContentSearchUtils.h \
    inspector/DOMEditor.h \
    inspector/DOMNodeHighlighter.h \
    inspector/DOMWrapperVisitor.h \
    inspector/IdentifiersFactory.h \
    inspector/InjectedScript.h \
    inspector/InjectedScriptHost.h \
    inspector/InjectedScriptManager.h \
    inspector/InspectorAgent.h \
    inspector/InspectorApplicationCacheAgent.h \
    inspector/InspectorBaseAgent.h \
    inspector/InspectorConsoleAgent.h \
    inspector/InspectorConsoleInstrumentation.h \
    inspector/InspectorController.h \
    inspector/InspectorCSSAgent.h \
    inspector/InspectorDatabaseAgent.h \
    inspector/InspectorDatabaseInstrumentation.h \
    inspector/InspectorDatabaseResource.h \
    inspector/InspectorDebuggerAgent.h \
    inspector/InspectorDOMDebuggerAgent.h \
    inspector/InspectorDOMStorageAgent.h \
    inspector/InspectorDOMStorageResource.h \
    inspector/InspectorFrontendChannel.h \
    inspector/InspectorFrontendClient.h \
    inspector/InspectorFrontendClientLocal.h \
    inspector/InspectorFrontendHost.h \
    inspector/InspectorInstrumentation.h \
    inspector/InspectorMemoryAgent.h \
    inspector/InspectorPageAgent.h \
    inspector/InspectorProfilerAgent.h \
    inspector/InspectorResourceAgent.h \
    inspector/InspectorRuntimeAgent.h \
    inspector/InspectorState.h \
    inspector/InspectorStyleSheet.h \
    inspector/InspectorStyleTextEditor.h \
    inspector/InspectorTimelineAgent.h \
    inspector/InspectorWorkerAgent.h \
    inspector/InstrumentingAgents.h \
    inspector/NetworkResourcesData.h \
    inspector/PageConsoleAgent.h \
    inspector/PageDebuggerAgent.h \
    inspector/PageRuntimeAgent.h \
    inspector/ScriptGCEventListener.h \
    inspector/TimelineRecordFactory.h \
    inspector/WorkerConsoleAgent.h \
    inspector/WorkerDebuggerAgent.h \
    inspector/WorkerRuntimeAgent.h \
    loader/appcache/ApplicationCacheGroup.h \
    loader/appcache/ApplicationCacheHost.h \
    loader/appcache/ApplicationCache.h \
    loader/appcache/ApplicationCacheResource.h \
    loader/appcache/ApplicationCacheStorage.h \
    loader/appcache/DOMApplicationCache.h \
    loader/appcache/ManifestParser.h \
    loader/archive/ArchiveResourceCollection.h \
    loader/archive/ArchiveResource.h \
    loader/cache/CachedCSSStyleSheet.h \
    loader/cache/CachedFont.h \
    loader/cache/CachedImage.h \
    loader/cache/CachedResourceClientWalker.h \
    loader/cache/CachedResource.h \
    loader/cache/CachedResourceHandle.h \
    loader/cache/CachedScript.h \
    loader/cache/CachedShader.h \
    loader/cache/CachedXSLStyleSheet.h \
    loader/cache/MemoryCache.h \
    loader/CrossOriginAccessControl.h \
    loader/CrossOriginPreflightResultCache.h \
    loader/cache/CachedResourceLoader.h \
    loader/DocumentLoader.h \
    loader/DocumentThreadableLoader.h \
    loader/FormState.h \
    loader/FrameLoader.h \
    loader/FrameLoaderStateMachine.h \
    loader/FTPDirectoryParser.h \
    loader/icon/IconController.h \
    loader/icon/IconDatabase.h \
    loader/icon/IconDatabaseBase.h \
    loader/icon/IconLoader.h \
    loader/icon/IconRecord.h \
    loader/icon/PageURLRecord.h \
    loader/ImageLoader.h \
    loader/LinkLoader.h \
    loader/LinkLoaderClient.h \
    loader/MainResourceLoader.h \
    loader/NavigationAction.h \
    loader/NetscapePlugInStreamLoader.h \
    loader/PlaceholderDocument.h \
    loader/ProgressTracker.h \
    loader/ResourceLoader.h \
    loader/SubresourceLoader.h \
    loader/TextResourceDecoder.h \
    loader/TextTrackLoader.h \
    loader/ThreadableLoader.h \
    loader/WorkerThreadableLoader.h \
    mathml/MathMLElement.h \
    mathml/MathMLInlineContainerElement.h \
    mathml/MathMLMathElement.h \
    mathml/MathMLTextElement.h \
    notifications/Notification.h \
    notifications/NotificationCenter.h \
    notifications/NotificationPresenter.h \
    notifications/NotificationContents.h \
    notifications/NotificationController.h \
    page/animation/AnimationBase.h \
    page/animation/AnimationController.h \
    page/animation/CompositeAnimation.h \
    page/animation/ImplicitAnimation.h \
    page/animation/KeyframeAnimation.h \
    page/AdjustViewSizeOrNot.h \
    page/BarInfo.h \
    page/Chrome.h \
    page/Console.h \
    page/ConsoleTypes.h \
    page/ContextMenuController.h \
    page/ContextMenuProvider.h \
    page/Coordinates.h \
    page/DOMSelection.h \
    page/DOMTimer.h \
    page/DOMWindow.h \
    page/DragController.h \
    page/DragState.h \
    page/EventHandler.h \
    page/EventSource.h \
    page/EditorClient.h \
    page/FocusController.h \
    page/Frame.h \
    page/FrameTree.h \
    page/FrameView.h \
    page/Geolocation.h \
    page/Geoposition.h \
    page/GestureTapHighlighter.h\
    page/GroupSettings.h \
    page/History.h \
    page/Location.h \
    page/MouseEventWithHitTestResults.h \
    page/NavigatorBase.h \
    page/Navigator.h \
    page/PageGroup.h \
    page/PageGroupLoadDeferrer.h \
    page/Page.h \
    page/PageVisibilityState.h \
    page/PrintContext.h \
    page/Screen.h \
    page/SecurityOrigin.h \
    page/Settings.h \
    page/SpatialNavigation.h \
    page/SpeechInput.h \
    page/SpeechInputClient.h \
    page/SpeechInputEvent.h \
    page/SpeechInputListener.h \
    page/SpeechInputResult.h \
    page/SpeechInputResultList.h \
    page/WebKitAnimation.h \
    page/WebKitAnimationList.h \
    page/WindowFeatures.h \
    page/WorkerNavigator.h \
    platform/animation/Animation.h \
    platform/animation/AnimationList.h \
    platform/animation/AnimationUtilities.h \
    platform/Arena.h \
    platform/AsyncFileStream.h \
    platform/Clock.h \
    platform/ClockGeneric.h \
    platform/ContentType.h \
    platform/ContextMenu.h \
    platform/CrossThreadCopier.h \
    platform/DateComponents.h \
    platform/DefaultLocalizationStrategy.h \
    platform/DragData.h \
    platform/DragImage.h \
    platform/FileChooser.h \
    platform/FileStream.h \
    platform/FileStreamClient.h \
    platform/FileSystem.h \
    platform/GeolocationService.h \
    platform/HistogramSupport.h \
    platform/image-decoders/ImageDecoder.h \
    platform/mock/DeviceOrientationClientMock.h \
    platform/mock/GeolocationClientMock.cpp \
    platform/mock/GeolocationServiceMock.h \
    platform/mock/SpeechInputClientMock.h \
    platform/mock/ScrollbarThemeMock.h \
    platform/graphics/BitmapImage.h \
    platform/graphics/Color.h \
    platform/graphics/CrossfadeGeneratedImage.h \
    platform/graphics/filters/CustomFilterMesh.h \
    platform/graphics/filters/CustomFilterNumberParameter.h \
    platform/graphics/filters/CustomFilterShader.h \
    platform/graphics/filters/CustomFilterOperation.h \
    platform/graphics/filters/CustomFilterParameter.h \
    platform/graphics/filters/CustomFilterProgram.h \
    platform/graphics/filters/FEBlend.h \
    platform/graphics/filters/FEColorMatrix.h \
    platform/graphics/filters/FEComponentTransfer.h \
    platform/graphics/filters/FEComposite.h \
    platform/graphics/filters/FEConvolveMatrix.h \
    platform/graphics/filters/FECustomFilter.h \
    platform/graphics/filters/FEDiffuseLighting.h \
    platform/graphics/filters/FEDisplacementMap.h \
    platform/graphics/filters/FEDropShadow.h \
    platform/graphics/filters/FEFlood.h \
    platform/graphics/filters/FEGaussianBlur.h \
    platform/graphics/filters/FELighting.h \
    platform/graphics/filters/FEMerge.h \
    platform/graphics/filters/FEMorphology.h \
    platform/graphics/filters/FEOffset.h \
    platform/graphics/filters/FESpecularLighting.h \
    platform/graphics/filters/FETile.h \
    platform/graphics/filters/FETurbulence.h \
    platform/graphics/filters/FilterEffect.h \
    platform/graphics/filters/FilterOperation.h \
    platform/graphics/filters/FilterOperations.h \
    platform/graphics/filters/LightSource.h \
    platform/graphics/filters/SourceAlpha.h \
    platform/graphics/filters/SourceGraphic.h \
    platform/graphics/filters/arm/FECompositeArithmeticNEON.h \
    platform/graphics/filters/arm/FEGaussianBlurNEON.h \
    platform/graphics/filters/arm/FELightingNEON.h \
    platform/graphics/FloatPoint3D.h \
    platform/graphics/FloatPoint.h \
    platform/graphics/FloatQuad.h \
    platform/graphics/FloatRect.h \
    platform/graphics/FloatSize.h \
    platform/graphics/FontData.h \
    platform/graphics/FontDescription.h \
    platform/graphics/FontFamily.h \
    platform/graphics/FontFeatureSettings.h \
    platform/graphics/FontMetrics.h \
    platform/graphics/Font.h \
    platform/graphics/GeneratorGeneratedImage.h \
    platform/graphics/GeneratedImage.h \
    platform/graphics/GlyphPageTreeNode.h \
    platform/graphics/Gradient.h \
    platform/graphics/GraphicsContext.h \
    platform/graphics/GraphicsLayer.h \
    platform/graphics/GraphicsLayerClient.h \
    platform/graphics/GraphicsTypes.h \
    platform/graphics/GraphicsTypes3D.h \
    platform/graphics/Image.h \
    platform/graphics/ImageSource.h \
    platform/graphics/IntPoint.h \
    platform/graphics/IntPointHash.h \
    platform/graphics/IntRect.h \
    platform/graphics/MediaPlayer.h \
    platform/graphics/Path.h \
    platform/graphics/PathTraversalState.h \
    platform/graphics/Pattern.h \
    platform/graphics/Region.h \
    platform/graphics/RoundedRect.h \
    platform/graphics/qt/FontCustomPlatformData.h \
    platform/graphics/qt/ImageDecoderQt.h \
    platform/graphics/qt/StillImageQt.h \
    platform/graphics/qt/TransparencyLayer.h \
    platform/graphics/SegmentedFontData.h \
    platform/graphics/ShadowBlur.h \
    platform/graphics/SimpleFontData.h \
    platform/graphics/Tile.h \
    platform/graphics/TiledBackingStore.h \
    platform/graphics/TiledBackingStoreClient.h \
    platform/graphics/transforms/Matrix3DTransformOperation.h \
    platform/graphics/transforms/MatrixTransformOperation.h \
    platform/graphics/transforms/PerspectiveTransformOperation.h \
    platform/graphics/transforms/RotateTransformOperation.h \
    platform/graphics/transforms/ScaleTransformOperation.h \
    platform/graphics/transforms/SkewTransformOperation.h \
    platform/graphics/transforms/TransformationMatrix.h \
    platform/graphics/transforms/TransformOperations.h \
    platform/graphics/transforms/TransformState.h \
    platform/graphics/transforms/TranslateTransformOperation.h \
    platform/KillRing.h \
    platform/KURL.h \
    platform/Length.h \
    platform/leveldb/LevelDBComparator.h \
    platform/leveldb/LevelDBDatabase.h \
    platform/leveldb/LevelDBIterator.h \
    platform/leveldb/LevelDBSlice.h \
    platform/leveldb/LevelDBTransaction.h \
    platform/leveldb/LevelDBWriteBatch.h \
    platform/text/BidiRunList.h \
    platform/text/LineEnding.h \
    platform/text/LocaleToScriptMapping.h \
    platform/text/TextCheckerClient.h \
    platform/text/TextChecking.h \
    platform/text/UnicodeBidi.h \
    platform/LinkHash.h \
    platform/Logging.h \
    platform/Language.h \
    platform/MemoryPressureHandler.h \
    platform/MIMETypeRegistry.h \
    platform/network/AuthenticationChallengeBase.h \
    platform/network/AuthenticationClient.h \
    platform/network/BlobData.h \
    platform/network/BlobRegistry.h \
    platform/network/BlobRegistryImpl.h \
    platform/network/BlobResourceHandle.h \
    platform/network/BlobStorageData.h \
    platform/network/CookieStorage.h \
    platform/network/Credential.h \
    platform/network/CredentialStorage.h \
    platform/network/ContentTypeParser.h \
    platform/network/FormDataBuilder.h \
    platform/network/FormData.h \
    platform/network/HTTPHeaderMap.h \
    platform/network/HTTPParsers.h \
    platform/network/HTTPValidation.h \
    platform/network/HTTPStatusCodes.h \
    platform/network/MIMESniffing.h \
    platform/network/NetworkingContext.h \
    platform/network/NetworkStateNotifier.h \
    platform/network/ProtectionSpace.h \
    platform/network/ProxyServer.h \
    platform/network/qt/QtMIMETypeSniffer.h \
    platform/network/qt/QNetworkReplyHandler.h \
    platform/network/ResourceErrorBase.h \
    platform/network/ResourceHandle.h \
    platform/network/ResourceLoadInfo.h \
    platform/network/ResourceLoadPriority.h \
    platform/network/ResourceLoadTiming.h \
    platform/network/ResourceRequestBase.h \
    platform/network/ResourceResponseBase.h \
    platform/network/qt/DnsPrefetchHelper.h \
    platform/network/qt/NetworkStateNotifierPrivate.h \
    platform/PlatformTouchEvent.h \
    platform/PlatformTouchPoint.h \
    platform/PopupMenu.h \
    platform/qt/ClipboardQt.h \
    platform/qt/CookieJarQt.h \
    platform/qt/QWebPageClient.h \
    platform/qt/RenderThemeQt.h \
    platform/qt/RenderThemeQtMobile.h \
    platform/ScrollableArea.h \
    platform/ScrollAnimator.h \
    platform/Scrollbar.h \
    platform/ScrollbarThemeComposite.h \
    platform/ScrollView.h \
    platform/SearchPopupMenu.h \
    platform/SharedBuffer.h \
    platform/SharedBufferChunkReader.h \
    platform/sql/SQLiteDatabase.h \
    platform/sql/SQLiteFileSystem.h \
    platform/sql/SQLiteStatement.h \
    platform/sql/SQLiteTransaction.h \
    platform/sql/SQLValue.h \
    platform/text/Base64.h \
    platform/text/BidiContext.h \
    platform/text/DecodeEscapeSequences.h \
    platform/text/Hyphenation.h \
    platform/text/QuotedPrintable.h \
    platform/text/qt/TextCodecQt.h \
    platform/text/RegularExpression.h \
    platform/text/SegmentedString.h \
    platform/text/TextBoundaries.h \
    platform/text/TextCodec.h \
    platform/text/TextCodecASCIIFastPath.h \
    platform/text/TextCodecLatin1.h \
    platform/text/TextCodecUserDefined.h \
    platform/text/TextCodecUTF16.h \
    platform/text/TextCodecUTF8.h \
    platform/text/TextEncoding.h \
    platform/text/TextEncodingRegistry.h \
    platform/text/TextStream.h \
    platform/text/UnicodeRange.h \
    platform/text/transcoder/FontTranscoder.h \
    platform/ThreadGlobalData.h \
    platform/ThreadTimers.h \
    platform/Timer.h \
    platform/Widget.h \
    platform/PlatformStrategies.h \
    platform/LocalizedStrings.h \
    plugins/DOMMimeTypeArray.h \
    plugins/DOMMimeType.h \
    plugins/DOMPluginArray.h \
    plugins/PluginDatabase.h \
    plugins/PluginData.h \
    plugins/PluginDebug.h \
    plugins/DOMPlugin.h \
    plugins/IFrameShimSupport.h \
    plugins/PluginMainThreadScheduler.h \
    plugins/PluginPackage.h \
    plugins/PluginStream.h \
    plugins/PluginView.h \
    plugins/win/PluginMessageThrottlerWin.h \
    rendering/AutoTableLayout.h \
    rendering/break_lines.h \
    rendering/CounterNode.h \
    rendering/EllipsisBox.h \
    rendering/FilterEffectRenderer.h \
    rendering/FixedTableLayout.h \
    rendering/HitTestingTransformState.h \
    rendering/HitTestResult.h \
    rendering/InlineBox.h \
    rendering/InlineFlowBox.h \
    rendering/InlineTextBox.h \
    rendering/LayoutState.h \
    rendering/LayoutTypes.h \
    rendering/LayoutRepainter.h \
    rendering/mathml/RenderMathMLBlock.h \
    rendering/mathml/RenderMathMLFenced.h \
    rendering/mathml/RenderMathMLFraction.h \
    rendering/mathml/RenderMathMLMath.h \
    rendering/mathml/RenderMathMLOperator.h \
    rendering/mathml/RenderMathMLRoot.h \
    rendering/mathml/RenderMathMLRow.h \
    rendering/mathml/RenderMathMLSquareRoot.h \
    rendering/mathml/RenderMathMLSubSup.h \
    rendering/mathml/RenderMathMLUnderOver.h \
    rendering/PaintInfo.h \
    rendering/PaintPhase.h \
    rendering/PointerEventsHitRules.h \
    rendering/RenderApplet.h \
    rendering/RenderArena.h \
    rendering/RenderBlock.h \
    rendering/RenderBox.h \
    rendering/RenderBoxModelObject.h \
    rendering/RenderBR.h \
    rendering/RenderButton.h \
    rendering/RenderCombineText.h \
    rendering/RenderCounter.h \
    rendering/RenderDeprecatedFlexibleBox.h \
    rendering/RenderDetails.h \
    rendering/RenderDetailsMarker.h \
    rendering/RenderEmbeddedObject.h \
    rendering/RenderFieldset.h \
    rendering/RenderFileUploadControl.h \
    rendering/RenderFlexibleBox.h \
    rendering/RenderFrame.h \
    rendering/RenderFrameBase.h \
    rendering/RenderFrameSet.h \
    rendering/RenderHTMLCanvas.h \
    rendering/RenderIFrame.h \
    rendering/RenderImageResource.h \
    rendering/RenderImageResourceStyleImage.h \
    rendering/RenderImage.h \
    rendering/RenderInline.h \
    rendering/RenderInputSpeech.h \
    rendering/RenderLayer.h \
    rendering/RenderLayerBacking.h \
    rendering/RenderLayerCompositor.h \
    rendering/RenderLineBoxList.h \
    rendering/RenderListBox.h \
    rendering/RenderListItem.h \
    rendering/RenderListMarker.h \
    rendering/RenderMarquee.h \
    rendering/RenderMedia.h \
    rendering/RenderMenuList.h \
    rendering/RenderMeter.h \
    rendering/RenderObjectChildList.h \
    rendering/RenderObject.h \
    rendering/RenderPart.h \
    rendering/RenderProgress.h \
    rendering/RenderQuote.h \
    rendering/RenderReplaced.h \
    rendering/RenderReplica.h \
    rendering/RenderRuby.h \
    rendering/RenderRubyBase.h \
    rendering/RenderRubyRun.h \
    rendering/RenderRubyText.h \
    rendering/RenderScrollbar.h \
    rendering/RenderScrollbarPart.h \
    rendering/RenderScrollbarTheme.h \
    rendering/RenderSlider.h \
    rendering/RenderSummary.h \
    rendering/RenderTableCaption.h \
    rendering/RenderTableCell.h \
    rendering/RenderTableCol.h \
    rendering/RenderTable.h \
    rendering/RenderTableRow.h \
    rendering/RenderTableSection.h \
    rendering/RenderTextControl.h \
    rendering/RenderTextControlMultiLine.h \
    rendering/RenderTextControlSingleLine.h \
    rendering/RenderTextFragment.h \
    rendering/RenderText.h \
    rendering/RenderTheme.h \
    rendering/RenderTreeAsText.h \
    rendering/RenderVideo.h \
    rendering/RenderView.h \
    rendering/RenderWidget.h \
    rendering/RenderWordBreak.h \
    rendering/RootInlineBox.h \
    rendering/ScrollBehavior.h \
    rendering/style/ContentData.h \
    rendering/style/CounterDirectives.h \
    rendering/style/CursorData.h \
    rendering/style/CursorList.h \
    rendering/style/FillLayer.h \
    rendering/style/KeyframeList.h \
    rendering/style/NinePieceImage.h \
    rendering/style/QuotesData.h \
    rendering/style/RenderStyle.h \
    rendering/style/ShadowData.h \
    rendering/style/StyleBackgroundData.h \
    rendering/style/StyleBoxData.h \
    rendering/style/StyleCachedImage.h \
    rendering/style/StyleCachedShader.h \
    rendering/style/StyleCustomFilterProgram.h \
    rendering/style/StyleDeprecatedFlexibleBoxData.h \
    rendering/style/StyleFilterData.h \
    rendering/style/StyleFlexibleBoxData.h \
    rendering/style/StyleGeneratedImage.h \
    rendering/style/StyleInheritedData.h \
    rendering/style/StyleMarqueeData.h \
    rendering/style/StyleMultiColData.h \
    rendering/style/StylePendingShader.h \
    rendering/style/StyleRareInheritedData.h \
    rendering/style/StyleRareNonInheritedData.h \
    rendering/style/StyleReflection.h \
    rendering/style/StyleShader.h \
    rendering/style/StyleSurroundData.h \
    rendering/style/StyleTransformData.h \
    rendering/style/StyleVisualData.h \
    rendering/style/SVGRenderStyleDefs.h \
    rendering/style/SVGRenderStyle.h \
    rendering/svg/RenderSVGBlock.h \
    rendering/svg/RenderSVGContainer.h \
    rendering/svg/RenderSVGForeignObject.h \
    rendering/svg/RenderSVGGradientStop.h \
    rendering/svg/RenderSVGHiddenContainer.h \
    rendering/svg/RenderSVGImage.h \
    rendering/svg/RenderSVGInline.h \
    rendering/svg/RenderSVGInlineText.h \
    rendering/svg/RenderSVGModelObject.h \
    rendering/svg/RenderSVGPath.h \
    rendering/svg/RenderSVGRect.h \
    rendering/svg/RenderSVGResource.h \
    rendering/svg/RenderSVGResourceClipper.h \
    rendering/svg/RenderSVGResourceContainer.h \
    rendering/svg/RenderSVGResourceFilter.h \
    rendering/svg/RenderSVGResourceFilterPrimitive.h \
    rendering/svg/RenderSVGResourceGradient.h \
    rendering/svg/RenderSVGResourceLinearGradient.h \
    rendering/svg/RenderSVGResourceMarker.h \
    rendering/svg/RenderSVGResourceMasker.h \
    rendering/svg/RenderSVGResourcePattern.h \
    rendering/svg/RenderSVGResourceRadialGradient.h \
    rendering/svg/RenderSVGResourceSolidColor.h \
    rendering/svg/RenderSVGRoot.h \
    rendering/svg/RenderSVGShadowTreeRootContainer.h \
    rendering/svg/RenderSVGShape.h \
    rendering/svg/RenderSVGTSpan.h \
    rendering/svg/RenderSVGText.h \
    rendering/svg/RenderSVGTextPath.h \
    rendering/svg/RenderSVGTransformableContainer.h \
    rendering/svg/RenderSVGViewportContainer.h \
    rendering/svg/SVGImageBufferTools.h \
    rendering/svg/SVGInlineFlowBox.h \
    rendering/svg/SVGInlineTextBox.h \
    rendering/svg/SVGMarkerData.h \
    rendering/svg/SVGMarkerLayoutInfo.h \
    rendering/svg/SVGPathData.h \
    rendering/svg/SVGRenderSupport.h \
    rendering/svg/SVGRenderTreeAsText.h \
    rendering/svg/SVGResources.h \
    rendering/svg/SVGResourcesCache.h \
    rendering/svg/SVGResourcesCycleSolver.h \
    rendering/svg/SVGRootInlineBox.h \
    rendering/svg/SVGShadowTreeElements.h \
    rendering/svg/SVGTextChunk.h \
    rendering/svg/SVGTextChunkBuilder.h \
    rendering/svg/SVGTextFragment.h \
    rendering/svg/SVGTextLayoutAttributes.h \
    rendering/svg/SVGTextLayoutAttributesBuilder.h \
    rendering/svg/SVGTextLayoutEngine.h \
    rendering/svg/SVGTextLayoutEngineBaseline.h \
    rendering/svg/SVGTextLayoutEngineSpacing.h \
    rendering/svg/SVGTextMetrics.h \
    rendering/svg/SVGTextMetricsBuilder.h \
    rendering/svg/SVGTextQuery.h \
    rendering/svg/SVGTextRunRenderingContext.h \
    storage/AbstractDatabase.h \
    storage/ChangeVersionWrapper.h \
    storage/DOMWindowSQLDatabase.h \
    storage/DatabaseAuthorizer.h \
    storage/Database.h \
    storage/DatabaseCallback.h \
    storage/DatabaseSync.h \
    storage/DatabaseTask.h \
    storage/DatabaseThread.h \
    storage/DatabaseTracker.h \
    storage/LocalStorageTask.h \
    storage/LocalStorageThread.h \
    storage/OriginQuotaManager.h \
    storage/OriginUsageRecord.h \
    storage/SQLCallbackWrapper.h \
    storage/SQLResultSet.h \
    storage/SQLResultSetRowList.h \
    storage/SQLStatement.h \
    storage/SQLStatementSync.h \
    storage/SQLTransaction.h \
    storage/SQLTransactionClient.h \
    storage/SQLTransactionCoordinator.h \
    storage/SQLTransactionSync.h \
    storage/SQLTransactionSyncCallback.h \
    storage/StorageArea.h \
    storage/StorageAreaImpl.h \
    storage/StorageAreaSync.h \
    storage/StorageEvent.h \
    storage/StorageEventDispatcher.h \
    storage/Storage.h \
    storage/StorageMap.h \
    storage/StorageNamespace.h \
    storage/StorageNamespaceImpl.h \
    storage/StorageSyncManager.h \
    storage/StorageTracker.h \
    storage/StorageTrackerClient.h \
    svg/animation/SMILTimeContainer.h \
    svg/animation/SMILTime.h \
    svg/animation/SVGSMILElement.h \
    svg/ColorDistance.h \
    svg/graphics/filters/SVGFEImage.h \
    svg/graphics/filters/SVGFilterBuilder.h \
    svg/graphics/filters/SVGFilter.h \
    svg/graphics/SVGImage.h \
    svg/graphics/SVGImageCache.h \
    svg/properties/SVGAttributeToPropertyMap.h \
    svg/properties/SVGAnimatedEnumerationPropertyTearOff.h \
    svg/properties/SVGAnimatedListPropertyTearOff.h \
    svg/properties/SVGAnimatedPathSegListPropertyTearOff.h \
    svg/properties/SVGAnimatedProperty.h \
    svg/properties/SVGAnimatedPropertyDescription.h \
    svg/properties/SVGAnimatedPropertyMacros.h \
    svg/properties/SVGAnimatedPropertySynchronizer.h \
    svg/properties/SVGAnimatedPropertyTearOff.h \
    svg/properties/SVGAnimatedStaticPropertyTearOff.h \
    svg/properties/SVGAnimatedTransformListPropertyTearOff.h \
    svg/properties/SVGListProperty.h \
    svg/properties/SVGListPropertyTearOff.h \
    svg/properties/SVGPathSegListPropertyTearOff.h \
    svg/properties/SVGProperty.h \
    svg/properties/SVGPropertyInfo.h \
    svg/properties/SVGPropertyTearOff.h \
    svg/properties/SVGPropertyTraits.h \
    svg/properties/SVGStaticListPropertyTearOff.h \
    svg/properties/SVGStaticPropertyTearOff.h \
    svg/properties/SVGStaticPropertyWithParentTearOff.h \
    svg/properties/SVGTransformListPropertyTearOff.h \
    svg/SVGAElement.h \
    svg/SVGAltGlyphDefElement.h \
    svg/SVGAltGlyphElement.h \
    svg/SVGAltGlyphItemElement.h \
    svg/SVGAngle.h \
    svg/SVGAnimateColorElement.h \
    svg/SVGAnimatedAngle.h \
    svg/SVGAnimatedBoolean.h \
    svg/SVGAnimatedColor.h \
    svg/SVGAnimatedEnumeration.h \
    svg/SVGAnimatedInteger.h \
    svg/SVGAnimatedLength.h \
    svg/SVGAnimatedLengthList.h \
    svg/SVGAnimatedNumber.h \
    svg/SVGAnimatedNumberList.h \
    svg/SVGAnimatedNumberOptionalNumber.h \
    svg/SVGAnimatedPath.h \
    svg/SVGAnimatedPreserveAspectRatio.h \
    svg/SVGAnimatedPointList.h \
    svg/SVGAnimatedRect.h \
    svg/SVGAnimatedString.h \
    svg/SVGAnimatedTransformList.h \
    svg/SVGAnimatedType.h \
    svg/SVGAnimatedTypeAnimator.h \
    svg/SVGAnimateElement.h \
    svg/SVGAnimateMotionElement.h \
    svg/SVGAnimateTransformElement.h \
    svg/SVGAnimationElement.h \
    svg/SVGAnimatorFactory.h \
    svg/SVGCircleElement.h \
    svg/SVGClipPathElement.h \
    svg/SVGColor.h \
    svg/SVGComponentTransferFunctionElement.h \
    svg/SVGCursorElement.h \
    svg/SVGDefsElement.h \
    svg/SVGDescElement.h \
    svg/SVGDocumentExtensions.h \
    svg/SVGDocument.h \
    svg/SVGElement.h \
    svg/SVGElementInstance.h \
    svg/SVGElementInstanceList.h \
    svg/SVGElementRareData.h \
    svg/SVGEllipseElement.h \
    svg/SVGExternalResourcesRequired.h \
    svg/SVGFEBlendElement.h \
    svg/SVGFEColorMatrixElement.h \
    svg/SVGFEComponentTransferElement.h \
    svg/SVGFECompositeElement.h \
    svg/SVGFEConvolveMatrixElement.h \
    svg/SVGFEDiffuseLightingElement.h \
    svg/SVGFEDisplacementMapElement.h \
    svg/SVGFEDistantLightElement.h \
    svg/SVGFEDropShadowElement.h \
    svg/SVGFEFloodElement.h \
    svg/SVGFEFuncAElement.h \
    svg/SVGFEFuncBElement.h \
    svg/SVGFEFuncGElement.h \
    svg/SVGFEFuncRElement.h \
    svg/SVGFEGaussianBlurElement.h \
    svg/SVGFEImageElement.h \
    svg/SVGFELightElement.h \
    svg/SVGFEMergeElement.h \
    svg/SVGFEMergeNodeElement.h \
    svg/SVGFEMorphologyElement.h \
    svg/SVGFEOffsetElement.h \
    svg/SVGFEPointLightElement.h \
    svg/SVGFESpecularLightingElement.h \
    svg/SVGFESpotLightElement.h \
    svg/SVGFETileElement.h \
    svg/SVGFETurbulenceElement.h \
    svg/SVGFilterElement.h \
    svg/SVGFilterPrimitiveStandardAttributes.h \
    svg/SVGFitToViewBox.h \
    svg/SVGFontData.h \
    svg/SVGFontElement.h \
    svg/SVGFontFaceElement.h \
    svg/SVGFontFaceFormatElement.h \
    svg/SVGFontFaceNameElement.h \
    svg/SVGFontFaceSrcElement.h \
    svg/SVGFontFaceUriElement.h \
    svg/SVGForeignObjectElement.h \
    svg/SVGGElement.h \
    svg/SVGGlyphElement.h \
    svg/SVGGlyphRefElement.h \
    svg/SVGGradientElement.h \
    svg/SVGHKernElement.h \
    svg/SVGImageElement.h \
    svg/SVGImageLoader.h \
    svg/SVGLangSpace.h \
    svg/SVGLength.h \
    svg/SVGLengthContext.h \
    svg/SVGLengthList.h \
    svg/SVGLinearGradientElement.h \
    svg/SVGLineElement.h \
    svg/SVGLocatable.h \
    svg/SVGMarkerElement.h \
    svg/SVGMaskElement.h \
    svg/SVGMatrix.h \
    svg/SVGMetadataElement.h \
    svg/SVGMissingGlyphElement.h \
    svg/SVGMPathElement.h \
    svg/SVGNumberList.h \
    svg/SVGPaint.h \
    svg/SVGParserUtilities.h \
    svg/SVGParsingError.h \
    svg/SVGPathBuilder.h \
    svg/SVGPathConsumer.h \
    svg/SVGPathElement.h \
    svg/SVGPathParser.h \
    svg/SVGPathSegArc.h \
    svg/SVGPathSegClosePath.h \
    svg/SVGPathSegCurvetoCubic.h \
    svg/SVGPathSegCurvetoCubicSmooth.h \
    svg/SVGPathSegCurvetoQuadratic.h \
    svg/SVGPathSegCurvetoQuadraticSmooth.h \
    svg/SVGPathSegLineto.h \
    svg/SVGPathSegLinetoHorizontal.h \
    svg/SVGPathSegLinetoVertical.h \
    svg/SVGPathSegList.h \
    svg/SVGPathSegListBuilder.h \
    svg/SVGPathSegMoveto.h \
    svg/SVGPatternElement.h \
    svg/SVGPointList.h \
    svg/SVGPolyElement.h \
    svg/SVGPolygonElement.h \
    svg/SVGPolylineElement.h \
    svg/SVGPreserveAspectRatio.h \
    svg/SVGRadialGradientElement.h \
    svg/SVGRect.h \
    svg/SVGRectElement.h \
    svg/SVGScriptElement.h \
    svg/SVGSetElement.h \
    svg/SVGStopElement.h \
    svg/SVGStringList.h \
    svg/SVGStylable.h \
    svg/SVGStyledElement.h \
    svg/SVGStyledLocatableElement.h \
    svg/SVGStyledTransformableElement.h \
    svg/SVGStyleElement.h \
    svg/SVGSVGElement.h \
    svg/SVGSwitchElement.h \
    svg/SVGSymbolElement.h \
    svg/SVGTests.h \
    svg/SVGTextContentElement.h \
    svg/SVGTextElement.h \
    svg/SVGTextPathElement.h \
    svg/SVGTextPositioningElement.h \
    svg/SVGTitleElement.h \
    svg/SVGTransformable.h \
    svg/SVGTransformDistance.h \
    svg/SVGTransform.h \
    svg/SVGTransformList.h \
    svg/SVGTRefElement.h \
    svg/SVGTSpanElement.h \
    svg/SVGURIReference.h \
    svg/SVGUseElement.h \
    svg/SVGViewElement.h \
    svg/SVGViewSpec.h \
    svg/SVGVKernElement.h \
    svg/SVGZoomAndPan.h \
    svg/SVGZoomEvent.h \
    testing/Internals.h \
    testing/InternalSettings.h \
    workers/AbstractWorker.h \
    workers/DedicatedWorkerContext.h \
    workers/DedicatedWorkerThread.h \
    workers/SharedWorker.h \
    workers/WorkerContext.h \
    workers/WorkerEventQueue.h \
    workers/Worker.h \
    workers/WorkerLocation.h \
    workers/WorkerMessagingProxy.h \
    workers/WorkerRunLoop.h \
    workers/WorkerScriptLoader.h \
    workers/WorkerThread.h \
    xml/parser/CharacterReferenceParserInlineMethods.h \
    xml/parser/MarkupTokenBase.h \
    xml/parser/MarkupTokenizerBase.h \
    xml/parser/MarkupTokenizerInlineMethods.h \
    xml/parser/NewXMLDocumentParser.h \
    xml/parser/XMLCharacterReferenceParser.h \
    xml/parser/XMLDocumentParser.h \
    xml/parser/XMLTokenizer.h \
    xml/parser/XMLToken.h \
    xml/parser/XMLTreeBuilder.h \
    xml/DOMParser.h \
    xml/NativeXPathNSResolver.h \
    xml/XMLHttpRequest.h \
    xml/XMLHttpRequestUpload.h \
    xml/XMLErrors.h \
    xml/XMLSerializer.h \
    xml/XPathEvaluator.h \
    xml/XPathExpression.h \
    xml/XPathExpressionNode.h \
    xml/XPathFunctions.h \
    xml/XPathNodeSet.h \
    xml/XPathNSResolver.h \
    xml/XPathParser.h \
    xml/XPathPath.h \
    xml/XPathPredicate.h \
    xml/XPathResult.h \
    xml/XPathStep.h \
    xml/XPathUtil.h \
    xml/XPathValue.h \
    xml/XPathVariableReference.h \
    xml/XSLImportRule.h \
    xml/XSLStyleSheet.h \
    xml/XSLTExtensions.h \
    xml/XSLTProcessor.h \
    xml/XSLTUnicodeSort.h

SOURCES += \
    accessibility/qt/AccessibilityObjectQt.cpp \
    page/qt/DragControllerQt.cpp \
    page/qt/EventHandlerQt.cpp \
    platform/graphics/qt/TransformationMatrixQt.cpp \
    platform/graphics/qt/ColorQt.cpp \
    platform/graphics/qt/FontQt.cpp \
    platform/graphics/qt/FontPlatformDataQt.cpp \
    platform/graphics/qt/FloatPointQt.cpp \
    platform/graphics/qt/FloatRectQt.cpp \
    platform/graphics/qt/GradientQt.cpp \
    platform/graphics/qt/GraphicsContextQt.cpp \
    platform/graphics/qt/IconQt.cpp \
    platform/graphics/qt/ImageBufferQt.cpp \
    platform/graphics/qt/ImageDecoderQt.cpp \
    platform/graphics/qt/ImageQt.cpp \
    platform/graphics/qt/IntPointQt.cpp \
    platform/graphics/qt/IntRectQt.cpp \
    platform/graphics/qt/IntSizeQt.cpp \
    platform/graphics/qt/PathQt.cpp \
    platform/graphics/qt/PatternQt.cpp \
    platform/graphics/qt/StillImageQt.cpp \
    platform/network/MIMESniffing.cpp \
    platform/network/qt/CredentialStorageQt.cpp \
    platform/network/qt/ResourceHandleQt.cpp \
    platform/network/qt/ResourceRequestQt.cpp \
    platform/network/qt/DnsPrefetchHelper.cpp \
    platform/network/qt/NetworkStateNotifierQt.cpp \
    platform/network/qt/ProxyServerQt.cpp \
    platform/network/qt/QtMIMETypeSniffer.cpp \
    platform/network/qt/QNetworkReplyHandler.cpp \
    editing/qt/EditorQt.cpp \
    platform/Cursor.cpp \
    platform/qt/ClipboardQt.cpp \
    platform/qt/ContextMenuItemQt.cpp \
    platform/qt/ContextMenuQt.cpp \
    platform/qt/CookieJarQt.cpp \
    platform/qt/CursorQt.cpp \
    platform/qt/DragDataQt.cpp \
    platform/qt/DragImageQt.cpp \
    platform/qt/EventLoopQt.cpp \
    platform/qt/FileSystemQt.cpp \
    platform/qt/RunLoopQt.cpp \
    platform/qt/SharedBufferQt.cpp \
    platform/qt/ThirdPartyCookiesQt.cpp \
    platform/graphics/qt/FontCacheQt.cpp \
    platform/graphics/qt/FontCustomPlatformDataQt.cpp \
    platform/graphics/qt/GlyphPageTreeNodeQt.cpp \
    platform/graphics/qt/SimpleFontDataQt.cpp \
    platform/graphics/qt/TileQt.cpp \
    platform/qt/KURLQt.cpp \
    platform/qt/MIMETypeRegistryQt.cpp \
    platform/qt/PasteboardQt.cpp \
    platform/qt/PlatformKeyboardEventQt.cpp \
    platform/qt/PlatformMouseEventQt.cpp \
    platform/qt/PlatformScreenQt.cpp \
    platform/qt/PlatformTouchEventQt.cpp \
    platform/qt/PlatformTouchPointQt.cpp \
    platform/qt/RenderThemeQt.cpp \
    platform/qt/RenderThemeQtMobile.cpp \
    platform/qt/ScrollbarThemeQt.cpp \
    platform/qt/ScrollViewQt.cpp \
    platform/qt/SharedTimerQt.cpp \
    platform/qt/SoundQt.cpp \
    platform/qt/LoggingQt.cpp \
    platform/qt/LanguageQt.cpp \
    platform/qt/TemporaryLinkStubsQt.cpp \
    platform/text/qt/TextBoundariesQt.cpp \
    platform/text/qt/TextBreakIteratorInternalICUQt.cpp \
    platform/text/qt/TextCodecQt.cpp \
    platform/qt/WheelEventQt.cpp \
    platform/qt/WidgetQt.cpp

!contains(DEFINES, WTF_USE_LIBXML2=1) {
    SOURCES += xml/parser/XMLDocumentParserQt.cpp
}

contains(DEFINES, HAVE_QSTYLE=1) {
    HEADERS += platform/qt/QtStyleOptionWebComboBox.h \
               platform/qt/RenderThemeQStyle.h \
               platform/qt/ScrollbarThemeQt.h
    SOURCES += platform/qt/RenderThemeQStyle.cpp \
               platform/qt/ScrollbarQt.cpp
}

contains(DEFINES, ENABLE_SMOOTH_SCROLLING=1) {
    win32-*|wince* {
        HEADERS += platform/ScrollAnimatorWin.h
        SOURCES += platform/ScrollAnimatorWin.cpp
    }
}

win32-*|wince* {
    HEADERS += platform/win/SystemInfo.h
    SOURCES += \
        platform/win/SystemInfo.cpp \
        platform/win/SystemTimeWin.cpp \
        platform/graphics/win/TransformationMatrixWin.cpp
}

mac {
    SOURCES += \
        platform/text/cf/StringCF.cpp \
        platform/text/cf/StringImplCF.cpp
}

haveQt(5) {
    contains(QT_CONFIG,icu)|mac: SOURCES += platform/text/TextBreakIteratorICU.cpp
    mac {
        # For Mac we use the same SmartReplace implementation as the Apple port.
        SOURCES += editing/SmartReplaceCF.cpp
        INCLUDEPATH += $$PWD/icu
    } else {
        SOURCES += editing/SmartReplaceICU.cpp
    }
} else {
    SOURCES += platform/text/qt/TextBreakIteratorQt.cpp \
               editing/qt/SmartReplaceQt.cpp
}

contains(DEFINES, ENABLE_NETSCAPE_PLUGIN_API=1) {

    SOURCES += plugins/npapi.cpp

    unix {
        mac {
            SOURCES += \
                plugins/mac/PluginPackageMac.cpp
            OBJECTIVE_SOURCES += \
                platform/text/mac/StringImplMac.mm \
                platform/mac/WebCoreNSStringExtras.mm \
                plugins/mac/PluginViewMac.mm
        } else {
            SOURCES += \
                plugins/qt/PluginContainerQt.cpp \
                plugins/qt/PluginPackageQt.cpp \
                plugins/qt/PluginViewQt.cpp
            HEADERS += \
                plugins/qt/PluginContainerQt.h
        }
    }

    win32-* {
        INCLUDEPATH += $$PWD/plugins/win \
                       $$PWD/platform/win \
                       $$PWD/platform/graphics/win

        SOURCES += plugins/win/PluginDatabaseWin.cpp \
                   plugins/win/PluginPackageWin.cpp \
                   plugins/win/PluginMessageThrottlerWin.cpp \
                   plugins/win/PluginViewWin.cpp \
                   platform/win/BitmapInfo.cpp \
                   platform/win/WebCoreInstanceHandle.cpp
    }

} else {
    SOURCES += \
        plugins/PluginPackageNone.cpp \
        plugins/PluginViewNone.cpp
}

plugin_backend_xlib {
    SOURCES += plugins/qt/QtX11ImageConversion.cpp
    HEADERS += plugins/qt/QtX11ImageConversion.h
}

contains(DEFINES, ENABLE_SQL_DATABASE=1) {
    SOURCES += \
        storage/ChangeVersionWrapper.cpp \
        storage/DatabaseTask.cpp \
        storage/DatabaseThread.cpp \
        storage/DatabaseTracker.cpp \
        storage/OriginQuotaManager.cpp \
        storage/OriginUsageRecord.cpp \
        storage/SQLException.cpp \
        storage/SQLResultSet.cpp \
        storage/SQLResultSetRowList.cpp \
        storage/SQLStatement.cpp \
        storage/SQLStatementSync.cpp \
        storage/SQLTransaction.cpp \
        storage/SQLTransactionClient.cpp \
        storage/SQLTransactionCoordinator.cpp \
        storage/SQLTransactionSync.cpp

    !v8 {
        SOURCES += \
            bindings/js/JSCustomSQLStatementErrorCallback.cpp \
            bindings/js/JSSQLResultSetRowListCustom.cpp \
            bindings/js/JSSQLTransactionCustom.cpp \
            bindings/js/JSSQLTransactionSyncCustom.cpp
    }
}

contains(DEFINES, ENABLE_INDEXED_DATABASE=1) {
    !v8 {
        HEADERS += \
            bindings/js/IDBBindingUtilities.h \
    }

    HEADERS += \
        storage/IDBAny.h \
        storage/IDBCallbacks.h \
        storage/IDBCursor.h \
        storage/IDBCursorBackendImpl.h \
        storage/IDBCursorBackendInterface.h \
        storage/IDBDatabase.h \
        storage/IDBDatabaseBackendImpl.h \
        storage/IDBDatabaseBackendInterface.h \
        storage/IDBDatabaseError.h \
        storage/IDBDatabaseException.h \
        storage/IDBEventDispatcher.h \
        storage/IDBFactory.h \
        storage/IDBFactoryBackendInterface.h \
        storage/IDBFactoryBackendImpl.h \
        storage/IDBIndex.h \
        storage/IDBIndexBackendInterface.h \
        storage/IDBIndexBackendImpl.h \
        storage/IDBKey.h \
        storage/IDBKeyRange.h \
        storage/IDBObjectStore.h \
        storage/IDBObjectStoreBackendImpl.h \
        storage/IDBObjectStoreBackendInterface.h \
        storage/IDBRequest.h \
        storage/IDBTransaction.h \
        storage/IDBTransactionBackendInterface.h

    !v8 {
        SOURCES += \
            bindings/js/IDBBindingUtilities.cpp \
            bindings/js/JSIDBAnyCustom.cpp \
            bindings/js/JSIDBKeyCustom.cpp
    }

    SOURCES += \
        storage/IDBAny.cpp \
        storage/IDBCursor.cpp \
        storage/IDBCursorBackendImpl.cpp \
        storage/IDBDatabase.cpp \
        storage/IDBDatabaseBackendImpl.cpp \
        storage/IDBDatabaseException.cpp \
        storage/IDBEventDispatcher.cpp \
        storage/IDBFactory.cpp \
        storage/IDBFactoryBackendInterface.cpp \
        storage/IDBFactoryBackendImpl.cpp \
        storage/IDBIndex.cpp \
        storage/IDBIndexBackendImpl.cpp \
        storage/IDBKey.cpp \
        storage/IDBKeyRange.cpp \
        storage/IDBObjectStore.cpp \
        storage/IDBObjectStoreBackendImpl.cpp \
        storage/IDBRequest.cpp \
        storage/IDBTransaction.cpp
}

contains(DEFINES, ENABLE_DATA_TRANSFER_ITEMS=1) {
    HEADERS += \
        dom/DataTransferItem.h \
        dom/DataTransferItemList.h \
        dom/StringCallback.h \
        platform/qt/DataTransferItemQt.h \
        platform/qt/DataTransferItemListQt.h
    SOURCES += \
        dom/DataTransferItem.cpp \
        dom/StringCallback.cpp \
        platform/qt/DataTransferItemQt.cpp \
        platform/qt/DataTransferItemListQt.cpp
}

contains(DEFINES, ENABLE_FILE_SYSTEM=1) {
    HEADERS += \
        fileapi/AsyncFileWriter.h \
        fileapi/DirectoryEntry.h \
        fileapi/DirectoryEntrySync.h \
        fileapi/DirectoryReader.h \
        fileapi/DirectoryReaderBase.h \
        fileapi/DirectoryReaderSync.h \
        fileapi/DOMFilePath.h \
        fileapi/DOMFileSystem.h \
        fileapi/DOMFileSystemBase.h \
        fileapi/DOMFileSystemSync.h \
        fileapi/EntriesCallback.h \
        fileapi/Entry.h \
        fileapi/EntryArray.h \
        fileapi/EntryArraySync.h \
        fileapi/EntryBase.h \
        fileapi/EntryCallback.h \
        fileapi/EntrySync.h \
        fileapi/ErrorCallback.h \
        fileapi/FileCallback.h \
        fileapi/FileEntry.h \
        fileapi/FileEntrySync.h \
        fileapi/FileSystemCallback.h \
        fileapi/FileSystemCallbacks.h \
        fileapi/FileWriter.h \
        fileapi/FileWriterBase.h \
        fileapi/FileWriterBaseCallback.h \
        fileapi/FileWriterCallback.h \
        fileapi/FileWriterClient.h \
        fileapi/FileWriterSync.h \
        fileapi/WebKitFlags.h \
        fileapi/LocalFileSystem.h \
        fileapi/Metadata.h \
        fileapi/MetadataCallback.h \
        platform/AsyncFileSystem.h \
        platform/AsyncFileSystemCallbacks.h \
        platform/FileMetadata.h

    SOURCES += \
        bindings/js/JSDirectoryEntryCustom.cpp \
        bindings/js/JSDirectoryEntrySyncCustom.cpp \
        bindings/js/JSEntryCustom.cpp \
        bindings/js/JSEntrySyncCustom.cpp \
        platform/AsyncFileSystem.cpp
}

contains(DEFINES, ENABLE_ICONDATABASE=1) {
    SOURCES += \
        loader/icon/IconDatabase.cpp \
        loader/icon/IconRecord.cpp \
        loader/icon/PageURLRecord.cpp
}

contains(DEFINES, ENABLE_WORKERS=1) {
    !v8 {
        SOURCES += \
            bindings/js/JSDedicatedWorkerContextCustom.cpp \
            bindings/js/JSWorkerContextBase.cpp \
            bindings/js/JSWorkerContextCustom.cpp \
            bindings/js/JSWorkerCustom.cpp \
            bindings/js/WorkerScriptController.cpp \
            bindings/js/WorkerScriptDebugServer.cpp
    }
    SOURCES += \
        loader/WorkerThreadableLoader.cpp \
        page/WorkerNavigator.cpp \
        workers/AbstractWorker.cpp \
        workers/DedicatedWorkerContext.cpp \
        workers/DedicatedWorkerThread.cpp \
        workers/Worker.cpp \
        workers/WorkerContext.cpp \
        workers/WorkerEventQueue.cpp \
        workers/WorkerLocation.cpp \
        workers/WorkerMessagingProxy.cpp \
        workers/WorkerRunLoop.cpp \
        workers/WorkerThread.cpp \
        workers/WorkerScriptLoader.cpp
}

contains(DEFINES, ENABLE_SHARED_WORKERS=1) {
    !v8 {
        SOURCES += \
            bindings/js/JSSharedWorkerCustom.cpp
    }
    SOURCES += \
        workers/DefaultSharedWorkerRepository.cpp \
        workers/SharedWorker.cpp \
        workers/SharedWorkerContext.cpp \
        workers/SharedWorkerThread.cpp
}

contains(DEFINES, ENABLE_INPUT_SPEECH=1) {
    SOURCES += \
        page/SpeechInput.cpp \
        page/SpeechInputEvent.cpp \
        page/SpeechInputResult.cpp \
        page/SpeechInputResultList.cpp \
        rendering/RenderInputSpeech.cpp
}

contains(DEFINES, ENABLE_QUOTA=1) {
    HEADERS += \
        storage/StorageInfo.h \
        storage/StorageInfoErrorCallback.h \
        storage/StorageInfoQuotaCallback.h \
        storage/StorageInfoUsageCallback.h

    SOURCES += \
        storage/StorageInfo.cpp
}

contains(DEFINES, ENABLE_VIDEO=1) {
    SOURCES += \
        html/HTMLAudioElement.cpp \
        html/HTMLMediaElement.cpp \
        html/HTMLSourceElement.cpp \
        html/HTMLVideoElement.cpp \
        html/MediaController.cpp \
        html/MediaFragmentURIParser.cpp \
        html/shadow/MediaControlElements.cpp \
        html/TimeRanges.cpp \
        platform/graphics/MediaPlayer.cpp \
        rendering/RenderVideo.cpp \
        rendering/RenderMedia.cpp

    contains(DEFINES, WTF_USE_QTKIT=1) {
        INCLUDEPATH += \
            $$SOURCE_DIR/../WebKitLibraries/ \
            $$PWD/platform/mac


        HEADERS += \
            platform/graphics/mac/MediaPlayerPrivateQTKit.h \
            platform/mac/WebCoreObjCExtras.h \
            platform/qt/WebCoreSystemInterface.h \
            platform/mac/BlockExceptions.h \
            platform/mac/WebCoreObjCExtras.h \
            platform/mac/WebVideoFullscreenController.h \
            platform/mac/WebVideoFullscreenHUDWindowController.h \
            platform/mac/WebWindowAnimation.h

        SOURCES += \
            platform/mac/DisplaySleepDisabler.cpp \
            platform/graphics/cg/IntRectCG.cpp \
            platform/graphics/cg/FloatSizeCG.cpp \
            platform/cf/SharedBufferCF.cpp \
            platform/cf/KURLCFNet.cpp

         OBJECTIVE_SOURCES += \
            platform/qt/WebCoreSystemInterface.mm \
            platform/mac/BlockExceptions.mm \
            platform/mac/WebCoreObjCExtras.mm \
            platform/graphics/mac/MediaPlayerPrivateQTKit.mm \
            platform/mac/SharedBufferMac.mm \
            platform/mac/KURLMac.mm \
            platform/text/mac/StringMac.mm \
            platform/text/mac/StringImplMac.mm \
            platform/graphics/mac/FloatSizeMac.mm \
            platform/graphics/mac/IntRectMac.mm \
            platform/mac/WebVideoFullscreenController.mm \
            platform/mac/WebVideoFullscreenHUDWindowController.mm \
            platform/mac/WebWindowAnimation.mm

        DEFINES+=NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
        contains(CONFIG, "x86") {
            DEFINES+=NS_BUILD_32_LIKE_64
        }

        QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.5

    } else: contains(DEFINES, WTF_USE_GSTREAMER=1) {
        HEADERS += \
            platform/graphics/gstreamer/GRefPtrGStreamer.h \
            platform/graphics/gstreamer/GStreamerGWorld.h \
            platform/graphics/gstreamer/MediaPlayerPrivateGStreamer.h \
            platform/graphics/gstreamer/VideoSinkGStreamer.h \
            platform/graphics/gstreamer/WebKitWebSourceGStreamer.h \
            platform/graphics/gstreamer/PlatformVideoWindow.h \
            platform/graphics/gstreamer/PlatformVideoWindowPrivate.h \
            platform/graphics/gstreamer/ImageGStreamer.h
        SOURCES += \
            platform/graphics/gstreamer/GRefPtrGStreamer.cpp \
            platform/graphics/gstreamer/GStreamerGWorld.cpp \
            platform/graphics/gstreamer/MediaPlayerPrivateGStreamer.cpp \
            platform/graphics/gstreamer/VideoSinkGStreamer.cpp \
            platform/graphics/gstreamer/WebKitWebSourceGStreamer.cpp \
            platform/graphics/gstreamer/PlatformVideoWindowQt.cpp \
            platform/graphics/gstreamer/ImageGStreamerQt.cpp

    } else:contains(DEFINES, WTF_USE_QT_MULTIMEDIA=1) {
        HEADERS += \
            platform/graphics/qt/MediaPlayerPrivateQt.h

        SOURCES += \
            platform/graphics/qt/MediaPlayerPrivateQt.cpp
    }
}

contains(DEFINES, ENABLE_FULLSCREEN_API=1) {
    SOURCES += \
        rendering/RenderFullScreen.cpp
    HEADERS += \
        rendering/RenderFullScreen.h
}

contains(DEFINES, ENABLE_XSLT=1) {
    v8 {
        SOURCES += \
            bindings/v8/custom/V8XSLTProcessorCustom.cpp
    } else {
         SOURCES += \
            bindings/js/JSXSLTProcessorCustom.cpp
    }

    SOURCES += xml/XMLTreeViewer.cpp
    HEADERS += xml/XMLTreeViewer.h

    contains(DEFINES, WTF_USE_LIBXML2=1) {
        SOURCES += \
            xml/XSLTProcessor.cpp \
            xml/XSLTProcessorLibxslt.cpp \
            dom/TransformSourceLibxslt.cpp \
            xml/XSLStyleSheetLibxslt.cpp \
            xml/XSLImportRule.cpp \
            xml/XSLTExtensions.cpp \
            xml/XSLImportRule.cpp \
            xml/XSLTUnicodeSort.cpp \
            xml/parser/XMLDocumentParserLibxml2.cpp \
            xml/parser/XMLDocumentParserScope.cpp

            HEADERS += \
                xml/XSLImportRule.h \
                xml/XSLTExtensions.h \
                xml/XSLImportRule.h \
                xml/XSLTUnicodeSort.h \
                xml/parser/XMLDocumentParserScope.h

    } else {
        SOURCES += \
            dom/TransformSourceQt.cpp \
            xml/XSLStyleSheetQt.cpp \
            xml/XSLTProcessor.cpp \
            xml/XSLTProcessorQt.cpp
    }
}

contains(DEFINES, ENABLE_FILTERS=1) {
    SOURCES += \
        platform/graphics/filters/CustomFilterOperation.cpp \
        platform/graphics/filters/CustomFilterProgram.cpp \
        platform/graphics/filters/CustomFilterShader.cpp \
        platform/graphics/filters/CustomFilterMesh.cpp \
        platform/graphics/filters/DistantLightSource.cpp \
        platform/graphics/filters/FEBlend.cpp \
        platform/graphics/filters/FEColorMatrix.cpp \
        platform/graphics/filters/FEComponentTransfer.cpp \
        platform/graphics/filters/FEComposite.cpp \
        platform/graphics/filters/FEConvolveMatrix.cpp \
        platform/graphics/filters/FECustomFilter.cpp \
        platform/graphics/filters/FEDiffuseLighting.cpp \
        platform/graphics/filters/FEDisplacementMap.cpp \
        platform/graphics/filters/FEDropShadow.cpp \
        platform/graphics/filters/FEFlood.cpp \
        platform/graphics/filters/FEGaussianBlur.cpp \
        platform/graphics/filters/FELighting.cpp \
        platform/graphics/filters/FEMerge.cpp \
        platform/graphics/filters/FEMorphology.cpp \
        platform/graphics/filters/FEOffset.cpp \
        platform/graphics/filters/FESpecularLighting.cpp \
        platform/graphics/filters/FETile.cpp \
        platform/graphics/filters/FETurbulence.cpp \
        platform/graphics/filters/FilterOperations.cpp \
        platform/graphics/filters/FilterOperation.cpp \
        platform/graphics/filters/FilterEffect.cpp \
        platform/graphics/filters/LightSource.cpp \
        platform/graphics/filters/PointLightSource.cpp \
        platform/graphics/filters/SpotLightSource.cpp \
        platform/graphics/filters/SourceAlpha.cpp \
        platform/graphics/filters/SourceGraphic.cpp \
        platform/graphics/filters/arm/FECompositeArithmeticNEON.cpp \
        platform/graphics/filters/arm/FELightingNEON.cpp \
        platform/graphics/filters/arm/FEGaussianBlurNEON.cpp \
}

contains(DEFINES, ENABLE_MATHML=1) {
    SOURCES += \
        mathml/MathMLElement.cpp \
        mathml/MathMLInlineContainerElement.cpp \
        mathml/MathMLMathElement.cpp \
        mathml/MathMLTextElement.cpp \
        rendering/mathml/RenderMathMLBlock.cpp \
        rendering/mathml/RenderMathMLFenced.cpp \
        rendering/mathml/RenderMathMLFraction.cpp \
        rendering/mathml/RenderMathMLMath.cpp \
        rendering/mathml/RenderMathMLOperator.cpp \
        rendering/mathml/RenderMathMLRoot.cpp \
        rendering/mathml/RenderMathMLRow.cpp \
        rendering/mathml/RenderMathMLSquareRoot.cpp \
        rendering/mathml/RenderMathMLSubSup.cpp \
        rendering/mathml/RenderMathMLUnderOver.cpp
}

# QRawFont feature added in Qt 4.8.0
#
# If available, this is used to implement the fast path for text rendering
# and measurement in WebCore. Because the feature is still undergoing
# development, it is disabled in builds.
#
# exists($$[QT_INSTALL_HEADERS]/QtGui/QRawFont): HAVE_QRAWFONT=1

!isEmpty(HAVE_QRAWFONT) {
    DEFINES += HAVE_QRAWFONT=1

    SOURCES += \
        platform/graphics/FontFastPath.cpp \
        platform/graphics/GlyphPageTreeNode.cpp \
        platform/graphics/WidthIterator.cpp \
        platform/graphics/SurrogatePairAwareTextIterator.cpp

    HEADERS += \
        platform/graphics/WidthIterator.h \
        platform/graphics/SurrogatePairAwareTextIterator.h
}

contains(DEFINES, ENABLE_GEOLOCATION=1) {
    v8 {
        SOURCES += \
            bindings/v8/custom/V8GeolocationCustom.cpp
    }
}

contains(DEFINES, ENABLE_SVG=1) {
    !v8 {
        SOURCES += \
    # TODO: this-one-is-not-auto-added! FIXME! tmp/SVGElementFactory.cpp \
            bindings/js/JSSVGElementInstanceCustom.cpp \
            bindings/js/JSSVGLengthCustom.cpp \
            bindings/js/JSSVGPathSegCustom.cpp
    }

    SOURCES += \
        css/SVGCSSComputedStyleDeclaration.cpp \
        css/SVGCSSParser.cpp \
        css/SVGCSSStyleSelector.cpp \
        rendering/style/SVGRenderStyle.cpp \
        rendering/style/SVGRenderStyleDefs.cpp \
        rendering/PointerEventsHitRules.cpp \
        rendering/svg/RenderSVGPath.cpp \
        rendering/svg/RenderSVGRect.cpp \
        rendering/svg/RenderSVGShape.cpp \
        svg/animation/SMILTime.cpp \
        svg/animation/SMILTimeContainer.cpp \
        svg/animation/SVGSMILElement.cpp \
        svg/graphics/filters/SVGFEImage.cpp \
        svg/graphics/filters/SVGFilter.cpp \
        svg/graphics/filters/SVGFilterBuilder.cpp \
        svg/graphics/SVGImage.cpp \
        svg/graphics/SVGImageCache.cpp \
        svg/properties/SVGAttributeToPropertyMap.cpp \
        svg/properties/SVGPathSegListPropertyTearOff.cpp

    linux-g++*:CONFIG(debug, debug|release):isEqual(QT_ARCH,i386) {
         # Using all in one files to avoid memory exhaustion
         # during the linking phase.
         SOURCES += \
              rendering/svg/RenderSVGAllInOne.cpp \
              svg/SVGAllInOne.cpp
    } else {
         SOURCES += \
              rendering/svg/RenderSVGBlock.cpp \
              rendering/svg/RenderSVGContainer.cpp \
              rendering/svg/RenderSVGForeignObject.cpp \
              rendering/svg/RenderSVGGradientStop.cpp \
              rendering/svg/RenderSVGHiddenContainer.cpp \
              rendering/svg/RenderSVGImage.cpp \
              rendering/svg/RenderSVGInline.cpp \
              rendering/svg/RenderSVGInlineText.cpp \
              rendering/svg/RenderSVGModelObject.cpp \
              rendering/svg/RenderSVGResource.cpp \
              rendering/svg/RenderSVGResourceClipper.cpp \
              rendering/svg/RenderSVGResourceContainer.cpp \
              rendering/svg/RenderSVGResourceFilter.cpp \
              rendering/svg/RenderSVGResourceFilterPrimitive.cpp \
              rendering/svg/RenderSVGResourceGradient.cpp \
              rendering/svg/RenderSVGResourceLinearGradient.cpp \
              rendering/svg/RenderSVGResourceMarker.cpp \
              rendering/svg/RenderSVGResourceMasker.cpp \
              rendering/svg/RenderSVGResourcePattern.cpp \
              rendering/svg/RenderSVGResourceRadialGradient.cpp \
              rendering/svg/RenderSVGResourceSolidColor.cpp \
              rendering/svg/RenderSVGRoot.cpp \
              rendering/svg/RenderSVGShadowTreeRootContainer.cpp \
              rendering/svg/RenderSVGTSpan.cpp \
              rendering/svg/RenderSVGText.cpp \
              rendering/svg/RenderSVGTextPath.cpp \
              rendering/svg/RenderSVGTransformableContainer.cpp \
              rendering/svg/RenderSVGViewportContainer.cpp \
              rendering/svg/SVGImageBufferTools.cpp \
              rendering/svg/SVGInlineFlowBox.cpp \
              rendering/svg/SVGInlineTextBox.cpp \
              rendering/svg/SVGMarkerLayoutInfo.cpp \
              rendering/svg/SVGPathData.cpp \
              rendering/svg/SVGRenderSupport.cpp \
              rendering/svg/SVGRenderTreeAsText.cpp \
              rendering/svg/SVGResources.cpp \
              rendering/svg/SVGResourcesCache.cpp \
              rendering/svg/SVGResourcesCycleSolver.cpp \
              rendering/svg/SVGRootInlineBox.cpp \
              rendering/svg/SVGShadowTreeElements.cpp \
              rendering/svg/SVGTextChunk.cpp \
              rendering/svg/SVGTextChunkBuilder.cpp \
              rendering/svg/SVGTextLayoutAttributes.cpp \
              rendering/svg/SVGTextLayoutAttributesBuilder.cpp \
              rendering/svg/SVGTextLayoutEngine.cpp \
              rendering/svg/SVGTextLayoutEngineBaseline.cpp \
              rendering/svg/SVGTextLayoutEngineSpacing.cpp \
              rendering/svg/SVGTextMetrics.cpp \
              rendering/svg/SVGTextMetricsBuilder.cpp \
              rendering/svg/SVGTextQuery.cpp \
              rendering/svg/SVGTextRunRenderingContext.cpp \
              svg/SVGDocumentExtensions.cpp \
              svg/SVGImageLoader.cpp \
              svg/ColorDistance.cpp \
              svg/SVGAElement.cpp \
              svg/SVGAltGlyphDefElement.cpp \
              svg/SVGAltGlyphElement.cpp \
              svg/SVGAltGlyphItemElement.cpp \
              svg/SVGAngle.cpp \
              svg/SVGAnimateColorElement.cpp \
              svg/SVGAnimatedAngle.cpp \
              svg/SVGAnimatedBoolean.cpp \
              svg/SVGAnimatedColor.cpp \
              svg/SVGAnimatedInteger.cpp \
              svg/SVGAnimatedLength.cpp \
              svg/SVGAnimatedLengthList.cpp \
              svg/SVGAnimatedNumber.cpp \
              svg/SVGAnimatedNumberList.cpp \
              svg/SVGAnimatedNumberOptionalNumber.cpp \
              svg/SVGAnimatedPath.cpp \
              svg/SVGAnimatedPointList.cpp \
              svg/SVGAnimatedPreserveAspectRatio.cpp \
              svg/SVGAnimatedRect.cpp \
              svg/SVGAnimatedString.cpp \
              svg/SVGAnimatedType.cpp \
              svg/SVGAnimateElement.cpp \
              svg/SVGAnimateMotionElement.cpp \
              svg/SVGAnimateTransformElement.cpp \
              svg/SVGAnimationElement.cpp \
              svg/SVGCircleElement.cpp \
              svg/SVGClipPathElement.cpp \
              svg/SVGColor.cpp \
              svg/SVGComponentTransferFunctionElement.cpp \
              svg/SVGCursorElement.cpp \
              svg/SVGDefsElement.cpp \
              svg/SVGDescElement.cpp \
              svg/SVGDocument.cpp \
              svg/SVGElement.cpp \
              svg/SVGElementInstance.cpp \
              svg/SVGElementInstanceList.cpp \
              svg/SVGEllipseElement.cpp \
              svg/SVGException.cpp \
              svg/SVGExternalResourcesRequired.cpp \
              svg/SVGFEBlendElement.cpp \
              svg/SVGFEColorMatrixElement.cpp \
              svg/SVGFEComponentTransferElement.cpp \
              svg/SVGFECompositeElement.cpp \
              svg/SVGFEConvolveMatrixElement.cpp \
              svg/SVGFEDiffuseLightingElement.cpp \
              svg/SVGFEDisplacementMapElement.cpp \
              svg/SVGFEDistantLightElement.cpp \
              svg/SVGFEDropShadowElement.cpp \
              svg/SVGFEFloodElement.cpp \
              svg/SVGFEFuncAElement.cpp \
              svg/SVGFEFuncBElement.cpp \
              svg/SVGFEFuncGElement.cpp \
              svg/SVGFEFuncRElement.cpp \
              svg/SVGFEGaussianBlurElement.cpp \
              svg/SVGFEImageElement.cpp \
              svg/SVGFELightElement.cpp \
              svg/SVGFEMergeElement.cpp \
              svg/SVGFEMergeNodeElement.cpp \
              svg/SVGFEMorphologyElement.cpp \
              svg/SVGFEOffsetElement.cpp \
              svg/SVGFEPointLightElement.cpp \
              svg/SVGFESpecularLightingElement.cpp \
              svg/SVGFESpotLightElement.cpp \
              svg/SVGFETileElement.cpp \
              svg/SVGFETurbulenceElement.cpp \
              svg/SVGFilterElement.cpp \
              svg/SVGFilterPrimitiveStandardAttributes.cpp \
              svg/SVGFitToViewBox.cpp \
              svg/SVGFontData.cpp \
              svg/SVGFontElement.cpp \
              svg/SVGFontFaceElement.cpp \
              svg/SVGFontFaceFormatElement.cpp \
              svg/SVGFontFaceNameElement.cpp \
              svg/SVGFontFaceSrcElement.cpp \
              svg/SVGFontFaceUriElement.cpp \
              svg/SVGForeignObjectElement.cpp \
              svg/SVGGElement.cpp \
              svg/SVGGlyphElement.cpp \
              svg/SVGGlyphRefElement.cpp \
              svg/SVGGradientElement.cpp \
              svg/SVGHKernElement.cpp \
              svg/SVGImageElement.cpp \
              svg/SVGLangSpace.cpp \
              svg/SVGLength.cpp \
              svg/SVGLengthContext.cpp \
              svg/SVGLengthList.cpp \
              svg/SVGLinearGradientElement.cpp \
              svg/SVGLineElement.cpp \
              svg/SVGLocatable.cpp \
              svg/SVGMarkerElement.cpp \
              svg/SVGMaskElement.cpp \
              svg/SVGMetadataElement.cpp \
              svg/SVGMissingGlyphElement.cpp \
              svg/SVGMPathElement.cpp \
              svg/SVGNumberList.cpp \
              svg/SVGPaint.cpp \
              svg/SVGParserUtilities.cpp \
              svg/SVGPathBlender.cpp \
              svg/SVGPathBuilder.cpp \
              svg/SVGPathByteStreamBuilder.cpp \
              svg/SVGPathByteStreamSource.cpp \
              svg/SVGPathElement.cpp \
              svg/SVGPathParser.cpp \
              svg/SVGPathParserFactory.cpp \
              svg/SVGPathSegList.cpp \
              svg/SVGPathSegListBuilder.cpp \
              svg/SVGPathSegListSource.cpp \
              svg/SVGPathStringBuilder.cpp \
              svg/SVGPathStringSource.cpp \
              svg/SVGPathTraversalStateBuilder.cpp \
              svg/SVGPatternElement.cpp \
              svg/SVGPointList.cpp \
              svg/SVGPolyElement.cpp \
              svg/SVGPolygonElement.cpp \
              svg/SVGPolylineElement.cpp \
              svg/SVGPreserveAspectRatio.cpp \
              svg/SVGRadialGradientElement.cpp \
              svg/SVGRectElement.cpp \
              svg/SVGScriptElement.cpp \
              svg/SVGSetElement.cpp \
              svg/SVGStopElement.cpp \
              svg/SVGStringList.cpp \
              svg/SVGStylable.cpp \
              svg/SVGStyledElement.cpp \
              svg/SVGStyledLocatableElement.cpp \
              svg/SVGStyledTransformableElement.cpp \
              svg/SVGStyleElement.cpp \
              svg/SVGSVGElement.cpp \
              svg/SVGSwitchElement.cpp \
              svg/SVGSymbolElement.cpp \
              svg/SVGTests.cpp \
              svg/SVGTextContentElement.cpp \
              svg/SVGTextElement.cpp \
              svg/SVGTextPathElement.cpp \
              svg/SVGTextPositioningElement.cpp \
              svg/SVGTitleElement.cpp \
              svg/SVGTransformable.cpp \
              svg/SVGTransform.cpp \
              svg/SVGTransformDistance.cpp \
              svg/SVGTransformList.cpp \
              svg/SVGTRefElement.cpp \
              svg/SVGTSpanElement.cpp \
              svg/SVGURIReference.cpp \
              svg/SVGUseElement.cpp \
              svg/SVGViewElement.cpp \
              svg/SVGViewSpec.cpp \
              svg/SVGVKernElement.cpp \
              svg/SVGZoomAndPan.cpp \
              svg/SVGZoomEvent.cpp
   }
}

contains(DEFINES, ENABLE_JAVASCRIPT_DEBUGGER=1) {
    v8 {
        SOURCES += \
            bindings/v8/ScriptDebugServer.cpp \
            bindings/v8/ScriptProfiler.cpp \
            bindings/v8/ScriptHeapSnapshot.cpp \
            bindings/v8/JavaScriptCallFrame.cpp \
            bindings/v8/custom/V8ScriptProfileCustom.cpp \
            bindings/v8/custom/V8JavaScriptCallFrameCustom.cpp \
            bindings/v8/custom/V8ScriptProfileNodeCustom.cpp \
            bindings/v8/ScriptProfileNode.cpp \
            bindings/v8/ScriptProfile.cpp
    } else {
        SOURCES += \
            bindings/js/JSJavaScriptCallFrameCustom.cpp \
            bindings/js/ScriptProfiler.cpp \
            bindings/js/JavaScriptCallFrame.cpp
    }
}


contains(DEFINES, ENABLE_VIDEO_TRACK=1) {
    v8 {
        SOURCES += \
            bindings/v8/custom/V8TrackEventCustom.cpp
    } else {
        SOURCES += \
            bindings/js/JSTextTrackCueCustom.cpp \
            bindings/js/JSTextTrackCustom.cpp \
            bindings/js/JSTextTrackCustom.h \
            bindings/js/JSTrackCustom.cpp \
            bindings/js/JSTrackCustom.h \
            bindings/js/JSTrackEventCustom.cpp \
            bindings/js/JSTextTrackListCustom.cpp
    }
}

contains(DEFINES, ENABLE_WEB_SOCKETS=1) {
    HEADERS += \
        websockets/CloseEvent.h \
        websockets/ThreadableWebSocketChannel.h \
        websockets/ThreadableWebSocketChannelClientWrapper.h \
        websockets/WebSocket.h \
        websockets/WebSocketChannel.h \
        websockets/WebSocketChannelClient.h \
        websockets/WebSocketHandshake.h \
        websockets/WebSocketHandshakeRequest.h \
        websockets/WebSocketHandshakeResponse.h \
        platform/network/qt/SocketStreamHandlePrivate.h

    SOURCES += \
        websockets/WebSocket.cpp \
        websockets/WebSocketChannel.cpp \
        websockets/WebSocketHandshake.cpp \
        websockets/WebSocketHandshakeRequest.cpp \
        websockets/WebSocketHandshakeResponse.cpp \
        websockets/ThreadableWebSocketChannel.cpp \
        websockets/ThreadableWebSocketChannelClientWrapper.cpp \
        platform/network/SocketStreamErrorBase.cpp \
        platform/network/SocketStreamHandleBase.cpp \
        platform/network/qt/SocketStreamHandleQt.cpp

    !v8 {
        SOURCES += \
            bindings/js/JSWebSocketCustom.cpp
    }

    contains(DEFINES, ENABLE_WORKERS=1) {
        HEADERS += \
            websockets/WorkerThreadableWebSocketChannel.h

        SOURCES += \
            websockets/WorkerThreadableWebSocketChannel.cpp
    }
}

contains(DEFINES, ENABLE_WEBGL=1) {
    HEADERS += \
        html/canvas/CanvasContextAttributes.h \
        html/canvas/WebGLObject.h \
        html/canvas/WebGLActiveInfo.h \
        html/canvas/WebGLBuffer.h \
        html/canvas/WebGLCompressedTextures.h \
        html/canvas/WebGLContextAttributes.h \
        html/canvas/WebGLContextEvent.h \
        html/canvas/WebGLContextGroup.h \
        html/canvas/WebGLContextObject.h \
        html/canvas/WebGLDebugRendererInfo.h \
        html/canvas/WebGLDebugShaders.h \
        html/canvas/WebGLExtension.h \
        html/canvas/WebGLFramebuffer.h \
        html/canvas/WebGLGetInfo.h \
        html/canvas/WebGLLoseContext.h \
        html/canvas/WebGLProgram.h \
        html/canvas/WebGLRenderbuffer.h \
        html/canvas/WebGLRenderingContext.h \
        html/canvas/WebGLShader.h \
        html/canvas/WebGLSharedObject.h \
        html/canvas/OESStandardDerivatives.h \
        html/canvas/OESTextureFloat.h \
        html/canvas/OESVertexArrayObject.h \
        html/canvas/WebGLTexture.h \
        html/canvas/WebGLUniformLocation.h \
        html/canvas/WebGLVertexArrayObjectOES.h \
        platform/graphics/ANGLEWebKitBridge.h \
        platform/graphics/Extensions3D.h \
        platform/graphics/GraphicsContext3D.h \
        platform/graphics/gpu/DrawingBuffer.h \
        platform/graphics/qt/Extensions3DQt.h

    v8 {
        SOURCES += \
            bindings/v8/custom/V8WebGLRenderingContextCustom.cpp
    } else {
        SOURCES += \
            bindings/js/JSWebGLRenderingContextCustom.cpp
    }

    SOURCES += \
        html/canvas/CanvasContextAttributes.cpp \
        html/canvas/WebGLObject.cpp \
        html/canvas/WebGLBuffer.cpp \
        html/canvas/WebGLCompressedTextures.cpp \
        html/canvas/WebGLContextAttributes.cpp \
        html/canvas/WebGLContextEvent.cpp \
        html/canvas/WebGLContextGroup.cpp \
        html/canvas/WebGLContextObject.cpp \
        html/canvas/WebGLDebugRendererInfo.cpp \
        html/canvas/WebGLDebugShaders.cpp \
        html/canvas/WebGLExtension.cpp \
        html/canvas/WebGLFramebuffer.cpp \
        html/canvas/WebGLGetInfo.cpp \
        html/canvas/WebGLLoseContext.cpp \
        html/canvas/WebGLProgram.cpp \
        html/canvas/WebGLRenderbuffer.cpp \
        html/canvas/WebGLRenderingContext.cpp \
        html/canvas/WebGLShader.cpp \
        html/canvas/WebGLSharedObject.cpp \
        html/canvas/OESStandardDerivatives.cpp \
        html/canvas/OESTextureFloat.cpp \
        html/canvas/OESVertexArrayObject.cpp \
        html/canvas/WebGLTexture.cpp \
        html/canvas/WebGLUniformLocation.cpp \
        html/canvas/WebGLVertexArrayObjectOES.cpp \
        platform/graphics/ANGLEWebKitBridge.cpp \
        platform/graphics/GraphicsContext3D.cpp \
        platform/graphics/gpu/DrawingBuffer.cpp \
        platform/graphics/gpu/qt/DrawingBufferQt.cpp \
        platform/graphics/qt/Extensions3DQt.cpp \
        platform/graphics/qt/GraphicsContext3DQt.cpp

    INCLUDEPATH += $$PWD/platform/graphics/gpu

    !contains(QT_CONFIG, opengles2) {
        HEADERS += \
            platform/graphics/opengl/Extensions3DOpenGL.h

        SOURCES += \
            platform/graphics/opengl/Extensions3DOpenGL.cpp \
            platform/graphics/opengl/GraphicsContext3DOpenGL.cpp \
            platform/graphics/opengl/GraphicsContext3DOpenGLCommon.cpp

        ANGLE_DIR = $$replace(PWD, "WebCore", "ThirdParty/ANGLE")

        INCLUDEPATH += $$ANGLE_DIR/src $$ANGLE_DIR/include

        ANGLE_HEADERS += \
            $$ANGLE_DIR/src/compiler/BaseTypes.h \
            $$ANGLE_DIR/src/compiler/BuiltInFunctionEmulator.h \
            $$ANGLE_DIR/src/compiler/Common.h \
            $$ANGLE_DIR/src/compiler/ConstantUnion.h \
            $$ANGLE_DIR/src/compiler/debug.h \
            $$ANGLE_DIR/src/compiler/DetectRecursion.h \
            $$ANGLE_DIR/src/compiler/ExtensionBehavior.h \
            $$ANGLE_DIR/src/compiler/ForLoopUnroll.h \
            $$ANGLE_DIR/src/compiler/glslang.h \
            $$ANGLE_DIR/src/compiler/glslang_tab.h \
            $$ANGLE_DIR/src/compiler/InfoSink.h \
            $$ANGLE_DIR/src/compiler/InitializeDll.h \
            $$ANGLE_DIR/src/compiler/InitializeGlobals.h \
            $$ANGLE_DIR/src/compiler/Initialize.h \
            $$ANGLE_DIR/src/compiler/InitializeParseContext.h \
            $$ANGLE_DIR/src/compiler/intermediate.h \
            $$ANGLE_DIR/src/compiler/localintermediate.h \
            $$ANGLE_DIR/src/compiler/MMap.h \
            $$ANGLE_DIR/src/compiler/MapLongVariableNames.h \
            $$ANGLE_DIR/src/compiler/osinclude.h \
            $$ANGLE_DIR/src/compiler/preprocessor/atom.h \
            $$ANGLE_DIR/src/compiler/preprocessor/compile.h \
            $$ANGLE_DIR/src/compiler/preprocessor/cpp.h \
            $$ANGLE_DIR/src/compiler/preprocessor/length_limits.h \
            $$ANGLE_DIR/src/compiler/preprocessor/memory.h \
            $$ANGLE_DIR/src/compiler/preprocessor/parser.h \
            $$ANGLE_DIR/src/compiler/preprocessor/preprocess.h \
            $$ANGLE_DIR/src/compiler/preprocessor/scanner.h \
            $$ANGLE_DIR/src/compiler/preprocessor/slglobals.h \
            $$ANGLE_DIR/src/compiler/preprocessor/symbols.h \
            $$ANGLE_DIR/src/compiler/preprocessor/tokens.h \
            $$ANGLE_DIR/src/compiler/OutputESSL.h \
            $$ANGLE_DIR/src/compiler/OutputGLSL.h \
            $$ANGLE_DIR/src/compiler/OutputGLSLBase.h \
            $$ANGLE_DIR/src/compiler/OutputHLSL.h \
            $$ANGLE_DIR/src/compiler/ParseHelper.h \
            $$ANGLE_DIR/src/compiler/PoolAlloc.h \
            $$ANGLE_DIR/src/compiler/QualifierAlive.h \
            $$ANGLE_DIR/src/compiler/RemoveTree.h \
            $$ANGLE_DIR/src/compiler/SearchSymbol.h \
            $$ANGLE_DIR/src/compiler/ShHandle.h \
            $$ANGLE_DIR/src/compiler/SymbolTable.h \
            $$ANGLE_DIR/src/compiler/TranslatorESSL.h \
            $$ANGLE_DIR/src/compiler/TranslatorGLSL.h \
            $$ANGLE_DIR/src/compiler/TranslatorHLSL.h \
            $$ANGLE_DIR/src/compiler/Types.h \
            $$ANGLE_DIR/src/compiler/UnfoldSelect.h \
            $$ANGLE_DIR/src/compiler/util.h \
            $$ANGLE_DIR/src/compiler/ValidateLimitations.h \
            $$ANGLE_DIR/src/compiler/VariableInfo.h \
            $$ANGLE_DIR/src/compiler/VersionGLSL.h

        HEADERS += $$ANGLE_HEADERS

        ANGLE_SOURCES += \
            $$ANGLE_DIR/src/compiler/BuiltInFunctionEmulator.cpp \
            $$ANGLE_DIR/src/compiler/CodeGenGLSL.cpp \
            $$ANGLE_DIR/src/compiler/Compiler.cpp \
            $$ANGLE_DIR/src/compiler/debug.cpp \
            $$ANGLE_DIR/src/compiler/DetectRecursion.cpp \
            $$ANGLE_DIR/src/compiler/ForLoopUnroll.cpp \
            $$ANGLE_DIR/src/compiler/glslang_lex.cpp \
            $$ANGLE_DIR/src/compiler/glslang_tab.cpp \
            $$ANGLE_DIR/src/compiler/InfoSink.cpp \
            $$ANGLE_DIR/src/compiler/Initialize.cpp \
            $$ANGLE_DIR/src/compiler/InitializeDll.cpp \
            $$ANGLE_DIR/src/compiler/Intermediate.cpp \
            $$ANGLE_DIR/src/compiler/intermOut.cpp \
            $$ANGLE_DIR/src/compiler/IntermTraverse.cpp \
            $$ANGLE_DIR/src/compiler/MapLongVariableNames.cpp \
            $$ANGLE_DIR/src/compiler/ossource_posix.cpp \
            $$ANGLE_DIR/src/compiler/OutputESSL.cpp \
            $$ANGLE_DIR/src/compiler/OutputGLSL.cpp \
            $$ANGLE_DIR/src/compiler/OutputGLSLBase.cpp \
            $$ANGLE_DIR/src/compiler/OutputHLSL.cpp \
            $$ANGLE_DIR/src/compiler/parseConst.cpp \
            $$ANGLE_DIR/src/compiler/ParseHelper.cpp \
            $$ANGLE_DIR/src/compiler/PoolAlloc.cpp \
            $$ANGLE_DIR/src/compiler/QualifierAlive.cpp \
            $$ANGLE_DIR/src/compiler/RemoveTree.cpp \
            $$ANGLE_DIR/src/compiler/SearchSymbol.cpp \
            $$ANGLE_DIR/src/compiler/ShaderLang.cpp \
            $$ANGLE_DIR/src/compiler/SymbolTable.cpp \
            $$ANGLE_DIR/src/compiler/TranslatorESSL.cpp \
            $$ANGLE_DIR/src/compiler/TranslatorGLSL.cpp \
            $$ANGLE_DIR/src/compiler/TranslatorHLSL.cpp \
            $$ANGLE_DIR/src/compiler/UnfoldSelect.cpp \
            $$ANGLE_DIR/src/compiler/util.cpp \
            $$ANGLE_DIR/src/compiler/ValidateLimitations.cpp \
            $$ANGLE_DIR/src/compiler/VariableInfo.cpp \
            $$ANGLE_DIR/src/compiler/VersionGLSL.cpp

        SOURCES += \
            $$ANGLE_DIR/src/compiler/preprocessor/atom.c \
            $$ANGLE_DIR/src/compiler/preprocessor/cpp.c \
            $$ANGLE_DIR/src/compiler/preprocessor/cppstruct.c \
            $$ANGLE_DIR/src/compiler/preprocessor/memory.c \
            $$ANGLE_DIR/src/compiler/preprocessor/scanner.c \
            $$ANGLE_DIR/src/compiler/preprocessor/symbols.c \
            $$ANGLE_DIR/src/compiler/preprocessor/tokens.c

        *g++* {
            ANGLE_CFLAGS += -Wno-unused-variable
            ANGLE_CFLAGS += -Wno-missing-noreturn
            ANGLE_CFLAGS += -Wno-unused-function
            ANGLE_CFLAGS += -Wno-reorder

            angle_cxx.commands = $$QMAKE_CXX -c $(CXXFLAGS) $$ANGLE_CFLAGS $(INCPATH) ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}
            angle_cxx.output = ${QMAKE_VAR_OBJECTS_DIR}${QMAKE_FILE_BASE}$$QMAKE_EXT_OBJ
            anglc_cxx.dependency_type = TYPE_C
            angle_cxx.depends = $$ANGLE_HEADERS
            angle_cxx.input = ANGLE_SOURCES
            QMAKE_EXTRA_COMPILERS += angle_cxx
        } else {
            SOURCES += $$ANGLE_SOURCES
        }
    }

    CONFIG += opengl-shims
}

contains(DEFINES, ENABLE_MHTML=1) {

    INCLUDEPATH += $$PWD/loader/archive/mhtml

    HEADERS += \
        loader/archive/Archive.h \
        page/PageSerializer.h

    SOURCES += \
        loader/archive/Archive.cpp \
        loader/archive/ArchiveFactory.cpp \
        loader/archive/mhtml/MHTMLArchive.cpp \
        loader/archive/mhtml/MHTMLParser.cpp \
        page/PageSerializer.cpp
}

!system-sqlite:exists( $${SQLITE3SRCDIR}/sqlite3.c ) {
    # Build sqlite3 into WebCore from source
    # somewhat copied from $$QT_SOURCE_TREE/src/plugins/sqldrivers/sqlite/sqlite.pro
    SOURCES += $${SQLITE3SRCDIR}/sqlite3.c
}

win32:!win32-g++*:contains(QMAKE_HOST.arch, x86_64):{
    asm_compiler.commands = ml64 /c
    asm_compiler.commands +=  /Fo ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
    asm_compiler.output = ${QMAKE_VAR_OBJECTS_DIR}${QMAKE_FILE_BASE}$${first(QMAKE_EXT_OBJ)}
    asm_compiler.input = ASM_SOURCES
    asm_compiler.variable_out = OBJECTS
    asm_compiler.name = compiling[asm] ${QMAKE_FILE_IN}
    silent:asm_compiler.commands = @echo compiling[asm] ${QMAKE_FILE_IN} && $$asm_compiler.commands
    QMAKE_EXTRA_COMPILERS += asm_compiler

    ASM_SOURCES += \
        plugins/win/PaintHooks.asm
   if(win32-msvc2005|win32-msvc2008):equals(TEMPLATE_PREFIX, "vc") {
        SOURCES += \
            plugins/win/PaintHooks.asm
   }
}

contains(CONFIG, texmap) {
    HEADERS += \
        platform/graphics/qt/TextureMapperQt.h \
        platform/graphics/texmap/GraphicsLayerTextureMapper.h \
        platform/graphics/texmap/LayerTransform.h \
        platform/graphics/texmap/TextureMapper.h \
        platform/graphics/texmap/TextureMapperAnimation.h \
        platform/graphics/texmap/TextureMapperNode.h \
        platform/graphics/texmap/TextureMapperPlatformLayer.h

    SOURCES += \
        platform/graphics/qt/TextureMapperQt.cpp \
        platform/graphics/texmap/LayerTransform.cpp \
        platform/graphics/texmap/TextureMapper.cpp \
        platform/graphics/texmap/TextureMapperAnimation.cpp \
        platform/graphics/texmap/TextureMapperNode.cpp \
        platform/graphics/texmap/GraphicsLayerTextureMapper.cpp

    !win32-*:contains(QT_CONFIG, opengl) {
        HEADERS += platform/graphics/opengl/TextureMapperGL.h
        SOURCES += platform/graphics/opengl/TextureMapperGL.cpp
        CONFIG += opengl-shims
    }
} else {
    HEADERS += platform/graphics/qt/GraphicsLayerQt.h
    SOURCES += platform/graphics/qt/GraphicsLayerQt.cpp
}

contains(CONFIG, opengl-shims) {
    HEADERS += platform/graphics/cairo/OpenGLShims.h
    SOURCES += platform/graphics/cairo/OpenGLShims.cpp
    DEFINES += QT_OPENGL_SHIMS=1
}

# Make sure the derived sources are built
include(DerivedSources.pri)

