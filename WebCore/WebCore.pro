# -*- Mode:makefile -*-
# WebCore - qmake build info
CONFIG += building-libs
include($$PWD/../WebKit.pri)
gdk-port:LIBS -= -lWebKitGdk

TEMPLATE = lib
qt-port:TARGET = QtWebKit
gdk-port:TARGET = WebKitGdk
OBJECTS_DIR = tmp
INCLUDEPATH += tmp $$OUTPUT_DIR/WebCore/tmp

DESTDIR = $$OUTPUT_DIR/lib
DEPENDPATH += css dom loader editing history html \
	loader page platform platform/graphics rendering xml

include($$OUTPUT_DIR/config.pri)

CONFIG -= warn_on
QMAKE_CXXFLAGS += -Wreturn-type
#QMAKE_CXXFLAGS += -Wall -Wno-undef -Wno-unused-parameter

qt-port:contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols
unix:contains(QT_CONFIG, reduce_relocations):CONFIG += bsymbolic_functions

linux-*: DEFINES += HAVE_STDINT_H
freebsd-*: DEFINES += HAVE_PTHREAD_NP_H

# PRE-BUILD: make the required config.h file
#config_h.target = config.h
#config_h.commands = cp config.h.qmake config.h
#config_h.depends = config.h.qmake
#QMAKE_EXTRA_TARGETS += config_h
#PRE_TARGETDEPS += config.h

DEFINES += BUILD_WEBKIT

# Optional components (look for defs in config.h and included files!)
DEFINES += ENABLE_XPATH=1
DEFINES += ENABLE_XSLT=1
#DEFINES += ENABLE_XBL=1
qt-port:DEFINES += ENABLE_SVG=1
gdk-port:DEFINES += ENABLE_SVG=1

DEFINES += WTF_CHANGES=1

include($$PWD/../JavaScriptCore/JavaScriptCore.pri)

#INCLUDEPATH += $$PWD/../JavaScriptCore
#LIBS += -L$$OUTPUT_DIR/lib -lJavaScriptCore

macx {
    INCLUDEPATH += /usr/include/libxml2
    LIBS += -lxml2 -lxslt
}

qt-port {
INCLUDEPATH += \
                $$[QT_INSTALL_PREFIX]/src/3rdparty/sqlite/ \
                $$PWD/platform/qt \
                $$PWD/platform/network/qt \
                $$PWD/platform/graphics/qt \
                $$PWD/platform/graphics/svg/qt \
                $$PWD/loader/qt \
                $$PWD/page/qt \
                $$PWD/../WebKitQt/WebCoreSupport \
                $$PWD/../WebKitQt/Api

DEPENDPATH += editing/qt history/qt loader/qt page/qt \
	platform/graphics/qt ../WeKitQt/Api ../WebKitQt/WebCoreSupport
}

gdk-port:INCLUDEPATH += \
    $$PWD/platform/graphics/svg/cairo \
    $$PWD/platform/image-decoders/bmp \
    $$PWD/platform/image-decoders/gif \
    $$PWD/platform/image-decoders/ico \
    $$PWD/platform/image-decoders/jpeg \
    $$PWD/platform/image-decoders/png \
    $$PWD/platform/image-decoders/xbm

INCLUDEPATH +=  $$PWD \
                $$PWD/ForwardingHeaders \
                $$PWD/../JavaScriptCore/kjs \
                $$PWD/../JavaScriptCore/bindings \
                $$PWD/platform \
                $$PWD/platform/network \
                $$PWD/platform/graphics \
                $$PWD/platform/graphics/svg \
                $$PWD/platform/graphics/svg/filters \
                $$PWD/loader $$PWD/loader/icon \
                $$PWD/css \
                $$PWD/dom \
                $$PWD/page \
                $$PWD/bridge \
                $$PWD/editing \
                $$PWD/rendering \
                $$PWD/history \
                $$PWD/xml \
                $$PWD/html \
                $$PWD/bindings/js \
                $$PWD/ksvg2 $$PWD/ksvg2/css $$PWD/ksvg2/svg $$PWD/ksvg2/misc $$PWD/ksvg2/events \
                $$PWD/platform/image-decoders \
                $$PWD/../WebKitQt/WebCoreSupport
QT += network
LIBS += -lsqlite3


FEATURE_DEFINES_JAVASCRIPT = LANGUAGE_JAVASCRIPT

TOKENIZER = $$PWD/css/tokenizer.flex

CSSBISON = $$PWD/css/CSSGrammar.y

HTML_NAMES = $$PWD/html/HTMLTagNames.in

XML_NAMES = $$PWD/xml/xmlattrs.in

ENTITIES_GPERF = $$PWD/html/HTMLEntityNames.gperf

COLORDAT_GPERF = $$PWD/platform/ColorData.gperf

WALDOCSSPROPS = $$PWD/css/CSSPropertyNames.in

WALDOCSSVALUES = $$PWD/css/CSSValueKeywords.in

SVGCSSPROPERTIES = $$PWD/ksvg2/css/CSSPropertyNames.in

SVGCSSVALUES = $$PWD/ksvg2/css/CSSValueKeywords.in

STYLESHEETS_EMBED = $$PWD/css/html4.css

LUT_FILES += \
    bindings/js/JSDOMExceptionConstructor.cpp \
    bindings/js/JSEventTargetNode.cpp \
    bindings/js/JSXMLHttpRequest.cpp \
    bindings/js/JSXSLTProcessor.cpp \
    bindings/js/kjs_css.cpp \
    bindings/js/kjs_events.cpp \
    bindings/js/kjs_navigator.cpp \
    bindings/js/kjs_window.cpp

LUT_TABLE_FILES += \
    bindings/js/JSHTMLInputElementBase.cpp

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
    css/MediaList.idl \
    css/Rect.idl \
    css/StyleSheet.idl \
    dom/Attr.idl \
    dom/CharacterData.idl \
    dom/CDATASection.idl \
    dom/Comment.idl \
    dom/DocumentFragment.idl \
    dom/Document.idl \
    dom/DocumentType.idl \
    dom/DOMImplementation.idl \
    dom/Element.idl \
    dom/Entity.idl \
    dom/EntityReference.idl \
    dom/Event.idl \
#    dom/EventListener.idl \
#    dom/EventTarget.idl \
    dom/KeyboardEvent.idl \
    dom/MouseEvent.idl \
    dom/MutationEvent.idl \
    dom/NamedNodeMap.idl \
    dom/Node.idl \
    dom/NodeFilter.idl \
    dom/NodeIterator.idl \
    dom/NodeList.idl \
    dom/Notation.idl \
    dom/OverflowEvent.idl \
    dom/ProcessingInstruction.idl \
    dom/RangeException.idl \
    dom/Range.idl \
    dom/Text.idl \
    dom/TextEvent.idl \
    dom/TreeWalker.idl \
    dom/UIEvent.idl \
    dom/WheelEvent.idl \
    html/CanvasGradient.idl \
    html/CanvasPattern.idl \
    html/CanvasRenderingContext2D.idl \
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
    page/DOMWindow.idl \
    xml/DOMParser.idl \
    xml/XMLSerializer.idl


SOURCES += \
    bindings/js/JSCanvasRenderingContext2DCustom.cpp \
    bindings/js/JSCSSRuleCustom.cpp \
    bindings/js/JSCSSStyleDeclarationCustom.cpp \
    bindings/js/JSCSSValueCustom.cpp \
    bindings/js/JSCustomXPathNSResolver.cpp \
    bindings/js/JSDocumentCustom.cpp \
    bindings/js/JSDOMExceptionConstructor.cpp \
    bindings/js/JSDOMWindowCustom.cpp \
    bindings/js/JSEventTargetNode.cpp \
    bindings/js/JSHTMLAppletElementCustom.cpp \
    bindings/js/JSHTMLCollectionCustom.cpp \
    bindings/js/JSHTMLDocumentCustom.cpp \
    bindings/js/JSHTMLElementCustom.cpp \
    bindings/js/JSHTMLElementWrapperFactory.cpp \
    bindings/js/JSHTMLEmbedElementCustom.cpp \
    bindings/js/JSHTMLFormElementCustom.cpp \
    bindings/js/JSHTMLFrameSetElementCustom.cpp \
    bindings/js/JSHTMLInputElementBase.cpp \
    bindings/js/JSHTMLObjectElementCustom.cpp \
    bindings/js/JSHTMLOptionElementConstructor.cpp \
    bindings/js/JSHTMLOptionsCollectionCustom.cpp \
    bindings/js/JSHTMLSelectElementCustom.cpp \
    bindings/js/JSNamedNodeMapCustom.cpp \
    bindings/js/JSNamedNodesCollection.cpp  \
    bindings/js/JSNodeCustom.cpp \
    bindings/js/JSNodeFilterCondition.cpp \
    bindings/js/JSNodeFilterCustom.cpp \
    bindings/js/JSNodeIteratorCustom.cpp \
    bindings/js/JSNodeListCustom.cpp \
    bindings/js/JSStyleSheetCustom.cpp \
    bindings/js/JSTreeWalkerCustom.cpp \
    bindings/js/JSXMLHttpRequest.cpp \
    bindings/js/JSXSLTProcessor.cpp \
    bindings/js/kjs_binding.cpp \
    bindings/js/kjs_css.cpp \
    bindings/js/kjs_dom.cpp \
    bindings/js/kjs_events.cpp \
    bindings/js/kjs_html.cpp \
    bindings/js/kjs_navigator.cpp \
    bindings/js/kjs_proxy.cpp \
    bindings/js/kjs_window.cpp \
    css/CSSBorderImageValue.cpp \
    css/CSSCharsetRule.cpp \
    css/CSSComputedStyleDeclaration.cpp \
    css/CSSCursorImageValue.cpp \
    css/CSSFontFaceRule.cpp \
    css/csshelper.cpp \
    css/CSSImageValue.cpp \
    css/CSSImportRule.cpp \
    css/CSSInheritedValue.cpp \
    css/CSSInitialValue.cpp \
    css/CSSMediaRule.cpp \
    css/CSSMutableStyleDeclaration.cpp \
    css/CSSPageRule.cpp \
    css/cssparser.cpp \
    css/CSSPrimitiveValue.cpp \
    css/CSSProperty.cpp \
    css/CSSRule.cpp \
    css/CSSRuleList.cpp \
    css/CSSSelector.cpp \
    css/CSSStyleDeclaration.cpp \
    css/CSSStyleRule.cpp \
    css/cssstyleselector.cpp \
    css/CSSStyleSheet.cpp \
    css/CSSValueList.cpp \
    css/FontFamilyValue.cpp \
    css/FontValue.cpp \
    css/MediaFeatureNames.cpp \
    css/MediaList.cpp \
    css/MediaQuery.cpp \
    css/MediaQueryEvaluator.cpp \
    css/MediaQueryExp.cpp \
    css/ShadowValue.cpp \
    css/StyleBase.cpp \
    css/StyleList.cpp \
    css/StyleSheet.cpp \
    css/StyleSheetList.cpp \
    dom/Attr.cpp \
    dom/Attribute.cpp \
    dom/BeforeTextInsertedEvent.cpp \
    dom/BeforeUnloadEvent.cpp \
    dom/CDATASection.cpp \
    dom/CharacterData.cpp \
    dom/ChildNodeList.cpp \
    dom/Clipboard.cpp \
    dom/ClipboardEvent.cpp \
    dom/Comment.cpp \
    dom/ContainerNode.cpp \
    dom/CSSMappedAttributeDeclaration.cpp \
    dom/Document.cpp \
    dom/DocumentFragment.cpp \
    dom/DocumentType.cpp \
    dom/DOMImplementation.cpp \
    dom/EditingText.cpp \
    dom/Element.cpp \
    dom/Entity.cpp \
    dom/EntityReference.cpp \
    dom/Event.cpp \
    dom/EventNames.cpp \
    dom/EventTarget.cpp \
    dom/EventTargetNode.cpp \
    dom/KeyboardEvent.cpp \
    dom/MappedAttribute.cpp \
    dom/MouseEvent.cpp \
    dom/MouseRelatedEvent.cpp \
    dom/MutationEvent.cpp \
    dom/NamedAttrMap.cpp \
    dom/NamedMappedAttrMap.cpp \
    dom/NameNodeList.cpp \
    dom/Node.cpp \
    dom/NodeFilterCondition.cpp \
    dom/NodeFilter.cpp \
    dom/NodeIterator.cpp \
    dom/NodeList.cpp \
    dom/Notation.cpp \
    dom/OverflowEvent.cpp \
    dom/Position.cpp \
    dom/PositionIterator.cpp \
    dom/ProcessingInstruction.cpp \
    dom/QualifiedName.cpp \
    dom/Range.cpp \
    dom/RegisteredEventListener.cpp \
    dom/StyledElement.cpp \
    dom/StyleElement.cpp \
    dom/Text.cpp \
    dom/TextEvent.cpp \
    dom/Traversal.cpp \
    dom/TreeWalker.cpp \
    dom/UIEvent.cpp \
    dom/UIEventWithKeyState.cpp \
    dom/WheelEvent.cpp \
    dom/XMLTokenizer.cpp \
    editing/AppendNodeCommand.cpp \
    editing/ApplyStyleCommand.cpp \
    editing/BreakBlockquoteCommand.cpp \
    editing/CommandByName.cpp \
    editing/CompositeEditCommand.cpp \
    editing/CreateLinkCommand.cpp \
    editing/DeleteButtonController.cpp \
    editing/DeleteButton.cpp \
    editing/DeleteFromTextNodeCommand.cpp \
    editing/DeleteSelectionCommand.cpp \
    editing/EditCommand.cpp \
    editing/Editor.cpp \
    editing/FormatBlockCommand.cpp \
    editing/htmlediting.cpp \
    editing/HTMLInterchange.cpp \
    editing/IndentOutdentCommand.cpp \
    editing/InsertIntoTextNodeCommand.cpp \
    editing/InsertLineBreakCommand.cpp \
    editing/InsertListCommand.cpp \
    editing/InsertNodeBeforeCommand.cpp \
    editing/InsertParagraphSeparatorCommand.cpp \
    editing/InsertTextCommand.cpp \
    editing/JoinTextNodesCommand.cpp \
    editing/JSEditor.cpp \
    editing/markup.cpp \
    editing/MergeIdenticalElementsCommand.cpp \
    editing/ModifySelectionListLevel.cpp \
    editing/MoveSelectionCommand.cpp \
    editing/RemoveCSSPropertyCommand.cpp \
    editing/RemoveNodeAttributeCommand.cpp \
    editing/RemoveNodeCommand.cpp \
    editing/RemoveNodePreservingChildrenCommand.cpp \
    editing/ReplaceSelectionCommand.cpp \
    editing/SelectionController.cpp \
    editing/Selection.cpp \
    editing/SetNodeAttributeCommand.cpp \
    editing/SplitElementCommand.cpp \
    editing/SplitTextNodeCommand.cpp \
    editing/SplitTextNodeContainingElementCommand.cpp \
    editing/TextIterator.cpp \
    editing/TypingCommand.cpp \
    editing/UnlinkCommand.cpp \
    editing/VisiblePosition.cpp \
    editing/visible_units.cpp \
    editing/WrapContentsInDummySpanCommand.cpp \
    history/BackForwardList.cpp \
    history/CachedPage.cpp \
    history/HistoryItem.cpp \
    history/PageCache.cpp \
    html/CanvasGradient.cpp \
    html/CanvasPattern.cpp \
    html/CanvasRenderingContext2D.cpp \
    html/CanvasStyle.cpp \
    html/FormDataList.cpp \
    html/HTMLAnchorElement.cpp \
    html/HTMLAppletElement.cpp \
    html/HTMLAreaElement.cpp \
    html/HTMLBaseElement.cpp \
    html/HTMLBaseFontElement.cpp \
    html/HTMLBlockquoteElement.cpp \
    html/HTMLBodyElement.cpp \
    html/HTMLBRElement.cpp \
    html/HTMLButtonElement.cpp \
    html/HTMLCanvasElement.cpp \
    html/HTMLCollection.cpp \
    html/HTMLDirectoryElement.cpp \
    html/HTMLDivElement.cpp \
    html/HTMLDListElement.cpp \
    html/HTMLDocument.cpp \
    html/HTMLElement.cpp \
    html/HTMLElementFactory.cpp \
    html/HTMLEmbedElement.cpp \
    html/HTMLFieldSetElement.cpp \
    html/HTMLFontElement.cpp \
    html/HTMLFormCollection.cpp \
    html/HTMLFormElement.cpp \
    html/HTMLFrameElementBase.cpp \
    html/HTMLFrameElement.cpp \
    html/HTMLFrameOwnerElement.cpp \
    html/HTMLFrameSetElement.cpp \
    html/HTMLGenericFormElement.cpp \
    html/HTMLHeadElement.cpp \
    html/HTMLHeadingElement.cpp \
    html/HTMLHRElement.cpp \
    html/HTMLHtmlElement.cpp \
    html/HTMLIFrameElement.cpp \
    html/HTMLImageElement.cpp \
    html/HTMLImageLoader.cpp \
    html/HTMLInputElement.cpp \
    html/HTMLIsIndexElement.cpp \
    html/HTMLKeygenElement.cpp \
    html/HTMLLabelElement.cpp \
    html/HTMLLegendElement.cpp \
    html/HTMLLIElement.cpp \
    html/HTMLLinkElement.cpp \
    html/HTMLMapElement.cpp \
    html/HTMLMarqueeElement.cpp \
    html/HTMLMenuElement.cpp \
    html/HTMLMetaElement.cpp \
    html/HTMLModElement.cpp \
    html/HTMLNameCollection.cpp \
    html/HTMLObjectElement.cpp \
    html/HTMLOListElement.cpp \
    html/HTMLOptGroupElement.cpp \
    html/HTMLOptionElement.cpp \
    html/HTMLOptionsCollection.cpp \
    html/HTMLParagraphElement.cpp \
    html/HTMLParamElement.cpp \
    html/HTMLParser.cpp \
    html/HTMLParserErrorCodes.cpp \
    html/HTMLPlugInElement.cpp \
    html/HTMLPreElement.cpp \
    html/HTMLQuoteElement.cpp \
    html/HTMLScriptElement.cpp \
    html/HTMLSelectElement.cpp \
    html/HTMLStyleElement.cpp \
    html/HTMLTableCaptionElement.cpp \
    html/HTMLTableCellElement.cpp \
    html/HTMLTableColElement.cpp \
    html/HTMLTableElement.cpp \
    html/HTMLTablePartElement.cpp \
    html/HTMLTableRowElement.cpp \
    html/HTMLTableSectionElement.cpp \
    html/HTMLTextAreaElement.cpp \
    html/HTMLTextFieldInnerElement.cpp \
    html/HTMLTitleElement.cpp \
    html/HTMLTokenizer.cpp \
    html/HTMLUListElement.cpp \
    html/HTMLViewSourceDocument.cpp \
#    icon/IconDatabase.cpp \
#    icon/SiteIcon.cpp \
#    icon/SQLDatabase.cpp \
#    icon/SQLStatement.cpp \
#    icon/SQLTransaction.cpp \
    loader/Cache.cpp \
    loader/CachedCSSStyleSheet.cpp \
    loader/CachedImage.cpp \
    loader/CachedResourceClientWalker.cpp \
    loader/CachedResource.cpp \
    loader/CachedScript.cpp \
    loader/CachedXSLStyleSheet.cpp \
    loader/DocLoader.cpp \
    loader/DocumentLoader.cpp \
    loader/FormState.cpp \
    loader/FrameLoader.cpp \
    loader/icon/IconDatabase.cpp \
    loader/icon/IconDataCache.cpp \
    loader/icon/IconLoader.cpp \
    loader/icon/SQLDatabase.cpp \
    loader/icon/SQLStatement.cpp \
    loader/icon/SQLTransaction.cpp \
    loader/ImageDocument.cpp \
    loader/loader.cpp \
    loader/MainResourceLoader.cpp \
    loader/NavigationAction.cpp \
    loader/NetscapePlugInStreamLoader.cpp \
    loader/PluginDocument.cpp \
    loader/ProgressTracker.cpp \
    loader/Request.cpp \
    loader/ResourceLoader.cpp \
    loader/SubresourceLoader.cpp \
    loader/TextDocument.cpp \
    loader/TextResourceDecoder.cpp \
    page/Chrome.cpp \
    page/ContextMenuController.cpp \
    page/DOMWindow.cpp \
    page/DragController.cpp \
    page/EventHandler.cpp \
    page/FocusController.cpp \
    page/Frame.cpp \
    page/FrameTree.cpp \
    page/FrameView.cpp \
    page/MouseEventWithHitTestResults.cpp \
    page/Page.cpp \
    page/Settings.cpp \
    platform/Arena.cpp \
    platform/ArrayImpl.cpp \
    platform/AtomicString.cpp \
    platform/Base64.cpp \
    platform/ContextMenu.cpp \
    platform/CString.cpp \
    platform/DeprecatedCString.cpp \
    platform/DeprecatedPtrListImpl.cpp \
    platform/DeprecatedString.cpp \
    platform/DeprecatedStringList.cpp \
    platform/DeprecatedValueListImpl.cpp \
    platform/DragData.cpp \
    platform/DragImage.cpp \
    platform/FileChooser.cpp \
    platform/FontCache.cpp \
    platform/Font.cpp \
    platform/FontData.cpp \
    platform/FontFallbackList.cpp \
    platform/FontFamily.cpp \
    platform/GlyphPageTreeNode.cpp \
    platform/GlyphWidthMap.cpp \
    platform/graphics/AffineTransform.cpp \
    platform/graphics/BitmapImage.cpp \
    platform/graphics/Color.cpp \
    platform/graphics/FloatPoint3D.cpp \
    platform/graphics/FloatPoint.cpp \
    platform/graphics/FloatRect.cpp \
    platform/graphics/FloatSize.cpp \
    platform/graphics/GraphicsContext.cpp \
    platform/graphics/GraphicsTypes.cpp \
    platform/graphics/ImageBuffer.cpp \
    platform/graphics/Image.cpp \
    platform/graphics/IntRect.cpp \
    platform/graphics/Path.cpp \
    platform/graphics/PathTraversalState.cpp \
    platform/graphics/Pen.cpp \
    platform/KURL.cpp \
    platform/Logging.cpp \
    platform/MimeTypeRegistry.cpp \
    platform/network/AuthenticationChallenge.cpp \
    platform/network/Credential.cpp \
    platform/network/FormData.cpp \
    platform/network/HTTPParsers.cpp \
    platform/network/ProtectionSpace.cpp \
    platform/network/ResourceHandle.cpp \
    platform/network/ResourceRequest.cpp \
    platform/network/ResourceResponse.cpp \
    platform/RegularExpression.cpp \
    platform/ScrollBar.cpp \
#    platform/SearchPopupMenu.cpp \
    platform/SegmentedString.cpp \
    platform/SharedBuffer.cpp \
    platform/String.cpp \
    platform/StringImpl.cpp \
    platform/TextCodec.cpp \
    platform/TextCodecLatin1.cpp \
    platform/TextCodecUTF16.cpp \
    platform/TextDecoder.cpp \
    platform/TextEncoding.cpp \
    platform/TextEncodingRegistry.cpp \
    platform/TextStream.cpp \
    platform/Timer.cpp \
    platform/Widget.cpp \
    rendering/AutoTableLayout.cpp \
    rendering/bidi.cpp \
    rendering/break_lines.cpp \
    rendering/CounterNode.cpp \
    rendering/EllipsisBox.cpp \
    rendering/FixedTableLayout.cpp \
    rendering/HitTestResult.cpp \
    rendering/InlineBox.cpp \
    rendering/InlineFlowBox.cpp \
    rendering/InlineTextBox.cpp \
    rendering/LayoutState.cpp \
    rendering/ListMarkerBox.cpp \
    rendering/RenderApplet.cpp \
    rendering/RenderArena.cpp \
    rendering/RenderBlock.cpp \
    rendering/RenderBox.cpp \
    rendering/RenderBR.cpp \
    rendering/RenderButton.cpp \
    rendering/RenderContainer.cpp \
    rendering/RenderCounter.cpp \
    rendering/RenderFieldset.cpp \
    rendering/RenderFileUploadControl.cpp \
    rendering/RenderFlexibleBox.cpp \
    rendering/RenderFlow.cpp \
    rendering/RenderFrame.cpp \
    rendering/RenderFrameSet.cpp \
    rendering/RenderHTMLCanvas.cpp \
    rendering/RenderImage.cpp \
    rendering/RenderInline.cpp \
    rendering/RenderLayer.cpp \
    rendering/RenderLegend.cpp \
    rendering/RenderListBox.cpp \
    rendering/RenderListItem.cpp \
    rendering/RenderListMarker.cpp \
    rendering/RenderMenuList.cpp \
    rendering/RenderObject.cpp \
    rendering/RenderPart.cpp \
    rendering/RenderPartObject.cpp \
    rendering/RenderReplaced.cpp \
    rendering/RenderSlider.cpp \
    rendering/RenderStyle.cpp \
    rendering/RenderTableCell.cpp \
    rendering/RenderTableCol.cpp \
    rendering/RenderTable.cpp \
    rendering/RenderTableRow.cpp \
    rendering/RenderTableSection.cpp \
    rendering/RenderTextControl.cpp \
    rendering/RenderText.cpp \
    rendering/RenderTextFragment.cpp \
    rendering/RenderTheme.cpp \
    rendering/RenderTreeAsText.cpp \
    rendering/RenderView.cpp \
    rendering/RenderWidget.cpp \
    rendering/RenderWordBreak.cpp \
    rendering/RootInlineBox.cpp \
    rendering/SVGRenderTreeAsText.cpp \
    xml/DOMParser.cpp \
    xml/NativeXPathNSResolver.cpp \
    xml/xmlhttprequest.cpp \
    xml/XMLSerializer.cpp \
    xml/XPathEvaluator.cpp \
    xml/XPathExpression.cpp \
    xml/XPathExpressionNode.cpp \
    xml/XPathFunctions.cpp \
    xml/XPathNamespace.cpp \
    xml/XPathNodeSet.cpp \
    xml/XPathNSResolver.cpp \
    xml/XPathParser.cpp \
    xml/XPathPath.cpp \
    xml/XPathPredicate.cpp \
    xml/XPathResult.cpp \
    xml/XPathStep.cpp \
    xml/XPathUtil.cpp \
    xml/XPathValue.cpp \
    xml/XPathVariableReference.cpp \
    xml/XSLImportRule.cpp \
    xml/XSLStyleSheet.cpp \
    xml/XSLTProcessor.cpp

qt-port:HEADERS += \
    $$PWD/platform/qt/QWebPopup.h \
    $$PWD/platform/qt/SharedTimerQt.h \
    $$PWD/../WebKitQt/Api/qwebframe.h \
    $$PWD/../WebKitQt/Api/qwebpage.h \
    $$PWD/../WebKitQt/Api/qwebnetworkinterface.h \
    $$PWD/../WebKitQt/Api/qwebnetworkinterface_p.h \
    $$PWD/../WebKitQt/Api/qwebobjectplugin.h \
    $$PWD/../WebKitQt/Api/qwebobjectplugin_p.h \
    $$PWD/../WebKitQt/Api/qwebobjectpluginconnector.h \
    $$PWD/../WebKitQt/Api/qcookiejar.h \
    $$PWD/../WebKitQt/WebCoreSupport/FrameLoaderClientQt.h

qt-port:SOURCES += \
    page/qt/DragControllerQt.cpp \
    page/qt/EventHandlerQt.cpp \
    page/qt/FrameQt.cpp \
    loader/qt/DocumentLoaderQt.cpp \
    platform/graphics/qt/AffineTransformQt.cpp \
    platform/graphics/qt/ColorQt.cpp \
    platform/graphics/qt/FloatPointQt.cpp \
    platform/graphics/qt/FloatRectQt.cpp \
    platform/graphics/qt/GraphicsContextQt.cpp \
    platform/graphics/qt/IconQt.cpp \
    platform/graphics/qt/ImageBufferQt.cpp \
    platform/graphics/qt/ImageDecoderQt.cpp \
    platform/graphics/qt/ImageQt.cpp \
    platform/graphics/qt/ImageSourceQt.cpp \
    platform/graphics/qt/IntPointQt.cpp \
    platform/graphics/qt/IntRectQt.cpp \
    platform/graphics/qt/IntSizeQt.cpp \
    platform/graphics/qt/PathQt.cpp \
    platform/network/qt/ResourceHandleQt.cpp \
    editing/qt/EditorQt.cpp \
    history/qt/CachedPageQt.cpp \
    platform/qt/ClipboardQt.cpp \
    platform/qt/ContextMenuItemQt.cpp \
    platform/qt/ContextMenuQt.cpp \
    platform/qt/CookieJarQt.cpp \
    platform/qt/CursorQt.cpp \
    platform/qt/DragDataQt.cpp \
    platform/qt/DragImageQt.cpp \
    platform/qt/FileChooserQt.cpp \
    platform/qt/FontCacheQt.cpp \
    platform/qt/FontDataQt.cpp \
    platform/qt/FontPlatformDataQt.cpp \
    platform/qt/FontQt.cpp \
    platform/qt/GlyphPageTreeNodeQt.cpp \
    platform/qt/MimeTypeRegistryQt.cpp \
    platform/qt/PasteboardQt.cpp \
    platform/qt/PlatformKeyboardEventQt.cpp \
    platform/qt/PlatformMouseEventQt.cpp \
    platform/qt/PlatformScrollBarQt.cpp \
    platform/qt/PlugInInfoStoreQt.cpp \
    platform/qt/PopupMenuQt.cpp \
    platform/qt/QWebPopup.cpp \
    platform/qt/RenderThemeQt.cpp \
    platform/qt/ScreenQt.cpp \
    platform/qt/ScrollViewQt.cpp \
    platform/qt/SearchPopupMenuQt.cpp \
    platform/qt/SharedTimerQt.cpp \
    platform/qt/SoundQt.cpp \
    platform/qt/StringQt.cpp \
    platform/qt/SystemTimeQt.cpp \
    platform/qt/TemporaryLinkStubs.cpp \
    platform/qt/TextBoundaries.cpp \
    platform/qt/TextBreakIteratorQt.cpp \
    platform/qt/TextCodecQt.cpp \
    platform/qt/WheelEventQt.cpp \
    platform/qt/WidgetQt.cpp \
    ../WebKitQt/WebCoreSupport/ChromeClientQt.cpp \
    ../WebKitQt/WebCoreSupport/ContextMenuClientQt.cpp \
    ../WebKitQt/WebCoreSupport/DragClientQt.cpp \
    ../WebKitQt/WebCoreSupport/EditorClientQt.cpp \
    ../WebKitQt/WebCoreSupport/EditCommandQt.cpp \
    ../WebKitQt/WebCoreSupport/FrameLoaderClientQt.cpp \
    ../WebKitQt/Api/qwebframe.cpp \
    ../WebKitQt/Api/qwebnetworkinterface.cpp \
    ../WebKitQt/Api/qcookiejar.cpp \
    ../WebKitQt/Api/qwebpage.cpp \
    ../WebKitQt/Api/qwebpagehistory.cpp \
    ../WebKitQt/Api/qwebsettings.cpp \
    ../WebKitQt/Api/qwebobjectplugin.cpp \
    ../WebKitQt/Api/qwebobjectpluginconnector.cpp 

gdk-port:SOURCES += \
        platform/TextCodecICU.cpp \
        platform/TextBreakIteratorICU.cpp \
        page/gdk/EventHandlerGdk.cpp \
        page/gdk/ContextMenuClientGdk.cpp \
        page/gdk/DragControllerGdk.cpp \
        loader/gdk/DocumentLoaderGdk.cpp \
        loader/gdk/FrameLoaderClientGdk.cpp \
        platform/gdk/CookieJarGdk.cpp \
        platform/gdk/CursorGdk.cpp \
        platform/gdk/DragDataGdk.cpp \
        platform/gdk/DragImageGdk.cpp \
        platform/gdk/EditorClientGdk.cpp \
        platform/gdk/FontCacheGdk.cpp \
        platform/gdk/FontDataGdk.cpp \
        platform/gdk/FontGdk.cpp \
        platform/gdk/FontPlatformDataGdk.cpp \
        platform/gdk/FrameGdk.cpp \
        platform/gdk/GlyphPageTreeNodeGdk.cpp \
        platform/gdk/KeyEventGdk.cpp \
        platform/gdk/LoggingGdk.cpp \
        platform/gdk/MimeTypeRegistryGdk.cpp \
        platform/gdk/MouseEventGdk.cpp \
        platform/gdk/PlatformScrollBarGdk.cpp \
        platform/gdk/PopupMenuGdk.cpp \
        platform/gdk/RenderThemeGdk.cpp \
        platform/gdk/ScrollViewGdk.cpp \
        platform/gdk/ScreenGdk.cpp \
        platform/gdk/SharedTimerLinux.cpp \
        platform/gdk/SystemTimeLinux.cpp \
        platform/gdk/TemporaryLinkStubs.cpp \
        platform/gdk/WheelEventGdk.cpp \
        platform/gdk/WidgetGdk.cpp \
        platform/graphics/gdk/ImageGdk.cpp \
        platform/network/curl/ResourceHandleCurl.cpp \
        platform/network/curl/ResourceHandleManager.cpp \
        platform/graphics/cairo/AffineTransformCairo.cpp \
        platform/graphics/cairo/GraphicsContextCairo.cpp \
        platform/graphics/cairo/ImageBufferCairo.cpp \
        platform/graphics/cairo/ImageCairo.cpp \
        platform/graphics/cairo/ImageSourceCairo.cpp \
        platform/graphics/cairo/PathCairo.cpp \
        platform/image-decoders/gif/GIFImageDecoder.cpp \
        platform/image-decoders/gif/GIFImageReader.cpp  \
        platform/image-decoders/png/PNGImageDecoder.cpp \
        platform/image-decoders/png/png.c \
        platform/image-decoders/png/pngerror.c \
        platform/image-decoders/png/pnggccrd.c \
        platform/image-decoders/png/pngget.c \
        platform/image-decoders/png/pngmem.c \
        platform/image-decoders/png/pngpread.c \
        platform/image-decoders/png/pngread.c \
        platform/image-decoders/png/pngrio.c \
        platform/image-decoders/png/pngrtran.c \
        platform/image-decoders/png/pngrutil.c \
        platform/image-decoders/png/pngset.c \
        platform/image-decoders/png/pngtrans.c \
        platform/image-decoders/png/pngvcrd.c \
        platform/image-decoders/png/pngwio.c \
        platform/image-decoders/png/pngwrite.c \
        platform/image-decoders/png/pngwtran.c \
        platform/image-decoders/png/pngwutil.c \
        platform/image-decoders/jpeg/JPEGImageDecoder.cpp \
        platform/image-decoders/jpeg/jcomapi.c \
        platform/image-decoders/jpeg/jdapimin.c \
        platform/image-decoders/jpeg/jdapistd.c \
        platform/image-decoders/jpeg/jdatadst.c \
        platform/image-decoders/jpeg/jdatasrc.c \
        platform/image-decoders/jpeg/jdcoefct.c \
        platform/image-decoders/jpeg/jdcolor.c \
        platform/image-decoders/jpeg/jddctmgr.c \
        platform/image-decoders/jpeg/jdhuff.c \
        platform/image-decoders/jpeg/jdinput.c \
        platform/image-decoders/jpeg/jdmainct.c \
        platform/image-decoders/jpeg/jdmarker.c \
        platform/image-decoders/jpeg/jdmaster.c \
        platform/image-decoders/jpeg/jdmerge.c \
        platform/image-decoders/jpeg/jdphuff.c \
        platform/image-decoders/jpeg/jdpostct.c \
        platform/image-decoders/jpeg/jdsample.c \
        platform/image-decoders/jpeg/jerror.c \
        platform/image-decoders/jpeg/jfdctflt.c \
        platform/image-decoders/jpeg/jfdctfst.c \
        platform/image-decoders/jpeg/jfdctint.c \
        platform/image-decoders/jpeg/jidctflt.c \
        platform/image-decoders/jpeg/jidctfst.c \
        platform/image-decoders/jpeg/jidctint.c \
        platform/image-decoders/jpeg/jmemmgr.c \
        platform/image-decoders/jpeg/jmemnobs.c \
        platform/image-decoders/jpeg/jquant1.c \
        platform/image-decoders/jpeg/jquant2.c \
        platform/image-decoders/jpeg/jutils.c \
        platform/image-decoders/bmp/BMPImageDecoder.cpp \
        platform/image-decoders/ico/ICOImageDecoder.cpp \
        platform/image-decoders/xbm/XBMImageDecoder.cpp
 

contains(DEFINES, ENABLE_XPATH=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_XPATH

    XPATHBISON = $$PWD/xml/XPathGrammar.y

    IDL_BINDINGS += \
        xml/XPathNSResolver.idl \
        xml/XPathExpression.idl \
        xml/XPathResult.idl \
        xml/XPathEvaluator.idl
}

contains(DEFINES, ENABLE_XSLT=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_XSLT
    !mac:CONFIG += link_pkgconfig
    PKGCONFIG += libxslt
}

contains(DEFINES, ENABLE_XBL=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_XBL
}

contains(DEFINES, ENABLE_SVG=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_SVG

    DEPENDPATH += ksvg2/css ksvg2/events ksvg2/misc ksvg2/svg platform/graphics/svg
    qt-port {
	DEPENDPATH += platform/graphics/svg/qt
    }

    gdk-port {
	DEPENDPATH += platform/graphics/svg/cairo
    }

    SVG_NAMES = $$PWD/ksvg2/svg/svgtags.in

    XLINK_NAMES = $$PWD/ksvg2/misc/xlinkattrs.in

    IDL_BINDINGS += ksvg2/events/SVGZoomEvent.idl \
        ksvg2/svg/SVGAElement.idl \
        ksvg2/svg/SVGAngle.idl \
        ksvg2/svg/SVGAnimateColorElement.idl \
        ksvg2/svg/SVGAnimatedAngle.idl \
        ksvg2/svg/SVGAnimatedBoolean.idl \
        ksvg2/svg/SVGAnimatedEnumeration.idl \
        ksvg2/svg/SVGAnimatedInteger.idl \
        ksvg2/svg/SVGAnimatedLength.idl \
        ksvg2/svg/SVGAnimatedLengthList.idl \
        ksvg2/svg/SVGAnimatedNumber.idl \
        ksvg2/svg/SVGAnimatedNumberList.idl \
        ksvg2/svg/SVGAnimatedPreserveAspectRatio.idl \
        ksvg2/svg/SVGAnimatedRect.idl \
        ksvg2/svg/SVGAnimatedString.idl \
        ksvg2/svg/SVGAnimatedTransformList.idl \
        ksvg2/svg/SVGAnimateElement.idl \
        ksvg2/svg/SVGAnimateTransformElement.idl \
        ksvg2/svg/SVGAnimationElement.idl \
        ksvg2/svg/SVGCircleElement.idl \
        ksvg2/svg/SVGClipPathElement.idl \
        ksvg2/svg/SVGColor.idl \
        ksvg2/svg/SVGComponentTransferFunctionElement.idl \
        ksvg2/svg/SVGCursorElement.idl \
        ksvg2/svg/SVGDefsElement.idl \
        ksvg2/svg/SVGDescElement.idl \
        ksvg2/svg/SVGDocument.idl \
        ksvg2/svg/SVGElement.idl \
        ksvg2/svg/SVGElementInstance.idl \
        ksvg2/svg/SVGElementInstanceList.idl \
        ksvg2/svg/SVGEllipseElement.idl \
        ksvg2/svg/SVGFEBlendElement.idl \
        ksvg2/svg/SVGFEColorMatrixElement.idl \
        ksvg2/svg/SVGFEComponentTransferElement.idl \
        ksvg2/svg/SVGFECompositeElement.idl \
        ksvg2/svg/SVGFEDiffuseLightingElement.idl \
        ksvg2/svg/SVGFEDisplacementMapElement.idl \
        ksvg2/svg/SVGFEDistantLightElement.idl \
        ksvg2/svg/SVGFEFloodElement.idl \
        ksvg2/svg/SVGFEFuncAElement.idl \
        ksvg2/svg/SVGFEFuncBElement.idl \
        ksvg2/svg/SVGFEFuncGElement.idl \
        ksvg2/svg/SVGFEFuncRElement.idl \
        ksvg2/svg/SVGFEGaussianBlurElement.idl \
        ksvg2/svg/SVGFEImageElement.idl \
        ksvg2/svg/SVGFEMergeElement.idl \
        ksvg2/svg/SVGFEMergeNodeElement.idl \
        ksvg2/svg/SVGFEOffsetElement.idl \
        ksvg2/svg/SVGFEPointLightElement.idl \
        ksvg2/svg/SVGFESpecularLightingElement.idl \
        ksvg2/svg/SVGFESpotLightElement.idl \
        ksvg2/svg/SVGFETileElement.idl \
        ksvg2/svg/SVGFETurbulenceElement.idl \
        ksvg2/svg/SVGFilterElement.idl \
        ksvg2/svg/SVGForeignObjectElement.idl \
        ksvg2/svg/SVGGElement.idl \
        ksvg2/svg/SVGGradientElement.idl \
        ksvg2/svg/SVGImageElement.idl \
        ksvg2/svg/SVGLength.idl \
        ksvg2/svg/SVGLengthList.idl \
        ksvg2/svg/SVGLinearGradientElement.idl \
        ksvg2/svg/SVGLineElement.idl \
        ksvg2/svg/SVGMarkerElement.idl \
        ksvg2/svg/SVGMaskElement.idl \
        ksvg2/svg/SVGMatrix.idl \
        ksvg2/svg/SVGMetadataElement.idl \
        ksvg2/svg/SVGNumber.idl \
        ksvg2/svg/SVGNumberList.idl \
        ksvg2/svg/SVGPaint.idl \
        ksvg2/svg/SVGPathElement.idl \
        ksvg2/svg/SVGPathSegArcAbs.idl \
        ksvg2/svg/SVGPathSegArcRel.idl \
        ksvg2/svg/SVGPathSegClosePath.idl \
        ksvg2/svg/SVGPathSegCurvetoCubicAbs.idl \
        ksvg2/svg/SVGPathSegCurvetoCubicRel.idl \
        ksvg2/svg/SVGPathSegCurvetoCubicSmoothAbs.idl \
        ksvg2/svg/SVGPathSegCurvetoCubicSmoothRel.idl \
        ksvg2/svg/SVGPathSegCurvetoQuadraticAbs.idl \
        ksvg2/svg/SVGPathSegCurvetoQuadraticRel.idl \
        ksvg2/svg/SVGPathSegCurvetoQuadraticSmoothAbs.idl \
        ksvg2/svg/SVGPathSegCurvetoQuadraticSmoothRel.idl \
        ksvg2/svg/SVGPathSeg.idl \
        ksvg2/svg/SVGPathSegLinetoAbs.idl \
        ksvg2/svg/SVGPathSegLinetoHorizontalAbs.idl \
        ksvg2/svg/SVGPathSegLinetoHorizontalRel.idl \
        ksvg2/svg/SVGPathSegLinetoRel.idl \
        ksvg2/svg/SVGPathSegLinetoVerticalAbs.idl \
        ksvg2/svg/SVGPathSegLinetoVerticalRel.idl \
        ksvg2/svg/SVGPathSegList.idl \
        ksvg2/svg/SVGPathSegMovetoAbs.idl \
        ksvg2/svg/SVGPathSegMovetoRel.idl \
        ksvg2/svg/SVGPatternElement.idl \
        ksvg2/svg/SVGPoint.idl \
        ksvg2/svg/SVGPointList.idl \
        ksvg2/svg/SVGPolygonElement.idl \
        ksvg2/svg/SVGPolylineElement.idl \
        ksvg2/svg/SVGPreserveAspectRatio.idl \
        ksvg2/svg/SVGRadialGradientElement.idl \
        ksvg2/svg/SVGRectElement.idl \
        ksvg2/svg/SVGRect.idl \
        ksvg2/svg/SVGRenderingIntent.idl \
        ksvg2/svg/SVGScriptElement.idl \
        ksvg2/svg/SVGSetElement.idl \
        ksvg2/svg/SVGStopElement.idl \
        ksvg2/svg/SVGStringList.idl \
        ksvg2/svg/SVGStyleElement.idl \
        ksvg2/svg/SVGSVGElement.idl \
        ksvg2/svg/SVGSwitchElement.idl \
        ksvg2/svg/SVGSymbolElement.idl \
        ksvg2/svg/SVGTextContentElement.idl \
        ksvg2/svg/SVGTextElement.idl \
        ksvg2/svg/SVGTextPositioningElement.idl \
        ksvg2/svg/SVGTitleElement.idl \
        ksvg2/svg/SVGTransform.idl \
        ksvg2/svg/SVGTransformList.idl \
        ksvg2/svg/SVGTRefElement.idl \
        ksvg2/svg/SVGTSpanElement.idl \
        ksvg2/svg/SVGUnitTypes.idl \
        ksvg2/svg/SVGUseElement.idl \
        ksvg2/svg/SVGViewElement.idl 

    SOURCES += \
# TODO: this-one-is-not-auto-added! FIXME! tmp/SVGElementFactory.cpp \
        bindings/js/JSSVGElementWrapperFactory.cpp \
        bindings/js/JSSVGMatrixCustom.cpp \
        bindings/js/JSSVGPathSegCustom.cpp \
        bindings/js/JSSVGPathSegListCustom.cpp \
        bindings/js/JSSVGPointListCustom.cpp \
        ksvg2/css/SVGCSSParser.cpp \
        ksvg2/css/SVGCSSStyleSelector.cpp \
        ksvg2/css/SVGRenderStyle.cpp \
        ksvg2/css/SVGRenderStyleDefs.cpp \
        ksvg2/events/JSSVGLazyEventListener.cpp \
        ksvg2/events/SVGZoomEvent.cpp \
        ksvg2/misc/KCanvasRenderingStyle.cpp \
        ksvg2/misc/PointerEventsHitRules.cpp \
        ksvg2/misc/SVGDocumentExtensions.cpp \
        ksvg2/misc/SVGImageLoader.cpp \
        ksvg2/misc/SVGTimer.cpp \
        ksvg2/misc/TimeScheduler.cpp \
        ksvg2/svg/ColorDistance.cpp \
        ksvg2/svg/SVGAElement.cpp \
        ksvg2/svg/SVGAngle.cpp \
        ksvg2/svg/SVGAnimateColorElement.cpp \
        ksvg2/svg/SVGAnimatedPathData.cpp \
        ksvg2/svg/SVGAnimatedPoints.cpp \
        ksvg2/svg/SVGAnimateElement.cpp \
        ksvg2/svg/SVGAnimateMotionElement.cpp \
        ksvg2/svg/SVGAnimateTransformElement.cpp \
        ksvg2/svg/SVGAnimationElement.cpp \
        ksvg2/svg/SVGCircleElement.cpp \
        ksvg2/svg/SVGClipPathElement.cpp \
        ksvg2/svg/SVGColor.cpp \
        ksvg2/svg/SVGComponentTransferFunctionElement.cpp \
        ksvg2/svg/SVGCursorElement.cpp \
        ksvg2/svg/SVGDefsElement.cpp \
        ksvg2/svg/SVGDescElement.cpp \
        ksvg2/svg/SVGDocument.cpp \
        ksvg2/svg/SVGElement.cpp \
        ksvg2/svg/SVGElementInstance.cpp \
        ksvg2/svg/SVGElementInstanceList.cpp \
        ksvg2/svg/SVGEllipseElement.cpp \
        ksvg2/svg/SVGExternalResourcesRequired.cpp \
        ksvg2/svg/SVGFEBlendElement.cpp \
        ksvg2/svg/SVGFEColorMatrixElement.cpp \
        ksvg2/svg/SVGFEComponentTransferElement.cpp \
        ksvg2/svg/SVGFECompositeElement.cpp \
        ksvg2/svg/SVGFEDiffuseLightingElement.cpp \
        ksvg2/svg/SVGFEDisplacementMapElement.cpp \
        ksvg2/svg/SVGFEDistantLightElement.cpp \
        ksvg2/svg/SVGFEFloodElement.cpp \
        ksvg2/svg/SVGFEFuncAElement.cpp \
        ksvg2/svg/SVGFEFuncBElement.cpp \
        ksvg2/svg/SVGFEFuncGElement.cpp \
        ksvg2/svg/SVGFEFuncRElement.cpp \
        ksvg2/svg/SVGFEGaussianBlurElement.cpp \
        ksvg2/svg/SVGFEImageElement.cpp \
        ksvg2/svg/SVGFELightElement.cpp \
        ksvg2/svg/SVGFEMergeElement.cpp \
        ksvg2/svg/SVGFEMergeNodeElement.cpp \
        ksvg2/svg/SVGFEOffsetElement.cpp \
        ksvg2/svg/SVGFEPointLightElement.cpp \
        ksvg2/svg/SVGFESpecularLightingElement.cpp \
        ksvg2/svg/SVGFESpotLightElement.cpp \
        ksvg2/svg/SVGFETileElement.cpp \
        ksvg2/svg/SVGFETurbulenceElement.cpp \
        ksvg2/svg/SVGFilterElement.cpp \
        ksvg2/svg/SVGFilterPrimitiveStandardAttributes.cpp \
        ksvg2/svg/SVGFitToViewBox.cpp \
        ksvg2/svg/SVGForeignObjectElement.cpp \
        ksvg2/svg/SVGGElement.cpp \
        ksvg2/svg/SVGGradientElement.cpp \
        ksvg2/svg/SVGImageElement.cpp \
        ksvg2/svg/SVGLangSpace.cpp \
        ksvg2/svg/SVGLength.cpp \
        ksvg2/svg/SVGLengthList.cpp \
        ksvg2/svg/SVGLinearGradientElement.cpp \
        ksvg2/svg/SVGLineElement.cpp \
        ksvg2/svg/SVGLocatable.cpp \
        ksvg2/svg/SVGMarkerElement.cpp \
        ksvg2/svg/SVGMaskElement.cpp \
        ksvg2/svg/SVGMetadataElement.cpp \
        ksvg2/svg/SVGMPathElement.cpp \
        ksvg2/svg/SVGNumberList.cpp \
        ksvg2/svg/SVGPaint.cpp \
        ksvg2/svg/SVGParserUtilities.cpp \
        ksvg2/svg/SVGPathElement.cpp \
        ksvg2/svg/SVGPathSegArc.cpp \
        ksvg2/svg/SVGPathSegClosePath.cpp \
        ksvg2/svg/SVGPathSegCurvetoCubic.cpp \
        ksvg2/svg/SVGPathSegCurvetoCubicSmooth.cpp \
        ksvg2/svg/SVGPathSegCurvetoQuadratic.cpp \
        ksvg2/svg/SVGPathSegCurvetoQuadraticSmooth.cpp \
        ksvg2/svg/SVGPathSegLineto.cpp \
        ksvg2/svg/SVGPathSegLinetoHorizontal.cpp \
        ksvg2/svg/SVGPathSegLinetoVertical.cpp \
        ksvg2/svg/SVGPathSegList.cpp \
        ksvg2/svg/SVGPathSegMoveto.cpp \
        ksvg2/svg/SVGPatternElement.cpp \
        ksvg2/svg/SVGPointList.cpp \
        ksvg2/svg/SVGPolyElement.cpp \
        ksvg2/svg/SVGPolygonElement.cpp \
        ksvg2/svg/SVGPolylineElement.cpp \
        ksvg2/svg/SVGPreserveAspectRatio.cpp \
        ksvg2/svg/SVGRadialGradientElement.cpp \
        ksvg2/svg/SVGRectElement.cpp \
        ksvg2/svg/SVGScriptElement.cpp \
        ksvg2/svg/SVGSetElement.cpp \
        ksvg2/svg/SVGStopElement.cpp \
        ksvg2/svg/SVGStringList.cpp \
        ksvg2/svg/SVGStylable.cpp \
        ksvg2/svg/SVGStyledElement.cpp \
        ksvg2/svg/SVGStyledLocatableElement.cpp \
        ksvg2/svg/SVGStyledTransformableElement.cpp \
        ksvg2/svg/SVGStyleElement.cpp \
        ksvg2/svg/SVGSVGElement.cpp \
        ksvg2/svg/SVGSwitchElement.cpp \
        ksvg2/svg/SVGSymbolElement.cpp \
        ksvg2/svg/SVGTests.cpp \
        ksvg2/svg/SVGTextContentElement.cpp \
        ksvg2/svg/SVGTextElement.cpp \
        ksvg2/svg/SVGTextPositioningElement.cpp \
        ksvg2/svg/SVGTitleElement.cpp \
        ksvg2/svg/SVGTransformable.cpp \
        ksvg2/svg/SVGTransform.cpp \
        ksvg2/svg/SVGTransformDistance.cpp \
        ksvg2/svg/SVGTransformList.cpp \
        ksvg2/svg/SVGTRefElement.cpp \
        ksvg2/svg/SVGTSpanElement.cpp \
        ksvg2/svg/SVGURIReference.cpp \
        ksvg2/svg/SVGUseElement.cpp \
        ksvg2/svg/SVGViewElement.cpp \
        ksvg2/svg/SVGZoomAndPan.cpp \
        platform/graphics/svg/filters/SVGFEBlend.cpp \
        platform/graphics/svg/filters/SVGFEColorMatrix.cpp \
        platform/graphics/svg/filters/SVGFEComponentTransfer.cpp \
        platform/graphics/svg/filters/SVGFEComposite.cpp \
        platform/graphics/svg/filters/SVGFEConvolveMatrix.cpp \
        platform/graphics/svg/filters/SVGFEDiffuseLighting.cpp \
        platform/graphics/svg/filters/SVGFEDisplacementMap.cpp \
        platform/graphics/svg/filters/SVGFEFlood.cpp \
        platform/graphics/svg/filters/SVGFEGaussianBlur.cpp \
        platform/graphics/svg/filters/SVGFEImage.cpp \
        platform/graphics/svg/filters/SVGFEMerge.cpp \
        platform/graphics/svg/filters/SVGFEMorphology.cpp \
        platform/graphics/svg/filters/SVGFEOffset.cpp \
        platform/graphics/svg/filters/SVGFESpecularLighting.cpp \
        platform/graphics/svg/filters/SVGFETurbulence.cpp \
        platform/graphics/svg/filters/SVGFilterEffect.cpp \
        platform/graphics/svg/filters/SVGLightSource.cpp \
        platform/graphics/svg/SVGImage.cpp \
        platform/graphics/svg/SVGPaintServer.cpp \
        platform/graphics/svg/SVGPaintServerGradient.cpp \
        platform/graphics/svg/SVGPaintServerLinearGradient.cpp \
        platform/graphics/svg/SVGPaintServerPattern.cpp \
        platform/graphics/svg/SVGPaintServerRadialGradient.cpp \
        platform/graphics/svg/SVGPaintServerSolid.cpp \
        platform/graphics/svg/SVGResourceClipper.cpp \
        platform/graphics/svg/SVGResource.cpp \
        platform/graphics/svg/SVGResourceFilter.cpp \
        platform/graphics/svg/SVGResourceMarker.cpp \
        platform/graphics/svg/SVGResourceMasker.cpp \
        rendering/RenderForeignObject.cpp \
        rendering/RenderPath.cpp \
        rendering/RenderSVGBlock.cpp \
        rendering/RenderSVGContainer.cpp \
        rendering/RenderSVGGradientStop.cpp \
        rendering/RenderSVGHiddenContainer.cpp \
        rendering/RenderSVGImage.cpp \
        rendering/RenderSVGInline.cpp \
        rendering/RenderSVGInlineText.cpp \
        rendering/RenderSVGText.cpp \
        rendering/RenderSVGTSpan.cpp \
        rendering/SVGInlineFlowBox.cpp \
        rendering/SVGInlineTextBox.cpp \
        rendering/SVGRootInlineBox.cpp

qt-port:SOURCES += \
        platform/graphics/svg/qt/RenderPathQt.cpp \
        platform/graphics/svg/qt/SVGPaintServerGradientQt.cpp \
        platform/graphics/svg/qt/SVGPaintServerLinearGradientQt.cpp \
        platform/graphics/svg/qt/SVGPaintServerPatternQt.cpp \
        platform/graphics/svg/qt/SVGPaintServerQt.cpp \
        platform/graphics/svg/qt/SVGPaintServerRadialGradientQt.cpp \
        platform/graphics/svg/qt/SVGPaintServerSolidQt.cpp \
        platform/graphics/svg/qt/SVGResourceClipperQt.cpp \
        platform/graphics/svg/qt/SVGResourceFilterQt.cpp \
        platform/graphics/svg/qt/SVGResourceMaskerQt.cpp

gdk-port:SOURCES += \
        platform/graphics/svg/cairo/RenderPathCairo.cpp \
        platform/graphics/svg/cairo/SVGPaintServerCairo.cpp \
        platform/graphics/svg/cairo/SVGPaintServerGradientCairo.cpp \
        platform/graphics/svg/cairo/SVGPaintServerPatternCairo.cpp \
        platform/graphics/svg/cairo/SVGPaintServerSolidCairo.cpp \
        platform/graphics/svg/cairo/SVGResourceClipperCairo.cpp \
        platform/graphics/svg/cairo/SVGResourceMaskerCairo.cpp

        # GENERATOR 5-C:
        svgnames_a.output = tmp/SVGNames.cpp
        svgnames_a.commands = perl $$PWD/ksvg2/scripts/make_names.pl --tags $$PWD/ksvg2/svg/svgtags.in --attrs $$PWD/ksvg2/svg/svgattrs.in --namespace SVG --cppNamespace WebCore --namespaceURI 'http://www.w3.org/2000/svg' --factory --attrsNullNamespace --output tmp
        svgnames_a.input = SVG_NAMES
        svgnames_a.dependency_type = TYPE_C
        svgnames_a.CONFIG = target_predeps
        svgnames_a.variable_out = GENERATED_SOURCES
        svgnames_a.clean = ${QMAKE_FILE_OUT} tmp/SVGNames.h
        QMAKE_EXTRA_COMPILERS += svgnames_a
        svgnames_b.output = tmp/SVGElementFactory.cpp
        svgnames_b.commands = @echo -n ''
        svgnames_b.input = SVG_NAMES
        svgnames_b.depends = tmp/SVGNames.cpp
        svgnames_b.CONFIG = target_predeps
        svgnames_b.variable_out = GENERATED_SOURCES
        svgnames_b.clean += tmp/SVGElementFactory.h ${QMAKE_FILE_OUT}
        QMAKE_EXTRA_COMPILERS += svgnames_b

        # GENERATOR 5-D:
        xlinknames.output = tmp/XLinkNames.cpp
        xlinknames.commands = perl $$PWD/ksvg2/scripts/make_names.pl --attrs $$PWD/ksvg2/misc/xlinkattrs.in --namespace XLink --cppNamespace WebCore --namespaceURI 'http://www.w3.org/1999/xlink' --output tmp
        xlinknames.input = XLINK_NAMES
        xlinknames.dependency_type = TYPE_C
        xlinknames.CONFIG = target_predeps
        xlinknames.variable_out = GENERATED_SOURCES
        xlinknames.clean = ${QMAKE_FILE_OUT} tmp/XLinkNames.h
        QMAKE_EXTRA_COMPILERS += xlinknames
}


# GENERATOR 1: IDL compiler
idl.output = tmp/JS${QMAKE_FILE_BASE}.cpp
idl.variable_out = GENERATED_SOURCES
idl.input = IDL_BINDINGS
idl.commands = perl -I$$PWD/bindings/scripts $$PWD/bindings/scripts/generate-bindings.pl --defines \"$${FEATURE_DEFINES_JAVASCRIPT}\" --generator JS --include $$PWD/dom --include $$PWD/html --include $$PWD/xml --include $$PWD/ksvg2/svg --outputdir tmp ${QMAKE_FILE_NAME}
idl.CONFIG += target_predeps
idl.clean = tmp/JS${QMAKE_FILE_BASE}.h ${QMAKE_FILE_OUT}
QMAKE_EXTRA_COMPILERS += idl

# GENERATOR 2-A: LUT creator
#lut.output = tmp/${QMAKE_FILE_BASE}.lut.h
#lut.commands = perl $$PWD/../JavaScriptCore/kjs/create_hash_table ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
#lut.depend = ${QMAKE_FILE_NAME}
#lut.input = LUT_FILES
#lut.CONFIG += no_link
#QMAKE_EXTRA_COMPILERS += lut

# GENERATOR 2-B: like JavaScriptCore/LUT Generator, but rename output
luttable.output = tmp/${QMAKE_FILE_BASE}Table.cpp
luttable.commands = perl $$PWD/../JavaScriptCore/kjs/create_hash_table ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
luttable.depend = ${QMAKE_FILE_NAME}
luttable.input = LUT_TABLE_FILES
luttable.CONFIG += no_link
luttable.dependency_type = TYPE_C
QMAKE_EXTRA_COMPILERS += luttable

# GENERATOR 3: tokenizer (flex)
tokenizer.output = tmp/${QMAKE_FILE_BASE}.cpp
tokenizer.commands = flex -t < ${QMAKE_FILE_NAME} | perl $$PWD/css/maketokenizer > ${QMAKE_FILE_OUT}
tokenizer.dependency_Type = TYPE_C
tokenizer.input = TOKENIZER
tokenizer.CONFIG += target_predeps no_link
QMAKE_EXTRA_COMPILERS += tokenizer

# GENERATOR 4: CSS grammar
CSS_GRAMMAR_GUARD_A = \"\\043ifndef CSSGRAMMAR_H\\n\\043define CSSGRAMMAR_H\\n\"
CSS_GRAMMAR_GUARD_B = \"\\043endif\\n\"
cssbison.output = tmp/${QMAKE_FILE_BASE}.cpp
cssbison.commands = bison -d -p cssyy ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_BASE}.tab.c && mv ${QMAKE_FILE_BASE}.tab.c tmp/${QMAKE_FILE_BASE}.cpp && touch tmp/${QMAKE_FILE_BASE}.h && perl -e \'printf $$CSS_GRAMMAR_GUARD_A\' > tmp/${QMAKE_FILE_BASE}.h && cat ${QMAKE_FILE_BASE}.tab.h >> tmp/${QMAKE_FILE_BASE}.h && perl -e \'printf $$CSS_GRAMMAR_GUARD_B\' >> tmp/${QMAKE_FILE_BASE}.h && rm ${QMAKE_FILE_BASE}.tab.h
cssbison.depend = ${QMAKE_FILE_NAME}
cssbison.input = CSSBISON
cssbison.CONFIG = target_predeps
cssbison.dependency_type = TYPE_C
cssbison.variable_out = GENERATED_SOURCES
cssbison.clean = ${QMAKE_FILE_OUT} tmp/${QMAKE_FILE_BASE}.h
QMAKE_EXTRA_COMPILERS += cssbison
#PRE_TARGETDEPS += tmp/CSSGrammar.cpp
grammar_h_dep.target = tmp/cssparser.o
grammar_h_dep.depends = tmp/CSSGrammar.cpp
QMAKE_EXTRA_TARGETS += grammar_h_dep

# GENERATOR 5-A:
htmlnames.output = tmp/HTMLNames.cpp
htmlnames.commands = perl $$PWD/ksvg2/scripts/make_names.pl --tags $$PWD/html/HTMLTagNames.in --attrs $$PWD/html/HTMLAttributeNames.in --namespace HTML --namespacePrefix xhtml --cppNamespace WebCore --namespaceURI 'http://www.w3.org/1999/xhtml' --attrsNullNamespace --output tmp
htmlnames.input = HTML_NAMES
htmlnames.dependency_type = TYPE_C
htmlnames.CONFIG = target_predeps
htmlnames.variable_out = GENERATED_SOURCES
htmlnames.clean = ${QMAKE_FILE_OUT} tmp/HTMLNames.h
QMAKE_EXTRA_COMPILERS += htmlnames

# GENERATOR 5-B:
xmlnames.output = tmp/XMLNames.cpp
xmlnames.commands = perl $$PWD/ksvg2/scripts/make_names.pl --attrs $$PWD/xml/xmlattrs.in --namespace XML --cppNamespace WebCore --namespaceURI 'http://www.w3.org/XML/1998/namespace' --output tmp
xmlnames.input = XML_NAMES
xmlnames.dependency_type = TYPE_C
xmlnames.CONFIG = target_predeps
xmlnames.variable_out = GENERATED_SOURCES
xmlnames.clean = ${QMAKE_FILE_OUT} tmp/XMLNames.h
QMAKE_EXTRA_COMPILERS += xmlnames

# GENERATOR 6-A:
cssprops.output = tmp/CSSPropertyNames.c
cssprops.input = WALDOCSSPROPS
cssprops.commands = cp ${QMAKE_FILE_NAME} tmp && cd tmp && sh $$PWD/css/makeprop && rm ${QMAKE_FILE_BASE}.strip ${QMAKE_FILE_BASE}.in ${QMAKE_FILE_BASE}.gperf
cssprops.CONFIG = target_predeps no_link
cssprops.clean = ${QMAKE_FILE_OUT} tmp/CSSPropertyNames.h
QMAKE_EXTRA_COMPILERS += cssprops

# GENERATOR 6-B:
cssvalues.output = tmp/CSSValueKeywords.c
cssvalues.input = WALDOCSSVALUES
cssvalues.commands = cp ${QMAKE_FILE_NAME} tmp && cd tmp && sh $$PWD/css/makevalues && rm CSSValueKeywords.in CSSValueKeywords.strip CSSValueKeywords.gperf
cssvalues.CONFIG = target_predeps no_link
cssvalues.clean = ${QMAKE_FILE_OUT} tmp/CSSValueKeywords.h
QMAKE_EXTRA_COMPILERS += cssvalues

# GENERATOR 7-A:
svgcssproperties.output = tmp/ksvgcssproperties.h
svgcssproperties.input = SVGCSSPROPERTIES
svgcssproperties.commands = cp $$PWD/ksvg2/css/CSSPropertyNames.in tmp/ksvgcssproperties.in && cd tmp && perl $$PWD/ksvg2/scripts/cssmakeprops -n SVG -f ksvgcssproperties.in && rm ksvgcssproperties.in ksvgcssproperties.gperf
svgcssproperties.CONFIG = target_predeps no_link
svgcssproperties.clean = ${QMAKE_FILE_OUT} tmp/ksvgcssproperties.c
QMAKE_EXTRA_COMPILERS += svgcssproperties

# GENERATOR 7-B:
svgcssvalues.output = tmp/ksvgcssvalues.h
svgcssvalues.input = SVGCSSVALUES
svgcssvalues.commands = perl -ne \'print lc\' $$PWD/ksvg2/css/CSSValueKeywords.in > tmp/ksvgcssvalues.in && cd tmp && perl $$PWD/ksvg2/scripts/cssmakevalues -n SVG -f ksvgcssvalues.in && rm ksvgcssvalues.in ksvgcssvalues.gperf
svgcssvalues.CONFIG = target_predeps no_link
svgcssvalues.clean = ${QMAKE_FILE_OUT} tmp/ksvgcssvalues.c tmp/CSSValueKeywords.h
QMAKE_EXTRA_COMPILERS += svgcssvalues

# GENERATOR 8-A:
entities.output = tmp/HTMLEntityNames.c
entities.commands = gperf -a -L ANSI-C -C -G -c -o -t -k '\*' -N findEntity -D -s 2 < $$PWD/html/HTMLEntityNames.gperf > tmp/HTMLEntityNames.c
entities.input = ENTITIES_GPERF
entities.dependency_type = TYPE_C
entities.CONFIG = target_predeps no_link
entities.clean = ${QMAKE_FILE_OUT}
QMAKE_EXTRA_COMPILERS += entities

# PRE-GENERATOR 8-B:
doctypestrings.target = $$OUTPUT_DIR/WebCore/tmp/DocTypeStrings.cpp
doctypestrings.commands = echo \"$${LITERAL_HASH}include <string.h>\" > $$doctypestrings.target && gperf -CEot -L ANSI-C -k \'*\' -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards < $$PWD/html/DocTypeStrings.gperf >> $$doctypestrings.target
QMAKE_EXTRA_TARGETS += doctypestrings
PRE_TARGETDEPS += $$OUTPUT_DIR/WebCore/tmp/DocTypeStrings.cpp
CLEAN_FILES += $$doctypestrings.target

# GENERATOR 8-C:
colordata.output = tmp/ColorData.c
colordata.commands = echo \"$${LITERAL_HASH}include <string.h>\" > ${QMAKE_FILE_OUT} && gperf -CDEot -L ANSI-C -k \'*\' -N findColor -D -s 2 < ${QMAKE_FILE_NAME} >> ${QMAKE_FILE_OUT}
colordata.input = COLORDAT_GPERF
colordata.CONFIG = target_predeps no_link
QMAKE_EXTRA_COMPILERS += colordata

# GENERATOR 9:
stylesheets.output = tmp/UserAgentStyleSheetsData.cpp
stylesheets.commands = perl $$PWD/css/make-css-file-arrays.pl tmp/UserAgentStyleSheets.h tmp/UserAgentStyleSheetsData.cpp $$PWD/css/html4.css $$PWD/css/quirks.css $$PWD/css/svg.css $$PWD/css/view-source.css
stylesheets.input = STYLESHEETS_EMBED
stylesheets.CONFIG = target_predeps
stylesheets.variable_out = GENERATED_SOURCES
stylesheets.clean = ${QMAKE_FILE_OUT} tmp/UserAgentStyleSheets.h
QMAKE_EXTRA_COMPILERS += stylesheets

# GENERATOR 10: XPATH grammar
xpathbison.output = tmp/${QMAKE_FILE_BASE}.cpp
xpathbison.commands = bison -d -p xpathyy ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_BASE}.tab.c && mv ${QMAKE_FILE_BASE}.tab.c tmp/${QMAKE_FILE_BASE}.cpp && mv ${QMAKE_FILE_BASE}.tab.h tmp/${QMAKE_FILE_BASE}.h
xpathbison.depend = ${QMAKE_FILE_NAME}
xpathbison.input = XPATHBISON
xpathbison.CONFIG = target_predeps
xpathbison.dependency_type = TYPE_C
xpathbison.variable_out = GENERATED_SOURCES
xpathbison.clean = ${QMAKE_FILE_OUT} tmp/${QMAKE_FILE_BASE}.h
QMAKE_EXTRA_COMPILERS += xpathbison

qt-port {
    target.path = $$[QT_INSTALL_LIBS]
    include($$PWD/../WebKitQt/Api/headers.pri)
    headers.files = $$WEBKIT_API_HEADERS
    headers.path = $$[QT_INSTALL_HEADERS]/QtWebKit
    prf.files = $$PWD/../WebKitQt/Api/qtwebkit.prf
    prf.path = $$[QT_INSTALL_PREFIX]/mkspecs/features

    INSTALLS += target headers prf

    unix {
        CONFIG += create_pc create_prl
        QMAKE_PKGCONFIG_LIBDIR = $$target.path
        QMAKE_PKGCONFIG_INCDIR = $$headers.path
        QMAKE_PKGCONFIG_DESTDIR = pkgconfig
        lib_replace.match = $$DESTDIR
        lib_replace.replace = $$[QT_INSTALL_LIBS]
        QMAKE_PKGCONFIG_INSTALL_REPLACE += lib_replace
    }
}

