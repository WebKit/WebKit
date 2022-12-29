list(APPEND WebKit_UNIFIED_SOURCE_LIST_FILES
    "SourcesGTKDeprecated.txt"
)

add_definitions(-DWEBKIT_DOM_USE_UNSTABLE_API)

file(MAKE_DIRECTORY ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit2)

list(APPEND WebKitGTK_HEADER_TEMPLATES
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitMimeInfo.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitPlugin.h.in
)

list(APPEND WebKitWebExtension_HEADER_TEMPLATES
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitConsoleMessage.h.in
)

set(WebKitDOM_INSTALLED_HEADERS
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/webkitdomautocleanups.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/webkitdomdefines.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/webkitdom.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMAttr.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMBlob.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCDATASection.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCharacterData.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMClientRect.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMClientRectList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMComment.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSRule.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSRuleList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSStyleDeclaration.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSStyleSheet.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCSSValue.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCustom.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMCustomUnstable.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDeprecated.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocument.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentFragment.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentFragmentUnstable.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentType.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDocumentUnstable.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMImplementation.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMSelection.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMTokenList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMWindow.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMDOMWindowUnstable.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMElementUnstable.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMEvent.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMEventTarget.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMFile.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMFileList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAnchorElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAppletElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLAreaElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBaseElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBodyElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLBRElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLButtonElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLCanvasElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLCollection.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDirectoryElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDivElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDListElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLDocument.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLElementUnstable.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLEmbedElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFieldSetElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFontElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFormElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFrameElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLFrameSetElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHeadElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHeadingElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHRElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLHtmlElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLIFrameElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLImageElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLInputElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLabelElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLegendElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLIElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLLinkElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMapElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMarqueeElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMenuElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLMetaElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLModElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLObjectElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOListElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptGroupElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptionElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLOptionsCollection.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLParagraphElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLParamElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLPreElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLQuoteElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLScriptElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLSelectElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLStyleElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableCaptionElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableCellElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableColElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableRowElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTableSectionElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTextAreaElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLTitleElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMHTMLUListElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMKeyboardEvent.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMMediaList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMMouseEvent.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNamedNodeMap.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNode.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeFilter.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeIterator.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMNodeList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMObject.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMProcessingInstruction.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMRange.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMRangeUnstable.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMStyleSheet.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMStyleSheetList.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMText.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMTreeWalker.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMUIEvent.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMWheelEvent.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathExpression.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathNSResolver.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM/WebKitDOMXPathResult.h
)

set(WebKitDOM_SOURCES_FOR_INTROSPECTION
    ${WebKitDOM_INSTALLED_HEADERS}
    WebProcess/InjectedBundle/API/glib/DOM
)

list(APPEND WebKitGTK_FAKE_API_HEADERS
    ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit2/webkit2.h
    ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit2/webkit-web-extension.h
    ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkitgtk-webextension/webkitdom
)

list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/DOM"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM"
)

install(FILES ${WebKitDOM_INSTALLED_HEADERS}
    DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/webkitdom"
)

# For GTK 3 builds, we have to maintain webkit2/webkit2.h and webkit2/webkit-web-extension.h for API
# compatibility. These are the only headers still installed under webkit2/. Install them manually.
install(FILES ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit2/webkit2.h
              ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit2/webkit-web-extension.h
    DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/webkit2"
)

add_custom_command(
    OUTPUT ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit2/webkit2.h
    DEPENDS ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/webkit.h
    COMMAND ${CMAKE_COMMAND} -E copy ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/webkit.h ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit2/webkit2.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit2/webkit-web-extension.h
    DEPENDS ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/webkit-web-extension.h
    COMMAND ${CMAKE_COMMAND} -E copy ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit/webkit-web-extension.h ${WebKitGTK_DERIVED_SOURCES_DIR}/webkit2/
    VERBATIM
)

add_custom_command(
    OUTPUT ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkitgtk-webextension/webkitdom
    DEPENDS ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM
    COMMAND ln -n -s -f ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/gtk/DOM ${WebKitGTK_FRAMEWORK_HEADERS_DIR}/webkitgtk-webextension/webkitdom
)
