
project( WebCore )

IF (WEBKIT_USE_KDE_SUPPORT)
    include (CheckCXXCompilerFlag)
ENDIF (WEBKIT_USE_KDE_SUPPORT)

include_directories(
   BEFORE ${CMAKE_CURRENT_BINARY_DIR}
   ${CMAKE_CURRENT_SOURCE_DIR}/../
   ${CMAKE_CURRENT_SOURCE_DIR}/../JavaScriptCore/
   ${CMAKE_CURRENT_SOURCE_DIR}/../JavaScriptCore/bindings
   ${CMAKE_CURRENT_SOURCE_DIR}/../JavaScriptCore/kjs
   ${CMAKE_CURRENT_SOURCE_DIR}/bindings/js
   ${CMAKE_CURRENT_SOURCE_DIR}/bridge
   ${CMAKE_CURRENT_BINARY_DIR}/css
   ${CMAKE_CURRENT_SOURCE_DIR}/css
   ${CMAKE_CURRENT_SOURCE_DIR}/dom
   ${CMAKE_CURRENT_SOURCE_DIR}/editing
   ${CMAKE_CURRENT_SOURCE_DIR}/editing/qt
   ${CMAKE_CURRENT_BINARY_DIR}/html
   ${CMAKE_CURRENT_SOURCE_DIR}/html
   ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2 
   ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/css
   ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/events 
   ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/misc 
   ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/misc
   ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/svg
   ${CMAKE_CURRENT_SOURCE_DIR}/ksvg2/svg
   ${CMAKE_CURRENT_SOURCE_DIR}/loader
   ${CMAKE_CURRENT_SOURCE_DIR}/loader/icon
   ${CMAKE_CURRENT_SOURCE_DIR}/loader/qt
   ${CMAKE_CURRENT_SOURCE_DIR}/page
   ${CMAKE_CURRENT_SOURCE_DIR}/page/qt
   ${CMAKE_CURRENT_BINARY_DIR}/platform
   ${CMAKE_CURRENT_SOURCE_DIR}/platform 
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/graphics
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/graphics/qt
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/graphics/svg
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/graphics/svg/filters
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/image-decoders
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/image-decoders/gif
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/image-decoders/jpeg
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/image-decoders/ico
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/image-decoders/xbm
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/image-decoders/bmp
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/image-decoders/png
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/network
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/network/qt
   ${CMAKE_CURRENT_SOURCE_DIR}/platform/qt
   ${CMAKE_CURRENT_SOURCE_DIR}/rendering
   ${CMAKE_CURRENT_BINARY_DIR}/xml
   ${CMAKE_CURRENT_SOURCE_DIR}/xml
   ${LIBXML2_INCLUDE_DIR}
   ${QT_INCLUDES}
)

add_definitions( -DQT3_SUPPORT -Wno-undef -Wno-unused-parameter )
IF (WEBKIT_USE_KDE_SUPPORT)
    check_cxx_compiler_flag( -Wno-variadic-macros __KDE_HAVE_WVARIADIC_MACROS )
ENDIF (WEBKIT_USE_KDE_SUPPORT)

IF ( __KDE_HAVE_WVARIADIC_MACROS )
  add_definitions( -Wno-variadic-macros )
ENDIF (__KDE_HAVE_WVARIADIC_MACROS)

IF (WEBKIT_USE_KDE_SUPPORT)
    kde4_automoc( WebCore platform/network/qt/ResourceHandleManager.cpp )
    kde4_automoc( WebCore platform/qt/ScrollViewCanvasQt.cpp )
    kde4_automoc( WebCore platform/qt/SharedTimerQt.cpp )
ELSE (WEBKIT_USE_KDE_SUPPORT)
    qt4_automoc( WebCore platform/network/qt/ResourceHandleManager.cpp )
    qt4_automoc( WebCore platform/qt/ScrollViewCanvasQt.cpp )
    qt4_automoc( WebCore platform/qt/SharedTimerQt.cpp )
ENDIF (WEBKIT_USE_KDE_SUPPORT)

SET (FEATURE_DEFINES_JAVASCRIPT LANGUAGE_JAVASCRIPT)

IF (WEBKIT_USE_XPATH_SUPPORT)
SET (FEATURE_DEFINES_JAVASCRIPT ${FEATURE_DEFINES_JAVASCRIPT} XPATH_SUPPORT)

set(IDL_BINDINGS_XPATH
    xml/XPathEvaluator.idl
    xml/XPathExpression.idl
    xml/XPathNSResolver.idl
    xml/XPathResult.idl
)
ENDIF (WEBKIT_USE_XPATH_SUPPORT)

IF (WEBKIT_USE_XSLT_SUPPORT)
SET (FEATURE_DEFINES_JAVASCRIPT ${FEATURE_DEFINES_JAVASCRIPT} XSLT_SUPPORT)
ENDIF (WEBKIT_USE_XSLT_SUPPORT)

IF (WEBKIT_USE_XBL_SUPPORT)
SET (FEATURE_DEFINES_JAVASCRIPT ${FEATURE_DEFINES_JAVASCRIPT} XBL_SUPPORT)
ENDIF (WEBKIT_USE_XBL_SUPPORT)

# rules for generated files
IF (WEBKIT_USE_SVG_SUPPORT)
SET (FEATURE_DEFINES_JAVASCRIPT ${FEATURE_DEFINES_JAVASCRIPT} SVG_SUPPORT)

set(IDL_BINDINGS_SVG
    ksvg2/events/SVGZoomEvent.idl

    ksvg2/svg/SVGAElement.idl
    ksvg2/svg/SVGAngle.idl
    ksvg2/svg/SVGAnimateColorElement.idl
    ksvg2/svg/SVGAnimatedAngle.idl
    ksvg2/svg/SVGAnimatedBoolean.idl
    ksvg2/svg/SVGAnimatedEnumeration.idl
    ksvg2/svg/SVGAnimatedInteger.idl
    ksvg2/svg/SVGAnimatedLength.idl
    ksvg2/svg/SVGAnimatedLengthList.idl
    ksvg2/svg/SVGAnimatedNumber.idl
    ksvg2/svg/SVGAnimatedNumberList.idl
    ksvg2/svg/SVGAnimatedPreserveAspectRatio.idl
    ksvg2/svg/SVGAnimatedRect.idl
    ksvg2/svg/SVGAnimatedString.idl
    ksvg2/svg/SVGAnimatedTransformList.idl
    ksvg2/svg/SVGAnimateElement.idl
    ksvg2/svg/SVGAnimateTransformElement.idl
    ksvg2/svg/SVGAnimationElement.idl
    ksvg2/svg/SVGCircleElement.idl
    ksvg2/svg/SVGClipPathElement.idl
    ksvg2/svg/SVGColor.idl
    ksvg2/svg/SVGComponentTransferFunctionElement.idl
    ksvg2/svg/SVGCursorElement.idl
    ksvg2/svg/SVGDefsElement.idl
    ksvg2/svg/SVGDescElement.idl
    ksvg2/svg/SVGDocument.idl
    ksvg2/svg/SVGElement.idl
    ksvg2/svg/SVGEllipseElement.idl
    ksvg2/svg/SVGFEBlendElement.idl
    ksvg2/svg/SVGFEColorMatrixElement.idl
    ksvg2/svg/SVGFEComponentTransferElement.idl
    ksvg2/svg/SVGFECompositeElement.idl
    ksvg2/svg/SVGFEDiffuseLightingElement.idl
    ksvg2/svg/SVGFEDisplacementMapElement.idl
    ksvg2/svg/SVGFEDistantLightElement.idl
    ksvg2/svg/SVGFEFloodElement.idl
    ksvg2/svg/SVGFEFuncAElement.idl
    ksvg2/svg/SVGFEFuncBElement.idl
    ksvg2/svg/SVGFEFuncGElement.idl
    ksvg2/svg/SVGFEFuncRElement.idl
    ksvg2/svg/SVGFEGaussianBlurElement.idl
    ksvg2/svg/SVGFEImageElement.idl
    ksvg2/svg/SVGFEMergeElement.idl
    ksvg2/svg/SVGFEMergeNodeElement.idl
    ksvg2/svg/SVGFEOffsetElement.idl
    ksvg2/svg/SVGFEPointLightElement.idl
    ksvg2/svg/SVGFESpecularLightingElement.idl
    ksvg2/svg/SVGFESpotLightElement.idl
    ksvg2/svg/SVGFETileElement.idl
    ksvg2/svg/SVGFETurbulenceElement.idl
    ksvg2/svg/SVGFilterElement.idl
    ksvg2/svg/SVGForeignObjectElement.idl
    ksvg2/svg/SVGGElement.idl
    ksvg2/svg/SVGGradientElement.idl
    ksvg2/svg/SVGImageElement.idl
    ksvg2/svg/SVGLength.idl
    ksvg2/svg/SVGLengthList.idl
    ksvg2/svg/SVGLinearGradientElement.idl
    ksvg2/svg/SVGLineElement.idl
    ksvg2/svg/SVGMarkerElement.idl
    ksvg2/svg/SVGMaskElement.idl
    ksvg2/svg/SVGMatrix.idl
    ksvg2/svg/SVGMetadataElement.idl
    ksvg2/svg/SVGNumberList.idl
    ksvg2/svg/SVGPaint.idl
    ksvg2/svg/SVGPathElement.idl
    ksvg2/svg/SVGPathSegArcAbs.idl
    ksvg2/svg/SVGPathSegArcRel.idl
    ksvg2/svg/SVGPathSegClosePath.idl
    ksvg2/svg/SVGPathSegCurvetoCubicAbs.idl
    ksvg2/svg/SVGPathSegCurvetoCubicRel.idl
    ksvg2/svg/SVGPathSegCurvetoCubicSmoothAbs.idl
    ksvg2/svg/SVGPathSegCurvetoCubicSmoothRel.idl
    ksvg2/svg/SVGPathSegCurvetoQuadraticAbs.idl
    ksvg2/svg/SVGPathSegCurvetoQuadraticRel.idl
    ksvg2/svg/SVGPathSegCurvetoQuadraticSmoothAbs.idl
    ksvg2/svg/SVGPathSegCurvetoQuadraticSmoothRel.idl
    ksvg2/svg/SVGPathSeg.idl
    ksvg2/svg/SVGPathSegLinetoAbs.idl
    ksvg2/svg/SVGPathSegLinetoHorizontalAbs.idl
    ksvg2/svg/SVGPathSegLinetoHorizontalRel.idl
    ksvg2/svg/SVGPathSegLinetoRel.idl
    ksvg2/svg/SVGPathSegLinetoVerticalAbs.idl
    ksvg2/svg/SVGPathSegLinetoVerticalRel.idl
    ksvg2/svg/SVGPathSegList.idl
    ksvg2/svg/SVGPathSegMovetoAbs.idl
    ksvg2/svg/SVGPathSegMovetoRel.idl
    ksvg2/svg/SVGPatternElement.idl
    ksvg2/svg/SVGPointList.idl
    ksvg2/svg/SVGPolygonElement.idl
    ksvg2/svg/SVGPolylineElement.idl
    ksvg2/svg/SVGPreserveAspectRatio.idl
    ksvg2/svg/SVGRadialGradientElement.idl
    ksvg2/svg/SVGRectElement.idl
    ksvg2/svg/SVGRenderingIntent.idl
    ksvg2/svg/SVGScriptElement.idl
    ksvg2/svg/SVGSetElement.idl
    ksvg2/svg/SVGStopElement.idl
    ksvg2/svg/SVGStringList.idl
    ksvg2/svg/SVGStyleElement.idl
    ksvg2/svg/SVGSVGElement.idl
    ksvg2/svg/SVGSwitchElement.idl
    ksvg2/svg/SVGSymbolElement.idl
    ksvg2/svg/SVGTextContentElement.idl
    ksvg2/svg/SVGTextElement.idl
    ksvg2/svg/SVGTextPositioningElement.idl
    ksvg2/svg/SVGTitleElement.idl
    ksvg2/svg/SVGTransform.idl
    ksvg2/svg/SVGTransformList.idl
    ksvg2/svg/SVGTRefElement.idl
    ksvg2/svg/SVGTSpanElement.idl
    ksvg2/svg/SVGUnitTypes.idl
    ksvg2/svg/SVGUseElement.idl
    ksvg2/svg/SVGViewElement.idl
)

ENDIF (WEBKIT_USE_SVG_SUPPORT)

set(IDL_BINDINGS
    css/Counter.idl
    css/CSSPrimitiveValue.idl
    css/CSSRule.idl
    css/CSSRuleList.idl
    css/CSSStyleDeclaration.idl
    css/CSSValue.idl
    css/CSSValueList.idl
    css/MediaList.idl

    dom/Attr.idl
    dom/CharacterData.idl
    dom/DocumentFragment.idl
    dom/Document.idl
    dom/DocumentType.idl
    dom/DOMImplementation.idl
    dom/Element.idl
    dom/Entity.idl
    dom/Event.idl
    dom/KeyboardEvent.idl
    dom/MouseEvent.idl
    dom/MutationEvent.idl
    dom/NodeFilter.idl
    dom/Node.idl
    dom/NodeIterator.idl
    dom/Notation.idl
    dom/OverflowEvent.idl
    dom/ProcessingInstruction.idl
    dom/RangeException.idl
    dom/Range.idl
    dom/Text.idl
    dom/TreeWalker.idl
    dom/UIEvent.idl
    dom/WheelEvent.idl

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
    html/HTMLOptionsCollection.idl
    html/HTMLParagraphElement.idl
    html/HTMLParamElement.idl
    html/HTMLPreElement.idl
    html/HTMLQuoteElement.idl
    html/HTMLScriptElement.idl
    html/HTMLStyleElement.idl
    html/HTMLTextAreaElement.idl
    html/HTMLTitleElement.idl
    html/HTMLUListElement.idl

    page/DOMWindow.idl

    xml/DOMParser.idl
    xml/XMLSerializer.idl

    ${IDL_BINDINGS_SVG}
    ${IDL_BINDINGS_XPATH}
)

set(WebCore-unity-IDLGeneratedSources )

# create these files at build time on demand, not everytime at cmake-time, Alex
foreach (idl ${IDL_BINDINGS})
   get_filename_component(_basename ${idl} NAME_WE)
   add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/JS${_basename}.cpp ${CMAKE_CURRENT_BINARY_DIR}/JS${_basename}.h
      COMMAND perl -I${CMAKE_CURRENT_SOURCE_DIR}/bindings/scripts ${CMAKE_CURRENT_SOURCE_DIR}/bindings/scripts/generate-bindings.pl  --defines \"${FEATURE_DEFINES_JAVASCRIPT}\" --generator JS --include ${CMAKE_SOURCE_DIR}/WebCore/dom --include ${CMAKE_SOURCE_DIR}/WebCore/html --include ${CMAKE_SOURCE_DIR}/WebCore/xml --include ${CMAKE_SOURCE_DIR}/WebCore/ksvg2/svg --outputdir ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${idl}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${idl} ${CMAKE_CURRENT_SOURCE_DIR}/bindings/scripts/CodeGenerator.pm ${CMAKE_CURRENT_SOURCE_DIR}/bindings/scripts/CodeGeneratorJS.pm)
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


create_lut( WebCore-unity_LIB_SRCS bindings/js/kjs_css.cpp kjs_css.lut.h bindings/js/kjs_css.cpp )
create_lut( WebCore-unity_LIB_SRCS bindings/js/kjs_dom.cpp kjs_dom.lut.h bindings/js/kjs_dom.cpp )
create_lut( WebCore-unity_LIB_SRCS bindings/js/kjs_events.cpp kjs_events.lut.h bindings/js/kjs_events.cpp )
create_lut( WebCore-unity_LIB_SRCS bindings/js/kjs_html.cpp kjs_html.lut.h bindings/js/kjs_html.cpp )
create_lut( WebCore-unity_LIB_SRCS bindings/js/kjs_navigator.cpp kjs_navigator.lut.h bindings/js/kjs_navigator.cpp )
create_lut( WebCore-unity_LIB_SRCS bindings/js/kjs_traversal.cpp kjs_traversal.lut.h bindings/js/kjs_traversal.cpp )
create_lut( WebCore-unity_LIB_SRCS bindings/js/kjs_window.cpp kjs_window.lut.h bindings/js/kjs_window.cpp )
create_lut( WebCore-unity_LIB_SRCS bindings/js/JSXMLHttpRequest.cpp JSXMLHttpRequest.lut.h bindings/js/JSXMLHttpRequest.cpp )
create_lut( WebCore-unity_LIB_SRCS bindings/js/JSXSLTProcessor.cpp JSXSLTProcessor.lut.h bindings/js/JSXSLTProcessor.cpp )

# now this is ugly
# WebKit sources include some generated files which have the suffix ".cpp"
# the CMake way to create the dependencies of targets to generated files is
# to list the generated files as part of the sources for the target.
# If this would be done for the generated-and-then-included cpp-files
# they would be compiled twice (once included and once on their own).
# The easy way would be to rename the generated files to use the suffix
# ".inl" or something like this. But this is not wanted by the WebKit developers
# AFAIK. So we have to work around it here. This is done by renaming the
# generated files to *.inl and adding these files to the list of sources
# (WebCore-unity_LIB_SRCS). But the include-statements still want a cpp-file,
# so tell CMake to create these cpp-files at cmake-time, but these files contain
# nothing except an include-statement for the inl-file. Alex
macro(CREATE_GENERATED_CPP_HELPER  _inlName)
   get_filename_component(_baseName ${_inlName} NAME_WE)
   if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${_baseName}.cpp)
      file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${_baseName}.cpp
"/* This file is autogenerated by CMake.                                       *
 * Do not edit, changes will be lost.                                         *
 * With the CMake buildsystem giving generated-but-not-compiled-on-their-own  *
 * files the suffix \".cpp\" is not recommended, because it breaks the          *
 * dependency handling. So the actual generated file has the suffix \".inl\"    *
 * and this file here only exists as a helper.                                */
#include \"${CMAKE_CURRENT_BINARY_DIR}/${_inlName}\"\n")
   endif (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${_baseName}.cpp)
   set(WebCore-unity_LIB_SRCS ${WebCore-unity_LIB_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/${_inlName})
endmacro(CREATE_GENERATED_CPP_HELPER  _inlName)

create_cpp_lut( bindings/js/JSHTMLInputElementBase.cpp JSHTMLInputElementBaseTable.inl bindings/js/JSHTMLInputElementBase.cpp )
create_generated_cpp_helper( JSHTMLInputElementBaseTable.inl)

IF (WEBKIT_USE_SVG_SUPPORT)
    create_cpp_lut( bindings/js/JSSVGNumber.cpp JSSVGNumberTable.inl bindings/js/JSSVGNumber.cpp )
    create_generated_cpp_helper( JSSVGNumberTable.inl )

    create_cpp_lut( bindings/js/JSSVGPoint.cpp JSSVGPointTable.inl bindings/js/JSSVGPoint.cpp )
    create_generated_cpp_helper( JSSVGPointTable.inl )

    create_cpp_lut( bindings/js/JSSVGRect.cpp JSSVGRectTable.inl bindings/js/JSSVGRect.cpp )
    create_generated_cpp_helper( JSSVGRectTable.inl )
ENDIF (WEBKIT_USE_SVG_SUPPORT)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tokenizer.inl
  COMMAND flex -t < ${CMAKE_CURRENT_SOURCE_DIR}/css/tokenizer.flex | perl ${CMAKE_CURRENT_SOURCE_DIR}/css/maketokenizer > ${CMAKE_CURRENT_BINARY_DIR}/tokenizer.inl
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/css/tokenizer.flex
)
create_generated_cpp_helper( tokenizer.inl )

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

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/XPathGrammar.cpp ${CMAKE_CURRENT_BINARY_DIR}/XPathGrammar.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BISON_EXECUTABLE} -d -p xpathyy ${CMAKE_CURRENT_SOURCE_DIR}/xml/XPathGrammar.y
  COMMAND mv XPathGrammar.tab.c ${CMAKE_CURRENT_BINARY_DIR}/XPathGrammar.cpp
  #COMMAND rm -f XPathGrammar.tab.h
  #COMMAND touch XPathGrammar.h
  COMMAND mv XPathGrammar.tab.h ${CMAKE_CURRENT_BINARY_DIR}/XPathGrammar.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/xml/XPathGrammar.y
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

# source stuff
if (WEBKIT_USE_XPATH_SUPPORT)
SET (XPATH_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/XPathGrammar.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/XPathGrammar.h
)
ENDIF (WEBKIT_USE_XPATH_SUPPORT)

IF (WEBKIT_USE_SVG_SUPPORT)
set (SVG_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/misc/XLinkNames.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/svg/SVGElementFactory.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/ksvg2/svg/SVGNames.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/xml/XMLNames.cpp

    bindings/js/JSSVGElementWrapperFactory.cpp
    bindings/js/JSSVGNumber.cpp
    bindings/js/JSSVGPoint.cpp
    bindings/js/JSSVGRect.cpp

    ksvg2/css/SVGCSSParser.cpp
    ksvg2/css/SVGCSSStyleSelector.cpp
    ksvg2/css/SVGRenderStyle.cpp
    ksvg2/css/SVGRenderStyleDefs.cpp

    ksvg2/events/JSSVGLazyEventListener.cpp
    ksvg2/events/SVGZoomEvent.cpp

    ksvg2/misc/KCanvasRenderingStyle.cpp
    ksvg2/misc/KSVGTimeScheduler.cpp
    ksvg2/misc/SVGDocumentExtensions.cpp
    ksvg2/misc/SVGImageLoader.cpp

    ksvg2/svg/SVGAElement.cpp
    ksvg2/svg/SVGAngle.cpp
    ksvg2/svg/SVGAnimateColorElement.cpp
    ksvg2/svg/SVGAnimatedPathData.cpp
    ksvg2/svg/SVGAnimatedPoints.cpp
    ksvg2/svg/SVGAnimateElement.cpp
    ksvg2/svg/SVGAnimateMotionElement.cpp
    ksvg2/svg/SVGAnimateTransformElement.cpp
    ksvg2/svg/SVGAnimationElement.cpp
    ksvg2/svg/SVGCircleElement.cpp
    ksvg2/svg/SVGClipPathElement.cpp
    ksvg2/svg/SVGColor.cpp
    ksvg2/svg/SVGComponentTransferFunctionElement.cpp
    ksvg2/svg/SVGCursorElement.cpp
    ksvg2/svg/SVGDefsElement.cpp
    ksvg2/svg/SVGDescElement.cpp
    ksvg2/svg/SVGDocument.cpp
    ksvg2/svg/SVGDOMImplementation.cpp
    ksvg2/svg/SVGElement.cpp
#   ksvg2/svg/SVGElementInstance.cpp
#   ksvg2/svg/SVGElementInstanceList.cpp
    ksvg2/svg/SVGEllipseElement.cpp
    ksvg2/svg/SVGExternalResourcesRequired.cpp
    ksvg2/svg/SVGFEBlendElement.cpp
    ksvg2/svg/SVGFEColorMatrixElement.cpp
    ksvg2/svg/SVGFEComponentTransferElement.cpp
    ksvg2/svg/SVGFECompositeElement.cpp
    ksvg2/svg/SVGFEDiffuseLightingElement.cpp
    ksvg2/svg/SVGFEDisplacementMapElement.cpp
    ksvg2/svg/SVGFEDistantLightElement.cpp
    ksvg2/svg/SVGFEFloodElement.cpp
    ksvg2/svg/SVGFEFuncAElement.cpp
    ksvg2/svg/SVGFEFuncBElement.cpp
    ksvg2/svg/SVGFEFuncGElement.cpp
    ksvg2/svg/SVGFEFuncRElement.cpp
    ksvg2/svg/SVGFEGaussianBlurElement.cpp
    ksvg2/svg/SVGFEImageElement.cpp
    ksvg2/svg/SVGFELightElement.cpp
    ksvg2/svg/SVGFEMergeElement.cpp
    ksvg2/svg/SVGFEMergeNodeElement.cpp
    ksvg2/svg/SVGFEOffsetElement.cpp
    ksvg2/svg/SVGFEPointLightElement.cpp
    ksvg2/svg/SVGFESpecularLightingElement.cpp
    ksvg2/svg/SVGFESpotLightElement.cpp
    ksvg2/svg/SVGFETileElement.cpp
    ksvg2/svg/SVGFETurbulenceElement.cpp
    ksvg2/svg/SVGFilterElement.cpp
    ksvg2/svg/SVGFilterPrimitiveStandardAttributes.cpp
    ksvg2/svg/SVGFitToViewBox.cpp
    ksvg2/svg/SVGForeignObjectElement.cpp
    ksvg2/svg/SVGGElement.cpp
    ksvg2/svg/SVGGradientElement.cpp
    ksvg2/svg/SVGImageElement.cpp
    ksvg2/svg/SVGLangSpace.cpp
    ksvg2/svg/SVGLength.cpp
    ksvg2/svg/SVGLengthList.cpp
    ksvg2/svg/SVGLinearGradientElement.cpp
    ksvg2/svg/SVGLineElement.cpp
    ksvg2/svg/SVGLocatable.cpp
    ksvg2/svg/SVGMarkerElement.cpp
    ksvg2/svg/SVGMaskElement.cpp
    ksvg2/svg/SVGMatrix.cpp
    ksvg2/svg/SVGMetadataElement.cpp
    ksvg2/svg/SVGNumberList.cpp
    ksvg2/svg/SVGPaint.cpp
    ksvg2/svg/SVGPathElement.cpp
    ksvg2/svg/SVGParserUtilities.cpp
    ksvg2/svg/SVGPathSegArc.cpp
    ksvg2/svg/SVGPathSegClosePath.cpp
    ksvg2/svg/SVGPathSeg.cpp
    ksvg2/svg/SVGPathSegCurvetoCubic.cpp
    ksvg2/svg/SVGPathSegCurvetoCubicSmooth.cpp
    ksvg2/svg/SVGPathSegCurvetoQuadratic.cpp
    ksvg2/svg/SVGPathSegCurvetoQuadraticSmooth.cpp
    ksvg2/svg/SVGPathSegLineto.cpp
    ksvg2/svg/SVGPathSegLinetoHorizontal.cpp
    ksvg2/svg/SVGPathSegLinetoVertical.cpp
    ksvg2/svg/SVGPathSegList.cpp
    ksvg2/svg/SVGPathSegMoveto.cpp
    ksvg2/svg/SVGPatternElement.cpp
    ksvg2/svg/SVGPointList.cpp
    ksvg2/svg/SVGPolyElement.cpp
    ksvg2/svg/SVGPolygonElement.cpp
    ksvg2/svg/SVGPolylineElement.cpp
    ksvg2/svg/SVGPreserveAspectRatio.cpp
    ksvg2/svg/SVGRadialGradientElement.cpp
    ksvg2/svg/SVGRectElement.cpp
    ksvg2/svg/SVGScriptElement.cpp
    ksvg2/svg/SVGSetElement.cpp
    ksvg2/svg/SVGStopElement.cpp
    ksvg2/svg/SVGStringList.cpp
    ksvg2/svg/SVGStylable.cpp
    ksvg2/svg/SVGStyledElement.cpp
    ksvg2/svg/SVGStyledLocatableElement.cpp
    ksvg2/svg/SVGStyledTransformableElement.cpp
    ksvg2/svg/SVGStyleElement.cpp
    ksvg2/svg/SVGSVGElement.cpp
    ksvg2/svg/SVGSwitchElement.cpp
    ksvg2/svg/SVGSymbolElement.cpp
    ksvg2/svg/SVGTests.cpp
    ksvg2/svg/SVGTextContentElement.cpp
    ksvg2/svg/SVGTextElement.cpp
    ksvg2/svg/SVGTextPositioningElement.cpp
    ksvg2/svg/SVGTitleElement.cpp
    ksvg2/svg/SVGTransformable.cpp
    ksvg2/svg/SVGTransform.cpp
    ksvg2/svg/SVGTransformList.cpp
    ksvg2/svg/SVGTRefElement.cpp
    ksvg2/svg/SVGTSpanElement.cpp
    ksvg2/svg/SVGURIReference.cpp
    ksvg2/svg/SVGUseElement.cpp
    ksvg2/svg/SVGViewElement.cpp
    ksvg2/svg/SVGZoomAndPan.cpp

    rendering/RenderForeignObject.cpp
    rendering/RenderPath.cpp
    rendering/RenderSVGBlock.cpp
    rendering/RenderSVGContainer.cpp
    rendering/RenderSVGImage.cpp
    rendering/RenderSVGInline.cpp
    rendering/RenderSVGInlineText.cpp
    rendering/RenderSVGText.cpp
    rendering/RenderSVGTSpan.cpp
    rendering/SVGInlineFlowBox.cpp
    rendering/SVGRootInlineBox.cpp
)
ENDIF(WEBKIT_USE_SVG_SUPPORT)

set(WebCore_SRCS
    ${WebCore-unity_LIB_SRCS}

    # Generated files first
    ${CMAKE_CURRENT_BINARY_DIR}/CSSGrammar.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/CSSGrammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/CSSPropertyNames.h
    ${CMAKE_CURRENT_BINARY_DIR}/CSSValueKeywords.h
    ${CMAKE_CURRENT_BINARY_DIR}/html/DocTypeStrings.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/html/HTMLEntityNames.c
    ${CMAKE_CURRENT_BINARY_DIR}/html/HTMLNames.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/ksvgcssproperties.h
    ${CMAKE_CURRENT_BINARY_DIR}/ksvgcssvalues.h
    ${CMAKE_CURRENT_BINARY_DIR}/platform/ColorData.c
    ${CMAKE_CURRENT_BINARY_DIR}/UserAgentStyleSheetsData.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/UserAgentStyleSheets.h

    # Hand-written JavaScript bindings
    bindings/js/kjs_binding.cpp
    bindings/js/kjs_css.cpp
    bindings/js/kjs_dom.cpp
    bindings/js/kjs_events.cpp
    bindings/js/kjs_html.cpp
    bindings/js/kjs_navigator.cpp
    bindings/js/kjs_proxy.cpp
    bindings/js/kjs_traversal.cpp
    bindings/js/kjs_window.cpp

    # Custom JavaScript object implementations
    bindings/js/JSCanvasRenderingContext2DCustom.cpp
    bindings/js/JSHTMLElementWrapperFactory.cpp
    bindings/js/JSHTMLFormElementCustom.cpp
    bindings/js/JSHTMLInputElementBase.cpp
    bindings/js/JSHTMLOptionElementConstructor.cpp
    bindings/js/JSHTMLOptionsCollectionCustom.cpp
    bindings/js/JSNodeIteratorCustom.cpp
    bindings/js/JSTreeWalkerCustom.cpp
    bindings/js/JSXMLHttpRequest.cpp
    bindings/js/JSXSLTProcessor.cpp

    css/CSSBorderImageValue.cpp
    css/CSSCharsetRule.cpp
    css/CSSComputedStyleDeclaration.cpp
    css/CSSCursorImageValue.cpp
    css/CSSFontFaceRule.cpp
    css/csshelper.cpp
    css/CSSImageValue.cpp
    css/CSSImportRule.cpp
    css/CSSInheritedValue.cpp
    css/CSSInitialValue.cpp
    css/CSSMediaRule.cpp
    css/CSSMutableStyleDeclaration.cpp
    css/CSSPageRule.cpp
    css/cssparser.cpp
    css/CSSPrimitiveValue.cpp
    css/CSSProperty.cpp
    css/CSSRule.cpp
    css/CSSRuleList.cpp
    css/CSSSelector.cpp
    css/CSSStyleDeclaration.cpp
    css/CSSStyleRule.cpp
    css/cssstyleselector.cpp
    css/CSSStyleSheet.cpp
    css/CSSValueList.cpp
    css/FontFamilyValue.cpp
    css/FontValue.cpp
    css/MediaFeatureNames.cpp
    css/MediaList.cpp
    css/MediaQuery.cpp
    css/MediaQueryEvaluator.cpp
    css/MediaQueryExp.cpp
    css/ShadowValue.cpp
    css/StyleBase.cpp
    css/StyleList.cpp
    css/StyleSheet.cpp
    css/StyleSheetList.cpp

    dom/Attr.cpp
    dom/Attribute.cpp
    dom/BeforeTextInsertedEvent.cpp
    dom/BeforeUnloadEvent.cpp
    dom/CDATASection.cpp
    dom/CharacterData.cpp
    dom/ChildNodeList.cpp
    dom/ClipboardEvent.cpp
    dom/Comment.cpp
    dom/ContainerNode.cpp
    dom/CSSMappedAttributeDeclaration.cpp
    dom/Document.cpp
    dom/DocumentFragment.cpp
    dom/DocumentType.cpp
    dom/DOMImplementation.cpp
    dom/EditingText.cpp
    dom/Element.cpp
    dom/Entity.cpp
    dom/EntityReference.cpp
    dom/Event.cpp
    dom/EventNames.cpp
    dom/EventTargetNode.cpp
    dom/KeyboardEvent.cpp
    dom/MappedAttribute.cpp
    dom/MouseEvent.cpp
    dom/MouseRelatedEvent.cpp
    dom/MutationEvent.cpp
    dom/NamedAttrMap.cpp
    dom/NamedMappedAttrMap.cpp
    dom/NameNodeList.cpp
    dom/Node.cpp
    dom/NodeFilterCondition.cpp
    dom/NodeFilter.cpp
    dom/NodeIterator.cpp
    dom/NodeList.cpp
    dom/Notation.cpp
    dom/OverflowEvent.cpp
    dom/Position.cpp
    dom/ProcessingInstruction.cpp
    dom/QualifiedName.cpp
    dom/Range.cpp
    dom/RegisteredEventListener.cpp
    dom/StyleElement.cpp
    dom/StyledElement.cpp
    dom/Text.cpp
    dom/Traversal.cpp
    dom/TreeWalker.cpp
    dom/UIEvent.cpp
    dom/UIEventWithKeyState.cpp
    dom/WheelEvent.cpp
    dom/XMLTokenizer.cpp

    editing/AppendNodeCommand.cpp
    editing/ApplyStyleCommand.cpp
    editing/BreakBlockquoteCommand.cpp
    editing/CommandByName.cpp
    editing/CompositeEditCommand.cpp
    editing/CreateLinkCommand.cpp
    editing/DeleteButtonController.cpp
    editing/DeleteButton.cpp
    editing/DeleteFromTextNodeCommand.cpp
    editing/DeleteSelectionCommand.cpp
    editing/EditCommand.cpp
    editing/Editor.cpp
    editing/FormatBlockCommand.cpp
    editing/htmlediting.cpp
    editing/HTMLInterchange.cpp
    editing/IndentOutdentCommand.cpp
    editing/InsertIntoTextNodeCommand.cpp
    editing/InsertLineBreakCommand.cpp
    editing/InsertListCommand.cpp
    editing/InsertNodeBeforeCommand.cpp
    editing/InsertParagraphSeparatorCommand.cpp
    editing/InsertTextCommand.cpp
    editing/JoinTextNodesCommand.cpp
    editing/JSEditor.cpp
    editing/markup.cpp
    editing/MergeIdenticalElementsCommand.cpp
    editing/ModifySelectionListLevel.cpp
    editing/MoveSelectionCommand.cpp
    editing/RemoveCSSPropertyCommand.cpp
    editing/RemoveNodeAttributeCommand.cpp
    editing/RemoveNodeCommand.cpp
    editing/RemoveNodePreservingChildrenCommand.cpp
    editing/ReplaceSelectionCommand.cpp
    editing/SelectionController.cpp
    editing/Selection.cpp
    editing/SetNodeAttributeCommand.cpp
    editing/SplitElementCommand.cpp
    editing/SplitTextNodeCommand.cpp
    editing/SplitTextNodeContainingElementCommand.cpp
    editing/TextIterator.cpp
    editing/TypingCommand.cpp
    editing/UnlinkCommand.cpp
    editing/VisiblePosition.cpp
    editing/visible_units.cpp
    editing/WrapContentsInDummySpanCommand.cpp
    editing/qt/EditorQt.cpp

    html/CanvasGradient.cpp
    html/CanvasPattern.cpp
    html/CanvasRenderingContext2D.cpp
    html/CanvasStyle.cpp
    html/FormDataList.cpp
    html/HTMLAnchorElement.cpp
    html/HTMLAppletElement.cpp
    html/HTMLAreaElement.cpp
    html/HTMLBaseElement.cpp
    html/HTMLBaseFontElement.cpp
    html/HTMLBlockquoteElement.cpp
    html/HTMLBodyElement.cpp
    html/HTMLBRElement.cpp
    html/HTMLButtonElement.cpp
    html/HTMLCanvasElement.cpp
    html/HTMLCollection.cpp
    html/HTMLDirectoryElement.cpp
    html/HTMLDivElement.cpp
    html/HTMLDListElement.cpp
    html/HTMLDocument.cpp
    html/HTMLElement.cpp
    html/HTMLElementFactory.cpp
    html/HTMLEmbedElement.cpp
    html/HTMLFieldSetElement.cpp
    html/HTMLFontElement.cpp
    html/HTMLFormCollection.cpp
    html/HTMLFormElement.cpp
    html/HTMLFrameElementBase.cpp
    html/HTMLFrameElement.cpp
    html/HTMLFrameOwnerElement.cpp
    html/HTMLFrameSetElement.cpp
    html/HTMLGenericFormElement.cpp
    html/HTMLHeadElement.cpp
    html/HTMLHeadingElement.cpp
    html/HTMLHRElement.cpp
    html/HTMLHtmlElement.cpp
    html/HTMLIFrameElement.cpp
    html/HTMLImageElement.cpp
    html/HTMLImageLoader.cpp
    html/HTMLInputElement.cpp
    html/HTMLIsIndexElement.cpp
    html/HTMLKeygenElement.cpp
    html/HTMLLabelElement.cpp
    html/HTMLLegendElement.cpp
    html/HTMLLIElement.cpp
    html/HTMLLinkElement.cpp
    html/HTMLMapElement.cpp
    html/HTMLMarqueeElement.cpp
    html/HTMLMenuElement.cpp
    html/HTMLMetaElement.cpp
    html/HTMLModElement.cpp
    html/HTMLNameCollection.cpp
    html/HTMLObjectElement.cpp
    html/HTMLOListElement.cpp
    html/HTMLOptGroupElement.cpp
    html/HTMLOptionElement.cpp
    html/HTMLOptionsCollection.cpp
    html/HTMLParagraphElement.cpp
    html/HTMLParamElement.cpp
    html/HTMLParser.cpp
    html/HTMLPlugInElement.cpp
    html/HTMLPreElement.cpp
    html/HTMLQuoteElement.cpp
    html/HTMLScriptElement.cpp
    html/HTMLSelectElement.cpp
    html/HTMLStyleElement.cpp
    html/HTMLTableCaptionElement.cpp
    html/HTMLTableCellElement.cpp
    html/HTMLTableColElement.cpp
    html/HTMLTableElement.cpp
    html/HTMLTablePartElement.cpp
    html/HTMLTableRowElement.cpp
    html/HTMLTableSectionElement.cpp
    html/HTMLTextAreaElement.cpp
    html/HTMLTextFieldInnerElement.cpp
    html/HTMLTitleElement.cpp
    html/HTMLTokenizer.cpp
    html/HTMLUListElement.cpp
    html/HTMLViewSourceDocument.cpp

#   icon/IconDatabase.cpp
#   icon/SiteIcon.cpp
#   icon/SQLDatabase.cpp
#   icon/SQLStatement.cpp
#   icon/SQLTransaction.cpp

    loader/Cache.cpp
    loader/CachedCSSStyleSheet.cpp
    loader/CachedImage.cpp
    loader/CachedResourceClientWalker.cpp
    loader/CachedResource.cpp
    loader/CachedScript.cpp
    loader/CachedXSLStyleSheet.cpp
    loader/DocLoader.cpp
    loader/DocumentLoader.cpp
    loader/FormState.cpp
    loader/FrameLoader.cpp
    loader/ImageDocument.cpp
    loader/loader.cpp
    loader/MainResourceLoader.cpp
    loader/NetscapePlugInStreamLoader.cpp
    loader/PluginDocument.cpp
    loader/Request.cpp
    loader/ResourceLoader.cpp
    loader/SubresourceLoader.cpp
    loader/TextDocument.cpp
    loader/TextResourceDecoder.cpp
    loader/icon/IconDatabase.cpp
    loader/icon/IconDataCache.cpp
    loader/icon/IconLoader.cpp
    loader/icon/SQLDatabase.cpp
    loader/icon/SQLStatement.cpp
    loader/icon/SQLTransaction.cpp
    loader/qt/DocumentLoaderQt.cpp
    loader/qt/FrameLoaderClientQt.cpp
    loader/qt/FrameLoaderQt.cpp
    loader/qt/NavigationActionQt.cpp
    loader/qt/ResourceLoaderQt.cpp

    page/Chrome.cpp
    page/ContextMenuController.cpp
    page/DOMWindow.cpp
    page/EventHandler.cpp
    page/Frame.cpp
    page/FrameTree.cpp
    page/FrameView.cpp
    page/MouseEventWithHitTestResults.cpp
    page/Page.cpp
    page/PageState.cpp

    page/qt/EventHandlerQt.cpp

    platform/Arena.cpp
    platform/ArrayImpl.cpp
    platform/AtomicString.cpp
    platform/CString.cpp
    platform/ContextMenu.cpp
    platform/DeprecatedCString.cpp
    platform/DeprecatedPtrListImpl.cpp
    platform/DeprecatedString.cpp
    platform/DeprecatedStringList.cpp
    platform/DeprecatedValueListImpl.cpp
    platform/FontCache.cpp
    platform/Font.cpp
    platform/FontData.cpp
    platform/FontFallbackList.cpp
    platform/FontFamily.cpp
    platform/GlyphMap.cpp
    platform/GlyphWidthMap.cpp
    platform/KURL.cpp
    platform/Logging.cpp
    platform/MimeTypeRegistry.cpp
#    plafform/Pasteboard.cpp
    platform/RegularExpression.cpp
    platform/SegmentedString.cpp
    platform/String.cpp
    platform/StringImpl.cpp
    platform/TextCodec.cpp
    platform/TextCodecLatin1.cpp
    platform/TextCodecUTF16.cpp
    platform/TextDecoder.cpp
    platform/TextEncoding.cpp
    platform/TextEncodingRegistry.cpp
    platform/TextStream.cpp
    platform/Timer.cpp
    platform/Widget.cpp

    platform/graphics/AffineTransform.cpp
    platform/graphics/Color.cpp
    platform/graphics/FloatPoint3D.cpp
    platform/graphics/FloatPoint.cpp
    platform/graphics/FloatRect.cpp
    platform/graphics/FloatSize.cpp
    platform/graphics/GraphicsContext.cpp
    platform/graphics/GraphicsTypes.cpp
    platform/graphics/Image.cpp
    platform/graphics/ImageBuffer.cpp
    platform/graphics/IntRect.cpp
    platform/graphics/Path.cpp
    platform/graphics/Pen.cpp

    platform/graphics/qt/AffineTransformQt.cpp
    platform/graphics/qt/ColorQt.cpp
    platform/graphics/qt/FloatPointQt.cpp
    platform/graphics/qt/FloatRectQt.cpp
    platform/graphics/qt/GraphicsContextQt.cpp
    platform/graphics/qt/IconQt.cpp
    platform/graphics/qt/ImageQt.cpp
    platform/graphics/qt/ImageBufferQt.cpp 
    platform/graphics/qt/ImageSourceQt.cpp
    platform/graphics/qt/ImageDecoderQt.cpp
    platform/graphics/qt/IntPointQt.cpp
    platform/graphics/qt/IntRectQt.cpp
    platform/graphics/qt/IntSizeQt.cpp
    platform/graphics/qt/PathQt.cpp

    platform/graphics/svg/SVGPaintServer.cpp
    platform/graphics/svg/SVGPaintServerGradient.cpp
    platform/graphics/svg/SVGPaintServerLinearGradient.cpp
    platform/graphics/svg/SVGPaintServerPattern.cpp
    platform/graphics/svg/SVGPaintServerRadialGradient.cpp
    platform/graphics/svg/SVGPaintServerSolid.cpp
    platform/graphics/svg/SVGResource.cpp
    platform/graphics/svg/SVGResourceClipper.cpp
    platform/graphics/svg/SVGResourceFilter.cpp
    platform/graphics/svg/SVGResourceMarker.cpp
    platform/graphics/svg/SVGResourceMasker.cpp

    platform/graphics/svg/filters/SVGFEBlend.cpp
    platform/graphics/svg/filters/SVGFEColorMatrix.cpp
    platform/graphics/svg/filters/SVGFEComponentTransfer.cpp
    platform/graphics/svg/filters/SVGFEComposite.cpp
    platform/graphics/svg/filters/SVGFEConvolveMatrix.cpp
    platform/graphics/svg/filters/SVGFEDiffuseLighting.cpp
    platform/graphics/svg/filters/SVGFEDisplacementMap.cpp
    platform/graphics/svg/filters/SVGFEFlood.cpp
    platform/graphics/svg/filters/SVGFEGaussianBlur.cpp
    platform/graphics/svg/filters/SVGFEImage.cpp
    platform/graphics/svg/filters/SVGFEMerge.cpp
    platform/graphics/svg/filters/SVGFEMorphology.cpp
    platform/graphics/svg/filters/SVGFEOffset.cpp
    platform/graphics/svg/filters/SVGFESpecularLighting.cpp
    platform/graphics/svg/filters/SVGFETurbulence.cpp
    platform/graphics/svg/filters/SVGFilterEffect.cpp
    platform/graphics/svg/filters/SVGLightSource.cpp

    platform/graphics/svg/qt/RenderPathQt.cpp
    platform/graphics/svg/qt/SVGPaintServerGradientQt.cpp
    platform/graphics/svg/qt/SVGPaintServerLinearGradientQt.cpp
    platform/graphics/svg/qt/SVGPaintServerPatternQt.cpp
    platform/graphics/svg/qt/SVGPaintServerQt.cpp
    platform/graphics/svg/qt/SVGPaintServerRadialGradientQt.cpp
    platform/graphics/svg/qt/SVGPaintServerSolidQt.cpp
    platform/graphics/svg/qt/SVGResourceClipperQt.cpp
    platform/graphics/svg/qt/SVGResourceMaskerQt.cpp
    platform/graphics/svg/qt/SVGResourceFilterQt.cpp

    platform/image-decoders/bmp/BMPImageDecoder.cpp
    platform/image-decoders/gif/GIFImageDecoder.cpp
    platform/image-decoders/gif/GIFImageReader.cpp
    platform/image-decoders/ico/ICOImageDecoder.cpp

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
    platform/image-decoders/jpeg/JPEGImageDecoder.cpp
    platform/image-decoders/jpeg/jquant1.c
    platform/image-decoders/jpeg/jquant2.c
    platform/image-decoders/jpeg/jutils.c

    platform/image-decoders/png/png.c
    platform/image-decoders/png/pngerror.c
    platform/image-decoders/png/pnggccrd.c
    platform/image-decoders/png/pngget.c
    platform/image-decoders/png/PNGImageDecoder.cpp
    platform/image-decoders/png/pngmem.c
    platform/image-decoders/png/pngpread.c
    platform/image-decoders/png/pngread.c
    platform/image-decoders/png/pngrio.c
    platform/image-decoders/png/pngrtran.c
    platform/image-decoders/png/pngrutil.c
    platform/image-decoders/png/pngset.c
    platform/image-decoders/png/pngtrans.c
    platform/image-decoders/png/pngvcrd.c
    platform/image-decoders/png/pngwio.c
    platform/image-decoders/png/pngwrite.c
    platform/image-decoders/png/pngwtran.c
    platform/image-decoders/png/pngwutil.c

    platform/image-decoders/xbm/XBMImageDecoder.cpp

    platform/network/FormData.cpp
    platform/network/HTTPParsers.cpp
    platform/network/ResourceHandle.cpp
    platform/network/ResourceRequest.cpp
    platform/network/ResourceResponse.cpp

    platform/network/qt/ResourceHandleManager.cpp
    platform/network/qt/ResourceHandleQt.cpp

    platform/qt/ChromeClientQt.cpp
    platform/qt/ContextMenuClientQt.cpp
    platform/qt/ContextMenuQt.cpp
    platform/qt/ContextMenuItemQt.cpp
    platform/qt/CookieJarQt.cpp
    platform/qt/CursorQt.cpp
    platform/qt/EditorClientQt.cpp
    platform/qt/FileChooserQt.cpp
    platform/qt/FontCacheQt.cpp
    platform/qt/FontDataQt.cpp
    platform/qt/FontPlatformDataQt.cpp
    platform/qt/FontQt.cpp
    platform/qt/FrameQtClient.cpp
    platform/qt/FrameQt.cpp
    platform/qt/GlyphMapQt.cpp
#   platform/qt/LineEditQt.cpp
    platform/qt/LoaderFunctionsQt.cpp
    platform/qt/MimeTypeRegistryQt.cpp
    platform/qt/PageQt.cpp
    platform/qt/PasteboardQt.cpp
    platform/qt/PlatformKeyboardEventQt.cpp
    platform/qt/PlatformMouseEventQt.cpp
    platform/qt/PopupMenuQt.cpp
    platform/qt/RenderThemeQt.cpp
    platform/qt/ScreenQt.cpp
    platform/qt/ScrollViewCanvasQt.cpp
    platform/qt/ScrollViewQt.cpp
    platform/qt/SharedTimerQt.cpp
    platform/qt/SoundQt.cpp
    platform/qt/StringQt.cpp
    platform/qt/SystemTimeQt.cpp
    platform/qt/TemporaryLinkStubs.cpp
    platform/qt/TextBoundaries.cpp
    platform/qt/TextBreakIteratorQt.cpp
    platform/qt/TextCodecQt.cpp
#   platform/qt/TextEditQt.cpp
    platform/qt/WidgetQt.cpp

    xml/DOMParser.cpp
    xml/xmlhttprequest.cpp
    xml/XMLSerializer.cpp
    xml/XPathEvaluator.cpp
    xml/XPathExpression.cpp
    xml/XPathExpressionNode.cpp
    xml/XPathFunctions.cpp
    xml/XPathNamespace.cpp
    xml/XPathNSResolver.cpp
    xml/XPathParser.cpp
    xml/XPathPath.cpp
    xml/XPathPredicate.cpp
    xml/XPathResult.cpp
    xml/XPathStep.cpp
    xml/XPathUtil.cpp
    xml/XPathValue.cpp
    xml/XPathVariableReference.cpp
    xml/XSLImportRule.cpp
    xml/XSLStyleSheet.cpp
    xml/XSLTProcessor.cpp

    rendering/AutoTableLayout.cpp
    rendering/bidi.cpp
    rendering/break_lines.cpp
    rendering/CounterNode.cpp
    rendering/CounterResetNode.cpp
    rendering/EllipsisBox.cpp
    rendering/FixedTableLayout.cpp
    rendering/HitTestResult.cpp
    rendering/InlineBox.cpp
    rendering/InlineFlowBox.cpp
    rendering/InlineTextBox.cpp
    rendering/ListMarkerBox.cpp
    rendering/RenderApplet.cpp
    rendering/RenderArena.cpp
    rendering/RenderBlock.cpp
    rendering/RenderBox.cpp
    rendering/RenderBR.cpp
    rendering/RenderButton.cpp
    rendering/RenderContainer.cpp
    rendering/RenderCounter.cpp
    rendering/RenderFieldset.cpp
    rendering/RenderFileUploadControl.cpp
    rendering/RenderFlexibleBox.cpp
    rendering/RenderFlow.cpp
    rendering/RenderFormElement.cpp
    rendering/RenderFrame.cpp
    rendering/RenderFrameSet.cpp
    rendering/RenderHTMLCanvas.cpp
    rendering/RenderImage.cpp
    rendering/RenderInline.cpp
    rendering/RenderLabel.cpp
    rendering/RenderLayer.cpp
    rendering/RenderLegend.cpp
    rendering/RenderListBox.cpp
    rendering/RenderListItem.cpp
    rendering/RenderListMarker.cpp
    rendering/RenderMenuList.cpp
    rendering/RenderObject.cpp
    rendering/RenderPart.cpp
    rendering/RenderPartObject.cpp
    rendering/RenderReplaced.cpp
    rendering/RenderSlider.cpp
    rendering/RenderStyle.cpp
    rendering/RenderTableCell.cpp
    rendering/RenderTableCol.cpp
    rendering/RenderTable.cpp
    rendering/RenderTableRow.cpp
    rendering/RenderTableSection.cpp
    rendering/RenderTextControl.cpp
    rendering/RenderText.cpp
    rendering/RenderTextFragment.cpp
    rendering/RenderTheme.cpp
    rendering/RenderTreeAsText.cpp
    rendering/RenderView.cpp
    rendering/RenderWidget.cpp
    rendering/RootInlineBox.cpp
    rendering/SVGRenderTreeAsText.cpp

    # Optional SVG/XPath support
    ${SVG_SOURCES}
    ${XPATH_SOURCES}

    # Generated JavaScript bindings
    ${WebCore-unity-IDLGeneratedSources}
)

IF (WEBKIT_USE_KDE_SUPPORT)
    kde4_add_library( WebCore-unity SHARED  ${WebCore_SRCS} )
ELSE (WEBKIT_USE_KDE_SUPPORT)
    add_library( WebCore-unity SHARED  ${WebCore_SRCS} )
ENDIF (WEBKIT_USE_KDE_SUPPORT)

IF (WEBKIT_USE_KDE_SUPPORT)
target_link_libraries(WebCore-unity
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}

    ${QT_QTCORE_LIBRARY}
    ${QT_QTGUI_LIBRARY}

    icuuc
    kio
    kjs-unity
    pcre-unity
    sqlite3
    wtf-unity
    z
)
ELSE (WEBKIT_USE_KDE_SUPPORT)
target_link_libraries(WebCore-unity
    ${LIBXSLT_LIBRARIES}
    ${LIBXML2_LIBRARIES}

    ${QT_QTCORE_LIBRARY}
    ${QT_QTGUI_LIBRARY}

    icuuc
    kjs-unity
    pcre-unity
    sqlite3
    wtf-unity
    z
)
ENDIF (WEBKIT_USE_KDE_SUPPORT)

IF (WEBKIT_USE_KDE_SUPPORT)
    install( TARGETS WebCore-unity DESTINATION ${LIB_INSTALL_DIR} )
ENDIF (WEBKIT_USE_KDE_SUPPORT)
