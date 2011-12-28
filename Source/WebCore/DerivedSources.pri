# -------------------------------------------------------------------
# Derived sources for WebCore
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

# This file is both a top level target, and included from Target.pri,
# so that the resulting generated sources can be added to SOURCES.
# We only set the template if we're a top level target, so that we
# don't override what Target.pri has already set.
sanitizedFile = $$toSanitizedPath($$_FILE_)
equals(sanitizedFile, $$toSanitizedPath($$_PRO_FILE_)):TEMPLATE = derived

load(features)

mac {
    # FIXME: This runs the perl script every time. Is there a way we can run it only when deps change?
    fwheader_generator.commands = perl $${ROOT_WEBKIT_DIR}/Source/WebKit2/Scripts/generate-forwarding-headers.pl $${ROOT_WEBKIT_DIR}/Source/WebCore $${ROOT_BUILD_DIR}/Source/include mac
    fwheader_generator.depends = $${ROOT_WEBKIT_DIR}/Source/WebKit2/Scripts/generate-forwarding-headers.pl
    GENERATORS += fwheader_generator
}

MATHML_NAMES = $$PWD/mathml/mathtags.in

SVG_NAMES = $$PWD/svg/svgtags.in

XLINK_NAMES = $$PWD/svg/xlinkattrs.in

TOKENIZER = $$PWD/css/tokenizer.flex

CSSBISON = $$PWD/css/CSSGrammar.y

contains(DEFINES, ENABLE_XSLT=1) {
    XMLVIEWER_CSS = $$PWD/xml/XMLViewer.css
    XMLVIEWER_JS = $$PWD/xml/XMLViewer.js
}

FONT_NAMES = $$PWD/css/WebKitFontFamilyNames.in

HTML_NAMES = $$PWD/html/HTMLTagNames.in

XML_NAMES = $$PWD/xml/xmlattrs.in

XMLNS_NAMES = $$PWD/xml/xmlnsattrs.in

HTML_ENTITIES = $$PWD/html/parser/HTMLEntityNames.in

EVENT_FACTORY = $$PWD/dom/EventFactory.in

EVENT_TARGET_FACTORY = $$PWD/dom/EventTargetFactory.in

DOM_EXCEPTIONS = $$PWD/dom/DOMExceptions.in

COLORDATA_GPERF = $$PWD/platform/ColorData.gperf

WALDOCSSPROPS = $$PWD/css/CSSPropertyNames.in

WALDOCSSVALUES = $$PWD/css/CSSValueKeywords.in

INSPECTOR_JSON = $$PWD/inspector/Inspector.json

INSPECTOR_BACKEND_STUB_QRC = $$PWD/inspector/front-end/InspectorBackendStub.qrc

INJECTED_SCRIPT_SOURCE = $$PWD/inspector/InjectedScriptSource.js

DEBUGGER_SCRIPT_SOURCE = $$PWD/bindings/v8/DebuggerScript.js

ARRAY_BUFFER_VIEW_CUSTOM_SCRIPT_SOURCE = $$PWD/bindings/v8/custom/V8ArrayBufferViewCustomScript.js

contains(DEFINES, ENABLE_DASHBOARD_SUPPORT=1): DASHBOARDSUPPORTCSSPROPERTIES = $$PWD/css/DashboardSupportCSSPropertyNames.in

XPATHBISON = $$PWD/xml/XPathGrammar.y

contains(DEFINES, ENABLE_SVG=1) {
    EXTRACSSPROPERTIES += $$PWD/css/SVGCSSPropertyNames.in
    EXTRACSSVALUES += $$PWD/css/SVGCSSValueKeywords.in
}

STYLESHEETS_EMBED = \
    $$PWD/css/html.css \
    $$PWD/css/quirks.css \
    $$PWD/css/mathml.css \
    $$PWD/css/svg.css \
    $$PWD/css/view-source.css \
    $$PWD/css/fullscreen.css \
    $$PWD/css/mediaControls.css \
    $$PWD/css/mediaControlsQt.css \
    $$PWD/css/mediaControlsQtFullscreen.css \
    $$PWD/css/themeQtNoListboxes.css \
    $$PWD/css/mobileThemeQt.css

IDL_BINDINGS += \
    css/Counter.idl \
    css/CSSCharsetRule.idl \
    css/CSSFontFaceRule.idl \
    css/CSSImportRule.idl \
    css/CSSMediaRule.idl \
    css/CSSPageRule.idl \
    css/CSSPrimitiveValue.idl \
    css/CSSRule.idl \
    css/CSSRuleList.idl \
    css/CSSStyleDeclaration.idl \
    css/CSSStyleRule.idl \
    css/CSSStyleSheet.idl \
    css/CSSValue.idl \
    css/CSSValueList.idl \
    css/MediaList.idl \
    css/MediaQueryList.idl \
    css/Rect.idl \
    css/RGBColor.idl \
    css/StyleMedia.idl \
    css/StyleSheet.idl \
    css/StyleSheetList.idl \
    css/WebKitCSSFilterValue.idl \
    css/WebKitCSSKeyframeRule.idl \
    css/WebKitCSSKeyframesRule.idl \
    css/WebKitCSSMatrix.idl \
    css/WebKitCSSTransformValue.idl \
    dom/Attr.idl \
    dom/BeforeLoadEvent.idl \
    dom/CharacterData.idl \
    dom/ClientRect.idl \
    dom/ClientRectList.idl \
    dom/Clipboard.idl \
    dom/CDATASection.idl \
    dom/Comment.idl \
    dom/CompositionEvent.idl \
    dom/CustomEvent.idl \
    dom/DataTransferItem.idl \
    dom/DataTransferItemList.idl \
    dom/DeviceMotionEvent.idl \
    dom/DeviceOrientationEvent.idl \
    dom/DocumentFragment.idl \
    dom/Document.idl \
    dom/DocumentType.idl \
    dom/DOMCoreException.idl \
    dom/DOMImplementation.idl \
    dom/DOMStringList.idl \
    dom/DOMStringMap.idl \
    dom/Element.idl \
    dom/Entity.idl \
    dom/EntityReference.idl \
    dom/ErrorEvent.idl \
    dom/Event.idl \
    dom/EventException.idl \
#    dom/EventListener.idl \
#    dom/EventTarget.idl \
    dom/HashChangeEvent.idl \
    dom/KeyboardEvent.idl \
    dom/MouseEvent.idl \
    dom/MessageChannel.idl \
    dom/MessageEvent.idl \
    dom/MessagePort.idl \
    dom/MutationEvent.idl \
    dom/NamedNodeMap.idl \
    dom/Node.idl \
    dom/NodeFilter.idl \
    dom/NodeIterator.idl \
    dom/NodeList.idl \
    dom/Notation.idl \
    dom/OverflowEvent.idl \
    dom/PageTransitionEvent.idl \
    dom/PopStateEvent.idl \
    dom/ProcessingInstruction.idl \
    dom/ProgressEvent.idl \
    dom/RangeException.idl \
    dom/Range.idl \
    dom/RequestAnimationFrameCallback.idl \
    dom/StringCallback.idl \
    dom/Text.idl \
    dom/TextEvent.idl \
    dom/Touch.idl \
    dom/TouchEvent.idl \
    dom/TouchList.idl \
    dom/TreeWalker.idl \
    dom/UIEvent.idl \
    dom/WebKitAnimationEvent.idl \
    dom/WebKitNamedFlow.idl \
    dom/WebKitTransitionEvent.idl \
    dom/WheelEvent.idl \
    fileapi/Blob.idl \
    fileapi/DirectoryEntry.idl \
    fileapi/DirectoryEntrySync.idl \
    fileapi/DirectoryReader.idl \
    fileapi/DirectoryReaderSync.idl \
    fileapi/DOMFileSystem.idl \
    fileapi/DOMFileSystemSync.idl \
    fileapi/EntriesCallback.idl \
    fileapi/Entry.idl \
    fileapi/EntryArray.idl \
    fileapi/EntryArraySync.idl \
    fileapi/EntryCallback.idl \
    fileapi/EntrySync.idl \
    fileapi/ErrorCallback.idl \
    fileapi/File.idl \
    fileapi/FileCallback.idl \
    fileapi/FileEntry.idl \
    fileapi/FileEntrySync.idl \
    fileapi/FileError.idl \
    fileapi/FileException.idl \
    fileapi/FileList.idl \
    fileapi/FileReader.idl \
    fileapi/FileReaderSync.idl \
    fileapi/FileSystemCallback.idl \
    fileapi/FileWriter.idl \
    fileapi/FileWriterCallback.idl \
    fileapi/OperationNotAllowedException.idl \
    fileapi/WebKitFlags.idl \
    fileapi/Metadata.idl \
    fileapi/MetadataCallback.idl \
    fileapi/WebKitBlobBuilder.idl \
    html/canvas/ArrayBufferView.idl \
    html/canvas/ArrayBuffer.idl \
    html/canvas/DataView.idl \
    html/canvas/Int8Array.idl \
    html/canvas/Float32Array.idl \
    html/canvas/Float64Array.idl \
    html/canvas/CanvasGradient.idl \
    html/canvas/Int32Array.idl \
    html/canvas/CanvasPattern.idl \
    html/canvas/CanvasRenderingContext.idl \
    html/canvas/CanvasRenderingContext2D.idl \
    html/canvas/OESStandardDerivatives.idl \
    html/canvas/OESTextureFloat.idl \
    html/canvas/OESVertexArrayObject.idl \
    html/canvas/WebGLActiveInfo.idl \
    html/canvas/WebGLBuffer.idl \
    html/canvas/WebGLContextAttributes.idl \
    html/canvas/WebGLContextEvent.idl \
    html/canvas/WebGLDebugRendererInfo.idl \
    html/canvas/WebGLDebugShaders.idl \
    html/canvas/WebGLFramebuffer.idl \
    html/canvas/WebGLLoseContext.idl \
    html/canvas/WebGLProgram.idl \
    html/canvas/WebGLRenderbuffer.idl \
    html/canvas/WebGLRenderingContext.idl \
    html/canvas/WebGLShader.idl \
    html/canvas/Int16Array.idl \
    html/canvas/WebGLTexture.idl \
    html/canvas/WebGLUniformLocation.idl \
    html/canvas/WebGLVertexArrayObjectOES.idl \
    html/canvas/Uint8Array.idl \
    html/canvas/Uint32Array.idl \
    html/canvas/Uint16Array.idl \
    html/DOMFormData.idl \
    html/DOMSettableTokenList.idl \
    html/DOMTokenList.idl \
    html/DOMURL.idl \
    html/HTMLAllCollection.idl \
    html/HTMLAudioElement.idl \
    html/HTMLAnchorElement.idl \
    html/HTMLAppletElement.idl \
    html/HTMLAreaElement.idl \
    html/HTMLBaseElement.idl \
    html/HTMLBaseFontElement.idl \
    html/HTMLBodyElement.idl \
    html/HTMLBRElement.idl \
    html/HTMLButtonElement.idl \
    html/HTMLCanvasElement.idl \
    html/HTMLCollection.idl \
    html/HTMLDataListElement.idl \
    html/HTMLDetailsElement.idl \
    html/HTMLDirectoryElement.idl \
    html/HTMLDivElement.idl \
    html/HTMLDListElement.idl \
    html/HTMLDocument.idl \
    html/HTMLElement.idl \
    html/HTMLEmbedElement.idl \
    html/HTMLFieldSetElement.idl \
    html/HTMLFontElement.idl \
    html/HTMLFormElement.idl \
    html/HTMLFrameElement.idl \
    html/HTMLFrameSetElement.idl \
    html/HTMLHeadElement.idl \
    html/HTMLHeadingElement.idl \
    html/HTMLHRElement.idl \
    html/HTMLHtmlElement.idl \
    html/HTMLIFrameElement.idl \
    html/HTMLImageElement.idl \
    html/HTMLInputElement.idl \
    html/HTMLIsIndexElement.idl \
    html/HTMLKeygenElement.idl \
    html/HTMLLabelElement.idl \
    html/HTMLLegendElement.idl \
    html/HTMLLIElement.idl \
    html/HTMLLinkElement.idl \
    html/HTMLMapElement.idl \
    html/HTMLMarqueeElement.idl \
    html/HTMLMediaElement.idl \
    html/HTMLMenuElement.idl \
    html/HTMLMetaElement.idl \
    html/HTMLMeterElement.idl \
    html/HTMLModElement.idl \
    html/HTMLObjectElement.idl \
    html/HTMLOListElement.idl \
    html/HTMLOptGroupElement.idl \
    html/HTMLOptionElement.idl \
    html/HTMLOptionsCollection.idl \
    html/HTMLOutputElement.idl \
    html/HTMLParagraphElement.idl \
    html/HTMLParamElement.idl \
    html/HTMLPreElement.idl \
    html/HTMLProgressElement.idl \
    html/HTMLPropertiesCollection.idl \
    html/HTMLQuoteElement.idl \
    html/HTMLScriptElement.idl \
    html/HTMLSelectElement.idl \
    html/HTMLSourceElement.idl \
    html/HTMLSpanElement.idl \
    html/HTMLStyleElement.idl \
    html/HTMLTableCaptionElement.idl \
    html/HTMLTableCellElement.idl \
    html/HTMLTableColElement.idl \
    html/HTMLTableElement.idl \
    html/HTMLTableRowElement.idl \
    html/HTMLTableSectionElement.idl \
    html/HTMLTextAreaElement.idl \
    html/HTMLTitleElement.idl \
    html/HTMLTrackElement.idl \
    html/HTMLUListElement.idl \
    html/HTMLUnknownElement.idl \
    html/HTMLVideoElement.idl \
    html/ImageData.idl \
    html/MediaController.idl \
    html/MediaError.idl \
    html/TextMetrics.idl \
    html/TimeRanges.idl \
    html/ValidityState.idl \
    html/VoidCallback.idl \
    inspector/InjectedScriptHost.idl \
    inspector/InspectorFrontendHost.idl \
    inspector/JavaScriptCallFrame.idl \
    inspector/ScriptProfile.idl \
    inspector/ScriptProfileNode.idl \
    loader/appcache/DOMApplicationCache.idl \
    notifications/Notification.idl \
    notifications/NotificationCenter.idl \
    page/BarInfo.idl \
    page/Console.idl \
    page/Coordinates.idl \
    page/Crypto.idl \
    page/DOMSelection.idl \
    page/DOMWindow.idl \
    page/EventSource.idl \
    page/Geolocation.idl \
    page/Geoposition.idl \
    page/History.idl \
    page/Location.idl \
    page/MemoryInfo.idl \
    page/Navigator.idl \
    page/Performance.idl \
    page/PerformanceNavigation.idl \
    page/PerformanceTiming.idl \
    page/PositionCallback.idl \
    page/PositionError.idl \
    page/PositionErrorCallback.idl \
    page/Screen.idl \
    page/SpeechInputEvent.idl \
    page/SpeechInputResult.idl \
    page/SpeechInputResultList.idl \
    page/WebKitAnimation.idl \
    page/WebKitAnimationList.idl \
    page/WebKitPoint.idl \
    page/WorkerNavigator.idl \
    plugins/DOMPlugin.idl \
    plugins/DOMMimeType.idl \
    plugins/DOMPluginArray.idl \
    plugins/DOMMimeTypeArray.idl \
    storage/Database.idl \
    storage/DatabaseCallback.idl \
    storage/DatabaseSync.idl \
    storage/IDBAny.idl \
    storage/IDBCursor.idl \
    storage/IDBDatabaseError.idl \
    storage/IDBDatabaseException.idl \
    storage/IDBDatabase.idl \
    storage/IDBFactory.idl \
    storage/IDBIndex.idl \
    storage/IDBKey.idl \
    storage/IDBKeyRange.idl \
    storage/IDBObjectStore.idl \
    storage/IDBRequest.idl \
    storage/IDBTransaction.idl \
    storage/Storage.idl \
    storage/StorageEvent.idl \
    storage/StorageInfo.idl \
    storage/StorageInfoErrorCallback.idl \
    storage/StorageInfoQuotaCallback.idl \
    storage/StorageInfoUsageCallback.idl \
    storage/SQLError.idl \
    storage/SQLException.idl \
    storage/SQLResultSet.idl \
    storage/SQLResultSetRowList.idl \
    storage/SQLStatementCallback.idl \
    storage/SQLStatementErrorCallback.idl \
    storage/SQLTransaction.idl \
    storage/SQLTransactionCallback.idl \
    storage/SQLTransactionErrorCallback.idl \
    storage/SQLTransactionSync.idl \
    storage/SQLTransactionSyncCallback.idl \
    testing/Internals.idl \
    webaudio/AudioBuffer.idl \
    webaudio/AudioBufferSourceNode.idl \
    webaudio/AudioChannelMerger.idl \
    webaudio/AudioChannelSplitter.idl \
    webaudio/AudioContext.idl \
    webaudio/AudioDestinationNode.idl \
    webaudio/AudioGain.idl \
    webaudio/AudioGainNode.idl \
    webaudio/AudioListener.idl \
    webaudio/AudioNode.idl \
    webaudio/AudioPannerNode.idl \
    webaudio/AudioParam.idl \
    webaudio/AudioProcessingEvent.idl \
    webaudio/AudioSourceNode.idl \
    webaudio/ConvolverNode.idl \
    webaudio/DelayNode.idl \
    webaudio/DOMWindowWebAudio.idl \
    webaudio/HighPass2FilterNode.idl \
    webaudio/JavaScriptAudioNode.idl \
    webaudio/LowPass2FilterNode.idl \
    webaudio/RealtimeAnalyserNode.idl \
    websockets/CloseEvent.idl \
    websockets/DOMWindowWebSocket.idl \
    websockets/WebSocket.idl \
    workers/AbstractWorker.idl \
    workers/DedicatedWorkerContext.idl \
    workers/SharedWorker.idl \
    workers/SharedWorkerContext.idl \
    workers/Worker.idl \
    workers/WorkerContext.idl \
    workers/WorkerLocation.idl \
    xml/DOMParser.idl \
    xml/XMLHttpRequest.idl \
    xml/XMLHttpRequestException.idl \
    xml/XMLHttpRequestProgressEvent.idl \
    xml/XMLHttpRequestUpload.idl \
    xml/XMLSerializer.idl \
    xml/XPathNSResolver.idl \
    xml/XPathException.idl \
    xml/XPathExpression.idl \
    xml/XPathResult.idl \
    xml/XPathEvaluator.idl \
    xml/XSLTProcessor.idl

v8 {
  IDL_BINDINGS += \
    html/canvas/CanvasPixelArray.idl \
    storage/IDBVersionChangeEvent.idl \
    storage/IDBVersionChangeRequest.idl
}

contains(DEFINES, ENABLE_SVG=1) {
  IDL_BINDINGS += \
    svg/SVGZoomEvent.idl \
    svg/SVGAElement.idl \
    svg/SVGAltGlyphDefElement.idl \
    svg/SVGAltGlyphElement.idl \
    svg/SVGAltGlyphItemElement.idl \
    svg/SVGAngle.idl \
    svg/SVGAnimateColorElement.idl \
    svg/SVGAnimateMotionElement.idl \
    svg/SVGAnimatedAngle.idl \
    svg/SVGAnimatedBoolean.idl \
    svg/SVGAnimatedEnumeration.idl \
    svg/SVGAnimatedInteger.idl \
    svg/SVGAnimatedLength.idl \
    svg/SVGAnimatedLengthList.idl \
    svg/SVGAnimatedNumber.idl \
    svg/SVGAnimatedNumberList.idl \
    svg/SVGAnimatedPreserveAspectRatio.idl \
    svg/SVGAnimatedRect.idl \
    svg/SVGAnimatedString.idl \
    svg/SVGAnimatedTransformList.idl \
    svg/SVGAnimateElement.idl \
    svg/SVGAnimateTransformElement.idl \
    svg/SVGAnimationElement.idl \
    svg/SVGCircleElement.idl \
    svg/SVGClipPathElement.idl \
    svg/SVGColor.idl \
    svg/SVGComponentTransferFunctionElement.idl \
    svg/SVGCursorElement.idl \
    svg/SVGDefsElement.idl \
    svg/SVGDescElement.idl \
    svg/SVGDocument.idl \
    svg/SVGElement.idl \
    svg/SVGElementInstance.idl \
    svg/SVGElementInstanceList.idl \
    svg/SVGEllipseElement.idl \
    svg/SVGException.idl \
    svg/SVGFEBlendElement.idl \
    svg/SVGFEColorMatrixElement.idl \
    svg/SVGFEComponentTransferElement.idl \
    svg/SVGFECompositeElement.idl \
    svg/SVGFEConvolveMatrixElement.idl \
    svg/SVGFEDiffuseLightingElement.idl \
    svg/SVGFEDisplacementMapElement.idl \
    svg/SVGFEDistantLightElement.idl \
    svg/SVGFEDropShadowElement.idl \
    svg/SVGFEFloodElement.idl \
    svg/SVGFEFuncAElement.idl \
    svg/SVGFEFuncBElement.idl \
    svg/SVGFEFuncGElement.idl \
    svg/SVGFEFuncRElement.idl \
    svg/SVGFEGaussianBlurElement.idl \
    svg/SVGFEImageElement.idl \
    svg/SVGFEMergeElement.idl \
    svg/SVGFEMergeNodeElement.idl \
    svg/SVGFEMorphologyElement.idl \
    svg/SVGFEOffsetElement.idl \
    svg/SVGFEPointLightElement.idl \
    svg/SVGFESpecularLightingElement.idl \
    svg/SVGFESpotLightElement.idl \
    svg/SVGFETileElement.idl \
    svg/SVGFETurbulenceElement.idl \
    svg/SVGFilterElement.idl \
    svg/SVGFontElement.idl \
    svg/SVGFontFaceElement.idl \
    svg/SVGFontFaceFormatElement.idl \
    svg/SVGFontFaceNameElement.idl \
    svg/SVGFontFaceSrcElement.idl \
    svg/SVGFontFaceUriElement.idl \
    svg/SVGForeignObjectElement.idl \
    svg/SVGGElement.idl \
    svg/SVGGlyphElement.idl \
    svg/SVGGlyphRefElement.idl \
    svg/SVGGradientElement.idl \
    svg/SVGHKernElement.idl \
    svg/SVGImageElement.idl \
    svg/SVGLength.idl \
    svg/SVGLengthList.idl \
    svg/SVGLinearGradientElement.idl \
    svg/SVGLineElement.idl \
    svg/SVGMarkerElement.idl \
    svg/SVGMaskElement.idl \
    svg/SVGMatrix.idl \
    svg/SVGMetadataElement.idl \
    svg/SVGMissingGlyphElement.idl \
    svg/SVGMPathElement.idl \
    svg/SVGNumber.idl \
    svg/SVGNumberList.idl \
    svg/SVGPaint.idl \
    svg/SVGPathElement.idl \
    svg/SVGPathSegArcAbs.idl \
    svg/SVGPathSegArcRel.idl \
    svg/SVGPathSegClosePath.idl \
    svg/SVGPathSegCurvetoCubicAbs.idl \
    svg/SVGPathSegCurvetoCubicRel.idl \
    svg/SVGPathSegCurvetoCubicSmoothAbs.idl \
    svg/SVGPathSegCurvetoCubicSmoothRel.idl \
    svg/SVGPathSegCurvetoQuadraticAbs.idl \
    svg/SVGPathSegCurvetoQuadraticRel.idl \
    svg/SVGPathSegCurvetoQuadraticSmoothAbs.idl \
    svg/SVGPathSegCurvetoQuadraticSmoothRel.idl \
    svg/SVGPathSeg.idl \
    svg/SVGPathSegLinetoAbs.idl \
    svg/SVGPathSegLinetoHorizontalAbs.idl \
    svg/SVGPathSegLinetoHorizontalRel.idl \
    svg/SVGPathSegLinetoRel.idl \
    svg/SVGPathSegLinetoVerticalAbs.idl \
    svg/SVGPathSegLinetoVerticalRel.idl \
    svg/SVGPathSegList.idl \
    svg/SVGPathSegMovetoAbs.idl \
    svg/SVGPathSegMovetoRel.idl \
    svg/SVGPatternElement.idl \
    svg/SVGPoint.idl \
    svg/SVGPointList.idl \
    svg/SVGPolygonElement.idl \
    svg/SVGPolylineElement.idl \
    svg/SVGPreserveAspectRatio.idl \
    svg/SVGRadialGradientElement.idl \
    svg/SVGRectElement.idl \
    svg/SVGRect.idl \
    svg/SVGRenderingIntent.idl \
    svg/SVGScriptElement.idl \
    svg/SVGSetElement.idl \
    svg/SVGStopElement.idl \
    svg/SVGStringList.idl \
    svg/SVGStyleElement.idl \
    svg/SVGSVGElement.idl \
    svg/SVGSwitchElement.idl \
    svg/SVGSymbolElement.idl \
    svg/SVGTextContentElement.idl \
    svg/SVGTextElement.idl \
    svg/SVGTextPathElement.idl \
    svg/SVGTextPositioningElement.idl \
    svg/SVGTitleElement.idl \
    svg/SVGTransform.idl \
    svg/SVGTransformList.idl \
    svg/SVGTRefElement.idl \
    svg/SVGTSpanElement.idl \
    svg/SVGUnitTypes.idl \
    svg/SVGUseElement.idl \
    svg/SVGViewElement.idl \
    svg/SVGVKernElement.idl
}

contains(DEFINES, ENABLE_VIDEO_TRACK=1) {
  IDL_BINDINGS += \
    html/TextTrack.idl \
    html/TextTrackCue.idl \
    html/TextTrackCueList.idl \
    html/track/TextTrackList.idl \
    html/track/TrackEvent.idl \
}

v8: wrapperFactoryArg = --wrapperFactoryV8
else: wrapperFactoryArg = --wrapperFactory

mathmlnames.output = MathMLNames.cpp
mathmlnames.input = MATHML_NAMES
mathmlnames.script = $$PWD/dom/make_names.pl
mathmlnames.commands = perl -I$$PWD/bindings/scripts $$mathmlnames.script --tags $$PWD/mathml/mathtags.in --attrs $$PWD/mathml/mathattrs.in --extraDefines \"$${DEFINES}\" --preprocessor \"$${QMAKE_MOC} -E\" --factory $$wrapperFactoryArg --outputDir ${QMAKE_FUNC_FILE_OUT_PATH}
mathmlnames.extra_sources = MathMLElementFactory.cpp
GENERATORS += mathmlnames

# GENERATOR 5-C:
svgnames.output = SVGNames.cpp
svgnames.input = SVG_NAMES
svgnames.depends = $$PWD/svg/svgattrs.in
svgnames.script = $$PWD/dom/make_names.pl
svgnames.commands = perl -I$$PWD/bindings/scripts $$svgnames.script --tags $$PWD/svg/svgtags.in --attrs $$PWD/svg/svgattrs.in --extraDefines \"$${DEFINES}\" --preprocessor \"$${QMAKE_MOC} -E\" --factory $$wrapperFactoryArg --outputDir ${QMAKE_FUNC_FILE_OUT_PATH}
svgnames.extra_sources = SVGElementFactory.cpp
v8 {
    svgnames.extra_sources += V8SVGElementWrapperFactory.cpp
} else {
    svgnames.extra_sources += JSSVGElementWrapperFactory.cpp
}
GENERATORS += svgnames

# GENERATOR 5-D:
xlinknames.output = XLinkNames.cpp
xlinknames.script = $$PWD/dom/make_names.pl
xlinknames.commands = perl -I$$PWD/bindings/scripts $$xlinknames.script --attrs $$PWD/svg/xlinkattrs.in --preprocessor \"$${QMAKE_MOC} -E\" --outputDir ${QMAKE_FUNC_FILE_OUT_PATH}
xlinknames.input = XLINK_NAMES
GENERATORS += xlinknames

# GENERATOR 6-A:
cssprops.script = $$PWD/css/makeprop.pl
cssprops.output = CSSPropertyNames.cpp
cssprops.input = WALDOCSSPROPS
cssprops.commands = perl -ne \"print $1\" ${QMAKE_FILE_NAME} $${DASHBOARDSUPPORTCSSPROPERTIES} $${EXTRACSSPROPERTIES} > ${QMAKE_FUNC_FILE_OUT_PATH}/${QMAKE_FILE_BASE}.in && cd ${QMAKE_FUNC_FILE_OUT_PATH} && perl -I$$PWD/bindings/scripts $$cssprops.script --defines \"$${FEATURE_DEFINES_JAVASCRIPT}\" --preprocessor \"$${QMAKE_MOC} -E\" ${QMAKE_FILE_NAME} && $(DEL_FILE) ${QMAKE_FILE_BASE}.in ${QMAKE_FILE_BASE}.gperf
cssprops.depends = ${QMAKE_FILE_NAME} $${DASHBOARDSUPPORTCSSPROPERTIES} $${EXTRACSSPROPERTIES} $$cssprops.script
GENERATORS += cssprops

# GENERATOR 6-B:
cssvalues.script = $$PWD/css/makevalues.pl
cssvalues.output = CSSValueKeywords.cpp
cssvalues.input = WALDOCSSVALUES
cssvalues.commands = perl -ne \"print $1\" ${QMAKE_FILE_NAME} $$EXTRACSSVALUES > ${QMAKE_FUNC_FILE_OUT_PATH}/${QMAKE_FILE_BASE}.in && cd ${QMAKE_FUNC_FILE_OUT_PATH} && perl -I$$PWD/bindings/scripts $$cssvalues.script --defines \"$${FEATURE_DEFINES_JAVASCRIPT}\" --preprocessor \"$${QMAKE_MOC} -E\" ${QMAKE_FILE_NAME} && $(DEL_FILE) ${QMAKE_FILE_BASE}.in ${QMAKE_FILE_BASE}.gperf
cssvalues.depends = ${QMAKE_FILE_NAME} $${EXTRACSSVALUES} $$cssvalues.script
cssvalues.clean = ${QMAKE_FILE_OUT} ${QMAKE_FUNC_FILE_OUT_PATH}/${QMAKE_FILE_BASE}.h
GENERATORS += cssvalues

# GENERATOR 0: Resolve [Supplemental] dependency in IDLs
SUPPLEMENTAL_DEPENDENCY_FILE = supplemental_dependency.tmp
IDL_FILES_TMP = ${QMAKE_FUNC_FILE_OUT_PATH}/idl_files.tmp
RESOLVE_SUPPLEMENTAL_SCRIPT = $$PWD/bindings/scripts/resolve-supplemental.pl

resolveSupplemental.input = RESOLVE_SUPPLEMENTAL_SCRIPT # dummy input to fire this rule
resolveSupplemental.script = $$RESOLVE_SUPPLEMENTAL_SCRIPT
resolveSupplemental.commands = echo $(addprefix $${ROOT_WEBKIT_DIR}/Source/WebCore/, $$IDL_BINDINGS) | sed \'s/\\s/\\n/g\' > $$IDL_FILES_TMP && \
                               perl -I$$PWD/bindings/scripts $$resolveSupplemental.script \
                               --defines \"$${FEATURE_DEFINES_JAVASCRIPT}\" \
                               --idlFilesList $$IDL_FILES_TMP \
                               --supplementalDependencyFile ${QMAKE_FUNC_FILE_OUT_PATH}/$$SUPPLEMENTAL_DEPENDENCY_FILE \
                               --preprocessor \"$${QMAKE_MOC} -E\"
resolveSupplemental.output = $$SUPPLEMENTAL_DEPENDENCY_FILE
resolveSupplemental.add_output_to_sources = false
resolveSupplemental.depends = $$PWD/bindings/scripts/IDLParser.pm
GENERATORS += resolveSupplemental

# GENERATOR 1: Generate .h and .cpp from IDLs
generateBindings.input = IDL_BINDINGS
generateBindings.script = $$PWD/bindings/scripts/generate-bindings.pl
v8: generator = V8
else: generator = JS
generateBindings.commands = perl -I$$PWD/bindings/scripts $$generateBindings.script \
                            --defines \"$${FEATURE_DEFINES_JAVASCRIPT}\" \
                            --generator $$generator \
                            --include $$PWD/dom \
                            --include $$PWD/fileapi \
                            --include $$PWD/html \
                            --include $$PWD/xml \
                            --include $$PWD/svg \
                            --include $$PWD/storage \
                            --include $$PWD/css \
                            --include $$PWD/testing \
                            --include $$PWD/webaudio \
                            --include $$PWD/workers \
                            --outputDir ${QMAKE_FUNC_FILE_OUT_PATH} \
                            --supplementalDependencyFile ${QMAKE_FUNC_FILE_OUT_PATH}/$$SUPPLEMENTAL_DEPENDENCY_FILE \
                            --preprocessor \"$${QMAKE_MOC} -E\" ${QMAKE_FILE_NAME}
v8 {
    generateBindings.output = V8${QMAKE_FILE_BASE}.cpp
    generateBindings.depends = ${QMAKE_FUNC_FILE_OUT_PATH}/$$SUPPLEMENTAL_DEPENDENCY_FILE \
                               $$PWD/bindings/scripts/CodeGenerator.pm \
                               $$PWD/bindings/scripts/CodeGeneratorV8.pm \
                               $$PWD/bindings/scripts/IDLParser.pm \
                               $$PWD/bindings/scripts/IDLStructure.pm \
                               $$PWD/bindings/scripts/InFilesParser.pm \
                               $$PWD/bindings/scripts/preprocessor.pm
} else {
    generateBindings.output = JS${QMAKE_FILE_BASE}.cpp
    generateBindings.depends = ${QMAKE_FUNC_FILE_OUT_PATH}/$$SUPPLEMENTAL_DEPENDENCY_FILE \
                               $$PWD/bindings/scripts/CodeGenerator.pm \
                               $$PWD/bindings/scripts/CodeGeneratorJS.pm \
                               $$PWD/bindings/scripts/IDLParser.pm \
                               $$PWD/bindings/scripts/IDLStructure.pm \
                               $$PWD/bindings/scripts/InFilesParser.pm \
                               $$PWD/bindings/scripts/preprocessor.pm
}
GENERATORS += generateBindings

# GENERATOR 2: inspector idl compiler
inspectorValidate.output = InspectorProtocolVersion.h
inspectorValidate.input = INSPECTOR_JSON
inspectorValidate.script = $$PWD/inspector/generate-inspector-protocol-version
inspectorValidate.commands = python $$inspectorValidate.script -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
inspectorValidate.depends = $$PWD/inspector/generate-inspector-protocol-version
inspectorValidate.add_output_to_sources = false
GENERATORS += inspectorValidate

inspectorJSON.output = InspectorFrontend.cpp InspectorBackendDispatcher.cpp
inspectorJSON.input = INSPECTOR_JSON
inspectorJSON.script = $$PWD/inspector/CodeGeneratorInspector.py
inspectorJSON.commands = python $$inspectorJSON.script $$PWD/inspector/Inspector.json --output_h_dir ${QMAKE_FUNC_FILE_OUT_PATH} --output_cpp_dir ${QMAKE_FUNC_FILE_OUT_PATH} --defines \"$${FEATURE_DEFINES_JAVASCRIPT}\"
inspectorJSON.depends = $$inspectorJSON.script
GENERATORS += inspectorJSON

inspectorBackendStub.output = InspectorBackendStub.qrc
inspectorBackendStub.input = INSPECTOR_BACKEND_STUB_QRC
inspectorBackendStub.commands = $$QMAKE_COPY $$toSystemPath($$INSPECTOR_BACKEND_STUB_QRC) ${QMAKE_FUNC_FILE_OUT_PATH}$${QMAKE_DIR_SEP}InspectorBackendStub.qrc
inspectorBackendStub.add_output_to_sources = false
GENERATORS += inspectorBackendStub

# GENERATOR 2-a: inspector injected script source compiler
injectedScriptSource.output = InjectedScriptSource.h
injectedScriptSource.input = INJECTED_SCRIPT_SOURCE
injectedScriptSource.commands = perl $$PWD/inspector/xxd.pl InjectedScriptSource_js $$PWD/inspector/InjectedScriptSource.js ${QMAKE_FILE_OUT}
injectedScriptSource.add_output_to_sources = false
GENERATORS += injectedScriptSource

# GENERATOR 2-b: inspector debugger script source compiler
debuggerScriptSource.output = DebuggerScriptSource.h
debuggerScriptSource.input = DEBUGGER_SCRIPT_SOURCE
debuggerScriptSource.commands = perl $$PWD/inspector/xxd.pl DebuggerScriptSource_js ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
debuggerScriptSource.add_output_to_sources = false
GENERATORS += debuggerScriptSource

arrayBufferViewCustomScript.output = V8ArrayBufferViewCustomScript.h
arrayBufferViewCustomScript.input = ARRAY_BUFFER_VIEW_CUSTOM_SCRIPT_SOURCE
arrayBufferViewCustomScript.commands = perl $$PWD/inspector/xxd.pl V8ArrayBufferViewCustomScript_js ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
arrayBufferViewCustomScript.add_output_to_sources = false
GENERATORS += arrayBufferViewCustomScript

# GENERATOR 3: tokenizer (flex)
tokenizer.output = ${QMAKE_FILE_BASE}.cpp
tokenizer.input = TOKENIZER
tokenizer.script = $$PWD/css/maketokenizer
tokenizer.commands = flex -t < ${QMAKE_FILE_NAME} | perl $$tokenizer.script > ${QMAKE_FILE_OUT}
# tokenizer.cpp is included into CSSParser.cpp
tokenizer.add_output_to_sources = false
GENERATORS += tokenizer

# GENERATOR 4: CSS grammar
cssbison.output = ${QMAKE_FILE_BASE}.cpp
cssbison.input = CSSBISON
cssbison.script = $$PWD/css/makegrammar.pl
cssbison.commands = perl $$cssbison.script ${QMAKE_FILE_NAME} ${QMAKE_FUNC_FILE_OUT_PATH}/${QMAKE_FILE_BASE}
cssbison.depends = ${QMAKE_FILE_NAME}
GENERATORS += cssbison

# GENERATOR 5-A:
htmlnames.output = HTMLNames.cpp
htmlnames.input = HTML_NAMES
htmlnames.script = $$PWD/dom/make_names.pl
htmlnames.depends = $$PWD/html/HTMLAttributeNames.in
htmlnames.commands = perl -I$$PWD/bindings/scripts $$htmlnames.script --tags $$PWD/html/HTMLTagNames.in --attrs $$PWD/html/HTMLAttributeNames.in --extraDefines \"$${DEFINES}\" --preprocessor \"$${QMAKE_MOC} -E\"  --factory $$wrapperFactoryArg --outputDir ${QMAKE_FUNC_FILE_OUT_PATH}
htmlnames.extra_sources = HTMLElementFactory.cpp
v8 {
    htmlnames.extra_sources += V8HTMLElementWrapperFactory.cpp
} else {
    htmlnames.extra_sources += JSHTMLElementWrapperFactory.cpp
}
GENERATORS += htmlnames

# GENERATOR 5-B:
xmlnsnames.output = XMLNSNames.cpp
xmlnsnames.input = XMLNS_NAMES
xmlnsnames.script = $$PWD/dom/make_names.pl
xmlnsnames.commands = perl -I$$PWD/bindings/scripts $$xmlnsnames.script --attrs $$PWD/xml/xmlnsattrs.in --preprocessor \"$${QMAKE_MOC} -E\" --outputDir ${QMAKE_FUNC_FILE_OUT_PATH}
GENERATORS += xmlnsnames

# GENERATOR 5-C:
xmlnames.output = XMLNames.cpp
xmlnames.input = XML_NAMES
xmlnames.script = $$PWD/dom/make_names.pl
xmlnames.commands = perl -I$$PWD/bindings/scripts $$xmlnames.script --attrs $$PWD/xml/xmlattrs.in --preprocessor \"$${QMAKE_MOC} -E\" --outputDir ${QMAKE_FUNC_FILE_OUT_PATH}
GENERATORS += xmlnames

# GENERATOR 5-D:
fontnames.output = WebKitFontFamilyNames.cpp
fontnames.input = FONT_NAMES
fontnames.script = $$PWD/dom/make_names.pl
fontnames.commands = perl -I$$PWD/bindings/scripts $$fontnames.script --fonts $$FONT_NAMES --outputDir ${QMAKE_FUNC_FILE_OUT_PATH}
entities.depends = $$PWD/dom/make_names.pl $$FONT_NAMES
GENERATORS += fontnames

# GENERATOR 5-E:
eventfactory.output = EventFactory.cpp
eventfactory.input = EVENT_FACTORY
eventfactory.script = $$PWD/dom/make_event_factory.pl
eventfactory.commands = perl -I$$PWD/bindings/scripts $$eventfactory.script --input $$EVENT_FACTORY --outputDir ${QMAKE_FUNC_FILE_OUT_PATH}
eventfactory.depends = $$PWD/dom/make_event_factory.pl $$EVENT_FACTORY
GENERATORS += eventfactory

# GENERATOR 5-F:
eventtargetfactory.output = EventTargetInterfaces.h
eventtargetfactory.add_output_to_sources = false
eventtargetfactory.input = EVENT_TARGET_FACTORY
eventtargetfactory.script = $$PWD/dom/make_event_factory.pl
eventtargetfactory.commands = perl -I$$PWD/bindings/scripts $$eventfactory.script --input $$EVENT_TARGET_FACTORY --outputDir ${QMAKE_FUNC_FILE_OUT_PATH}
eventtargetfactory.depends = $$PWD/dom/make_event_factory.pl $$EVENT_TARGET_FACTORY
GENERATORS += eventtargetfactory

# GENERATOR 5-G:
exceptioncodedescription.output = ExceptionCodeDescription.cpp
exceptioncodedescription.input = DOM_EXCEPTIONS
exceptioncodedescription.script = $$PWD/dom/make_dom_exceptions.pl
exceptioncodedescription.commands = perl -I$$PWD/bindings/scripts $$exceptioncodedescription.script --input $$DOM_EXCEPTIONS --outputDir ${QMAKE_FUNC_FILE_OUT_PATH}
exceptioncodedescription.depends = $$PWD/dom/make_dom_exceptions.pl $$DOM_EXCEPTIONS
GENERATORS += exceptioncodedescription

# GENERATOR 8-A:
entities.output = HTMLEntityTable.cpp
entities.input = HTML_ENTITIES
entities.script = $$PWD/html/parser/create-html-entity-table
entities.commands = python $$entities.script -o ${QMAKE_FILE_OUT} $$HTML_ENTITIES
entities.clean = ${QMAKE_FILE_OUT}
entities.depends = $$PWD/html/parser/create-html-entity-table
GENERATORS += entities

# GENERATOR 8-B:
colordata.output = ColorData.cpp
colordata.input = COLORDATA_GPERF
colordata.script = $$PWD/make-hash-tools.pl
colordata.commands = perl $$colordata.script ${QMAKE_FUNC_FILE_OUT_PATH} $$COLORDATA_GPERF
colordata.clean = ${QMAKE_FILE_OUT}
colordata.depends = $$PWD/make-hash-tools.pl
GENERATORS += colordata

contains(DEFINES, ENABLE_XSLT=1) {
    # GENERATOR 8-C:
    xmlviewercss.output = XMLViewerCSS.h
    xmlviewercss.input = XMLVIEWER_CSS
    xmlviewercss.script = $$PWD/inspector/xxd.pl
    xmlviewercss.commands = perl $$xmlviewercss.script XMLViewer_css $$XMLVIEWER_CSS ${QMAKE_FILE_OUT}
    xmlviewercss.clean = ${QMAKE_FILE_OUT}
    xmlviewercss.depends = $$PWD/inspector/xxd.pl
    xmlviewercss.add_output_to_sources = false
    GENERATORS += xmlviewercss

    # GENERATOR 8-D:
    xmlviewerjs.output = XMLViewerJS.h
    xmlviewerjs.input = XMLVIEWER_JS
    xmlviewerjs.script = $$PWD/inspector/xxd.pl
    xmlviewerjs.commands = perl $$xmlviewerjs.script XMLViewer_js $$XMLVIEWER_JS ${QMAKE_FILE_OUT}
    xmlviewerjs.clean = ${QMAKE_FILE_OUT}
    xmlviewerjs.depends = $$PWD/inspector/xxd.pl
    xmlviewerjs.add_output_to_sources = false
    GENERATORS += xmlviewerjs
}

# GENERATOR 9:
stylesheets.script = $$PWD/css/make-css-file-arrays.pl
stylesheets.output = UserAgentStyleSheetsData.cpp
stylesheets.input = stylesheets.script
stylesheets.commands = perl $$stylesheets.script ${QMAKE_FUNC_FILE_OUT_PATH}/UserAgentStyleSheets.h ${QMAKE_FILE_OUT} $$STYLESHEETS_EMBED
stylesheets.depends = $$STYLESHEETS_EMBED
stylesheets.clean = ${QMAKE_FILE_OUT} ${QMAKE_FUNC_FILE_OUT_PATH}/UserAgentStyleSheets.h
GENERATORS += stylesheets

# GENERATOR 10: XPATH grammar
xpathbison.output = ${QMAKE_FILE_BASE}.cpp
xpathbison.input = XPATHBISON
xpathbison.commands = bison -d -p xpathyy ${QMAKE_FILE_NAME} -o ${QMAKE_FUNC_FILE_OUT_PATH}/${QMAKE_FILE_BASE}.tab.c && $(MOVE) ${QMAKE_FUNC_FILE_OUT_PATH}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.c ${QMAKE_FUNC_FILE_OUT_PATH}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.cpp && $(MOVE) ${QMAKE_FUNC_FILE_OUT_PATH}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.h ${QMAKE_FUNC_FILE_OUT_PATH}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.h
xpathbison.depends = ${QMAKE_FILE_NAME}
GENERATORS += xpathbison

# GENERATOR 11: WebKit Version
# The appropriate Apple-maintained Version.xcconfig file for WebKit version information is in Source/WebKit/mac/Configurations/.
webkitversion.script = $$PWD/../WebKit/scripts/generate-webkitversion.pl
webkitversion.output = WebKitVersion.h
webkitversion.input = webkitversion.script
webkitversion.commands = perl $$webkitversion.script --config $$PWD/../WebKit/mac/Configurations/Version.xcconfig --outputDir ${QMAKE_FUNC_FILE_OUT_PATH}/
webkitversion.clean = ${QMAKE_FUNC_FILE_OUT_PATH}/WebKitVersion.h
webkitversion.add_output_to_sources = false
GENERATORS += webkitversion

# Stolen from JavaScriptCore, needed for YARR
v8 {
    retgen.output = RegExpJitTables.h
    retgen.script = $$PWD/../JavaScriptCore/create_regex_tables
    retgen.input = retgen.script
    retgen.commands = python $$retgen.script > ${QMAKE_FILE_OUT}
    GENERATORS += retgen
}
