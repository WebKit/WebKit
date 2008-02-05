# -*- Mode:makefile -*-
# WebCore - qmake build info
CONFIG += building-libs
# do not use implicit rules in nmake Makefiles to avoid the clash
# of API/Node.c and dom/Node.cpp
CONFIG += no_batch
include($$PWD/../WebKit.pri)
gtk-port:LIBS -= -lWebKitGtk

TEMPLATE = lib
qt-port:TARGET = QtWebKit
gtk-port:TARGET = WebKitGtk

CONFIG(QTDIR_build) {
    GENERATED_SOURCES_DIR = $$PWD/generated
    include($$QT_SOURCE_TREE/src/qbase.pri)
    PRECOMPILED_HEADER = $$PWD/../WebKit/qt/WebKit_pch.h
}

isEmpty(GENERATED_SOURCES_DIR):GENERATED_SOURCES_DIR = tmp
GENERATED_SOURCES_DIR_SLASH = $$GENERATED_SOURCES_DIR/
win32-*: GENERATED_SOURCES_DIR_SLASH ~= s|/|\|

INCLUDEPATH += $$GENERATED_SOURCES_DIR

!CONFIG(QTDIR_build) {
     OBJECTS_DIR = tmp
     DESTDIR = $$OUTPUT_DIR/lib
}

DEPENDPATH += css dom loader editing history html \
    loader page platform platform/graphics platform/network platform/text plugins rendering xml \
    bindings/js

include($$OUTPUT_DIR/config.pri)

CONFIG -= warn_on
*-g++*:QMAKE_CXXFLAGS += -Wreturn-type -fno-strict-aliasing
#QMAKE_CXXFLAGS += -Wall -Wno-undef -Wno-unused-parameter

CONFIG(release):!CONFIG(QTDIR_build) {
    contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols
    unix:contains(QT_CONFIG, reduce_relocations):CONFIG += bsymbolic_functions
}

linux-*: DEFINES += HAVE_STDINT_H
freebsd-*: DEFINES += HAVE_PTHREAD_NP_H

# PRE-BUILD: make the required config.h file
#config_h.target = config.h
#config_h.commands = cp config.h.qmake config.h
#config_h.depends = config.h.qmake
#QMAKE_EXTRA_TARGETS += config_h
#PRE_TARGETDEPS += config.h

DEFINES += BUILD_WEBKIT

win32-*: DEFINES += ENABLE_ICONDATABASE=0 ENABLE_DATABASE=0

# Pick up 3rdparty libraries from INCLUDE/LIB just like with MSVC
win32-g++ {
    TMPPATH            = $$quote($$(INCLUDE))
    QMAKE_INCDIR_POST += $$split(TMPPATH,";")
    TMPPATH            = $$quote($$(LIB))
    QMAKE_LIBDIR_POST += $$split(TMPPATH,";")
}

gtk-port: PKGCONFIG += gthread-2.0

# Optional components (look for defs in config.h and included files!)
!contains(DEFINES, ENABLE_DATABASE=.): DEFINES += ENABLE_DATABASE=1
!contains(DEFINES, ENABLE_ICONDATABASE=.): DEFINES += ENABLE_ICONDATABASE=1
!contains(DEFINES, ENABLE_XPATH=.): DEFINES += ENABLE_XPATH=1
gtk-port:!contains(DEFINES, ENABLE_XSLT=.): DEFINES += ENABLE_XSLT=1
#!contains(DEFINES, ENABLE_XBL=.): DEFINES += ENABLE_XBL=1
qt-port: !contains(DEFINES, ENABLE_SVG=.): DEFINES += ENABLE_SVG=1
gtk-port:DEFINES += ENABLE_SVG=0
DEFINES += ENABLE_VIDEO=0

DEFINES += WTF_CHANGES=1

#
# For builds inside Qt we interpret the output rule and the input of each extra compiler manually
# and add the resulting sources to the SOURCES variable, because the build inside Qt contains already
# all the generated files. We do not need to generate any extra compiler rules in that case.
#
# In addition this function adds a new target called 'generated_files' that allows manually calling
# all the extra compilers to generate all the necessary files for the build using 'make generated_files'
#
defineTest(addExtraCompiler) {
    CONFIG(QTDIR_build) {
        outputRule = $$eval($${1}.output)

        input = $$eval($${1}.input)
        input = $$eval($$input)

        for(file,input) {
            base = $$basename(file)
            base ~= s/\..+//
            newfile=$$replace(outputRule,\\$\\{QMAKE_FILE_BASE\\},$$base)
            SOURCES += $$newfile
        }

        export(SOURCES)
    } else {
        QMAKE_EXTRA_COMPILERS += $$1
        generated_files.depends += compiler_$${1}_make_all
        export(QMAKE_EXTRA_COMPILERS)
        export(generated_files.depends)
    }
    return(true)
}

include($$PWD/../JavaScriptCore/JavaScriptCore.pri)

#INCLUDEPATH += $$PWD/../JavaScriptCore
#LIBS += -L$$OUTPUT_DIR/lib -lJavaScriptCore

qt-port {
RESOURCES += $$PWD/../WebCore/page/inspector/WebKit.qrc
INCLUDEPATH += \
                $$PWD/platform/qt \
                $$PWD/platform/network/qt \
                $$PWD/platform/graphics/qt \
                $$PWD/svg/graphics/qt \
                $$PWD/loader/qt \
                $$PWD/page/qt \
                $$PWD/../WebKit/qt/WebCoreSupport \
                $$PWD/../WebKit/qt/Api

DEPENDPATH += editing/qt history/qt loader/qt page/qt \
    platform/graphics/qt ../WebKit/qt/Api ../WebKit/qt/WebCoreSupport

    DEFINES += WTF_USE_JAVASCRIPTCORE_BINDINGS=1
}

gtk-port {
    x11:plugins {
        DEFINES += XP_UNIX
    }

    INCLUDEPATH += \
    $$PWD/platform/gtk \
    $$PWD/platform/graphics/gtk \
    $$PWD/platform/graphics/cairo \
    $$PWD/svg/graphics/cairo \
    $$PWD/platform/network/curl \
    $$PWD/platform/image-decoders \
    $$PWD/platform/image-decoders/bmp \
    $$PWD/platform/image-decoders/gif \
    $$PWD/platform/image-decoders/ico \
    $$PWD/platform/image-decoders/jpeg \
    $$PWD/platform/image-decoders/png \
    $$PWD/platform/image-decoders/xbm \
    $$PWD/loader/gtk \
    $$PWD/page/gtk \
    $$PWD/../WebKit/gtk \
    $$PWD/../WebKit/gtk/WebCoreSupport \
    $$PWD/../WebKit/gtk/webkit

    DEPENDPATH += \
    platform/gtk \
    platform/graphics/gtk \
    platform/graphics/cairo \
    svg/graphics/cairo \
    platform/network/curl \
    platform/image-decoders \
    platform/image-decoders/bmp \
    platform/image-decoders/gif \
    platform/image-decoders/ico \
    platform/image-decoders/jpeg \
    platform/image-decoders/png \
    platform/image-decoders/xbm \
    loader/gtk \
    page/gtk \
    ../WebKit/gtk \
    ../WebKit/gtk/WebCoreSupport \
    ../WebKit/gtk/webkit
}

INCLUDEPATH +=  $$PWD \
                $$PWD/ForwardingHeaders \
                $$PWD/.. \
                $$PWD/../JavaScriptCore/kjs \
                $$PWD/../JavaScriptCore/bindings \
                $$PWD/../JavaScriptCore/wtf \
                $$PWD/platform \
                $$PWD/platform/network \
                $$PWD/platform/graphics \
                $$PWD/svg/graphics \
                $$PWD/svg/graphics/filters \
                $$PWD/platform/sql \
                $$PWD/platform/text \
                $$PWD/storage \
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
                $$PWD/svg \
                $$PWD/platform/image-decoders \
                $$PWD/plugins

QT += network
lessThan(QT_MINOR_VERSION, 4): QT += xml

QMAKE_EXTRA_TARGETS += generated_files

FEATURE_DEFINES_JAVASCRIPT = LANGUAGE_JAVASCRIPT=1

TOKENIZER = $$PWD/css/tokenizer.flex

DOCTYPESTRINGS = $$PWD/html/DocTypeStrings.gperf

CSSBISON = $$PWD/css/CSSGrammar.y

HTML_NAMES = $$PWD/html/HTMLTagNames.in

XML_NAMES = $$PWD/xml/xmlattrs.in

ENTITIES_GPERF = $$PWD/html/HTMLEntityNames.gperf

COLORDAT_GPERF = $$PWD/platform/ColorData.gperf

WALDOCSSPROPS = $$PWD/css/CSSPropertyNames.in

WALDOCSSVALUES = $$PWD/css/CSSValueKeywords.in

SVGCSSPROPERTIES = $$PWD/css/SVGCSSPropertyNames.in

SVGCSSVALUES = $$PWD/css/SVGCSSValueKeywords.in

STYLESHEETS_EMBED = $$PWD/css/html4.css

LUT_FILES += \
    bindings/js/JSEventTargetBase.cpp \
    bindings/js/JSLocation.cpp \
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
    css/StyleSheetList.idl \
    dom/Attr.idl \
    dom/CharacterData.idl \
    dom/CDATASection.idl \
    dom/Comment.idl \
    dom/DocumentFragment.idl \
    dom/Document.idl \
    dom/DocumentType.idl \
    dom/DOMCoreException.idl \
    dom/DOMImplementation.idl \
    dom/Element.idl \
    dom/Entity.idl \
    dom/EntityReference.idl \
    dom/Event.idl \
    dom/EventException.idl \
#    dom/EventListener.idl \
#    dom/EventTarget.idl \
    dom/KeyboardEvent.idl \
    dom/MessageEvent.idl \
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
    dom/ProgressEvent.idl \
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
    page/BarInfo.idl \
    page/Console.idl \
    page/DOMSelection.idl \
    page/DOMWindow.idl \
    page/History.idl \
    page/Screen.idl \
    xml/DOMParser.idl \
    xml/XMLHttpRequestException.idl \
    xml/XMLSerializer.idl


SOURCES += \
    bindings/js/GCController.cpp \
    bindings/js/JSAttrCustom.cpp \
    bindings/js/JSCanvasRenderingContext2DCustom.cpp \
    bindings/js/JSCSSRuleCustom.cpp \
    bindings/js/JSCSSStyleDeclarationCustom.cpp \
    bindings/js/JSCSSValueCustom.cpp \
    bindings/js/JSCustomVoidCallback.cpp \
    bindings/js/JSCustomXPathNSResolver.cpp \
    bindings/js/JSDocumentCustom.cpp \
    bindings/js/JSDOMWindowCustom.cpp \
    bindings/js/JSElementCustom.cpp \
    bindings/js/JSEventCustom.cpp \
    bindings/js/JSEventTargetBase.cpp \
    bindings/js/JSEventTargetNode.cpp \
    bindings/js/JSHistoryCustom.cpp \
    bindings/js/JSHTMLAppletElementCustom.cpp \
    bindings/js/JSHTMLCollectionCustom.cpp \
    bindings/js/JSHTMLDocumentCustom.cpp \
    bindings/js/JSHTMLElementCustom.cpp \
    bindings/js/JSHTMLElementWrapperFactory.cpp \
    bindings/js/JSHTMLEmbedElementCustom.cpp \
    bindings/js/JSHTMLFormElementCustom.cpp \
    bindings/js/JSHTMLFrameElementCustom.cpp \
    bindings/js/JSHTMLFrameSetElementCustom.cpp \
    bindings/js/JSHTMLIFrameElementCustom.cpp \
    bindings/js/JSHTMLInputElementBase.cpp \
    bindings/js/JSHTMLObjectElementCustom.cpp \
    bindings/js/JSHTMLOptionElementConstructor.cpp \
    bindings/js/JSHTMLOptionsCollectionCustom.cpp \
    bindings/js/JSHTMLSelectElementCustom.cpp \
    bindings/js/JSLocation.cpp \
    bindings/js/JSNamedNodeMapCustom.cpp \
    bindings/js/JSNamedNodesCollection.cpp  \
    bindings/js/JSNodeCustom.cpp \
    bindings/js/JSNodeFilterCondition.cpp \
    bindings/js/JSNodeFilterCustom.cpp \
    bindings/js/JSNodeIteratorCustom.cpp \
    bindings/js/JSNodeListCustom.cpp \
    bindings/js/JSStyleSheetCustom.cpp \
    bindings/js/JSStyleSheetListCustom.cpp \
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
    bindings/js/PausedTimeouts.cpp \
    bindings/js/ScheduledAction.cpp \
    css/CSSBorderImageValue.cpp \
    css/CSSCharsetRule.cpp \
    css/CSSComputedStyleDeclaration.cpp \
    css/CSSCursorImageValue.cpp \
    css/CSSFontFace.cpp \
    css/CSSFontFaceRule.cpp \
    css/CSSFontFaceSrcValue.cpp \
    css/CSSFontSelector.cpp \
    css/CSSFontFaceSource.cpp \
    css/CSSHelper.cpp \
    css/CSSImageValue.cpp \
    css/CSSImportRule.cpp \
    css/CSSInheritedValue.cpp \
    css/CSSInitialValue.cpp \
    css/CSSMediaRule.cpp \
    css/CSSMutableStyleDeclaration.cpp \
    css/CSSPageRule.cpp \
    css/CSSParser.cpp \
    css/CSSPrimitiveValue.cpp \
    css/CSSProperty.cpp \
    css/CSSRule.cpp \
    css/CSSRuleList.cpp \
    css/CSSSelector.cpp \
    css/CSSSegmentedFontFace.cpp \
    css/CSSStyleDeclaration.cpp \
    css/CSSStyleRule.cpp \
    css/CSSStyleSelector.cpp \
    css/CSSStyleSheet.cpp \
    css/CSSTimingFunctionValue.cpp \
    css/CSSTransformValue.cpp \
    css/CSSUnicodeRangeValue.cpp \
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
    dom/ClassNames.cpp \
    dom/ClassNodeList.cpp \
    dom/Clipboard.cpp \
    dom/ClipboardEvent.cpp \
    dom/Comment.cpp \
    dom/ContainerNode.cpp \
    dom/CSSMappedAttributeDeclaration.cpp \
    dom/Document.cpp \
    dom/DocumentFragment.cpp \
    dom/DocumentType.cpp \
    dom/DOMImplementation.cpp \
    dom/DynamicNodeList.cpp \
    dom/EditingText.cpp \
    dom/Element.cpp \
    dom/Entity.cpp \
    dom/EntityReference.cpp \
    dom/Event.cpp \
    dom/EventNames.cpp \
    dom/EventTarget.cpp \
    dom/EventTargetNode.cpp \
    dom/ExceptionBase.cpp \
    dom/ExceptionCode.cpp \
    dom/KeyboardEvent.cpp \
    dom/MappedAttribute.cpp \
    dom/MessageEvent.cpp \
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
    dom/Notation.cpp \
    dom/OverflowEvent.cpp \
    dom/Position.cpp \
    dom/PositionIterator.cpp \
    dom/ProcessingInstruction.cpp \
    dom/ProgressEvent.cpp \
    dom/QualifiedName.cpp \
    dom/Range.cpp \
    dom/RegisteredEventListener.cpp \
    dom/SelectorNodeList.cpp \
    dom/StaticNodeList.cpp \
    dom/StyledElement.cpp \
    dom/StyleElement.cpp \
    dom/TagNodeList.cpp \
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
    editing/CompositeEditCommand.cpp \
    editing/CreateLinkCommand.cpp \
    editing/DeleteButtonController.cpp \
    editing/DeleteButton.cpp \
    editing/DeleteFromTextNodeCommand.cpp \
    editing/DeleteSelectionCommand.cpp \
    editing/EditCommand.cpp \
    editing/Editor.cpp \
    editing/EditorCommand.cpp \
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
    editing/markup.cpp \
    editing/MergeIdenticalElementsCommand.cpp \
    editing/ModifySelectionListLevel.cpp \
    editing/MoveSelectionCommand.cpp \
    editing/RemoveCSSPropertyCommand.cpp \
    editing/RemoveFormatCommand.cpp \
    editing/RemoveNodeAttributeCommand.cpp \
    editing/RemoveNodeCommand.cpp \
    editing/RemoveNodePreservingChildrenCommand.cpp \
    editing/ReplaceSelectionCommand.cpp \
    editing/SelectionController.cpp \
    editing/Selection.cpp \
    editing/SetNodeAttributeCommand.cpp \
    editing/SmartReplace.cpp \
    editing/SmartReplaceICU.cpp \
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
    html/HTMLTableRowsCollection.cpp \
    html/HTMLTableSectionElement.cpp \
    html/HTMLTextAreaElement.cpp \
    html/HTMLTextFieldInnerElement.cpp \
    html/HTMLTitleElement.cpp \
    html/HTMLTokenizer.cpp \
    html/HTMLUListElement.cpp \
    html/HTMLViewSourceDocument.cpp \
    loader/Cache.cpp \
    loader/CachedCSSStyleSheet.cpp \
    loader/CachedFont.cpp \
    loader/CachedImage.cpp \
    loader/CachedResourceClientWalker.cpp \
    loader/CachedResource.cpp \
    loader/CachedScript.cpp \
    loader/CachedXSLStyleSheet.cpp \
    loader/DocLoader.cpp \
    loader/DocumentLoader.cpp \
    loader/FormState.cpp \
    loader/FrameLoader.cpp \
    loader/FTPDirectoryDocument.cpp \
    loader/FTPDirectoryParser.cpp \
    loader/icon/IconLoader.cpp \
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
    page/AnimationController.cpp \
    page/BarInfo.cpp \
    page/Chrome.cpp \
    page/Console.cpp \
    page/ContextMenuController.cpp \
    page/DOMSelection.cpp \
    page/DOMWindow.cpp \
    page/DragController.cpp \
    page/EventHandler.cpp \
    page/FocusController.cpp \
    page/Frame.cpp \
    page/FrameTree.cpp \
    page/FrameView.cpp \
    page/History.cpp \
    page/InspectorController.cpp \
    page/MouseEventWithHitTestResults.cpp \
    page/Page.cpp \
    page/Screen.cpp \
    page/Settings.cpp \
    page/WindowFeatures.cpp \
    platform/Arena.cpp \
    platform/ArrayImpl.cpp \
    platform/text/AtomicString.cpp \
    platform/text/Base64.cpp \
    platform/text/BidiContext.cpp \
    platform/ContextMenu.cpp \
    platform/text/CString.cpp \
    platform/DeprecatedCString.cpp \
    platform/DeprecatedPtrListImpl.cpp \
    platform/DeprecatedString.cpp \
    platform/DeprecatedStringList.cpp \
    platform/DeprecatedValueListImpl.cpp \
    platform/DragData.cpp \
    platform/DragImage.cpp \
    platform/FileChooser.cpp \
    platform/graphics/FontFamily.cpp \
    platform/graphics/AffineTransform.cpp \
    platform/graphics/BitmapImage.cpp \
    platform/graphics/Color.cpp \
    platform/graphics/FloatPoint3D.cpp \
    platform/graphics/FloatPoint.cpp \
    platform/graphics/FloatRect.cpp \
    platform/graphics/FloatSize.cpp \
    platform/graphics/FontData.cpp \
    platform/graphics/GraphicsContext.cpp \
    platform/graphics/GraphicsTypes.cpp \
    platform/graphics/Image.cpp \
    platform/graphics/IntRect.cpp \
    platform/graphics/Path.cpp \
    platform/graphics/PathTraversalState.cpp \
    platform/graphics/Pen.cpp \
    platform/graphics/SegmentedFontData.cpp \
    platform/KURL.cpp \
    platform/Logging.cpp \
    platform/MIMETypeRegistry.cpp \
    platform/network/AuthenticationChallenge.cpp \
    platform/network/Credential.cpp \
    platform/network/FormData.cpp \
    platform/network/HTTPParsers.cpp \
    platform/network/ProtectionSpace.cpp \
    platform/network/ResourceHandle.cpp \
    platform/network/ResourceRequestBase.cpp \
    platform/network/ResourceResponseBase.cpp \
    platform/text/RegularExpression.cpp \
    platform/ScrollBar.cpp \
#    platform/SearchPopupMenu.cpp \
    platform/SecurityOrigin.cpp \
    platform/text/SegmentedString.cpp \
    platform/SharedBuffer.cpp \
    platform/text/String.cpp \
    platform/text/StringImpl.cpp \
    platform/text/TextCodec.cpp \
    platform/text/TextCodecLatin1.cpp \
    platform/text/TextCodecUserDefined.cpp \
    platform/text/TextCodecUTF16.cpp \
    platform/text/TextDecoder.cpp \
    platform/text/TextEncoding.cpp \
    platform/text/TextEncodingRegistry.cpp \
    platform/text/TextStream.cpp \
    platform/Timer.cpp \
    platform/text/UnicodeRange.cpp \
    platform/Widget.cpp \
    plugins/PluginStream.cpp \
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
    xml/XMLHttpRequest.cpp \
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
    xml/XSLTExtensions.cpp \
    xml/XSLTUnicodeSort.cpp \
    xml/XSLTProcessor.cpp

gtk-port {
  SOURCES += \
    platform/graphics/GlyphPageTreeNode.cpp \
    platform/graphics/GlyphWidthMap.cpp \
    platform/graphics/FontCache.cpp \
    platform/graphics/Font.cpp \
    platform/graphics/FontFallbackList.cpp \
    platform/graphics/SimpleFontData.cpp 
}

qt-port {

    HEADERS += \
    $$PWD/platform/qt/QWebPopup.h \
    $$PWD/platform/qt/MenuEventProxy.h \
    $$PWD/platform/qt/SharedTimerQt.h \
    $$PWD/../WebKit/qt/Api/qwebframe.h \
    $$PWD/../WebKit/qt/Api/qwebpage.h \
    $$PWD/../WebKit/qt/Api/qwebview.h \
    $$PWD/../WebKit/qt/Api/qwebhistoryinterface.h \
    $$PWD/../WebKit/qt/WebCoreSupport/FrameLoaderClientQt.h \
    $$PWD/platform/network/qt/QNetworkReplyHandler.h

    SOURCES += \
    page/qt/DragControllerQt.cpp \
    page/qt/EventHandlerQt.cpp \
    page/qt/FrameQt.cpp \
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
    platform/network/qt/ResourceRequestQt.cpp \
    platform/network/qt/QNetworkReplyHandler.cpp \
    editing/qt/EditorQt.cpp \
    platform/qt/ClipboardQt.cpp \
    platform/qt/ContextMenuItemQt.cpp \
    platform/qt/ContextMenuQt.cpp \
    platform/qt/CookieJarQt.cpp \
    platform/qt/CursorQt.cpp \
    platform/qt/DragDataQt.cpp \
    platform/qt/DragImageQt.cpp \
    platform/qt/FileChooserQt.cpp \
    platform/qt/FileSystemQt.cpp \
    platform/graphics/qt/FontCacheQt.cpp \
    platform/graphics/qt/FontCustomPlatformData.cpp \
    platform/graphics/qt/FontQt.cpp \
    platform/graphics/qt/GlyphPageTreeNodeQt.cpp \
    platform/graphics/qt/SimpleFontDataQt.cpp \
    platform/qt/KURLQt.cpp \
    platform/qt/Localizations.cpp \
    platform/qt/MIMETypeRegistryQt.cpp \
    platform/qt/PasteboardQt.cpp \
    platform/qt/PlatformKeyboardEventQt.cpp \
    platform/qt/PlatformMouseEventQt.cpp \
    platform/qt/PlatformScreenQt.cpp \
    platform/qt/PlatformScrollBarQt.cpp \
    platform/qt/PlugInInfoStoreQt.cpp \
    platform/qt/PopupMenuQt.cpp \
    platform/qt/QWebPopup.cpp \
    platform/qt/RenderThemeQt.cpp \
    platform/qt/ScrollViewQt.cpp \
    platform/qt/SearchPopupMenuQt.cpp \
    platform/qt/SharedTimerQt.cpp \
    platform/qt/SoundQt.cpp \
    platform/text/qt/StringQt.cpp \
    platform/qt/TemporaryLinkStubs.cpp \
    platform/text/qt/TextBoundaries.cpp \
    platform/text/qt/TextBreakIteratorQt.cpp \
    platform/text/qt/TextCodecQt.cpp \
    platform/qt/ThreadingQt.cpp \
    platform/qt/WheelEventQt.cpp \
    platform/qt/WidgetQt.cpp \
    ../WebKit/qt/WebCoreSupport/ChromeClientQt.cpp \
    ../WebKit/qt/WebCoreSupport/ContextMenuClientQt.cpp \
    ../WebKit/qt/WebCoreSupport/DragClientQt.cpp \
    ../WebKit/qt/WebCoreSupport/EditorClientQt.cpp \
    ../WebKit/qt/WebCoreSupport/EditCommandQt.cpp \
    ../WebKit/qt/WebCoreSupport/FrameLoaderClientQt.cpp \
    ../WebKit/qt/WebCoreSupport/InspectorClientQt.cpp \
    ../WebKit/qt/Api/qwebframe.cpp \
    ../WebKit/qt/Api/qwebpage.cpp \
    ../WebKit/qt/Api/qwebview.cpp \
    ../WebKit/qt/Api/qwebhistory.cpp \
    ../WebKit/qt/Api/qwebsettings.cpp \
    ../WebKit/qt/Api/qwebhistoryinterface.cpp \
    platform/ThreadingNone.cpp

    unix: SOURCES += platform/qt/SystemTimeQt.cpp
    else: SOURCES += platform/win/SystemTimeWin.cpp

    # Files belonging to the Qt 4.3 build
    lessThan(QT_MINOR_VERSION, 4) {
        HEADERS += \
            $$PWD/../WebKit/qt/Api/qwebnetworkinterface.h \
            $$PWD/../WebKit/qt/Api/qwebnetworkinterface_p.h \
            $$PWD/../WebKit/qt/Api/qwebobjectplugin.h \
            $$PWD/../WebKit/qt/Api/qwebobjectplugin_p.h \
            $$PWD/../WebKit/qt/Api/qwebobjectpluginconnector.h \
            $$PWD/../WebKit/qt/Api/qcookiejar.h

        SOURCES += \
            ../WebKit/qt/Api/qwebnetworkinterface.cpp \
            ../WebKit/qt/Api/qwebobjectplugin.cpp \
            ../WebKit/qt/Api/qwebobjectpluginconnector.cpp \
            ../WebKit/qt/Api/qcookiejar.cpp

     }
}

gtk-port {
    HEADERS += \
        ../WebCore/platform/gtk/ClipboardGtk.h \
        ../WebCore/platform/gtk/PasteboardHelper.h \
        ../WebKit/gtk/webkit/webkit.h \
        ../WebKit/gtk/webkit/webkitdefines.h \
        ../WebKit/gtk/webkit/webkitnetworkrequest.h \
        ../WebKit/gtk/webkit/webkitprivate.h \
        ../WebKit/gtk/webkit/webkitwebbackforwardlist.h \
        ../WebKit/gtk/webkit/webkitwebframe.h \
        ../WebKit/gtk/webkit/webkitwebhistoryitem.h \
        ../WebKit/gtk/webkit/webkitwebsettings.h \
        ../WebKit/gtk/webkit/webkitwebview.h \
        ../WebKit/gtk/WebCoreSupport/ChromeClientGtk.h \
        ../WebKit/gtk/WebCoreSupport/ContextMenuClientGtk.h \
        ../WebKit/gtk/WebCoreSupport/DragClientGtk.h \
        ../WebKit/gtk/WebCoreSupport/EditorClientGtk.h \
        ../WebKit/gtk/WebCoreSupport/FrameLoaderClientGtk.h \
        ../WebKit/gtk/WebCoreSupport/InspectorClientGtk.h \
        ../WebKit/gtk/WebCoreSupport/PasteboardHelperGtk.h
    SOURCES += \
        platform/graphics/StringTruncator.cpp \
        platform/text/TextCodecICU.cpp \
        platform/text/TextBoundariesICU.cpp \
        platform/text/TextBreakIteratorICU.cpp \
        page/gtk/EventHandlerGtk.cpp \
        page/gtk/FrameGtk.cpp \
        page/gtk/DragControllerGtk.cpp \
        platform/gtk/ClipboardGtk.cpp \
        platform/gtk/CookieJarGtk.cpp \
        platform/gtk/CursorGtk.cpp \
        platform/gtk/ContextMenuGtk.cpp \
        platform/gtk/ContextMenuItemGtk.cpp \
        platform/gtk/DragDataGtk.cpp \
        platform/gtk/DragImageGtk.cpp \
        platform/gtk/FileChooserGtk.cpp \
        platform/gtk/FileSystemGtk.cpp \
        platform/graphics/gtk/FontCacheGtk.cpp \
        platform/graphics/gtk/FontCustomPlatformData.cpp \
        platform/graphics/gtk/FontGtk.cpp \
        platform/graphics/gtk/FontPlatformDataGtk.cpp \
        platform/graphics/gtk/GlyphPageTreeNodeGtk.cpp \
        platform/graphics/gtk/SimpleFontDataGtk.cpp \
        platform/gtk/KeyEventGtk.cpp \
        platform/gtk/Language.cpp \
        platform/gtk/LocalizedStringsGtk.cpp \
        platform/gtk/LoggingGtk.cpp \
        platform/gtk/MIMETypeRegistryGtk.cpp \
        platform/gtk/MouseEventGtk.cpp \
        platform/gtk/PasteboardGtk.cpp \
        platform/gtk/PlatformScreenGtk.cpp \
        platform/gtk/PlatformScrollBarGtk.cpp \
        platform/gtk/PopupMenuGtk.cpp \
        platform/gtk/RenderThemeGtk.cpp \
        platform/gtk/SearchPopupMenuGtk.cpp \
        platform/gtk/ScrollViewGtk.cpp \
        platform/gtk/SharedTimerGtk.cpp \
        platform/gtk/SoundGtk.cpp \
        platform/gtk/SystemTimeGtk.cpp \
        platform/gtk/TemporaryLinkStubs.cpp \
        platform/text/gtk/TextBreakIteratorInternalICUGtk.cpp \
        platform/gtk/ThreadingGtk.cpp \
        platform/gtk/WheelEventGtk.cpp \
        platform/gtk/WidgetGtk.cpp \
        platform/gtk/gtk2drawing.c \
        platform/graphics/gtk/ColorGtk.cpp \
        platform/graphics/gtk/IconGtk.cpp \
        platform/graphics/gtk/ImageGtk.cpp \
        platform/graphics/gtk/IntPointGtk.cpp \
        platform/graphics/gtk/IntRectGtk.cpp \
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
        platform/image-decoders/jpeg/JPEGImageDecoder.cpp \
        platform/image-decoders/bmp/BMPImageDecoder.cpp \
        platform/image-decoders/ico/ICOImageDecoder.cpp \
        platform/image-decoders/xbm/XBMImageDecoder.cpp \
        ../WebKit/gtk/webkit/webkitnetworkrequest.cpp \
        ../WebKit/gtk/webkit/webkitprivate.cpp \
        ../WebKit/gtk/webkit/webkitwebbackforwardlist.cpp \
        ../WebKit/gtk/webkit/webkitwebframe.cpp \
        ../WebKit/gtk/webkit/webkitwebhistoryitem.cpp \
        ../WebKit/gtk/webkit/webkitwebsettings.cpp \
        ../WebKit/gtk/webkit/webkitwebview.cpp \
        ../WebKit/gtk/WebCoreSupport/ChromeClientGtk.cpp \
        ../WebKit/gtk/WebCoreSupport/ContextMenuClientGtk.cpp \
        ../WebKit/gtk/WebCoreSupport/DragClientGtk.cpp \
        ../WebKit/gtk/WebCoreSupport/EditorClientGtk.cpp \
        ../WebKit/gtk/WebCoreSupport/FrameLoaderClientGtk.cpp \
        ../WebKit/gtk/WebCoreSupport/InspectorClientGtk.cpp \
        ../WebKit/gtk/WebCoreSupport/PasteboardHelperGtk.cpp
}

contains(DEFINES, ENABLE_DATABASE=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_DATABASE=1

    CONFIG(QTDIR_build) {
        # some what copied from src/plugins/sqldrivers/sqlite/sqlite.pro
        system-sqlite {
            LIBS *= $$QT_LFLAGS_SQLITE
            QMAKE_CXXFLAGS *= $$QT_CFLAGS_SQLITE
        } else {
            CONFIG(release, debug|release):DEFINES *= NDEBUG
            INCLUDEPATH += $$QT_SOURCE_TREE/src/3rdparty/sqlite/
            SOURCES += $$QT_SOURCE_TREE/src/3rdparty/sqlite/sqlite3.c
        }
    } else {
        qt-port: INCLUDEPATH += $$[QT_INSTALL_PREFIX]/src/3rdparty/sqlite/
        LIBS += -lsqlite3
    }

    SOURCES += \
        platform/sql/SQLiteAuthorizer.cpp \
        platform/sql/SQLiteDatabase.cpp \
        platform/sql/SQLiteStatement.cpp \
        platform/sql/SQLiteTransaction.cpp \
        platform/sql/SQLValue.cpp \
        storage/ChangeVersionWrapper.cpp \
        storage/DatabaseAuthorizer.cpp \
        storage/Database.cpp \
        storage/DatabaseTask.cpp \
        storage/DatabaseThread.cpp \
        storage/DatabaseTracker.cpp \
        storage/OriginQuotaManager.cpp \
        storage/OriginUsageRecord.cpp \
        storage/SQLResultSet.cpp \
        storage/SQLResultSetRowList.cpp \
        storage/SQLStatement.cpp \
        storage/SQLTransaction.cpp \
        bindings/js/JSCustomSQLStatementCallback.cpp \
        bindings/js/JSCustomSQLStatementErrorCallback.cpp \
        bindings/js/JSCustomSQLTransactionCallback.cpp \
        bindings/js/JSCustomSQLTransactionErrorCallback.cpp \
        bindings/js/JSDatabaseCustom.cpp \
        bindings/js/JSSQLResultSetRowListCustom.cpp \
        bindings/js/JSSQLTransactionCustom.cpp

    IDL_BINDINGS += \
        storage/Database.idl \
        storage/SQLError.idl \
        storage/SQLResultSet.idl \
        storage/SQLResultSetRowList.idl \
        storage/SQLTransaction.idl
}

contains(DEFINES, ENABLE_ICONDATABASE=1) {
    SOURCES += \
        loader/icon/IconDatabase.cpp \
        loader/icon/IconRecord.cpp \
        loader/icon/PageURLRecord.cpp
} else {
    SOURCES += \
        loader/icon/IconDatabaseNone.cpp
}

contains(DEFINES, ENABLE_VIDEO=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_VIDEO=1

    IDL_BINDINGS += \
        html/HTMLAudioElement.idl \
        html/HTMLMediaElement.idl \
        html/HTMLSourceElement.idl \
        html/HTMLVideoElement.idl \
        html/MediaError.idl \
        html/TimeRanges.idl \
        html/VoidCallback.idl 

    SOURCES += \
        html/HTMLAudioElement.cpp \
        html/HTMLMediaElement.cpp \
        html/HTMLSourceElement.cpp \
        html/HTMLVideoElement.cpp \
        html/TimeRanges.cpp \
        platform/graphics/MediaPlayer.cpp \
        rendering/MediaControlElements.cpp \
        rendering/RenderVideo.cpp \
        rendering/RenderMedia.cpp \
        bindings/js/JSAudioConstructor.cpp

    gtk-port {
        SOURCES += \
            platform/graphics/gtk/MediaPlayerPrivateGStreamer.cpp \
            platform/graphics/gtk/VideoSinkGStreamer.cpp

        CONFIG(debug):DEFINES += GST_DISABLE_DEPRECATED

        PKGCONFIG += gstreamer-0.10 gstreamer-plugins-base-0.10 gnome-vfs-2.0
        LIBS += -lgstinterfaces-0.10 -lgstbase-0.10 -lgstvideo-0.10
    }
}

contains(DEFINES, ENABLE_XPATH=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_XPATH=1

    XPATHBISON = $$PWD/xml/XPathGrammar.y

    IDL_BINDINGS += \
        xml/XPathNSResolver.idl \
        xml/XPathException.idl \
        xml/XPathExpression.idl \
        xml/XPathResult.idl \
        xml/XPathEvaluator.idl
}

unix:!mac:CONFIG += link_pkgconfig

contains(DEFINES, ENABLE_XSLT=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_XSLT=1
    PKGCONFIG += libxml-2.0 libxslt

    macx {
        INCLUDEPATH += /usr/include/libxml2
        LIBS += -lxml2 -lxslt
    }

    win32-msvc* {
        LIBS += -llibxml2 -llibxslt
    }
}

contains(DEFINES, ENABLE_XBL=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_XBL=1
}

contains(DEFINES, ENABLE_SVG=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_SVG=1

    DEPENDPATH += svg svg/graphics
    qt-port {
    DEPENDPATH += svg/graphics/qt
    }

    gtk-port {
    DEPENDPATH += svg/graphics/cairo
    }

    SVG_NAMES = $$PWD/svg/svgtags.in

    XLINK_NAMES = $$PWD/svg/xlinkattrs.in

    IDL_BINDINGS += svg/SVGZoomEvent.idl \
        svg/SVGAElement.idl \
        svg/SVGAngle.idl \
        svg/SVGAnimateColorElement.idl \
        svg/SVGAnimatedAngle.idl \
        svg/SVGAnimatedBoolean.idl \
        svg/SVGAnimatedEnumeration.idl \
        svg/SVGAnimatedInteger.idl \
        svg/SVGAnimatedLength.idl \
        svg/SVGAnimatedLengthList.idl \
        svg/SVGAnimatedNumber.idl \
        svg/SVGAnimatedNumberList.idl \
        svg/SVGAnimatedPreserveAspectRatio.idl \
        svg/SVGAnimatedRect.idl \
        svg/SVGAnimatedString.idl \
        svg/SVGAnimatedTransformList.idl \
        svg/SVGAnimateElement.idl \
        svg/SVGAnimateTransformElement.idl \
        svg/SVGAnimationElement.idl \
        svg/SVGCircleElement.idl \
        svg/SVGClipPathElement.idl \
        svg/SVGColor.idl \
        svg/SVGComponentTransferFunctionElement.idl \
        svg/SVGCursorElement.idl \
        svg/SVGDefinitionSrcElement.idl \
        svg/SVGDefsElement.idl \
        svg/SVGDescElement.idl \
        svg/SVGDocument.idl \
        svg/SVGElement.idl \
        svg/SVGElementInstance.idl \
        svg/SVGElementInstanceList.idl \
        svg/SVGEllipseElement.idl \
        svg/SVGException.idl \
        svg/SVGFEBlendElement.idl \
        svg/SVGFEColorMatrixElement.idl \
        svg/SVGFEComponentTransferElement.idl \
        svg/SVGFECompositeElement.idl \
        svg/SVGFEDiffuseLightingElement.idl \
        svg/SVGFEDisplacementMapElement.idl \
        svg/SVGFEDistantLightElement.idl \
        svg/SVGFEFloodElement.idl \
        svg/SVGFEFuncAElement.idl \
        svg/SVGFEFuncBElement.idl \
        svg/SVGFEFuncGElement.idl \
        svg/SVGFEFuncRElement.idl \
        svg/SVGFEGaussianBlurElement.idl \
        svg/SVGFEImageElement.idl \
        svg/SVGFEMergeElement.idl \
        svg/SVGFEMergeNodeElement.idl \
        svg/SVGFEOffsetElement.idl \
        svg/SVGFEPointLightElement.idl \
        svg/SVGFESpecularLightingElement.idl \
        svg/SVGFESpotLightElement.idl \
        svg/SVGFETileElement.idl \
        svg/SVGFETurbulenceElement.idl \
        svg/SVGFilterElement.idl \
        svg/SVGFontElement.idl \
        svg/SVGFontFaceElement.idl \
        svg/SVGFontFaceFormatElement.idl \
        svg/SVGFontFaceNameElement.idl \
        svg/SVGFontFaceSrcElement.idl \
        svg/SVGFontFaceUriElement.idl \
        svg/SVGForeignObjectElement.idl \
        svg/SVGGElement.idl \
        svg/SVGGlyphElement.idl \
        svg/SVGGradientElement.idl \
        svg/SVGImageElement.idl \
        svg/SVGLength.idl \
        svg/SVGLengthList.idl \
        svg/SVGLinearGradientElement.idl \
        svg/SVGLineElement.idl \
        svg/SVGMarkerElement.idl \
        svg/SVGMaskElement.idl \
        svg/SVGMatrix.idl \
        svg/SVGMetadataElement.idl \
        svg/SVGMissingGlyphElement.idl \
        svg/SVGNumber.idl \
        svg/SVGNumberList.idl \
        svg/SVGPaint.idl \
        svg/SVGPathElement.idl \
        svg/SVGPathSegArcAbs.idl \
        svg/SVGPathSegArcRel.idl \
        svg/SVGPathSegClosePath.idl \
        svg/SVGPathSegCurvetoCubicAbs.idl \
        svg/SVGPathSegCurvetoCubicRel.idl \
        svg/SVGPathSegCurvetoCubicSmoothAbs.idl \
        svg/SVGPathSegCurvetoCubicSmoothRel.idl \
        svg/SVGPathSegCurvetoQuadraticAbs.idl \
        svg/SVGPathSegCurvetoQuadraticRel.idl \
        svg/SVGPathSegCurvetoQuadraticSmoothAbs.idl \
        svg/SVGPathSegCurvetoQuadraticSmoothRel.idl \
        svg/SVGPathSeg.idl \
        svg/SVGPathSegLinetoAbs.idl \
        svg/SVGPathSegLinetoHorizontalAbs.idl \
        svg/SVGPathSegLinetoHorizontalRel.idl \
        svg/SVGPathSegLinetoRel.idl \
        svg/SVGPathSegLinetoVerticalAbs.idl \
        svg/SVGPathSegLinetoVerticalRel.idl \
        svg/SVGPathSegList.idl \
        svg/SVGPathSegMovetoAbs.idl \
        svg/SVGPathSegMovetoRel.idl \
        svg/SVGPatternElement.idl \
        svg/SVGPoint.idl \
        svg/SVGPointList.idl \
        svg/SVGPolygonElement.idl \
        svg/SVGPolylineElement.idl \
        svg/SVGPreserveAspectRatio.idl \
        svg/SVGRadialGradientElement.idl \
        svg/SVGRectElement.idl \
        svg/SVGRect.idl \
        svg/SVGRenderingIntent.idl \
        svg/SVGScriptElement.idl \
        svg/SVGSetElement.idl \
        svg/SVGStopElement.idl \
        svg/SVGStringList.idl \
        svg/SVGStyleElement.idl \
        svg/SVGSVGElement.idl \
        svg/SVGSwitchElement.idl \
        svg/SVGSymbolElement.idl \
        svg/SVGTextContentElement.idl \
        svg/SVGTextElement.idl \
        svg/SVGTextPathElement.idl \
        svg/SVGTextPositioningElement.idl \
        svg/SVGTitleElement.idl \
        svg/SVGTransform.idl \
        svg/SVGTransformList.idl \
        svg/SVGTRefElement.idl \
        svg/SVGTSpanElement.idl \
        svg/SVGUnitTypes.idl \
        svg/SVGUseElement.idl \
        svg/SVGViewElement.idl 

    SOURCES += \
# TODO: this-one-is-not-auto-added! FIXME! tmp/SVGElementFactory.cpp \
        bindings/js/JSSVGElementWrapperFactory.cpp \
        bindings/js/JSSVGMatrixCustom.cpp \
        bindings/js/JSSVGPathSegCustom.cpp \
        bindings/js/JSSVGPathSegListCustom.cpp \
        bindings/js/JSSVGPointListCustom.cpp \
        bindings/js/JSSVGTransformListCustom.cpp \
        css/SVGCSSComputedStyleDeclaration.cpp \
        css/SVGCSSParser.cpp \
        css/SVGCSSStyleSelector.cpp \
        rendering/SVGRenderStyle.cpp \
        rendering/SVGRenderStyleDefs.cpp \
        bindings/js/JSSVGLazyEventListener.cpp \
        svg/SVGZoomEvent.cpp \
        rendering/PointerEventsHitRules.cpp \
        svg/SVGDocumentExtensions.cpp \
        svg/SVGImageLoader.cpp \
        svg/SVGTimer.cpp \
        svg/TimeScheduler.cpp \
        svg/ColorDistance.cpp \
        svg/SVGAElement.cpp \
        svg/SVGAngle.cpp \
        svg/SVGAnimateColorElement.cpp \
        svg/SVGAnimatedPathData.cpp \
        svg/SVGAnimatedPoints.cpp \
        svg/SVGAnimateElement.cpp \
        svg/SVGAnimateMotionElement.cpp \
        svg/SVGAnimateTransformElement.cpp \
        svg/SVGAnimationElement.cpp \
        svg/SVGCircleElement.cpp \
        svg/SVGClipPathElement.cpp \
        svg/SVGColor.cpp \
        svg/SVGComponentTransferFunctionElement.cpp \
        svg/SVGCursorElement.cpp \
        svg/SVGDefinitionSrcElement.cpp \
        svg/SVGDefsElement.cpp \
        svg/SVGDescElement.cpp \
        svg/SVGDocument.cpp \
        svg/SVGElement.cpp \
        svg/SVGElementInstance.cpp \
        svg/SVGElementInstanceList.cpp \
        svg/SVGEllipseElement.cpp \
        svg/SVGExternalResourcesRequired.cpp \
        svg/SVGFEBlendElement.cpp \
        svg/SVGFEColorMatrixElement.cpp \
        svg/SVGFEComponentTransferElement.cpp \
        svg/SVGFECompositeElement.cpp \
        svg/SVGFEDiffuseLightingElement.cpp \
        svg/SVGFEDisplacementMapElement.cpp \
        svg/SVGFEDistantLightElement.cpp \
        svg/SVGFEFloodElement.cpp \
        svg/SVGFEFuncAElement.cpp \
        svg/SVGFEFuncBElement.cpp \
        svg/SVGFEFuncGElement.cpp \
        svg/SVGFEFuncRElement.cpp \
        svg/SVGFEGaussianBlurElement.cpp \
        svg/SVGFEImageElement.cpp \
        svg/SVGFELightElement.cpp \
        svg/SVGFEMergeElement.cpp \
        svg/SVGFEMergeNodeElement.cpp \
        svg/SVGFEOffsetElement.cpp \
        svg/SVGFEPointLightElement.cpp \
        svg/SVGFESpecularLightingElement.cpp \
        svg/SVGFESpotLightElement.cpp \
        svg/SVGFETileElement.cpp \
        svg/SVGFETurbulenceElement.cpp \
        svg/SVGFilterElement.cpp \
        svg/SVGFilterPrimitiveStandardAttributes.cpp \
        svg/SVGFitToViewBox.cpp \
        svg/SVGFont.cpp \
        svg/SVGFontData.cpp \
        svg/SVGFontElement.cpp \
        svg/SVGFontFaceElement.cpp \
        svg/SVGFontFaceFormatElement.cpp \
        svg/SVGFontFaceNameElement.cpp \
        svg/SVGFontFaceSrcElement.cpp \
        svg/SVGFontFaceUriElement.cpp \
        svg/SVGForeignObjectElement.cpp \
        svg/SVGGElement.cpp \
        svg/SVGGlyphElement.cpp \
        svg/SVGGradientElement.cpp \
        svg/SVGImageElement.cpp \
        svg/SVGLangSpace.cpp \
        svg/SVGLength.cpp \
        svg/SVGLengthList.cpp \
        svg/SVGLinearGradientElement.cpp \
        svg/SVGLineElement.cpp \
        svg/SVGLocatable.cpp \
        svg/SVGMarkerElement.cpp \
        svg/SVGMaskElement.cpp \
        svg/SVGMetadataElement.cpp \
        svg/SVGMissingGlyphElement.cpp \
        svg/SVGMPathElement.cpp \
        svg/SVGNumberList.cpp \
        svg/SVGPaint.cpp \
        svg/SVGParserUtilities.cpp \
        svg/SVGPathElement.cpp \
        svg/SVGPathSegArc.cpp \
        svg/SVGPathSegClosePath.cpp \
        svg/SVGPathSegCurvetoCubic.cpp \
        svg/SVGPathSegCurvetoCubicSmooth.cpp \
        svg/SVGPathSegCurvetoQuadratic.cpp \
        svg/SVGPathSegCurvetoQuadraticSmooth.cpp \
        svg/SVGPathSegLineto.cpp \
        svg/SVGPathSegLinetoHorizontal.cpp \
        svg/SVGPathSegLinetoVertical.cpp \
        svg/SVGPathSegList.cpp \
        svg/SVGPathSegMoveto.cpp \
        svg/SVGPatternElement.cpp \
        svg/SVGPointList.cpp \
        svg/SVGPolyElement.cpp \
        svg/SVGPolygonElement.cpp \
        svg/SVGPolylineElement.cpp \
        svg/SVGPreserveAspectRatio.cpp \
        svg/SVGRadialGradientElement.cpp \
        svg/SVGRectElement.cpp \
        svg/SVGScriptElement.cpp \
        svg/SVGSetElement.cpp \
        svg/SVGStopElement.cpp \
        svg/SVGStringList.cpp \
        svg/SVGStylable.cpp \
        svg/SVGStyledElement.cpp \
        svg/SVGStyledLocatableElement.cpp \
        svg/SVGStyledTransformableElement.cpp \
        svg/SVGStyleElement.cpp \
        svg/SVGSVGElement.cpp \
        svg/SVGSwitchElement.cpp \
        svg/SVGSymbolElement.cpp \
        svg/SVGTests.cpp \
        svg/SVGTextContentElement.cpp \
        svg/SVGTextElement.cpp \
        svg/SVGTextPathElement.cpp \
        svg/SVGTextPositioningElement.cpp \
        svg/SVGTitleElement.cpp \
        svg/SVGTransformable.cpp \
        svg/SVGTransform.cpp \
        svg/SVGTransformDistance.cpp \
        svg/SVGTransformList.cpp \
        svg/SVGTRefElement.cpp \
        svg/SVGTSpanElement.cpp \
        svg/SVGURIReference.cpp \
        svg/SVGUseElement.cpp \
        svg/SVGViewElement.cpp \
        svg/SVGViewSpec.cpp \
        svg/SVGZoomAndPan.cpp \
        svg/graphics/filters/SVGFEBlend.cpp \
        svg/graphics/filters/SVGFEColorMatrix.cpp \
        svg/graphics/filters/SVGFEComponentTransfer.cpp \
        svg/graphics/filters/SVGFEComposite.cpp \
        svg/graphics/filters/SVGFEConvolveMatrix.cpp \
        svg/graphics/filters/SVGFEDiffuseLighting.cpp \
        svg/graphics/filters/SVGFEDisplacementMap.cpp \
        svg/graphics/filters/SVGFEFlood.cpp \
        svg/graphics/filters/SVGFEGaussianBlur.cpp \
        svg/graphics/filters/SVGFEImage.cpp \
        svg/graphics/filters/SVGFEMerge.cpp \
        svg/graphics/filters/SVGFEMorphology.cpp \
        svg/graphics/filters/SVGFEOffset.cpp \
        svg/graphics/filters/SVGFESpecularLighting.cpp \
        svg/graphics/filters/SVGFETurbulence.cpp \
        svg/graphics/filters/SVGFilterEffect.cpp \
        svg/graphics/filters/SVGLightSource.cpp \
        svg/graphics/SVGImage.cpp \
        svg/graphics/SVGPaintServer.cpp \
        svg/graphics/SVGPaintServerGradient.cpp \
        svg/graphics/SVGPaintServerLinearGradient.cpp \
        svg/graphics/SVGPaintServerPattern.cpp \
        svg/graphics/SVGPaintServerRadialGradient.cpp \
        svg/graphics/SVGPaintServerSolid.cpp \
        svg/graphics/SVGResourceClipper.cpp \
        svg/graphics/SVGResource.cpp \
        svg/graphics/SVGResourceFilter.cpp \
        svg/graphics/SVGResourceMarker.cpp \
        svg/graphics/SVGResourceMasker.cpp \
        rendering/RenderForeignObject.cpp \
        rendering/RenderPath.cpp \
        rendering/RenderSVGBlock.cpp \
        rendering/RenderSVGContainer.cpp \
        rendering/RenderSVGGradientStop.cpp \
        rendering/RenderSVGHiddenContainer.cpp \
        rendering/RenderSVGImage.cpp \
        rendering/RenderSVGInline.cpp \
        rendering/RenderSVGInlineText.cpp \
        rendering/RenderSVGRoot.cpp \
        rendering/RenderSVGText.cpp \
        rendering/RenderSVGTextPath.cpp \
        rendering/RenderSVGTransformableContainer.cpp \
        rendering/RenderSVGTSpan.cpp \
        rendering/RenderSVGViewportContainer.cpp \
        rendering/SVGCharacterLayoutInfo.cpp \
        rendering/SVGInlineFlowBox.cpp \
        rendering/SVGInlineTextBox.cpp \
        rendering/SVGRenderSupport.cpp \
        rendering/SVGRootInlineBox.cpp

qt-port:SOURCES += \
        svg/graphics/qt/RenderPathQt.cpp \
        svg/graphics/qt/SVGPaintServerGradientQt.cpp \
        svg/graphics/qt/SVGPaintServerLinearGradientQt.cpp \
        svg/graphics/qt/SVGPaintServerPatternQt.cpp \
        svg/graphics/qt/SVGPaintServerQt.cpp \
        svg/graphics/qt/SVGPaintServerRadialGradientQt.cpp \
        svg/graphics/qt/SVGPaintServerSolidQt.cpp \
        svg/graphics/qt/SVGResourceClipperQt.cpp \
        svg/graphics/qt/SVGResourceFilterQt.cpp \
        svg/graphics/qt/SVGResourceMaskerQt.cpp

gtk-port:SOURCES += \
        svg/graphics/cairo/RenderPathCairo.cpp \
        svg/graphics/cairo/SVGPaintServerCairo.cpp \
        svg/graphics/cairo/SVGPaintServerGradientCairo.cpp \
        svg/graphics/cairo/SVGPaintServerPatternCairo.cpp \
        svg/graphics/cairo/SVGPaintServerSolidCairo.cpp \
        svg/graphics/cairo/SVGResourceClipperCairo.cpp \
        svg/graphics/cairo/SVGResourceMaskerCairo.cpp

        # GENERATOR 5-C:
        svgnames_a.output = $$GENERATED_SOURCES_DIR/SVGNames.cpp
        svgnames_a.commands = perl $$PWD/dom/make_names.pl --tags $$PWD/svg/svgtags.in --attrs $$PWD/svg/svgattrs.in --namespace SVG --cppNamespace WebCore --namespaceURI 'http://www.w3.org/2000/svg' --factory --attrsNullNamespace --preprocessor \"$${QMAKE_MOC} -E\" --output $$GENERATED_SOURCES_DIR
        svgnames_a.input = SVG_NAMES
        svgnames_a.dependency_type = TYPE_C
        svgnames_a.CONFIG = target_predeps
        svgnames_a.variable_out = GENERATED_SOURCES
        svgnames_a.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}SVGNames.h
        addExtraCompiler(svgnames_a)
        svgnames_b.output = $$GENERATED_SOURCES_DIR/SVGElementFactory.cpp
        svgnames_b.commands = @echo -n ''
        svgnames_b.input = SVG_NAMES
        svgnames_b.depends = $$GENERATED_SOURCES_DIR/SVGNames.cpp
        svgnames_b.CONFIG = target_predeps
        svgnames_b.variable_out = GENERATED_SOURCES
        svgnames_b.clean += ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}SVGElementFactory.h ${QMAKE_FILE_OUT}
        addExtraCompiler(svgnames_b)

        # GENERATOR 5-D:
        xlinknames.output = $$GENERATED_SOURCES_DIR/XLinkNames.cpp
        xlinknames.commands = perl $$PWD/dom/make_names.pl --attrs $$PWD/svg/xlinkattrs.in --namespace XLink --cppNamespace WebCore --namespaceURI 'http://www.w3.org/1999/xlink' --preprocessor \"$${QMAKE_MOC} -E\" --output $$GENERATED_SOURCES_DIR
        xlinknames.input = XLINK_NAMES
        xlinknames.dependency_type = TYPE_C
        xlinknames.CONFIG = target_predeps
        xlinknames.variable_out = GENERATED_SOURCES
        xlinknames.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}XLinkNames.h
        addExtraCompiler(xlinknames)

    # GENERATOR 6-A:
    cssprops.output = $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.c
    cssprops.input = WALDOCSSPROPS
    cssprops.commands = perl -ne \"print lc\" ${QMAKE_FILE_NAME} $$SVGCSSPROPERTIES > $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.in && cd $$GENERATED_SOURCES_DIR && perl $$PWD/css/makeprop.pl && $(DEL_FILE) ${QMAKE_FILE_BASE}.strip ${QMAKE_FILE_BASE}.in ${QMAKE_FILE_BASE}.gperf
    cssprops.CONFIG = target_predeps no_link
    cssprops.depend = ${QMAKE_FILE_NAME} SVGCSSPROPERTIES
    cssprops.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}${QMAKE_FILE_BASE}.h
    addExtraCompiler(cssprops)

    # GENERATOR 6-B:
    cssvalues.output = $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.c
    cssvalues.input = WALDOCSSVALUES
    cssvalues.commands = perl -ne \"print lc\" ${QMAKE_FILE_NAME} $$SVGCSSVALUES > $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.in && cd $$GENERATED_SOURCES_DIR && perl $$PWD/css/makevalues.pl && $(DEL_FILE) ${QMAKE_FILE_BASE}.in ${QMAKE_FILE_BASE}.strip ${QMAKE_FILE_BASE}.gperf
    cssvalues.CONFIG = target_predeps no_link
    cssvalues.depend = ${QMAKE_FILE_NAME} SVGCSSVALUES
    cssvalues.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}${QMAKE_FILE_BASE}.h
    addExtraCompiler(cssvalues)
} else {
    # GENERATOR 6-A:
    cssprops.output = $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.c
    cssprops.input = WALDOCSSPROPS
    cssprops.commands = $(COPY_FILE) ${QMAKE_FILE_NAME} $$GENERATED_SOURCES_DIR && cd $$GENERATED_SOURCES_DIR && perl $$PWD/css/makeprop.pl && $(DEL_FILE) ${QMAKE_FILE_BASE}.strip ${QMAKE_FILE_BASE}.in ${QMAKE_FILE_BASE}.gperf
    cssprops.CONFIG = target_predeps no_link
    cssprops.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}${QMAKE_FILE_BASE}.h
    addExtraCompiler(cssprops)

    # GENERATOR 6-B:
    cssvalues.output = $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.c
    cssvalues.input = WALDOCSSVALUES
    cssvalues.commands = $(COPY_FILE) ${QMAKE_FILE_NAME} $$GENERATED_SOURCES_DIR && cd $$GENERATED_SOURCES_DIR && perl $$PWD/css/makevalues.pl && $(DEL_FILE) ${QMAKE_FILE_BASE}.in ${QMAKE_FILE_BASE}.strip ${QMAKE_FILE_BASE}.gperf
    cssvalues.CONFIG = target_predeps no_link
    cssvalues.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}${QMAKE_FILE_BASE}.h
    addExtraCompiler(cssvalues)
}


# GENERATOR 1: IDL compiler
idl.output = $$GENERATED_SOURCES_DIR/JS${QMAKE_FILE_BASE}.cpp
idl.variable_out = GENERATED_SOURCES
idl.input = IDL_BINDINGS
idl.commands = perl -I$$PWD/bindings/scripts $$PWD/bindings/scripts/generate-bindings.pl --defines \"$${FEATURE_DEFINES_JAVASCRIPT}\" --generator JS --include $$PWD/dom --include $$PWD/html --include $$PWD/xml --include $$PWD/svg --outputdir $$GENERATED_SOURCES_DIR --preprocessor \"$${QMAKE_MOC} -E\" ${QMAKE_FILE_NAME}
idl.CONFIG += target_predeps
idl.clean = ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}JS${QMAKE_FILE_BASE}.h ${QMAKE_FILE_OUT}
addExtraCompiler(idl)

# GENERATOR 2-A: LUT creator
#lut.output = $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.lut.h
#lut.commands = perl $$PWD/../JavaScriptCore/kjs/create_hash_table ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
#lut.depend = ${QMAKE_FILE_NAME}
#lut.input = LUT_FILES
#lut.CONFIG += no_link
#QMAKE_EXTRA_COMPILERS += lut

# GENERATOR 2-B: like JavaScriptCore/LUT Generator, but rename output
luttable.output = $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}Table.cpp
luttable.commands = perl $$PWD/../JavaScriptCore/kjs/create_hash_table ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
luttable.depend = ${QMAKE_FILE_NAME}
luttable.input = LUT_TABLE_FILES
luttable.CONFIG += no_link
luttable.dependency_type = TYPE_C
addExtraCompiler(luttable)

# GENERATOR 3: tokenizer (flex)
tokenizer.output = $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.cpp
tokenizer.commands = flex -t < ${QMAKE_FILE_NAME} | perl $$PWD/css/maketokenizer > ${QMAKE_FILE_OUT}
tokenizer.dependency_type = TYPE_C
tokenizer.input = TOKENIZER
tokenizer.CONFIG += target_predeps no_link
addExtraCompiler(tokenizer)

# GENERATOR 4: CSS grammar
cssbison.output = $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.cpp
cssbison.commands = perl $$PWD/css/makegrammar.pl ${QMAKE_FILE_NAME} $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}
cssbison.depend = ${QMAKE_FILE_NAME}
cssbison.input = CSSBISON
cssbison.CONFIG = target_predeps
cssbison.dependency_type = TYPE_C
cssbison.variable_out = GENERATED_SOURCES
cssbison.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}${QMAKE_FILE_BASE}.h
addExtraCompiler(cssbison)
#PRE_TARGETDEPS += $$GENERATED_SOURCES_DIR/CSSGrammar.cpp
grammar_h_dep.target = tmp/CSSParser.o
grammar_h_dep.depends = $$GENERATED_SOURCES_DIR/CSSGrammar.cpp $$GENERATED_SOURCES_DIR/HTMLNames.cpp
QMAKE_EXTRA_TARGETS += grammar_h_dep

# GENERATOR 5-A:
htmlnames.output = $$GENERATED_SOURCES_DIR/HTMLNames.cpp
htmlnames.commands = perl $$PWD/dom/make_names.pl --tags $$PWD/html/HTMLTagNames.in --attrs $$PWD/html/HTMLAttributeNames.in --namespace HTML --namespacePrefix xhtml --cppNamespace WebCore --namespaceURI 'http://www.w3.org/1999/xhtml' --attrsNullNamespace --preprocessor \"$${QMAKE_MOC} -E\" --output $$GENERATED_SOURCES_DIR
htmlnames.input = HTML_NAMES
htmlnames.dependency_type = TYPE_C
htmlnames.CONFIG = target_predeps
htmlnames.variable_out = GENERATED_SOURCES
htmlnames.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}HTMLNames.h
addExtraCompiler(htmlnames)

# GENERATOR 5-B:
xmlnames.output = $$GENERATED_SOURCES_DIR/XMLNames.cpp
xmlnames.commands = perl $$PWD/dom/make_names.pl --attrs $$PWD/xml/xmlattrs.in --namespace XML --cppNamespace WebCore --namespaceURI 'http://www.w3.org/XML/1998/namespace' --preprocessor \"$${QMAKE_MOC} -E\" --output $$GENERATED_SOURCES_DIR
xmlnames.input = XML_NAMES
xmlnames.dependency_type = TYPE_C
xmlnames.CONFIG = target_predeps
xmlnames.variable_out = GENERATED_SOURCES
xmlnames.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}XMLNames.h
addExtraCompiler(xmlnames)

# GENERATOR 8-A:
entities.output = $$GENERATED_SOURCES_DIR/HTMLEntityNames.c
entities.commands = gperf -a -L ANSI-C -C -G -c -o -t --key-positions="*" -N findEntity -D -s 2 < $$PWD/html/HTMLEntityNames.gperf > $$GENERATED_SOURCES_DIR/HTMLEntityNames.c
entities.input = ENTITIES_GPERF
entities.dependency_type = TYPE_C
entities.CONFIG = target_predeps no_link
entities.clean = ${QMAKE_FILE_OUT}
addExtraCompiler(entities)

# GENERATOR 8-B:
doctypestrings.output = $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.cpp
doctypestrings.input = DOCTYPESTRINGS
doctypestrings.commands = perl -e \"print \'$${LITERAL_HASH}include <string.h>\';\" > ${QMAKE_FILE_OUT} && echo // bogus >> ${QMAKE_FILE_OUT} && gperf -CEot -L ANSI-C --key-positions="*" -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards < ${QMAKE_FILE_NAME} >> ${QMAKE_FILE_OUT}
doctypestrings.dependency_type = TYPE_C
doctypestrings.CONFIG += target_predeps no_link
doctypestrings.clean = ${QMAKE_FILE_OUT}
addExtraCompiler(doctypestrings)

# GENERATOR 8-C:
colordata.output = $$GENERATED_SOURCES_DIR/ColorData.c
colordata.commands = perl -e \"print \'$${LITERAL_HASH}include <string.h>\';\" > ${QMAKE_FILE_OUT} && echo // bogus >> ${QMAKE_FILE_OUT} && gperf -CDEot -L ANSI-C --key-positions="*" -N findColor -D -s 2 < ${QMAKE_FILE_NAME} >> ${QMAKE_FILE_OUT}
colordata.input = COLORDAT_GPERF
colordata.CONFIG = target_predeps no_link
addExtraCompiler(colordata)

# GENERATOR 9:
stylesheets.output = $$GENERATED_SOURCES_DIR/UserAgentStyleSheetsData.cpp
stylesheets.commands = perl $$PWD/css/make-css-file-arrays.pl --preprocessor \"$${QMAKE_MOC} -E\" $$GENERATED_SOURCES_DIR/UserAgentStyleSheets.h $$GENERATED_SOURCES_DIR/UserAgentStyleSheetsData.cpp $$PWD/css/html4.css $$PWD/css/quirks.css $$PWD/css/svg.css $$PWD/css/view-source.css
stylesheets.input = STYLESHEETS_EMBED
stylesheets.CONFIG = target_predeps
stylesheets.variable_out = GENERATED_SOURCES
stylesheets.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}UserAgentStyleSheets.h
addExtraCompiler(stylesheets)

# GENERATOR 10: XPATH grammar
xpathbison.output = $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.cpp
xpathbison.commands = bison -d -p xpathyy ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_BASE}.tab.c && $(MOVE) ${QMAKE_FILE_BASE}.tab.c $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.cpp && $(MOVE) ${QMAKE_FILE_BASE}.tab.h $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.h
xpathbison.depend = ${QMAKE_FILE_NAME}
xpathbison.input = XPATHBISON
xpathbison.CONFIG = target_predeps
xpathbison.dependency_type = TYPE_C
xpathbison.variable_out = GENERATED_SOURCES
xpathbison.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}${QMAKE_FILE_BASE}.h
addExtraCompiler(xpathbison)

qt-port:!CONFIG(QTDIR_build) {
    target.path = $$[QT_INSTALL_LIBS]
    include($$PWD/../WebKit/qt/Api/headers.pri)
    headers.files = $$WEBKIT_API_HEADERS
    headers.path = $$[QT_INSTALL_HEADERS]/QtWebKit
    prf.files = $$PWD/../WebKit/qt/Api/qtwebkit.prf
    prf.path = $$[QT_INSTALL_PREFIX]/mkspecs/features

    VERSION=$${QT_MAJOR_VERSION}.$${QT_MINOR_VERSION}.$${QT_PATCH_VERSION}

    win32-* {
        DLLDESTDIR = $$OUTPUT_DIR/bin

        dlltarget.commands = $(COPY_FILE) $(DESTDIR)$(TARGET) $$[QT_INSTALL_BINS]
        dlltarget.CONFIG = no_path
        INSTALLS += dlltarget
    }


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

gtk-port {
    isEmpty(WEBKIT_LIB_DIR):WEBKIT_LIB_DIR=/usr/local/lib
    isEmpty(WEBKIT_INC_DIR):WEBKIT_INC_DIR=/usr/local/include/WebKitGtk

    target.path = $$WEBKIT_LIB_DIR
    INSTALLS += target

    include($$PWD/../WebKit/gtk/webkit/headers.pri)
    headers.files = $$WEBKIT_API_HEADERS
    headers.path = $$WEBKIT_INC_DIR
    INSTALLS += headers

    include($$PWD/../JavaScriptCore/headers.pri)
    jsheaders.files = $$JS_API_HEADERS
    jsheaders.path = $$WEBKIT_INC_DIR/JavaScriptCore
    INSTALLS += jsheaders

    unix {
        CONFIG += create_pc create_prl
        QMAKE_PKGCONFIG_LIBDIR = $$target.path
        QMAKE_PKGCONFIG_INCDIR = $$headers.path
        QMAKE_PKGCONFIG_DESTDIR = pkgconfig
        lib_replace.match = $$DESTDIR
        lib_replace.replace = $$[QT_INSTALL_LIBS]
        QMAKE_PKGCONFIG_INSTALL_REPLACE += lib_replace
    }

    GENMARSHALS = ../WebKit/gtk/webkit/webkit-marshal.list
    GENMARSHALS_PREFIX = webkit_marshal

    #
    # integrate glib-genmarshal as additional compiler
    #
    QMAKE_GENMARSHAL_CC = glib-genmarshal
    glib-genmarshal.output = $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.cpp
    glib-genmarshal.commands = echo 'extern \\"C\\" {' > ${QMAKE_FILE_OUT} && $${QMAKE_GENMARSHAL_CC} --prefix=$${GENMARSHALS_PREFIX} ${QMAKE_FILE_IN} --body >> ${QMAKE_FILE_OUT} && echo '}' >> ${QMAKE_FILE_OUT}
    glib-genmarshal.input = GENMARSHALS
    glib-genmarshal.variable_out = GENERATED_SOURCES
    glib-genmarshal.name = GENMARSHALS
    QMAKE_EXTRA_UNIX_COMPILERS += glib-genmarshal

    glib-genmarshalh.output = $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.h
    glib-genmarshalh.commands = $${QMAKE_GENMARSHAL_CC} --prefix=$${GENMARSHALS_PREFIX} ${QMAKE_FILE_IN} --header > $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.h
    glib-genmarshalh.input = GENMARSHALS
    glib-genmarshalh.variable_out = GENERATED_SOURCES
    glib-genmarshalh.name = GENMARSHALS
    QMAKE_EXTRA_UNIX_COMPILERS += glib-genmarshalh
}
