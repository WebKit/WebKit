# WebCore - Qt4 build info

TEMPLATE = lib
TARGET = WebKitQt
OBJECTS_DIR = tmp
INCLUDEPATH += tmp

isEmpty(OUTPUT_DIR):OUTPUT_DIR=$$PWD/..
DESTDIR = $$OUTPUT_DIR/lib

include($$OUTPUT_DIR/config.pri)

CONFIG -= warn_on
#QMAKE_CXXFLAGS_RELEASE += -Wall -Wno-undef -Wno-unused-parameter

# PRE-BUILD: make the required config.h file
#config_h.target = config.h
#config_h.commands = cp config.h.qmake config.h
#config_h.depends = config.h.qmake
#QMAKE_EXTRA_TARGETS += config_h
#PRE_TARGETDEPS += config.h

# Optional components (look for defs in config.h and included files!)
DEFINES += XPATH_SUPPORT=1
DEFINES += XSLT_SUPPORT=1
#DEFINES += XBL_SUPPORT=1
DEFINES += SVG_SUPPORT=1

DEFINES += WTF_CHANGES=1 BUILDING_QT__=1

INCLUDEPATH += $$PWD/../JavaScriptCore
LIBS += -L$$OUTPUT_DIR/lib -lJavaScriptCore

macx {
    INCLUDEPATH += /opt/local/include /opt/local/include/libxml2
    INCLUDEPATH += /usr/include/libxml2
    LIBS += -L/opt/local/lib -lxml2 -lxslt
}

INCLUDEPATH +=  $$PWD \
                $$[QT_INSTALL_PREFIX]/src/3rdparty/sqlite/ \
                $$PWD/ForwardingHeaders \
                $$PWD/../JavaScriptCore/kjs \
                $$PWD/../JavaScriptCore/bindings \
                $$PWD/platform \
                $$PWD/platform/qt \
                $$PWD/platform/network \
                $$PWD/platform/network/qt \
                $$PWD/platform/graphics \
                $$PWD/platform/graphics/qt \
                $$PWD/platform/graphics/svg \
                $$PWD/platform/graphics/svg/qt \
                $$PWD/platform/graphics/svg/filters \
                $$PWD/loader $$PWD/loader/icon $$PWD/loader/qt \
                $$PWD/css \
                $$PWD/dom \
                $$PWD/page \
                $$PWD/page/qt \
                $$PWD/bridge \
                $$PWD/editing \
                $$PWD/rendering \
                $$PWD/history \
                $$PWD/xml \
                $$PWD/html \
                $$PWD/bindings/js \
                $$PWD/kcanvas $$PWD/kcanvas/device $$PWD/kcanvas/device/qt \
                $$PWD/ksvg2 $$PWD/ksvg2/css $$PWD/ksvg2/svg $$PWD/ksvg2/misc $$PWD/ksvg2/events \
                $$PWD/platform/image-decoders \
                $$PWD/../WebKitQt/WebCoreSupport \
                $$PWD/WebCore+SVG
QT += network
!mac:CONFIG += link_pkgconfig
PKGCONFIG += libxslt
LIBS += -lsqlite3


FEATURE_DEFINES_JAVASCRIPT = LANGUAGE_JAVASCRIPT



TOKENIZER = $$PWD/css/tokenizer.flex

CSSBISON = $$PWD/css/CSSGrammar.y

HTML_NAMES = $$PWD/html/HTMLTagNames.in

ENTITIES_GPERF = $$PWD/html/HTMLEntityNames.gperf

COLORDAT_GPERF = $$PWD/platform/ColorData.gperf

WALDOCSSPROPS = $$PWD/css/CSSPropertyNames.in

WALDOCSSVALUES = $$PWD/css/CSSValueKeywords.in

SVGCSSPROPERTIES = $$PWD/ksvg2/css/CSSPropertyNames.in

SVGCSSVALUES = $$PWD/ksvg2/css/CSSValueKeywords.in

STYLESHEETS_EMBED = $$PWD/css/html4.css

MANUALMOC += \
    $$PWD/platform/qt/SharedTimerQt.h \
    $$PWD/platform/qt/ScrollViewCanvasQt.h \
    $$PWD/platform/network/qt/ResourceHandleManagerQt.h

LUT_FILES += \
    bindings/js/kjs_window.cpp \
    bindings/js/kjs_css.cpp \
    bindings/js/kjs_dom.cpp \
    bindings/js/kjs_html.cpp \
    bindings/js/kjs_events.cpp \
    bindings/js/kjs_navigator.cpp \
    bindings/js/kjs_traversal.cpp \
    bindings/js/JSXMLHttpRequest.cpp \
    bindings/js/JSXSLTProcessor.cpp

LUT_TABLE_FILES += \
    bindings/js/JSHTMLInputElementBase.cpp

IDL_BINDINGS += \
    css/CSSValue.idl \
    css/CSSRuleList.idl \
    css/CSSValueList.idl \
    css/CSSStyleDeclaration.idl \
    css/CSSPrimitiveValue.idl \
    css/CSSRule.idl \
    css/Counter.idl \
    css/MediaList.idl \
    dom/Event.idl \
#    dom/EventListener.idl \
#    dom/EventTarget.idl \
    dom/Range.idl \
    dom/Text.idl \
    dom/DOMImplementation.idl \
    dom/NodeFilter.idl \
    dom/MouseEvent.idl \
    dom/CharacterData.idl \
    dom/DocumentFragment.idl \
    dom/Entity.idl \
    dom/UIEvent.idl \
    dom/Node.idl \
    dom/ProcessingInstruction.idl \
    dom/Notation.idl \
    dom/Element.idl \
    dom/DocumentType.idl \
    dom/Document.idl \
    dom/Attr.idl \
    dom/MutationEvent.idl \
    dom/KeyboardEvent.idl \
    dom/WheelEvent.idl \
    dom/OverflowEvent.idl \
    dom/NodeIterator.idl \
    dom/TreeWalker.idl \
    dom/RangeException.idl \
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
    html/HTMLDirectoryElement.idl \
    html/HTMLDivElement.idl \
    html/HTMLDListElement.idl \
    html/HTMLDocument.idl \
    html/HTMLElement.idl \
    html/HTMLFieldSetElement.idl \
    html/HTMLFontElement.idl \
    html/HTMLFormElement.idl \
    html/HTMLHeadElement.idl \
    html/HTMLHeadingElement.idl \
    html/HTMLHRElement.idl \
    html/HTMLHtmlElement.idl \
    html/HTMLImageElement.idl \
    html/HTMLInputElement.idl \
    html/HTMLIsIndexElement.idl \
    html/HTMLLabelElement.idl \
    html/HTMLLegendElement.idl \
    html/HTMLLIElement.idl \
    html/HTMLLinkElement.idl \
    html/HTMLMapElement.idl \
    html/HTMLMenuElement.idl \
    html/HTMLMetaElement.idl \
    html/HTMLModElement.idl \
    html/HTMLOListElement.idl \
    html/HTMLOptGroupElement.idl \
    html/HTMLOptionElement.idl \
    html/HTMLParagraphElement.idl \
    html/HTMLParamElement.idl \
    html/HTMLPreElement.idl \
    html/HTMLQuoteElement.idl \
    html/HTMLScriptElement.idl \
    html/HTMLStyleElement.idl \
    html/HTMLTextAreaElement.idl \
    html/HTMLTitleElement.idl \
    html/HTMLUListElement.idl \
    html/HTMLOptionsCollection.idl \
    xml/DOMParser.idl \
    xml/XMLSerializer.idl \
    page/DOMWindow.idl

SOURCES += \
    bindings/js/kjs_binding.cpp \
    bindings/js/kjs_css.cpp \
    bindings/js/kjs_dom.cpp \
    bindings/js/kjs_events.cpp \
    bindings/js/kjs_html.cpp \
    bindings/js/kjs_navigator.cpp \
    bindings/js/kjs_proxy.cpp \
    bindings/js/kjs_traversal.cpp \
    bindings/js/kjs_window.cpp \
    bindings/js/JSCanvasRenderingContext2DCustom.cpp \
    bindings/js/JSHTMLElementWrapperFactory.cpp \
    bindings/js/JSHTMLFormElementCustom.cpp \
    bindings/js/JSHTMLOptionElementConstructor.cpp \
    bindings/js/JSHTMLOptionsCollectionCustom.cpp \
    bindings/js/JSHTMLInputElementBase.cpp \
    bindings/js/JSXMLHttpRequest.cpp \
    bindings/js/JSNodeIteratorCustom.cpp \
    bindings/js/JSTreeWalkerCustom.cpp \
    bindings/js/JSXSLTProcessor.cpp \
    css/cssparser.cpp \
    css/cssstyleselector.cpp \
    css/csshelper.cpp \
    css/FontValue.cpp \
    css/CSSSelector.cpp \
    css/MediaFeatureNames.cpp \
    css/CSSRuleList.cpp \
    css/CSSCharsetRule.cpp \
    css/MediaQueryExp.cpp \
    css/CSSImportRule.cpp \
    css/CSSValueList.cpp \
    css/CSSStyleDeclaration.cpp \
    css/CSSPrimitiveValue.cpp \
    css/CSSProperty.cpp \
    css/CSSCursorImageValue.cpp \
    css/CSSBorderImageValue.cpp \
    css/MediaQuery.cpp \
    css/StyleSheet.cpp \
    css/CSSPageRule.cpp \
    css/StyleSheetList.cpp \
    css/MediaQueryEvaluator.cpp \
    css/StyleBase.cpp \
    css/CSSRule.cpp \
    css/CSSStyleSheet.cpp \
    css/CSSInitialValue.cpp \
    css/CSSImageValue.cpp \
    css/CSSStyleRule.cpp \
    css/CSSInheritedValue.cpp \
    css/StyleList.cpp \
    css/FontFamilyValue.cpp \
    css/CSSMediaRule.cpp \
    css/CSSComputedStyleDeclaration.cpp \
    css/CSSMutableStyleDeclaration.cpp \
    css/MediaList.cpp \
    css/CSSFontFaceRule.cpp \
    css/ShadowValue.cpp \
    dom/Event.cpp \
    dom/EventTarget.cpp \
    dom/Range.cpp \
    dom/Text.cpp \
    dom/DOMImplementation.cpp \
    dom/NodeFilter.cpp \
    dom/MouseEvent.cpp \
    dom/EntityReference.cpp \
    dom/NameNodeList.cpp \
    dom/CharacterData.cpp \
    dom/XMLTokenizer.cpp \
    dom/StyleElement.cpp \
    dom/StyledElement.cpp \
    dom/MappedAttribute.cpp \
    dom/NamedAttrMap.cpp \
    dom/ContainerNode.cpp \
    dom/NamedMappedAttrMap.cpp \
    dom/EventNames.cpp \
    dom/Comment.cpp \
    dom/EditingText.cpp \
    dom/DocumentFragment.cpp \
    dom/ChildNodeList.cpp \
    dom/Entity.cpp \
    dom/BeforeTextInsertedEvent.cpp \
    dom/UIEvent.cpp \
    dom/Node.cpp \
    dom/Attribute.cpp \
    dom/Position.cpp \
    dom/ProcessingInstruction.cpp \
    dom/TreeWalker.cpp \
    dom/Notation.cpp \
    dom/Element.cpp \
    dom/NodeFilterCondition.cpp \
    dom/CDATASection.cpp \
    dom/DocumentType.cpp \
    dom/NodeList.cpp \
    dom/CSSMappedAttributeDeclaration.cpp \
    dom/QualifiedName.cpp \
    dom/Document.cpp \
    dom/Attr.cpp \
    dom/OverflowEvent.cpp \
    dom/RegisteredEventListener.cpp \
    dom/EventTargetNode.cpp \
    dom/BeforeUnloadEvent.cpp \
    dom/MutationEvent.cpp \
    dom/MouseRelatedEvent.cpp \
    dom/KeyboardEvent.cpp \
    dom/NodeIterator.cpp \
    dom/ClipboardEvent.cpp \
    dom/Traversal.cpp \
    dom/WheelEvent.cpp \
    dom/UIEventWithKeyState.cpp \
    editing/DeleteButtonController.cpp \
    editing/DeleteButton.cpp \
    editing/Editor.cpp \
    editing/CommandByName.cpp \
    editing/InsertIntoTextNodeCommand.cpp \
    editing/WrapContentsInDummySpanCommand.cpp \
    editing/ReplaceSelectionCommand.cpp \
    editing/MoveSelectionCommand.cpp \
    editing/RemoveNodePreservingChildrenCommand.cpp \
    editing/HTMLInterchange.cpp \
    editing/UnlinkCommand.cpp \
    editing/InsertLineBreakCommand.cpp \
    editing/FormatBlockCommand.cpp \
    editing/AppendNodeCommand.cpp \
    editing/BreakBlockquoteCommand.cpp \
    editing/htmlediting.cpp \
    editing/markup.cpp \
    editing/InsertParagraphSeparatorCommand.cpp \
    editing/ModifySelectionListLevel.cpp \
    editing/JSEditor.cpp \
    editing/Selection.cpp \
    editing/TextIterator.cpp \
    editing/InsertListCommand.cpp \
    editing/IndentOutdentCommand.cpp \
    editing/InsertNodeBeforeCommand.cpp \
    editing/SplitTextNodeContainingElementCommand.cpp \
    editing/TypingCommand.cpp \
    editing/MergeIdenticalElementsCommand.cpp \
    editing/EditCommand.cpp \
    editing/SplitTextNodeCommand.cpp \
    editing/RemoveCSSPropertyCommand.cpp \
    editing/JoinTextNodesCommand.cpp \
    editing/InsertTextCommand.cpp \
    editing/SelectionController.cpp \
    editing/DeleteSelectionCommand.cpp \
    editing/SplitElementCommand.cpp \
    editing/VisiblePosition.cpp \
    editing/ApplyStyleCommand.cpp \
    editing/visible_units.cpp \
    editing/RemoveNodeAttributeCommand.cpp \
    editing/DeleteFromTextNodeCommand.cpp \
    editing/RemoveNodeCommand.cpp \
    editing/CompositeEditCommand.cpp \
    editing/SetNodeAttributeCommand.cpp \
    editing/CreateLinkCommand.cpp \
    editing/qt/EditorQt.cpp \
    xml/XSLStyleSheet.cpp \
    xml/XSLTProcessor.cpp \
    xml/XSLImportRule.cpp \
    xml/DOMParser.cpp \
    xml/XMLSerializer.cpp \
    xml/xmlhttprequest.cpp \
#    icon/IconDatabase.cpp \
#    icon/SQLTransaction.cpp \
#    icon/SQLStatement.cpp \
#    icon/SiteIcon.cpp \
#    icon/SQLDatabase.cpp \
    html/HTMLParser.cpp \
    html/HTMLFontElement.cpp \
    html/HTMLEmbedElement.cpp \
    html/HTMLLinkElement.cpp \
    html/HTMLOptGroupElement.cpp \
    html/HTMLCanvasElement.cpp \
    html/HTMLTitleElement.cpp \
    html/CanvasRenderingContext2D.cpp \
    html/HTMLObjectElement.cpp \
    html/HTMLAppletElement.cpp \
    html/HTMLKeygenElement.cpp \
    html/HTMLDivElement.cpp \
    html/HTMLMapElement.cpp \
    html/HTMLScriptElement.cpp \
    html/HTMLHtmlElement.cpp \
    html/HTMLTokenizer.cpp \
    html/HTMLOptionElement.cpp \
    html/HTMLTableCaptionElement.cpp \
    html/HTMLImageLoader.cpp \
    html/FormDataList.cpp \
    html/HTMLLabelElement.cpp \
    html/HTMLTableColElement.cpp \
    html/HTMLDListElement.cpp \
    html/HTMLTablePartElement.cpp \
    html/HTMLTableSectionElement.cpp \
    html/HTMLTextAreaElement.cpp \
    html/HTMLTextFieldInnerElement.cpp \
    html/HTMLAreaElement.cpp \
    html/CanvasStyle.cpp \
    html/HTMLIsIndexElement.cpp \
    html/HTMLHeadElement.cpp \
    html/HTMLFrameSetElement.cpp \
    html/HTMLBodyElement.cpp \
    html/HTMLBRElement.cpp \
    html/HTMLFrameOwnerElement.cpp \
    html/HTMLNameCollection.cpp \
    html/HTMLLegendElement.cpp \
    html/HTMLLIElement.cpp \
    html/HTMLParamElement.cpp \
    html/HTMLMetaElement.cpp \
    html/HTMLHeadingElement.cpp \
    html/HTMLUListElement.cpp \
    html/HTMLInputElement.cpp \
    html/HTMLElementFactory.cpp \
    html/HTMLPlugInElement.cpp \
    html/HTMLFieldSetElement.cpp \
    html/HTMLParagraphElement.cpp \
    html/HTMLStyleElement.cpp \
    html/HTMLMarqueeElement.cpp \
    html/HTMLGenericFormElement.cpp \
    html/HTMLElement.cpp \
    html/HTMLDocument.cpp \
    html/HTMLOListElement.cpp \
    html/HTMLFormElement.cpp \
    html/HTMLPreElement.cpp \
    html/HTMLTableElement.cpp \
    html/CanvasGradient.cpp \
    html/HTMLViewSourceDocument.cpp \
    html/HTMLFrameElement.cpp \
    html/HTMLFrameElementBase.cpp \
    html/HTMLAnchorElement.cpp \
    html/HTMLTableCellElement.cpp \
    html/CanvasPattern.cpp \
    html/HTMLBlockquoteElement.cpp \
    html/HTMLIFrameElement.cpp \
    html/HTMLMenuElement.cpp \
    html/HTMLCollection.cpp \
    html/HTMLModElement.cpp \
    html/HTMLQuoteElement.cpp \
    html/HTMLDirectoryElement.cpp \
    html/HTMLSelectElement.cpp \
    html/HTMLImageElement.cpp \
    html/HTMLOptionsCollection.cpp \
    html/HTMLTableRowElement.cpp \
    html/HTMLBaseFontElement.cpp \
    html/HTMLHRElement.cpp \
    html/HTMLButtonElement.cpp \
    html/HTMLFormCollection.cpp \
    html/HTMLBaseElement.cpp \
    page/FrameTree.cpp \
    page/FocusController.cpp \
    page/DOMWindow.cpp \
    page/MouseEventWithHitTestResults.cpp \
    page/Frame.cpp \
    page/Settings.cpp \
    page/Page.cpp \
    page/Chrome.cpp \
    page/FrameView.cpp \
    page/PageState.cpp \
    page/ContextMenuController.cpp \
    page/EventHandler.cpp \
    page/qt/EventHandlerQt.cpp \
    page/qt/FrameQt.cpp \
    page/qt/FrameQtClient.cpp \
    xml/XPathUtil.cpp \
    xml/XPathPredicate.cpp \
    xml/XPathVariableReference.cpp \
    xml/XPathValue.cpp \
    xml/XPathPath.cpp \
    xml/XPathFunctions.cpp \
    xml/XPathParser.cpp \
    xml/XPathStep.cpp \
    xml/XPathExpressionNode.cpp \
    xml/XPathNamespace.cpp \
    xml/XPathNSResolver.cpp \
    xml/XPathExpression.cpp \
    xml/XPathResult.cpp \
    xml/XPathEvaluator.cpp \
    loader/Cache.cpp \
    loader/CachedCSSStyleSheet.cpp \
    loader/CachedImage.cpp \
    loader/CachedResource.cpp \
    loader/CachedResourceClientWalker.cpp \
    loader/CachedScript.cpp \
    loader/CachedXSLStyleSheet.cpp \
    loader/DocLoader.cpp \
    loader/DocumentLoader.cpp \
    loader/FormState.cpp \
    loader/FrameLoader.cpp \
    loader/ImageDocument.cpp \
    loader/MainResourceLoader.cpp \
    loader/NetscapePlugInStreamLoader.cpp \
    loader/PluginDocument.cpp \
    loader/Request.cpp \
    loader/ResourceLoader.cpp \
    loader/SubresourceLoader.cpp \
    loader/TextDocument.cpp \
    loader/TextResourceDecoder.cpp \
    loader/loader.cpp \
    loader/icon/IconLoader.cpp \
    loader/icon/IconDatabase.cpp \ 
    loader/icon/IconDataCache.cpp \
    loader/icon/SQLTransaction.cpp \
    loader/icon/SQLStatement.cpp \
    loader/icon/SQLDatabase.cpp \
    loader/qt/FrameLoaderQt.cpp \
    loader/qt/DocumentLoaderQt.cpp \
    loader/qt/NavigationActionQt.cpp \
    platform/CString.cpp \
    platform/AtomicString.cpp \
    platform/Base64.cpp \
    platform/graphics/AffineTransform.cpp \
    platform/TextStream.cpp \
    platform/Widget.cpp \
    platform/GlyphWidthMap.cpp \
    platform/graphics/Pen.cpp \
    platform/graphics/Image.cpp \
    platform/DeprecatedStringList.cpp \
    platform/graphics/FloatSize.cpp \
    platform/graphics/PathTraversalState.cpp \
    platform/String.cpp \
    platform/DeprecatedValueListImpl.cpp \
    platform/graphics/IntRect.cpp \
    platform/Arena.cpp \
    platform/ArrayImpl.cpp \
    platform/graphics/FloatPoint3D.cpp \
    platform/graphics/FloatPoint.cpp \
    platform/SegmentedString.cpp \
    platform/TextCodec.cpp \
    platform/qt/TextCodecQt.cpp \
    platform/DeprecatedString.cpp \
    platform/DeprecatedCString.cpp \
    platform/SharedBuffer.cpp \
    platform/qt/TextBreakIteratorQt.cpp \
    platform/TextCodecLatin1.cpp \
    platform/TextCodecUTF16.cpp \
    platform/TextDecoder.cpp \
    platform/TextEncoding.cpp \
    platform/TextEncodingRegistry.cpp \
    platform/Logging.cpp \
    platform/graphics/Color.cpp \
    platform/graphics/ImageBuffer.cpp \
    platform/DeprecatedPtrListImpl.cpp \
    platform/KURL.cpp \
    platform/StringImpl.cpp \
    platform/graphics/FloatRect.cpp \
    platform/graphics/Path.cpp \
    platform/MimeTypeRegistry.cpp \
    platform/qt/MimeTypeRegistryQt.cpp \
    platform/qt/SoundQt.cpp \
    platform/qt/FileChooserQt.cpp \
    platform/graphics/qt/IconQt.cpp \
    platform/graphics/qt/ImageBufferQt.cpp \
    platform/graphics/qt/AffineTransformQt.cpp \
    platform/qt/StringQt.cpp \
    platform/graphics/qt/ColorQt.cpp \
    platform/qt/GlyphMapQt.cpp \
    platform/qt/CookieJarQt.cpp \
    platform/qt/FontPlatformDataQt.cpp \
    platform/qt/ScrollViewQt.cpp \
    platform/qt/TemporaryLinkStubs.cpp \
    platform/qt/CursorQt.cpp \
    platform/qt/WidgetQt.cpp \
    platform/qt/SystemTimeQt.cpp \
    platform/qt/RenderThemeQt.cpp \
    platform/qt/FontDataQt.cpp \
    platform/qt/SharedTimerQt.cpp \
    platform/qt/PopupMenuQt.cpp \
    platform/qt/ContextMenuQt.cpp \
    platform/qt/ContextMenuItemQt.cpp \
    platform/qt/PasteboardQt.cpp \
    platform/ContextMenu.cpp \
#    platform/SearchPopupMenu.cpp \ 
    platform/qt/SearchPopupMenuQt.cpp \ 
    platform/network/FormData.cpp \
    platform/network/ResourceHandle.cpp \
    platform/network/ResourceRequest.cpp \
    platform/network/ResourceResponse.cpp \
    platform/network/qt/ResourceHandleQt.cpp \
    platform/network/qt/ResourceHandleManagerQt.cpp \
    platform/network/HTTPParsers.cpp \
    platform/graphics/BitmapImage.cpp \
    platform/graphics/qt/FloatPointQt.cpp \
    platform/graphics/qt/FloatRectQt.cpp \
    platform/graphics/qt/GraphicsContextQt.cpp \
    platform/graphics/qt/IntPointQt.cpp \
    platform/graphics/qt/IntRectQt.cpp \
    platform/graphics/qt/IntSizeQt.cpp \
    platform/graphics/qt/PathQt.cpp \
    platform/graphics/qt/ImageQt.cpp \
    platform/graphics/qt/ImageSourceQt.cpp \
    platform/graphics/qt/ImageDecoderQt.cpp \
    platform/qt/FontCacheQt.cpp \
    platform/qt/FontQt.cpp \
    platform/qt/ScreenQt.cpp \
    platform/qt/ScrollViewCanvasQt.cpp \
    platform/qt/PlatformMouseEventQt.cpp \
    platform/qt/PlatformKeyboardEventQt.cpp \
    platform/qt/TextBoundaries.cpp \
    platform/graphics/GraphicsTypes.cpp \
    platform/graphics/GraphicsContext.cpp \
    platform/FontFamily.cpp \
    platform/Timer.cpp \
    platform/FontCache.cpp \
    platform/FontFallbackList.cpp \
    platform/RegularExpression.cpp \
    platform/GlyphMap.cpp \
    platform/Font.cpp \
    platform/FontData.cpp \
    rendering/HitTestResult.cpp \
    rendering/RenderCounter.cpp \
    rendering/CounterNode.cpp \
    rendering/CounterResetNode.cpp \
    rendering/RenderListBox.cpp \
    rendering/RenderReplaced.cpp \
    rendering/RenderPartObject.cpp \
    rendering/RenderView.cpp \
    rendering/RenderMenuList.cpp \
    rendering/InlineFlowBox.cpp \
    rendering/RenderListMarker.cpp \
    rendering/RenderImage.cpp \
    rendering/RenderTheme.cpp \
    rendering/RenderLayer.cpp \
    rendering/RenderTableCell.cpp \
    rendering/RenderListItem.cpp \
    rendering/AutoTableLayout.cpp \
    rendering/RenderArena.cpp \
    rendering/RenderWidget.cpp \
    rendering/break_lines.cpp \
    rendering/RenderStyle.cpp \
    rendering/RenderContainer.cpp \
    rendering/EllipsisBox.cpp \
    rendering/RenderFieldset.cpp \
    rendering/RenderFrameSet.cpp \
    rendering/RenderTable.cpp \
    rendering/RenderPart.cpp \
    rendering/RenderBlock.cpp \
    rendering/InlineBox.cpp \
    rendering/RenderText.cpp \
    rendering/RenderFrame.cpp \
    rendering/FixedTableLayout.cpp \
    rendering/RenderTableCol.cpp \
    rendering/RenderObject.cpp \
    rendering/RenderTreeAsText.cpp \
    rendering/SVGRenderTreeAsText.cpp \
    rendering/RootInlineBox.cpp \
    rendering/RenderBox.cpp \
    rendering/RenderButton.cpp \
    rendering/RenderTableSection.cpp \
    rendering/ListMarkerBox.cpp \
    rendering/RenderTableRow.cpp \
    rendering/RenderInline.cpp \
    rendering/RenderFileUploadControl.cpp \
    rendering/RenderHTMLCanvas.cpp \
    rendering/bidi.cpp \
    rendering/RenderFlexibleBox.cpp \
    rendering/RenderApplet.cpp \
    rendering/RenderLegend.cpp \
    rendering/RenderTextControl.cpp \
    rendering/RenderTextFragment.cpp \
    rendering/RenderBR.cpp \
    rendering/InlineTextBox.cpp \
    rendering/RenderFlow.cpp \
    rendering/RenderSlider.cpp \
    ../WebKitQt/WebCoreSupport/FrameLoaderClientQt.cpp \
    ../WebKitQt/WebCoreSupport/EditorClientQt.cpp \
    ../WebKitQt/WebCoreSupport/ChromeClientQt.cpp \
    ../WebKitQt/WebCoreSupport/ContextMenuClientQt.cpp

contains(DEFINES, XPATH_SUPPORT=1) {
    FEATURE_DEFINES_JAVASCRIPT += XPATH_SUPPORT

    XPATHBISON = $$PWD/xml/XPathGrammar.y

    IDL_BINDINGS += \
        xml/XPathNSResolver.idl \
        xml/XPathExpression.idl \
        xml/XPathResult.idl \
        xml/XPathEvaluator.idl
}

contains(DEFINES, XSLT_SUPPORT=1) {
    FEATURE_DEFINES_JAVASCRIPT += XSLT_SUPPORT
}

contains(DEFINES, XBL_SUPPORT=1) {
    FEATURE_DEFINES_JAVASCRIPT += XBL_SUPPORT
}

contains(DEFINES, SVG_SUPPORT=1) {
    FEATURE_DEFINES_JAVASCRIPT += SVG_SUPPORT

    SVG_NAMES = $$PWD/ksvg2/svg/svgtags.in

    XML_NAMES = $$PWD/xml/xmlattrs.in

    XLINK_NAMES = $$PWD/ksvg2/misc/xlinkattrs.in

    IDL_BINDINGS += ksvg2/svg/SVGAElement.idl \
        ksvg2/svg/SVGAngle.idl \
        ksvg2/svg/SVGAnimateColorElement.idl \
        ksvg2/svg/SVGAnimateElement.idl \
        ksvg2/svg/SVGAnimateTransformElement.idl \
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
        ksvg2/svg/SVGLineElement.idl \
        ksvg2/svg/SVGLinearGradientElement.idl \
        ksvg2/svg/SVGMaskElement.idl \
        ksvg2/svg/SVGMarkerElement.idl \
        ksvg2/svg/SVGMatrix.idl \
        ksvg2/svg/SVGMetadataElement.idl \
        ksvg2/svg/SVGNumberList.idl \
        ksvg2/svg/SVGPaint.idl \
        ksvg2/svg/SVGPathElement.idl \
        ksvg2/svg/SVGPathSeg.idl \
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
        ksvg2/svg/SVGPointList.idl \
        ksvg2/svg/SVGPolygonElement.idl \
        ksvg2/svg/SVGPolylineElement.idl \
        ksvg2/svg/SVGPreserveAspectRatio.idl \
        ksvg2/svg/SVGRadialGradientElement.idl \
        ksvg2/svg/SVGRectElement.idl \
        ksvg2/svg/SVGRenderingIntent.idl \
        ksvg2/svg/SVGSetElement.idl \
        ksvg2/svg/SVGScriptElement.idl \
        ksvg2/svg/SVGStyleElement.idl \
        ksvg2/svg/SVGSwitchElement.idl \
        ksvg2/svg/SVGStopElement.idl \
        ksvg2/svg/SVGStringList.idl \
        ksvg2/svg/SVGSymbolElement.idl \
        ksvg2/svg/SVGSVGElement.idl \
        ksvg2/svg/SVGTRefElement.idl \
        ksvg2/svg/SVGTSpanElement.idl \
        ksvg2/svg/SVGTextElement.idl \
        ksvg2/svg/SVGTextContentElement.idl \
        ksvg2/svg/SVGTextPositioningElement.idl \
        ksvg2/svg/SVGTitleElement.idl \
        ksvg2/svg/SVGTransform.idl \
        ksvg2/svg/SVGTransformList.idl \
        ksvg2/svg/SVGUnitTypes.idl \
        ksvg2/svg/SVGUseElement.idl \
        ksvg2/svg/SVGViewElement.idl \
        ksvg2/events/SVGZoomEvent.idl \
        ksvg2/svg/SVGNumber.idl \
        ksvg2/svg/SVGPoint.idl \
        ksvg2/svg/SVGRect.idl

    SOURCES += \
# TODO: this-one-is-not-auto-added! FIXME! tmp/SVGElementFactory.cpp \
        bindings/js/JSSVGElementWrapperFactory.cpp \
        bindings/js/JSSVGMatrixCustom.cpp \
        bindings/js/JSSVGPathSegCustom.cpp \
        bindings/js/JSSVGPathSegListCustom.cpp \
        bindings/js/JSSVGPointListCustom.cpp \
        ksvg2/css/SVGCSSParser.cpp \
        ksvg2/css/SVGRenderStyleDefs.cpp \
        ksvg2/css/SVGRenderStyle.cpp \
        ksvg2/css/SVGCSSStyleSelector.cpp \
        ksvg2/svg/SVGFEFuncBElement.cpp \
        ksvg2/svg/SVGColor.cpp \
        ksvg2/svg/SVGSwitchElement.cpp \
        ksvg2/svg/SVGFETileElement.cpp \
        ksvg2/svg/SVGDOMImplementation.cpp \
        ksvg2/svg/SVGMarkerElement.cpp \
        ksvg2/svg/SVGFECompositeElement.cpp \
        ksvg2/svg/SVGImageElement.cpp \
        ksvg2/svg/SVGAnimateElement.cpp \
        ksvg2/svg/SVGAnimateMotionElement.cpp \
        ksvg2/svg/SVGURIReference.cpp \
        ksvg2/svg/SVGLength.cpp \
        ksvg2/svg/SVGPathSegCurvetoCubic.cpp \
        ksvg2/svg/SVGExternalResourcesRequired.cpp \
        ksvg2/svg/SVGPolylineElement.cpp \
        ksvg2/svg/SVGFEOffsetElement.cpp \
        ksvg2/svg/SVGFETurbulenceElement.cpp \
        ksvg2/svg/SVGZoomAndPan.cpp \
        ksvg2/svg/SVGFilterPrimitiveStandardAttributes.cpp \
        ksvg2/svg/SVGStyledLocatableElement.cpp \
        ksvg2/svg/SVGLineElement.cpp \
        ksvg2/svg/SVGTransform.cpp \
        ksvg2/svg/SVGPathSegLinetoVertical.cpp \
        ksvg2/svg/SVGFitToViewBox.cpp \
        ksvg2/svg/SVGRadialGradientElement.cpp \
        ksvg2/svg/SVGMaskElement.cpp \
        ksvg2/svg/SVGTitleElement.cpp \
        ksvg2/svg/SVGTRefElement.cpp \
        ksvg2/svg/SVGLangSpace.cpp \
        ksvg2/svg/SVGTransformList.cpp \
        ksvg2/svg/SVGStylable.cpp \
        ksvg2/svg/SVGPolyElement.cpp \
        ksvg2/svg/SVGPolygonElement.cpp \
#        ksvg2/svg/SVGElementInstanceList.cpp \
        ksvg2/svg/SVGTSpanElement.cpp \
        ksvg2/svg/SVGFEFuncRElement.cpp \
        ksvg2/svg/SVGFEFloodElement.cpp \
        ksvg2/svg/SVGPointList.cpp \
        ksvg2/svg/SVGAnimatedPoints.cpp \
        ksvg2/svg/SVGAnimatedPathData.cpp \
        ksvg2/svg/SVGUseElement.cpp \
        ksvg2/svg/SVGNumberList.cpp \
        ksvg2/svg/SVGFEPointLightElement.cpp \
        ksvg2/svg/SVGPathSegLineto.cpp \
        ksvg2/svg/SVGRectElement.cpp \
        ksvg2/svg/SVGTextContentElement.cpp \
        ksvg2/svg/SVGFESpotLightElement.cpp \
        ksvg2/svg/SVGLocatable.cpp \
        ksvg2/svg/SVGEllipseElement.cpp \
        ksvg2/svg/SVGPathElement.cpp \
        ksvg2/svg/SVGStyledElement.cpp \
        ksvg2/svg/SVGFEMergeNodeElement.cpp \
        ksvg2/svg/SVGFEGaussianBlurElement.cpp \
        ksvg2/svg/SVGLinearGradientElement.cpp \
        ksvg2/svg/SVGFEDisplacementMapElement.cpp \
        ksvg2/svg/SVGFEImageElement.cpp \
        ksvg2/svg/SVGFEDiffuseLightingElement.cpp \
        ksvg2/svg/SVGSymbolElement.cpp \
        ksvg2/svg/SVGForeignObjectElement.cpp \
        ksvg2/svg/SVGAngle.cpp \
        ksvg2/svg/SVGPathSegCurvetoQuadratic.cpp \
        ksvg2/svg/SVGSVGElement.cpp \
        ksvg2/svg/SVGFESpecularLightingElement.cpp \
        ksvg2/svg/SVGAnimateColorElement.cpp \
        ksvg2/svg/SVGGElement.cpp \
        ksvg2/svg/SVGFEFuncGElement.cpp \
        ksvg2/svg/SVGFEComponentTransferElement.cpp \
        ksvg2/svg/SVGSetElement.cpp \
        ksvg2/svg/SVGFEBlendElement.cpp \
        ksvg2/svg/SVGFEMergeElement.cpp \
        ksvg2/svg/SVGCursorElement.cpp \
        ksvg2/svg/SVGStringList.cpp \
#        ksvg2/svg/SVGElementInstance.cpp \
        ksvg2/svg/SVGFilterElement.cpp \
        ksvg2/svg/SVGPathSegCurvetoCubicSmooth.cpp \
        ksvg2/svg/SVGPatternElement.cpp \
        ksvg2/svg/SVGPathSegList.cpp \
        ksvg2/svg/SVGStyleElement.cpp \
        ksvg2/svg/SVGPaint.cpp \
        ksvg2/svg/SVGFEDistantLightElement.cpp \
        ksvg2/svg/SVGTextPositioningElement.cpp \
        ksvg2/svg/SVGPreserveAspectRatio.cpp \
        ksvg2/svg/SVGScriptElement.cpp \
        ksvg2/svg/SVGComponentTransferFunctionElement.cpp \
        ksvg2/svg/SVGTextElement.cpp \
        ksvg2/svg/SVGViewElement.cpp \
        ksvg2/svg/SVGLengthList.cpp \
        ksvg2/svg/SVGStyledTransformableElement.cpp \
        ksvg2/svg/SVGPathSegArc.cpp \
        ksvg2/svg/SVGDescElement.cpp \
        ksvg2/svg/SVGTransformable.cpp \
        ksvg2/svg/SVGDocument.cpp \
        ksvg2/svg/SVGClipPathElement.cpp \
        ksvg2/svg/SVGPathSegMoveto.cpp \
        ksvg2/svg/SVGAElement.cpp \
        ksvg2/svg/SVGCircleElement.cpp \
        ksvg2/svg/SVGFEFuncAElement.cpp \
        ksvg2/svg/SVGTests.cpp \
        ksvg2/svg/SVGPathSegCurvetoQuadraticSmooth.cpp \
        ksvg2/svg/SVGElement.cpp \
        ksvg2/svg/SVGAnimateTransformElement.cpp \
        ksvg2/svg/SVGFEColorMatrixElement.cpp \
        ksvg2/svg/SVGGradientElement.cpp \
        ksvg2/svg/SVGAnimationElement.cpp \
        ksvg2/svg/SVGFELightElement.cpp \
        ksvg2/svg/SVGPathSegClosePath.cpp \
        ksvg2/svg/SVGPathSegLinetoHorizontal.cpp \
        ksvg2/svg/SVGStopElement.cpp \
        ksvg2/svg/SVGDefsElement.cpp \
        ksvg2/svg/SVGMetadataElement.cpp \
        ksvg2/svg/SVGParserUtilities.cpp \
        ksvg2/misc/SVGImageLoader.cpp \
        ksvg2/misc/SVGDocumentExtensions.cpp \
        ksvg2/misc/SVGTimer.cpp \
        ksvg2/misc/TimeScheduler.cpp \
        ksvg2/misc/KCanvasRenderingStyle.cpp \
        ksvg2/misc/PointerEventsHitRules.cpp \
        ksvg2/events/JSSVGLazyEventListener.cpp \
        ksvg2/events/SVGZoomEvent.cpp \
        platform/graphics/svg/SVGPaintServer.cpp \
        platform/graphics/svg/SVGPaintServerGradient.cpp \
        platform/graphics/svg/SVGPaintServerLinearGradient.cpp \
        platform/graphics/svg/SVGPaintServerPattern.cpp \
        platform/graphics/svg/SVGPaintServerRadialGradient.cpp \
        platform/graphics/svg/SVGPaintServerSolid.cpp \
        platform/graphics/svg/SVGResource.cpp \
        platform/graphics/svg/SVGResourceClipper.cpp \
        platform/graphics/svg/SVGResourceFilter.cpp \
        platform/graphics/svg/SVGResourceMarker.cpp \
        platform/graphics/svg/SVGResourceMasker.cpp \
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
        platform/graphics/svg/qt/RenderPathQt.cpp \
        platform/graphics/svg/qt/SVGPaintServerGradientQt.cpp \
        platform/graphics/svg/qt/SVGPaintServerLinearGradientQt.cpp \
        platform/graphics/svg/qt/SVGPaintServerPatternQt.cpp \
        platform/graphics/svg/qt/SVGPaintServerQt.cpp \
        platform/graphics/svg/qt/SVGPaintServerRadialGradientQt.cpp \
        platform/graphics/svg/qt/SVGPaintServerSolidQt.cpp \
        platform/graphics/svg/qt/SVGResourceClipperQt.cpp \
        platform/graphics/svg/qt/SVGResourceMaskerQt.cpp \
        platform/graphics/svg/qt/SVGResourceFilterQt.cpp \
        rendering/RenderForeignObject.cpp \
        rendering/RenderPath.cpp \
        rendering/RenderSVGBlock.cpp \
        rendering/RenderSVGContainer.cpp \
        rendering/RenderSVGImage.cpp \
        rendering/RenderSVGInline.cpp \
        rendering/RenderSVGInlineText.cpp \
        rendering/RenderSVGText.cpp \
        rendering/RenderSVGTSpan.cpp \
        rendering/SVGInlineFlowBox.cpp \
        rendering/SVGRootInlineBox.cpp \
        history/BackForwardList.cpp \
        history/HistoryItem.cpp \
        history/HistoryItemTimer.cpp \
        history/PageCache.cpp \
        history/qt/PageCacheQt.cpp \

        # GENERATOR 5-B:
        svgnames_a.output = tmp/SVGNames.cpp
        svgnames_a.commands = perl $$PWD/ksvg2/scripts/make_names.pl --tags $$PWD/ksvg2/svg/svgtags.in --attrs $$PWD/ksvg2/svg/svgattrs.in --namespace SVG --cppNamespace WebCore --namespaceURI 'http://www.w3.org/2000/svg' --factory --attrsNullNamespace --output tmp
        svgnames_a.input = SVG_NAMES
        svgnames_a.dependency_type = TYPE_C
        svgnames_a.CONFIG = target_predeps
        svgnames_a.variable_out = GENERATED_SOURCES
        QMAKE_EXTRA_COMPILERS += svgnames_a
        svgnames_b.output = tmp/SVGElementFactory.cpp
        svgnames_b.commands = @echo -n ''
        svgnames_b.input = SVG_NAMES
        svgnames_b.depends = tmp/SVGNames.cpp
        svgnames_b.CONFIG = target_predeps
        svgnames_b.variable_out = GENERATED_SOURCES
        QMAKE_EXTRA_COMPILERS += svgnames_b

        # GENERATOR 5-C:
        xlinknames.output = tmp/XLinkNames.cpp
        xlinknames.commands = perl $$PWD/ksvg2/scripts/make_names.pl --attrs $$PWD/ksvg2/misc/xlinkattrs.in --namespace XLink --cppNamespace WebCore --namespaceURI 'http://www.w3.org/1999/xlink' --output tmp
        xlinknames.input = XLINK_NAMES
        xlinknames.dependency_type = TYPE_C
        xlinknames.CONFIG = target_predeps
        xlinknames.variable_out = GENERATED_SOURCES
        QMAKE_EXTRA_COMPILERS += xlinknames

        # GENERATOR 5-D:
        xmlnames.output = tmp/XMLNames.cpp
        xmlnames.commands = perl $$PWD/ksvg2/scripts/make_names.pl --attrs $$PWD/xml/xmlattrs.in --namespace XML --cppNamespace WebCore --namespaceURI 'http://www.w3.org/XML/1998/namespace' --output tmp
        xmlnames.input = XML_NAMES
        xmlnames.dependency_type = TYPE_C
        xmlnames.CONFIG = target_predeps
        xmlnames.variable_out = GENERATED_SOURCES
        QMAKE_EXTRA_COMPILERS += xmlnames
}


# GENERATOR 1: IDL compiler
idl.output = tmp/JS${QMAKE_FILE_BASE}.cpp
idl.variable_out = GENERATED_SOURCES
idl.input = IDL_BINDINGS
idl.commands = perl -I$$PWD/bindings/scripts $$PWD/bindings/scripts/generate-bindings.pl --defines \"$${FEATURE_DEFINES_JAVASCRIPT}\" --generator JS --include $$PWD/dom --include $$PWD/html --include $$PWD/xml --include $$PWD/ksvg2/svg --outputdir tmp ${QMAKE_FILE_NAME}
idl.CONFIG += target_predeps
QMAKE_EXTRA_COMPILERS += idl

# GENERATOR 2-A: LUT creator
lut.output = tmp/${QMAKE_FILE_BASE}.lut.h
lut.commands = perl $$PWD/../JavaScriptCore/kjs/create_hash_table ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
lut.depend = ${QMAKE_FILE_NAME}
lut.input = LUT_FILES
lut.CONFIG += no_link
QMAKE_EXTRA_COMPILERS += lut

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
cssbison.output = tmp/${QMAKE_FILE_BASE}.cpp
cssbison.commands = bison -d -p cssyy ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_BASE}.tab.c && mv ${QMAKE_FILE_BASE}.tab.c tmp/${QMAKE_FILE_BASE}.cpp && mv ${QMAKE_FILE_BASE}.tab.h tmp/${QMAKE_FILE_BASE}.h
cssbison.depend = ${QMAKE_FILE_NAME}
cssbison.input = CSSBISON
cssbison.CONFIG = target_predeps
cssbison.dependency_type = TYPE_C
cssbison.variable_out = GENERATED_SOURCES
QMAKE_EXTRA_COMPILERS += cssbison
#PRE_TARGETDEPS += tmp/CSSGrammar.cpp

# GENERATOR 5-A:
htmlnames.output = tmp/HTMLNames.cpp
htmlnames.commands = perl $$PWD/ksvg2/scripts/make_names.pl --tags $$PWD/html/HTMLTagNames.in --attrs $$PWD/html/HTMLAttributeNames.in --namespace HTML --namespacePrefix xhtml --cppNamespace WebCore --namespaceURI 'http://www.w3.org/1999/xhtml' --attrsNullNamespace --output tmp
htmlnames.input = HTML_NAMES
htmlnames.dependency_type = TYPE_C
htmlnames.CONFIG = target_predeps
htmlnames.variable_out = GENERATED_SOURCES
QMAKE_EXTRA_COMPILERS += htmlnames

# GENERATOR 6-A:
cssprops.output = tmp/CSSPropertyNames.c
cssprops.input = WALDOCSSPROPS
cssprops.commands = cp ${QMAKE_FILE_NAME} tmp && cd tmp && sh $$PWD/css/makeprop
cssprops.CONFIG = target_predeps no_link
QMAKE_EXTRA_COMPILERS += cssprops

# GENERATOR 6-B:
cssvalues.output = tmp/CSSValueKeywords.c
cssvalues.input = WALDOCSSVALUES
cssvalues.commands = cp ${QMAKE_FILE_NAME} tmp && cd tmp && sh $$PWD/css/makevalues
cssvalues.CONFIG = target_predeps no_link
QMAKE_EXTRA_COMPILERS += cssvalues

# GENERATOR 7-A:
svgcssproperties.output = tmp/ksvgcssproperties.h
svgcssproperties.input = SVGCSSPROPERTIES
svgcssproperties.commands = cp $$PWD/ksvg2/css/CSSPropertyNames.in tmp/ksvgcssproperties.in && cd tmp && perl $$PWD/ksvg2/scripts/cssmakeprops -n SVG -f ksvgcssproperties.in
svgcssproperties.CONFIG = target_predeps no_link
QMAKE_EXTRA_COMPILERS += svgcssproperties

# GENERATOR 7-B:
svgcssvalues.output = tmp/ksvgcssvalues.h
svgcssvalues.input = SVGCSSVALUES
svgcssvalues.commands = perl -ne \'print lc\' $$PWD/ksvg2/css/CSSValueKeywords.in > tmp/ksvgcssvalues.in && cd tmp && perl $$PWD/ksvg2/scripts/cssmakevalues -n SVG -f ksvgcssvalues.in
svgcssvalues.CONFIG = target_predeps no_link
QMAKE_EXTRA_COMPILERS += svgcssvalues

# GENERATOR 8-A:
entities.output = tmp/HTMLEntityNames.c
entities.commands = gperf -a -L ANSI-C -C -G -c -o -t -k '\*' -N findEntity -D -s 2 < $$PWD/html/HTMLEntityNames.gperf > tmp/HTMLEntityNames.c
entities.input = ENTITIES_GPERF
entities.dependency_type = TYPE_C
entities.CONFIG = target_predeps no_link
QMAKE_EXTRA_COMPILERS += entities

# PRE-GENERATOR 8-B:
doctypestrings.target = $$OUTPUT_DIR/WebCore/tmp/DocTypeStrings.cpp
doctypestrings.commands = echo \"$${LITERAL_HASH}include <string.h>\" > $$doctypestrings.target && gperf -CEot -L ANSI-C -k \'*\' -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards < $$PWD/html/DocTypeStrings.gperf >> $$doctypestrings.target
QMAKE_EXTRA_TARGETS += doctypestrings
PRE_TARGETDEPS += $$OUTPUT_DIR/WebCore/tmp/DocTypeStrings.cpp

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
QMAKE_EXTRA_COMPILERS += stylesheets

# GENERATOR M
manual_moc.output = tmp/${QMAKE_FILE_BASE}.moc
manual_moc.commands = $$QMAKE_MOC ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_OUT}
manual_moc.input = MANUALMOC
manual_moc.CONFIG += target_predeps no_link
QMAKE_EXTRA_COMPILERS += manual_moc

# GENERATOR 10: XPATH grammar
xpathbison.output = tmp/${QMAKE_FILE_BASE}.cpp
xpathbison.commands = bison -d -p xpathyy ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_BASE}.tab.c && mv ${QMAKE_FILE_BASE}.tab.c tmp/${QMAKE_FILE_BASE}.cpp && mv ${QMAKE_FILE_BASE}.tab.h tmp/${QMAKE_FILE_BASE}.h
xpathbison.depend = ${QMAKE_FILE_NAME}
xpathbison.input = XPATHBISON
xpathbison.CONFIG = target_predeps
xpathbison.dependency_type = TYPE_C
xpathbison.variable_out = GENERATED_SOURCES
QMAKE_EXTRA_COMPILERS += xpathbison


