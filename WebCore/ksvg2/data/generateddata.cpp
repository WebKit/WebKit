#include <kdom/ecma/Ecma.h>
#include <kdom/ecma/DOMLookup.h>
#include <ksvg2/ecma/EcmaInterface.h>
#include <SVGRectElement.h>
#include <CSSRule.h>
#include <SVGGElement.h>
#include <SVGClipPathElement.h>
#include <Attr.h>
#include <SVGStopElement.h>
#include <SVGAnimateElementImpl.h>
#include <SVGFEGaussianBlurElementImpl.h>
#include <Text.h>
#include <SVGZoomAndPanImpl.h>
#include <SVGDocument.h>
#include <CSSValueListImpl.h>
#include <SVGMatrix.h>
#include <SVGComponentTransferFunctionElementImpl.h>
#include <DOMUserDataImpl.h>
#include <UIEventImpl.h>
#include <CSSImportRuleImpl.h>
#include <SVGPathSegCurvetoCubic.h>
#include <SVGFilterPrimitiveStandardAttributes.h>
#include <KeyboardEvent.h>
#include <DocumentEventImpl.h>
#include <SVGStringListImpl.h>
#include <NamedNodeMap.h>
#include <StyleSheetListImpl.h>
#include <CDATASection.h>
#include <DOMImplementationImpl.h>
#include <KeyboardEventImpl.h>
#include <SVGPathSegCurvetoCubicSmooth.h>
#include <SVGFEFuncGElement.h>
#include <SVGAnimateColorElement.h>
#include <NodeIteratorImpl.h>
#include <CSSCharsetRuleImpl.h>
#include <CSSRuleListImpl.h>
#include <SVGNumberImpl.h>
#include <SVGElementInstance.h>
#include <SVGElementInstanceListImpl.h>
#include <EventTargetImpl.h>
#include <Event.h>
#include <CSSUnknownRuleImpl.h>
//#include <KSVGBrowserExtension.h>
#include <SVGLocatableImpl.h>
#include <DocumentTypeImpl.h>
#include <DOMLocatorImpl.h>
#include <SVGExceptionImpl.h>
#include <SVGCSSStyleSelector.h>
#include <KDOMCacheHelper.h>
#include <KDOMCache.h>
#include <TreeWalker.h>
#include <SVGTitleElement.h>
#include <RGBColorImpl.h>
#include <SVGAElementImpl.h>
#include <MutationEvent.h>
#include <SVGPathSegLinetoHorizontal.h>
#include <SVGZoomAndPan.h>
#include <SVGPathSegLinetoImpl.h>
#include <KDOMCachedStyleSheet.h>
#include <SVGPatternElementImpl.h>
#include <SVGComponentTransferFunctionElement.h>
#include <EventExceptionImpl.h>
#include <SVGElementImpl.h>
#include <CSSFontFaceRule.h>
#include <SVGStylableImpl.h>
#include <SVGFEOffsetElement.h>
#include <SVGGradientElement.h>
#include <SVGCircleElement.h>
#include <RangeExceptionImpl.h>
#include <SVGPathSegCurvetoQuadraticSmooth.h>
#include <SVGAnimateTransformElementImpl.h>
#include <SVGLangSpace.h>
#include <SVGAnimatedLengthList.h>
#include <DOMStringList.h>
#include <SVGStylable.h>
#include <MediaList.h>
#include <SVGAnimatedLengthListImpl.h>
#include <CSSRuleImpl.h>
#include <MouseEventImpl.h>
#include <SVGAnimatedAngleImpl.h>
#include <NodeFilter.h>
#include <CSSStyleRule.h>
#include <SVGAnimatedPreserveAspectRatio.h>
#include <SVGURIReferenceImpl.h>
#include <SVGTests.h>
#include <Comment.h>
//#include <KSVGFactory.h>
#include <DOMErrorHandler.h>
#include <SVGAnimatedAngle.h>
#include <DocumentTraversal.h>
#include <SVGEventImpl.h>
#include <KDOMCachedImage.h>
#include <DOMError.h>
#include <SVGLengthImpl.h>
#include <SVGTransformableImpl.h>
#include <MutationEventImpl.h>
#include <Entity.h>
#include <KDOMView.h>
#include <SVGTitleElementImpl.h>
#include <SVGTSpanElementImpl.h>
#include <SVGTextPositioningElement.h>
#include <SVGTextContentElementImpl.h>
#include <SVGSetElement.h>
#include <LinkStyle.h>
#include <CSSStyleDeclarationImpl.h>
#include <SVGDefsElementImpl.h>
#include <ViewCSS.h>
#include <SVGStyleElementImpl.h>
#include <SVGSwitchElementImpl.h>
#include <SVGTransformListImpl.h>
#include <SVGFECompositeElementImpl.h>
#include <SVGExternalResourcesRequired.h>
#include <SVGAnimatedNumber.h>
#include <ksvg2/KSVGPart.h>
#include <CDFInterface.h>
#include <RangeException.h>
#include <SVGZoomEvent.h>
#include <SVGPathSegMovetoImpl.h>
#include <SVGPathSegLinetoVerticalImpl.h>
#include <SVGAnimatedLengthImpl.h>
#include <CSSPrimitiveValueImpl.h>
#include <DOMStringImpl.h>
#include <SVGPathSegLinetoVertical.h>
#include <kdom/Shared.h>
#include <SVGAnimatedRectImpl.h>
#include <DocumentCSS.h>
#include <SVGStopElementImpl.h>
#include <TraversalImpl.h>
#include <SVGNumber.h>
#include <SVGFEComponentTransferElement.h>
#include <SVGLocatable.h>
#include <SVGAngle.h>
#include <CSSStyleRuleImpl.h>
#include <SVGPathSeg.h>
#include <Rect.h>
#include <SVGAnimatedRect.h>
#include <SVGSymbolElementImpl.h>
#include <RenderStyleDefs.h>
#include <DocumentView.h>
#include <KSVGSettings.h>
#include <SVGFETileElementImpl.h>
#include <SVGTextPositioningElementImpl.h>
#include <KDOMCachedScript.h>
#include <SVGAElement.h>
#include <CSSPageRuleImpl.h>
#include <SVGFEFuncAElement.h>
#include <CSSFontFaceRuleImpl.h>
#include <SVGSetElementImpl.h>
#include <SVGEvent.h>
#include <SVGEventImpl.h>
#include <AbstractViewImpl.h>
#include <CSSValueList.h>
#include <Counter.h>
#include <SVGMarkerElementImpl.h>
#include <SVGViewElementImpl.h>
#include <SVGFEFuncGElementImpl.h>
#include <SVGElement.h>
#include <SVGStyleElement.h>
#include <UIEvent.h>
#include <CSSValue.h>
#include <DOMImplementation.h>
#include <Notation.h>
#include <SVGPathSegMoveto.h>
#include <DOMErrorImpl.h>
#include <SVGTestsImpl.h>
#include <MouseEvent.h>
#include <TreeWalkerImpl.h>
#include <NodeList.h>
#include <SVGStringList.h>
#include <SVGNumberListImpl.h>
#include <SVGPointImpl.h>
#include <SVGPathSegCurvetoCubicSmoothImpl.h>
#include <SVGException.h>
#include <EventException.h>
#include <SVGDOMImplementation.h>
#include <DOMExceptionImpl.h>
#include <SVGLengthList.h>
#include <SVGAnimatedLength.h>
#include <SVGDefsElement.h>
#include <SVGAnimatedEnumeration.h>
#include <CSSUnknownRule.h>
#include <SVGRenderStyle.h>
#include <SVGPointList.h>
#include <TypeInfo.h>
#include <SVGImageElement.h>
#include <StyleSheetList.h>
#include <SVGAnimatedPathData.h>
#include <kdom/css/impl/CSSStyleSelector.h>
#include <DocumentRange.h>
#include <SVGAnimateTransformElement.h>
#include <DOMImplementationCSS.h>
#include <SVGPathSegList.h>
#include <SVGRenderStyleDefs.h>
#include <SVGImageElementImpl.h>
#include <SVGFilterElementImpl.h>
#include <SVGPreserveAspectRatio.h>
#include <KDOMLoader.h>
#include <SVGAnimationElementImpl.h>
#include <DocumentTraversalImpl.h>
#include <SVGFEFloodElement.h>
#include <SVGPathSegClosePathImpl.h>
#include <SVGTransformImpl.h>
#include <SVGFEImageElementImpl.h>
#include <NodeIterator.h>
#include <SVGClipPathElementImpl.h>
#include <SVGZoomEventImpl.h>
#include <ProcessingInstruction.h>
#include <SVGSVGElementImpl.h>
#include <SVGFETurbulenceElementImpl.h>
#include <SVGFEMergeElement.h>
#include <SVGElementInstanceImpl.h>
#include <SVGURIReference.h>
#include <SVGColor.h>
#include <SVGNumberList.h>
#include <SVGAnimatedString.h>
#include <SVGFEFuncRElementImpl.h>
#include <SVGRect.h>
#include <SVGPathSegListImpl.h>
#include <SVGFEFuncAElementImpl.h>
#include <Element.h>
#include <DocumentType.h>
#include <KDOMPart.h>
#include <CSSImportRule.h>
#include <DocumentFragment.h>
#include <SVGAnimatedInteger.h>
#include <SVGAnimatedPoints.h>
#include <RGBColor.h>
#include <SVGPolyElementImpl.h>
#include <CSSValueImpl.h>
#include <AbstractView.h>
#include <RenderStyle.h>
#include <SVGPathSegCurvetoQuadratic.h>
#include <SVGLinearGradientElementImpl.h>
#include <CSSPrimitiveValue.h>
#include <CharacterDataImpl.h>
#include <SVGScriptElement.h>
#include <svgpathparser.h>
#include <CDATASectionImpl.h>
#include <SVGPathSegImpl.h>
#include <SVGAnimatedIntegerImpl.h>
#include <NodeImpl.h>
#include <CSSStyleSheet.h>
#include <SVGTransform.h>
#include <SVGAngleImpl.h>
#include <CSSPageRule.h>
#include <SVGAnimatedPreserveAspectRatioImpl.h>
#include <SVGColorImpl.h>
#include <KDOMCachedDocument.h>
#include <SVGEllipseElementImpl.h>
#include <DOMConfiguration.h>
#include <SVGScriptElementImpl.h>
#include <EventImpl.h>
#include <SVGDocumentImpl.h>
#include <SVGPathSegCurvetoQuadraticSmoothImpl.h>
#include <SVGGElementImpl.h>
#include <DocumentEvent.h>
#include <SVGFilterElement.h>
#include <SVGRadialGradientElement.h>
#include <SVGAnimatedStringImpl.h>
#include <SVGAnimatedTransformListImpl.h>
#include <SVGLineElement.h>
#include <KDOMCachedObjectClient.h>
#include <SVGFETurbulenceElement.h>
#include <DocumentImpl.h>
#include <NamedAttrMapImpl.h>
#include <SVGTransformable.h>
#include <SVGDescElement.h>
#include <SVGAnimatedTransformList.h>
#include <Node.h>
#include <SVGFETileElement.h>
#include <SVGPreserveAspectRatioImpl.h>
#include <Document.h>
#include <SVGMatrixImpl.h>
#include <SVGStyledElementImpl.h>
#include <SVGAnimateElement.h>
#include <NodeFilterImpl.h>
#include <CSSStyleSheetImpl.h>
#include <RangeImpl.h>
#include <SVGFEFuncBElement.h>
#include <SVGPolylineElementImpl.h>
#include <SVGTextElement.h>
#include <DOMUserData.h>
#include <SVGPathSegLinetoHorizontalImpl.h>
#include <SVGFitToViewBoxImpl.h>
#include <DOMException.h>
#include <SVGEllipseElement.h>
#include <SVGFEBlendElement.h>
#include <EventListenerImpl.h>
#include <SVGCSSStyleSheetImpl.h>
#include <SVGLengthListImpl.h>
#include <SVGPoint.h>
#include <CSSCharsetRule.h>
#include <SVGDescElementImpl.h>
#include <SVGTextContentElement.h>
#include <SVGFEFuncBElementImpl.h>
#include <DocumentFragmentImpl.h>
#include <SVGAnimationElement.h>
#include <TagNodeListImpl.h>
#include <SVGPointListImpl.h>
#include <ImageSource.h>
#include <DOMString.h>
#include <TypeInfoImpl.h>
#include <AttrImpl.h>
#include <DOMConfigurationImpl.h>
#include <SVGMarkerElement.h>
#include <SVGCSSStyleDeclarationImpl.h>
#include <SVGRadialGradientElementImpl.h>
#include <SVGLangSpaceImpl.h>
#include <kdom/css/impl/Font.h>
#include <SVGFEBlendElementImpl.h>
#include <SVGFilterPrimitiveStandardAttributesImpl.h>
#include <SVGPathElementImpl.h>
#include <SVGFEFuncRElement.h>
#include <Range.h>
#include <SVGAnimateColorElementImpl.h>
#include <SVGPathSegArcImpl.h>
#include <CharacterData.h>
#include <SVGAnimatedPointsImpl.h>
#include <SVGUseElementImpl.h>
#include <SVGUseElement.h>
#include <SVGFEMergeNodeElement.h>
#include <DOMStringListImpl.h>
#include <SVGPathSegLineto.h>
#include <CSSStyleDeclaration.h>
#include <SVGLength.h>
#include <NodeListImpl.h>
#include <SVGFEColorMatrixElement.h>
#include <StyleSheetImpl.h>
#include <SVGFEGaussianBlurElement.h>
#include <SVGSymbolElement.h>
//#include <KSVGView.h>
#include <SVGFEMergeNodeElementImpl.h>
#include <NotationImpl.h>
#include <SVGFECompositeElement.h>
#include <SVGFEFloodElementImpl.h>
#include <SVGAnimatedNumberListImpl.h>
#include <SVGExternalResourcesRequiredImpl.h>
#include <ProcessingInstructionImpl.h>
#include <RectImpl.h>
#include <SVGPathSegArc.h>
#include <RegisteredEventListener.h>
#include <SVGViewElement.h>
#include <DOMLocator.h>
#include <SVGAnimatedBooleanImpl.h>
#include <SVGElementInstanceList.h>
#include <SVGTextElementImpl.h>
#include <SVGFEMergeElementImpl.h>
#include <SVGGradientElementImpl.h>
#include <SVGSwitchElement.h>
#include <CounterImpl.h>
#include <SVGLineElementImpl.h>
#include <KSVGCSSParser.h>
#include <SVGAnimatedNumberList.h>
#include <DOMErrorHandlerImpl.h>
#include <DocumentViewImpl.h>
#include <StyleBaseImpl.h>
#include <SVGPolylineElement.h>
#include <SVGFEColorMatrixElementImpl.h>
#include <SVGFitToViewBox.h>
#include <SVGTSpanElement.h>
#include <StyleSheet.h>
#include <SVGCircleElementImpl.h>
#include <SVGTransformList.h>
#include <SVGFEImageElement.h>
#include <CSSMediaRuleImpl.h>
#include <SVGRectImpl.h>
#include <SVGRectElementImpl.h>
#include <SVGPolygonElementImpl.h>
#include <ElementImpl.h>
#include <SVGLinearGradientElement.h>
#include <SVGPathSegClosePath.h>
#include <EntityReferenceImpl.h>
#include <SVGAnimatedBoolean.h>
#include <SVGPathSegCurvetoCubicImpl.h>
#include <DocumentStyleImpl.h>
#include <CommentImpl.h>
#include <TextImpl.h>
#include <SVGFEComponentTransferElementImpl.h>
#include <EventListener.h>
#include <EntityImpl.h>
#include <SVGPaint.h>
#include <SVGSVGElement.h>
#include <NamedNodeMapImpl.h>
#include <MediaListImpl.h>
#include <SVGPatternElement.h>
#include <SVGAnimatedEnumerationImpl.h>
#include <SVGAnimatedPathDataImpl.h>
#include <SVGFEOffsetElementImpl.h>
#include <DocumentStyle.h>
#include <SVGPolygonElement.h>
#include <SVGPaintImpl.h>
#include <EntityReference.h>
#include <XMLElementImpl.h>
#include <EventTarget.h>
#include <KDOMCachedObject.h>
#include <SVGAnimatedNumberImpl.h>
#include <CSSRuleList.h>
#include <DocumentRangeImpl.h>
#include <SVGDOMImplementationImpl.h>
#include <CSSMediaRule.h>
#include <SVGPathElement.h>
#include <SVGPathSegCurvetoQuadraticImpl.h>
#include <DOMObject.h>

using namespace KJS;
using namespace KSVG;

// For all classes with generated data: the ClassInfo
const ClassInfo SVGAElement::s_classInfo = {"KSVG::SVGAElement",0,&SVGAElement::s_hashTable,0};
const ClassInfo SVGAngle::s_classInfo = {"KSVG::SVGAngle",0,&SVGAngle::s_hashTable,0};
const ClassInfo SVGAnimateColorElement::s_classInfo = {"KSVG::SVGAnimateColorElement",0,&SVGAnimateColorElement::s_hashTable,0};
const ClassInfo SVGAnimateElement::s_classInfo = {"KSVG::SVGAnimateElement",0,&SVGAnimateElement::s_hashTable,0};
const ClassInfo SVGAnimateTransformElement::s_classInfo = {"KSVG::SVGAnimateTransformElement",0,&SVGAnimateTransformElement::s_hashTable,0};
const ClassInfo SVGAnimatedAngle::s_classInfo = {"KSVG::SVGAnimatedAngle",0,&SVGAnimatedAngle::s_hashTable,0};
const ClassInfo SVGAnimatedBoolean::s_classInfo = {"KSVG::SVGAnimatedBoolean",0,&SVGAnimatedBoolean::s_hashTable,0};
const ClassInfo SVGAnimatedEnumeration::s_classInfo = {"KSVG::SVGAnimatedEnumeration",0,&SVGAnimatedEnumeration::s_hashTable,0};
const ClassInfo SVGAnimatedInteger::s_classInfo = {"KSVG::SVGAnimatedInteger",0,&SVGAnimatedInteger::s_hashTable,0};
const ClassInfo SVGAnimatedLength::s_classInfo = {"KSVG::SVGAnimatedLength",0,&SVGAnimatedLength::s_hashTable,0};
const ClassInfo SVGAnimatedLengthList::s_classInfo = {"KSVG::SVGAnimatedLengthList",0,&SVGAnimatedLengthList::s_hashTable,0};
const ClassInfo SVGAnimatedNumber::s_classInfo = {"KSVG::SVGAnimatedNumber",0,&SVGAnimatedNumber::s_hashTable,0};
const ClassInfo SVGAnimatedNumberList::s_classInfo = {"KSVG::SVGAnimatedNumberList",0,&SVGAnimatedNumberList::s_hashTable,0};
const ClassInfo SVGAnimatedPathData::s_classInfo = {"KSVG::SVGAnimatedPathData",0,&SVGAnimatedPathData::s_hashTable,0};
const ClassInfo SVGAnimatedPoints::s_classInfo = {"KSVG::SVGAnimatedPoints",0,&SVGAnimatedPoints::s_hashTable,0};
const ClassInfo SVGAnimatedPreserveAspectRatio::s_classInfo = {"KSVG::SVGAnimatedPreserveAspectRatio",0,&SVGAnimatedPreserveAspectRatio::s_hashTable,0};
const ClassInfo SVGAnimatedRect::s_classInfo = {"KSVG::SVGAnimatedRect",0,&SVGAnimatedRect::s_hashTable,0};
const ClassInfo SVGAnimatedString::s_classInfo = {"KSVG::SVGAnimatedString",0,&SVGAnimatedString::s_hashTable,0};
const ClassInfo SVGAnimatedTransformList::s_classInfo = {"KSVG::SVGAnimatedTransformList",0,&SVGAnimatedTransformList::s_hashTable,0};
const ClassInfo SVGAnimationElement::s_classInfo = {"KSVG::SVGAnimationElement",0,&SVGAnimationElement::s_hashTable,0};
const ClassInfo SVGCircleElement::s_classInfo = {"KSVG::SVGCircleElement",0,&SVGCircleElement::s_hashTable,0};
const ClassInfo SVGClipPathElement::s_classInfo = {"KSVG::SVGClipPathElement",0,&SVGClipPathElement::s_hashTable,0};
const ClassInfo SVGColor::s_classInfo = {"KSVG::SVGColor",0,&SVGColor::s_hashTable,0};
const ClassInfo SVGComponentTransferFunctionElement::s_classInfo = {"KSVG::SVGComponentTransferFunctionElement",0,&SVGComponentTransferFunctionElement::s_hashTable,0};
const ClassInfo SVGDefsElement::s_classInfo = {"KSVG::SVGDefsElement",0,&SVGDefsElement::s_hashTable,0};
const ClassInfo SVGDescElement::s_classInfo = {"KSVG::SVGDescElement",0,&SVGDescElement::s_hashTable,0};
const ClassInfo SVGDocument::s_classInfo = {"KSVG::SVGDocument",0,&SVGDocument::s_hashTable,0};
const ClassInfo SVGElement::s_classInfo = {"KSVG::SVGElement",0,&SVGElement::s_hashTable,0};
const ClassInfo SVGElementInstance::s_classInfo = {"KSVG::SVGElementInstance",0,&SVGElementInstance::s_hashTable,0};
const ClassInfo SVGElementInstanceList::s_classInfo = {"KSVG::SVGElementInstanceList",0,&SVGElementInstanceList::s_hashTable,0};
const ClassInfo SVGEllipseElement::s_classInfo = {"KSVG::SVGEllipseElement",0,&SVGEllipseElement::s_hashTable,0};
const ClassInfo SVGEvent::s_classInfo = {"KSVG::SVGEvent",0,&SVGEvent::s_hashTable,0};
const ClassInfo SVGException::s_classInfo = {"KSVG::SVGException",0,&SVGException::s_hashTable,0};
const ClassInfo SVGExternalResourcesRequired::s_classInfo = {"KSVG::SVGExternalResourcesRequired",0,&SVGExternalResourcesRequired::s_hashTable,0};
const ClassInfo SVGFEBlendElement::s_classInfo = {"KSVG::SVGFEBlendElement",0,&SVGFEBlendElement::s_hashTable,0};
const ClassInfo SVGFEColorMatrixElement::s_classInfo = {"KSVG::SVGFEColorMatrixElement",0,&SVGFEColorMatrixElement::s_hashTable,0};
const ClassInfo SVGFEComponentTransferElement::s_classInfo = {"KSVG::SVGFEComponentTransferElement",0,&SVGFEComponentTransferElement::s_hashTable,0};
const ClassInfo SVGFECompositeElement::s_classInfo = {"KSVG::SVGFECompositeElement",0,&SVGFECompositeElement::s_hashTable,0};
const ClassInfo SVGFEFloodElement::s_classInfo = {"KSVG::SVGFEFloodElement",0,&SVGFEFloodElement::s_hashTable,0};
const ClassInfo SVGFEGaussianBlurElement::s_classInfo = {"KSVG::SVGFEGaussianBlurElement",0,&SVGFEGaussianBlurElement::s_hashTable,0};
const ClassInfo SVGFEImageElement::s_classInfo = {"KSVG::SVGFEImageElement",0,&SVGFEImageElement::s_hashTable,0};
const ClassInfo SVGFEMergeElement::s_classInfo = {"KSVG::SVGFEMergeElement",0,&SVGFEMergeElement::s_hashTable,0};
const ClassInfo SVGFEMergeNodeElement::s_classInfo = {"KSVG::SVGFEMergeNodeElement",0,&SVGFEMergeNodeElement::s_hashTable,0};
const ClassInfo SVGFEOffsetElement::s_classInfo = {"KSVG::SVGFEOffsetElement",0,&SVGFEOffsetElement::s_hashTable,0};
const ClassInfo SVGFETileElement::s_classInfo = {"KSVG::SVGFETileElement",0,&SVGFETileElement::s_hashTable,0};
const ClassInfo SVGFETurbulenceElement::s_classInfo = {"KSVG::SVGFETurbulenceElement",0,&SVGFETurbulenceElement::s_hashTable,0};
const ClassInfo SVGFilterElement::s_classInfo = {"KSVG::SVGFilterElement",0,&SVGFilterElement::s_hashTable,0};
const ClassInfo SVGFilterPrimitiveStandardAttributes::s_classInfo = {"KSVG::SVGFilterPrimitiveStandardAttributes",0,&SVGFilterPrimitiveStandardAttributes::s_hashTable,0};
const ClassInfo SVGFitToViewBox::s_classInfo = {"KSVG::SVGFitToViewBox",0,&SVGFitToViewBox::s_hashTable,0};
const ClassInfo SVGGElement::s_classInfo = {"KSVG::SVGGElement",0,&SVGGElement::s_hashTable,0};
const ClassInfo SVGGradientElement::s_classInfo = {"KSVG::SVGGradientElement",0,&SVGGradientElement::s_hashTable,0};
const ClassInfo SVGImageElement::s_classInfo = {"KSVG::SVGImageElement",0,&SVGImageElement::s_hashTable,0};
const ClassInfo SVGLangSpace::s_classInfo = {"KSVG::SVGLangSpace",0,&SVGLangSpace::s_hashTable,0};
const ClassInfo SVGLength::s_classInfo = {"KSVG::SVGLength",0,&SVGLength::s_hashTable,0};
const ClassInfo SVGLengthList::s_classInfo = {"KSVG::SVGLengthList",0,&SVGLengthList::s_hashTable,0};
const ClassInfo SVGLineElement::s_classInfo = {"KSVG::SVGLineElement",0,&SVGLineElement::s_hashTable,0};
const ClassInfo SVGLinearGradientElement::s_classInfo = {"KSVG::SVGLinearGradientElement",0,&SVGLinearGradientElement::s_hashTable,0};
const ClassInfo SVGLocatable::s_classInfo = {"KSVG::SVGLocatable",0,&SVGLocatable::s_hashTable,0};
const ClassInfo SVGMarkerElement::s_classInfo = {"KSVG::SVGMarkerElement",0,&SVGMarkerElement::s_hashTable,0};
const ClassInfo SVGMatrix::s_classInfo = {"KSVG::SVGMatrix",0,&SVGMatrix::s_hashTable,0};
const ClassInfo SVGNumber::s_classInfo = {"KSVG::SVGNumber",0,&SVGNumber::s_hashTable,0};
const ClassInfo SVGNumberList::s_classInfo = {"KSVG::SVGNumberList",0,&SVGNumberList::s_hashTable,0};
const ClassInfo SVGPaint::s_classInfo = {"KSVG::SVGPaint",0,&SVGPaint::s_hashTable,0};
const ClassInfo SVGPathElement::s_classInfo = {"KSVG::SVGPathElement",0,&SVGPathElement::s_hashTable,0};
const ClassInfo SVGPathSeg::s_classInfo = {"KSVG::SVGPathSeg",0,&SVGPathSeg::s_hashTable,0};
const ClassInfo SVGPathSegArcAbs::s_classInfo = {"KSVG::SVGPathSegArcAbs",0,&SVGPathSegArcAbs::s_hashTable,0};
const ClassInfo SVGPathSegArcRel::s_classInfo = {"KSVG::SVGPathSegArcRel",0,&SVGPathSegArcRel::s_hashTable,0};
const ClassInfo SVGPathSegCurvetoCubicAbs::s_classInfo = {"KSVG::SVGPathSegCurvetoCubicAbs",0,&SVGPathSegCurvetoCubicAbs::s_hashTable,0};
const ClassInfo SVGPathSegCurvetoCubicRel::s_classInfo = {"KSVG::SVGPathSegCurvetoCubicRel",0,&SVGPathSegCurvetoCubicRel::s_hashTable,0};
const ClassInfo SVGPathSegCurvetoCubicSmoothAbs::s_classInfo = {"KSVG::SVGPathSegCurvetoCubicSmoothAbs",0,&SVGPathSegCurvetoCubicSmoothAbs::s_hashTable,0};
const ClassInfo SVGPathSegCurvetoCubicSmoothRel::s_classInfo = {"KSVG::SVGPathSegCurvetoCubicSmoothRel",0,&SVGPathSegCurvetoCubicSmoothRel::s_hashTable,0};
const ClassInfo SVGPathSegCurvetoQuadraticAbs::s_classInfo = {"KSVG::SVGPathSegCurvetoQuadraticAbs",0,&SVGPathSegCurvetoQuadraticAbs::s_hashTable,0};
const ClassInfo SVGPathSegCurvetoQuadraticRel::s_classInfo = {"KSVG::SVGPathSegCurvetoQuadraticRel",0,&SVGPathSegCurvetoQuadraticRel::s_hashTable,0};
const ClassInfo SVGPathSegCurvetoQuadraticSmoothAbs::s_classInfo = {"KSVG::SVGPathSegCurvetoQuadraticSmoothAbs",0,&SVGPathSegCurvetoQuadraticSmoothAbs::s_hashTable,0};
const ClassInfo SVGPathSegCurvetoQuadraticSmoothRel::s_classInfo = {"KSVG::SVGPathSegCurvetoQuadraticSmoothRel",0,&SVGPathSegCurvetoQuadraticSmoothRel::s_hashTable,0};
const ClassInfo SVGPathSegLinetoAbs::s_classInfo = {"KSVG::SVGPathSegLinetoAbs",0,&SVGPathSegLinetoAbs::s_hashTable,0};
const ClassInfo SVGPathSegLinetoHorizontalAbs::s_classInfo = {"KSVG::SVGPathSegLinetoHorizontalAbs",0,&SVGPathSegLinetoHorizontalAbs::s_hashTable,0};
const ClassInfo SVGPathSegLinetoHorizontalRel::s_classInfo = {"KSVG::SVGPathSegLinetoHorizontalRel",0,&SVGPathSegLinetoHorizontalRel::s_hashTable,0};
const ClassInfo SVGPathSegLinetoRel::s_classInfo = {"KSVG::SVGPathSegLinetoRel",0,&SVGPathSegLinetoRel::s_hashTable,0};
const ClassInfo SVGPathSegLinetoVerticalAbs::s_classInfo = {"KSVG::SVGPathSegLinetoVerticalAbs",0,&SVGPathSegLinetoVerticalAbs::s_hashTable,0};
const ClassInfo SVGPathSegLinetoVerticalRel::s_classInfo = {"KSVG::SVGPathSegLinetoVerticalRel",0,&SVGPathSegLinetoVerticalRel::s_hashTable,0};
const ClassInfo SVGPathSegList::s_classInfo = {"KSVG::SVGPathSegList",0,&SVGPathSegList::s_hashTable,0};
const ClassInfo SVGPathSegMovetoAbs::s_classInfo = {"KSVG::SVGPathSegMovetoAbs",0,&SVGPathSegMovetoAbs::s_hashTable,0};
const ClassInfo SVGPathSegMovetoRel::s_classInfo = {"KSVG::SVGPathSegMovetoRel",0,&SVGPathSegMovetoRel::s_hashTable,0};
const ClassInfo SVGPatternElement::s_classInfo = {"KSVG::SVGPatternElement",0,&SVGPatternElement::s_hashTable,0};
const ClassInfo SVGPoint::s_classInfo = {"KSVG::SVGPoint",0,&SVGPoint::s_hashTable,0};
const ClassInfo SVGPointList::s_classInfo = {"KSVG::SVGPointList",0,&SVGPointList::s_hashTable,0};
const ClassInfo SVGPolygonElement::s_classInfo = {"KSVG::SVGPolygonElement",0,&SVGPolygonElement::s_hashTable,0};
const ClassInfo SVGPolylineElement::s_classInfo = {"KSVG::SVGPolylineElement",0,&SVGPolylineElement::s_hashTable,0};
const ClassInfo SVGPreserveAspectRatio::s_classInfo = {"KSVG::SVGPreserveAspectRatio",0,&SVGPreserveAspectRatio::s_hashTable,0};
const ClassInfo SVGRadialGradientElement::s_classInfo = {"KSVG::SVGRadialGradientElement",0,&SVGRadialGradientElement::s_hashTable,0};
const ClassInfo SVGRect::s_classInfo = {"KSVG::SVGRect",0,&SVGRect::s_hashTable,0};
const ClassInfo SVGRectElement::s_classInfo = {"KSVG::SVGRectElement",0,&SVGRectElement::s_hashTable,0};
const ClassInfo SVGSVGElement::s_classInfo = {"KSVG::SVGSVGElement",0,&SVGSVGElement::s_hashTable,0};
const ClassInfo SVGScriptElement::s_classInfo = {"KSVG::SVGScriptElement",0,&SVGScriptElement::s_hashTable,0};
const ClassInfo SVGSetElement::s_classInfo = {"KSVG::SVGSetElement",0,&SVGSetElement::s_hashTable,0};
const ClassInfo SVGStopElement::s_classInfo = {"KSVG::SVGStopElement",0,&SVGStopElement::s_hashTable,0};
const ClassInfo SVGStringList::s_classInfo = {"KSVG::SVGStringList",0,&SVGStringList::s_hashTable,0};
const ClassInfo SVGStylable::s_classInfo = {"KSVG::SVGStylable",0,&SVGStylable::s_hashTable,0};
const ClassInfo SVGStyleElement::s_classInfo = {"KSVG::SVGStyleElement",0,&SVGStyleElement::s_hashTable,0};
const ClassInfo SVGSwitchElement::s_classInfo = {"KSVG::SVGSwitchElement",0,&SVGSwitchElement::s_hashTable,0};
const ClassInfo SVGSymbolElement::s_classInfo = {"KSVG::SVGSymbolElement",0,&SVGSymbolElement::s_hashTable,0};
const ClassInfo SVGTSpanElement::s_classInfo = {"KSVG::SVGTSpanElement",0,&SVGTSpanElement::s_hashTable,0};
const ClassInfo SVGTests::s_classInfo = {"KSVG::SVGTests",0,&SVGTests::s_hashTable,0};
const ClassInfo SVGTextContentElement::s_classInfo = {"KSVG::SVGTextContentElement",0,&SVGTextContentElement::s_hashTable,0};
const ClassInfo SVGTextElement::s_classInfo = {"KSVG::SVGTextElement",0,&SVGTextElement::s_hashTable,0};
const ClassInfo SVGTextPositioningElement::s_classInfo = {"KSVG::SVGTextPositioningElement",0,&SVGTextPositioningElement::s_hashTable,0};
const ClassInfo SVGTitleElement::s_classInfo = {"KSVG::SVGTitleElement",0,&SVGTitleElement::s_hashTable,0};
const ClassInfo SVGTransform::s_classInfo = {"KSVG::SVGTransform",0,&SVGTransform::s_hashTable,0};
const ClassInfo SVGTransformList::s_classInfo = {"KSVG::SVGTransformList",0,&SVGTransformList::s_hashTable,0};
const ClassInfo SVGTransformable::s_classInfo = {"KSVG::SVGTransformable",0,&SVGTransformable::s_hashTable,0};
const ClassInfo SVGURIReference::s_classInfo = {"KSVG::SVGURIReference",0,&SVGURIReference::s_hashTable,0};
const ClassInfo SVGUseElement::s_classInfo = {"KSVG::SVGUseElement",0,&SVGUseElement::s_hashTable,0};
const ClassInfo SVGViewElement::s_classInfo = {"KSVG::SVGViewElement",0,&SVGViewElement::s_hashTable,0};
const ClassInfo SVGZoomAndPan::s_classInfo = {"KSVG::SVGZoomAndPan",0,&SVGZoomAndPan::s_hashTable,0};
const ClassInfo SVGZoomEvent::s_classInfo = {"KSVG::SVGZoomEvent",0,&SVGZoomEvent::s_hashTable,0};
// Generated methods
bool SVGAElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGTransformable::hasProperty(p1,p2)) return true;
    if(SVGURIReference::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGAElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGTransformable::hasProperty(p1,p2)) return SVGTransformable::get(p1,p2,p3);
    if(SVGURIReference::hasProperty(p1,p2)) return SVGURIReference::get(p1,p2,p3);
    return Undefined();
}

bool SVGAElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGAElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGAElement>(p1,static_cast<SVGAElement::Private *>(d));
}

Value SVGAElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAElement>(p1,static_cast<SVGAElement::Private *>(d));
}

bool SVGAngle::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAngle::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGAngleProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value SVGAngle::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGAngleProtoFunc,SVGAngle>(p1,p2,&s_hashTable,this,p3);
}

SVGAngle SVGAngleProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    return KSVG::toSVGAngle(exec, p1);
}

SVGAngle KSVG::toSVGAngle(KJS::ExecState *, const ObjectImp *p1)
{
    { const KDOM::DOMBridge<SVGAngle> *test = dynamic_cast<const KDOM::DOMBridge<SVGAngle> * >(p1);
    if(test) return SVGAngle(static_cast<SVGAngle::Private *>(test->impl())); }
    return SVGAngle::null;
}

Value SVGAngle::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGAngleProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

bool SVGAngle::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGAngle>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGAngle::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGAngle::prototype(ExecState *p1) const
{
    if(p1) return SVGAngleProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAngle::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGAngle>(p1,static_cast<SVGAngle::Private *>(impl));
}

Value SVGAngle::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAngle>(p1,static_cast<SVGAngle::Private *>(impl));
}

bool SVGAnimateColorElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimateColorElement::s_hashTable,p2);
    if(e) return true;
    if(SVGAnimationElement::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGAnimateColorElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimateColorElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimateColorElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGAnimationElement::hasProperty(p1,p2)) return SVGAnimationElement::get(p1,p2,p3);
    return Undefined();
}

bool SVGAnimateColorElement::put(PUT_METHOD_ARGS)
{
    if(SVGAnimationElement::hasProperty(p1,p2)) {
        SVGAnimationElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGAnimateColorElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimateColorElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGAnimateColorElement>(p1,static_cast<SVGAnimateColorElement::Private *>(d));
}

Value SVGAnimateColorElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimateColorElement>(p1,static_cast<SVGAnimateColorElement::Private *>(d));
}

bool SVGAnimateElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimateElement::s_hashTable,p2);
    if(e) return true;
    if(SVGAnimationElement::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGAnimateElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimateElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimateElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGAnimationElement::hasProperty(p1,p2)) return SVGAnimationElement::get(p1,p2,p3);
    return Undefined();
}

bool SVGAnimateElement::put(PUT_METHOD_ARGS)
{
    if(SVGAnimationElement::hasProperty(p1,p2)) {
        SVGAnimationElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGAnimateElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimateElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGAnimateElement>(p1,static_cast<SVGAnimateElement::Private *>(d));
}

Value SVGAnimateElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimateElement>(p1,static_cast<SVGAnimateElement::Private *>(d));
}

bool SVGAnimateTransformElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimateTransformElement::s_hashTable,p2);
    if(e) return true;
    if(SVGAnimationElement::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGAnimateTransformElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimateTransformElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimateTransformElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGAnimationElement::hasProperty(p1,p2)) return SVGAnimationElement::get(p1,p2,p3);
    return Undefined();
}

bool SVGAnimateTransformElement::put(PUT_METHOD_ARGS)
{
    if(SVGAnimationElement::hasProperty(p1,p2)) {
        SVGAnimationElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGAnimateTransformElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimateTransformElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGAnimateTransformElement>(p1,static_cast<SVGAnimateTransformElement::Private *>(d));
}

Value SVGAnimateTransformElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimateTransformElement>(p1,static_cast<SVGAnimateTransformElement::Private *>(d));
}

bool SVGAnimatedAngle::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimatedAngle::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGAnimatedAngle::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimatedAngle>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimatedAngle::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object SVGAnimatedAngle::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimatedAngle::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGAnimatedAngle>(p1,static_cast<SVGAnimatedAngle::Private *>(impl));
}

Value SVGAnimatedAngle::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimatedAngle>(p1,static_cast<SVGAnimatedAngle::Private *>(impl));
}

bool SVGAnimatedBoolean::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimatedBoolean::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGAnimatedBoolean::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimatedBoolean>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimatedBoolean::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

bool SVGAnimatedBoolean::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGAnimatedBoolean>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGAnimatedBoolean::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGAnimatedBoolean::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimatedBoolean::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGAnimatedBoolean>(p1,static_cast<SVGAnimatedBoolean::Private *>(impl));
}

Value SVGAnimatedBoolean::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimatedBoolean>(p1,static_cast<SVGAnimatedBoolean::Private *>(impl));
}

bool SVGAnimatedEnumeration::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimatedEnumeration::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGAnimatedEnumeration::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimatedEnumeration>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimatedEnumeration::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

bool SVGAnimatedEnumeration::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGAnimatedEnumeration>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGAnimatedEnumeration::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGAnimatedEnumeration::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimatedEnumeration::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGAnimatedEnumeration>(p1,static_cast<SVGAnimatedEnumeration::Private *>(impl));
}

Value SVGAnimatedEnumeration::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimatedEnumeration>(p1,static_cast<SVGAnimatedEnumeration::Private *>(impl));
}

bool SVGAnimatedInteger::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimatedInteger::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGAnimatedInteger::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimatedInteger>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimatedInteger::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

bool SVGAnimatedInteger::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGAnimatedInteger>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGAnimatedInteger::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGAnimatedInteger::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimatedInteger::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGAnimatedInteger>(p1,static_cast<SVGAnimatedInteger::Private *>(impl));
}

Value SVGAnimatedInteger::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimatedInteger>(p1,static_cast<SVGAnimatedInteger::Private *>(impl));
}

bool SVGAnimatedLength::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimatedLength::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGAnimatedLength::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimatedLength>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimatedLength::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object SVGAnimatedLength::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimatedLength::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGAnimatedLength>(p1,static_cast<SVGAnimatedLength::Private *>(impl));
}

Value SVGAnimatedLength::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimatedLength>(p1,static_cast<SVGAnimatedLength::Private *>(impl));
}

bool SVGAnimatedLengthList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimatedLengthList::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGAnimatedLengthList::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimatedLengthList>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimatedLengthList::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object SVGAnimatedLengthList::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimatedLengthList::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGAnimatedLengthList>(p1,static_cast<SVGAnimatedLengthList::Private *>(impl));
}

Value SVGAnimatedLengthList::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimatedLengthList>(p1,static_cast<SVGAnimatedLengthList::Private *>(impl));
}

bool SVGAnimatedNumber::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimatedNumber::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGAnimatedNumber::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimatedNumber>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimatedNumber::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

bool SVGAnimatedNumber::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGAnimatedNumber>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGAnimatedNumber::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGAnimatedNumber::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimatedNumber::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGAnimatedNumber>(p1,static_cast<SVGAnimatedNumber::Private *>(impl));
}

Value SVGAnimatedNumber::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimatedNumber>(p1,static_cast<SVGAnimatedNumber::Private *>(impl));
}

bool SVGAnimatedNumberList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimatedNumberList::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGAnimatedNumberList::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimatedNumberList>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimatedNumberList::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object SVGAnimatedNumberList::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimatedNumberList::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGAnimatedNumberList>(p1,static_cast<SVGAnimatedNumberList::Private *>(impl));
}

Value SVGAnimatedNumberList::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimatedNumberList>(p1,static_cast<SVGAnimatedNumberList::Private *>(impl));
}

bool SVGAnimatedPathData::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimatedPathData::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGAnimatedPathData::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimatedPathData>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimatedPathData::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object SVGAnimatedPathData::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimatedPathData::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGAnimatedPathData>(p1,static_cast<SVGAnimatedPathData::Private *>(impl));
}

Value SVGAnimatedPathData::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimatedPathData>(p1,static_cast<SVGAnimatedPathData::Private *>(impl));
}

bool SVGAnimatedPoints::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimatedPoints::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGAnimatedPoints::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimatedPoints>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimatedPoints::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object SVGAnimatedPoints::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimatedPoints::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGAnimatedPoints>(p1,static_cast<SVGAnimatedPoints::Private *>(impl));
}

Value SVGAnimatedPoints::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimatedPoints>(p1,static_cast<SVGAnimatedPoints::Private *>(impl));
}

bool SVGAnimatedPreserveAspectRatio::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimatedPreserveAspectRatio::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGAnimatedPreserveAspectRatio::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimatedPreserveAspectRatio>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimatedPreserveAspectRatio::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object SVGAnimatedPreserveAspectRatio::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimatedPreserveAspectRatio::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGAnimatedPreserveAspectRatio>(p1,static_cast<SVGAnimatedPreserveAspectRatio::Private *>(impl));
}

Value SVGAnimatedPreserveAspectRatio::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimatedPreserveAspectRatio>(p1,static_cast<SVGAnimatedPreserveAspectRatio::Private *>(impl));
}

bool SVGAnimatedRect::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimatedRect::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGAnimatedRect::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimatedRect>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimatedRect::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object SVGAnimatedRect::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimatedRect::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGAnimatedRect>(p1,static_cast<SVGAnimatedRect::Private *>(impl));
}

Value SVGAnimatedRect::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimatedRect>(p1,static_cast<SVGAnimatedRect::Private *>(impl));
}

bool SVGAnimatedString::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimatedString::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGAnimatedString::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimatedString>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimatedString::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

bool SVGAnimatedString::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGAnimatedString>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGAnimatedString::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGAnimatedString::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimatedString::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGAnimatedString>(p1,static_cast<SVGAnimatedString::Private *>(impl));
}

Value SVGAnimatedString::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimatedString>(p1,static_cast<SVGAnimatedString::Private *>(impl));
}

bool SVGAnimatedTransformList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimatedTransformList::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGAnimatedTransformList::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGAnimatedTransformList>(p1,p2,&s_hashTable,this,p3);
}

Value SVGAnimatedTransformList::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object SVGAnimatedTransformList::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimatedTransformList::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGAnimatedTransformList>(p1,static_cast<SVGAnimatedTransformList::Private *>(impl));
}

Value SVGAnimatedTransformList::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimatedTransformList>(p1,static_cast<SVGAnimatedTransformList::Private *>(impl));
}

bool SVGAnimationElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGAnimationElement::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGAnimationElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGAnimationElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGAnimationElementProtoFunc,SVGAnimationElement>(p1,p2,&s_hashTable,this,p3);
}

SVGAnimationElement SVGAnimationElementProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGAnimationElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimationElement> * >(p1);
    if(test) return SVGAnimationElement(static_cast<SVGAnimationElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGAnimateColorElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimateColorElement> * >(p1);
    if(test) return SVGAnimationElement(static_cast<SVGAnimationElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGAnimateElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimateElement> * >(p1);
    if(test) return SVGAnimationElement(static_cast<SVGAnimationElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGAnimateTransformElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimateTransformElement> * >(p1);
    if(test) return SVGAnimationElement(static_cast<SVGAnimationElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGSetElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSetElement> * >(p1);
    if(test) return SVGAnimationElement(static_cast<SVGAnimationElement::Private *>(test->impl())); }
    return SVGAnimationElement::null;
}

Value SVGAnimationElement::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGAnimationElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    return Undefined();
}

bool SVGAnimationElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGAnimationElement::prototype(ExecState *p1) const
{
    if(p1) return SVGAnimationElementProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGAnimationElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGAnimationElement>(p1,static_cast<SVGAnimationElement::Private *>(d));
}

Value SVGAnimationElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGAnimationElement>(p1,static_cast<SVGAnimationElement::Private *>(d));
}

bool SVGCircleElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGCircleElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGTransformable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGCircleElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGCircleElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGCircleElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGTransformable::hasProperty(p1,p2)) return SVGTransformable::get(p1,p2,p3);
    return Undefined();
}

bool SVGCircleElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGCircleElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGCircleElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGCircleElement>(p1,static_cast<SVGCircleElement::Private *>(d));
}

Value SVGCircleElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGCircleElement>(p1,static_cast<SVGCircleElement::Private *>(d));
}

bool SVGClipPathElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGClipPathElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGTransformable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGClipPathElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGClipPathElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGClipPathElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGTransformable::hasProperty(p1,p2)) return SVGTransformable::get(p1,p2,p3);
    return Undefined();
}

bool SVGClipPathElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGClipPathElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGClipPathElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGClipPathElement>(p1,static_cast<SVGClipPathElement::Private *>(d));
}

Value SVGClipPathElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGClipPathElement>(p1,static_cast<SVGClipPathElement::Private *>(d));
}

bool SVGColor::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGColor::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGColorProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(KDOM::CSSValue::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGColor::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGColorProtoFunc,SVGColor>(p1,p2,&s_hashTable,this,p3);
}

SVGColor SVGColorProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGColor> *test = dynamic_cast<const KDOM::DOMBridge<SVGColor> * >(p1);
    if(test) return SVGColor(static_cast<SVGColor::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPaint> *test = dynamic_cast<const KDOM::DOMBridge<SVGPaint> * >(p1);
    if(test) return SVGColor(static_cast<SVGColor::Private *>(test->impl())); }
    return SVGColor::null;
}

Value SVGColor::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGColorProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(KDOM::CSSValue::hasProperty(p1,p2)) return KDOM::CSSValue::get(p1,p2,p3);
    return Undefined();
}

bool SVGColor::put(PUT_METHOD_ARGS)
{
    if(KDOM::CSSValue::hasProperty(p1,p2)) {
        KDOM::CSSValue::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGColor::prototype(ExecState *p1) const
{
    if(p1) return SVGColorProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGColor::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGColor>(p1,static_cast<SVGColor::Private *>(d));
}

Value SVGColor::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGColor>(p1,static_cast<SVGColor::Private *>(d));
}

bool SVGComponentTransferFunctionElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGComponentTransferFunctionElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGComponentTransferFunctionElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGComponentTransferFunctionElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGComponentTransferFunctionElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    return Undefined();
}

bool SVGComponentTransferFunctionElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGComponentTransferFunctionElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGComponentTransferFunctionElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGComponentTransferFunctionElement>(p1,static_cast<SVGComponentTransferFunctionElement::Private *>(d));
}

Value SVGComponentTransferFunctionElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGComponentTransferFunctionElement>(p1,static_cast<SVGComponentTransferFunctionElement::Private *>(d));
}

bool SVGDefsElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGDefsElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGTransformable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGDefsElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGDefsElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGDefsElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGTransformable::hasProperty(p1,p2)) return SVGTransformable::get(p1,p2,p3);
    return Undefined();
}

bool SVGDefsElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGDefsElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGDefsElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGDefsElement>(p1,static_cast<SVGDefsElement::Private *>(d));
}

Value SVGDefsElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGDefsElement>(p1,static_cast<SVGDefsElement::Private *>(d));
}

bool SVGDescElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGDescElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGDescElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGDescElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGDescElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    return Undefined();
}

bool SVGDescElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGDescElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGDescElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGDescElement>(p1,static_cast<SVGDescElement::Private *>(d));
}

Value SVGDescElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGDescElement>(p1,static_cast<SVGDescElement::Private *>(d));
}

bool SVGDocument::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGDocument::s_hashTable,p2);
    if(e) return true;
    if(KDOM::Document::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGDocument::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGDocument>(p1,p2,&s_hashTable,this,p3);
}

Value SVGDocument::getInParents(GET_METHOD_ARGS) const
{
    if(KDOM::Document::hasProperty(p1,p2)) return KDOM::Document::get(p1,p2,p3);
    return Undefined();
}

bool SVGDocument::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGDocument>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGDocument::putInParents(PUT_METHOD_ARGS)
{
    if(KDOM::Document::hasProperty(p1,p2)) {
        KDOM::Document::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGDocument::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

KDOM::Document KSVG::EcmaInterface::inheritedDocumentCast(const ObjectImp *p1)
{
	{ const KDOM::DOMBridge<KSVG::SVGDocument> *test = dynamic_cast<const KDOM::DOMBridge<KSVG::SVGDocument> * >(p1);
 	  if(test) return SVGDocument(static_cast<SVGDocument::Private *>(test->impl())); }
	{ KDOM::DocumentImpl *test = dynamic_cast<KDOM::DocumentImpl *>(inheritedDocumentEventCast(p1).d);
 	  if(test) return KDOM::Document(test); }
	{ KDOM::DocumentImpl *test = dynamic_cast<KDOM::DocumentImpl *>(inheritedDocumentRangeCast(p1).d);
 	  if(test) return KDOM::Document(test); }
	{ KDOM::DocumentImpl *test = dynamic_cast<KDOM::DocumentImpl *>(inheritedDocumentTraversalCast(p1).d);
 	  if(test) return KDOM::Document(test); }
	{ KDOM::DocumentImpl *test = dynamic_cast<KDOM::DocumentImpl *>(inheritedNodeCast(p1).d);
 	  if(test) return KDOM::Document(test); }
    return SVGDocument::null;
}

ObjectImp *SVGDocument::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGDocument>(p1,static_cast<SVGDocument::Private *>(KDOM::EventTarget::d));
}

Value SVGDocument::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGDocument>(p1,static_cast<SVGDocument::Private *>(KDOM::EventTarget::d));
}

bool SVGElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGElement::s_hashTable,p2);
    if(e) return true;
    if(KDOM::Element::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGElement>(p1,p2,&s_hashTable,this,p3);
}

SVGElement KSVG::toSVGElement(KJS::ExecState *, const ObjectImp *p1)
{
    { const KDOM::DOMBridge<SVGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGAElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGAnimateColorElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimateColorElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGAnimateElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimateElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGAnimateTransformElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimateTransformElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGAnimationElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimationElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGCircleElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGCircleElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGClipPathElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGClipPathElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGComponentTransferFunctionElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGComponentTransferFunctionElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGDefsElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGDefsElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGDescElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGDescElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGEllipseElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGEllipseElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEBlendElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEBlendElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEColorMatrixElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEColorMatrixElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEComponentTransferElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEComponentTransferElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFECompositeElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFECompositeElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEFloodElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFloodElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEFuncAElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFuncAElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEFuncBElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFuncBElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEFuncGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFuncGElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEFuncRElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFuncRElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEGaussianBlurElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEGaussianBlurElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEImageElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEImageElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEMergeElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEMergeElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEMergeNodeElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEMergeNodeElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEOffsetElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEOffsetElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFETileElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFETileElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFETurbulenceElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFETurbulenceElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFilterElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFilterElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGGElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGGradientElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGGradientElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGImageElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGImageElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGLineElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGLineElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGLinearGradientElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGLinearGradientElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGMarkerElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGMarkerElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPatternElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPatternElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPolygonElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPolygonElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPolylineElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPolylineElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGRadialGradientElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGRadialGradientElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGRectElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGRectElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGSVGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSVGElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGScriptElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGScriptElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGSetElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSetElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGStopElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGStopElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGStyleElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGStyleElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGSwitchElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSwitchElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGSymbolElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSymbolElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTSpanElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTSpanElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTextContentElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextContentElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTextElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTextPositioningElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextPositioningElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTitleElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTitleElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGUseElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGUseElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGViewElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGViewElement> * >(p1);
    if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
    return SVGElement::null;
}

Value SVGElement::getInParents(GET_METHOD_ARGS) const
{
    if(KDOM::Element::hasProperty(p1,p2)) return KDOM::Element::get(p1,p2,p3);
    return Undefined();
}

bool SVGElement::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGElement>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGElement::putInParents(PUT_METHOD_ARGS)
{
    if(KDOM::Element::hasProperty(p1,p2)) {
        KDOM::Element::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

KDOM::Element KSVG::EcmaInterface::inheritedElementCast(const ObjectImp *p1)
{
	{ const KDOM::DOMBridge<KSVG::SVGElement> *test = dynamic_cast<const KDOM::DOMBridge<KSVG::SVGElement> * >(p1);
 	  if(test) return SVGElement(static_cast<SVGElement::Private *>(test->impl())); }
	{ KDOM::ElementImpl *test = dynamic_cast<KDOM::ElementImpl *>(inheritedNodeCast(p1).d);
 	  if(test) return KDOM::Element(test); }
    return SVGElement::null;
}

ObjectImp *SVGElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGElement>(p1,static_cast<SVGElement::Private *>(d));
}

Value SVGElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGElement>(p1,static_cast<SVGElement::Private *>(d));
}

bool SVGElementInstance::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGElementInstance::s_hashTable,p2);
    if(e) return true;
    if(KDOM::EventTarget::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGElementInstance::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGElementInstance>(p1,p2,&s_hashTable,this,p3);
}

Value SVGElementInstance::getInParents(GET_METHOD_ARGS) const
{
    if(KDOM::EventTarget::hasProperty(p1,p2)) return KDOM::EventTarget::get(p1,p2,p3);
    return Undefined();
}

bool SVGElementInstance::put(PUT_METHOD_ARGS)
{
    if(KDOM::EventTarget::hasProperty(p1,p2)) {
        KDOM::EventTarget::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGElementInstance::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGElementInstance::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGElementInstance>(p1,static_cast<SVGElementInstance::Private *>(d));
}

Value SVGElementInstance::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGElementInstance>(p1,static_cast<SVGElementInstance::Private *>(d));
}

bool SVGElementInstanceList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGElementInstanceList::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGElementInstanceListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value SVGElementInstanceList::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGElementInstanceListProtoFunc,SVGElementInstanceList>(p1,p2,&s_hashTable,this,p3);
}

SVGElementInstanceList SVGElementInstanceListProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGElementInstanceList> *test = dynamic_cast<const KDOM::DOMBridge<SVGElementInstanceList> * >(p1);
    if(test) return SVGElementInstanceList(static_cast<SVGElementInstanceList::Private *>(test->impl())); }
    return SVGElementInstanceList::null;
}

Value SVGElementInstanceList::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGElementInstanceListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object SVGElementInstanceList::prototype(ExecState *p1) const
{
    if(p1) return SVGElementInstanceListProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGElementInstanceList::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGElementInstanceList>(p1,static_cast<SVGElementInstanceList::Private *>(impl));
}

Value SVGElementInstanceList::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGElementInstanceList>(p1,static_cast<SVGElementInstanceList::Private *>(impl));
}

bool SVGEllipseElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGEllipseElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGTransformable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGEllipseElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGEllipseElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGEllipseElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGTransformable::hasProperty(p1,p2)) return SVGTransformable::get(p1,p2,p3);
    return Undefined();
}

bool SVGEllipseElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGEllipseElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGEllipseElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGEllipseElement>(p1,static_cast<SVGEllipseElement::Private *>(d));
}

Value SVGEllipseElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGEllipseElement>(p1,static_cast<SVGEllipseElement::Private *>(d));
}

bool SVGEvent::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGEvent::s_hashTable,p2);
    if(e) return true;
    if(KDOM::Event::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGEvent::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGEvent>(p1,p2,&s_hashTable,this,p3);
}

Value SVGEvent::getInParents(GET_METHOD_ARGS) const
{
    if(KDOM::Event::hasProperty(p1,p2)) return KDOM::Event::get(p1,p2,p3);
    return Undefined();
}

bool SVGEvent::put(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGEvent::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGEvent::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGEvent>(p1,static_cast<SVGEvent::Private *>(d));
}

Value SVGEvent::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGEvent>(p1,static_cast<SVGEvent::Private *>(d));
}

bool SVGException::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGException::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGException::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGException>(p1,p2,&s_hashTable,this,p3);
}

Value SVGException::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object SVGException::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGException::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGException>(p1,static_cast<SVGException::Private *>(impl));
}

Value SVGException::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGException>(p1,static_cast<SVGException::Private *>(impl));
}

bool SVGExternalResourcesRequired::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGExternalResourcesRequired::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGExternalResourcesRequired::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGExternalResourcesRequired>(p1,p2,&s_hashTable,this,p3);
}

Value SVGExternalResourcesRequired::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object SVGExternalResourcesRequired::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGExternalResourcesRequired::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGExternalResourcesRequired>(p1,static_cast<SVGExternalResourcesRequired::Private *>(impl));
}

Value SVGExternalResourcesRequired::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGExternalResourcesRequired>(p1,static_cast<SVGExternalResourcesRequired::Private *>(impl));
}

bool SVGFEBlendElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGFEBlendElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGFEBlendElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGFEBlendElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGFEBlendElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return SVGFilterPrimitiveStandardAttributes::get(p1,p2,p3);
    return Undefined();
}

bool SVGFEBlendElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGFEBlendElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGFEBlendElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGFEBlendElement>(p1,static_cast<SVGFEBlendElement::Private *>(d));
}

Value SVGFEBlendElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGFEBlendElement>(p1,static_cast<SVGFEBlendElement::Private *>(d));
}

bool SVGFEColorMatrixElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGFEColorMatrixElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGFEColorMatrixElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGFEColorMatrixElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGFEColorMatrixElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return SVGFilterPrimitiveStandardAttributes::get(p1,p2,p3);
    return Undefined();
}

bool SVGFEColorMatrixElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGFEColorMatrixElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGFEColorMatrixElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGFEColorMatrixElement>(p1,static_cast<SVGFEColorMatrixElement::Private *>(d));
}

Value SVGFEColorMatrixElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGFEColorMatrixElement>(p1,static_cast<SVGFEColorMatrixElement::Private *>(d));
}

bool SVGFEComponentTransferElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGFEComponentTransferElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGFEComponentTransferElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGFEComponentTransferElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGFEComponentTransferElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return SVGFilterPrimitiveStandardAttributes::get(p1,p2,p3);
    return Undefined();
}

bool SVGFEComponentTransferElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGFEComponentTransferElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGFEComponentTransferElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGFEComponentTransferElement>(p1,static_cast<SVGFEComponentTransferElement::Private *>(d));
}

Value SVGFEComponentTransferElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGFEComponentTransferElement>(p1,static_cast<SVGFEComponentTransferElement::Private *>(d));
}

bool SVGFECompositeElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGFECompositeElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGFECompositeElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGFECompositeElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGFECompositeElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return SVGFilterPrimitiveStandardAttributes::get(p1,p2,p3);
    return Undefined();
}

bool SVGFECompositeElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGFECompositeElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGFECompositeElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGFECompositeElement>(p1,static_cast<SVGFECompositeElement::Private *>(d));
}

Value SVGFECompositeElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGFECompositeElement>(p1,static_cast<SVGFECompositeElement::Private *>(d));
}

bool SVGFEFloodElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGFEFloodElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGFEFloodElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGFEFloodElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGFEFloodElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return SVGFilterPrimitiveStandardAttributes::get(p1,p2,p3);
    return Undefined();
}

bool SVGFEFloodElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGFEFloodElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGFEFloodElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGFEFloodElement>(p1,static_cast<SVGFEFloodElement::Private *>(d));
}

Value SVGFEFloodElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGFEFloodElement>(p1,static_cast<SVGFEFloodElement::Private *>(d));
}

bool SVGFEGaussianBlurElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGFEGaussianBlurElement::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGFEGaussianBlurElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGFEGaussianBlurElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGFEGaussianBlurElementProtoFunc,SVGFEGaussianBlurElement>(p1,p2,&s_hashTable,this,p3);
}

SVGFEGaussianBlurElement SVGFEGaussianBlurElementProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGFEGaussianBlurElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEGaussianBlurElement> * >(p1);
    if(test) return SVGFEGaussianBlurElement(static_cast<SVGFEGaussianBlurElement::Private *>(test->impl())); }
    return SVGFEGaussianBlurElement::null;
}

Value SVGFEGaussianBlurElement::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGFEGaussianBlurElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return SVGFilterPrimitiveStandardAttributes::get(p1,p2,p3);
    return Undefined();
}

bool SVGFEGaussianBlurElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGFEGaussianBlurElement::prototype(ExecState *p1) const
{
    if(p1) return SVGFEGaussianBlurElementProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGFEGaussianBlurElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGFEGaussianBlurElement>(p1,static_cast<SVGFEGaussianBlurElement::Private *>(d));
}

Value SVGFEGaussianBlurElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGFEGaussianBlurElement>(p1,static_cast<SVGFEGaussianBlurElement::Private *>(d));
}

bool SVGFEImageElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGFEImageElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGURIReference::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGFEImageElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGFEImageElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGFEImageElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return SVGFilterPrimitiveStandardAttributes::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGURIReference::hasProperty(p1,p2)) return SVGURIReference::get(p1,p2,p3);
    return Undefined();
}

bool SVGFEImageElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGFEImageElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGFEImageElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGFEImageElement>(p1,static_cast<SVGFEImageElement::Private *>(d));
}

Value SVGFEImageElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGFEImageElement>(p1,static_cast<SVGFEImageElement::Private *>(d));
}

bool SVGFEMergeElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGFEMergeElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGFEMergeElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGFEMergeElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGFEMergeElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return SVGFilterPrimitiveStandardAttributes::get(p1,p2,p3);
    return Undefined();
}

bool SVGFEMergeElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGFEMergeElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGFEMergeElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGFEMergeElement>(p1,static_cast<SVGFEMergeElement::Private *>(d));
}

Value SVGFEMergeElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGFEMergeElement>(p1,static_cast<SVGFEMergeElement::Private *>(d));
}

bool SVGFEMergeNodeElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGFEMergeNodeElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGFEMergeNodeElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGFEMergeNodeElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGFEMergeNodeElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    return Undefined();
}

bool SVGFEMergeNodeElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGFEMergeNodeElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGFEMergeNodeElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGFEMergeNodeElement>(p1,static_cast<SVGFEMergeNodeElement::Private *>(d));
}

Value SVGFEMergeNodeElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGFEMergeNodeElement>(p1,static_cast<SVGFEMergeNodeElement::Private *>(d));
}

bool SVGFEOffsetElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGFEOffsetElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGFEOffsetElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGFEOffsetElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGFEOffsetElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return SVGFilterPrimitiveStandardAttributes::get(p1,p2,p3);
    return Undefined();
}

bool SVGFEOffsetElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGFEOffsetElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGFEOffsetElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGFEOffsetElement>(p1,static_cast<SVGFEOffsetElement::Private *>(d));
}

Value SVGFEOffsetElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGFEOffsetElement>(p1,static_cast<SVGFEOffsetElement::Private *>(d));
}

bool SVGFETileElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGFETileElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGFETileElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGFETileElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGFETileElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return SVGFilterPrimitiveStandardAttributes::get(p1,p2,p3);
    return Undefined();
}

bool SVGFETileElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGFETileElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGFETileElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGFETileElement>(p1,static_cast<SVGFETileElement::Private *>(d));
}

Value SVGFETileElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGFETileElement>(p1,static_cast<SVGFETileElement::Private *>(d));
}

bool SVGFETurbulenceElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGFETurbulenceElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGFETurbulenceElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGFETurbulenceElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGFETurbulenceElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGFilterPrimitiveStandardAttributes::hasProperty(p1,p2)) return SVGFilterPrimitiveStandardAttributes::get(p1,p2,p3);
    return Undefined();
}

bool SVGFETurbulenceElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGFETurbulenceElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGFETurbulenceElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGFETurbulenceElement>(p1,static_cast<SVGFETurbulenceElement::Private *>(d));
}

Value SVGFETurbulenceElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGFETurbulenceElement>(p1,static_cast<SVGFETurbulenceElement::Private *>(d));
}

bool SVGFilterElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGFilterElement::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGFilterElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGURIReference::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGFilterElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGFilterElementProtoFunc,SVGFilterElement>(p1,p2,&s_hashTable,this,p3);
}

SVGFilterElement SVGFilterElementProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGFilterElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFilterElement> * >(p1);
    if(test) return SVGFilterElement(static_cast<SVGFilterElement::Private *>(test->impl())); }
    return SVGFilterElement::null;
}

Value SVGFilterElement::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGFilterElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGURIReference::hasProperty(p1,p2)) return SVGURIReference::get(p1,p2,p3);
    return Undefined();
}

bool SVGFilterElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGFilterElement::prototype(ExecState *p1) const
{
    if(p1) return SVGFilterElementProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGFilterElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGFilterElement>(p1,static_cast<SVGFilterElement::Private *>(d));
}

Value SVGFilterElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGFilterElement>(p1,static_cast<SVGFilterElement::Private *>(d));
}

bool SVGFilterPrimitiveStandardAttributes::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGFilterPrimitiveStandardAttributes::s_hashTable,p2);
    if(e) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGFilterPrimitiveStandardAttributes::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGFilterPrimitiveStandardAttributes>(p1,p2,&s_hashTable,this,p3);
}

Value SVGFilterPrimitiveStandardAttributes::getInParents(GET_METHOD_ARGS) const
{
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    return Undefined();
}

Object SVGFilterPrimitiveStandardAttributes::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGFilterPrimitiveStandardAttributes::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGFilterPrimitiveStandardAttributes>(p1,static_cast<SVGFilterPrimitiveStandardAttributes::Private *>(impl));
}

Value SVGFilterPrimitiveStandardAttributes::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGFilterPrimitiveStandardAttributes>(p1,static_cast<SVGFilterPrimitiveStandardAttributes::Private *>(impl));
}

bool SVGFitToViewBox::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGFitToViewBox::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGFitToViewBox::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGFitToViewBox>(p1,p2,&s_hashTable,this,p3);
}

Value SVGFitToViewBox::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object SVGFitToViewBox::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGFitToViewBox::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGFitToViewBox>(p1,static_cast<SVGFitToViewBox::Private *>(impl));
}

Value SVGFitToViewBox::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGFitToViewBox>(p1,static_cast<SVGFitToViewBox::Private *>(impl));
}

bool SVGGElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGGElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGTransformable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGGElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGGElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGGElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGTransformable::hasProperty(p1,p2)) return SVGTransformable::get(p1,p2,p3);
    return Undefined();
}

bool SVGGElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGGElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGGElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGGElement>(p1,static_cast<SVGGElement::Private *>(d));
}

Value SVGGElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGGElement>(p1,static_cast<SVGGElement::Private *>(d));
}

bool SVGGradientElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGGradientElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGURIReference::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGGradientElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGGradientElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGGradientElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGURIReference::hasProperty(p1,p2)) return SVGURIReference::get(p1,p2,p3);
    return Undefined();
}

bool SVGGradientElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGGradientElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGGradientElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGGradientElement>(p1,static_cast<SVGGradientElement::Private *>(d));
}

Value SVGGradientElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGGradientElement>(p1,static_cast<SVGGradientElement::Private *>(d));
}

bool SVGImageElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGImageElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGTransformable::hasProperty(p1,p2)) return true;
    if(SVGURIReference::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGImageElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGImageElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGImageElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGTransformable::hasProperty(p1,p2)) return SVGTransformable::get(p1,p2,p3);
    if(SVGURIReference::hasProperty(p1,p2)) return SVGURIReference::get(p1,p2,p3);
    return Undefined();
}

bool SVGImageElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGImageElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGImageElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGImageElement>(p1,static_cast<SVGImageElement::Private *>(d));
}

Value SVGImageElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGImageElement>(p1,static_cast<SVGImageElement::Private *>(d));
}

bool SVGLangSpace::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGLangSpace::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGLangSpace::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGLangSpace>(p1,p2,&s_hashTable,this,p3);
}

Value SVGLangSpace::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

bool SVGLangSpace::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGLangSpace>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGLangSpace::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGLangSpace::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGLangSpace::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGLangSpace>(p1,static_cast<SVGLangSpace::Private *>(impl));
}

Value SVGLangSpace::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGLangSpace>(p1,static_cast<SVGLangSpace::Private *>(impl));
}

bool SVGLength::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGLength::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGLengthProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value SVGLength::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGLengthProtoFunc,SVGLength>(p1,p2,&s_hashTable,this,p3);
}

SVGLength SVGLengthProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    return KSVG::toSVGLength(exec, p1);
}

SVGLength KSVG::toSVGLength(KJS::ExecState *, const ObjectImp *p1)
{
    { const KDOM::DOMBridge<SVGLength> *test = dynamic_cast<const KDOM::DOMBridge<SVGLength> * >(p1);
    if(test) return SVGLength(static_cast<SVGLength::Private *>(test->impl())); }
    return SVGLength::null;
}

Value SVGLength::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGLengthProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

bool SVGLength::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGLength>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGLength::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGLength::prototype(ExecState *p1) const
{
    if(p1) return SVGLengthProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGLength::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGLength>(p1,static_cast<SVGLength::Private *>(impl));
}

Value SVGLength::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGLength>(p1,static_cast<SVGLength::Private *>(impl));
}

bool SVGLengthList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGLengthList::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGLengthListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value SVGLengthList::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGLengthListProtoFunc,SVGLengthList>(p1,p2,&s_hashTable,this,p3);
}

SVGLengthList SVGLengthListProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGLengthList> *test = dynamic_cast<const KDOM::DOMBridge<SVGLengthList> * >(p1);
    if(test) return SVGLengthList(static_cast<SVGLengthList::Private *>(test->impl())); }
    return SVGLengthList::null;
}

Value SVGLengthList::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGLengthListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object SVGLengthList::prototype(ExecState *p1) const
{
    if(p1) return SVGLengthListProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGLengthList::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGLengthList>(p1,static_cast<SVGLengthList::Private *>(impl));
}

Value SVGLengthList::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGLengthList>(p1,static_cast<SVGLengthList::Private *>(impl));
}

bool SVGLineElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGLineElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGTransformable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGLineElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGLineElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGLineElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGTransformable::hasProperty(p1,p2)) return SVGTransformable::get(p1,p2,p3);
    return Undefined();
}

bool SVGLineElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGLineElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGLineElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGLineElement>(p1,static_cast<SVGLineElement::Private *>(d));
}

Value SVGLineElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGLineElement>(p1,static_cast<SVGLineElement::Private *>(d));
}

bool SVGLinearGradientElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGLinearGradientElement::s_hashTable,p2);
    if(e) return true;
    if(SVGGradientElement::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGLinearGradientElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGLinearGradientElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGLinearGradientElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGGradientElement::hasProperty(p1,p2)) return SVGGradientElement::get(p1,p2,p3);
    return Undefined();
}

bool SVGLinearGradientElement::put(PUT_METHOD_ARGS)
{
    if(SVGGradientElement::hasProperty(p1,p2)) {
        SVGGradientElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGLinearGradientElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGLinearGradientElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGLinearGradientElement>(p1,static_cast<SVGLinearGradientElement::Private *>(d));
}

Value SVGLinearGradientElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGLinearGradientElement>(p1,static_cast<SVGLinearGradientElement::Private *>(d));
}

bool SVGLocatable::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGLocatable::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGLocatableProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value SVGLocatable::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGLocatableProtoFunc,SVGLocatable>(p1,p2,&s_hashTable,this,p3);
}

SVGLocatable SVGLocatableProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGLocatable> *test = dynamic_cast<const KDOM::DOMBridge<SVGLocatable> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGAElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGCircleElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGCircleElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGClipPathElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGClipPathElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGDefsElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGDefsElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGEllipseElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGEllipseElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGGElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGImageElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGImageElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGLineElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGLineElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPolygonElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPolygonElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPolylineElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPolylineElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGRectElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGRectElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGSVGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSVGElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGSwitchElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSwitchElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTextElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTransformable> *test = dynamic_cast<const KDOM::DOMBridge<SVGTransformable> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGUseElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGUseElement> * >(p1);
    if(test) return SVGLocatable(static_cast<SVGLocatable::Private *>(test->impl())); }
    return SVGLocatable::null;
}

Value SVGLocatable::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGLocatableProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object SVGLocatable::prototype(ExecState *p1) const
{
    if(p1) return SVGLocatableProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGLocatable::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGLocatable>(p1,static_cast<SVGLocatable::Private *>(impl));
}

Value SVGLocatable::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGLocatable>(p1,static_cast<SVGLocatable::Private *>(impl));
}

bool SVGMarkerElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGMarkerElement::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGMarkerElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGFitToViewBox::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGMarkerElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGMarkerElementProtoFunc,SVGMarkerElement>(p1,p2,&s_hashTable,this,p3);
}

SVGMarkerElement SVGMarkerElementProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGMarkerElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGMarkerElement> * >(p1);
    if(test) return SVGMarkerElement(static_cast<SVGMarkerElement::Private *>(test->impl())); }
    return SVGMarkerElement::null;
}

Value SVGMarkerElement::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGMarkerElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGFitToViewBox::hasProperty(p1,p2)) return SVGFitToViewBox::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    return Undefined();
}

bool SVGMarkerElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGMarkerElement::prototype(ExecState *p1) const
{
    if(p1) return SVGMarkerElementProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGMarkerElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGMarkerElement>(p1,static_cast<SVGMarkerElement::Private *>(d));
}

Value SVGMarkerElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGMarkerElement>(p1,static_cast<SVGMarkerElement::Private *>(d));
}

bool SVGMatrix::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGMatrix::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGMatrixProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value SVGMatrix::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGMatrixProtoFunc,SVGMatrix>(p1,p2,&s_hashTable,this,p3);
}

SVGMatrix SVGMatrixProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    return KSVG::toSVGMatrix(exec, p1);
}

SVGMatrix KSVG::toSVGMatrix(KJS::ExecState *, const ObjectImp *p1)
{
    { const KDOM::DOMBridge<SVGMatrix> *test = dynamic_cast<const KDOM::DOMBridge<SVGMatrix> * >(p1);
    if(test) return SVGMatrix(static_cast<SVGMatrix::Private *>(test->impl())); }
    return SVGMatrix::null;
}

Value SVGMatrix::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGMatrixProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

bool SVGMatrix::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGMatrix>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGMatrix::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGMatrix::prototype(ExecState *p1) const
{
    if(p1) return SVGMatrixProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGMatrix::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGMatrix>(p1,static_cast<SVGMatrix::Private *>(impl));
}

Value SVGMatrix::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGMatrix>(p1,static_cast<SVGMatrix::Private *>(impl));
}

bool SVGNumber::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGNumber::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGNumber::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGNumber>(p1,p2,&s_hashTable,this,p3);
}

SVGNumber KSVG::toSVGNumber(KJS::ExecState *, const ObjectImp *p1)
{
    { const KDOM::DOMBridge<SVGNumber> *test = dynamic_cast<const KDOM::DOMBridge<SVGNumber> * >(p1);
    if(test) return SVGNumber(static_cast<SVGNumber::Private *>(test->impl())); }
    return SVGNumber::null;
}

Value SVGNumber::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

bool SVGNumber::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGNumber>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGNumber::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGNumber::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGNumber::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGNumber>(p1,static_cast<SVGNumber::Private *>(impl));
}

Value SVGNumber::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGNumber>(p1,static_cast<SVGNumber::Private *>(impl));
}

bool SVGNumberList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGNumberList::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGNumberListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value SVGNumberList::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGNumberListProtoFunc,SVGNumberList>(p1,p2,&s_hashTable,this,p3);
}

SVGNumberList SVGNumberListProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGNumberList> *test = dynamic_cast<const KDOM::DOMBridge<SVGNumberList> * >(p1);
    if(test) return SVGNumberList(static_cast<SVGNumberList::Private *>(test->impl())); }
    return SVGNumberList::null;
}

Value SVGNumberList::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGNumberListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object SVGNumberList::prototype(ExecState *p1) const
{
    if(p1) return SVGNumberListProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGNumberList::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGNumberList>(p1,static_cast<SVGNumberList::Private *>(impl));
}

Value SVGNumberList::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGNumberList>(p1,static_cast<SVGNumberList::Private *>(impl));
}

bool SVGPaint::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPaint::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGPaintProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(SVGColor::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPaint::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGPaintProtoFunc,SVGPaint>(p1,p2,&s_hashTable,this,p3);
}

SVGPaint SVGPaintProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGPaint> *test = dynamic_cast<const KDOM::DOMBridge<SVGPaint> * >(p1);
    if(test) return SVGPaint(static_cast<SVGPaint::Private *>(test->impl())); }
    return SVGPaint::null;
}

Value SVGPaint::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGPaintProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(SVGColor::hasProperty(p1,p2)) return SVGColor::get(p1,p2,p3);
    return Undefined();
}

bool SVGPaint::put(PUT_METHOD_ARGS)
{
    if(SVGColor::hasProperty(p1,p2)) {
        SVGColor::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGPaint::prototype(ExecState *p1) const
{
    if(p1) return SVGPaintProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPaint::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPaint>(p1,static_cast<SVGPaint::Private *>(d));
}

Value SVGPaint::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPaint>(p1,static_cast<SVGPaint::Private *>(d));
}

bool SVGPathElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathElement::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGPathElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(SVGAnimatedPathData::hasProperty(p1,p2)) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGTransformable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGPathElementProtoFunc,SVGPathElement>(p1,p2,&s_hashTable,this,p3);
}

SVGPathElement SVGPathElementProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGPathElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathElement> * >(p1);
    if(test) return SVGPathElement(static_cast<SVGPathElement::Private *>(test->impl())); }
    return SVGPathElement::null;
}

Value SVGPathElement::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGPathElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(SVGAnimatedPathData::hasProperty(p1,p2)) return SVGAnimatedPathData::get(p1,p2,p3);
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGTransformable::hasProperty(p1,p2)) return SVGTransformable::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGPathElement::prototype(ExecState *p1) const
{
    if(p1) return SVGPathElementProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathElement>(p1,static_cast<SVGPathElement::Private *>(d));
}

Value SVGPathElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathElement>(p1,static_cast<SVGPathElement::Private *>(d));
}

bool SVGPathSeg::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSeg::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGPathSeg::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSeg>(p1,p2,&s_hashTable,this,p3);
}

SVGPathSeg KSVG::toSVGPathSeg(KJS::ExecState *, const ObjectImp *p1)
{
    { const KDOM::DOMBridge<SVGPathSeg> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSeg> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegArcAbs> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegArcAbs> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegArcRel> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegArcRel> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegClosePath> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegClosePath> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegCurvetoCubicAbs> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegCurvetoCubicAbs> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegCurvetoCubicRel> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegCurvetoCubicRel> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegCurvetoCubicSmoothAbs> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegCurvetoCubicSmoothAbs> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegCurvetoCubicSmoothRel> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegCurvetoCubicSmoothRel> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegCurvetoQuadraticAbs> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegCurvetoQuadraticAbs> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegCurvetoQuadraticRel> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegCurvetoQuadraticRel> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegCurvetoQuadraticSmoothAbs> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegCurvetoQuadraticSmoothAbs> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegCurvetoQuadraticSmoothRel> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegCurvetoQuadraticSmoothRel> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegLinetoAbs> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegLinetoAbs> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegLinetoHorizontalAbs> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegLinetoHorizontalAbs> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegLinetoHorizontalRel> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegLinetoHorizontalRel> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegLinetoRel> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegLinetoRel> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegLinetoVerticalAbs> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegLinetoVerticalAbs> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegLinetoVerticalRel> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegLinetoVerticalRel> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegMovetoAbs> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegMovetoAbs> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathSegMovetoRel> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegMovetoRel> * >(p1);
    if(test) return SVGPathSeg(static_cast<SVGPathSeg::Private *>(test->impl())); }
    return SVGPathSeg::null;
}

Value SVGPathSeg::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object SVGPathSeg::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSeg::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGPathSeg>(p1,static_cast<SVGPathSeg::Private *>(impl));
}

Value SVGPathSeg::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSeg>(p1,static_cast<SVGPathSeg::Private *>(impl));
}

bool SVGPathSegArcAbs::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegArcAbs::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegArcAbs::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegArcAbs>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegArcAbs::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegArcAbs::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegArcAbs>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegArcAbs::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegArcAbs::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegArcAbs::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegArcAbs>(p1,static_cast<SVGPathSegArcAbs::Private *>(impl));
}

Value SVGPathSegArcAbs::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegArcAbs>(p1,static_cast<SVGPathSegArcAbs::Private *>(impl));
}

bool SVGPathSegArcRel::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegArcRel::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegArcRel::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegArcRel>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegArcRel::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegArcRel::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegArcRel>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegArcRel::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegArcRel::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegArcRel::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegArcRel>(p1,static_cast<SVGPathSegArcRel::Private *>(impl));
}

Value SVGPathSegArcRel::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegArcRel>(p1,static_cast<SVGPathSegArcRel::Private *>(impl));
}

bool SVGPathSegCurvetoCubicAbs::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegCurvetoCubicAbs::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegCurvetoCubicAbs::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegCurvetoCubicAbs>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegCurvetoCubicAbs::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegCurvetoCubicAbs::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegCurvetoCubicAbs>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegCurvetoCubicAbs::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegCurvetoCubicAbs::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegCurvetoCubicAbs::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegCurvetoCubicAbs>(p1,static_cast<SVGPathSegCurvetoCubicAbs::Private *>(impl));
}

Value SVGPathSegCurvetoCubicAbs::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegCurvetoCubicAbs>(p1,static_cast<SVGPathSegCurvetoCubicAbs::Private *>(impl));
}

bool SVGPathSegCurvetoCubicRel::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegCurvetoCubicRel::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegCurvetoCubicRel::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegCurvetoCubicRel>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegCurvetoCubicRel::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegCurvetoCubicRel::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegCurvetoCubicRel>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegCurvetoCubicRel::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegCurvetoCubicRel::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegCurvetoCubicRel::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegCurvetoCubicRel>(p1,static_cast<SVGPathSegCurvetoCubicRel::Private *>(impl));
}

Value SVGPathSegCurvetoCubicRel::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegCurvetoCubicRel>(p1,static_cast<SVGPathSegCurvetoCubicRel::Private *>(impl));
}

bool SVGPathSegCurvetoCubicSmoothAbs::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegCurvetoCubicSmoothAbs::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegCurvetoCubicSmoothAbs::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegCurvetoCubicSmoothAbs>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegCurvetoCubicSmoothAbs::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegCurvetoCubicSmoothAbs::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegCurvetoCubicSmoothAbs>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegCurvetoCubicSmoothAbs::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegCurvetoCubicSmoothAbs::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegCurvetoCubicSmoothAbs::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegCurvetoCubicSmoothAbs>(p1,static_cast<SVGPathSegCurvetoCubicSmoothAbs::Private *>(impl));
}

Value SVGPathSegCurvetoCubicSmoothAbs::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegCurvetoCubicSmoothAbs>(p1,static_cast<SVGPathSegCurvetoCubicSmoothAbs::Private *>(impl));
}

bool SVGPathSegCurvetoCubicSmoothRel::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegCurvetoCubicSmoothRel::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegCurvetoCubicSmoothRel::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegCurvetoCubicSmoothRel>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegCurvetoCubicSmoothRel::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegCurvetoCubicSmoothRel::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegCurvetoCubicSmoothRel>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegCurvetoCubicSmoothRel::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegCurvetoCubicSmoothRel::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegCurvetoCubicSmoothRel::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegCurvetoCubicSmoothRel>(p1,static_cast<SVGPathSegCurvetoCubicSmoothRel::Private *>(impl));
}

Value SVGPathSegCurvetoCubicSmoothRel::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegCurvetoCubicSmoothRel>(p1,static_cast<SVGPathSegCurvetoCubicSmoothRel::Private *>(impl));
}

bool SVGPathSegCurvetoQuadraticAbs::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegCurvetoQuadraticAbs::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegCurvetoQuadraticAbs::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegCurvetoQuadraticAbs>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegCurvetoQuadraticAbs::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegCurvetoQuadraticAbs::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegCurvetoQuadraticAbs>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegCurvetoQuadraticAbs::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegCurvetoQuadraticAbs::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegCurvetoQuadraticAbs::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegCurvetoQuadraticAbs>(p1,static_cast<SVGPathSegCurvetoQuadraticAbs::Private *>(impl));
}

Value SVGPathSegCurvetoQuadraticAbs::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegCurvetoQuadraticAbs>(p1,static_cast<SVGPathSegCurvetoQuadraticAbs::Private *>(impl));
}

bool SVGPathSegCurvetoQuadraticRel::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegCurvetoQuadraticRel::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegCurvetoQuadraticRel::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegCurvetoQuadraticRel>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegCurvetoQuadraticRel::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegCurvetoQuadraticRel::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegCurvetoQuadraticRel>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegCurvetoQuadraticRel::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegCurvetoQuadraticRel::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegCurvetoQuadraticRel::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegCurvetoQuadraticRel>(p1,static_cast<SVGPathSegCurvetoQuadraticRel::Private *>(impl));
}

Value SVGPathSegCurvetoQuadraticRel::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegCurvetoQuadraticRel>(p1,static_cast<SVGPathSegCurvetoQuadraticRel::Private *>(impl));
}

bool SVGPathSegCurvetoQuadraticSmoothAbs::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegCurvetoQuadraticSmoothAbs::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegCurvetoQuadraticSmoothAbs::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegCurvetoQuadraticSmoothAbs>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegCurvetoQuadraticSmoothAbs::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegCurvetoQuadraticSmoothAbs::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegCurvetoQuadraticSmoothAbs>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegCurvetoQuadraticSmoothAbs::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegCurvetoQuadraticSmoothAbs::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegCurvetoQuadraticSmoothAbs::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegCurvetoQuadraticSmoothAbs>(p1,static_cast<SVGPathSegCurvetoQuadraticSmoothAbs::Private *>(impl));
}

Value SVGPathSegCurvetoQuadraticSmoothAbs::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegCurvetoQuadraticSmoothAbs>(p1,static_cast<SVGPathSegCurvetoQuadraticSmoothAbs::Private *>(impl));
}

bool SVGPathSegCurvetoQuadraticSmoothRel::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegCurvetoQuadraticSmoothRel::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegCurvetoQuadraticSmoothRel::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegCurvetoQuadraticSmoothRel>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegCurvetoQuadraticSmoothRel::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegCurvetoQuadraticSmoothRel::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegCurvetoQuadraticSmoothRel>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegCurvetoQuadraticSmoothRel::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegCurvetoQuadraticSmoothRel::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegCurvetoQuadraticSmoothRel::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegCurvetoQuadraticSmoothRel>(p1,static_cast<SVGPathSegCurvetoQuadraticSmoothRel::Private *>(impl));
}

Value SVGPathSegCurvetoQuadraticSmoothRel::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegCurvetoQuadraticSmoothRel>(p1,static_cast<SVGPathSegCurvetoQuadraticSmoothRel::Private *>(impl));
}

bool SVGPathSegLinetoAbs::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegLinetoAbs::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegLinetoAbs::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegLinetoAbs>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegLinetoAbs::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegLinetoAbs::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegLinetoAbs>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegLinetoAbs::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegLinetoAbs::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegLinetoAbs::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegLinetoAbs>(p1,static_cast<SVGPathSegLinetoAbs::Private *>(impl));
}

Value SVGPathSegLinetoAbs::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegLinetoAbs>(p1,static_cast<SVGPathSegLinetoAbs::Private *>(impl));
}

bool SVGPathSegLinetoHorizontalAbs::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegLinetoHorizontalAbs::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegLinetoHorizontalAbs::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegLinetoHorizontalAbs>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegLinetoHorizontalAbs::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegLinetoHorizontalAbs::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegLinetoHorizontalAbs>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegLinetoHorizontalAbs::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegLinetoHorizontalAbs::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegLinetoHorizontalAbs::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegLinetoHorizontalAbs>(p1,static_cast<SVGPathSegLinetoHorizontalAbs::Private *>(impl));
}

Value SVGPathSegLinetoHorizontalAbs::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegLinetoHorizontalAbs>(p1,static_cast<SVGPathSegLinetoHorizontalAbs::Private *>(impl));
}

bool SVGPathSegLinetoHorizontalRel::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegLinetoHorizontalRel::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegLinetoHorizontalRel::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegLinetoHorizontalRel>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegLinetoHorizontalRel::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegLinetoHorizontalRel::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegLinetoHorizontalRel>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegLinetoHorizontalRel::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegLinetoHorizontalRel::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegLinetoHorizontalRel::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegLinetoHorizontalRel>(p1,static_cast<SVGPathSegLinetoHorizontalRel::Private *>(impl));
}

Value SVGPathSegLinetoHorizontalRel::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegLinetoHorizontalRel>(p1,static_cast<SVGPathSegLinetoHorizontalRel::Private *>(impl));
}

bool SVGPathSegLinetoRel::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegLinetoRel::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegLinetoRel::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegLinetoRel>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegLinetoRel::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegLinetoRel::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegLinetoRel>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegLinetoRel::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegLinetoRel::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegLinetoRel::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegLinetoRel>(p1,static_cast<SVGPathSegLinetoRel::Private *>(impl));
}

Value SVGPathSegLinetoRel::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegLinetoRel>(p1,static_cast<SVGPathSegLinetoRel::Private *>(impl));
}

bool SVGPathSegLinetoVerticalAbs::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegLinetoVerticalAbs::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegLinetoVerticalAbs::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegLinetoVerticalAbs>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegLinetoVerticalAbs::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegLinetoVerticalAbs::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegLinetoVerticalAbs>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegLinetoVerticalAbs::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegLinetoVerticalAbs::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegLinetoVerticalAbs::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegLinetoVerticalAbs>(p1,static_cast<SVGPathSegLinetoVerticalAbs::Private *>(impl));
}

Value SVGPathSegLinetoVerticalAbs::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegLinetoVerticalAbs>(p1,static_cast<SVGPathSegLinetoVerticalAbs::Private *>(impl));
}

bool SVGPathSegLinetoVerticalRel::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegLinetoVerticalRel::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegLinetoVerticalRel::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegLinetoVerticalRel>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegLinetoVerticalRel::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegLinetoVerticalRel::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegLinetoVerticalRel>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegLinetoVerticalRel::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegLinetoVerticalRel::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegLinetoVerticalRel::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegLinetoVerticalRel>(p1,static_cast<SVGPathSegLinetoVerticalRel::Private *>(impl));
}

Value SVGPathSegLinetoVerticalRel::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegLinetoVerticalRel>(p1,static_cast<SVGPathSegLinetoVerticalRel::Private *>(impl));
}

bool SVGPathSegList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegList::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGPathSegListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegList::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGPathSegListProtoFunc,SVGPathSegList>(p1,p2,&s_hashTable,this,p3);
}

SVGPathSegList SVGPathSegListProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGPathSegList> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathSegList> * >(p1);
    if(test) return SVGPathSegList(static_cast<SVGPathSegList::Private *>(test->impl())); }
    return SVGPathSegList::null;
}

Value SVGPathSegList::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGPathSegListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object SVGPathSegList::prototype(ExecState *p1) const
{
    if(p1) return SVGPathSegListProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegList::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGPathSegList>(p1,static_cast<SVGPathSegList::Private *>(impl));
}

Value SVGPathSegList::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegList>(p1,static_cast<SVGPathSegList::Private *>(impl));
}

bool SVGPathSegMovetoAbs::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegMovetoAbs::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegMovetoAbs::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegMovetoAbs>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegMovetoAbs::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegMovetoAbs::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegMovetoAbs>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegMovetoAbs::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegMovetoAbs::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegMovetoAbs::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegMovetoAbs>(p1,static_cast<SVGPathSegMovetoAbs::Private *>(impl));
}

Value SVGPathSegMovetoAbs::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegMovetoAbs>(p1,static_cast<SVGPathSegMovetoAbs::Private *>(impl));
}

bool SVGPathSegMovetoRel::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPathSegMovetoRel::s_hashTable,p2);
    if(e) return true;
    if(SVGPathSeg::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPathSegMovetoRel::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPathSegMovetoRel>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPathSegMovetoRel::getInParents(GET_METHOD_ARGS) const
{
    if(SVGPathSeg::hasProperty(p1,p2)) return SVGPathSeg::get(p1,p2,p3);
    return Undefined();
}

bool SVGPathSegMovetoRel::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPathSegMovetoRel>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPathSegMovetoRel::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPathSegMovetoRel::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPathSegMovetoRel::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPathSegMovetoRel>(p1,static_cast<SVGPathSegMovetoRel::Private *>(impl));
}

Value SVGPathSegMovetoRel::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPathSegMovetoRel>(p1,static_cast<SVGPathSegMovetoRel::Private *>(impl));
}

bool SVGPatternElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPatternElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGFitToViewBox::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGURIReference::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPatternElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPatternElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPatternElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGFitToViewBox::hasProperty(p1,p2)) return SVGFitToViewBox::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGURIReference::hasProperty(p1,p2)) return SVGURIReference::get(p1,p2,p3);
    return Undefined();
}

bool SVGPatternElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGPatternElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPatternElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPatternElement>(p1,static_cast<SVGPatternElement::Private *>(d));
}

Value SVGPatternElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPatternElement>(p1,static_cast<SVGPatternElement::Private *>(d));
}

bool SVGPoint::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPoint::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGPointProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPoint::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGPointProtoFunc,SVGPoint>(p1,p2,&s_hashTable,this,p3);
}

SVGPoint SVGPointProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    return KSVG::toSVGPoint(exec, p1);
}

SVGPoint KSVG::toSVGPoint(KJS::ExecState *, const ObjectImp *p1)
{
    { const KDOM::DOMBridge<SVGPoint> *test = dynamic_cast<const KDOM::DOMBridge<SVGPoint> * >(p1);
    if(test) return SVGPoint(static_cast<SVGPoint::Private *>(test->impl())); }
    return SVGPoint::null;
}

Value SVGPoint::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGPointProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

bool SVGPoint::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPoint>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPoint::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPoint::prototype(ExecState *p1) const
{
    if(p1) return SVGPointProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPoint::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPoint>(p1,static_cast<SVGPoint::Private *>(impl));
}

Value SVGPoint::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPoint>(p1,static_cast<SVGPoint::Private *>(impl));
}

bool SVGPointList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPointList::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGPointListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPointList::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGPointListProtoFunc,SVGPointList>(p1,p2,&s_hashTable,this,p3);
}

SVGPointList SVGPointListProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGPointList> *test = dynamic_cast<const KDOM::DOMBridge<SVGPointList> * >(p1);
    if(test) return SVGPointList(static_cast<SVGPointList::Private *>(test->impl())); }
    return SVGPointList::null;
}

Value SVGPointList::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGPointListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object SVGPointList::prototype(ExecState *p1) const
{
    if(p1) return SVGPointListProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPointList::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGPointList>(p1,static_cast<SVGPointList::Private *>(impl));
}

Value SVGPointList::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPointList>(p1,static_cast<SVGPointList::Private *>(impl));
}

bool SVGPolygonElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPolygonElement::s_hashTable,p2);
    if(e) return true;
    if(SVGAnimatedPoints::hasProperty(p1,p2)) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGTransformable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPolygonElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPolygonElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPolygonElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGAnimatedPoints::hasProperty(p1,p2)) return SVGAnimatedPoints::get(p1,p2,p3);
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGTransformable::hasProperty(p1,p2)) return SVGTransformable::get(p1,p2,p3);
    return Undefined();
}

bool SVGPolygonElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGPolygonElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPolygonElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPolygonElement>(p1,static_cast<SVGPolygonElement::Private *>(d));
}

Value SVGPolygonElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPolygonElement>(p1,static_cast<SVGPolygonElement::Private *>(d));
}

bool SVGPolylineElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPolylineElement::s_hashTable,p2);
    if(e) return true;
    if(SVGAnimatedPoints::hasProperty(p1,p2)) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGTransformable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGPolylineElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPolylineElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPolylineElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGAnimatedPoints::hasProperty(p1,p2)) return SVGAnimatedPoints::get(p1,p2,p3);
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGTransformable::hasProperty(p1,p2)) return SVGTransformable::get(p1,p2,p3);
    return Undefined();
}

bool SVGPolylineElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGPolylineElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPolylineElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPolylineElement>(p1,static_cast<SVGPolylineElement::Private *>(d));
}

Value SVGPolylineElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPolylineElement>(p1,static_cast<SVGPolylineElement::Private *>(d));
}

bool SVGPreserveAspectRatio::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGPreserveAspectRatio::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGPreserveAspectRatio::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGPreserveAspectRatio>(p1,p2,&s_hashTable,this,p3);
}

Value SVGPreserveAspectRatio::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

bool SVGPreserveAspectRatio::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGPreserveAspectRatio>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGPreserveAspectRatio::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGPreserveAspectRatio::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGPreserveAspectRatio::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGPreserveAspectRatio>(p1,static_cast<SVGPreserveAspectRatio::Private *>(impl));
}

Value SVGPreserveAspectRatio::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGPreserveAspectRatio>(p1,static_cast<SVGPreserveAspectRatio::Private *>(impl));
}

bool SVGRadialGradientElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGRadialGradientElement::s_hashTable,p2);
    if(e) return true;
    if(SVGGradientElement::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGRadialGradientElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGRadialGradientElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGRadialGradientElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGGradientElement::hasProperty(p1,p2)) return SVGGradientElement::get(p1,p2,p3);
    return Undefined();
}

bool SVGRadialGradientElement::put(PUT_METHOD_ARGS)
{
    if(SVGGradientElement::hasProperty(p1,p2)) {
        SVGGradientElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGRadialGradientElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGRadialGradientElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGRadialGradientElement>(p1,static_cast<SVGRadialGradientElement::Private *>(d));
}

Value SVGRadialGradientElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGRadialGradientElement>(p1,static_cast<SVGRadialGradientElement::Private *>(d));
}

bool SVGRect::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGRect::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGRect::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGRect>(p1,p2,&s_hashTable,this,p3);
}

SVGRect KSVG::toSVGRect(KJS::ExecState *, const ObjectImp *p1)
{
    { const KDOM::DOMBridge<SVGRect> *test = dynamic_cast<const KDOM::DOMBridge<SVGRect> * >(p1);
    if(test) return SVGRect(static_cast<SVGRect::Private *>(test->impl())); }
    return SVGRect::null;
}

Value SVGRect::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

bool SVGRect::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGRect>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGRect::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGRect::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGRect::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGRect>(p1,static_cast<SVGRect::Private *>(impl));
}

Value SVGRect::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGRect>(p1,static_cast<SVGRect::Private *>(impl));
}

bool SVGRectElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGRectElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGTransformable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGRectElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGRectElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGRectElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGTransformable::hasProperty(p1,p2)) return SVGTransformable::get(p1,p2,p3);
    return Undefined();
}

bool SVGRectElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGRectElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGRectElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGRectElement>(p1,static_cast<SVGRectElement::Private *>(d));
}

Value SVGRectElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGRectElement>(p1,static_cast<SVGRectElement::Private *>(d));
}

bool SVGSVGElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGSVGElement::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGSVGElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(KDOM::DocumentEvent::hasProperty(p1,p2)) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGFitToViewBox::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGLocatable::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGZoomAndPan::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGSVGElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGSVGElementProtoFunc,SVGSVGElement>(p1,p2,&s_hashTable,this,p3);
}

SVGSVGElement SVGSVGElementProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGSVGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSVGElement> * >(p1);
    if(test) return SVGSVGElement(static_cast<SVGSVGElement::Private *>(test->impl())); }
    return SVGSVGElement::null;
}

Value SVGSVGElement::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGSVGElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(KDOM::DocumentEvent::hasProperty(p1,p2)) return KDOM::DocumentEvent::get(p1,p2,p3);
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGFitToViewBox::hasProperty(p1,p2)) return SVGFitToViewBox::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGLocatable::hasProperty(p1,p2)) return SVGLocatable::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGZoomAndPan::hasProperty(p1,p2)) return SVGZoomAndPan::get(p1,p2,p3);
    return Undefined();
}

bool SVGSVGElement::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGSVGElement>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGSVGElement::putInParents(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGZoomAndPan::hasProperty(p1,p2)) {
        SVGZoomAndPan::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGSVGElement::prototype(ExecState *p1) const
{
    if(p1) return SVGSVGElementProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGSVGElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGSVGElement>(p1,static_cast<SVGSVGElement::Private *>(KDOM::EventTarget::d));
}

Value SVGSVGElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGSVGElement>(p1,static_cast<SVGSVGElement::Private *>(KDOM::EventTarget::d));
}

bool SVGScriptElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGScriptElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGURIReference::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGScriptElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGScriptElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGScriptElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGURIReference::hasProperty(p1,p2)) return SVGURIReference::get(p1,p2,p3);
    return Undefined();
}

bool SVGScriptElement::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGScriptElement>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGScriptElement::putInParents(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGScriptElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGScriptElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGScriptElement>(p1,static_cast<SVGScriptElement::Private *>(d));
}

Value SVGScriptElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGScriptElement>(p1,static_cast<SVGScriptElement::Private *>(d));
}

bool SVGSetElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGSetElement::s_hashTable,p2);
    if(e) return true;
    if(SVGAnimationElement::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGSetElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGSetElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGSetElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGAnimationElement::hasProperty(p1,p2)) return SVGAnimationElement::get(p1,p2,p3);
    return Undefined();
}

bool SVGSetElement::put(PUT_METHOD_ARGS)
{
    if(SVGAnimationElement::hasProperty(p1,p2)) {
        SVGAnimationElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGSetElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGSetElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGSetElement>(p1,static_cast<SVGSetElement::Private *>(d));
}

Value SVGSetElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGSetElement>(p1,static_cast<SVGSetElement::Private *>(d));
}

bool SVGStopElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGStopElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGStopElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGStopElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGStopElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    return Undefined();
}

bool SVGStopElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGStopElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGStopElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGStopElement>(p1,static_cast<SVGStopElement::Private *>(d));
}

Value SVGStopElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGStopElement>(p1,static_cast<SVGStopElement::Private *>(d));
}

bool SVGStringList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGStringList::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGStringListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value SVGStringList::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGStringListProtoFunc,SVGStringList>(p1,p2,&s_hashTable,this,p3);
}

SVGStringList SVGStringListProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGStringList> *test = dynamic_cast<const KDOM::DOMBridge<SVGStringList> * >(p1);
    if(test) return SVGStringList(static_cast<SVGStringList::Private *>(test->impl())); }
    return SVGStringList::null;
}

Value SVGStringList::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGStringListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object SVGStringList::prototype(ExecState *p1) const
{
    if(p1) return SVGStringListProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGStringList::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGStringList>(p1,static_cast<SVGStringList::Private *>(impl));
}

Value SVGStringList::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGStringList>(p1,static_cast<SVGStringList::Private *>(impl));
}

bool SVGStylable::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGStylable::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGStylableProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value SVGStylable::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGStylableProtoFunc,SVGStylable>(p1,p2,&s_hashTable,this,p3);
}

SVGStylable SVGStylableProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGStylable> *test = dynamic_cast<const KDOM::DOMBridge<SVGStylable> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGAElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGCircleElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGCircleElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGClipPathElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGClipPathElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGDefsElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGDefsElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGDescElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGDescElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGEllipseElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGEllipseElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEBlendElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEBlendElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEColorMatrixElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEColorMatrixElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEComponentTransferElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEComponentTransferElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFECompositeElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFECompositeElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEFloodElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFloodElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEGaussianBlurElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEGaussianBlurElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEImageElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEImageElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEMergeElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEMergeElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFEOffsetElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEOffsetElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFETileElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFETileElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFETurbulenceElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFETurbulenceElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFilterElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFilterElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGFilterPrimitiveStandardAttributes> *test = dynamic_cast<const KDOM::DOMBridge<SVGFilterPrimitiveStandardAttributes> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGGElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGGradientElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGGradientElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGImageElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGImageElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGLineElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGLineElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGLinearGradientElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGLinearGradientElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGMarkerElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGMarkerElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPatternElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPatternElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPolygonElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPolygonElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPolylineElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPolylineElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGRadialGradientElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGRadialGradientElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGRectElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGRectElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGSVGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSVGElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGStopElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGStopElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGSwitchElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSwitchElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGSymbolElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSymbolElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTSpanElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTSpanElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTextContentElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextContentElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTextElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTextPositioningElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextPositioningElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTitleElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTitleElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGUseElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGUseElement> * >(p1);
    if(test) return SVGStylable(static_cast<SVGStylable::Private *>(test->impl())); }
    return SVGStylable::null;
}

Value SVGStylable::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGStylableProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object SVGStylable::prototype(ExecState *p1) const
{
    if(p1) return SVGStylableProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGStylable::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGStylable>(p1,static_cast<SVGStylable::Private *>(impl));
}

Value SVGStylable::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGStylable>(p1,static_cast<SVGStylable::Private *>(impl));
}

bool SVGStyleElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGStyleElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGStyleElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGStyleElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGStyleElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    return Undefined();
}

bool SVGStyleElement::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGStyleElement>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGStyleElement::putInParents(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGStyleElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGStyleElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGStyleElement>(p1,static_cast<SVGStyleElement::Private *>(d));
}

Value SVGStyleElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGStyleElement>(p1,static_cast<SVGStyleElement::Private *>(d));
}

bool SVGSwitchElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGSwitchElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGTransformable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGSwitchElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGSwitchElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGSwitchElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGTransformable::hasProperty(p1,p2)) return SVGTransformable::get(p1,p2,p3);
    return Undefined();
}

bool SVGSwitchElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGSwitchElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGSwitchElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGSwitchElement>(p1,static_cast<SVGSwitchElement::Private *>(d));
}

Value SVGSwitchElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGSwitchElement>(p1,static_cast<SVGSwitchElement::Private *>(d));
}

bool SVGSymbolElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGSymbolElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGFitToViewBox::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGSymbolElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGSymbolElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGSymbolElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGFitToViewBox::hasProperty(p1,p2)) return SVGFitToViewBox::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    return Undefined();
}

bool SVGSymbolElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGSymbolElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGSymbolElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGSymbolElement>(p1,static_cast<SVGSymbolElement::Private *>(d));
}

Value SVGSymbolElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGSymbolElement>(p1,static_cast<SVGSymbolElement::Private *>(d));
}

bool SVGTSpanElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGTSpanElement::s_hashTable,p2);
    if(e) return true;
    if(SVGTextPositioningElement::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGTSpanElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGTSpanElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGTSpanElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGTextPositioningElement::hasProperty(p1,p2)) return SVGTextPositioningElement::get(p1,p2,p3);
    return Undefined();
}

bool SVGTSpanElement::put(PUT_METHOD_ARGS)
{
    if(SVGTextPositioningElement::hasProperty(p1,p2)) {
        SVGTextPositioningElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGTSpanElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGTSpanElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGTSpanElement>(p1,static_cast<SVGTSpanElement::Private *>(d));
}

Value SVGTSpanElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGTSpanElement>(p1,static_cast<SVGTSpanElement::Private *>(d));
}

bool SVGTests::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGTests::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGTestsProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value SVGTests::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGTestsProtoFunc,SVGTests>(p1,p2,&s_hashTable,this,p3);
}

SVGTests SVGTestsProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGTests> *test = dynamic_cast<const KDOM::DOMBridge<SVGTests> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGAElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGAnimateColorElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimateColorElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGAnimateElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimateElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGAnimateTransformElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimateTransformElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGAnimationElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimationElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGCircleElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGCircleElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGClipPathElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGClipPathElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGDefsElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGDefsElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGEllipseElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGEllipseElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGGElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGImageElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGImageElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGLineElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGLineElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPathElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPatternElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPatternElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPolygonElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPolygonElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGPolylineElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPolylineElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGRectElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGRectElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGSVGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSVGElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGSetElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSetElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGSwitchElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSwitchElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTSpanElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTSpanElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTextContentElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextContentElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTextElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTextPositioningElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextPositioningElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGUseElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGUseElement> * >(p1);
    if(test) return SVGTests(static_cast<SVGTests::Private *>(test->impl())); }
    return SVGTests::null;
}

Value SVGTests::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGTestsProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object SVGTests::prototype(ExecState *p1) const
{
    if(p1) return SVGTestsProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGTests::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGTests>(p1,static_cast<SVGTests::Private *>(impl));
}

Value SVGTests::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGTests>(p1,static_cast<SVGTests::Private *>(impl));
}

bool SVGTextContentElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGTextContentElement::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGTextContentElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGTextContentElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGTextContentElementProtoFunc,SVGTextContentElement>(p1,p2,&s_hashTable,this,p3);
}

SVGTextContentElement SVGTextContentElementProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGTextContentElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextContentElement> * >(p1);
    if(test) return SVGTextContentElement(static_cast<SVGTextContentElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTSpanElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTSpanElement> * >(p1);
    if(test) return SVGTextContentElement(static_cast<SVGTextContentElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTextElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextElement> * >(p1);
    if(test) return SVGTextContentElement(static_cast<SVGTextContentElement::Private *>(test->impl())); }
    { const KDOM::DOMBridge<SVGTextPositioningElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextPositioningElement> * >(p1);
    if(test) return SVGTextContentElement(static_cast<SVGTextContentElement::Private *>(test->impl())); }
    return SVGTextContentElement::null;
}

Value SVGTextContentElement::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGTextContentElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    return Undefined();
}

bool SVGTextContentElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGTextContentElement::prototype(ExecState *p1) const
{
    if(p1) return SVGTextContentElementProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGTextContentElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGTextContentElement>(p1,static_cast<SVGTextContentElement::Private *>(d));
}

Value SVGTextContentElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGTextContentElement>(p1,static_cast<SVGTextContentElement::Private *>(d));
}

bool SVGTextElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGTextElement::s_hashTable,p2);
    if(e) return true;
    if(SVGTextPositioningElement::hasProperty(p1,p2)) return true;
    if(SVGTransformable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGTextElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGTextElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGTextElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGTextPositioningElement::hasProperty(p1,p2)) return SVGTextPositioningElement::get(p1,p2,p3);
    if(SVGTransformable::hasProperty(p1,p2)) return SVGTransformable::get(p1,p2,p3);
    return Undefined();
}

bool SVGTextElement::put(PUT_METHOD_ARGS)
{
    if(SVGTextPositioningElement::hasProperty(p1,p2)) {
        SVGTextPositioningElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGTextElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGTextElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGTextElement>(p1,static_cast<SVGTextElement::Private *>(d));
}

Value SVGTextElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGTextElement>(p1,static_cast<SVGTextElement::Private *>(d));
}

bool SVGTextPositioningElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGTextPositioningElement::s_hashTable,p2);
    if(e) return true;
    if(SVGTextContentElement::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGTextPositioningElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGTextPositioningElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGTextPositioningElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGTextContentElement::hasProperty(p1,p2)) return SVGTextContentElement::get(p1,p2,p3);
    return Undefined();
}

bool SVGTextPositioningElement::put(PUT_METHOD_ARGS)
{
    if(SVGTextContentElement::hasProperty(p1,p2)) {
        SVGTextContentElement::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGTextPositioningElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGTextPositioningElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGTextPositioningElement>(p1,static_cast<SVGTextPositioningElement::Private *>(d));
}

Value SVGTextPositioningElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGTextPositioningElement>(p1,static_cast<SVGTextPositioningElement::Private *>(d));
}

bool SVGTitleElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGTitleElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGTitleElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGTitleElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGTitleElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    return Undefined();
}

bool SVGTitleElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGTitleElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGTitleElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGTitleElement>(p1,static_cast<SVGTitleElement::Private *>(d));
}

Value SVGTitleElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGTitleElement>(p1,static_cast<SVGTitleElement::Private *>(d));
}

bool SVGTransform::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGTransform::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGTransformProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value SVGTransform::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGTransformProtoFunc,SVGTransform>(p1,p2,&s_hashTable,this,p3);
}

SVGTransform SVGTransformProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    return KSVG::toSVGTransform(exec, p1);
}

SVGTransform KSVG::toSVGTransform(KJS::ExecState *, const ObjectImp *p1)
{
    { const KDOM::DOMBridge<SVGTransform> *test = dynamic_cast<const KDOM::DOMBridge<SVGTransform> * >(p1);
    if(test) return SVGTransform(static_cast<SVGTransform::Private *>(test->impl())); }
    return SVGTransform::null;
}

Value SVGTransform::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGTransformProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object SVGTransform::prototype(ExecState *p1) const
{
    if(p1) return SVGTransformProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGTransform::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGTransform>(p1,static_cast<SVGTransform::Private *>(impl));
}

Value SVGTransform::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGTransform>(p1,static_cast<SVGTransform::Private *>(impl));
}

bool SVGTransformList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGTransformList::s_hashTable,p2);
    if(e) return true;
    Object proto = SVGTransformListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value SVGTransformList::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGet<SVGTransformListProtoFunc,SVGTransformList>(p1,p2,&s_hashTable,this,p3);
}

SVGTransformList SVGTransformListProtoFunc::cast(KJS::ExecState *, const ObjectImp *p1) const
{
    { const KDOM::DOMBridge<SVGTransformList> *test = dynamic_cast<const KDOM::DOMBridge<SVGTransformList> * >(p1);
    if(test) return SVGTransformList(static_cast<SVGTransformList::Private *>(test->impl())); }
    return SVGTransformList::null;
}

Value SVGTransformList::getInParents(GET_METHOD_ARGS) const
{
    Object proto = SVGTransformListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object SVGTransformList::prototype(ExecState *p1) const
{
    if(p1) return SVGTransformListProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *SVGTransformList::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGTransformList>(p1,static_cast<SVGTransformList::Private *>(impl));
}

Value SVGTransformList::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGTransformList>(p1,static_cast<SVGTransformList::Private *>(impl));
}

bool SVGTransformable::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGTransformable::s_hashTable,p2);
    if(e) return true;
    if(SVGLocatable::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGTransformable::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGTransformable>(p1,p2,&s_hashTable,this,p3);
}

Value SVGTransformable::getInParents(GET_METHOD_ARGS) const
{
    if(SVGLocatable::hasProperty(p1,p2)) return SVGLocatable::get(p1,p2,p3);
    return Undefined();
}

Object SVGTransformable::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGTransformable::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGTransformable>(p1,static_cast<SVGTransformable::Private *>(impl));
}

Value SVGTransformable::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGTransformable>(p1,static_cast<SVGTransformable::Private *>(impl));
}

bool SVGURIReference::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGURIReference::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGURIReference::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGURIReference>(p1,p2,&s_hashTable,this,p3);
}

Value SVGURIReference::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object SVGURIReference::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGURIReference::bridge(ExecState *p1) const
{
    return new KDOM::DOMBridge<SVGURIReference>(p1,static_cast<SVGURIReference::Private *>(impl));
}

Value SVGURIReference::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGURIReference>(p1,static_cast<SVGURIReference::Private *>(impl));
}

bool SVGUseElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGUseElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGLangSpace::hasProperty(p1,p2)) return true;
    if(SVGStylable::hasProperty(p1,p2)) return true;
    if(SVGTests::hasProperty(p1,p2)) return true;
    if(SVGTransformable::hasProperty(p1,p2)) return true;
    if(SVGURIReference::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGUseElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGUseElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGUseElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGLangSpace::hasProperty(p1,p2)) return SVGLangSpace::get(p1,p2,p3);
    if(SVGStylable::hasProperty(p1,p2)) return SVGStylable::get(p1,p2,p3);
    if(SVGTests::hasProperty(p1,p2)) return SVGTests::get(p1,p2,p3);
    if(SVGTransformable::hasProperty(p1,p2)) return SVGTransformable::get(p1,p2,p3);
    if(SVGURIReference::hasProperty(p1,p2)) return SVGURIReference::get(p1,p2,p3);
    return Undefined();
}

bool SVGUseElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGLangSpace::hasProperty(p1,p2)) {
        SVGLangSpace::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGUseElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGUseElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGUseElement>(p1,static_cast<SVGUseElement::Private *>(d));
}

Value SVGUseElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGUseElement>(p1,static_cast<SVGUseElement::Private *>(d));
}

bool SVGViewElement::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGViewElement::s_hashTable,p2);
    if(e) return true;
    if(SVGElement::hasProperty(p1,p2)) return true;
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return true;
    if(SVGFitToViewBox::hasProperty(p1,p2)) return true;
    if(SVGZoomAndPan::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGViewElement::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGViewElement>(p1,p2,&s_hashTable,this,p3);
}

Value SVGViewElement::getInParents(GET_METHOD_ARGS) const
{
    if(SVGElement::hasProperty(p1,p2)) return SVGElement::get(p1,p2,p3);
    if(SVGExternalResourcesRequired::hasProperty(p1,p2)) return SVGExternalResourcesRequired::get(p1,p2,p3);
    if(SVGFitToViewBox::hasProperty(p1,p2)) return SVGFitToViewBox::get(p1,p2,p3);
    if(SVGZoomAndPan::hasProperty(p1,p2)) return SVGZoomAndPan::get(p1,p2,p3);
    return Undefined();
}

bool SVGViewElement::put(PUT_METHOD_ARGS)
{
    if(SVGElement::hasProperty(p1,p2)) {
        SVGElement::put(p1,p2,p3,p4);
        return true;
    }
    if(SVGZoomAndPan::hasProperty(p1,p2)) {
        SVGZoomAndPan::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object SVGViewElement::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGViewElement::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGViewElement>(p1,static_cast<SVGViewElement::Private *>(d));
}

Value SVGViewElement::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGViewElement>(p1,static_cast<SVGViewElement::Private *>(d));
}

bool SVGZoomAndPan::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGZoomAndPan::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value SVGZoomAndPan::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGZoomAndPan>(p1,p2,&s_hashTable,this,p3);
}

Value SVGZoomAndPan::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

bool SVGZoomAndPan::put(PUT_METHOD_ARGS)
{
    return KDOM::lookupPut<SVGZoomAndPan>(p1,p2,p3,p4,&s_hashTable,this);
}

bool SVGZoomAndPan::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGZoomAndPan::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *SVGZoomAndPan::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGZoomAndPan>(p1,static_cast<SVGZoomAndPan::Private *>(impl));
}

Value SVGZoomAndPan::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGZoomAndPan>(p1,static_cast<SVGZoomAndPan::Private *>(impl));
}

bool SVGZoomEvent::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&SVGZoomEvent::s_hashTable,p2);
    if(e) return true;
    if(KDOM::UIEvent::hasProperty(p1,p2)) return true;
    return false;
}

Value SVGZoomEvent::get(GET_METHOD_ARGS) const
{
    return KDOM::lookupGetValue<SVGZoomEvent>(p1,p2,&s_hashTable,this,p3);
}

Value SVGZoomEvent::getInParents(GET_METHOD_ARGS) const
{
    if(KDOM::UIEvent::hasProperty(p1,p2)) return KDOM::UIEvent::get(p1,p2,p3);
    return Undefined();
}

bool SVGZoomEvent::put(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object SVGZoomEvent::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

KDOM::UIEvent KSVG::EcmaInterface::inheritedUIEventCast(const ObjectImp *p1)
{
	{ const KDOM::DOMBridge<KSVG::SVGZoomEvent> *test = dynamic_cast<const KDOM::DOMBridge<KSVG::SVGZoomEvent> * >(p1);
 	  if(test) return SVGZoomEvent(static_cast<SVGZoomEvent::Private *>(test->impl())); }
	{ KDOM::UIEventImpl *test = dynamic_cast<KDOM::UIEventImpl *>(inheritedEventCast(p1).d);
 	  if(test) return KDOM::UIEvent(test); }
    return SVGZoomEvent::null;
}

ObjectImp *SVGZoomEvent::bridge(ExecState *p1) const
{
    return new KDOM::DOMRWBridge<SVGZoomEvent>(p1,static_cast<SVGZoomEvent::Private *>(d));
}

Value SVGZoomEvent::cache(ExecState *p1) const
{
    return KDOM::cacheDOMObject<SVGZoomEvent>(p1,static_cast<SVGZoomEvent::Private *>(d));
}

KDOM::DocumentEvent KSVG::EcmaInterface::inheritedDocumentEventCast(const ObjectImp *p1)
{
    { const KDOM::DOMBridge<SVGDocument> *test = dynamic_cast<const KDOM::DOMBridge<SVGDocument> * >(p1);
    if(test) return SVGDocument(test->impl()); }
    { const KDOM::DOMBridge<SVGSVGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSVGElement> * >(p1);
    if(test) return SVGSVGElement(test->impl()); }
    return KDOM::DocumentEvent::null;
}

KDOM::DocumentRange KSVG::EcmaInterface::inheritedDocumentRangeCast(const ObjectImp *p1)
{
    { const KDOM::DOMBridge<SVGDocument> *test = dynamic_cast<const KDOM::DOMBridge<SVGDocument> * >(p1);
    if(test) return SVGDocument(test->impl()); }
    return KDOM::DocumentRange::null;
}

KDOM::DocumentTraversal KSVG::EcmaInterface::inheritedDocumentTraversalCast(const ObjectImp *p1)
{
    { const KDOM::DOMBridge<SVGDocument> *test = dynamic_cast<const KDOM::DOMBridge<SVGDocument> * >(p1);
    if(test) return SVGDocument(test->impl()); }
    return KDOM::DocumentTraversal::null;
}

KDOM::Event KSVG::EcmaInterface::inheritedEventCast(const ObjectImp *p1)
{
    { const KDOM::DOMBridge<SVGEvent> *test = dynamic_cast<const KDOM::DOMBridge<SVGEvent> * >(p1);
    if(test) return SVGEvent(test->impl()); }
    { const KDOM::DOMBridge<SVGZoomEvent> *test = dynamic_cast<const KDOM::DOMBridge<SVGZoomEvent> * >(p1);
    if(test) return SVGZoomEvent(test->impl()); }
    return KDOM::Event::null;
}

KDOM::EventTarget KSVG::EcmaInterface::inheritedEventTargetCast(const ObjectImp *p1)
{
    { const KDOM::DOMBridge<SVGAElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAElement> * >(p1);
    if(test) return SVGAElement(test->impl()); }
    { const KDOM::DOMBridge<SVGAnimateColorElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimateColorElement> * >(p1);
    if(test) return SVGAnimateColorElement(test->impl()); }
    { const KDOM::DOMBridge<SVGAnimateElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimateElement> * >(p1);
    if(test) return SVGAnimateElement(test->impl()); }
    { const KDOM::DOMBridge<SVGAnimateTransformElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimateTransformElement> * >(p1);
    if(test) return SVGAnimateTransformElement(test->impl()); }
    { const KDOM::DOMBridge<SVGAnimationElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimationElement> * >(p1);
    if(test) return SVGAnimationElement(test->impl()); }
    { const KDOM::DOMBridge<SVGCircleElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGCircleElement> * >(p1);
    if(test) return SVGCircleElement(test->impl()); }
    { const KDOM::DOMBridge<SVGClipPathElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGClipPathElement> * >(p1);
    if(test) return SVGClipPathElement(test->impl()); }
    { const KDOM::DOMBridge<SVGComponentTransferFunctionElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGComponentTransferFunctionElement> * >(p1);
    if(test) return SVGComponentTransferFunctionElement(test->impl()); }
    { const KDOM::DOMBridge<SVGDefsElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGDefsElement> * >(p1);
    if(test) return SVGDefsElement(test->impl()); }
    { const KDOM::DOMBridge<SVGDescElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGDescElement> * >(p1);
    if(test) return SVGDescElement(test->impl()); }
    { const KDOM::DOMBridge<SVGDocument> *test = dynamic_cast<const KDOM::DOMBridge<SVGDocument> * >(p1);
    if(test) return SVGDocument(test->impl()); }
    { const KDOM::DOMBridge<SVGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGElement> * >(p1);
    if(test) return SVGElement(test->impl()); }
    { const KDOM::DOMBridge<SVGElementInstance> *test = dynamic_cast<const KDOM::DOMBridge<SVGElementInstance> * >(p1);
    if(test) return SVGElementInstance(test->impl()); }
    { const KDOM::DOMBridge<SVGEllipseElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGEllipseElement> * >(p1);
    if(test) return SVGEllipseElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEBlendElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEBlendElement> * >(p1);
    if(test) return SVGFEBlendElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEColorMatrixElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEColorMatrixElement> * >(p1);
    if(test) return SVGFEColorMatrixElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEComponentTransferElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEComponentTransferElement> * >(p1);
    if(test) return SVGFEComponentTransferElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFECompositeElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFECompositeElement> * >(p1);
    if(test) return SVGFECompositeElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEFloodElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFloodElement> * >(p1);
    if(test) return SVGFEFloodElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEFuncAElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFuncAElement> * >(p1);
    if(test) return SVGFEFuncAElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEFuncBElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFuncBElement> * >(p1);
    if(test) return SVGFEFuncBElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEFuncGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFuncGElement> * >(p1);
    if(test) return SVGFEFuncGElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEFuncRElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFuncRElement> * >(p1);
    if(test) return SVGFEFuncRElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEGaussianBlurElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEGaussianBlurElement> * >(p1);
    if(test) return SVGFEGaussianBlurElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEImageElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEImageElement> * >(p1);
    if(test) return SVGFEImageElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEMergeElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEMergeElement> * >(p1);
    if(test) return SVGFEMergeElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEMergeNodeElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEMergeNodeElement> * >(p1);
    if(test) return SVGFEMergeNodeElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEOffsetElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEOffsetElement> * >(p1);
    if(test) return SVGFEOffsetElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFETileElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFETileElement> * >(p1);
    if(test) return SVGFETileElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFETurbulenceElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFETurbulenceElement> * >(p1);
    if(test) return SVGFETurbulenceElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFilterElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFilterElement> * >(p1);
    if(test) return SVGFilterElement(test->impl()); }
    { const KDOM::DOMBridge<SVGGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGGElement> * >(p1);
    if(test) return SVGGElement(test->impl()); }
    { const KDOM::DOMBridge<SVGGradientElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGGradientElement> * >(p1);
    if(test) return SVGGradientElement(test->impl()); }
    { const KDOM::DOMBridge<SVGImageElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGImageElement> * >(p1);
    if(test) return SVGImageElement(test->impl()); }
    { const KDOM::DOMBridge<SVGLineElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGLineElement> * >(p1);
    if(test) return SVGLineElement(test->impl()); }
    { const KDOM::DOMBridge<SVGLinearGradientElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGLinearGradientElement> * >(p1);
    if(test) return SVGLinearGradientElement(test->impl()); }
    { const KDOM::DOMBridge<SVGMarkerElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGMarkerElement> * >(p1);
    if(test) return SVGMarkerElement(test->impl()); }
    { const KDOM::DOMBridge<SVGPathElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathElement> * >(p1);
    if(test) return SVGPathElement(test->impl()); }
    { const KDOM::DOMBridge<SVGPatternElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPatternElement> * >(p1);
    if(test) return SVGPatternElement(test->impl()); }
    { const KDOM::DOMBridge<SVGPolygonElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPolygonElement> * >(p1);
    if(test) return SVGPolygonElement(test->impl()); }
    { const KDOM::DOMBridge<SVGPolylineElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPolylineElement> * >(p1);
    if(test) return SVGPolylineElement(test->impl()); }
    { const KDOM::DOMBridge<SVGRadialGradientElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGRadialGradientElement> * >(p1);
    if(test) return SVGRadialGradientElement(test->impl()); }
    { const KDOM::DOMBridge<SVGRectElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGRectElement> * >(p1);
    if(test) return SVGRectElement(test->impl()); }
    { const KDOM::DOMBridge<SVGSVGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSVGElement> * >(p1);
    if(test) return SVGSVGElement(test->impl()); }
    { const KDOM::DOMBridge<SVGScriptElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGScriptElement> * >(p1);
    if(test) return SVGScriptElement(test->impl()); }
    { const KDOM::DOMBridge<SVGSetElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSetElement> * >(p1);
    if(test) return SVGSetElement(test->impl()); }
    { const KDOM::DOMBridge<SVGStopElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGStopElement> * >(p1);
    if(test) return SVGStopElement(test->impl()); }
    { const KDOM::DOMBridge<SVGStyleElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGStyleElement> * >(p1);
    if(test) return SVGStyleElement(test->impl()); }
    { const KDOM::DOMBridge<SVGSwitchElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSwitchElement> * >(p1);
    if(test) return SVGSwitchElement(test->impl()); }
    { const KDOM::DOMBridge<SVGSymbolElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSymbolElement> * >(p1);
    if(test) return SVGSymbolElement(test->impl()); }
    { const KDOM::DOMBridge<SVGTSpanElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTSpanElement> * >(p1);
    if(test) return SVGTSpanElement(test->impl()); }
    { const KDOM::DOMBridge<SVGTextContentElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextContentElement> * >(p1);
    if(test) return SVGTextContentElement(test->impl()); }
    { const KDOM::DOMBridge<SVGTextElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextElement> * >(p1);
    if(test) return SVGTextElement(test->impl()); }
    { const KDOM::DOMBridge<SVGTextPositioningElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextPositioningElement> * >(p1);
    if(test) return SVGTextPositioningElement(test->impl()); }
    { const KDOM::DOMBridge<SVGTitleElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTitleElement> * >(p1);
    if(test) return SVGTitleElement(test->impl()); }
    { const KDOM::DOMBridge<SVGUseElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGUseElement> * >(p1);
    if(test) return SVGUseElement(test->impl()); }
    { const KDOM::DOMBridge<SVGViewElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGViewElement> * >(p1);
    if(test) return SVGViewElement(test->impl()); }
    return KDOM::EventTarget::null;
}

KDOM::Node KSVG::EcmaInterface::inheritedNodeCast(const ObjectImp *p1)
{
    { const KDOM::DOMBridge<SVGAElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAElement> * >(p1);
    if(test) return SVGAElement(test->impl()); }
    { const KDOM::DOMBridge<SVGAnimateColorElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimateColorElement> * >(p1);
    if(test) return SVGAnimateColorElement(test->impl()); }
    { const KDOM::DOMBridge<SVGAnimateElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimateElement> * >(p1);
    if(test) return SVGAnimateElement(test->impl()); }
    { const KDOM::DOMBridge<SVGAnimateTransformElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimateTransformElement> * >(p1);
    if(test) return SVGAnimateTransformElement(test->impl()); }
    { const KDOM::DOMBridge<SVGAnimationElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGAnimationElement> * >(p1);
    if(test) return SVGAnimationElement(test->impl()); }
    { const KDOM::DOMBridge<SVGCircleElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGCircleElement> * >(p1);
    if(test) return SVGCircleElement(test->impl()); }
    { const KDOM::DOMBridge<SVGClipPathElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGClipPathElement> * >(p1);
    if(test) return SVGClipPathElement(test->impl()); }
    { const KDOM::DOMBridge<SVGComponentTransferFunctionElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGComponentTransferFunctionElement> * >(p1);
    if(test) return SVGComponentTransferFunctionElement(test->impl()); }
    { const KDOM::DOMBridge<SVGDefsElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGDefsElement> * >(p1);
    if(test) return SVGDefsElement(test->impl()); }
    { const KDOM::DOMBridge<SVGDescElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGDescElement> * >(p1);
    if(test) return SVGDescElement(test->impl()); }
    { const KDOM::DOMBridge<SVGDocument> *test = dynamic_cast<const KDOM::DOMBridge<SVGDocument> * >(p1);
    if(test) return SVGDocument(test->impl()); }
    { const KDOM::DOMBridge<SVGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGElement> * >(p1);
    if(test) return SVGElement(test->impl()); }
    { const KDOM::DOMBridge<SVGEllipseElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGEllipseElement> * >(p1);
    if(test) return SVGEllipseElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEBlendElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEBlendElement> * >(p1);
    if(test) return SVGFEBlendElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEColorMatrixElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEColorMatrixElement> * >(p1);
    if(test) return SVGFEColorMatrixElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEComponentTransferElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEComponentTransferElement> * >(p1);
    if(test) return SVGFEComponentTransferElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFECompositeElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFECompositeElement> * >(p1);
    if(test) return SVGFECompositeElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEFloodElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFloodElement> * >(p1);
    if(test) return SVGFEFloodElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEFuncAElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFuncAElement> * >(p1);
    if(test) return SVGFEFuncAElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEFuncBElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFuncBElement> * >(p1);
    if(test) return SVGFEFuncBElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEFuncGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFuncGElement> * >(p1);
    if(test) return SVGFEFuncGElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEFuncRElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEFuncRElement> * >(p1);
    if(test) return SVGFEFuncRElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEGaussianBlurElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEGaussianBlurElement> * >(p1);
    if(test) return SVGFEGaussianBlurElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEImageElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEImageElement> * >(p1);
    if(test) return SVGFEImageElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEMergeElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEMergeElement> * >(p1);
    if(test) return SVGFEMergeElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEMergeNodeElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEMergeNodeElement> * >(p1);
    if(test) return SVGFEMergeNodeElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFEOffsetElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFEOffsetElement> * >(p1);
    if(test) return SVGFEOffsetElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFETileElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFETileElement> * >(p1);
    if(test) return SVGFETileElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFETurbulenceElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFETurbulenceElement> * >(p1);
    if(test) return SVGFETurbulenceElement(test->impl()); }
    { const KDOM::DOMBridge<SVGFilterElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGFilterElement> * >(p1);
    if(test) return SVGFilterElement(test->impl()); }
    { const KDOM::DOMBridge<SVGGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGGElement> * >(p1);
    if(test) return SVGGElement(test->impl()); }
    { const KDOM::DOMBridge<SVGGradientElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGGradientElement> * >(p1);
    if(test) return SVGGradientElement(test->impl()); }
    { const KDOM::DOMBridge<SVGImageElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGImageElement> * >(p1);
    if(test) return SVGImageElement(test->impl()); }
    { const KDOM::DOMBridge<SVGLineElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGLineElement> * >(p1);
    if(test) return SVGLineElement(test->impl()); }
    { const KDOM::DOMBridge<SVGLinearGradientElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGLinearGradientElement> * >(p1);
    if(test) return SVGLinearGradientElement(test->impl()); }
    { const KDOM::DOMBridge<SVGMarkerElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGMarkerElement> * >(p1);
    if(test) return SVGMarkerElement(test->impl()); }
    { const KDOM::DOMBridge<SVGPathElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPathElement> * >(p1);
    if(test) return SVGPathElement(test->impl()); }
    { const KDOM::DOMBridge<SVGPatternElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPatternElement> * >(p1);
    if(test) return SVGPatternElement(test->impl()); }
    { const KDOM::DOMBridge<SVGPolygonElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPolygonElement> * >(p1);
    if(test) return SVGPolygonElement(test->impl()); }
    { const KDOM::DOMBridge<SVGPolylineElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGPolylineElement> * >(p1);
    if(test) return SVGPolylineElement(test->impl()); }
    { const KDOM::DOMBridge<SVGRadialGradientElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGRadialGradientElement> * >(p1);
    if(test) return SVGRadialGradientElement(test->impl()); }
    { const KDOM::DOMBridge<SVGRectElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGRectElement> * >(p1);
    if(test) return SVGRectElement(test->impl()); }
    { const KDOM::DOMBridge<SVGSVGElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSVGElement> * >(p1);
    if(test) return SVGSVGElement(test->impl()); }
    { const KDOM::DOMBridge<SVGScriptElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGScriptElement> * >(p1);
    if(test) return SVGScriptElement(test->impl()); }
    { const KDOM::DOMBridge<SVGSetElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSetElement> * >(p1);
    if(test) return SVGSetElement(test->impl()); }
    { const KDOM::DOMBridge<SVGStopElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGStopElement> * >(p1);
    if(test) return SVGStopElement(test->impl()); }
    { const KDOM::DOMBridge<SVGStyleElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGStyleElement> * >(p1);
    if(test) return SVGStyleElement(test->impl()); }
    { const KDOM::DOMBridge<SVGSwitchElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSwitchElement> * >(p1);
    if(test) return SVGSwitchElement(test->impl()); }
    { const KDOM::DOMBridge<SVGSymbolElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGSymbolElement> * >(p1);
    if(test) return SVGSymbolElement(test->impl()); }
    { const KDOM::DOMBridge<SVGTSpanElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTSpanElement> * >(p1);
    if(test) return SVGTSpanElement(test->impl()); }
    { const KDOM::DOMBridge<SVGTextContentElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextContentElement> * >(p1);
    if(test) return SVGTextContentElement(test->impl()); }
    { const KDOM::DOMBridge<SVGTextElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextElement> * >(p1);
    if(test) return SVGTextElement(test->impl()); }
    { const KDOM::DOMBridge<SVGTextPositioningElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTextPositioningElement> * >(p1);
    if(test) return SVGTextPositioningElement(test->impl()); }
    { const KDOM::DOMBridge<SVGTitleElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGTitleElement> * >(p1);
    if(test) return SVGTitleElement(test->impl()); }
    { const KDOM::DOMBridge<SVGUseElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGUseElement> * >(p1);
    if(test) return SVGUseElement(test->impl()); }
    { const KDOM::DOMBridge<SVGViewElement> *test = dynamic_cast<const KDOM::DOMBridge<SVGViewElement> * >(p1);
    if(test) return SVGViewElement(test->impl()); }
    return KDOM::Node::null;
}

