# Derived source generators

include(features.pri)

isEmpty(WC_GENERATED_SOURCES_DIR):WC_GENERATED_SOURCES_DIR = generated

## Derived source generators
MATHML_NAMES = $$PWD/mathml/mathtags.in

SVG_NAMES = $$PWD/svg/svgtags.in

XLINK_NAMES = $$PWD/svg/xlinkattrs.in

TOKENIZER = $$PWD/css/tokenizer.flex

CSSBISON = $$PWD/css/CSSGrammar.y

contains(DEFINES, ENABLE_XSLT=1) {
    contains(DEFINES, WTF_USE_LIBXML2=1) {
        XMLVIEWER_CSS = $$PWD/xml/XMLViewer.css
        XMLVIEWER_JS = $$PWD/xml/XMLViewer.js
    }
}

FONT_NAMES = $$PWD/css/WebKitFontFamilyNames.in

HTML_NAMES = $$PWD/html/HTMLTagNames.in

XML_NAMES = $$PWD/xml/xmlattrs.in

XMLNS_NAMES = $$PWD/xml/xmlnsattrs.in

HTML_ENTITIES = $$PWD/html/parser/HTMLEntityNames.in

EVENT_FACTORY = $$PWD/dom/EventFactory.in

COLORDATA_GPERF = $$PWD/platform/ColorData.gperf

WALDOCSSPROPS = $$PWD/css/CSSPropertyNames.in

WALDOCSSVALUES = $$PWD/css/CSSValueKeywords.in

INSPECTOR_JSON = $$PWD/inspector/Inspector.json

INSPECTOR_BACKEND_STUB_QRC = $$PWD/inspector/front-end/InspectorBackendStub.qrc

INJECTED_SCRIPT_SOURCE = $$PWD/inspector/InjectedScriptSource.js

DEBUGGER_SCRIPT_SOURCE = $$PWD/bindings/v8/DebuggerScript.js

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
    $$PWD/css/themeQtNoListboxes.css

v8 {
    IDL_BINDINGS += \
        html/canvas/CanvasPixelArray.idl \
        storage/IDBVersionChangeEvent.idl \
        storage/IDBVersionChangeRequest.idl
}

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
    dom/DataTransferItems.idl \
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
    dom/LocalMediaStream.idl \
    dom/MediaStream.idl \
    dom/MediaStreamList.idl \
    dom/MediaStreamTrack.idl \
    dom/MediaStreamTrackList.idl \
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
    html/canvas/WebGLFramebuffer.idl \
    html/canvas/WebGLProgram.idl \
    html/canvas/WebGLRenderbuffer.idl \
    html/canvas/WebGLRenderingContext.idl \
    html/canvas/WebGLShader.idl \
    html/canvas/Int16Array.idl \
    html/canvas/WebGLTexture.idl \
    html/canvas/WebGLUniformLocation.idl \
    html/canvas/WebGLVertexArrayObjectOES.idl \
    html/canvas/WebKitLoseContext.idl \
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
    p2p/MediaStreamEvent.idl \
    p2p/PeerConnection.idl \
    p2p/SignalingCallback.idl \
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
    page/NavigatorUserMediaError.idl \
    page/NavigatorUserMediaErrorCallback.idl \
    page/NavigatorUserMediaSuccessCallback.idl \
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
    webaudio/HighPass2FilterNode.idl \
    webaudio/JavaScriptAudioNode.idl \
    webaudio/LowPass2FilterNode.idl \
    webaudio/RealtimeAnalyserNode.idl \
    websockets/CloseEvent.idl \
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
    html/MutableTextTrack.idl \
    html/TextTrack.idl \
    html/TextTrackCue.idl \
    html/TextTrackCueList.idl \
}

v8: wrapperFactoryArg = --wrapperFactoryV8
else: wrapperFactoryArg = --wrapperFactory

mathmlnames.output = $${WC_GENERATED_SOURCES_DIR}/MathMLNames.cpp
mathmlnames.input = MATHML_NAMES
mathmlnames.wkScript = $$PWD/dom/make_names.pl
mathmlnames.commands = perl -I$$PWD/bindings/scripts $$mathmlnames.wkScript --tags $$PWD/mathml/mathtags.in --attrs $$PWD/mathml/mathattrs.in --extraDefines \"$${DEFINES}\" --preprocessor \"$${QMAKE_MOC} -E\" --factory $$wrapperFactoryArg --outputDir $$WC_GENERATED_SOURCES_DIR
mathmlnames.wkExtraSources = $${WC_GENERATED_SOURCES_DIR}/MathMLElementFactory.cpp 
addExtraCompiler(mathmlnames)

# GENERATOR 5-C:
svgnames.output = $${WC_GENERATED_SOURCES_DIR}/SVGNames.cpp
svgnames.input = SVG_NAMES
svgnames.depends = $$PWD/svg/svgattrs.in
svgnames.wkScript = $$PWD/dom/make_names.pl
svgnames.commands = perl -I$$PWD/bindings/scripts $$svgnames.wkScript --tags $$PWD/svg/svgtags.in --attrs $$PWD/svg/svgattrs.in --extraDefines \"$${DEFINES}\" --preprocessor \"$${QMAKE_MOC} -E\" --factory $$wrapperFactoryArg --outputDir $$WC_GENERATED_SOURCES_DIR
svgnames.wkExtraSources = $${WC_GENERATED_SOURCES_DIR}/SVGElementFactory.cpp
v8 {
    svgnames.wkExtraSources += $${WC_GENERATED_SOURCES_DIR}/V8SVGElementWrapperFactory.cpp
} else {
    svgnames.wkExtraSources += $${WC_GENERATED_SOURCES_DIR}/JSSVGElementWrapperFactory.cpp
}
addExtraCompiler(svgnames)

# GENERATOR 5-D:
xlinknames.output = $${WC_GENERATED_SOURCES_DIR}/XLinkNames.cpp
xlinknames.wkScript = $$PWD/dom/make_names.pl
xlinknames.commands = perl -I$$PWD/bindings/scripts $$xlinknames.wkScript --attrs $$PWD/svg/xlinkattrs.in --preprocessor \"$${QMAKE_MOC} -E\" --outputDir $$WC_GENERATED_SOURCES_DIR
xlinknames.input = XLINK_NAMES
addExtraCompiler(xlinknames)

# GENERATOR 6-A:
cssprops.wkScript = $$PWD/css/makeprop.pl
cssprops.output = $${WC_GENERATED_SOURCES_DIR}/CSSPropertyNames.cpp
cssprops.input = WALDOCSSPROPS
cssprops.commands = perl -ne \"print $1\" ${QMAKE_FILE_NAME} $${DASHBOARDSUPPORTCSSPROPERTIES} $${EXTRACSSPROPERTIES} > $${WC_GENERATED_SOURCES_DIR}/${QMAKE_FILE_BASE}.in && cd $$WC_GENERATED_SOURCES_DIR && perl -I$$PWD/bindings/scripts $$cssprops.wkScript --defines \"$${FEATURE_DEFINES_JAVASCRIPT}\" --preprocessor \"$${QMAKE_MOC} -E\" ${QMAKE_FILE_NAME} && $(DEL_FILE) ${QMAKE_FILE_BASE}.in ${QMAKE_FILE_BASE}.gperf
cssprops.depends = ${QMAKE_FILE_NAME} $${DASHBOARDSUPPORTCSSPROPERTIES} $${EXTRACSSPROPERTIES} $$cssprops.wkScript
addExtraCompiler(cssprops)

# GENERATOR 6-B:
cssvalues.wkScript = $$PWD/css/makevalues.pl
cssvalues.output = $${WC_GENERATED_SOURCES_DIR}/CSSValueKeywords.cpp
cssvalues.input = WALDOCSSVALUES
cssvalues.commands = perl -ne \"print $1\" ${QMAKE_FILE_NAME} $$EXTRACSSVALUES > $${WC_GENERATED_SOURCES_DIR}/${QMAKE_FILE_BASE}.in && cd $$WC_GENERATED_SOURCES_DIR && perl -I$$PWD/bindings/scripts $$cssvalues.wkScript --defines \"$${FEATURE_DEFINES_JAVASCRIPT}\" --preprocessor \"$${QMAKE_MOC} -E\" ${QMAKE_FILE_NAME} && $(DEL_FILE) ${QMAKE_FILE_BASE}.in ${QMAKE_FILE_BASE}.gperf
cssvalues.depends = ${QMAKE_FILE_NAME} $${EXTRACSSVALUES} $$cssvalues.wkScript
cssvalues.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_WC_GENERATED_SOURCES_DIR}/${QMAKE_FILE_BASE}.h
addExtraCompiler(cssvalues)

# GENERATOR 1: IDL compiler
idl.input = IDL_BINDINGS
idl.wkScript = $$PWD/bindings/scripts/generate-bindings.pl
v8: generator = V8
else: generator = JS
idl.commands = perl -I$$PWD/bindings/scripts $$idl.wkScript \
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
               --outputDir $$WC_GENERATED_SOURCES_DIR \
               --preprocessor \"$${QMAKE_MOC} -E\" ${QMAKE_FILE_NAME}
v8 {
    idl.output = $${WC_GENERATED_SOURCES_DIR}/V8${QMAKE_FILE_BASE}.cpp
    idl.depends = $$PWD/bindings/scripts/CodeGenerator.pm \
                  $$PWD/bindings/scripts/CodeGeneratorV8.pm \
                  $$PWD/bindings/scripts/IDLParser.pm \
                  $$PWD/bindings/scripts/IDLStructure.pm \
                  $$PWD/bindings/scripts/InFilesParser.pm \
                  $$PWD/bindings/scripts/preprocessor.pm
} else {
    idl.output = $${WC_GENERATED_SOURCES_DIR}/JS${QMAKE_FILE_BASE}.cpp
    idl.depends = $$PWD/bindings/scripts/CodeGenerator.pm \
                  $$PWD/bindings/scripts/CodeGeneratorJS.pm \
                  $$PWD/bindings/scripts/IDLParser.pm \
                  $$PWD/bindings/scripts/IDLStructure.pm \
                  $$PWD/bindings/scripts/InFilesParser.pm \
                  $$PWD/bindings/scripts/preprocessor.pm
}
addExtraCompiler(idl)

# GENERATOR 2: inspector idl compiler
inspectorValidate.output = $${WC_GENERATED_SOURCES_DIR}/InspectorProtocolVersion.h
inspectorValidate.input = INSPECTOR_JSON
inspectorValidate.wkScript = $$PWD/inspector/generate-inspector-protocol-version
inspectorValidate.commands = python $$inspectorValidate.wkScript -o $${WC_GENERATED_SOURCES_DIR}/InspectorProtocolVersion.h $$PWD/inspector/Inspector.json
inspectorValidate.depends = $$PWD/inspector/generate-inspector-protocol-version
inspectorValidate.wkAddOutputToSources = false
addExtraCompiler(inspectorValidate)

inspectorJSON.output = $${WC_GENERATED_SOURCES_DIR}/Inspector.idl
inspectorJSON.input = INSPECTOR_JSON
inspectorJSON.wkScript = $$PWD/inspector/generate-inspector-idl
inspectorJSON.commands = python $$inspectorJSON.wkScript -o $${WC_GENERATED_SOURCES_DIR}/Inspector.idl $$PWD/inspector/Inspector.json
inspectorJSON.depends = $$PWD/inspector/generate-inspector-idl
inspectorJSON.wkAddOutputToSources = false
addExtraCompiler(inspectorJSON)
inspectorJSON.variable_out = INSPECTOR_JSON_OUTPUT

inspectorIDL.output = $${WC_GENERATED_SOURCES_DIR}/InspectorFrontend.cpp $${WC_GENERATED_SOURCES_DIR}/InspectorBackendDispatcher.cpp
inspectorIDL.input = INSPECTOR_JSON_OUTPUT
inspectorIDL.wkScript = $$PWD/bindings/scripts/generate-bindings.pl
inspectorIDL.commands = perl -I$$PWD/bindings/scripts -I$$PWD/inspector $$inspectorIDL.wkScript --defines \"$${FEATURE_DEFINES_JAVASCRIPT}\" --generator Inspector --outputDir $$WC_GENERATED_SOURCES_DIR --preprocessor \"$${QMAKE_MOC} -E\" ${QMAKE_FILE_NAME}
inspectorIDL.depends = $$PWD/bindings/scripts/CodeGenerator.pm \
              $$PWD/inspector/CodeGeneratorInspector.pm \
              $$PWD/bindings/scripts/IDLParser.pm \
              $$PWD/bindings/scripts/IDLStructure.pm \
              $$PWD/bindings/scripts/InFilesParser.pm \
              $$PWD/bindings/scripts/preprocessor.pm \
              $$PWD/inspector/Inspector.json \
              $$PWD/inspector/generate-inspector-idl
inspectorIDL.wkExtraSources = $$inspectorIDL.output
addExtraCompiler(inspectorIDL)

inspectorBackendStub.output = generated/InspectorBackendStub.qrc
inspectorBackendStub.input = INSPECTOR_BACKEND_STUB_QRC
inspectorBackendStub.tempNames = $$INSPECTOR_BACKEND_STUB_QRC $${WC_GENERATED_SOURCES_DIR}/InspectorBackendStub.qrc
inspectorBackendStub.commands = $$QMAKE_COPY $$replace(inspectorBackendStub.tempNames, "/", $$QMAKE_DIR_SEP)
inspectorBackendStub.wkAddOutputToSources = false
addExtraCompiler(inspectorBackendStub)

# GENERATOR 2-a: inspector injected script source compiler
injectedScriptSource.output = $${WC_GENERATED_SOURCES_DIR}/InjectedScriptSource.h
injectedScriptSource.input = INJECTED_SCRIPT_SOURCE
injectedScriptSource.commands = perl $$PWD/inspector/xxd.pl InjectedScriptSource_js $$PWD/inspector/InjectedScriptSource.js  $${WC_GENERATED_SOURCES_DIR}/InjectedScriptSource.h
injectedScriptSource.wkAddOutputToSources = false
addExtraCompiler(injectedScriptSource)

# GENERATOR 2-b: inspector debugger script source compiler
debuggerScriptSource.output = $${WC_GENERATED_SOURCES_DIR}/DebuggerScriptSource.h
debuggerScriptSource.input = DEBUGGER_SCRIPT_SOURCE
debuggerScriptSource.commands = perl $$PWD/inspector/xxd.pl DebuggerScriptSource_js $$PWD/bindings/v8/DebuggerScript.js  $${WC_GENERATED_SOURCES_DIR}/DebuggerScriptSource.h
debuggerScriptSource.wkAddOutputToSources = false
addExtraCompiler(debuggerScriptSource)

# GENERATOR 3: tokenizer (flex)
tokenizer.output = $${WC_GENERATED_SOURCES_DIR}/${QMAKE_FILE_BASE}.cpp
tokenizer.input = TOKENIZER
tokenizer.wkScript = $$PWD/css/maketokenizer
tokenizer.commands = flex -t < ${QMAKE_FILE_NAME} | perl $$tokenizer.wkScript > ${QMAKE_FILE_OUT}
# tokenizer.cpp is included into CSSParser.cpp
tokenizer.wkAddOutputToSources = false
addExtraCompiler(tokenizer)

# GENERATOR 4: CSS grammar
cssbison.output = $${WC_GENERATED_SOURCES_DIR}/${QMAKE_FILE_BASE}.cpp
cssbison.input = CSSBISON
cssbison.wkScript = $$PWD/css/makegrammar.pl
cssbison.commands = perl $$cssbison.wkScript ${QMAKE_FILE_NAME} $${WC_GENERATED_SOURCES_DIR}/${QMAKE_FILE_BASE}
cssbison.depends = ${QMAKE_FILE_NAME}
addExtraCompiler(cssbison)

# GENERATOR 5-A:
htmlnames.output = $${WC_GENERATED_SOURCES_DIR}/HTMLNames.cpp
htmlnames.input = HTML_NAMES
htmlnames.wkScript = $$PWD/dom/make_names.pl
htmlnames.depends = $$PWD/html/HTMLAttributeNames.in
htmlnames.commands = perl -I$$PWD/bindings/scripts $$htmlnames.wkScript --tags $$PWD/html/HTMLTagNames.in --attrs $$PWD/html/HTMLAttributeNames.in --extraDefines \"$${DEFINES}\" --preprocessor \"$${QMAKE_MOC} -E\"  --factory $$wrapperFactoryArg --outputDir $$WC_GENERATED_SOURCES_DIR
htmlnames.wkExtraSources = $${WC_GENERATED_SOURCES_DIR}/HTMLElementFactory.cpp
v8 {
    htmlnames.wkExtraSources += $${WC_GENERATED_SOURCES_DIR}/V8HTMLElementWrapperFactory.cpp
} else {
    htmlnames.wkExtraSources += $${WC_GENERATED_SOURCES_DIR}/JSHTMLElementWrapperFactory.cpp
}
addExtraCompiler(htmlnames)

# GENERATOR 5-B:
xmlnsnames.output = $${WC_GENERATED_SOURCES_DIR}/XMLNSNames.cpp
xmlnsnames.input = XMLNS_NAMES
xmlnsnames.wkScript = $$PWD/dom/make_names.pl
xmlnsnames.commands = perl -I$$PWD/bindings/scripts $$xmlnsnames.wkScript --attrs $$PWD/xml/xmlnsattrs.in --preprocessor \"$${QMAKE_MOC} -E\" --outputDir $$WC_GENERATED_SOURCES_DIR
addExtraCompiler(xmlnsnames)

# GENERATOR 5-C:
xmlnames.output = $${WC_GENERATED_SOURCES_DIR}/XMLNames.cpp
xmlnames.input = XML_NAMES
xmlnames.wkScript = $$PWD/dom/make_names.pl
xmlnames.commands = perl -I$$PWD/bindings/scripts $$xmlnames.wkScript --attrs $$PWD/xml/xmlattrs.in --preprocessor \"$${QMAKE_MOC} -E\" --outputDir $$WC_GENERATED_SOURCES_DIR
addExtraCompiler(xmlnames)

# GENERATOR 5-D:
fontnames.output = $${WC_GENERATED_SOURCES_DIR}/WebKitFontFamilyNames.cpp
fontnames.input = FONT_NAMES
fontnames.wkScript = $$PWD/dom/make_names.pl
fontnames.commands = perl -I$$PWD/bindings/scripts $$fontnames.wkScript --fonts $$FONT_NAMES --outputDir $$WC_GENERATED_SOURCES_DIR
entities.depends = $$PWD/dom/make_names.pl $$FONT_NAMES
addExtraCompiler(fontnames)

# GENERATOR 5-E:
eventfactory.output = $${WC_GENERATED_SOURCES_DIR}/EventFactory.cpp
eventfactory.input = EVENT_FACTORY
eventfactory.wkScript = $$PWD/dom/make_event_factory.pl
eventfactory.commands = perl -I$$PWD/bindings/scripts $$eventfactory.wkScript --events $$EVENT_FACTORY --outputDir $$WC_GENERATED_SOURCES_DIR
eventfactory.depends = $$PWD/dom/make_event_factory.pl $$EVENT_FACTORY
addExtraCompiler(eventfactory)

# GENERATOR 8-A:
entities.output = $${WC_GENERATED_SOURCES_DIR}/HTMLEntityTable.cpp
entities.input = HTML_ENTITIES
entities.wkScript = $$PWD/html/parser/create-html-entity-table
entities.commands = python $$entities.wkScript -o $${WC_GENERATED_SOURCES_DIR}/HTMLEntityTable.cpp $$HTML_ENTITIES
entities.clean = ${QMAKE_FILE_OUT}
entities.depends = $$PWD/html/parser/create-html-entity-table
addExtraCompiler(entities)

# GENERATOR 8-B:
colordata.output = $${WC_GENERATED_SOURCES_DIR}/ColorData.cpp
colordata.input = COLORDATA_GPERF
colordata.wkScript = $$PWD/make-hash-tools.pl
colordata.commands = perl $$colordata.wkScript $${WC_GENERATED_SOURCES_DIR} $$COLORDATA_GPERF
colordata.clean = ${QMAKE_FILE_OUT}
colordata.depends = $$PWD/make-hash-tools.pl
addExtraCompiler(colordata)

contains(DEFINES, ENABLE_XSLT=1) {
contains(DEFINES, WTF_USE_LIBXML2=1) {
# GENERATOR 8-C:
xmlviewercss.output = $${WC_GENERATED_SOURCES_DIR}/XMLViewerCSS.h
xmlviewercss.input = XMLVIEWER_CSS
xmlviewercss.wkScript = $$PWD/inspector/xxd.pl
xmlviewercss.commands = perl $$xmlviewercss.wkScript XMLViewer_css $$XMLVIEWER_CSS $${WC_GENERATED_SOURCES_DIR}/XMLViewerCSS.h
xmlviewercss.clean = ${QMAKE_FILE_OUT}
xmlviewercss.depends = $$PWD/inspector/xxd.pl
xmlviewercss.wkAddOutputToSources = false
addExtraCompiler(xmlviewercss)

# GENERATOR 8-D:
xmlviewerjs.output = $${WC_GENERATED_SOURCES_DIR}/XMLViewerJS.h
xmlviewerjs.input = XMLVIEWER_JS
xmlviewerjs.wkScript = $$PWD/inspector/xxd.pl
xmlviewerjs.commands = perl $$xmlviewerjs.wkScript XMLViewer_js $$XMLVIEWER_JS $${WC_GENERATED_SOURCES_DIR}/XMLViewerJS.h
xmlviewerjs.clean = ${QMAKE_FILE_OUT}
xmlviewerjs.depends = $$PWD/inspector/xxd.pl
xmlviewerjs.wkAddOutputToSources = false
addExtraCompiler(xmlviewerjs)
}
}

# GENERATOR 9:
stylesheets.wkScript = $$PWD/css/make-css-file-arrays.pl
stylesheets.output = $${WC_GENERATED_SOURCES_DIR}/UserAgentStyleSheetsData.cpp
stylesheets.input = stylesheets.wkScript
stylesheets.commands = perl $$stylesheets.wkScript $${WC_GENERATED_SOURCES_DIR}/UserAgentStyleSheets.h ${QMAKE_FILE_OUT} $$STYLESHEETS_EMBED
stylesheets.depends = $$STYLESHEETS_EMBED
stylesheets.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_WC_GENERATED_SOURCES_DIR}/UserAgentStyleSheets.h
addExtraCompiler(stylesheets, $${WC_GENERATED_SOURCES_DIR}/UserAgentStyleSheets.h)

# GENERATOR 10: XPATH grammar
xpathbison.output = $${WC_GENERATED_SOURCES_DIR}/${QMAKE_FILE_BASE}.cpp
xpathbison.input = XPATHBISON
xpathbison.commands = bison -d -p xpathyy ${QMAKE_FILE_NAME} -o $${WC_GENERATED_SOURCES_DIR}/${QMAKE_FILE_BASE}.tab.c && $(MOVE) $${WC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.c $${WC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.cpp && $(MOVE) $${WC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.h $${WC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.h
xpathbison.depends = ${QMAKE_FILE_NAME}
addExtraCompiler(xpathbison)

# GENERATOR 11: WebKit Version
# The appropriate Apple-maintained Version.xcconfig file for WebKit version information is in Source/WebKit/mac/Configurations/.
webkitversion.wkScript = $$PWD/../WebKit/scripts/generate-webkitversion.pl
webkitversion.output = $${WC_GENERATED_SOURCES_DIR}/WebKitVersion.h
webkitversion.input = webkitversion.wkScript
webkitversion.commands = perl $$webkitversion.wkScript --config $$PWD/../WebKit/mac/Configurations/Version.xcconfig --outputDir $${WC_GENERATED_SOURCES_DIR}/
webkitversion.clean = ${QMAKE_VAR_WC_GENERATED_SOURCES_DIR}/WebKitVersion.h
webkitversion.wkAddOutputToSources = false
addExtraCompiler(webkitversion)
