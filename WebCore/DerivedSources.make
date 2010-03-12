# Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
# Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com> 
# Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

VPATH = \
    $(WebCore) \
    $(WebCore)/bindings/js \
    $(WebCore)/bindings/objc \
    $(WebCore)/css \
    $(WebCore)/dom \
    $(WebCore)/html \
    $(WebCore)/html/canvas \
    $(WebCore)/inspector \
    $(WebCore)/loader/appcache \
    $(WebCore)/notifications \
    $(WebCore)/page \
    $(WebCore)/plugins \
    $(WebCore)/storage \
    $(WebCore)/xml \
    $(WebCore)/wml \
    $(WebCore)/workers \
    $(WebCore)/svg \
    $(WebCore)/websockets \
#

DOM_CLASSES = \
    AbstractView \
    AbstractWorker \
    Attr \
    BarInfo \
    BeforeLoadEvent \
    Blob \
    CDATASection \
    CSSCharsetRule \
    CSSFontFaceRule \
    CSSImportRule \
    CSSMediaRule \
    CSSPageRule \
    CSSPrimitiveValue \
    CSSRule \
    CSSRuleList \
    CSSStyleDeclaration \
    CSSStyleRule \
    CSSStyleSheet \
    CSSUnknownRule \
    CSSValue \
    CSSValueList \
    CSSVariablesRule \
    CSSVariablesDeclaration \
    WebGLActiveInfo \
    WebGLArray \
    WebGLArrayBuffer \
    WebGLBuffer \
    WebGLByteArray \
    WebGLContextAttributes \
    WebGLFloatArray \
    WebGLFramebuffer \
    CanvasGradient \
    WebGLIntArray \
    CanvasPattern \
    WebGLProgram \
    WebGLRenderbuffer \
    CanvasRenderingContext \
    CanvasRenderingContext2D \
    WebGLRenderingContext \
    WebGLShader \
    WebGLShortArray \
    WebGLTexture \
    WebGLUniformLocation \
    WebGLUnsignedByteArray \
    WebGLUnsignedIntArray \
    WebGLUnsignedShortArray \
    CharacterData \
    ClientRect \
    ClientRectList \
    Clipboard \
    Comment \
    CompositionEvent \
    Console \
    Coordinates \
    Counter \
    DataGridColumn \
    DataGridColumnList \
    DedicatedWorkerContext \
    DOMApplicationCache \
    DOMCoreException \
    DOMImplementation \
    DOMParser \
    DOMSelection \
    DOMWindow \
    Database \
    Document \
    DocumentFragment \
    DocumentType \
    Element \
    ElementTimeControl \
    Entity \
    EntityReference \
    ErrorEvent \
    Event \
    EventException \
    EventListener \
    EventSource \
    EventTarget \
    File \
    FileList \
    Geolocation \
    Geoposition \
    HTMLAllCollection \
    HTMLAnchorElement \
    HTMLAppletElement \
    HTMLAreaElement \
    HTMLAudioElement \
    HTMLBRElement \
    HTMLBaseElement \
    HTMLBaseFontElement \
    HTMLBlockquoteElement \
    HTMLBodyElement \
    HTMLButtonElement \
    HTMLCanvasElement \
    HTMLCollection \
    HTMLDataGridElement \
    HTMLDataGridCellElement \
    HTMLDataGridColElement \
    HTMLDataGridRowElement \
    HTMLDataListElement \
    HTMLDListElement \
    HTMLDirectoryElement \
    HTMLDivElement \
    HTMLDocument \
    HTMLElement \
    HTMLEmbedElement \
    HTMLFieldSetElement \
    HTMLFontElement \
    HTMLFormElement \
    HTMLFrameElement \
    HTMLFrameSetElement \
    HTMLHRElement \
    HTMLHeadElement \
    HTMLHeadingElement \
    HTMLHtmlElement \
    HTMLIFrameElement \
    HTMLImageElement \
    HTMLInputElement \
    HTMLIsIndexElement \
    HTMLLIElement \
    HTMLLabelElement \
    HTMLLegendElement \
    HTMLLinkElement \
    HTMLMapElement \
    HTMLMarqueeElement \
    HTMLMediaElement \
    HTMLMenuElement \
    HTMLMetaElement \
    HTMLModElement \
    HTMLOListElement \
    HTMLObjectElement \
    HTMLOptGroupElement \
    HTMLOptionElement \
    HTMLOptionsCollection \
    HTMLParagraphElement \
    HTMLParamElement \
    HTMLPreElement \
    HTMLQuoteElement \
    HTMLScriptElement \
    HTMLSelectElement \
    HTMLSourceElement \
    HTMLStyleElement \
    HTMLTableCaptionElement \
    HTMLTableCellElement \
    HTMLTableColElement \
    HTMLTableElement \
    HTMLTableRowElement \
    HTMLTableSectionElement \
    HTMLTextAreaElement \
    HTMLTitleElement \
    HTMLUListElement \
    HTMLVideoElement \
    History \
    ImageData \
    InjectedScriptHost \
    InspectorBackend \
    InspectorFrontendHost \
    KeyboardEvent \
    Location \
    Media \
    MediaError \
    MediaList \
    MessageChannel \
    MessageEvent \
    MessagePort \
    MimeType \
    MimeTypeArray \
    MouseEvent \
    MutationEvent \
    NamedNodeMap \
    Navigator \
    Node \
    NodeFilter \
    NodeIterator \
    NodeList \
    Notation \
    Notification \
    NotificationCenter \
    OverflowEvent \
    PageTransitionEvent \
    Plugin \
    PluginArray \
    PopStateEvent \
    PositionError \
    ProcessingInstruction \
    ProgressEvent \
    RGBColor \
    Range \
    RangeException \
    Rect \
    SharedWorker \
    SharedWorkerContext \
    SQLError \
    SQLResultSet \
    SQLResultSetRowList \
    SQLTransaction \
    Storage \
    StorageEvent \
    SVGAElement \
    SVGAltGlyphElement \
    SVGAngle \
    SVGAnimateColorElement \
    SVGAnimateElement \
    SVGAnimateTransformElement \
    SVGAnimatedAngle \
    SVGAnimatedBoolean \
    SVGAnimatedEnumeration \
    SVGAnimatedInteger \
    SVGAnimatedLength \
    SVGAnimatedLengthList \
    SVGAnimatedNumber \
    SVGAnimatedNumberList \
    SVGAnimatedPathData \
    SVGAnimatedPoints \
    SVGAnimatedPreserveAspectRatio \
    SVGAnimatedRect \
    SVGAnimatedString \
    SVGAnimatedTransformList \
    SVGAnimationElement \
    SVGCircleElement \
    SVGClipPathElement \
    SVGColor \
    SVGComponentTransferFunctionElement \
    SVGCursorElement \
    SVGDefsElement \
    SVGDescElement \
    SVGDocument \
    SVGElement \
    SVGElementInstance \
    SVGElementInstanceList \
    SVGEllipseElement \
    SVGException \
    SVGExternalResourcesRequired \
    SVGFEBlendElement \
    SVGFEColorMatrixElement \
    SVGFEComponentTransferElement \
    SVGFECompositeElement \
    SVGFEDiffuseLightingElement \
    SVGFEDisplacementMapElement \
    SVGFEDistantLightElement \
    SVGFEFloodElement \
    SVGFEFuncAElement \
    SVGFEFuncBElement \
    SVGFEFuncGElement \
    SVGFEFuncRElement \
    SVGFEGaussianBlurElement \
    SVGFEImageElement \
    SVGFEMergeElement \
    SVGFEMergeNodeElement \
    SVGFEMorphologyElement \
    SVGFEOffsetElement \
    SVGFEPointLightElement \
    SVGFESpecularLightingElement \
    SVGFESpotLightElement \
    SVGFETileElement \
    SVGFETurbulenceElement \
    SVGFilterElement \
    SVGFilterPrimitiveStandardAttributes \
    SVGFitToViewBox \
    SVGFontElement \
    SVGFontFaceElement \
    SVGFontFaceFormatElement \
    SVGFontFaceNameElement \
    SVGFontFaceSrcElement \
    SVGFontFaceUriElement \
    SVGForeignObjectElement \
    SVGGElement \
    SVGGlyphElement \
    SVGGradientElement \
    SVGHKernElement \
    SVGImageElement \
    SVGLangSpace \
    SVGLength \
    SVGLengthList \
    SVGLineElement \
    SVGLinearGradientElement \
    SVGLocatable \
    SVGMarkerElement \
    SVGMaskElement \
    SVGMatrix \
    SVGMetadataElement \
    SVGMissingGlyphElement \
    SVGNumber \
    SVGNumberList \
    SVGPaint \
    SVGPathElement \
    SVGPathSeg \
    SVGPathSegArcAbs \
    SVGPathSegArcRel \
    SVGPathSegClosePath \
    SVGPathSegCurvetoCubicAbs \
    SVGPathSegCurvetoCubicRel \
    SVGPathSegCurvetoCubicSmoothAbs \
    SVGPathSegCurvetoCubicSmoothRel \
    SVGPathSegCurvetoQuadraticAbs \
    SVGPathSegCurvetoQuadraticRel \
    SVGPathSegCurvetoQuadraticSmoothAbs \
    SVGPathSegCurvetoQuadraticSmoothRel \
    SVGPathSegLinetoAbs \
    SVGPathSegLinetoHorizontalAbs \
    SVGPathSegLinetoHorizontalRel \
    SVGPathSegLinetoRel \
    SVGPathSegLinetoVerticalAbs \
    SVGPathSegLinetoVerticalRel \
    SVGPathSegList \
    SVGPathSegMovetoAbs \
    SVGPathSegMovetoRel \
    SVGPatternElement \
    SVGPoint \
    SVGPointList \
    SVGPolygonElement \
    SVGPolylineElement \
    SVGPreserveAspectRatio \
    SVGRadialGradientElement \
    SVGRect \
    SVGRectElement \
    SVGRenderingIntent \
    SVGSVGElement \
    SVGScriptElement \
    SVGSetElement \
    SVGStopElement \
    SVGStringList \
    SVGStylable \
    SVGStyleElement \
    SVGSwitchElement \
    SVGSymbolElement \
    SVGTRefElement \
    SVGTSpanElement \
    SVGTests \
    SVGTextContentElement \
    SVGTextElement \
    SVGTextPathElement \
    SVGTextPositioningElement \
    SVGTitleElement \
    SVGTransform \
    SVGTransformList \
    SVGTransformable \
    SVGURIReference \
    SVGUnitTypes \
    SVGUseElement \
    SVGViewElement \
    SVGZoomAndPan \
    SVGZoomEvent \
    Screen \
    StyleSheet \
    StyleSheetList \
    Text \
    TextEvent \
    TextMetrics \
    TimeRanges \
    TreeWalker \
    UIEvent \
    ValidityState \
    WebKitAnimationEvent \
    WebKitCSSKeyframeRule \
    WebKitCSSKeyframesRule \
    WebKitCSSMatrix \
    WebKitCSSTransformValue \
    WebKitPoint \
    WebKitTransitionEvent \
    WebSocket \
    WheelEvent \
    Worker \
    WorkerContext \
    WorkerLocation \
    WorkerNavigator \
    XMLHttpRequest \
    XMLHttpRequestException \
    XMLHttpRequestProgressEvent \
    XMLHttpRequestUpload \
    XMLSerializer \
    XPathEvaluator \
    XPathException \
    XPathExpression \
    XPathNSResolver \
    XPathResult \
    XSLTProcessor \
#

.PHONY : all

JS_DOM_HEADERS=$(filter-out JSEventListener.h JSEventTarget.h,$(DOM_CLASSES:%=JS%.h))

all : \
    $(JS_DOM_HEADERS) \
    \
    JSJavaScriptCallFrame.h \
    \
    CSSGrammar.cpp \
    CSSPropertyNames.h \
    CSSValueKeywords.h \
    ColorData.c \
    DocTypeStrings.cpp \
    HTMLElementFactory.cpp \
    HTMLEntityNames.c \
    HTMLNames.cpp \
    WMLElementFactory.cpp \
    WMLNames.cpp \
    JSSVGElementWrapperFactory.cpp \
    SVGElementFactory.cpp \
    SVGNames.cpp \
    UserAgentStyleSheets.h \
    XLinkNames.cpp \
    XMLNSNames.cpp \
    XMLNames.cpp \
    MathMLElementFactory.cpp \
    MathMLNames.cpp \
    XPathGrammar.cpp \
    tokenizer.cpp \
#

# --------

ADDITIONAL_IDL_DEFINES :=

ifeq ($(OS),MACOS)

FRAMEWORK_FLAGS = $(shell echo $(BUILT_PRODUCTS_DIR) $(FRAMEWORK_SEARCH_PATHS) | perl -e 'print "-F " . join(" -F ", split(" ", <>));')

ifeq ($(shell gcc -E -P -dM $(FRAMEWORK_FLAGS) WebCore/ForwardingHeaders/wtf/Platform.h | grep ENABLE_DASHBOARD_SUPPORT | cut -d' ' -f3), 1)
    ENABLE_DASHBOARD_SUPPORT = 1
else
    ENABLE_DASHBOARD_SUPPORT = 0
endif

ifeq ($(shell gcc -E -P -dM $(FRAMEWORK_FLAGS) WebCore/ForwardingHeaders/wtf/Platform.h | grep ENABLE_ORIENTATION_EVENTS | cut -d' ' -f3), 1)
    ENABLE_ORIENTATION_EVENTS = 1
else
    ENABLE_ORIENTATION_EVENTS = 0
endif

else

ifndef ENABLE_DASHBOARD_SUPPORT
    ENABLE_DASHBOARD_SUPPORT = 0
endif

ifndef ENABLE_ORIENTATION_EVENTS
    ENABLE_ORIENTATION_EVENTS = 0
endif

endif # MACOS

ifeq ($(ENABLE_ORIENTATION_EVENTS), 1)
    ADDITIONAL_IDL_DEFINES := $(ADDITIONAL_IDL_DEFINES) ENABLE_ORIENTATION_EVENTS
endif

# --------

# CSS property names and value keywords

WEBCORE_CSS_PROPERTY_NAMES := $(WebCore)/css/CSSPropertyNames.in
WEBCORE_CSS_VALUE_KEYWORDS := $(WebCore)/css/CSSValueKeywords.in

ifeq ($(findstring ENABLE_SVG,$(FEATURE_DEFINES)), ENABLE_SVG)
    WEBCORE_CSS_PROPERTY_NAMES := $(WEBCORE_CSS_PROPERTY_NAMES) $(WebCore)/css/SVGCSSPropertyNames.in
    WEBCORE_CSS_VALUE_KEYWORDS := $(WEBCORE_CSS_VALUE_KEYWORDS) $(WebCore)/css/SVGCSSValueKeywords.in
endif

ifeq ($(ENABLE_DASHBOARD_SUPPORT), 1)
    WEBCORE_CSS_PROPERTY_NAMES := $(WEBCORE_CSS_PROPERTY_NAMES) $(WebCore)/css/DashboardSupportCSSPropertyNames.in
endif

# The grep commands below reject output containing anything other than:
# 1. Lines beginning with '#'
# 2. Lines containing only whitespace
# These two types of lines will be ignored by make{prop,values}.pl.
CSSPropertyNames.h : $(WEBCORE_CSS_PROPERTY_NAMES) css/makeprop.pl
	if sort $(WEBCORE_CSS_PROPERTY_NAMES) | uniq -d | grep -E -v '(^#)|(^[[:space:]]*$$)'; then echo 'Duplicate value!'; exit 1; fi
	cat $(WEBCORE_CSS_PROPERTY_NAMES) > CSSPropertyNames.in
	perl "$(WebCore)/css/makeprop.pl"

CSSValueKeywords.h : $(WEBCORE_CSS_VALUE_KEYWORDS) css/makevalues.pl
	# Lower case all the values, as CSS values are case-insensitive
	perl -ne 'print lc' $(WEBCORE_CSS_VALUE_KEYWORDS) > CSSValueKeywords.in
	if sort CSSValueKeywords.in | uniq -d | grep -E -v '(^#)|(^[[:space:]]*$$)'; then echo 'Duplicate value!'; exit 1; fi
	perl "$(WebCore)/css/makevalues.pl"

# --------

# DOCTYPE strings

DocTypeStrings.cpp : html/DocTypeStrings.gperf
	gperf -CEot -L ANSI-C -k "*" -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards $< > $@

# --------

# HTML entity names

HTMLEntityNames.c : html/HTMLEntityNames.gperf
	gperf -a -L ANSI-C -C -G -c -o -t -k '*' -N findEntity -D -s 2 $< > $@

# --------

# color names

ColorData.c : platform/ColorData.gperf
	gperf -CDEot -L ANSI-C -k '*' -N findColor -D -s 2 $< > $@

# --------

# CSS tokenizer

tokenizer.cpp : css/tokenizer.flex css/maketokenizer
	flex -t $< | perl $(WebCore)/css/maketokenizer > $@

# --------

# CSS grammar
# NOTE: Older versions of bison do not inject an inclusion guard, so we add one.

CSSGrammar.cpp : css/CSSGrammar.y
	bison -d -p cssyy $< -o $@
	touch CSSGrammar.cpp.h
	touch CSSGrammar.hpp
	echo '#ifndef CSSGrammar_h' > CSSGrammar.h
	echo '#define CSSGrammar_h' >> CSSGrammar.h
	cat CSSGrammar.cpp.h CSSGrammar.hpp >> CSSGrammar.h
	echo '#endif' >> CSSGrammar.h
	rm -f CSSGrammar.cpp.h CSSGrammar.hpp

# --------

# XPath grammar
# NOTE: Older versions of bison do not inject an inclusion guard, so we add one.

XPathGrammar.cpp : xml/XPathGrammar.y $(PROJECT_FILE)
	bison -d -p xpathyy $< -o $@
	touch XPathGrammar.cpp.h
	touch XPathGrammar.hpp
	echo '#ifndef XPathGrammar_h' > XPathGrammar.h
	echo '#define XPathGrammar_h' >> XPathGrammar.h
	cat XPathGrammar.cpp.h XPathGrammar.hpp >> XPathGrammar.h
	echo '#endif' >> XPathGrammar.h
	rm -f XPathGrammar.cpp.h XPathGrammar.hpp

# --------

# user agent style sheets

USER_AGENT_STYLE_SHEETS = $(WebCore)/css/html.css $(WebCore)/css/quirks.css $(WebCore)/css/view-source.css $(WebCore)/css/themeWin.css $(WebCore)/css/themeWinQuirks.css 

ifeq ($(findstring ENABLE_SVG,$(FEATURE_DEFINES)), ENABLE_SVG)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/css/svg.css 
endif

ifeq ($(findstring ENABLE_WML,$(FEATURE_DEFINES)), ENABLE_WML)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/css/wml.css
endif

ifeq ($(findstring ENABLE_MATHML,$(FEATURE_DEFINES)), ENABLE_MATHML)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/css/mathml.css
endif

ifeq ($(findstring ENABLE_VIDEO,$(FEATURE_DEFINES)), ENABLE_VIDEO)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/css/mediaControls.css
ifeq ($(OS),MACOS)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/css/mediaControlsQuickTime.css
endif
endif

UserAgentStyleSheets.h : css/make-css-file-arrays.pl $(USER_AGENT_STYLE_SHEETS)
	perl $< $@ UserAgentStyleSheetsData.cpp $(USER_AGENT_STYLE_SHEETS)

# --------

# HTML tag and attribute names

ifeq ($(findstring ENABLE_DATALIST,$(FEATURE_DEFINES)), ENABLE_DATALIST)
    HTML_FLAGS := $(HTML_FLAGS) ENABLE_DATALIST=1
endif

ifeq ($(findstring ENABLE_VIDEO,$(FEATURE_DEFINES)), ENABLE_VIDEO)
    HTML_FLAGS := $(HTML_FLAGS) ENABLE_VIDEO=1
endif

ifeq ($(findstring ENABLE_RUBY,$(FEATURE_DEFINES)), ENABLE_RUBY)
    HTML_FLAGS := $(HTML_FLAGS) ENABLE_RUBY=1
endif

ifdef HTML_FLAGS

HTMLElementFactory.cpp HTMLNames.cpp : dom/make_names.pl html/HTMLTagNames.in html/HTMLAttributeNames.in
	perl -I $(WebCore)/bindings/scripts $< --tags $(WebCore)/html/HTMLTagNames.in --attrs $(WebCore)/html/HTMLAttributeNames.in --factory --wrapperFactory --extraDefines "$(HTML_FLAGS)"

else

HTMLElementFactory.cpp HTMLNames.cpp : dom/make_names.pl html/HTMLTagNames.in html/HTMLAttributeNames.in
	perl -I $(WebCore)/bindings/scripts $< --tags $(WebCore)/html/HTMLTagNames.in --attrs $(WebCore)/html/HTMLAttributeNames.in --factory --wrapperFactory

endif

JSHTMLElementWrapperFactory.cpp : HTMLNames.cpp

XMLNSNames.cpp : dom/make_names.pl xml/xmlnsattrs.in
	perl -I $(WebCore)/bindings/scripts $< --attrs $(WebCore)/xml/xmlnsattrs.in

XMLNames.cpp : dom/make_names.pl xml/xmlattrs.in
	perl -I $(WebCore)/bindings/scripts $< --attrs $(WebCore)/xml/xmlattrs.in

# --------

# SVG tag and attribute names, and element factory

ifeq ($(findstring ENABLE_SVG,$(FEATURE_DEFINES)), ENABLE_SVG)

ifeq ($(findstring ENABLE_SVG_DOM_OBJC_BINDINGS,$(FEATURE_DEFINES)), ENABLE_SVG_DOM_OBJC_BINDINGS)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.SVG.exp
endif

ifeq ($(findstring ENABLE_SVG_USE,$(FEATURE_DEFINES)), ENABLE_SVG_USE)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_USE=1
endif

ifeq ($(findstring ENABLE_SVG_FONTS,$(FEATURE_DEFINES)), ENABLE_SVG_FONTS)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_FONTS=1
endif

ifeq ($(findstring ENABLE_FILTERS,$(FEATURE_DEFINES)), ENABLE_FILTERS)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_FILTERS=1
ifeq ($(findstring ENABLE_SVG_DOM_OBJC_BINDINGS,$(FEATURE_DEFINES)), ENABLE_SVG_DOM_OBJC_BINDINGS)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.SVG.Filters.exp
endif
endif

ifeq ($(findstring ENABLE_SVG_AS_IMAGE,$(FEATURE_DEFINES)), ENABLE_SVG_AS_IMAGE)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_AS_IMAGE=1
endif

ifeq ($(findstring ENABLE_SVG_ANIMATION,$(FEATURE_DEFINES)), ENABLE_SVG_ANIMATION)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_ANIMATION=1
ifeq ($(findstring ENABLE_SVG_DOM_OBJC_BINDINGS,$(FEATURE_DEFINES)), ENABLE_SVG_DOM_OBJC_BINDINGS)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.SVG.Animation.exp
endif
endif

ifeq ($(findstring ENABLE_SVG_FOREIGN_OBJECT,$(FEATURE_DEFINES)), ENABLE_SVG_FOREIGN_OBJECT)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_FOREIGN_OBJECT=1
ifeq ($(findstring ENABLE_SVG_DOM_OBJC_BINDINGS,$(FEATURE_DEFINES)), ENABLE_SVG_DOM_OBJC_BINDINGS)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.SVG.ForeignObject.exp
endif
endif

# SVG tag and attribute names (need to pass an extra flag if svg experimental features are enabled)

ifdef SVG_FLAGS

SVGElementFactory.cpp SVGNames.cpp : dom/make_names.pl svg/svgtags.in svg/svgattrs.in
	perl -I $(WebCore)/bindings/scripts $< --tags $(WebCore)/svg/svgtags.in --attrs $(WebCore)/svg/svgattrs.in --extraDefines "$(SVG_FLAGS)" --factory --wrapperFactory
else

SVGElementFactory.cpp SVGNames.cpp : dom/make_names.pl svg/svgtags.in svg/svgattrs.in
	perl -I $(WebCore)/bindings/scripts $< --tags $(WebCore)/svg/svgtags.in --attrs $(WebCore)/svg/svgattrs.in --factory --wrapperFactory

endif

JSSVGElementWrapperFactory.cpp : SVGNames.cpp

XLinkNames.cpp : dom/make_names.pl svg/xlinkattrs.in
	perl -I $(WebCore)/bindings/scripts $< --attrs $(WebCore)/svg/xlinkattrs.in

else

SVGElementFactory.cpp :
	echo > $@

SVGNames.cpp :
	echo > $@

XLinkNames.cpp :
	echo > $@

# This file is autogenerated by make_names.pl when SVG is enabled.

JSSVGElementWrapperFactory.cpp :
	echo > $@

endif

# --------

# WML tag and attribute names, and element factory

ifeq ($(findstring ENABLE_WML,$(FEATURE_DEFINES)), ENABLE_WML)

WMLElementFactory.cpp WMLNames.cpp : dom/make_names.pl wml/WMLTagNames.in wml/WMLAttributeNames.in
	perl -I $(WebCore)/bindings/scripts $< --tags $(WebCore)/wml/WMLTagNames.in --attrs $(WebCore)/wml/WMLAttributeNames.in --factory --wrapperFactory

else

WMLElementFactory.cpp :
	echo > $@

WMLNames.cpp :
	echo > $@

endif

# --------
 
# MathML tag and attribute names, and element factory

ifeq ($(findstring ENABLE_MATHML,$(FEATURE_DEFINES)), ENABLE_MATHML)

MathMLElementFactory.cpp MathMLNames.cpp : dom/make_names.pl mathml/mathtags.in mathml/mathattrs.in
	perl -I $(WebCore)/bindings/scripts $< --tags $(WebCore)/mathml/mathtags.in --attrs $(WebCore)/mathml/mathattrs.in --factory --wrapperFactory

else

MathMLElementFactory.cpp :
	echo > $@

MathMLNames.cpp :
	echo > $@

endif



# --------

# JavaScript bindings

GENERATE_BINDINGS = perl -I $(WebCore)/bindings/scripts $(WebCore)/bindings/scripts/generate-bindings.pl \
    --include dom --include html --include css --include page --include notifications --include xml --include svg --write-dependencies --outputDir .

GENERATE_BINDINGS_SCRIPTS = \
    bindings/scripts/CodeGenerator.pm \
    bindings/scripts/IDLParser.pm \
    bindings/scripts/IDLStructure.pm \
    bindings/scripts/generate-bindings.pl \
#

JS%.h : %.idl $(GENERATE_BINDINGS_SCRIPTS) bindings/scripts/CodeGeneratorJS.pm
	$(GENERATE_BINDINGS) --defines "$(FEATURE_DEFINES) $(ADDITIONAL_IDL_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS $<

-include $(JS_DOM_HEADERS:.h=.dep)

# ------------------------

# Mac-specific rules

ifeq ($(OS),MACOS)

OBJC_DOM_HEADERS=$(filter-out DOMDOMWindow.h DOMMimeType.h DOMPlugin.h,$(DOM_CLASSES:%=DOM%.h))

all : $(OBJC_DOM_HEADERS)

all : CharsetData.cpp WebCore.exp WebCore.LP64.exp

# --------

# character set name table

CharsetData.cpp : platform/text/mac/make-charset-table.pl platform/text/mac/character-sets.txt platform/text/mac/mac-encodings.txt
	perl $^ kTextEncoding > $@

# --------

# export file

ifeq ($(shell gcc -E -P -dM $(FRAMEWORK_FLAGS) WebCore/ForwardingHeaders/wtf/Platform.h | grep ENABLE_MAC_JAVA_BRIDGE | cut -d' ' -f3), 1)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.JNI.exp
endif

ifeq ($(shell gcc -E -P -dM $(FRAMEWORK_FLAGS) WebCore/ForwardingHeaders/wtf/Platform.h | grep ENABLE_NETSCAPE_PLUGIN_API | cut -d' ' -f3), 1)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.NPAPI.exp
endif

# FIXME: WTF_USE_PLUGIN_HOST_PROCESS is only true when building for x86_64, but we shouldn't have to know about that here.
ifeq ($(findstring x86_64,$(ARCHS)) $(findstring x86_64,$(VALID_ARCHS)), x86_64 x86_64)
ifeq ($(shell gcc -arch x86_64 -E -P -dM $(FRAMEWORK_FLAGS) WebCore/ForwardingHeaders/wtf/Platform.h | grep WTF_USE_PLUGIN_HOST_PROCESS | cut -d' ' -f3), 1)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.PluginHostProcess.exp
endif
endif

ifeq ($(findstring ENABLE_3D_RENDERING,$(FEATURE_DEFINES)), ENABLE_3D_RENDERING)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.3DRendering.exp
endif

ifeq ($(shell gcc -E -P -dM $(FRAMEWORK_FLAGS) WebCore/ForwardingHeaders/wtf/Platform.h | grep ENABLE_CONTEXT_MENUS | cut -d' ' -f3), 1)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.ContextMenus.exp
endif

ifeq ($(ENABLE_DASHBOARD_SUPPORT), 1)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.DashboardSupport.exp
endif

ifeq ($(shell gcc -E -P -dM $(FRAMEWORK_FLAGS) WebCore/ForwardingHeaders/wtf/Platform.h | grep ENABLE_DRAG_SUPPORT | cut -d' ' -f3), 1)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.DragSupport.exp
endif

ifeq ($(shell gcc -E -P -dM $(FRAMEWORK_FLAGS) WebCore/ForwardingHeaders/wtf/Platform.h | grep ENABLE_INSPECTOR | cut -d' ' -f3), 1)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.Inspector.exp
endif

ifeq ($(ENABLE_ORIENTATION_EVENTS), 1)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.OrientationEvents.exp
endif

ifeq ($(findstring 10.4,$(MACOSX_DEPLOYMENT_TARGET)), 10.4)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.Tiger.exp
endif

ifeq ($(findstring ENABLE_PLUGIN_PROXY_FOR_VIDEO,$(FEATURE_DEFINES)), ENABLE_PLUGIN_PROXY_FOR_VIDEO)
     WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.VideoProxy.exp
endif

ifeq ($(findstring ENABLE_VIDEO,$(FEATURE_DEFINES)), ENABLE_VIDEO)
     WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.Video.exp
endif


ifeq ($(findstring ENABLE_CLIENT_BASED_GEOLOCATION,$(FEATURE_DEFINES)), ENABLE_CLIENT_BASED_GEOLOCATION)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.ClientBasedGeolocation.exp
endif

WebCore.exp : WebCore.base.exp $(WEBCORE_EXPORT_DEPENDENCIES)
	cat $^ > $@

# Switch NSRect, NSSize and NSPoint with their CG counterparts for 64-bit.
WebCore.LP64.exp : WebCore.exp
	cat $^ | sed -e s/7_NSRect/6CGRect/ -e s/7_NSSize/6CGSize/ -e s/8_NSPoint/7CGPoint/ > $@

# --------

# Objective-C bindings

DOM%.h : %.idl $(GENERATE_BINDINGS_SCRIPTS) bindings/scripts/CodeGeneratorObjC.pm bindings/objc/PublicDOMInterfaces.h
	$(GENERATE_BINDINGS) --defines "$(FEATURE_DEFINES) $(ADDITIONAL_IDL_DEFINES) LANGUAGE_OBJECTIVE_C" --generator ObjC $<

-include $(OBJC_DOM_HEADERS:.h=.dep)

# --------

endif # MACOS

# ------------------------
