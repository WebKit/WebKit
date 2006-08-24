
project( WebCore )

# Global switch for SVG support
SET (USE_WEBKIT_SVG_SUPPORT 1)

include (CheckCXXCompilerFlag)

include_directories(
   BEFORE ${CMAKE_CURRENT_BINARY_DIR}
   ${CMAKE_CURRENT_SOURCE_DIR}/ForwardingHeaders
   ${CMAKE_CURRENT_SOURCE_DIR}/platform
   ${CMAKE_CURRENT_SOURCE_DIR}/loader
   ${CMAKE_CURRENT_BINARY_DIR}/css
   ${CMAKE_CURRENT_SOURCE_DIR}/css
   ${CMAKE_CURRENT_SOURCE_DIR}/dom
   ${CMAKE_CURRENT_SOURCE_DIR}/page
   ${CMAKE_CURRENT_SOURCE_DIR}/bridge
   ${CMAKE_CURRENT_SOURCE_DIR}/editing
   ${CMAKE_CURRENT_SOURCE_DIR}/kwq
   ${CMAKE_CURRENT_SOURCE_DIR}/rendering
   ${CMAKE_CURRENT_SOURCE_DIR}/xml
   ${CMAKE_CURRENT_BINARY_DIR}/xml
   ${CMAKE_CURRENT_SOURCE_DIR}/html
   ${CMAKE_CURRENT_SOURCE_DIR}/bindings/js
   ${CMAKE_CURRENT_BINARY_DIR}/html
   ${CMAKE_CURRENT_SOURCE_DIR}/kcanvas
   ${CMAKE_CURRENT_SOURCE_DIR}/kcanvas/device
   ${CMAKE_CURRENT_SOURCE_DIR}/kcanvas/device/qt 
   ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2 
   ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/css
   ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/svg
   ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/svg
   ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/misc 
   ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/misc 
   ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/events
   ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/bindings/js
   ${CMAKE_CURRENT_BINARY_DIR}/platform
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/image-decoders
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/image-decoders/gif
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/image-decoders/jpeg
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/image-decoders/ico
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/image-decoders/xbm
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/image-decoders/bmp
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/image-decoders/png
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/qt
   ${CMAKE_CURRENT_SOURCE_DIR}/WebCore+SVG
   ${CMAKE_CURRENT_SOURCE_DIR}/../JavaScriptCore/kjs
   ${CMAKE_CURRENT_SOURCE_DIR}/../JavaScriptCore/
   ${CMAKE_CURRENT_SOURCE_DIR}/../
   )

include_directories( ${LIBXML2_INCLUDE_DIR} ${QT_INCLUDES}
  # don't. pulls in kjs
  # ${KDE4_KDECORE_INCLUDES}
  )

add_definitions ( -DQT3_SUPPORT -Wno-undef -Wno-unused-parameter )

IF (USE_WEBKIT_SVG_SUPPORT)
add_definitions ( -DSVG_SUPPORT=1 )
ENDIF (USE_WEBKIT_SVG_SUPPORT)

check_cxx_compiler_flag(-Wno-variadic-macros __KDE_HAVE_WVARIADIC_MACROS)

IF(__KDE_HAVE_WVARIADIC_MACROS)
  add_definitions ( -Wno-variadic-macros )
ENDIF(__KDE_HAVE_WVARIADIC_MACROS)

kde4_automoc(platform/qt/ScrollViewCanvasQt.cpp)

# rules for generated files
IF (USE_WEBKIT_SVG_SUPPORT)
set(IDL_BINDINGS_SVG
# ksvg2/svg/SVGNumber.idl
# ksvg2/svg/SVGPoint.idl
# ksvg2/svg/SVGRect.idl

  ksvg2/svg/SVGAngle.idl
  ksvg2/svg/SVGMatrix.idl
  ksvg2/svg/SVGTransform.idl
  ksvg2/svg/SVGLength.idl
  ksvg2/svg/SVGAnimatedLength.idl
  ksvg2/svg/SVGElement.idl
  ksvg2/svg/SVGColor.idl
  ksvg2/svg/SVGDocument.idl
  ksvg2/svg/SVGSVGElement.idl
)
ENDIF (USE_WEBKIT_SVG_SUPPORT)

set(IDL_BINDINGS
  css/CSSValue.idl
  css/CSSRuleList.idl
  css/CSSValueList.idl
  css/CSSStyleDeclaration.idl
  css/CSSPrimitiveValue.idl
  css/CSSRule.idl
  css/Counter.idl
  css/MediaList.idl
  dom/Event.idl
  dom/Range.idl
  dom/Text.idl
  dom/DOMImplementation.idl
  dom/NodeFilter.idl
  dom/MouseEvent.idl
  dom/CharacterData.idl
  dom/DocumentFragment.idl
  dom/Entity.idl
  dom/UIEvent.idl
  dom/Node.idl
  dom/ProcessingInstruction.idl
  dom/Notation.idl
  dom/Element.idl
  dom/DocumentType.idl
  dom/Document.idl
  dom/Attr.idl
  dom/MutationEvent.idl
  dom/KeyboardEvent.idl
  dom/WheelEvent.idl
  dom/OverflowEvent.idl
  dom/NodeIterator.idl
  dom/TreeWalker.idl
  html/CanvasGradient.idl
  html/CanvasPattern.idl
  html/CanvasRenderingContext2D.idl
  html/HTMLAnchorElement.idl
  html/HTMLAppletElement.idl
  html/HTMLAreaElement.idl
  html/HTMLBaseElement.idl
  html/HTMLBaseFontElement.idl
  html/HTMLBlockquoteElement.idl
  html/HTMLBodyElement.idl
  html/HTMLBRElement.idl
  html/HTMLButtonElement.idl
  html/HTMLCanvasElement.idl
  html/HTMLDirectoryElement.idl
  html/HTMLDivElement.idl
  html/HTMLDListElement.idl
  html/HTMLDocument.idl
  html/HTMLElement.idl
  html/HTMLFieldSetElement.idl
  html/HTMLFontElement.idl
  html/HTMLFormElement.idl
  html/HTMLHeadElement.idl
  html/HTMLHeadingElement.idl
  html/HTMLHRElement.idl
  html/HTMLHtmlElement.idl
  html/HTMLImageElement.idl
  html/HTMLInputElement.idl
  html/HTMLIsIndexElement.idl
  html/HTMLLabelElement.idl
  html/HTMLLegendElement.idl
  html/HTMLLIElement.idl
  html/HTMLLinkElement.idl
  html/HTMLMapElement.idl
  html/HTMLMenuElement.idl
  html/HTMLMetaElement.idl
  html/HTMLModElement.idl
  html/HTMLOListElement.idl
  html/HTMLOptGroupElement.idl
  html/HTMLOptionElement.idl
  html/HTMLParagraphElement.idl
  html/HTMLParamElement.idl
  html/HTMLPreElement.idl
  html/HTMLQuoteElement.idl
  html/HTMLScriptElement.idl
  html/HTMLStyleElement.idl
  html/HTMLTextAreaElement.idl
  html/HTMLTitleElement.idl
  html/HTMLUListElement.idl
  html/HTMLOptionsCollection.idl

  xml/XPathNSResolver.idl
  xml/XPathExpression.idl
  xml/XPathResult.idl
  xml/XPathEvaluator.idl
 
  xml/DOMParser.idl
  xml/XMLSerializer.idl

  page/DOMWindow.idl

  ${IDL_BINDINGS_SVG}
)

set(WebCore-unity-IDLGeneratedSources )

# create these files at build time on demand, not everytime at cmake-time, Alex
foreach (idl ${IDL_BINDINGS})
   get_filename_component(_basename ${idl} NAME_WE)
   add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/JS${_basename}.cpp ${CMAKE_CURRENT_BINARY_DIR}/JS${_basename}.h
      COMMAND perl -I${CMAKE_CURRENT_SOURCE_DIR}/bindings/scripts ${CMAKE_CURRENT_SOURCE_DIR}/bindings/scripts/generate-bindings.pl  --defines \"SVG_SUPPORT=${USE_WEBKIT_SVG_SUPPORT}\" --generator JS --include dom --include html --include xml --include ksvg2/svg --outputdir ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${idl}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${idl} )
   set(WebCore-unity-IDLGeneratedSources ${WebCore-unity-IDLGeneratedSources} ${CMAKE_CURRENT_BINARY_DIR}/JS${_basename}.cpp ${CMAKE_CURRENT_BINARY_DIR}/JS${_basename}.h )
endforeach (idl)

set(CREATE_HASH_TABLE ${CMAKE_CURRENT_SOURCE_DIR}/../JavaScriptCore/kjs/create_hash_table )

macro(CREATE_LUT _srcs_LIST _in_FILE _out_FILE _dep_FILE)

   add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_out_FILE}
      COMMAND ${PERL_EXECUTABLE} ${CREATE_HASH_TABLE} ${CMAKE_CURRENT_SOURCE_DIR}/${_in_FILE} -i > ${CMAKE_CURRENT_BINARY_DIR}/${_out_FILE}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_in_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/${_dep_FILE} )
   set( ${_srcs_LIST}  ${${_srcs_LIST}} ${CMAKE_CURRENT_BINARY_DIR}/${_out_FILE})
endmacro(CREATE_LUT)

macro(CREATE_CPP_LUT _in_FILE _out_FILE _dep_FILE)
   add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_out_FILE}
      COMMAND ${PERL_EXECUTABLE} ${CREATE_HASH_TABLE} ${CMAKE_CURRENT_SOURCE_DIR}/${_in_FILE} -i > ${CMAKE_CURRENT_BINARY_DIR}/${_out_FILE}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_in_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/${_dep_FILE} )
endmacro(CREATE_CPP_LUT)

create_lut(WebCore-unity_LIB_SRCS bindings/js/kjs_window.cpp kjs_window.lut.h bindings/js/kjs_window.cpp)
create_lut(WebCore-unity_LIB_SRCS bindings/js/kjs_css.cpp kjs_css.lut.h bindings/js/kjs_css.cpp)
create_lut(WebCore-unity_LIB_SRCS bindings/js/kjs_dom.cpp kjs_dom.lut.h bindings/js/kjs_dom.cpp)
create_lut(WebCore-unity_LIB_SRCS bindings/js/kjs_html.cpp kjs_html.lut.h bindings/js/kjs_html.cpp)
create_lut(WebCore-unity_LIB_SRCS bindings/js/kjs_events.cpp kjs_events.lut.h bindings/js/kjs_events.cpp)
create_lut(WebCore-unity_LIB_SRCS bindings/js/kjs_navigator.cpp kjs_navigator.lut.h bindings/js/kjs_navigator.cpp)
create_lut(WebCore-unity_LIB_SRCS bindings/js/kjs_traversal.cpp kjs_traversal.lut.h bindings/js/kjs_traversal.cpp)
create_lut(WebCore-unity_LIB_SRCS bindings/js/JSXMLHttpRequest.cpp JSXMLHttpRequest.lut.h bindings/js/JSXMLHttpRequest.cpp)
create_lut(WebCore-unity_LIB_SRCS bindings/js/JSXSLTProcessor.cpp JSXSLTProcessor.lut.h bindings/js/JSXSLTProcessor.cpp)
create_cpp_lut(bindings/js/JSHTMLInputElementBase.cpp JSHTMLInputElementBaseTable.cpp bindings/js/JSHTMLInputElementBase.cpp)

IF (USE_WEBKIT_SVG_SUPPORT)
  create_cpp_lut(ksvg2/bindings/js/JSSVGPoint.cpp JSSVGPointTable.cpp ksvg2/bindings/js/JSSVGPoint.cpp)
  create_cpp_lut(ksvg2/bindings/js/JSSVGRect.cpp JSSVGRectTable.cpp ksvg2/bindings/js/JSSVGRect.cpp)
ENDIF (USE_WEBKIT_SVG_SUPPORT)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tokenizer.cpp
  COMMAND flex -t < ${CMAKE_CURRENT_SOURCE_DIR}/css/tokenizer.flex | perl ${CMAKE_CURRENT_SOURCE_DIR}/css/maketokenizer > ${CMAKE_CURRENT_BINARY_DIR}/tokenizer.cpp

  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/css/tokenizer.flex
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/CSSPropertyNames.h ${CMAKE_CURRENT_BINARY_DIR}/CSSPropertyNames.c

  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/css/CSSPropertyNames.in ${CMAKE_CURRENT_BINARY_DIR}/CSSPropertyNames.in
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/css/makeprop

  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/css/CSSPropertyNames.in
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/CSSValueKeywords.h ${CMAKE_CURRENT_BINARY_DIR}/CSSValueKeywords.c

  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/css/CSSValueKeywords.in ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/css/makevalues

  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/css/CSSValueKeywords.in
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/CSSGrammar.cpp ${CMAKE_CURRENT_BINARY_DIR}/CSSGrammar.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BISON_EXECUTABLE} -d -p cssyy ${CMAKE_CURRENT_SOURCE_DIR}/css/CSSGrammar.y
  COMMAND mv CSSGrammar.tab.c ${CMAKE_CURRENT_BINARY_DIR}/CSSGrammar.cpp
  #COMMAND rm -f CSSGrammar.tab.h
  #COMMAND touch CSSGrammar.h
  COMMAND mv CSSGrammar.tab.h ${CMAKE_CURRENT_BINARY_DIR}/CSSGrammar.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/css/CSSGrammar.y
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/html/HTMLNames.cpp ${CMAKE_CURRENT_BINARY_DIR}/html/HTMLNames.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/html
  COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/scripts/make_names.pl --tags ${CMAKE_CURRENT_SOURCE_DIR}/html/HTMLTagNames.in --attrs ${CMAKE_CURRENT_SOURCE_DIR}/html/HTMLAttributeNames.in --namespace HTML --namespacePrefix xhtml --cppNamespace WebCore --namespaceURI "http://www.w3.org/1999/xhtml" --attrsNullNamespace --output html

  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/html/HTMLTagNames.in ${CMAKE_CURRENT_SOURCE_DIR}/html/HTMLAttributeNames.in
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/svg/SVGNames.cpp ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/svg/SVGNames.h ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/svg/SVGElementFactory.cpp ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/svg/SVGElementFactory.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/svg
  COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/scripts/make_names.pl --tags ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/svg/svgtags.in --attrs ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/svg/svgattrs.in --namespace SVG --cppNamespace WebCore --namespaceURI "http://www.w3.org/2000/svg" --factory --attrsNullNamespace --output ksvg2/svg

  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/svg/svgtags.in ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/svg/svgattrs.in
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/misc/XLinkNames.cpp ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/misc/XLinkNames.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/misc
  COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/scripts/make_names.pl --attrs ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/misc/xlinkattrs.in --namespace XLink --cppNamespace WebCore --namespaceURI "http://www.w3.org/1999/xlink" --output ksvg2/misc

  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/misc/xlinkattrs.in
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xml/XMLNames.cpp ${CMAKE_CURRENT_BINARY_DIR}/xml/XMLNames.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/xml
  COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/scripts/make_names.pl --attrs ${CMAKE_CURRENT_SOURCE_DIR}/xml/xmlattrs.in --namespace XML --cppNamespace WebCore --namespaceURI "http://www.w3.org/XML/1998/namespace" --output xml

  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/xml/xmlattrs.in
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/html/HTMLEntityNames.c
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND mkdir -p html
  COMMAND echo '\#include <string.h>' > html/HTMLEntityNames.c
  COMMAND ${GPERF_EXECUTABLE} -a -L ANSI-C -C -G -c -o -t -k '*' -N findEntity -D -s 2 < ${CMAKE_CURRENT_SOURCE_DIR}/html/HTMLEntityNames.gperf >> html/HTMLEntityNames.c

  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/html/HTMLEntityNames.gperf
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/html/DocTypeStrings.cpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND mkdir -p html
  COMMAND echo '\#include <string.h>' > html/DocTypeStrings.cpp
  COMMAND ${GPERF_EXECUTABLE} -CEot -L ANSI-C -k \"*\" -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards < ${CMAKE_CURRENT_SOURCE_DIR}/html/DocTypeStrings.gperf >> html/DocTypeStrings.cpp
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/html/DocTypeStrings.gperf
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/platform/ColorData.c
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND mkdir -p platform
  COMMAND echo '\#include <string.h>' > platform/ColorData.c
  COMMAND ${GPERF_EXECUTABLE} -CDEot -L ANSI-C -k \"*\" -N findColor -D -s 2 < ${CMAKE_CURRENT_SOURCE_DIR}/platform/ColorData.gperf >> platform/ColorData.c
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/platform/ColorData.gperf
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ksvgcssproperties.h
  COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/css/CSSPropertyNames.in ${CMAKE_CURRENT_BINARY_DIR}/ksvgcssproperties.in
  COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/scripts/cssmakeprops -n SVG -f ksvgcssproperties.in
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/scripts/cssmakeprops ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/css/CSSPropertyNames.in
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ksvgcssvalues.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND perl -ne 'print lc' ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/css/CSSValueKeywords.in > ksvgcssvalues.in
  COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/scripts/cssmakevalues -n SVG -f ksvgcssvalues.in
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/scripts/cssmakevalues ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/css/CSSValueKeywords.in
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/UserAgentStyleSheets.h ${CMAKE_CURRENT_BINARY_DIR}/UserAgentStyleSheetsData.cpp
  COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/css/make-css-file-arrays.pl ${CMAKE_CURRENT_BINARY_DIR}/UserAgentStyleSheets.h ${CMAKE_CURRENT_BINARY_DIR}/UserAgentStyleSheetsData.cpp ${CMAKE_CURRENT_SOURCE_DIR}/css/html4.css ${CMAKE_CURRENT_SOURCE_DIR}/css/quirks.css ${CMAKE_CURRENT_SOURCE_DIR}/css/svg.css ${CMAKE_CURRENT_SOURCE_DIR}/css/view-source.css
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/css/html4.css ${CMAKE_CURRENT_SOURCE_DIR}/css/quirks.css ${CMAKE_CURRENT_SOURCE_DIR}/css/svg.css
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/CharacterData.cpp
  COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/platform/make-charset-table.pl ${CMAKE_CURRENT_SOURCE_DIR}/platform/character-sets.txt ${CMAKE_CURRENT_SOURCE_DIR}/platform/qt/qt-encodings.txt \" \" > ${CMAKE_CURRENT_BINARY_DIR}/CharacterData.cpp
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/platform/make-charset-table.pl ${CMAKE_CURRENT_SOURCE_DIR}/platform/character-sets.txt
)

# source stuff
IF (USE_WEBKIT_SVG_SUPPORT)
set (SVG_STUFF
    ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/svg/SVGNames.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/svg/SVGElementFactory.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/misc/XLinkNames.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/xml/XMLNames.cpp

    ksvg2/bindings/js/JSSVGPoint.cpp
    ksvg2/bindings/js/JSSVGRect.cpp
    ksvg2/bindings/js/JSSVGNumber.cpp
    ksvg2/bindings/js/JSSVGElementWrapperFactory.cpp 

    ksvg2/css/SVGCSSParser.cpp
    ksvg2/css/SVGRenderStyleDefs.cpp
    ksvg2/css/SVGRenderStyle.cpp
    ksvg2/css/SVGCSSStyleSelector.cpp
    ksvg2/svg/SVGHelper.cpp
    ksvg2/svg/SVGFEFuncBElement.cpp
    ksvg2/svg/SVGPathSeg.cpp
    ksvg2/svg/SVGColor.cpp
    ksvg2/svg/SVGSwitchElement.cpp
    ksvg2/svg/SVGFETileElement.cpp
    ksvg2/svg/SVGDOMImplementation.cpp
    ksvg2/svg/SVGMarkerElement.cpp
    ksvg2/svg/SVGFECompositeElement.cpp
    ksvg2/svg/SVGPoint.cpp
    ksvg2/svg/SVGImageElement.cpp
    ksvg2/svg/SVGAnimateElement.cpp
    ksvg2/svg/SVGURIReference.cpp
    ksvg2/svg/SVGLength.cpp
    ksvg2/svg/SVGPathSegCurvetoCubic.cpp
    ksvg2/svg/SVGExternalResourcesRequired.cpp
    ksvg2/svg/SVGPolylineElement.cpp
    ksvg2/svg/SVGFEOffsetElement.cpp
    ksvg2/svg/SVGFETurbulenceElement.cpp
    ksvg2/svg/SVGZoomAndPan.cpp
    ksvg2/svg/SVGFilterPrimitiveStandardAttributes.cpp
    ksvg2/svg/SVGStyledLocatableElement.cpp
    ksvg2/svg/SVGLineElement.cpp
    ksvg2/svg/SVGAnimatedAngle.cpp
    ksvg2/svg/SVGTransform.cpp
    ksvg2/svg/SVGPathSegLinetoVertical.cpp
    ksvg2/svg/SVGFitToViewBox.cpp
    ksvg2/svg/SVGRadialGradientElement.cpp
    ksvg2/svg/SVGMatrix.cpp
    ksvg2/svg/SVGAnimatedPreserveAspectRatio.cpp
    ksvg2/svg/SVGMaskElement.cpp
    ksvg2/svg/SVGTitleElement.cpp
    ksvg2/svg/SVGTRefElement.cpp
    ksvg2/svg/SVGLangSpace.cpp
    ksvg2/svg/SVGTransformList.cpp
    ksvg2/svg/SVGNumber.cpp
    ksvg2/svg/SVGStylable.cpp
    ksvg2/svg/SVGPolyElement.cpp
    ksvg2/svg/SVGPolygonElement.cpp
#   ksvg2/svg/SVGElementInstanceList.cpp
    ksvg2/svg/SVGTSpanElement.cpp
    ksvg2/svg/SVGFEFuncRElement.cpp
    ksvg2/svg/SVGAnimatedPoints.cpp
    ksvg2/svg/SVGFEFloodElement.cpp
    ksvg2/svg/SVGPointList.cpp
    ksvg2/svg/SVGAnimatedEnumeration.cpp
    ksvg2/svg/SVGAnimatedPathData.cpp
    ksvg2/svg/SVGUseElement.cpp
    ksvg2/svg/SVGNumberList.cpp
    ksvg2/svg/SVGFEPointLightElement.cpp
    ksvg2/svg/SVGPathSegLineto.cpp
    ksvg2/svg/SVGRectElement.cpp
    ksvg2/svg/SVGTextContentElement.cpp
    ksvg2/svg/SVGFESpotLightElement.cpp
    ksvg2/svg/SVGLocatable.cpp
    ksvg2/svg/SVGEllipseElement.cpp
    ksvg2/svg/SVGPathElement.cpp
    ksvg2/svg/SVGStyledElement.cpp
    ksvg2/svg/SVGFEMergeNodeElement.cpp
    ksvg2/svg/SVGAnimatedLengthList.cpp
    ksvg2/svg/svgpathparser.cpp
    ksvg2/svg/SVGFEGaussianBlurElement.cpp
    ksvg2/svg/SVGLinearGradientElement.cpp
    ksvg2/svg/SVGFEDisplacementMapElement.cpp
    ksvg2/svg/SVGFEImageElement.cpp
    ksvg2/svg/SVGFEDiffuseLightingElement.cpp
    ksvg2/svg/SVGSymbolElement.cpp
    ksvg2/svg/SVGAnimatedLength.cpp
    ksvg2/svg/SVGForeignObjectElement.cpp
    ksvg2/svg/SVGAngle.cpp
    ksvg2/svg/SVGPathSegCurvetoQuadratic.cpp
    ksvg2/svg/SVGSVGElement.cpp
    ksvg2/svg/SVGFESpecularLightingElement.cpp
    ksvg2/svg/SVGAnimateColorElement.cpp
    ksvg2/svg/SVGGElement.cpp
    ksvg2/svg/SVGFEFuncGElement.cpp
    ksvg2/svg/SVGFEComponentTransferElement.cpp
    ksvg2/svg/SVGSetElement.cpp
    ksvg2/svg/SVGFEBlendElement.cpp
    ksvg2/svg/SVGFEMergeElement.cpp
    ksvg2/svg/SVGAnimatedString.cpp
    ksvg2/svg/SVGCursorElement.cpp
    ksvg2/svg/SVGAnimatedNumber.cpp
    ksvg2/svg/SVGStringList.cpp
#   ksvg2/svg/SVGElementInstance.cpp
    ksvg2/svg/SVGRect.cpp
    ksvg2/svg/SVGFilterElement.cpp
    ksvg2/svg/SVGPathSegCurvetoCubicSmooth.cpp
    ksvg2/svg/SVGPatternElement.cpp
    ksvg2/svg/SVGPathSegList.cpp
    ksvg2/svg/SVGStyleElement.cpp
    ksvg2/svg/SVGAnimatedColor.cpp
    ksvg2/svg/SVGPaint.cpp
    ksvg2/svg/SVGFEDistantLightElement.cpp
    ksvg2/svg/SVGTextPositioningElement.cpp
    ksvg2/svg/SVGAnimatedRect.cpp
    ksvg2/svg/SVGPreserveAspectRatio.cpp
    ksvg2/svg/SVGScriptElement.cpp
    ksvg2/svg/SVGComponentTransferFunctionElement.cpp
    ksvg2/svg/SVGTextElement.cpp
    ksvg2/svg/SVGViewElement.cpp
    ksvg2/svg/SVGAnimatedTransformList.cpp
    ksvg2/svg/SVGLengthList.cpp
    ksvg2/svg/SVGStyledTransformableElement.cpp
    ksvg2/svg/SVGPathSegArc.cpp
    ksvg2/svg/SVGAnimatedBoolean.cpp
    ksvg2/svg/SVGDescElement.cpp
    ksvg2/svg/SVGTransformable.cpp
    ksvg2/svg/SVGDocument.cpp
    ksvg2/svg/SVGAnimatedInteger.cpp
    ksvg2/svg/SVGClipPathElement.cpp
    ksvg2/svg/SVGPathSegMoveto.cpp
    ksvg2/svg/SVGAElement.cpp
    ksvg2/svg/SVGCircleElement.cpp
    ksvg2/svg/SVGFEFuncAElement.cpp
    ksvg2/svg/SVGTests.cpp
    ksvg2/svg/SVGPathSegCurvetoQuadraticSmooth.cpp
    ksvg2/svg/SVGElement.cpp
    ksvg2/svg/SVGAnimateTransformElement.cpp
    ksvg2/svg/SVGAnimatedNumberList.cpp
    ksvg2/svg/SVGFEColorMatrixElement.cpp
    ksvg2/svg/SVGGradientElement.cpp
    ksvg2/svg/SVGAnimationElement.cpp
    ksvg2/svg/SVGFELightElement.cpp
    ksvg2/svg/SVGPathSegClosePath.cpp
    ksvg2/svg/SVGPathSegLinetoHorizontal.cpp
    ksvg2/svg/SVGStopElement.cpp
    ksvg2/svg/SVGDefsElement.cpp
#   ksvg2/ecma/GlobalObject.cpp
#   ksvg2/ecma/Ecma.cpp
    ksvg2/misc/SVGImageLoader.cpp
    ksvg2/misc/SVGDocumentExtensions.cpp
    ksvg2/misc/KSVGTimeScheduler.cpp
    ksvg2/misc/KCanvasRenderingStyle.cpp
    ksvg2/events/JSSVGLazyEventListener.cpp
    ksvg2/events/SVGZoomEvent.cpp
    kcanvas/KCanvasCreator.cpp
    kcanvas/KCanvasPath.cpp
    kcanvas/KCanvasTreeDebug.cpp
    kcanvas/KCanvasFilters.cpp
    kcanvas/RenderSVGText.cpp
    kcanvas/device/KRenderingPaintServerGradient.cpp
    kcanvas/device/KRenderingPaintServerSolid.cpp
    kcanvas/device/KRenderingFillPainter.cpp
    kcanvas/device/KRenderingStrokePainter.cpp
    kcanvas/device/KRenderingPaintServerPattern.cpp
    kcanvas/device/KRenderingDevice.cpp
    kcanvas/device/qt/RenderPathQt.cpp
    kcanvas/device/qt/KCanvasPathQt.cpp
    kcanvas/device/qt/KRenderingDeviceQt.cpp
    kcanvas/device/qt/KRenderingPaintServerSolidQt.cpp
    kcanvas/device/qt/KRenderingPaintServerGradientQt.cpp
    kcanvas/device/qt/KRenderingPaintServerPatternQt.cpp
    kcanvas/device/qt/KRenderingPaintServerQt.cpp
    kcanvas/device/qt/KCanvasClipperQt.cpp
#   kcanvas/device/qt/KCanvasFilterQt.cpp
#   kcanvas/device/qt/KCanvasImageItemQt.cpp
    kcanvas/RenderSVGContainer.cpp
    kcanvas/RenderPath.cpp
    kcanvas/KCanvasResources.cpp
    kcanvas/RenderSVGImage.cpp
    kcanvas/RenderForeignObject.cpp
 )
ENDIF(USE_WEBKIT_SVG_SUPPORT)


set(WebCore_SRCS

    ${WebCore-unity_LIB_SRCS}

    ${CMAKE_CURRENT_BINARY_DIR}/CSSPropertyNames.h
    ${CMAKE_CURRENT_BINARY_DIR}/CSSValueKeywords.h
    ${CMAKE_CURRENT_BINARY_DIR}/CSSGrammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/CSSGrammar.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/CharacterData.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/UserAgentStyleSheets.h
    ${CMAKE_CURRENT_BINARY_DIR}/UserAgentStyleSheetsData.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/html/HTMLNames.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/html/HTMLEntityNames.c
    ${CMAKE_CURRENT_BINARY_DIR}/html/DocTypeStrings.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/platform/ColorData.c
    ${CMAKE_CURRENT_BINARY_DIR}/ksvgcssproperties.h
    ${CMAKE_CURRENT_BINARY_DIR}/ksvgcssvalues.h

    bindings/js/kjs_binding.cpp
    bindings/js/kjs_css.cpp
    bindings/js/kjs_dom.cpp
    bindings/js/kjs_events.cpp
    bindings/js/kjs_html.cpp
    bindings/js/kjs_navigator.cpp
    bindings/js/kjs_proxy.cpp
    bindings/js/kjs_traversal.cpp
    bindings/js/kjs_window.cpp

    bindings/js/JSCanvasRenderingContext2DCustom.cpp
    bindings/js/JSHTMLElementWrapperFactory.cpp
    bindings/js/JSHTMLFormElementCustom.cpp
    bindings/js/JSHTMLOptionElementConstructor.cpp
    bindings/js/JSHTMLOptionsCollectionCustom.cpp
    bindings/js/JSHTMLInputElementBase.cpp
    bindings/js/JSXMLHttpRequest.cpp
    bindings/js/JSNodeIteratorCustom.cpp
    bindings/js/JSTreeWalkerCustom.cpp
    bindings/js/JSXSLTProcessor.cpp

    css/cssparser.cpp
    css/cssstyleselector.cpp
    css/csshelper.cpp
    css/FontValue.cpp
    css/CSSSelector.cpp
    css/MediaFeatureNames.cpp
    css/CSSRuleList.cpp
    css/MediaQueryExp.cpp
    css/CSSImportRule.cpp
    css/CSSValueList.cpp
    css/CSSStyleDeclaration.cpp
    css/CSSPrimitiveValue.cpp
    css/CSSProperty.cpp
    css/CSSBorderImageValue.cpp
    css/MediaQuery.cpp
    css/StyleSheet.cpp
    css/CSSPageRule.cpp
    css/StyleSheetList.cpp
    css/MediaQueryEvaluator.cpp
    css/StyleBase.cpp
    css/CSSRule.cpp
    css/CSSStyleSheet.cpp
    css/CSSInitialValue.cpp
    css/CSSImageValue.cpp
    css/CSSStyleRule.cpp
    css/CSSInheritedValue.cpp
    css/StyleList.cpp
    css/FontFamilyValue.cpp
    css/CSSMediaRule.cpp
    css/CSSComputedStyleDeclaration.cpp
    css/CSSMutableStyleDeclaration.cpp
    css/MediaList.cpp
    css/CSSFontFaceRule.cpp
    css/ShadowValue.cpp
    dom/Event.cpp
    dom/Range.cpp
    dom/Text.cpp
    dom/DOMImplementation.cpp
    dom/NodeFilter.cpp
    dom/MouseEvent.cpp
    dom/EntityReference.cpp
    dom/NameNodeList.cpp
    dom/CharacterData.cpp
    dom/XMLTokenizer.cpp
    dom/StyledElement.cpp
    dom/MappedAttribute.cpp
    dom/NamedAttrMap.cpp
    dom/ContainerNode.cpp
    dom/NamedMappedAttrMap.cpp
    dom/EventNames.cpp
    dom/Comment.cpp
    dom/EditingText.cpp
    dom/DocumentFragment.cpp
    dom/ChildNodeList.cpp
    dom/Entity.cpp
    dom/BeforeTextInsertedEvent.cpp
    dom/UIEvent.cpp
    dom/Node.cpp
    dom/Attribute.cpp
    dom/Position.cpp
    dom/ProcessingInstruction.cpp
    dom/TreeWalker.cpp
    dom/Notation.cpp
    dom/Element.cpp
    dom/NodeFilterCondition.cpp
    dom/CDATASection.cpp
    dom/DocumentType.cpp
    dom/NodeList.cpp
    dom/CSSMappedAttributeDeclaration.cpp
    dom/QualifiedName.cpp
    dom/Document.cpp
    dom/Attr.cpp
    dom/OverflowEvent.cpp
    dom/RegisteredEventListener.cpp
    dom/EventTargetNode.cpp
    dom/BeforeUnloadEvent.cpp
    dom/MutationEvent.cpp
    dom/MouseRelatedEvent.cpp
    dom/KeyboardEvent.cpp
    dom/NodeIterator.cpp
    dom/ClipboardEvent.cpp
    dom/Traversal.cpp
    dom/WheelEvent.cpp
#   kwq/KWQWMatrix.cpp
#   kwq/KWQCString.cpp

    ${SVG_STUFF}
    editing/InsertIntoTextNodeCommand.cpp
    editing/WrapContentsInDummySpanCommand.cpp
    editing/ReplaceSelectionCommand.cpp
    editing/MoveSelectionCommand.cpp
    editing/RemoveNodePreservingChildrenCommand.cpp
    editing/HTMLInterchange.cpp
    editing/UnlinkCommand.cpp
    editing/InsertLineBreakCommand.cpp
    editing/FormatBlockCommand.cpp
    editing/AppendNodeCommand.cpp
    editing/BreakBlockquoteCommand.cpp
    editing/htmlediting.cpp
    editing/RebalanceWhitespaceCommand.cpp
    editing/markup.cpp
    editing/InsertParagraphSeparatorCommand.cpp
    editing/ModifySelectionListLevel.cpp
    editing/JSEditor.cpp
    editing/Selection.cpp
    editing/TextIterator.cpp
    editing/InsertListCommand.cpp
    editing/IndentOutdentCommand.cpp
    editing/InsertNodeBeforeCommand.cpp
    editing/SplitTextNodeContainingElementCommand.cpp
    editing/TypingCommand.cpp
    editing/MergeIdenticalElementsCommand.cpp
    editing/EditCommand.cpp
    editing/SplitTextNodeCommand.cpp
    editing/RemoveCSSPropertyCommand.cpp
    editing/JoinTextNodesCommand.cpp
    editing/InsertTextCommand.cpp
    editing/SelectionController.cpp
    editing/DeleteSelectionCommand.cpp
    editing/SplitElementCommand.cpp
    editing/VisiblePosition.cpp
    editing/ApplyStyleCommand.cpp
    editing/visible_units.cpp
    editing/RemoveNodeAttributeCommand.cpp
    editing/DeleteFromTextNodeCommand.cpp
    editing/RemoveNodeCommand.cpp
    editing/CompositeEditCommand.cpp
    editing/SetNodeAttributeCommand.cpp
    editing/CreateLinkCommand.cpp


#   kwq/KWQTextStream.cpp
    xml/XSLStyleSheet.cpp
    xml/XSLTProcessor.cpp
    xml/XSLImportRule.cpp
    xml/DOMParser.cpp
    xml/XMLSerializer.cpp
    xml/xmlhttprequest.cpp
#   icon/IconDatabase.cpp
#   icon/SQLTransaction.cpp
#   icon/SQLStatement.cpp
#   icon/SiteIcon.cpp
#   icon/SQLDatabase.cpp
    html/HTMLParser.cpp
    html/HTMLFontElement.cpp
    html/HTMLEmbedElement.cpp
    html/HTMLLinkElement.cpp
    html/HTMLOptGroupElement.cpp
    html/HTMLCanvasElement.cpp
    html/HTMLTitleElement.cpp
    html/CanvasRenderingContext2D.cpp
    html/HTMLObjectElement.cpp
    html/HTMLAppletElement.cpp
    html/HTMLKeygenElement.cpp
    html/HTMLDivElement.cpp
    html/HTMLMapElement.cpp
    html/HTMLScriptElement.cpp
    html/HTMLHtmlElement.cpp
    html/HTMLTokenizer.cpp
    html/HTMLOptionElement.cpp
    html/HTMLTableCaptionElement.cpp
    html/HTMLImageLoader.cpp
    html/FormDataList.cpp
    html/HTMLLabelElement.cpp
    html/HTMLTableColElement.cpp
    html/HTMLDListElement.cpp
    html/HTMLTablePartElement.cpp
    html/HTMLTableSectionElement.cpp
    html/HTMLTextAreaElement.cpp
    html/HTMLTextFieldInnerElement.cpp
    html/HTMLAreaElement.cpp
    html/CanvasStyle.cpp
    html/HTMLIsIndexElement.cpp
    html/HTMLHeadElement.cpp
    html/HTMLFrameSetElement.cpp
    html/HTMLBodyElement.cpp
    html/HTMLBRElement.cpp
    html/HTMLNameCollection.cpp
    html/HTMLLegendElement.cpp
    html/HTMLLIElement.cpp
    html/HTMLParamElement.cpp
    html/HTMLMetaElement.cpp
    html/HTMLHeadingElement.cpp
    html/HTMLUListElement.cpp
    html/HTMLInputElement.cpp
    html/HTMLElementFactory.cpp
    html/HTMLPlugInElement.cpp
    html/HTMLFieldSetElement.cpp
    html/HTMLParagraphElement.cpp
    html/HTMLStyleElement.cpp
    html/HTMLMarqueeElement.cpp
    html/HTMLGenericFormElement.cpp
    html/HTMLElement.cpp
    html/HTMLDocument.cpp
    html/HTMLOListElement.cpp
    html/HTMLFormElement.cpp
    html/HTMLPreElement.cpp
    html/HTMLTableElement.cpp
    html/CanvasGradient.cpp
    html/HTMLViewSourceDocument.cpp
    html/HTMLFrameElement.cpp
    html/HTMLAnchorElement.cpp
    html/HTMLTableCellElement.cpp
    html/CanvasPattern.cpp
    html/HTMLBlockquoteElement.cpp
    html/HTMLIFrameElement.cpp
    html/HTMLMenuElement.cpp
    html/HTMLCollection.cpp
    html/HTMLModElement.cpp
    html/HTMLQuoteElement.cpp
    html/HTMLDirectoryElement.cpp
    html/HTMLSelectElement.cpp
    html/HTMLImageElement.cpp
    html/HTMLOptionsCollection.cpp
    html/HTMLTableRowElement.cpp
    html/HTMLBaseFontElement.cpp
    html/HTMLHRElement.cpp
    html/HTMLButtonElement.cpp
    html/HTMLFormCollection.cpp
    html/HTMLBaseElement.cpp
    page/FrameTree.cpp
    page/DOMWindow.cpp
    page/MouseEventWithHitTestResults.cpp
    page/Frame.cpp
    page/Page.cpp
    page/FrameView.cpp
    xml/XPathUtil.cpp
    xml/XPathPredicate.cpp
    xml/XPathVariableReference.cpp
    xml/XPathValue.cpp
    xml/XPathPath.cpp
    xml/XPathFunctions.cpp
    xml/XPathParser.cpp
    xml/XPathStep.cpp
    xml/XPathExpressionNode.cpp
    xml/XPathNamespace.cpp
    xml/XPathNSResolver.cpp
    xml/XPathExpression.cpp
    xml/XPathResult.cpp
    xml/XPathEvaluator.cpp
    loader/CachedImage.cpp
    loader/CachedScript.cpp
    loader/Request.cpp
    loader/CachedXSLStyleSheet.cpp
    loader/DocLoader.cpp
    loader/PluginDocument.cpp
    loader/TextDocument.cpp
    loader/ImageDocument.cpp
    loader/CachedResourceClientWalker.cpp
    loader/loader.cpp
    loader/CachedCSSStyleSheet.cpp
    loader/CachedXBLDocument.cpp
    loader/CachedResource.cpp
    loader/Cache.cpp
    loader/Decoder.cpp
    loader/FormData.cpp
    platform/AtomicString.cpp
    platform/AffineTransform.cpp
    platform/TextStream.cpp
    platform/Widget.cpp
    platform/GlyphWidthMap.cpp
    platform/Pen.cpp
    platform/Image.cpp
    platform/DeprecatedStringList.cpp
    platform/FloatSize.cpp
    platform/String.cpp
    platform/DeprecatedValueListImpl.cpp
    platform/GraphicsTypes.cpp
    platform/IntRect.cpp
    platform/Arena.cpp
    platform/CharsetNames.cpp
    platform/ArrayImpl.cpp
    platform/FloatPoint.cpp
    platform/SegmentedString.cpp
    platform/StreamingTextDecoder.cpp
    platform/StreamingTextDecoderICU.cpp
    platform/DeprecatedString.cpp
    platform/DeprecatedCString.cpp
    platform/TextEncoding.cpp
    platform/GraphicsContext.cpp
    platform/Logging.cpp
    platform/Color.cpp
    platform/DeprecatedPtrListImpl.cpp
    platform/KURL.cpp
    platform/StringImpl.cpp
    platform/FloatRect.cpp
    platform/ResourceLoader.cpp
    platform/qt/AffineTransformQt.cpp
    platform/qt/StringQt.cpp
    platform/qt/ColorQt.cpp
    platform/qt/GlyphMapQt.cpp
    platform/qt/BrowserExtensionQt.cpp
    platform/qt/CookieJarQt.cpp
    platform/qt/FontPlatformDataQt.cpp
    platform/qt/GraphicsContextQt.cpp
    platform/qt/ScrollViewQt.cpp
    platform/qt/TemporaryLinkStubs.cpp
    platform/qt/CursorQt.cpp
    platform/qt/WidgetQt.cpp
    platform/qt/SystemTimeQt.cpp
    platform/qt/RenderThemeQt.cpp
    platform/qt/FontDataQt.cpp
    platform/qt/SharedTimerQt.cpp
    platform/qt/ResourceLoaderCurl.cpp
    platform/qt/ResourceLoaderManager.cpp
    platform/qt/FloatPointQt.cpp
    platform/qt/ListBoxQt.cpp
    platform/qt/FloatRectQt.cpp
    platform/qt/IntPointQt.cpp
    platform/qt/IntRectQt.cpp
    platform/qt/IntSizeQt.cpp
    platform/qt/PageQt.cpp
    platform/qt/PathQt.cpp
    platform/qt/ImageQt.cpp
    platform/qt/ImageSourceQt.cpp
    platform/qt/FontCacheQt.cpp
    platform/qt/FontQt.cpp
#   platform/qt/LineEditQt.cpp
    platform/qt/ScreenQt.cpp
#   platform/qt/FileButtonQt.cpp
    platform/qt/FrameQt.cpp
#   platform/qt/TextEditQt.cpp
    platform/qt/ScrollViewCanvasQt.cpp
    platform/qt/PlatformMouseEventQt.cpp
    platform/qt/PlatformKeyboardEventQt.cpp
    platform/image-decoders/bmp/BMPImageDecoder.cpp
    platform/image-decoders/gif/GIFImageReader.cpp
    platform/image-decoders/gif/GIFImageDecoder.cpp
    platform/image-decoders/ico/ICOImageDecoder.cpp
    platform/image-decoders/png/PNGImageDecoder.cpp
    platform/image-decoders/xbm/XBMImageDecoder.cpp
    platform/image-decoders/jpeg/JPEGImageDecoder.cpp
    platform/FontFamily.cpp
    platform/Timer.cpp
    platform/FontCache.cpp
    platform/FontFallbackList.cpp
    platform/RegularExpression.cpp
    platform/GlyphMap.cpp
    platform/Font.cpp
    platform/FontData.cpp
    rendering/RenderReplaced.cpp
    rendering/RenderPartObject.cpp
    rendering/RenderView.cpp
    rendering/RenderMenuList.cpp
    rendering/InlineFlowBox.cpp
    rendering/RenderListMarker.cpp
    rendering/RenderImage.cpp
    rendering/RenderTheme.cpp
    rendering/RenderLayer.cpp
    rendering/RenderTableCell.cpp
    rendering/RenderListItem.cpp
    rendering/RenderImageButton.cpp
    rendering/AutoTableLayout.cpp
    rendering/RenderArena.cpp
    rendering/RenderWidget.cpp
    rendering/break_lines.cpp
    rendering/RenderStyle.cpp
    rendering/RenderContainer.cpp
    rendering/EllipsisBox.cpp
    rendering/DeprecatedRenderSelect.cpp
    rendering/RenderFieldset.cpp
    rendering/RenderFrameSet.cpp
    rendering/RenderTable.cpp
    rendering/RenderPart.cpp
    rendering/RenderBlock.cpp
    rendering/DeprecatedSlider.cpp
    rendering/InlineBox.cpp
    rendering/RenderText.cpp
    rendering/RenderFrame.cpp
    rendering/RenderLineEdit.cpp
    rendering/FixedTableLayout.cpp
    rendering/RenderTableCol.cpp
    rendering/RenderObject.cpp
    rendering/RenderTreeAsText.cpp
    rendering/RootInlineBox.cpp
    rendering/RenderFormElement.cpp
    rendering/RenderBox.cpp
    rendering/RenderButton.cpp
    rendering/RenderTableSection.cpp
    rendering/ListMarkerBox.cpp
    rendering/RenderLabel.cpp
    rendering/RenderTableRow.cpp
    rendering/RenderInline.cpp
    rendering/RenderFileButton.cpp
    rendering/RenderHTMLCanvas.cpp
    rendering/bidi.cpp
    rendering/RenderFlexibleBox.cpp
    rendering/RenderApplet.cpp
    rendering/RenderLegend.cpp
    rendering/RenderTextControl.cpp
    rendering/RenderTextFragment.cpp
    rendering/RenderBR.cpp
    rendering/InlineTextBox.cpp
    rendering/RenderFlow.cpp
    platform/image-decoders/png/png.c
    platform/image-decoders/png/pngget.c
    platform/image-decoders/png/pngread.c
    platform/image-decoders/png/pngrutil.c
    platform/image-decoders/png/pngvcrd.c
    platform/image-decoders/png/pngwtran.c
    platform/image-decoders/png/pngerror.c
    platform/image-decoders/png/pngmem.c
    platform/image-decoders/png/pngrio.c
    platform/image-decoders/png/pngset.c
    platform/image-decoders/png/pngwio.c
    platform/image-decoders/png/pngwutil.c
    platform/image-decoders/png/pnggccrd.c
    platform/image-decoders/png/pngpread.c
    platform/image-decoders/png/pngrtran.c
    platform/image-decoders/png/pngtrans.c
    platform/image-decoders/png/pngwrite.c
    platform/image-decoders/jpeg/jcomapi.c
    platform/image-decoders/jpeg/jdapimin.c
    platform/image-decoders/jpeg/jdapistd.c
    platform/image-decoders/jpeg/jdatadst.c
    platform/image-decoders/jpeg/jdatasrc.c
    platform/image-decoders/jpeg/jdcoefct.c
    platform/image-decoders/jpeg/jdcolor.c
    platform/image-decoders/jpeg/jddctmgr.c
    platform/image-decoders/jpeg/jdhuff.c
    platform/image-decoders/jpeg/jdinput.c
    platform/image-decoders/jpeg/jdmainct.c
    platform/image-decoders/jpeg/jdmarker.c
    platform/image-decoders/jpeg/jdmaster.c
    platform/image-decoders/jpeg/jdmerge.c
    platform/image-decoders/jpeg/jdphuff.c
    platform/image-decoders/jpeg/jdpostct.c
    platform/image-decoders/jpeg/jdsample.c
    platform/image-decoders/jpeg/jerror.c
    platform/image-decoders/jpeg/jfdctflt.c
    platform/image-decoders/jpeg/jfdctfst.c
    platform/image-decoders/jpeg/jfdctint.c
    platform/image-decoders/jpeg/jidctflt.c
    platform/image-decoders/jpeg/jidctfst.c
    platform/image-decoders/jpeg/jidctint.c
    platform/image-decoders/jpeg/jmemmgr.c
    platform/image-decoders/jpeg/jmemnobs.c
    platform/image-decoders/jpeg/jquant1.c
    platform/image-decoders/jpeg/jquant2.c
    platform/image-decoders/jpeg/jutils.c

    ${WebCore-unity-IDLGeneratedSources}
)


kde4_automoc(platform/qt/SharedTimerQt.cpp)

kde4_add_library(WebCore-unity SHARED  ${WebCore_SRCS})

target_link_libraries(WebCore-unity

  ${QT_QT3SUPPORT_LIBRARY}
  ${QT_QTCORE_LIBRARY}
  ${QT_QTSVG_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${LIBXSLT_LIBRARIES}
  ${LIBXML2_LIBRARIES}
  icuuc
  curl
  wtf-unity
  kjs-unity
  pcre-unity

)
