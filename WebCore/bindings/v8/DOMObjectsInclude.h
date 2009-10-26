/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef DOMObjectsInclude_h
#define DOMObjectsInclude_h

#include "BarInfo.h"
#include "BeforeLoadEvent.h"
#include "CanvasActiveInfo.h"
#include "CanvasArray.h"
#include "CanvasArrayBuffer.h"
#include "CanvasBuffer.h"
#include "CanvasByteArray.h"
#include "CanvasFloatArray.h"
#include "CanvasFramebuffer.h"
#include "CanvasGradient.h"
#include "CanvasIntArray.h"
#include "CanvasObject.h"
#include "CanvasPattern.h"
#include "CanvasPixelArray.h"
#include "CanvasProgram.h"
#include "CanvasRenderbuffer.h"
#include "CanvasRenderingContext.h"
#include "CanvasRenderingContext2D.h"
#include "CanvasRenderingContext3D.h"
#include "CanvasShader.h"
#include "CanvasShortArray.h"
#include "CanvasUnsignedByteArray.h"
#include "CanvasUnsignedIntArray.h"
#include "CanvasUnsignedShortArray.h"
#include "CanvasStyle.h"
#include "CanvasTexture.h"
#include "CharacterData.h"
#include "ClientRect.h"
#include "ClientRectList.h"
#include "Clipboard.h"
#include "Console.h"
#include "Counter.h"
#include "CSSCharsetRule.h"
#include "CSSFontFaceRule.h"
#include "CSSImportRule.h"
#include "CSSMediaRule.h"
#include "CSSPageRule.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleDeclaration.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSValueList.h"
#include "CSSVariablesDeclaration.h"
#include "CSSVariablesRule.h"
#include "DocumentType.h"
#include "DocumentFragment.h"
#include "DOMCoreException.h"
#include "DOMImplementation.h"
#include "DOMParser.h"
#include "DOMSelection.h"
#include "DOMWindow.h"
#include "Entity.h"
#include "ErrorEvent.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "Event.h"
#include "EventException.h"
#include "ExceptionCode.h"
#include "File.h"
#include "FileList.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "History.h"
#include "HTMLNames.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLSelectElement.h"
#include "HTMLOptionsCollection.h"
#include "ImageData.h"
#include "KeyboardEvent.h"
#include "Location.h"
#include "Media.h"
#include "MediaError.h"
#include "MediaList.h"
#include "MediaPlayer.h"
#include "MessageChannel.h"
#include "MessageEvent.h"
#include "MessagePort.h"
#include "MimeTypeArray.h"
#include "MouseEvent.h"
#include "MutationEvent.h"
#include "Navigator.h" // for MimeTypeArray
#include "NodeFilter.h"
#include "Notation.h"
#include "NodeList.h"
#include "NodeIterator.h"
#include "OverflowEvent.h"
#include "Page.h"
#include "PageTransitionEvent.h"
#include "Plugin.h"
#include "PluginArray.h"
#include "ProcessingInstruction.h"
#include "ProgressEvent.h"
#include "Range.h"
#include "RangeException.h"
#include "Rect.h"
#include "RGBColor.h"
#include "Screen.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "StyleSheet.h"
#include "StyleSheetList.h"
#include "TextEvent.h"
#include "TextMetrics.h"
#include "TimeRanges.h"
#include "TreeWalker.h"
#include "V8AbstractEventListener.h"
#include "V8CustomEventListener.h"
#include "V8DOMWindow.h"
#include "V8HTMLElement.h"
#include "V8LazyEventListener.h"
#include "V8NodeFilterCondition.h"
#include "ValidityState.h"
#include "WebKitAnimationEvent.h"
#include "WebKitCSSKeyframeRule.h"
#include "WebKitCSSKeyframesRule.h"
#include "WebKitCSSMatrix.h"
#include "WebKitCSSTransformValue.h"
#include "WebKitPoint.h"
#include "WebKitTransitionEvent.h"
#include "WheelEvent.h"
#include "XMLHttpRequest.h"
#include "XMLHttpRequestException.h"
#include "XMLHttpRequestProgressEvent.h"
#include "XMLHttpRequestUpload.h"
#include "XMLSerializer.h"

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
#include "DOMApplicationCache.h"
#endif

#if ENABLE(DATABASE)
#include "Database.h"
#include "SQLTransaction.h"
#include "SQLResultSet.h"
#include "SQLResultSetRowList.h"
#endif // DATABASE

#if ENABLE(DATAGRID)
#include "DataGridColumn.h"
#include "DataGridColumnList.h"
#endif // DATAGRID

#if ENABLE(DOM_STORAGE)
#include "Storage.h"
#include "StorageEvent.h"
#endif // DOM_STORAGE

#if ENABLE(SVG)
#include "SVGAngle.h"
#include "SVGAnimatedPoints.h"
#include "SVGColor.h"
#include "SVGElement.h"
#include "SVGElementInstance.h"
#include "SVGElementInstanceList.h"
#include "SVGException.h"
#include "SVGLength.h"
#include "SVGLengthList.h"
#include "SVGNumberList.h"
#include "SVGPaint.h"
#include "SVGPathSeg.h"
#include "SVGPathSegArc.h"
#include "SVGPathSegClosePath.h"
#include "SVGPathSegCurvetoCubic.h"
#include "SVGPathSegCurvetoCubicSmooth.h"
#include "SVGPathSegCurvetoQuadratic.h"
#include "SVGPathSegCurvetoQuadraticSmooth.h"
#include "SVGPathSegLineto.h"
#include "SVGPathSegLinetoHorizontal.h"
#include "SVGPathSegLinetoVertical.h"
#include "SVGPathSegList.h"
#include "SVGPathSegMoveto.h"
#include "SVGPointList.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGRenderingIntent.h"
#include "SVGStringList.h"
#include "SVGTransform.h"
#include "SVGTransformList.h"
#include "SVGUnitTypes.h"
#include "SVGURIReference.h"
#include "SVGZoomEvent.h"
#include "V8SVGPODTypeWrapper.h"
#endif // SVG

#if ENABLE(WEB_SOCKETS)
#include "WebSocket.h"
#endif

#if ENABLE(WORKERS)
#include "AbstractWorker.h"
#include "DedicatedWorkerContext.h"
#include "Worker.h"
#include "WorkerContext.h"
#include "WorkerLocation.h"
#include "WorkerNavigator.h"
#endif // WORKERS

#if ENABLE(SHARED_WORKERS)
#include "SharedWorker.h"
#include "SharedWorkerContext.h"
#endif  // SHARED_WORKERS

#if ENABLE(NOTIFICATIONS)
#include "Notification.h"
#include "NotificationCenter.h"
#endif // NOTIFICATIONS

#if ENABLE(XPATH)
#include "XPathEvaluator.h"
#include "XPathException.h"
#include "XPathExpression.h"
#include "XPathNSResolver.h"
#include "XPathResult.h"
#endif // XPATH

#if ENABLE(XSLT)
#include "XSLTProcessor.h"
#endif // XSLT

#if ENABLE(INSPECTOR)
#include "InspectorBackend.h"
#endif // INSPECTOR

namespace WebCore {

    // A helper class for undetectable document.all
    class HTMLAllCollection : public HTMLCollection {
    };

} // namespace WebCore

#endif // DOMObjectsInclude_h
