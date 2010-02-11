CONFIG(standalone_package) {
    isEmpty(WC_GENERATED_SOURCES_DIR):WC_GENERATED_SOURCES_DIR = $$PWD/generated
} else {
    isEmpty(WC_GENERATED_SOURCES_DIR):WC_GENERATED_SOURCES_DIR = generated
}

## Define default features macros for optional components
## (look for defs in config.h and included files!)
# Try to locate sqlite3 source
CONFIG(QTDIR_build) {
    SQLITE3SRCDIR = $$QT_SOURCE_TREE/src/3rdparty/sqlite/
} else {
    SQLITE3SRCDIR = $$(SQLITE3SRCDIR)
    isEmpty(SQLITE3SRCDIR) {
        SQLITE3SRCDIR = $$[QT_INSTALL_PREFIX]/src/3rdparty/sqlite/
    }
}

contains(DEFINES, ENABLE_SINGLE_THREADED=1) {
    DEFINES+=ENABLE_DATABASE=0 ENABLE_DOM_STORAGE=0 ENABLE_ICONDATABASE=0 ENABLE_WORKERS=0 ENABLE_SHARED_WORKERS=0
}

# turn off SQLITE support if we do not have sqlite3 available
!CONFIG(QTDIR_build):win32-*:!exists( $${SQLITE3SRCDIR}/sqlite3.c ): DEFINES += ENABLE_SQLITE=0 ENABLE_DATABASE=0 ENABLE_WORKERS=0 ENABLE_SHARED_WORKERS=0 ENABLE_ICONDATABASE=0 ENABLE_OFFLINE_WEB_APPLICATIONS=0 ENABLE_DOM_STORAGE=0

!contains(DEFINES, ENABLE_JAVASCRIPT_DEBUGGER=.): DEFINES += ENABLE_JAVASCRIPT_DEBUGGER=1
!contains(DEFINES, ENABLE_DATABASE=.): DEFINES += ENABLE_DATABASE=1
!contains(DEFINES, ENABLE_EVENTSOURCE=.): DEFINES += ENABLE_EVENTSOURCE=1
!contains(DEFINES, ENABLE_OFFLINE_WEB_APPLICATIONS=.): DEFINES += ENABLE_OFFLINE_WEB_APPLICATIONS=1
!contains(DEFINES, ENABLE_DOM_STORAGE=.): DEFINES += ENABLE_DOM_STORAGE=1
!contains(DEFINES, ENABLE_ICONDATABASE=.): DEFINES += ENABLE_ICONDATABASE=1
!contains(DEFINES, ENABLE_CHANNEL_MESSAGING=.): DEFINES += ENABLE_CHANNEL_MESSAGING=1
!contains(DEFINES, ENABLE_ORIENTATION_EVENTS=.): DEFINES += ENABLE_ORIENTATION_EVENTS=0

# turn on SQLITE support if any of the dependent features are turned on
!contains(DEFINES, ENABLE_SQLITE=.) {
  contains(DEFINES, ENABLE_DATABASE=1)|contains(DEFINES, ENABLE_ICONDATABASE=1)|contains(DEFINES, ENABLE_DOM_STORAGE=1)|contains(DEFINES, ENABLE_OFFLINE_WEB_APPLICATIONS=1) {
    DEFINES += ENABLE_SQLITE=1
  } else {
    DEFINES += ENABLE_SQLITE=0
  }
}

!contains(DEFINES, ENABLE_DASHBOARD_SUPPORT=.): DEFINES += ENABLE_DASHBOARD_SUPPORT=0
!contains(DEFINES, ENABLE_FILTERS=.): DEFINES += ENABLE_FILTERS=1
!contains(DEFINES, ENABLE_XPATH=.): DEFINES += ENABLE_XPATH=1
#!contains(DEFINES, ENABLE_XBL=.): DEFINES += ENABLE_XBL=1
!contains(DEFINES, ENABLE_WCSS=.): DEFINES += ENABLE_WCSS=0
!contains(DEFINES, ENABLE_WML=.): DEFINES += ENABLE_WML=0
!contains(DEFINES, ENABLE_SHARED_WORKERS=.): DEFINES += ENABLE_SHARED_WORKERS=1
!contains(DEFINES, ENABLE_WORKERS=.): DEFINES += ENABLE_WORKERS=1
!contains(DEFINES, ENABLE_XHTMLMP=.): DEFINES += ENABLE_XHTMLMP=0
!contains(DEFINES, ENABLE_DATAGRID=.): DEFINES += ENABLE_DATAGRID=0
!contains(DEFINES, ENABLE_VIDEO=.): DEFINES += ENABLE_VIDEO=1
!contains(DEFINES, ENABLE_RUBY=.): DEFINES += ENABLE_RUBY=1

# SVG support
!contains(DEFINES, ENABLE_SVG=0) {
    !contains(DEFINES, ENABLE_SVG=.): DEFINES += ENABLE_SVG=1
    !contains(DEFINES, ENABLE_SVG_FONTS=.): DEFINES += ENABLE_SVG_FONTS=1
    !contains(DEFINES, ENABLE_SVG_FOREIGN_OBJECT=.): DEFINES += ENABLE_SVG_FOREIGN_OBJECT=1
    !contains(DEFINES, ENABLE_SVG_ANIMATION=.): DEFINES += ENABLE_SVG_ANIMATION=1
    !contains(DEFINES, ENABLE_SVG_AS_IMAGE=.): DEFINES += ENABLE_SVG_AS_IMAGE=1
    !contains(DEFINES, ENABLE_SVG_USE=.): DEFINES += ENABLE_SVG_USE=1
} else {
    DEFINES += ENABLE_SVG_FONTS=0 ENABLE_SVG_FOREIGN_OBJECT=0 ENABLE_SVG_ANIMATION=0 ENABLE_SVG_AS_IMAGE=0 ENABLE_SVG_USE=0
}

# HTML5 media support
!contains(DEFINES, ENABLE_VIDEO=.): DEFINES += ENABLE_VIDEO=1

# HTML5 datalist support
!contains(DEFINES, ENABLE_DATALIST=.): DEFINES += ENABLE_DATALIST=1

# Nescape plugins support (NPAPI)
!contains(DEFINES, ENABLE_NETSCAPE_PLUGIN_API=.) {
    unix|win32-*:!embedded:!wince*: {
        DEFINES += ENABLE_NETSCAPE_PLUGIN_API=1
    } else {
        DEFINES += ENABLE_NETSCAPE_PLUGIN_API=0
    }
}

# Web Socket support.
!contains(DEFINES, ENABLE_WEB_SOCKETS=.): DEFINES += ENABLE_WEB_SOCKETS=1

# XSLT support with QtXmlPatterns
!contains(DEFINES, ENABLE_XSLT=.) {
    contains(QT_CONFIG, xmlpatterns):!lessThan(QT_MINOR_VERSION, 5):DEFINES += ENABLE_XSLT=1
    else:DEFINES += ENABLE_XSLT=0
}

!CONFIG(QTDIR_build):!contains(DEFINES, ENABLE_QT_BEARER=.) {
    symbian: {
        exists($${EPOCROOT}epoc32/release/winscw/udeb/QtBearer.lib)| \
        exists($${EPOCROOT}epoc32/release/armv5/lib/QtBearer.lib) {
            DEFINES += ENABLE_QT_BEARER=1
        }
    }
}

DEFINES += WTF_CHANGES=1

# Enable touch event support with Qt 4.6
!lessThan(QT_MINOR_VERSION, 6): DEFINES += ENABLE_TOUCH_EVENTS=1

# Used to compute defaults for the build-webkit script
CONFIG(compute_defaults) {
    message($$DEFINES)
    error("Done computing defaults")
}

contains(DEFINES, ENABLE_WCSS=1) {
    contains(DEFINES, ENABLE_XHTMLMP=0) {
        DEFINES -= ENABLE_XHTMLMP=0
        DEFINES += ENABLE_XHTMLMP=1
    }
}

## Forward enabled feature macros to JavaScript enabled features macros
FEATURE_DEFINES_JAVASCRIPT = LANGUAGE_JAVASCRIPT=1
contains(DEFINES, ENABLE_CHANNEL_MESSAGING=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_CHANNEL_MESSAGING=1
contains(DEFINES, ENABLE_ORIENTATION_EVENTS=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_ORIENTATION_EVENTS=1
contains(DEFINES, ENABLE_DASHBOARD_SUPPORT=0): DASHBOARDSUPPORTCSSPROPERTIES -= $$PWD/css/DashboardSupportCSSPropertyNames.in
contains(DEFINES, ENABLE_DATAGRID=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_DATAGRID=1
contains(DEFINES, ENABLE_EVENTSOURCE=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_EVENTSOURCE=1
contains(DEFINES, ENABLE_DATABASE=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_DATABASE=1
contains(DEFINES, ENABLE_DOM_STORAGE=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_DOM_STORAGE=1
contains(DEFINES, ENABLE_SHARED_SCRIPT=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_SHARED_SCRIPT=1
contains(DEFINES, ENABLE_WORKERS=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_WORKERS=1
contains(DEFINES, ENABLE_SHARED_WORKERS=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_SHARED_WORKERS=1
contains(DEFINES, ENABLE_VIDEO=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_VIDEO=1
contains(DEFINES, ENABLE_XPATH=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_XPATH=1
contains(DEFINES, ENABLE_XSLT=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_XSLT=1
contains(DEFINES, ENABLE_XBL=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_XBL=1
contains(DEFINES, ENABLE_FILTERS=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_FILTERS=1
contains(DEFINES, ENABLE_WCSS=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_WCSS=1
contains(DEFINES, ENABLE_WML=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_WML=1
contains(DEFINES, ENABLE_XHTMLMP=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_XHTMLMP=1
contains(DEFINES, ENABLE_SVG=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_SVG=1
contains(DEFINES, ENABLE_JAVASCRIPT_DEBUGGER=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_JAVASCRIPT_DEBUGGER=1
contains(DEFINES, ENABLE_OFFLINE_WEB_APPLICATIONS=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_OFFLINE_WEB_APPLICATIONS=1
contains(DEFINES, ENABLE_WEB_SOCKETS=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_WEB_SOCKETS=1
contains(DEFINES, ENABLE_TOUCH_EVENTS=1): FEATURE_DEFINES_JAVASCRIPT += ENABLE_TOUCH_EVENTS=1


## Derived source generators
WML_NAMES = $$PWD/wml/WMLTagNames.in

SVG_NAMES = $$PWD/svg/svgtags.in

XLINK_NAMES = $$PWD/svg/xlinkattrs.in

TOKENIZER = $$PWD/css/tokenizer.flex

DOCTYPESTRINGS = $$PWD/html/DocTypeStrings.gperf

CSSBISON = $$PWD/css/CSSGrammar.y

HTML_NAMES = $$PWD/html/HTMLTagNames.in

XML_NAMES = $$PWD/xml/xmlattrs.in

XMLNS_NAMES = $$PWD/xml/xmlnsattrs.in

ENTITIES_GPERF = $$PWD/html/HTMLEntityNames.gperf

COLORDAT_GPERF = $$PWD/platform/ColorData.gperf

WALDOCSSPROPS = $$PWD/css/CSSPropertyNames.in

WALDOCSSVALUES = $$PWD/css/CSSValueKeywords.in

DASHBOARDSUPPORTCSSPROPERTIES = $$PWD/css/DashboardSupportCSSPropertyNames.in

XPATHBISON = $$PWD/xml/XPathGrammar.y

contains(DEFINES, ENABLE_SVG=1) {
    EXTRACSSPROPERTIES += $$PWD/css/SVGCSSPropertyNames.in
    EXTRACSSVALUES += $$PWD/css/SVGCSSValueKeywords.in
}

contains(DEFINES, ENABLE_WCSS=1) {
    EXTRACSSPROPERTIES += $$PWD/css/WCSSPropertyNames.in
    EXTRACSSVALUES += $$PWD/css/WCSSValueKeywords.in
}

STYLESHEETS_EMBED = \
    $$PWD/css/html.css \
    $$PWD/css/quirks.css \
    $$PWD/css/svg.css \
    $$PWD/css/view-source.css \
    $$PWD/css/wml.css \
    $$PWD/css/mediaControls.css \
    $$PWD/css/mediaControlsQt.css

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
    css/CSSVariablesDeclaration.idl \
    css/CSSVariablesRule.idl \
    css/Media.idl \
    css/MediaList.idl \
    css/RGBColor.idl \
    css/Rect.idl \
    css/StyleSheet.idl \
    css/StyleSheetList.idl \
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
    dom/DocumentFragment.idl \
    dom/Document.idl \
    dom/DocumentType.idl \
    dom/DOMCoreException.idl \
    dom/DOMImplementation.idl \
    dom/Element.idl \
    dom/Entity.idl \
    dom/EntityReference.idl \
    dom/ErrorEvent.idl \
    dom/Event.idl \
    dom/EventException.idl \
#    dom/EventListener.idl \
#    dom/EventTarget.idl \
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
    html/Blob.idl \
    html/canvas/WebGLArray.idl \
    html/canvas/WebGLArrayBuffer.idl \
    html/canvas/WebGLByteArray.idl \
    html/canvas/WebGLFloatArray.idl \
    html/canvas/CanvasGradient.idl \
    html/canvas/WebGLIntArray.idl \
    html/canvas/CanvasPattern.idl \
    html/canvas/CanvasRenderingContext.idl \
    html/canvas/CanvasRenderingContext2D.idl \
    html/canvas/WebGLRenderingContext.idl \
    html/canvas/WebGLShortArray.idl \
    html/canvas/WebGLUnsignedByteArray.idl \
    html/canvas/WebGLUnsignedIntArray.idl \
    html/canvas/WebGLUnsignedShortArray.idl \
    html/DataGridColumn.idl \
    html/DataGridColumnList.idl \
    html/File.idl \
    html/FileList.idl \
    html/HTMLAllCollection.idl \
    html/HTMLAudioElement.idl \
    html/HTMLAnchorElement.idl \
    html/HTMLAppletElement.idl \
    html/HTMLAreaElement.idl \
    html/HTMLBaseElement.idl \
    html/HTMLBaseFontElement.idl \
    html/HTMLBlockquoteElement.idl \
    html/HTMLBodyElement.idl \
    html/HTMLBRElement.idl \
    html/HTMLButtonElement.idl \
    html/HTMLCanvasElement.idl \
    html/HTMLCollection.idl \
    html/HTMLDataGridElement.idl \
    html/HTMLDataGridCellElement.idl \
    html/HTMLDataGridColElement.idl \
    html/HTMLDataGridRowElement.idl \
    html/HTMLDataListElement.idl \
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
    html/HTMLLabelElement.idl \
    html/HTMLLegendElement.idl \
    html/HTMLLIElement.idl \
    html/HTMLLinkElement.idl \
    html/HTMLMapElement.idl \
    html/HTMLMarqueeElement.idl \
    html/HTMLMediaElement.idl \
    html/HTMLMenuElement.idl \
    html/HTMLMetaElement.idl \
    html/HTMLModElement.idl \
    html/HTMLObjectElement.idl \
    html/HTMLOListElement.idl \
    html/HTMLOptGroupElement.idl \
    html/HTMLOptionElement.idl \
    html/HTMLOptionsCollection.idl \
    html/HTMLParagraphElement.idl \
    html/HTMLParamElement.idl \
    html/HTMLPreElement.idl \
    html/HTMLQuoteElement.idl \
    html/HTMLScriptElement.idl \
    html/HTMLSelectElement.idl \
    html/HTMLSourceElement.idl \
    html/HTMLStyleElement.idl \
    html/HTMLTableCaptionElement.idl \
    html/HTMLTableCellElement.idl \
    html/HTMLTableColElement.idl \
    html/HTMLTableElement.idl \
    html/HTMLTableRowElement.idl \
    html/HTMLTableSectionElement.idl \
    html/HTMLTextAreaElement.idl \
    html/HTMLTitleElement.idl \
    html/HTMLUListElement.idl \
    html/HTMLVideoElement.idl \
    html/ImageData.idl \
    html/MediaError.idl \
    html/TextMetrics.idl \
    html/TimeRanges.idl \
    html/ValidityState.idl \
    html/VoidCallback.idl \
    inspector/InjectedScriptHost.idl \
    inspector/InspectorBackend.idl \
    inspector/InspectorFrontendHost.idl \
    inspector/JavaScriptCallFrame.idl \
    loader/appcache/DOMApplicationCache.idl \
    page/BarInfo.idl \
    page/Console.idl \
    page/Coordinates.idl \
    page/DOMSelection.idl \
    page/DOMWindow.idl \
    page/EventSource.idl \
    page/Geolocation.idl \
    page/Geoposition.idl \
    page/History.idl \
    page/Location.idl \
    page/Navigator.idl \
    page/PositionError.idl \
    page/Screen.idl \
    page/WebKitPoint.idl \
    page/WorkerNavigator.idl \
    plugins/Plugin.idl \
    plugins/MimeType.idl \
    plugins/PluginArray.idl \
    plugins/MimeTypeArray.idl \
    storage/Database.idl \
    storage/Storage.idl \
    storage/StorageEvent.idl \
    storage/SQLError.idl \
    storage/SQLResultSet.idl \
    storage/SQLResultSetRowList.idl \
    storage/SQLTransaction.idl \
    svg/SVGZoomEvent.idl \
    svg/SVGAElement.idl \
    svg/SVGAltGlyphElement.idl \
    svg/SVGAngle.idl \
    svg/SVGAnimateColorElement.idl \
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
    svg/SVGFEDiffuseLightingElement.idl \
    svg/SVGFEDisplacementMapElement.idl \
    svg/SVGFEDistantLightElement.idl \
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

contains(DEFINES, ENABLE_WML=1) {
    wmlnames.output = $${WC_GENERATED_SOURCES_DIR}/WMLNames.cpp
    wmlnames.input = WML_NAMES
    wmlnames.wkScript = $$PWD/dom/make_names.pl
    wmlnames.commands = perl -I$$PWD/bindings/scripts $$wmlnames.wkScript --tags $$PWD/wml/WMLTagNames.in --attrs $$PWD/wml/WMLAttributeNames.in --extraDefines \"$${DEFINES}\" --preprocessor \"$${QMAKE_MOC} -E\" --factory --wrapperFactory --outputDir $$WC_GENERATED_SOURCES_DIR
    wmlnames.wkExtraSources = $${WC_GENERATED_SOURCES_DIR}/WMLElementFactory.cpp
    addExtraCompiler(wmlnames)
}

contains(DEFINES, ENABLE_SVG=1) {
    # GENERATOR 5-C:
    svgnames.output = $${WC_GENERATED_SOURCES_DIR}/SVGNames.cpp
    svgnames.input = SVG_NAMES
    svgnames.wkScript = $$PWD/dom/make_names.pl
    svgnames.commands = perl -I$$PWD/bindings/scripts $$svgnames.wkScript --tags $$PWD/svg/svgtags.in --attrs $$PWD/svg/svgattrs.in --extraDefines \"$${DEFINES}\" --preprocessor \"$${QMAKE_MOC} -E\" --factory --wrapperFactory --outputDir $$WC_GENERATED_SOURCES_DIR
    svgnames.wkExtraSources = $${WC_GENERATED_SOURCES_DIR}/SVGElementFactory.cpp $${WC_GENERATED_SOURCES_DIR}/JSSVGElementWrapperFactory.cpp
    addExtraCompiler(svgnames)
}

# GENERATOR 5-D:
xlinknames.output = $${WC_GENERATED_SOURCES_DIR}/XLinkNames.cpp
xlinknames.wkScript = $$PWD/dom/make_names.pl
xlinknames.commands = perl -I$$PWD/bindings/scripts $$xlinknames.wkScript --attrs $$PWD/svg/xlinkattrs.in --preprocessor \"$${QMAKE_MOC} -E\" --outputDir $$WC_GENERATED_SOURCES_DIR
xlinknames.input = XLINK_NAMES
addExtraCompiler(xlinknames)

# GENERATOR 6-A:
cssprops.output = $${WC_GENERATED_SOURCES_DIR}/${QMAKE_FILE_BASE}.cpp
cssprops.input = WALDOCSSPROPS
cssprops.wkScript = $$PWD/css/makeprop.pl
cssprops.commands = perl -ne \"print lc\" ${QMAKE_FILE_NAME} $${DASHBOARDSUPPORTCSSPROPERTIES} $${EXTRACSSPROPERTIES} > $${WC_GENERATED_SOURCES_DIR}/${QMAKE_FILE_BASE}.in && cd $$WC_GENERATED_SOURCES_DIR && perl $$cssprops.wkScript && $(DEL_FILE) ${QMAKE_FILE_BASE}.in ${QMAKE_FILE_BASE}.gperf
cssprops.depends = ${QMAKE_FILE_NAME} $${DASHBOARDSUPPORTCSSPROPERTIES} $${EXTRACSSPROPERTIES}
addExtraCompiler(cssprops)

# GENERATOR 6-B:
cssvalues.output = $${WC_GENERATED_SOURCES_DIR}/${QMAKE_FILE_BASE}.c
cssvalues.input = WALDOCSSVALUES
cssvalues.wkScript = $$PWD/css/makevalues.pl
cssvalues.commands = perl -ne \"print lc\" ${QMAKE_FILE_NAME} $$EXTRACSSVALUES > $${WC_GENERATED_SOURCES_DIR}/${QMAKE_FILE_BASE}.in && cd $$WC_GENERATED_SOURCES_DIR && perl $$cssvalues.wkScript && $(DEL_FILE) ${QMAKE_FILE_BASE}.in ${QMAKE_FILE_BASE}.gperf
cssvalues.depends = ${QMAKE_FILE_NAME} $${EXTRACSSVALUES}
cssvalues.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_WC_GENERATED_SOURCES_DIR}/${QMAKE_FILE_BASE}.h
addExtraCompiler(cssvalues)

# GENERATOR 1: IDL compiler
idl.output = $${WC_GENERATED_SOURCES_DIR}/JS${QMAKE_FILE_BASE}.cpp
idl.input = IDL_BINDINGS
idl.wkScript = $$PWD/bindings/scripts/generate-bindings.pl
idl.commands = perl -I$$PWD/bindings/scripts $$idl.wkScript --defines \"$${FEATURE_DEFINES_JAVASCRIPT}\" --generator JS --include $$PWD/dom --include $$PWD/html --include $$PWD/xml --include $$PWD/svg --outputDir $$WC_GENERATED_SOURCES_DIR --preprocessor \"$${QMAKE_MOC} -E\" ${QMAKE_FILE_NAME}
idl.depends = $$PWD/bindings/scripts/CodeGenerator.pm \
              $$PWD/bindings/scripts/CodeGeneratorJS.pm \
              $$PWD/bindings/scripts/IDLParser.pm \
              $$PWD/bindings/scripts/IDLStructure.pm \
              $$PWD/bindings/scripts/InFilesParser.pm
addExtraCompiler(idl)

# GENERATOR 3: tokenizer (flex)
tokenizer.output = $${WC_GENERATED_SOURCES_DIR}/${QMAKE_FILE_BASE}.cpp
tokenizer.input = TOKENIZER
tokenizer.wkScript = $$PWD/css/maketokenizer
tokenizer.commands = flex -t < ${QMAKE_FILE_NAME} | perl $$tokenizer.wkScript > ${QMAKE_FILE_OUT}
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
htmlnames.commands = perl -I$$PWD/bindings/scripts $$htmlnames.wkScript --tags $$PWD/html/HTMLTagNames.in --attrs $$PWD/html/HTMLAttributeNames.in --extraDefines \"$${DEFINES}\" --preprocessor \"$${QMAKE_MOC} -E\"  --factory --wrapperFactory --outputDir $$WC_GENERATED_SOURCES_DIR
htmlnames.depends = $$PWD/html/HTMLAttributeNames.in
htmlnames.wkExtraSources = $${WC_GENERATED_SOURCES_DIR}/HTMLElementFactory.cpp $${WC_GENERATED_SOURCES_DIR}/JSHTMLElementWrapperFactory.cpp
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

# GENERATOR 8-A:
entities.output = $${WC_GENERATED_SOURCES_DIR}/HTMLEntityNames.c
entities.input = ENTITIES_GPERF
entities.commands = gperf -a -L ANSI-C -C -G -c -o -t --includes --key-positions="*" -N findEntity -D -s 2 < $$PWD/html/HTMLEntityNames.gperf > $${WC_GENERATED_SOURCES_DIR}/HTMLEntityNames.c
entities.clean = ${QMAKE_FILE_OUT}
addExtraCompiler(entities)

# GENERATOR 8-B:
doctypestrings.output = $${WC_GENERATED_SOURCES_DIR}/${QMAKE_FILE_BASE}.cpp
doctypestrings.input = DOCTYPESTRINGS
doctypestrings.commands = gperf -CEot -L ANSI-C --includes --key-positions="*" -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards < ${QMAKE_FILE_NAME} >> ${QMAKE_FILE_OUT}
doctypestrings.clean = ${QMAKE_FILE_OUT}
addExtraCompiler(doctypestrings)

# GENERATOR 8-C:
colordata.output = $${WC_GENERATED_SOURCES_DIR}/ColorData.c
colordata.input = COLORDAT_GPERF
colordata.commands = gperf -CDEot -L ANSI-C --includes --key-positions="*" -N findColor -D -s 2 < ${QMAKE_FILE_NAME} >> ${QMAKE_FILE_OUT}
addExtraCompiler(colordata)

# GENERATOR 9:
stylesheets.wkScript = $$PWD/css/make-css-file-arrays.pl
stylesheets.output = $${WC_GENERATED_SOURCES_DIR}/UserAgentStyleSheetsData.cpp
stylesheets.input = stylesheets.wkScript
stylesheets.commands = perl $$stylesheets.wkScript --preprocessor \"$${QMAKE_MOC} -E\" $${WC_GENERATED_SOURCES_DIR}/UserAgentStyleSheets.h ${QMAKE_FILE_OUT} $$STYLESHEETS_EMBED
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
# The appropriate Apple-maintained Version.xcconfig file for WebKit version information is in WebKit/mac/Configurations/.
webkitversion.wkScript = $$PWD/../WebKit/scripts/generate-webkitversion.pl
webkitversion.output = $${WC_GENERATED_SOURCES_DIR}/WebKitVersion.h
webkitversion.input = webkitversion.wkScript
webkitversion.commands = perl $$webkitversion.wkScript --config $$PWD/../WebKit/mac/Configurations/Version.xcconfig --outputDir $${WC_GENERATED_SOURCES_DIR}/
webkitversion.clean = ${QMAKE_VAR_WC_GENERATED_SOURCES_DIR}/WebKitVersion.h
webkitversion.wkAddOutputToSources = false
addExtraCompiler(webkitversion)

