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

js_binding_scripts := \
	$(LOCAL_PATH)/bindings/scripts/CodeGenerator.pm \
	$(LOCAL_PATH)/bindings/scripts/CodeGeneratorV8.pm \
	$(LOCAL_PATH)/bindings/scripts/IDLParser.pm \
	$(LOCAL_PATH)/bindings/scripts/IDLStructure.pm \
	$(LOCAL_PATH)/bindings/scripts/generate-bindings.pl

FEATURE_DEFINES := ANDROID_ORIENTATION_SUPPORT ENABLE_TOUCH_EVENTS=1 V8_BINDING ENABLE_DATABASE=1 ENABLE_OFFLINE_WEB_APPLICATIONS=1 ENABLE_DOM_STORAGE=1 ENABLE_VIDEO=1 ENABLE_WORKERS=1 ENABLE_GEOLOCATION=1

# CSS
GEN := \
    $(intermediates)/bindings/V8CSSCharsetRule.h \
    $(intermediates)/bindings/V8CSSFontFaceRule.h \
    $(intermediates)/bindings/V8CSSImportRule.h \
    $(intermediates)/bindings/V8CSSMediaRule.h \
    $(intermediates)/bindings/V8CSSPageRule.h \
    $(intermediates)/bindings/V8CSSPrimitiveValue.h \
    $(intermediates)/bindings/V8CSSRule.h \
    $(intermediates)/bindings/V8CSSRuleList.h \
    $(intermediates)/bindings/V8CSSStyleDeclaration.h \
    $(intermediates)/bindings/V8CSSStyleRule.h \
    $(intermediates)/bindings/V8CSSStyleSheet.h \
    $(intermediates)/bindings/V8CSSUnknownRule.h \
    $(intermediates)/bindings/V8CSSValue.h \
    $(intermediates)/bindings/V8CSSValueList.h \
    $(intermediates)/bindings/V8CSSVariablesDeclaration.h \
    $(intermediates)/bindings/V8CSSVariablesRule.h \
    $(intermediates)/bindings/V8Counter.h \
    $(intermediates)/bindings/V8MediaList.h \
    $(intermediates)/bindings/V8Rect.h \
    $(intermediates)/bindings/V8RGBColor.h \
    $(intermediates)/bindings/V8StyleMedia.h \
    $(intermediates)/bindings/V8StyleSheet.h \
    $(intermediates)/bindings/V8StyleSheetList.h  \
    $(intermediates)/bindings/V8WebKitCSSKeyframeRule.h \
    $(intermediates)/bindings/V8WebKitCSSKeyframesRule.h \
    $(intermediates)/bindings/V8WebKitCSSMatrix.h \
    $(intermediates)/bindings/V8WebKitCSSTransformValue.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(PRIVATE_PATH) perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include css --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(LOCAL_PATH)/css/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# DOM
GEN := \
    $(intermediates)/bindings/V8Attr.h \
    $(intermediates)/bindings/V8BeforeLoadEvent.h \
    $(intermediates)/bindings/V8CDATASection.h \
    $(intermediates)/bindings/V8CharacterData.h \
    $(intermediates)/bindings/V8ClientRect.h \
    $(intermediates)/bindings/V8ClientRectList.h \
    $(intermediates)/bindings/V8Clipboard.h \
    $(intermediates)/bindings/V8Comment.h \
    $(intermediates)/bindings/V8DOMCoreException.h \
    $(intermediates)/bindings/V8DOMImplementation.h \
    $(intermediates)/bindings/V8Document.h \
    $(intermediates)/bindings/V8DocumentFragment.h \
    $(intermediates)/bindings/V8DocumentType.h \
    $(intermediates)/bindings/V8Element.h \
    $(intermediates)/bindings/V8Entity.h \
    $(intermediates)/bindings/V8EntityReference.h \
    $(intermediates)/bindings/V8ErrorEvent.h \
    $(intermediates)/bindings/V8Event.h \
    $(intermediates)/bindings/V8EventException.h \
    $(intermediates)/bindings/V8KeyboardEvent.h \
    $(intermediates)/bindings/V8MessageChannel.h \
    $(intermediates)/bindings/V8MessageEvent.h \
    $(intermediates)/bindings/V8MessagePort.h \
    $(intermediates)/bindings/V8MouseEvent.h \
    $(intermediates)/bindings/V8MutationEvent.h \
    $(intermediates)/bindings/V8NamedNodeMap.h \
    $(intermediates)/bindings/V8Node.h \
    $(intermediates)/bindings/V8NodeFilter.h \
    $(intermediates)/bindings/V8NodeIterator.h \
    $(intermediates)/bindings/V8NodeList.h \
    $(intermediates)/bindings/V8Notation.h \
    $(intermediates)/bindings/V8OverflowEvent.h \
    $(intermediates)/bindings/V8PageTransitionEvent.h \
    $(intermediates)/bindings/V8ProcessingInstruction.h \
    $(intermediates)/bindings/V8ProgressEvent.h \
    $(intermediates)/bindings/V8Range.h \
    $(intermediates)/bindings/V8RangeException.h \
    $(intermediates)/bindings/V8Text.h \
    $(intermediates)/bindings/V8TextEvent.h \
    $(intermediates)/bindings/V8Touch.h \
    $(intermediates)/bindings/V8TouchEvent.h \
    $(intermediates)/bindings/V8TouchList.h \
    $(intermediates)/bindings/V8TreeWalker.h \
    $(intermediates)/bindings/V8UIEvent.h \
    $(intermediates)/bindings/V8WebKitAnimationEvent.h \
    $(intermediates)/bindings/V8WebKitTransitionEvent.h \
    $(intermediates)/bindings/V8WheelEvent.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(PRIVATE_PATH) perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(LOCAL_PATH)/dom/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# HTML
GEN := \
    $(intermediates)/bindings/V8Blob.h \
    $(intermediates)/bindings/V8DataGridColumn.h \
    $(intermediates)/bindings/V8DataGridColumnList.h \
    $(intermediates)/bindings/V8File.h \
    $(intermediates)/bindings/V8FileError.h \
    $(intermediates)/bindings/V8FileList.h \
    $(intermediates)/bindings/V8HTMLAllCollection.h \
    $(intermediates)/bindings/V8HTMLAnchorElement.h \
    $(intermediates)/bindings/V8HTMLAppletElement.h \
    $(intermediates)/bindings/V8HTMLAreaElement.h \
    $(intermediates)/bindings/V8HTMLAudioElement.h \
    $(intermediates)/bindings/V8HTMLBRElement.h \
    $(intermediates)/bindings/V8HTMLBaseElement.h \
    $(intermediates)/bindings/V8HTMLBaseFontElement.h \
    $(intermediates)/bindings/V8HTMLBlockquoteElement.h \
    $(intermediates)/bindings/V8HTMLBodyElement.h \
    $(intermediates)/bindings/V8HTMLButtonElement.h \
    $(intermediates)/bindings/V8HTMLCanvasElement.h \
    $(intermediates)/bindings/V8HTMLCollection.h \
    $(intermediates)/bindings/V8HTMLDataGridCellElement.h \
    $(intermediates)/bindings/V8HTMLDataGridColElement.h \
    $(intermediates)/bindings/V8HTMLDataGridElement.h \
    $(intermediates)/bindings/V8HTMLDataGridRowElement.h \
    $(intermediates)/bindings/V8HTMLDataListElement.h \
    $(intermediates)/bindings/V8HTMLDListElement.h \
    $(intermediates)/bindings/V8HTMLDirectoryElement.h \
    $(intermediates)/bindings/V8HTMLDivElement.h \
    $(intermediates)/bindings/V8HTMLDocument.h \
    $(intermediates)/bindings/V8HTMLElement.h \
    $(intermediates)/bindings/V8HTMLEmbedElement.h \
    $(intermediates)/bindings/V8HTMLFieldSetElement.h \
    $(intermediates)/bindings/V8HTMLFontElement.h \
    $(intermediates)/bindings/V8HTMLFormElement.h \
    $(intermediates)/bindings/V8HTMLFrameElement.h \
    $(intermediates)/bindings/V8HTMLFrameSetElement.h \
    $(intermediates)/bindings/V8HTMLHRElement.h \
    $(intermediates)/bindings/V8HTMLHeadElement.h \
    $(intermediates)/bindings/V8HTMLHeadingElement.h \
    $(intermediates)/bindings/V8HTMLHtmlElement.h \
    $(intermediates)/bindings/V8HTMLIFrameElement.h \
    $(intermediates)/bindings/V8HTMLImageElement.h \
    $(intermediates)/bindings/V8HTMLInputElement.h \
    $(intermediates)/bindings/V8HTMLIsIndexElement.h \
    $(intermediates)/bindings/V8HTMLLIElement.h \
    $(intermediates)/bindings/V8HTMLLabelElement.h \
    $(intermediates)/bindings/V8HTMLLegendElement.h \
    $(intermediates)/bindings/V8HTMLLinkElement.h \
    $(intermediates)/bindings/V8HTMLMapElement.h \
    $(intermediates)/bindings/V8HTMLMarqueeElement.h \
    $(intermediates)/bindings/V8HTMLMediaElement.h \
    $(intermediates)/bindings/V8HTMLMenuElement.h \
    $(intermediates)/bindings/V8HTMLMetaElement.h \
    $(intermediates)/bindings/V8HTMLModElement.h \
    $(intermediates)/bindings/V8HTMLOListElement.h \
    $(intermediates)/bindings/V8HTMLObjectElement.h \
    $(intermediates)/bindings/V8HTMLOptGroupElement.h \
    $(intermediates)/bindings/V8HTMLOptionElement.h \
    $(intermediates)/bindings/V8HTMLOptionsCollection.h \
    $(intermediates)/bindings/V8HTMLParagraphElement.h \
    $(intermediates)/bindings/V8HTMLParamElement.h \
    $(intermediates)/bindings/V8HTMLPreElement.h \
    $(intermediates)/bindings/V8HTMLQuoteElement.h \
    $(intermediates)/bindings/V8HTMLScriptElement.h \
    $(intermediates)/bindings/V8HTMLSelectElement.h \
    $(intermediates)/bindings/V8HTMLSourceElement.h \
    $(intermediates)/bindings/V8HTMLStyleElement.h \
    $(intermediates)/bindings/V8HTMLTableCaptionElement.h \
    $(intermediates)/bindings/V8HTMLTableCellElement.h \
    $(intermediates)/bindings/V8HTMLTableColElement.h \
    $(intermediates)/bindings/V8HTMLTableElement.h \
    $(intermediates)/bindings/V8HTMLTableRowElement.h \
    $(intermediates)/bindings/V8HTMLTableSectionElement.h \
    $(intermediates)/bindings/V8HTMLTextAreaElement.h \
    $(intermediates)/bindings/V8HTMLTitleElement.h \
    $(intermediates)/bindings/V8HTMLUListElement.h \
    $(intermediates)/bindings/V8HTMLVideoElement.h \
    $(intermediates)/bindings/V8ImageData.h \
    $(intermediates)/bindings/V8MediaError.h \
    $(intermediates)/bindings/V8TextMetrics.h \
    $(intermediates)/bindings/V8TimeRanges.h \
    $(intermediates)/bindings/V8ValidityState.h \
    $(intermediates)/bindings/V8VoidCallback.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(PRIVATE_PATH) perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(LOCAL_PATH)/html/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# Canvas
GEN := \
    $(intermediates)/bindings/V8CanvasGradient.h \
    $(intermediates)/bindings/V8CanvasNumberArray.h \
    $(intermediates)/bindings/V8CanvasPattern.h \
    $(intermediates)/bindings/V8CanvasRenderingContext.h \
    $(intermediates)/bindings/V8CanvasRenderingContext2D.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(PRIVATE_PATH) perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --include html/canvas --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(LOCAL_PATH)/html/canvas/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# Appcache
GEN := \
    $(intermediates)/bindings/V8DOMApplicationCache.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(PRIVATE_PATH) perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(LOCAL_PATH)/loader/appcache/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# Page
GEN := \
    $(intermediates)/bindings/V8BarInfo.h \
    $(intermediates)/bindings/V8Console.h \
    $(intermediates)/bindings/V8Coordinates.h \
    $(intermediates)/bindings/V8DOMSelection.h \
    $(intermediates)/bindings/V8DOMWindow.h \
    $(intermediates)/bindings/V8Geolocation.h \
    $(intermediates)/bindings/V8Geoposition.h \
    $(intermediates)/bindings/V8History.h \
    $(intermediates)/bindings/V8Location.h \
    $(intermediates)/bindings/V8Navigator.h \
    $(intermediates)/bindings/V8PositionError.h \
    $(intermediates)/bindings/V8Screen.h \
    $(intermediates)/bindings/V8WebKitPoint.h \
    $(intermediates)/bindings/V8WorkerNavigator.h
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(PRIVATE_PATH) perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(LOCAL_PATH)/page/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

GEN := \
    $(intermediates)/bindings/V8MimeType.h \
    $(intermediates)/bindings/V8MimeTypeArray.h \
    $(intermediates)/bindings/V8Plugin.h \
    $(intermediates)/bindings/V8PluginArray.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(PRIVATE_PATH) perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(LOCAL_PATH)/plugins/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# Database
GEN := \
    $(intermediates)/bindings/V8Database.h \
    $(intermediates)/bindings/V8SQLError.h \
    $(intermediates)/bindings/V8SQLResultSet.h \
    $(intermediates)/bindings/V8SQLResultSetRowList.h \
    $(intermediates)/bindings/V8SQLTransaction.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(PRIVATE_PATH) perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(LOCAL_PATH)/storage/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# DOM Storage
GEN := \
    $(intermediates)/bindings/V8Storage.h \
    $(intermediates)/bindings/V8StorageEvent.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(PRIVATE_PATH) perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(LOCAL_PATH)/storage/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# SVG
# These headers are required by the V8 bindings even when SVG is disabled
GEN := \
    $(intermediates)/bindings/V8SVGColor.h \
    $(intermediates)/bindings/V8SVGDocument.h \
    $(intermediates)/bindings/V8SVGElement.h \
    $(intermediates)/bindings/V8SVGElementInstance.h \
    $(intermediates)/bindings/V8SVGException.h \
    $(intermediates)/bindings/V8SVGPaint.h \
    $(intermediates)/bindings/V8SVGZoomEvent.h

ifeq ($(ENABLE_SVG), true)
GEN += \
    $(intermediates)/bindings/V8SVGAElement.h \
    $(intermediates)/bindings/V8SVGAltGlyphElement.h \
    $(intermediates)/bindings/V8SVGAngle.h \
    $(intermediates)/bindings/V8SVGCircleElement.h \
    $(intermediates)/bindings/V8SVGClipPathElement.h \
    $(intermediates)/bindings/V8SVGComponentTransferFunctionElement.h \
    $(intermediates)/bindings/V8SVGCursorElement.h \
    $(intermediates)/bindings/V8SVGDefsElement.h \
    $(intermediates)/bindings/V8SVGDescElement.h \
    $(intermediates)/bindings/V8SVGElement.h \
    $(intermediates)/bindings/V8SVGElementInstanceList.h \
    $(intermediates)/bindings/V8SVGEllipseElement.h \
    $(intermediates)/bindings/V8SVGFEBlendElement.h \
    $(intermediates)/bindings/V8SVGFEColorMatrixElement.h \
    $(intermediates)/bindings/V8SVGFEComponentTransferElement.h \
    $(intermediates)/bindings/V8SVGFECompositeElement.h \
    $(intermediates)/bindings/V8SVGFEDiffuseLightingElement.h \
    $(intermediates)/bindings/V8SVGFEDisplacementMapElement.h \
    $(intermediates)/bindings/V8SVGFEDistantLightElement.h \
    $(intermediates)/bindings/V8SVGFEFloodElement.h \
    $(intermediates)/bindings/V8SVGFEFuncAElement.h \
    $(intermediates)/bindings/V8SVGFEFuncBElement.h \
    $(intermediates)/bindings/V8SVGFEFuncGElement.h \
    $(intermediates)/bindings/V8SVGFEFuncRElement.h \
    $(intermediates)/bindings/V8SVGFEGaussianBlurElement.h \
    $(intermediates)/bindings/V8SVGFEImageElement.h \
    $(intermediates)/bindings/V8SVGFEMergeElement.h \
    $(intermediates)/bindings/V8SVGFEMergeNodeElement.h \
    $(intermediates)/bindings/V8SVGFEOffsetElement.h \
    $(intermediates)/bindings/V8SVGFEPointLightElement.h \
    $(intermediates)/bindings/V8SVGFESpecularLightingElement.h \
    $(intermediates)/bindings/V8SVGFESpotLightElement.h \
    $(intermediates)/bindings/V8SVGFETileElement.h \
    $(intermediates)/bindings/V8SVGFETurbulenceElement.h \
    $(intermediates)/bindings/V8SVGFilterElement.h \
    $(intermediates)/bindings/V8SVGFontElement.h \
    $(intermediates)/bindings/V8SVGFontFaceElement.h \
    $(intermediates)/bindings/V8SVGFontFaceFormatElement.h \
    $(intermediates)/bindings/V8SVGFontFaceNameElement.h \
    $(intermediates)/bindings/V8SVGFontFaceSrcElement.h \
    $(intermediates)/bindings/V8SVGFontFaceUriElement.h \
    $(intermediates)/bindings/V8SVGForeignObjectElement.h \
    $(intermediates)/bindings/V8SVGGElement.h \
    $(intermediates)/bindings/V8SVGGlyphElement.h \
    $(intermediates)/bindings/V8SVGGradientElement.h \
    $(intermediates)/bindings/V8SVGHKernElement.h \
    $(intermediates)/bindings/V8SVGImageElement.h \
    $(intermediates)/bindings/V8SVGLength.h \
    $(intermediates)/bindings/V8SVGLengthList.h \
    $(intermediates)/bindings/V8SVGLineElement.h \
    $(intermediates)/bindings/V8SVGLinearGradientElement.h \
    $(intermediates)/bindings/V8SVGMarkerElement.h \
    $(intermediates)/bindings/V8SVGMaskElement.h \
    $(intermediates)/bindings/V8SVGMatrix.h \
    $(intermediates)/bindings/V8SVGMetadataElement.h \
    $(intermediates)/bindings/V8SVGMissingGlyphElement.h \
    $(intermediates)/bindings/V8SVGNumber.h \
    $(intermediates)/bindings/V8SVGNumberList.h \
    $(intermediates)/bindings/V8SVGPathElement.h \
    $(intermediates)/bindings/V8SVGPathSeg.h \
    $(intermediates)/bindings/V8SVGPathSegArcAbs.h \
    $(intermediates)/bindings/V8SVGPathSegArcRel.h \
    $(intermediates)/bindings/V8SVGPathSegClosePath.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoCubicAbs.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoCubicRel.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoCubicSmoothAbs.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoCubicSmoothRel.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoQuadraticAbs.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoQuadraticRel.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoQuadraticSmoothAbs.h \
    $(intermediates)/bindings/V8SVGPathSegCurvetoQuadraticSmoothRel.h \
    $(intermediates)/bindings/V8SVGPathSegLinetoAbs.h \
    $(intermediates)/bindings/V8SVGPathSegLinetoHorizontalAbs.h \
    $(intermediates)/bindings/V8SVGPathSegLinetoHorizontalRel.h \
    $(intermediates)/bindings/V8SVGPathSegLinetoRel.h \
    $(intermediates)/bindings/V8SVGPathSegLinetoVerticalAbs.h \
    $(intermediates)/bindings/V8SVGPathSegLinetoVerticalRel.h \
    $(intermediates)/bindings/V8SVGPathSegList.h \
    $(intermediates)/bindings/V8SVGPathSegMovetoAbs.h \
    $(intermediates)/bindings/V8SVGPathSegMovetoRel.h \
    $(intermediates)/bindings/V8SVGPatternElement.h \
    $(intermediates)/bindings/V8SVGPoint.h \
    $(intermediates)/bindings/V8SVGPointList.h \
    $(intermediates)/bindings/V8SVGPolygonElement.h \
    $(intermediates)/bindings/V8SVGPolylineElement.h \
    $(intermediates)/bindings/V8SVGPreserveAspectRatio.h \
    $(intermediates)/bindings/V8SVGRadialGradientElement.h \
    $(intermediates)/bindings/V8SVGRect.h \
    $(intermediates)/bindings/V8SVGRectElement.h \
    $(intermediates)/bindings/V8SVGRenderingIntent.h \
    $(intermediates)/bindings/V8SVGSVGElement.h \
    $(intermediates)/bindings/V8SVGScriptElement.h \
    $(intermediates)/bindings/V8SVGStopElement.h \
    $(intermediates)/bindings/V8SVGStringList.h \
    $(intermediates)/bindings/V8SVGStyleElement.h \
    $(intermediates)/bindings/V8SVGSwitchElement.h \
    $(intermediates)/bindings/V8SVGSymbolElement.h \
    $(intermediates)/bindings/V8SVGTRefElement.h \
    $(intermediates)/bindings/V8SVGTSpanElement.h \
    $(intermediates)/bindings/V8SVGTextContentElement.h \
    $(intermediates)/bindings/V8SVGTextElement.h \
    $(intermediates)/bindings/V8SVGTextPathElement.h \
    $(intermediates)/bindings/V8SVGTextPositioningElement.h \
    $(intermediates)/bindings/V8SVGTitleElement.h \
    $(intermediates)/bindings/V8SVGTransform.h \
    $(intermediates)/bindings/V8SVGTransformList.h \
    $(intermediates)/bindings/V8SVGURIReference.h \
    $(intermediates)/bindings/V8SVGUnitTypes.h \
    $(intermediates)/bindings/V8SVGUseElement.h \
    $(intermediates)/bindings/V8SVGViewElement.h \
    \
    $(intermediates)/bindings/V8SVGAnimatedAngle.h \
    $(intermediates)/bindings/V8SVGAnimatedEnumeration.h \
    $(intermediates)/bindings/V8SVGAnimatedBoolean.h \
    $(intermediates)/bindings/V8SVGAnimatedInteger.h \
    $(intermediates)/bindings/V8SVGAnimatedLength.h \
    $(intermediates)/bindings/V8SVGAnimatedLengthList.h \
    $(intermediates)/bindings/V8SVGAnimatedNumber.h \
    $(intermediates)/bindings/V8SVGAnimatedNumberList.h \
    $(intermediates)/bindings/V8SVGAnimatedPoints.h \
    $(intermediates)/bindings/V8SVGAnimatedPreserveAspectRatio.h \
    $(intermediates)/bindings/V8SVGAnimatedRect.h \
    $(intermediates)/bindings/V8SVGAnimatedString.h \
    $(intermediates)/bindings/V8SVGAnimatedTransformList.h
endif

ifeq ($(ENABLE_SVG_ANIMATION), true)
GEN += \
    $(intermediates)/bindings/V8SVGAnimateColorElement.h \
    $(intermediates)/bindings/V8SVGAnimateElement.h \
    $(intermediates)/bindings/V8SVGAnimateTransformElement.h \
    $(intermediates)/bindings/V8SVGAnimationElement.h \
    $(intermediates)/bindings/V8SVGSetElement.h
endif

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(PRIVATE_PATH) perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include css --include dom --include html --include svg --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(LOCAL_PATH)/svg/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# Workers
GEN := \
	$(intermediates)/bindings/V8AbstractWorker.h \
	$(intermediates)/bindings/V8DedicatedWorkerContext.h \
	$(intermediates)/bindings/V8SharedWorker.h \
	$(intermediates)/bindings/V8SharedWorkerContext.h \
	$(intermediates)/bindings/V8Worker.h \
	$(intermediates)/bindings/V8WorkerContext.h \
	$(intermediates)/bindings/V8WorkerLocation.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(PRIVATE_PATH) perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --include workers --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(LOCAL_PATH)/workers/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# XML
GEN := \
    $(intermediates)/bindings/V8DOMFormData.h \
    $(intermediates)/bindings/V8DOMParser.h \
    $(intermediates)/bindings/V8XMLHttpRequest.h \
    $(intermediates)/bindings/V8XMLHttpRequestException.h \
    $(intermediates)/bindings/V8XMLHttpRequestProgressEvent.h \
    $(intermediates)/bindings/V8XMLHttpRequestUpload.h \
    $(intermediates)/bindings/V8XMLSerializer.h \
    $(intermediates)/bindings/V8XPathException.h \
    $(intermediates)/bindings/V8XPathNSResolver.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(PRIVATE_PATH) perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(LOCAL_PATH)/xml/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h
#end

# Inspector
# These headers are required by the V8 bindings even when Inspector is disabled
GEN := \
    $(intermediates)/bindings/V8InjectedScriptHost.h \
    $(intermediates)/bindings/V8InspectorBackend.h \
    $(intermediates)/bindings/V8InspectorFrontendHost.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(PRIVATE_PATH) perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(LOCAL_PATH)/inspector/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# Notifications
# These headers are required by the V8 bindings even when Notifications are disabled
GEN := \
    $(intermediates)/bindings/V8Notification.h \
    $(intermediates)/bindings/V8NotificationCenter.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(PRIVATE_PATH) perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(LOCAL_PATH)/notifications/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# Web Sockets
# These headers are required by the V8 bindings even when Web Sockets are disabled
GEN := \
    $(intermediates)/bindings/V8WebSocket.h

$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = SOURCE_ROOT=$(PRIVATE_PATH) perl -I$(PRIVATE_PATH)/bindings/scripts $(PRIVATE_PATH)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator V8 --include dom --include html --outputdir $(dir $@) $<
$(GEN): $(intermediates)/bindings/V8%.h : $(LOCAL_PATH)/websockets/%.idl $(js_binding_scripts)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/bindings/%.cpp : $(intermediates)/bindings/%.h

# HTML tag and attribute names
GEN:= $(intermediates)/HTMLNames.cpp $(intermediates)/HTMLElementFactory.cpp
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I $(PRIVATE_PATH)/bindings/scripts $< --tags $(PRIVATE_PATH)/html/HTMLTagNames.in --attrs $(PRIVATE_PATH)/html/HTMLAttributeNames.in --factory --wrapperFactory --output $(dir $@)
$(GEN): $(LOCAL_PATH)/dom/make_names.pl $(LOCAL_PATH)/html/HTMLTagNames.in $(LOCAL_PATH)/html/HTMLAttributeNames.in
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# SVG tag and attribute names
ifeq ($(ENABLE_SVG), true)
GEN:= $(intermediates)/SVGNames.cpp  $(intermediates)/SVGElementFactory.cpp
SVG_FLAGS:=ENABLE_SVG_AS_IMAGE=1 ENABLE_SVG_FILTERS=1 ENABLE_SVG_FONTS=1 ENABLE_SVG_FOREIGN_OBJECT=1 ENABLE_SVG_USE=1
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I $(PRIVATE_PATH)/bindings/scripts $< --tags $(PRIVATE_PATH)/svg/svgtags.in --attrs $(PRIVATE_PATH)/svg/svgattrs.in --extraDefines "$(SVG_FLAGS)" --factory --wrapperFactory --output $(dir $@)
$(GEN): $(LOCAL_PATH)/dom/make_names.pl $(LOCAL_PATH)/svg/svgtags.in $(LOCAL_PATH)/svg/svgattrs.in
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)
endif
