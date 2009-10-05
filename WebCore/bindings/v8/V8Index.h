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

#ifndef V8Index_h
#define V8Index_h

#include <v8.h>
#include "PlatformString.h"  // for WebCore::String

namespace WebCore {

typedef v8::Persistent<v8::FunctionTemplate> (*FunctionTemplateFactory)();

#if ENABLE(DATAGRID)
#define DATAGRID_HTMLELEMENT_TYPES(V)                                   \
    V(HTMLDATAGRIDCELLELEMENT, HTMLDataGridCellElement)                 \
    V(HTMLDATAGRIDCOLELEMENT, HTMLDataGridColElement)                   \
    V(HTMLDATAGRIDELEMENT, HTMLDataGridElement)                         \
    V(HTMLDATAGRIDROWELEMENT, HTMLDataGridRowElement)
#define DATAGRID_NONNODE_TYPES(V)                                       \
    V(DATAGRIDCOLUMN, DataGridColumn)                                   \
    V(DATAGRIDCOLUMNLIST, DataGridColumnList)
#else
#define DATAGRID_HTMLELEMENT_TYPES(V)
#define DATAGRID_NONNODE_TYPES(V)
#endif

#if ENABLE(VIDEO)
#define VIDEO_HTMLELEMENT_TYPES(V)                                      \
    V(AUDIO, HTMLAudioElementConstructor)                               \
    V(HTMLAUDIOELEMENT, HTMLAudioElement)                               \
    V(HTMLMEDIAELEMENT, HTMLMediaElement)                               \
    V(HTMLSOURCEELEMENT, HTMLSourceElement)                             \
    V(HTMLVIDEOELEMENT, HTMLVideoElement)
#define VIDEO_NONNODE_TYPES(V)                                          \
    V(MEDIAERROR, MediaError)                                           \
    V(TIMERANGES, TimeRanges)
#else
#define VIDEO_HTMLELEMENT_TYPES(V)
#define VIDEO_NONNODE_TYPES(V)
#endif

#if ENABLE(WORKERS)
#define WORKER_ACTIVE_OBJECT_WRAPPER_TYPES(V)                           \
    V(WORKER, Worker)

#define WORKER_NONNODE_WRAPPER_TYPES(V)                                 \
    V(ABSTRACTWORKER, AbstractWorker)                                   \
    V(DEDICATEDWORKERCONTEXT, DedicatedWorkerContext)                   \
    V(WORKERCONTEXT, WorkerContext)                                     \
    V(WORKERLOCATION, WorkerLocation)                                   \
    V(WORKERNAVIGATOR, WorkerNavigator)
#else
#define WORKER_ACTIVE_OBJECT_WRAPPER_TYPES(V)
#define WORKER_NONNODE_WRAPPER_TYPES(V)
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
#define APPLICATIONCACHE_NONNODE_WRAPPER_TYPES(V)                      \
  V(DOMAPPLICATIONCACHE, DOMApplicationCache)
#else
#define APPLICATIONCACHE_NONNODE_WRAPPER_TYPES(V)
#endif

#if ENABLE(NOTIFICATIONS)
#define NOTIFICATIONS_NONNODE_WRAPPER_TYPES(V)                          \
    V(NOTIFICATION, Notification)                                       \
    V(NOTIFICATIONCENTER, NotificationCenter)
#else
#define NOTIFICATIONS_NONNODE_WRAPPER_TYPES(V)
#endif

#if ENABLE(SHARED_WORKERS)
#define SHARED_WORKER_ACTIVE_OBJECT_WRAPPER_TYPES(V)                    \
    V(SHAREDWORKER, SharedWorker)
#define SHARED_WORKER_NONNODE_WRAPPER_TYPES(V)
#else
#define SHARED_WORKER_ACTIVE_OBJECT_WRAPPER_TYPES(V)
#define SHARED_WORKER_NONNODE_WRAPPER_TYPES(V)
#endif

#define DOM_NODE_TYPES(V)                                               \
    V(ATTR, Attr)                                                       \
    V(CHARACTERDATA, CharacterData)                                     \
    V(CDATASECTION, CDATASection)                                       \
    V(COMMENT, Comment)                                                 \
    V(DOCUMENT, Document)                                               \
    V(DOCUMENTFRAGMENT, DocumentFragment)                               \
    V(DOCUMENTTYPE, DocumentType)                                       \
    V(ELEMENT, Element)                                                 \
    V(ENTITY, Entity)                                                   \
    V(ENTITYREFERENCE, EntityReference)                                 \
    V(HTMLDOCUMENT, HTMLDocument)                                       \
    V(IMAGE, HTMLImageElementConstructor)                               \
    V(NODE, Node)                                                       \
    V(OPTION, HTMLOptionElementConstructor)                             \
    V(NOTATION, Notation)                                               \
    V(PROCESSINGINSTRUCTION, ProcessingInstruction)                     \
    V(TEXT, Text)                                                       \
    V(HTMLANCHORELEMENT, HTMLAnchorElement)                             \
    V(HTMLAPPLETELEMENT, HTMLAppletElement)                             \
    V(HTMLAREAELEMENT, HTMLAreaElement)                                 \
    V(HTMLBASEELEMENT, HTMLBaseElement)                                 \
    V(HTMLBASEFONTELEMENT, HTMLBaseFontElement)                         \
    V(HTMLBLOCKQUOTEELEMENT, HTMLBlockquoteElement)                     \
    V(HTMLBODYELEMENT, HTMLBodyElement)                                 \
    V(HTMLBRELEMENT, HTMLBRElement)                                     \
    V(HTMLBUTTONELEMENT, HTMLButtonElement)                             \
    V(HTMLCANVASELEMENT, HTMLCanvasElement)                             \
    V(HTMLDIRECTORYELEMENT, HTMLDirectoryElement)                       \
    V(HTMLDIVELEMENT, HTMLDivElement)                                   \
    V(HTMLDLISTELEMENT, HTMLDListElement)                               \
    V(HTMLEMBEDELEMENT, HTMLEmbedElement)                               \
    V(HTMLFIELDSETELEMENT, HTMLFieldSetElement)                         \
    V(HTMLFONTELEMENT, HTMLFontElement)                                 \
    V(HTMLFORMELEMENT, HTMLFormElement)                                 \
    V(HTMLFRAMEELEMENT, HTMLFrameElement)                               \
    V(HTMLFRAMESETELEMENT, HTMLFrameSetElement)                         \
    V(HTMLHEADINGELEMENT, HTMLHeadingElement)                           \
    V(HTMLHEADELEMENT, HTMLHeadElement)                                 \
    V(HTMLHRELEMENT, HTMLHRElement)                                     \
    V(HTMLHTMLELEMENT, HTMLHtmlElement)                                 \
    V(HTMLIFRAMEELEMENT, HTMLIFrameElement)                             \
    V(HTMLIMAGEELEMENT, HTMLImageElement)                               \
    V(HTMLINPUTELEMENT, HTMLInputElement)                               \
    V(HTMLISINDEXELEMENT, HTMLIsIndexElement)                           \
    V(HTMLLABELELEMENT, HTMLLabelElement)                               \
    V(HTMLLEGENDELEMENT, HTMLLegendElement)                             \
    V(HTMLLIELEMENT, HTMLLIElement)                                     \
    V(HTMLLINKELEMENT, HTMLLinkElement)                                 \
    V(HTMLMAPELEMENT, HTMLMapElement)                                   \
    V(HTMLMARQUEEELEMENT, HTMLMarqueeElement)                           \
    V(HTMLMENUELEMENT, HTMLMenuElement)                                 \
    V(HTMLMETAELEMENT, HTMLMetaElement)                                 \
    V(HTMLMODELEMENT, HTMLModElement)                                   \
    V(HTMLOBJECTELEMENT, HTMLObjectElement)                             \
    V(HTMLOLISTELEMENT, HTMLOListElement)                               \
    V(HTMLOPTGROUPELEMENT, HTMLOptGroupElement)                         \
    V(HTMLOPTIONELEMENT, HTMLOptionElement)                             \
    V(HTMLPARAGRAPHELEMENT, HTMLParagraphElement)                       \
    V(HTMLPARAMELEMENT, HTMLParamElement)                               \
    V(HTMLPREELEMENT, HTMLPreElement)                                   \
    V(HTMLQUOTEELEMENT, HTMLQuoteElement)                               \
    V(HTMLSCRIPTELEMENT, HTMLScriptElement)                             \
    V(HTMLSELECTELEMENT, HTMLSelectElement)                             \
    V(HTMLSTYLEELEMENT, HTMLStyleElement)                               \
    V(HTMLTABLECAPTIONELEMENT, HTMLTableCaptionElement)                 \
    V(HTMLTABLECOLELEMENT, HTMLTableColElement)                         \
    V(HTMLTABLEELEMENT, HTMLTableElement)                               \
    V(HTMLTABLESECTIONELEMENT, HTMLTableSectionElement)                 \
    V(HTMLTABLECELLELEMENT, HTMLTableCellElement)                       \
    V(HTMLTABLEROWELEMENT, HTMLTableRowElement)                         \
    V(HTMLTEXTAREAELEMENT, HTMLTextAreaElement)                         \
    V(HTMLTITLEELEMENT, HTMLTitleElement)                               \
    V(HTMLULISTELEMENT, HTMLUListElement)                               \
    V(HTMLELEMENT, HTMLElement)                                         \
    DATAGRID_HTMLELEMENT_TYPES(V)                                       \
    VIDEO_HTMLELEMENT_TYPES(V)

#if ENABLE(SVG_ANIMATION)
#define SVG_ANIMATION_ELEMENT_TYPES(V)                                  \
    V(SVGANIMATECOLORELEMENT, SVGAnimateColorElement)                   \
    V(SVGANIMATEELEMENT, SVGAnimateElement)                             \
    V(SVGANIMATETRANSFORMELEMENT, SVGAnimateTransformElement)           \
    V(SVGANIMATIONELEMENT, SVGAnimationElement)                         \
    V(SVGSETELEMENT, SVGSetElement)
#else
#define SVG_ANIMATION_ELEMENT_TYPES(V)
#endif

#if ENABLE(SVG_FILTERS)
#define SVG_FILTERS_ELEMENT_TYPES(V)                                    \
    V(SVGCOMPONENTTRANSFERFUNCTIONELEMENT, SVGComponentTransferFunctionElement)\
    V(SVGFEBLENDELEMENT, SVGFEBlendElement)                             \
    V(SVGFECOLORMATRIXELEMENT, SVGFEColorMatrixElement)                 \
    V(SVGFECOMPONENTTRANSFERELEMENT, SVGFEComponentTransferElement)     \
    V(SVGFECOMPOSITEELEMENT, SVGFECompositeElement)                     \
    V(SVGFEDIFFUSELIGHTINGELEMENT, SVGFEDiffuseLightingElement)         \
    V(SVGFEDISPLACEMENTMAPELEMENT, SVGFEDisplacementMapElement)         \
    V(SVGFEDISTANTLIGHTELEMENT, SVGFEDistantLightElement)               \
    V(SVGFEFLOODELEMENT, SVGFEFloodElement)                             \
    V(SVGFEFUNCAELEMENT, SVGFEFuncAElement)                             \
    V(SVGFEFUNCBELEMENT, SVGFEFuncBElement)                             \
    V(SVGFEFUNCGELEMENT, SVGFEFuncGElement)                             \
    V(SVGFEFUNCRELEMENT, SVGFEFuncRElement)                             \
    V(SVGFEGAUSSIANBLURELEMENT, SVGFEGaussianBlurElement)               \
    V(SVGFEIMAGEELEMENT, SVGFEImageElement)                             \
    V(SVGFEMERGEELEMENT, SVGFEMergeElement)                             \
    V(SVGFEMERGENODEELEMENT, SVGFEMergeNodeElement)                     \
    V(SVGFEOFFSETELEMENT, SVGFEOffsetElement)                           \
    V(SVGFEPOINTLIGHTELEMENT, SVGFEPointLightElement)                   \
    V(SVGFESPECULARLIGHTINGELEMENT, SVGFESpecularLightingElement)       \
    V(SVGFESPOTLIGHTELEMENT, SVGFESpotLightElement)                     \
    V(SVGFETILEELEMENT, SVGFETileElement)                               \
    V(SVGFETURBULENCEELEMENT, SVGFETurbulenceElement)                   \
    V(SVGFILTERELEMENT, SVGFilterElement)
#else
#define SVG_FILTERS_ELEMENT_TYPES(V)
#endif

#if ENABLE(SVG_FONTS)
#define SVG_FONTS_ELEMENT_TYPES(V)                                      \
    V(SVGFONTFACEELEMENT, SVGFontFaceElement)                           \
    V(SVGFONTFACEFORMATELEMENT, SVGFontFaceFormatElement)               \
    V(SVGFONTFACENAMEELEMENT, SVGFontFaceNameElement)                   \
    V(SVGFONTFACESRCELEMENT, SVGFontFaceSrcElement)                     \
    V(SVGFONTFACEURIELEMENT, SVGFontFaceUriElement)
#else
#define SVG_FONTS_ELEMENT_TYPES(V)
#endif

#if ENABLE(SVG_FOREIGN_OBJECT)
#define SVG_FOREIGN_OBJECT_ELEMENT_TYPES(V)                             \
    V(SVGFOREIGNOBJECTELEMENT, SVGForeignObjectElement)
#else
#define SVG_FOREIGN_OBJECT_ELEMENT_TYPES(V)
#endif

#if ENABLE(SVG_USE)
#define SVG_USE_ELEMENT_TYPES(V)                                        \
    V(SVGUSEELEMENT, SVGUseElement)
#else
#define SVG_USE_ELEMENT_TYPES(V)
#endif

#if ENABLE(SVG)
#define SVG_NODE_TYPES(V)                                               \
    SVG_ANIMATION_ELEMENT_TYPES(V)                                      \
    SVG_FILTERS_ELEMENT_TYPES(V)                                        \
    SVG_FONTS_ELEMENT_TYPES(V)                                          \
    SVG_FOREIGN_OBJECT_ELEMENT_TYPES(V)                                 \
    SVG_USE_ELEMENT_TYPES(V)                                            \
    V(SVGAELEMENT, SVGAElement)                                         \
    V(SVGALTGLYPHELEMENT, SVGAltGlyphElement)                           \
    V(SVGCIRCLEELEMENT, SVGCircleElement)                               \
    V(SVGCLIPPATHELEMENT, SVGClipPathElement)                           \
    V(SVGCURSORELEMENT, SVGCursorElement)                               \
    V(SVGDEFSELEMENT, SVGDefsElement)                                   \
    V(SVGDESCELEMENT, SVGDescElement)                                   \
    V(SVGELLIPSEELEMENT, SVGEllipseElement)                             \
    V(SVGGELEMENT, SVGGElement)                                         \
    V(SVGGLYPHELEMENT, SVGGlyphElement)                                 \
    V(SVGGRADIENTELEMENT, SVGGradientElement)                           \
    V(SVGIMAGEELEMENT, SVGImageElement)                                 \
    V(SVGLINEARGRADIENTELEMENT, SVGLinearGradientElement)               \
    V(SVGLINEELEMENT, SVGLineElement)                                   \
    V(SVGMARKERELEMENT, SVGMarkerElement)                               \
    V(SVGMASKELEMENT, SVGMaskElement)                                   \
    V(SVGMETADATAELEMENT, SVGMetadataElement)                           \
    V(SVGPATHELEMENT, SVGPathElement)                                   \
    V(SVGPATTERNELEMENT, SVGPatternElement)                             \
    V(SVGPOLYGONELEMENT, SVGPolygonElement)                             \
    V(SVGPOLYLINEELEMENT, SVGPolylineElement)                           \
    V(SVGRADIALGRADIENTELEMENT, SVGRadialGradientElement)               \
    V(SVGRECTELEMENT, SVGRectElement)                                   \
    V(SVGSCRIPTELEMENT, SVGScriptElement)                               \
    V(SVGSTOPELEMENT, SVGStopElement)                                   \
    V(SVGSTYLEELEMENT, SVGStyleElement)                                 \
    V(SVGSVGELEMENT, SVGSVGElement)                                     \
    V(SVGSWITCHELEMENT, SVGSwitchElement)                               \
    V(SVGSYMBOLELEMENT, SVGSymbolElement)                               \
    V(SVGTEXTCONTENTELEMENT, SVGTextContentElement)                     \
    V(SVGTEXTELEMENT, SVGTextElement)                                   \
    V(SVGTEXTPATHELEMENT, SVGTextPathElement)                           \
    V(SVGTEXTPOSITIONINGELEMENT, SVGTextPositioningElement)             \
    V(SVGTITLEELEMENT, SVGTitleElement)                                 \
    V(SVGTREFELEMENT, SVGTRefElement)                                   \
    V(SVGTSPANELEMENT, SVGTSpanElement)                                 \
    V(SVGVIEWELEMENT, SVGViewElement)                                   \
    V(SVGELEMENT, SVGElement)                                           \
    V(SVGDOCUMENT, SVGDocument)
#endif  // SVG


#if ENABLE(WEB_SOCKETS)
#define WEBSOCKET_ACTIVE_OBJECT_WRAPPER_TYPES(V)                        \
    V(WEBSOCKET, WebSocket)
#else
#define WEBSOCKET_ACTIVE_OBJECT_WRAPPER_TYPES(V)
#endif

// ACTIVE_DOM_OBJECT_TYPES are DOM_OBJECT_TYPES that need special treatement
// during GC.
#define ACTIVE_DOM_OBJECT_TYPES(V)                                      \
    V(MESSAGEPORT, MessagePort)                                         \
    V(XMLHTTPREQUEST, XMLHttpRequest)                                   \
    WORKER_ACTIVE_OBJECT_WRAPPER_TYPES(V)                               \
    SHARED_WORKER_ACTIVE_OBJECT_WRAPPER_TYPES(V)                        \
    WEBSOCKET_ACTIVE_OBJECT_WRAPPER_TYPES(V)

// NOTE: DOM_OBJECT_TYPES is split into two halves because
//       Visual Studio's Intellinonsense crashes when macros get
//       too large.  10-29-08
// DOM_OBJECT_TYPES are non-node DOM types.
#define DOM_OBJECT_TYPES_1(V)                                           \
    V(BARINFO, BarInfo)                                                 \
    V(BEFORELOADEVENT, BeforeLoadEvent)                                 \
    V(CANVASGRADIENT, CanvasGradient)                                   \
    V(CANVASPATTERN, CanvasPattern)                                     \
    V(CANVASRENDERINGCONTEXT, CanvasRenderingContext)                   \
    V(CANVASRENDERINGCONTEXT2D, CanvasRenderingContext2D)               \
    V(CLIENTRECT, ClientRect)                                           \
    V(CLIENTRECTLIST, ClientRectList)                                   \
    V(CLIPBOARD, Clipboard)                                             \
    V(CONSOLE, Console)                                                 \
    V(COUNTER, Counter)                                                 \
    V(CSSCHARSETRULE, CSSCharsetRule)                                   \
    V(CSSFONTFACERULE, CSSFontFaceRule)                                 \
    V(CSSIMPORTRULE, CSSImportRule)                                     \
    V(CSSMEDIARULE, CSSMediaRule)                                       \
    V(CSSPAGERULE, CSSPageRule)                                         \
    V(CSSPRIMITIVEVALUE, CSSPrimitiveValue)                             \
    V(CSSRULE, CSSRule)                                                 \
    V(CSSRULELIST, CSSRuleList)                                         \
    V(CSSSTYLEDECLARATION, CSSStyleDeclaration)                         \
    V(CSSSTYLERULE, CSSStyleRule)                                       \
    V(CSSSTYLESHEET, CSSStyleSheet)                                     \
    V(CSSVALUE, CSSValue)                                               \
    V(CSSVALUELIST, CSSValueList)                                       \
    V(CSSVARIABLESDECLARATION, CSSVariablesDeclaration)                 \
    V(CSSVARIABLESRULE, CSSVariablesRule)                               \
    V(DOMCOREEXCEPTION, DOMCoreException)                               \
    V(DOMIMPLEMENTATION, DOMImplementation)                             \
    V(DOMPARSER, DOMParser)                                             \
    V(DOMSELECTION, DOMSelection)                                       \
    V(DOMWINDOW, DOMWindow)                                             \
    V(EVENT, Event)                                                     \
    V(EVENTEXCEPTION, EventException)                                   \
    V(FILE, File)                                                       \
    V(FILELIST, FileList)                                               \
    V(HISTORY, History)                                                 \
    V(HTMLALLCOLLECTION, HTMLAllCollection)                             \
    V(HTMLCOLLECTION, HTMLCollection)                                   \
    V(HTMLOPTIONSCOLLECTION, HTMLOptionsCollection)                     \
    V(IMAGEDATA, ImageData)                                             \
    V(CANVASPIXELARRAY, CanvasPixelArray)                               \
    V(INSPECTORBACKEND, InspectorBackend)                               \
    V(KEYBOARDEVENT, KeyboardEvent)                                     \
    V(LOCATION, Location)                                               \
    V(MEDIA, Media)                                               \
    V(MEDIALIST, MediaList)

#define DOM_OBJECT_TYPES_2(V)                                           \
    V(MESSAGECHANNEL, MessageChannel)                                   \
    V(MESSAGEEVENT, MessageEvent)                                       \
    V(MIMETYPE, MimeType)                                               \
    V(MIMETYPEARRAY, MimeTypeArray)                                     \
    V(MOUSEEVENT, MouseEvent)                                           \
    V(MUTATIONEVENT, MutationEvent)                                     \
    V(NAMEDNODEMAP, NamedNodeMap)                                       \
    V(NAVIGATOR, Navigator)                                             \
    V(NODEFILTER, NodeFilter)                                           \
    V(NODEITERATOR, NodeIterator)                                       \
    V(NODELIST, NodeList)                                               \
    V(OVERFLOWEVENT, OverflowEvent)                                     \
    V(PAGETRANSITIONEVENT, PageTransitionEvent)                         \
    V(PLUGIN, Plugin)                                                   \
    V(PLUGINARRAY, PluginArray)                                         \
    V(PROGRESSEVENT, ProgressEvent)                                     \
    V(RANGE, Range)                                                     \
    V(RANGEEXCEPTION, RangeException)                                   \
    V(RECT, Rect)                                                       \
    V(RGBCOLOR, RGBColor)                                               \
    V(SCREEN, Screen)                                                   \
    V(STYLESHEET, StyleSheet)                                           \
    V(STYLESHEETLIST, StyleSheetList)                                   \
    V(TEXTEVENT, TextEvent)                                             \
    V(TEXTMETRICS, TextMetrics)                                         \
    V(TREEWALKER, TreeWalker)                                           \
    V(UIEVENT, UIEvent)                                                 \
    V(VALIDITYSTATE, ValidityState)                                     \
    V(WEBKITANIMATIONEVENT, WebKitAnimationEvent)                       \
    V(WEBKITCSSKEYFRAMERULE, WebKitCSSKeyframeRule)                     \
    V(WEBKITCSSKEYFRAMESRULE, WebKitCSSKeyframesRule)                   \
    V(WEBKITCSSMATRIX, WebKitCSSMatrix)                                 \
    V(WEBKITPOINT, WebKitPoint)                                         \
    V(WEBKITCSSTRANSFORMVALUE, WebKitCSSTransformValue)                 \
    V(WEBKITTRANSITIONEVENT, WebKitTransitionEvent)                     \
    V(WHEELEVENT, WheelEvent)                                           \
    V(XMLHTTPREQUESTUPLOAD, XMLHttpRequestUpload)                       \
    V(XMLHTTPREQUESTEXCEPTION, XMLHttpRequestException)                 \
    V(XMLHTTPREQUESTPROGRESSEVENT, XMLHttpRequestProgressEvent)         \
    V(XMLSERIALIZER, XMLSerializer)                                     \
    V(XPATHEVALUATOR, XPathEvaluator)                                   \
    V(XPATHEXCEPTION, XPathException)                                   \
    V(XPATHEXPRESSION, XPathExpression)                                 \
    V(XPATHNSRESOLVER, XPathNSResolver)                                 \
    V(XPATHRESULT, XPathResult)                                         \
    V(XSLTPROCESSOR, XSLTProcessor)                                     \
    ACTIVE_DOM_OBJECT_TYPES(V)                                          \
    APPLICATIONCACHE_NONNODE_WRAPPER_TYPES(V)                           \
    DATAGRID_NONNODE_TYPES(V)                                           \
    VIDEO_NONNODE_TYPES(V)                                              \
    NOTIFICATIONS_NONNODE_WRAPPER_TYPES(V)                              \
    SHARED_WORKER_NONNODE_WRAPPER_TYPES(V)                              \
    WORKER_NONNODE_WRAPPER_TYPES(V)

#if ENABLE(DATABASE)
#define DOM_OBJECT_DATABASE_TYPES(V)                                    \
    V(DATABASE, Database)                                               \
    V(SQLERROR, SQLError)                                               \
    V(SQLRESULTSET, SQLResultSet)                                       \
    V(SQLRESULTSETROWLIST, SQLResultSetRowList)                         \
    V(SQLTRANSACTION, SQLTransaction)
#else
#define DOM_OBJECT_DATABASE_TYPES(V)
#endif

#if ENABLE(DOM_STORAGE)
#define DOM_OBJECT_STORAGE_TYPES(V)                                     \
    V(STORAGE, Storage)                                                 \
    V(STORAGEEVENT, StorageEvent)
#else
#define DOM_OBJECT_STORAGE_TYPES(V)
#endif

#if ENABLE(WORKERS)
#define DOM_OBJECT_WORKERS_TYPES(V)                                     \
    V(ERROREVENT, ErrorEvent)
#else
#define DOM_OBJECT_WORKERS_TYPES(V)
#endif

#if ENABLE(3D_CANVAS)
#define DOM_OBJECT_3D_CANVAS_TYPES(V)                                   \
    V(CANVASARRAY, CanvasArray)                                         \
    V(CANVASARRAYBUFFER, CanvasArrayBuffer)                             \
    V(CANVASBUFFER, CanvasBuffer)                                       \
    V(CANVASBYTEARRAY, CanvasByteArray)                                 \
    V(CANVASFLOATARRAY, CanvasFloatArray)                               \
    V(CANVASFRAMEBUFFER, CanvasFramebuffer)                             \
    V(CANVASINTARRAY, CanvasIntArray)                                   \
    V(CANVASPROGRAM, CanvasProgram)                                     \
    V(CANVASRENDERBUFFER, CanvasRenderbuffer)                           \
    V(CANVASRENDERINGCONTEXT3D, CanvasRenderingContext3D)               \
    V(CANVASSHADER, CanvasShader)                                       \
    V(CANVASSHORTARRAY, CanvasShortArray)                               \
    V(CANVASTEXTURE, CanvasTexture)                                     \
    V(CANVASUNSIGNEDBYTEARRAY, CanvasUnsignedByteArray)                 \
    V(CANVASUNSIGNEDINTARRAY, CanvasUnsignedIntArray)                   \
    V(CANVASUNSIGNEDSHORTARRAY, CanvasUnsignedShortArray)
#else
#define DOM_OBJECT_3D_CANVAS_TYPES(V)
#endif

#define DOM_OBJECT_TYPES(V)                                             \
    DOM_OBJECT_TYPES_1(V)                                               \
    DOM_OBJECT_TYPES_2(V)                                               \
    DOM_OBJECT_DATABASE_TYPES(V)                                        \
    DOM_OBJECT_STORAGE_TYPES(V)                                         \
    DOM_OBJECT_WORKERS_TYPES(V)                                         \
    DOM_OBJECT_3D_CANVAS_TYPES(V)

#if ENABLE(SVG)
// SVG_OBJECT_TYPES are svg non-node, non-pod types.
#define SVG_OBJECT_TYPES(V)                                             \
    V(SVGANGLE, SVGAngle)                                               \
    V(SVGANIMATEDANGLE, SVGAnimatedAngle)                               \
    V(SVGANIMATEDBOOLEAN, SVGAnimatedBoolean)                           \
    V(SVGANIMATEDENUMERATION, SVGAnimatedEnumeration)                   \
    V(SVGANIMATEDINTEGER, SVGAnimatedInteger)                           \
    V(SVGANIMATEDLENGTH, SVGAnimatedLength)                             \
    V(SVGANIMATEDLENGTHLIST, SVGAnimatedLengthList)                     \
    V(SVGANIMATEDNUMBER, SVGAnimatedNumber)                             \
    V(SVGANIMATEDNUMBERLIST, SVGAnimatedNumberList)                     \
    V(SVGANIMATEDPRESERVEASPECTRATIO, SVGAnimatedPreserveAspectRatio)   \
    V(SVGANIMATEDRECT, SVGAnimatedRect)                                 \
    V(SVGANIMATEDSTRING, SVGAnimatedString)                             \
    V(SVGANIMATEDTRANSFORMLIST, SVGAnimatedTransformList)               \
    V(SVGCOLOR, SVGColor)                                               \
    V(SVGELEMENTINSTANCE, SVGElementInstance)                           \
    V(SVGELEMENTINSTANCELIST, SVGElementInstanceList)                   \
    V(SVGEXCEPTION, SVGException)                                       \
    V(SVGLENGTHLIST, SVGLengthList)                                     \
    V(SVGNUMBERLIST, SVGNumberList)                                     \
    V(SVGPAINT, SVGPaint)                                               \
    V(SVGPATHSEG, SVGPathSeg)                                           \
    V(SVGPATHSEGARCABS, SVGPathSegArcAbs)                               \
    V(SVGPATHSEGARCREL, SVGPathSegArcRel)                               \
    V(SVGPATHSEGCLOSEPATH, SVGPathSegClosePath)                         \
    V(SVGPATHSEGCURVETOCUBICABS, SVGPathSegCurvetoCubicAbs)             \
    V(SVGPATHSEGCURVETOCUBICREL, SVGPathSegCurvetoCubicRel)             \
    V(SVGPATHSEGCURVETOCUBICSMOOTHABS, SVGPathSegCurvetoCubicSmoothAbs) \
    V(SVGPATHSEGCURVETOCUBICSMOOTHREL, SVGPathSegCurvetoCubicSmoothRel) \
    V(SVGPATHSEGCURVETOQUADRATICABS, SVGPathSegCurvetoQuadraticAbs)     \
    V(SVGPATHSEGCURVETOQUADRATICREL, SVGPathSegCurvetoQuadraticRel)     \
    V(SVGPATHSEGCURVETOQUADRATICSMOOTHABS, SVGPathSegCurvetoQuadraticSmoothAbs)\
    V(SVGPATHSEGCURVETOQUADRATICSMOOTHREL, SVGPathSegCurvetoQuadraticSmoothRel)\
    V(SVGPATHSEGLINETOABS, SVGPathSegLinetoAbs)                         \
    V(SVGPATHSEGLINETOHORIZONTALABS, SVGPathSegLinetoHorizontalAbs)     \
    V(SVGPATHSEGLINETOHORIZONTALREL, SVGPathSegLinetoHorizontalRel)     \
    V(SVGPATHSEGLINETOREL, SVGPathSegLinetoRel)                         \
    V(SVGPATHSEGLINETOVERTICALABS, SVGPathSegLinetoVerticalAbs)         \
    V(SVGPATHSEGLINETOVERTICALREL, SVGPathSegLinetoVerticalRel)         \
    V(SVGPATHSEGLIST, SVGPathSegList)                                   \
    V(SVGPATHSEGMOVETOABS, SVGPathSegMovetoAbs)                         \
    V(SVGPATHSEGMOVETOREL, SVGPathSegMovetoRel)                         \
    V(SVGPOINTLIST, SVGPointList)                                       \
    V(SVGPRESERVEASPECTRATIO, SVGPreserveAspectRatio)                   \
    V(SVGRENDERINGINTENT, SVGRenderingIntent)                           \
    V(SVGSTRINGLIST, SVGStringList)                                     \
    V(SVGTRANSFORMLIST, SVGTransformList)                               \
    V(SVGUNITTYPES, SVGUnitTypes)                                       \
    V(SVGZOOMEVENT, SVGZoomEvent)

// SVG POD types should list all types whose IDL has PODType declaration.
#define SVG_POD_TYPES(V)                                                \
    V(SVGLENGTH, SVGLength)                                             \
    V(SVGTRANSFORM, SVGTransform)                                       \
    V(SVGMATRIX, SVGMatrix)                                             \
    V(SVGNUMBER, SVGNumber)                                             \
    V(SVGPOINT, SVGPoint)                                               \
    V(SVGRECT, SVGRect)

// POD types can have different implementation names, see CodeGenerateV8.pm.
#define SVG_POD_NATIVE_TYPES(V)                                         \
    V(SVGLENGTH, SVGLength)                                             \
    V(SVGTRANSFORM, SVGTransform)                                       \
    V(SVGMATRIX, TransformationMatrix)                                  \
    V(SVGNUMBER, float)                                                 \
    V(SVGPOINT, FloatPoint)                                             \
    V(SVGRECT, FloatRect)

// Shouldn't generate code for these two types.
#define SVG_NO_WRAPPER_TYPES(V)                                         \
    V(SVGURIREFERENCE, SVGURIReference)                                 \
    V(SVGANIMATEDPOINTS, SVGAnimatedPoints)

// SVG_NONNODE_TYPES are SVG non-node object types, pod typs and
// numerical types.
#define SVG_NONNODE_TYPES(V)                                            \
    SVG_OBJECT_TYPES(V)                                                 \
    SVG_POD_TYPES(V)
#endif  // SVG

// EVENTTARGET, EVENTLISTENER, and NPOBJECT do not have V8 wrappers.
#define DOM_NO_WRAPPER_TYPES(V)                                         \
    V(EVENTTARGET, EventTarget)                                         \
    V(EVENTLISTENER, EventListener)                                     \
    V(NPOBJECT, NPObject)

#if ENABLE(SVG)
#define WRAPPER_TYPES(V) DOM_NODE_TYPES(V) DOM_OBJECT_TYPES(V) SVG_NODE_TYPES(V) SVG_NONNODE_TYPES(V)
#define NO_WRAPPER_TYPES(V) DOM_NO_WRAPPER_TYPES(V) SVG_NO_WRAPPER_TYPES(V)
#else  // SVG
#define WRAPPER_TYPES(V) DOM_NODE_TYPES(V) DOM_OBJECT_TYPES(V)
#define NO_WRAPPER_TYPES(V) DOM_NO_WRAPPER_TYPES(V)
#endif  // SVG

#define ALL_WRAPPER_TYPES(V) WRAPPER_TYPES(V) NO_WRAPPER_TYPES(V)

    class V8ClassIndex {
    public:
        // Type must start at non-negative numbers. See ToInt, FromInt.
        enum V8WrapperType {
            INVALID_CLASS_INDEX = 0,

#define DEFINE_ENUM(name, type) name, 
            ALL_WRAPPER_TYPES(DEFINE_ENUM)
#undef DEFINE_ENUM

            CLASSINDEX_END,
            WRAPPER_TYPE_COUNT = CLASSINDEX_END
        };

        // FIXME: Convert to toInt after all the bindings are in one place.
        static int ToInt(V8WrapperType type) { return static_cast<int>(type); }

        // FIXME: Convert to fromInt after all the bindings are in one place.
        static V8WrapperType FromInt(int v) {
            ASSERT(INVALID_CLASS_INDEX <= v && v < CLASSINDEX_END);
            return static_cast<V8WrapperType>(v);
        }

        // FIXME: Convert to getFactory after all the bindings are in one place.
        static FunctionTemplateFactory GetFactory(V8WrapperType type);

        // Returns a field to be used as cache for the template for the given type
        // FIXME: Convert to getCache after all the bindings are in one place.
        static v8::Persistent<v8::FunctionTemplate>* GetCache(V8WrapperType type);
    };

}

#endif // V8Index_h
