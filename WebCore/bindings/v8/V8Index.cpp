/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8Index.h"

#include "V8Attr.h"
#include "V8BarInfo.h"
#include "V8BeforeLoadEvent.h"
#include "V8CanvasActiveInfo.h"
#include "V8CanvasRenderingContext.h"
#include "V8CanvasRenderingContext2D.h"
#include "V8CanvasGradient.h"
#include "V8CanvasPattern.h"
#include "V8CanvasPixelArray.h"
#include "V8CDATASection.h"
#include "V8CharacterData.h"
#include "V8ClientRect.h"
#include "V8ClientRectList.h"
#include "V8Clipboard.h"
#include "V8Comment.h"
#include "V8Console.h"
#include "V8Counter.h"
#include "V8CSSStyleDeclaration.h"
#include "V8CSSRule.h"
#include "V8CSSStyleRule.h"
#include "V8CSSCharsetRule.h"
#include "V8CSSImportRule.h"
#include "V8CSSMediaRule.h"
#include "V8CSSFontFaceRule.h"
#include "V8CSSPageRule.h"
#include "V8CSSRuleList.h"
#include "V8CSSPrimitiveValue.h"
#include "V8CSSValue.h"
#include "V8CSSValueList.h"
#include "V8CSSStyleSheet.h"
#include "V8CSSVariablesDeclaration.h"
#include "V8CSSVariablesRule.h"
#include "V8DataGridColumn.h"
#include "V8DataGridColumnList.h"
#include "V8Database.h"
#include "V8Document.h"
#include "V8DocumentFragment.h"
#include "V8DocumentType.h"
#include "V8Element.h"
#include "V8Entity.h"
#include "V8EntityReference.h"
#include "V8File.h"
#include "V8FileList.h"
#include "V8History.h"
#include "V8HTMLAllCollection.h"
#include "V8HTMLCanvasElement.h"
#include "V8HTMLCollection.h"
#include "V8HTMLDocument.h"
#include "V8HTMLElement.h"
#include "V8HTMLOptionsCollection.h"
#include "V8HTMLAnchorElement.h"
#include "V8HTMLAppletElement.h"
#include "V8HTMLAreaElement.h"
#include "V8HTMLBaseElement.h"
#include "V8HTMLBaseFontElement.h"
#include "V8HTMLBlockquoteElement.h"
#include "V8HTMLBodyElement.h"
#include "V8HTMLBRElement.h"
#include "V8HTMLButtonElement.h"
#include "V8HTMLCanvasElement.h"
#include "V8HTMLModElement.h"
#include "V8HTMLDataGridCellElement.h"
#include "V8HTMLDataGridColElement.h"
#include "V8HTMLDataGridElement.h"
#include "V8HTMLDataGridRowElement.h"
#include "V8HTMLDirectoryElement.h"
#include "V8HTMLDivElement.h"
#include "V8HTMLDListElement.h"
#include "V8HTMLEmbedElement.h"
#include "V8HTMLFieldSetElement.h"
#include "V8HTMLFormElement.h"
#include "V8HTMLFontElement.h"
#include "V8HTMLFrameElement.h"
#include "V8HTMLFrameSetElement.h"
#include "V8HTMLHeadingElement.h"
#include "V8HTMLHeadElement.h"
#include "V8HTMLHRElement.h"
#include "V8HTMLHtmlElement.h"
#include "V8HTMLIFrameElement.h"
#include "V8HTMLImageElement.h"
#include "V8HTMLImageElementConstructor.h"
#include "V8HTMLInputElement.h"
#include "V8HTMLIsIndexElement.h"
#include "V8HTMLLabelElement.h"
#include "V8HTMLLegendElement.h"
#include "V8HTMLLIElement.h"
#include "V8HTMLLinkElement.h"
#include "V8HTMLMapElement.h"
#include "V8HTMLMarqueeElement.h"
#include "V8HTMLMenuElement.h"
#include "V8HTMLMetaElement.h"
#include "V8HTMLObjectElement.h"
#include "V8HTMLOListElement.h"
#include "V8HTMLOptGroupElement.h"
#include "V8HTMLOptionElement.h"
#include "V8HTMLOptionElementConstructor.h"
#include "V8HTMLParagraphElement.h"
#include "V8HTMLParamElement.h"
#include "V8HTMLPreElement.h"
#include "V8HTMLQuoteElement.h"
#include "V8HTMLScriptElement.h"
#include "V8HTMLSelectElement.h"
#include "V8HTMLStyleElement.h"
#include "V8HTMLTableCaptionElement.h"
#include "V8HTMLTableColElement.h"
#include "V8HTMLTableElement.h"
#include "V8HTMLTableSectionElement.h"
#include "V8HTMLTableCellElement.h"
#include "V8HTMLTableRowElement.h"
#include "V8HTMLTextAreaElement.h"
#include "V8HTMLTitleElement.h"
#include "V8HTMLUListElement.h"
#include "V8ImageData.h"
#include "V8InspectorBackend.h"
#include "V8Media.h"
#include "V8MediaList.h"
#include "V8MessageChannel.h"
#include "V8MessageEvent.h"
#include "V8MessagePort.h"
#include "V8NamedNodeMap.h"
#include "V8Node.h"
#include "V8NodeList.h"
#include "V8NodeFilter.h"
#include "V8Notation.h"
#include "V8ProcessingInstruction.h"
#include "V8ProgressEvent.h"
#include "V8StyleSheet.h"
#include "V8Text.h"
#include "V8TextEvent.h"
#include "V8DOMCoreException.h"
#include "V8DOMParser.h"
#include "V8DOMWindow.h"
#include "V8ErrorEvent.h"
#include "V8Event.h"
#include "V8EventException.h"
#include "V8KeyboardEvent.h"
#include "V8MouseEvent.h"
#include "V8ValidityState.h"
#include "V8WebKitAnimationEvent.h"
#include "V8WebKitCSSKeyframeRule.h"
#include "V8WebKitCSSKeyframesRule.h"
#include "V8WebKitCSSMatrix.h"
#include "V8WebKitCSSTransformValue.h"
#include "V8WebKitPoint.h"
#include "V8WebKitTransitionEvent.h"
#include "V8WheelEvent.h"
#include "V8UIEvent.h"
#include "V8MutationEvent.h"
#include "V8OverflowEvent.h"
#include "V8Location.h"
#include "V8Screen.h"
#include "V8DOMSelection.h"
#include "V8Navigator.h"
#include "V8MimeType.h"
#include "V8MimeTypeArray.h"
#include "V8PageTransitionEvent.h"
#include "V8Plugin.h"
#include "V8PluginArray.h"
#include "V8Range.h"
#include "V8RangeException.h"
#include "V8Rect.h"
#include "V8SQLError.h"
#include "V8SQLResultSet.h"
#include "V8SQLResultSetRowList.h"
#include "V8SQLTransaction.h"
#include "V8NodeIterator.h"
#include "V8TextMetrics.h"
#include "V8TreeWalker.h"
#include "V8StyleSheetList.h"
#include "V8DOMImplementation.h"
#include "V8XPathResult.h"
#include "V8XPathException.h"
#include "V8XPathExpression.h"
#include "V8XPathNSResolver.h"
#include "V8XMLHttpRequest.h"
#include "V8XMLHttpRequestException.h"
#include "V8XMLHttpRequestProgressEvent.h"
#include "V8XMLHttpRequestUpload.h"
#include "V8XMLSerializer.h"
#include "V8XPathEvaluator.h"
#include "V8XSLTProcessor.h"
#include "V8RGBColor.h"

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
#include "V8DOMApplicationCache.h"
#endif

#if ENABLE(DOM_STORAGE)
#include "V8Storage.h"
#include "V8StorageEvent.h"
#endif

#if ENABLE(SVG_ANIMATION)
#include "V8SVGAnimateColorElement.h"
#include "V8SVGAnimateElement.h"
#include "V8SVGAnimateTransformElement.h"
#include "V8SVGAnimationElement.h"
#include "V8SVGSetElement.h"
#endif

#if ENABLE(SVG_FILTERS)
#include "V8SVGComponentTransferFunctionElement.h"
#include "V8SVGFEBlendElement.h"
#include "V8SVGFEColorMatrixElement.h"
#include "V8SVGFEComponentTransferElement.h"
#include "V8SVGFECompositeElement.h"
#include "V8SVGFEDiffuseLightingElement.h"
#include "V8SVGFEDisplacementMapElement.h"
#include "V8SVGFEDistantLightElement.h"
#include "V8SVGFEFloodElement.h"
#include "V8SVGFEFuncAElement.h"
#include "V8SVGFEFuncBElement.h"
#include "V8SVGFEFuncGElement.h"
#include "V8SVGFEFuncRElement.h"
#include "V8SVGFEGaussianBlurElement.h"
#include "V8SVGFEImageElement.h"
#include "V8SVGFEMergeElement.h"
#include "V8SVGFEMergeNodeElement.h"
#include "V8SVGFEOffsetElement.h"
#include "V8SVGFEPointLightElement.h"
#include "V8SVGFESpecularLightingElement.h"
#include "V8SVGFESpotLightElement.h"
#include "V8SVGFETileElement.h"
#include "V8SVGFETurbulenceElement.h"
#include "V8SVGFilterElement.h"
#endif

#if ENABLE(SVG_FONTS)
#include "V8SVGFontFaceElement.h"
#include "V8SVGFontFaceFormatElement.h"
#include "V8SVGFontFaceNameElement.h"
#include "V8SVGFontFaceSrcElement.h"
#include "V8SVGFontFaceUriElement.h"
#endif

#if ENABLE(SVG_FOREIGN_OBJECT)
#include "V8SVGForeignObjectElement.h"
#endif

#if ENABLE(SVG_USE)
#include "V8SVGUseElement.h"
#endif

#if ENABLE(SVG)
#include "V8SVGAElement.h"
#include "V8SVGAltGlyphElement.h"
#include "V8SVGCircleElement.h"
#include "V8SVGClipPathElement.h"
#include "V8SVGCursorElement.h"
#include "V8SVGDefsElement.h"
#include "V8SVGDescElement.h"
#include "V8SVGElement.h"
#include "V8SVGEllipseElement.h"
#include "V8SVGException.h"
#include "V8SVGGElement.h"
#include "V8SVGGlyphElement.h"
#include "V8SVGGradientElement.h"
#include "V8SVGImageElement.h"
#include "V8SVGLinearGradientElement.h"
#include "V8SVGLineElement.h"
#include "V8SVGMarkerElement.h"
#include "V8SVGMaskElement.h"
#include "V8SVGMetadataElement.h"
#include "V8SVGPathElement.h"
#include "V8SVGPatternElement.h"
#include "V8SVGPolygonElement.h"
#include "V8SVGPolylineElement.h"
#include "V8SVGRadialGradientElement.h"
#include "V8SVGRectElement.h"
#include "V8SVGScriptElement.h"
#include "V8SVGStopElement.h"
#include "V8SVGStyleElement.h"
#include "V8SVGSVGElement.h"
#include "V8SVGSwitchElement.h"
#include "V8SVGSymbolElement.h"
#include "V8SVGTextContentElement.h"
#include "V8SVGTextElement.h"
#include "V8SVGTextPathElement.h"
#include "V8SVGTextPositioningElement.h"
#include "V8SVGTitleElement.h"
#include "V8SVGTRefElement.h"
#include "V8SVGTSpanElement.h"
#include "V8SVGViewElement.h"
#include "V8SVGAngle.h"
#include "V8SVGAnimatedAngle.h"
#include "V8SVGAnimatedBoolean.h"
#include "V8SVGAnimatedEnumeration.h"
#include "V8SVGAnimatedInteger.h"
#include "V8SVGAnimatedLength.h"
#include "V8SVGAnimatedLengthList.h"
#include "V8SVGAnimatedNumber.h"
#include "V8SVGAnimatedNumberList.h"
#include "V8SVGAnimatedPoints.h"
#include "V8SVGAnimatedPreserveAspectRatio.h"
#include "V8SVGAnimatedRect.h"
#include "V8SVGAnimatedString.h"
#include "V8SVGAnimatedTransformList.h"
#include "V8SVGColor.h"
#include "V8SVGDocument.h"
#include "V8SVGElementInstance.h"
#include "V8SVGElementInstanceList.h"
#include "V8SVGLength.h"
#include "V8SVGLengthList.h"
#include "V8SVGMatrix.h"
#include "V8SVGNumber.h"
#include "V8SVGNumberList.h"
#include "V8SVGPaint.h"
#include "V8SVGPathSeg.h"
#include "V8SVGPathSegArcAbs.h"
#include "V8SVGPathSegArcRel.h"
#include "V8SVGPathSegClosePath.h"
#include "V8SVGPathSegCurvetoCubicAbs.h"
#include "V8SVGPathSegCurvetoCubicRel.h"
#include "V8SVGPathSegCurvetoCubicSmoothAbs.h"
#include "V8SVGPathSegCurvetoCubicSmoothRel.h"
#include "V8SVGPathSegCurvetoQuadraticAbs.h"
#include "V8SVGPathSegCurvetoQuadraticRel.h"
#include "V8SVGPathSegCurvetoQuadraticSmoothAbs.h"
#include "V8SVGPathSegCurvetoQuadraticSmoothRel.h"
#include "V8SVGPathSegLinetoAbs.h"
#include "V8SVGPathSegLinetoHorizontalAbs.h"
#include "V8SVGPathSegLinetoHorizontalRel.h"
#include "V8SVGPathSegLinetoRel.h"
#include "V8SVGPathSegLinetoVerticalAbs.h"
#include "V8SVGPathSegLinetoVerticalRel.h"
#include "V8SVGPathSegList.h"
#include "V8SVGPathSegMovetoAbs.h"
#include "V8SVGPathSegMovetoRel.h"
#include "V8SVGPoint.h"
#include "V8SVGPointList.h"
#include "V8SVGPreserveAspectRatio.h"
#include "V8SVGRect.h"
#include "V8SVGRenderingIntent.h"
#include "V8SVGStringList.h"
#include "V8SVGTransform.h"
#include "V8SVGTransformList.h"
#include "V8SVGUnitTypes.h"
#include "V8SVGZoomEvent.h"
#endif

#if ENABLE(VIDEO)
#include "V8HTMLAudioElement.h"
#include "V8HTMLAudioElementConstructor.h"
#include "V8HTMLMediaElement.h"
#include "V8HTMLSourceElement.h"
#include "V8HTMLVideoElement.h"
#include "V8MediaError.h"
#include "V8TimeRanges.h"
#endif

#if ENABLE(WEB_SOCKETS)
#include "V8WebSocket.h"
#endif

#if ENABLE(WORKERS)
#include "V8AbstractWorker.h"
#include "V8DedicatedWorkerContext.h"
#include "V8Worker.h"
#include "V8WorkerContext.h"
#include "V8WorkerLocation.h"
#include "V8WorkerNavigator.h"
#endif

#if ENABLE(NOTIFICATIONS)
#include "V8Notification.h"
#include "V8NotificationCenter.h"
#endif

#if ENABLE(SHARED_WORKERS)
#include "V8SharedWorker.h"
#include "V8SharedWorkerContext.h"
#endif

#if ENABLE(3D_CANVAS)
#include "V8CanvasRenderingContext3D.h"
#include "V8CanvasArrayBuffer.h"
#include "V8CanvasArray.h"
#include "V8CanvasByteArray.h"
#include "V8CanvasBuffer.h"
#include "V8CanvasFloatArray.h"
#include "V8CanvasFramebuffer.h"
#include "V8CanvasIntArray.h"
#include "V8CanvasProgram.h"
#include "V8CanvasRenderbuffer.h"
#include "V8CanvasShader.h"
#include "V8CanvasShortArray.h"
#include "V8CanvasTexture.h"
#include "V8CanvasUnsignedByteArray.h"
#include "V8CanvasUnsignedIntArray.h"
#include "V8CanvasUnsignedShortArray.h"
#endif

namespace WebCore {

FunctionTemplateFactory V8ClassIndex::GetFactory(V8WrapperType type)
{
    switch (type) {
#define MAKE_CASE(type, name)\
    case V8ClassIndex::type: return V8##name::GetTemplate;
    WRAPPER_TYPES(MAKE_CASE)
#undef MAKE_CASE
    default: return NULL;
    }
}


#define MAKE_CACHE(type, name)\
    static v8::Persistent<v8::FunctionTemplate> name##_cache_;
    ALL_WRAPPER_TYPES(MAKE_CACHE)
#undef MAKE_CACHE


v8::Persistent<v8::FunctionTemplate>* V8ClassIndex::GetCache(V8WrapperType type)
{
    switch (type) {
#define MAKE_CASE(type, name)\
    case V8ClassIndex::type: return &name##_cache_;
    ALL_WRAPPER_TYPES(MAKE_CASE)
#undef MAKE_CASE
    default:
        ASSERT(false);
        return NULL;
  }
}

}  // namespace WebCore
