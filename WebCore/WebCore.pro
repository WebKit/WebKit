# WebCore - qmake build info
CONFIG += building-libs
CONFIG += depend_includepath
include($$PWD/../WebKit.pri)

TEMPLATE = lib
TARGET = QtWebKit

contains(QT_CONFIG, embedded):CONFIG += embedded

CONFIG(QTDIR_build) {
    GENERATED_SOURCES_DIR = $$PWD/generated
    include($$QT_SOURCE_TREE/src/qbase.pri)
    PRECOMPILED_HEADER = $$PWD/../WebKit/qt/WebKit_pch.h
    DEFINES *= NDEBUG
} else {
    !static: DEFINES += QT_MAKEDLL

    CONFIG(debug, debug|release) {
        isEmpty(GENERATED_SOURCES_DIR):GENERATED_SOURCES_DIR = generated$${QMAKE_DIR_SEP}debug
        OBJECTS_DIR = obj/debug
    } else { # Release
        isEmpty(GENERATED_SOURCES_DIR):GENERATED_SOURCES_DIR = generated$${QMAKE_DIR_SEP}release
        OBJECTS_DIR = obj/release
    }

    DESTDIR = $$OUTPUT_DIR/lib
}

GENERATED_SOURCES_DIR_SLASH = $$GENERATED_SOURCES_DIR${QMAKE_DIR_SEP}

unix {
    QMAKE_PKGCONFIG_REQUIRES = QtCore QtGui QtNetwork
    lessThan(QT_MINOR_VERSION, 4): QMAKE_PKGCONFIG_REQUIRES += QtXml
}

include($$OUTPUT_DIR/config.pri)

CONFIG -= warn_on
*-g++*:QMAKE_CXXFLAGS += -Wreturn-type -fno-strict-aliasing

# Disable a few warnings on Windows. The warnings are also
# disabled in WebKitLibraries/win/tools/vsprops/common.vsprops
win32-*: QMAKE_CXXFLAGS += -wd4291 -wd4344

unix:!mac:*-g++*:QMAKE_CXXFLAGS += -ffunction-sections -fdata-sections 
unix:!mac:*-g++*:QMAKE_LFLAGS += -Wl,--gc-sections

#QMAKE_CXXFLAGS += -Wall -Wno-undef -Wno-unused-parameter

CONFIG(release):!CONFIG(QTDIR_build) {
    contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols
    unix:contains(QT_CONFIG, reduce_relocations):CONFIG += bsymbolic_functions
}

linux-*: DEFINES += HAVE_STDINT_H
freebsd-*: DEFINES += HAVE_PTHREAD_NP_H

DEFINES += BUILD_WEBKIT

# Remove whole program optimizations due to miscompilations
win32-msvc2005|win32-msvc2008:{
    QMAKE_CFLAGS_RELEASE -= -GL
    QMAKE_CXXFLAGS_RELEASE -= -GL
}

win32-*: DEFINES += _HAS_TR1=0
wince* {
#    DEFINES += ENABLE_SVG=0 ENABLE_XPATH=0 ENABLE_XBL=0 \
#               ENABLE_SVG_ANIMATION=0 ENABLE_SVG_USE=0  \
#               ENABLE_SVG_FOREIGN_OBJECT=0 ENABLE_SVG_AS_IMAGE=0

    INCLUDEPATH += $$PWD/../JavaScriptCore/os-wince
    INCLUDEPATH += $$PWD/../JavaScriptCore/os-win32
}

# Pick up 3rdparty libraries from INCLUDE/LIB just like with MSVC
win32-g++ {
    TMPPATH            = $$quote($$(INCLUDE))
    QMAKE_INCDIR_POST += $$split(TMPPATH,";")
    TMPPATH            = $$quote($$(LIB))
    QMAKE_LIBDIR_POST += $$split(TMPPATH,";")
}

# Try to locate sqlite3 source
CONFIG(QTDIR_build) {
    SQLITE3SRCDIR = $$QT_SOURCE_TREE/src/3rdparty/sqlite/
} else {
    SQLITE3SRCDIR = $$(SQLITE3SRCDIR)
    isEmpty(SQLITE3SRCDIR) {
        SQLITE3SRCDIR = $$[QT_INSTALL_PREFIX]/src/3rdparty/sqlite/
    } 
}

# Optional components (look for defs in config.h and included files!)

contains(DEFINES, ENABLE_SINGLE_THREADED=1) {
    DEFINES+=ENABLE_DATABASE=0 ENABLE_DOM_STORAGE=0 ENABLE_ICONDATABASE=0 ENABLE_WORKERS=0
}

# turn off SQLITE support if we do not have sqlite3 available
!CONFIG(QTDIR_build):win32-*:!exists( $${SQLITE3SRCDIR}/sqlite3.c ): DEFINES += ENABLE_SQLITE=0 ENABLE_DATABASE=0 ENABLE_ICONDATABASE=0 ENABLE_OFFLINE_WEB_APPLICATIONS=0 ENABLE_DOM_STORAGE=0

!contains(DEFINES, ENABLE_JAVASCRIPT_DEBUGGER=.): DEFINES += ENABLE_JAVASCRIPT_DEBUGGER=1
!contains(DEFINES, ENABLE_DATABASE=.): DEFINES += ENABLE_DATABASE=1
!contains(DEFINES, ENABLE_OFFLINE_WEB_APPLICATIONS=.): DEFINES += ENABLE_OFFLINE_WEB_APPLICATIONS=1
!contains(DEFINES, ENABLE_DOM_STORAGE=.): DEFINES += ENABLE_DOM_STORAGE=1
!contains(DEFINES, ENABLE_ICONDATABASE=.): DEFINES += ENABLE_ICONDATABASE=1
!contains(DEFINES, ENABLE_CHANNEL_MESSAGING=.): DEFINES += ENABLE_CHANNEL_MESSAGING=1

# turn on SQLITE support if any of the dependent features are turned on
!contains(DEFINES, ENABLE_SQLITE=.) {
  contains(DEFINES, ENABLE_DATABASE=1)|contains(DEFINES, ENABLE_ICONDATABASE=1)|contains(DEFINES, ENABLE_DOM_STORAGE=1)|contains(DEFINES, ENABLE_OFFLINE_WEB_APPLICATIONS=1) {
    DEFINES += ENABLE_SQLITE=1
  } else {
    DEFINES += ENABLE_SQLITE=0
  }
}

!contains(DEFINES, ENABLE_DASHBOARD_SUPPORT=.): DEFINES += ENABLE_DASHBOARD_SUPPORT=0
!contains(DEFINES, ENABLE_FILTERS=.): DEFINES += ENABLE_FILTERS=0
!contains(DEFINES, ENABLE_XPATH=.): DEFINES += ENABLE_XPATH=1
!contains(DEFINES, ENABLE_XSLT=.): DEFINES += ENABLE_XSLT=0
#!contains(DEFINES, ENABLE_XBL=.): DEFINES += ENABLE_XBL=1
!contains(DEFINES, ENABLE_WML=.): DEFINES += ENABLE_WML=0
!contains(DEFINES, ENABLE_WORKERS=.): DEFINES += ENABLE_WORKERS=1
!contains(DEFINES, ENABLE_XHTMLMP=.): DEFINES += ENABLE_XHTMLMP=0

# SVG support
!contains(DEFINES, ENABLE_SVG=0) {
    !contains(DEFINES, ENABLE_SVG=.): DEFINES += ENABLE_SVG=1
    !contains(DEFINES, ENABLE_SVG_FONTS=.): DEFINES += ENABLE_SVG_FONTS=1
    !contains(DEFINES, ENABLE_SVG_FOREIGN_OBJECT=.): DEFINES += ENABLE_SVG_FOREIGN_OBJECT=1
    !contains(DEFINES, ENABLE_SVG_ANIMATION=.): DEFINES += ENABLE_SVG_ANIMATION=1
    !contains(DEFINES, ENABLE_SVG_AS_IMAGE=.): DEFINES += ENABLE_SVG_AS_IMAGE=1
    !contains(DEFINES, ENABLE_SVG_USE=.): DEFINES += ENABLE_SVG_USE=1
} else {
    DEFINES += ENABLE_SVG_FONTS=0 ENABLE_SVG_FOREIGN_OBJECT=0 ENABLE_SVG_ANIMATION=0 ENABLE_SVG_AS_IMAGE=0 ENABLE_SVG_USE=0
}

# HTML5 media support
!contains(DEFINES, ENABLE_VIDEO=.) {
    contains(QT_CONFIG, phonon):DEFINES += ENABLE_VIDEO=1
    else:DEFINES += ENABLE_VIDEO=0
}

# Nescape plugins support (NPAPI)
!contains(DEFINES, ENABLE_NETSCAPE_PLUGIN_API=.) {
    unix|win32-*:!embedded:!wince*:!symbian {
        DEFINES += ENABLE_NETSCAPE_PLUGIN_API=1
    } else {
        DEFINES += ENABLE_NETSCAPE_PLUGIN_API=0
    }
}

DEFINES += WTF_USE_JAVASCRIPTCORE_BINDINGS=1 WTF_CHANGES=1

# Used to compute defaults for the build-webkit script
CONFIG(compute_defaults) {
    message($$DEFINES)
    error("Done computing defaults")
}

# Ensure that we pick up WebCore's config.h over JavaScriptCore's
INCLUDEPATH = $$PWD $$INCLUDEPATH

include($$PWD/../JavaScriptCore/JavaScriptCore.pri)

RESOURCES += \
    $$PWD/../WebCore/inspector/front-end/WebKit.qrc \
    $$PWD/../WebCore/WebCore.qrc
INCLUDEPATH += \
    $$PWD/platform/qt \
    $$PWD/platform/network/qt \
    $$PWD/platform/graphics/filters \
    $$PWD/platform/graphics/transforms \
    $$PWD/platform/graphics/qt \
    $$PWD/page/qt \
    $$PWD/../WebKit/qt/WebCoreSupport \

# Make sure storage/ appears before JavaScriptCore/. Both provide LocalStorage.h
# but the header from the former include path is included across directories while
# LocalStorage.h is included only from files within the same directory
INCLUDEPATH = $$PWD/storage $$INCLUDEPATH

INCLUDEPATH +=  $$PWD/accessibility \
                $$PWD/ForwardingHeaders \
                $$PWD/platform \
                $$PWD/platform/animation \
                $$PWD/platform/network \
                $$PWD/platform/graphics \
                $$PWD/svg/animation \
                $$PWD/svg/graphics \
                $$PWD/svg/graphics/filters \
                $$PWD/platform/sql \
                $$PWD/platform/text \
                $$PWD/loader \
                $$PWD/loader/appcache \
                $$PWD/loader/archive \
                $$PWD/loader/icon \
                $$PWD/css \
                $$PWD/dom \
                $$PWD/dom/default \
                $$PWD/page \
                $$PWD/page/animation \
                $$PWD/editing \
                $$PWD/rendering \
                $$PWD/rendering/style \
                $$PWD/history \
                $$PWD/inspector \
                $$PWD/xml \
                $$PWD/html \
                $$PWD/wml \
                $$PWD/workers \
                $$PWD/bindings/js \
                $$PWD/svg \
                $$PWD/platform/image-decoders \
                $$PWD/plugins \
                $$PWD/bridge \
                $$PWD/bridge/c \
                $$PWD/bridge/qt
INCLUDEPATH *=  $$GENERATED_SOURCES_DIR

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

DASHBOARDSUPPORTCSSPROPERTIES = $$PWD/css/DashboardSupportCSSPropertyNames.in

SVGCSSPROPERTIES = $$PWD/css/SVGCSSPropertyNames.in

SVGCSSVALUES = $$PWD/css/SVGCSSValueKeywords.in

STYLESHEETS_EMBED = \
    $$PWD/css/html.css \
    $$PWD/css/quirks.css \
    $$PWD/css/svg.css \
    $$PWD/css/view-source.css \
    $$PWD/css/wml.css \
    $$PWD/css/mediaControls.css

DOMLUT_FILES += \
    bindings/js/JSDOMWindowBase.cpp \
    bindings/js/JSRGBColor.cpp \
    bindings/js/JSWorkerContextBase.cpp

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
    css/CSSVariablesDeclaration.idl \
    css/CSSVariablesRule.idl \
    css/MediaList.idl \
    css/Rect.idl \
    css/StyleSheet.idl \
    css/StyleSheetList.idl \
    css/WebKitCSSKeyframeRule.idl \
    css/WebKitCSSKeyframesRule.idl \
    css/WebKitCSSMatrix.idl \
    css/WebKitCSSTransformValue.idl \
    dom/Attr.idl \
    dom/CharacterData.idl \
    dom/ClientRect.idl \
    dom/ClientRectList.idl \
    dom/Clipboard.idl \
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
    dom/MouseEvent.idl \
    dom/MessageChannel.idl \
    dom/MessageEvent.idl \
    dom/MessagePort.idl \
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
    dom/WebKitAnimationEvent.idl \
    dom/WebKitTransitionEvent.idl \
    dom/WheelEvent.idl \
    html/CanvasGradient.idl \
    html/CanvasPattern.idl \
    html/CanvasRenderingContext2D.idl \
    html/DataGridColumn.idl \
    html/DataGridColumnList.idl \
    html/File.idl \
    html/FileList.idl \
    html/HTMLAudioElement.idl \
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
    html/HTMLDataGridElement.idl \
    html/HTMLDataGridCellElement.idl \
    html/HTMLDataGridColElement.idl \
    html/HTMLDataGridRowElement.idl \
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
    html/HTMLMediaElement.idl \
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
    html/HTMLSourceElement.idl \
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
    html/HTMLVideoElement.idl \
    html/ImageData.idl \
    html/MediaError.idl \
    html/TextMetrics.idl \
    html/VoidCallback.idl \
    inspector/InspectorController.idl \
    page/BarInfo.idl \
    page/Console.idl \
    page/Coordinates.idl \
    page/DOMSelection.idl \
    page/DOMWindow.idl \
    page/Geolocation.idl \
    page/Geoposition.idl \
    page/History.idl \
    page/Location.idl \
    page/Navigator.idl \
    page/PositionError.idl \
    page/Screen.idl \
    page/WebKitPoint.idl \
    plugins/Plugin.idl \
    plugins/MimeType.idl \
    plugins/PluginArray.idl \
    plugins/MimeTypeArray.idl \
    xml/DOMParser.idl \
    xml/XMLHttpRequest.idl \
    xml/XMLHttpRequestException.idl \
    xml/XMLHttpRequestProgressEvent.idl \
    xml/XMLHttpRequestUpload.idl \
    xml/XMLSerializer.idl


SOURCES += \
    accessibility/AccessibilityImageMapLink.cpp \
    accessibility/AccessibilityObject.cpp \    
    accessibility/AccessibilityList.cpp \    
    accessibility/AccessibilityListBox.cpp \    
    accessibility/AccessibilityListBoxOption.cpp \    
    accessibility/AccessibilityRenderObject.cpp \    
    accessibility/AccessibilityARIAGrid.cpp \    
    accessibility/AccessibilityARIAGridCell.cpp \    
    accessibility/AccessibilityARIAGridRow.cpp \    
    accessibility/AccessibilityTable.cpp \    
    accessibility/AccessibilityTableCell.cpp \    
    accessibility/AccessibilityTableColumn.cpp \    
    accessibility/AccessibilityTableHeaderContainer.cpp \    
    accessibility/AccessibilityTableRow.cpp \    
    accessibility/AXObjectCache.cpp \
    bindings/js/GCController.cpp \
    bindings/js/JSAttrCustom.cpp \
    bindings/js/JSCDATASectionCustom.cpp \
    bindings/js/JSCanvasRenderingContext2DCustom.cpp \
    bindings/js/JSClipboardCustom.cpp \
    bindings/js/JSConsoleCustom.cpp \
    bindings/js/JSCSSRuleCustom.cpp \
    bindings/js/JSCSSStyleDeclarationCustom.cpp \
    bindings/js/JSCSSValueCustom.cpp \
    bindings/js/JSCoordinatesCustom.cpp \
    bindings/js/JSCustomPositionCallback.cpp \
    bindings/js/JSCustomPositionErrorCallback.cpp \
    bindings/js/JSCustomVoidCallback.cpp \
    bindings/js/JSCustomXPathNSResolver.cpp \
    bindings/js/JSDataGridColumnListCustom.cpp \
    bindings/js/JSDataGridDataSource.cpp \
    bindings/js/JSDocumentCustom.cpp \
    bindings/js/JSDocumentFragmentCustom.cpp \
    bindings/js/JSDOMGlobalObject.cpp \
    bindings/js/JSDOMWindowBase.cpp \
    bindings/js/JSDOMWindowCustom.cpp \
    bindings/js/JSDOMWindowShell.cpp \
    bindings/js/JSElementCustom.cpp \
    bindings/js/JSEventCustom.cpp \
    bindings/js/JSEventTarget.cpp \
    bindings/js/JSGeolocationCustom.cpp \
    bindings/js/JSHTMLAllCollection.cpp \
    bindings/js/JSHistoryCustom.cpp \
    bindings/js/JSHTMLAppletElementCustom.cpp \
    bindings/js/JSHTMLCollectionCustom.cpp \
    bindings/js/JSHTMLDataGridElementCustom.cpp \
    bindings/js/JSHTMLDocumentCustom.cpp \
    bindings/js/JSHTMLElementCustom.cpp \
    bindings/js/JSHTMLEmbedElementCustom.cpp \
    bindings/js/JSHTMLFormElementCustom.cpp \
    bindings/js/JSHTMLFrameElementCustom.cpp \
    bindings/js/JSHTMLFrameSetElementCustom.cpp \
    bindings/js/JSHTMLIFrameElementCustom.cpp \
    bindings/js/JSHTMLInputElementCustom.cpp \
    bindings/js/JSHTMLObjectElementCustom.cpp \
    bindings/js/JSHTMLOptionsCollectionCustom.cpp \
    bindings/js/JSHTMLSelectElementCustom.cpp \
    bindings/js/JSImageConstructor.cpp \
    bindings/js/JSImageDataCustom.cpp \
    bindings/js/JSInspectedObjectWrapper.cpp \
    bindings/js/JSInspectorCallbackWrapper.cpp \
    bindings/js/JSInspectorControllerCustom.cpp \
    bindings/js/JSLocationCustom.cpp \
    bindings/js/JSNamedNodeMapCustom.cpp \
    bindings/js/JSNamedNodesCollection.cpp  \
    bindings/js/JSNavigatorCustom.cpp  \
    bindings/js/JSNodeCustom.cpp \
    bindings/js/JSNodeFilterCondition.cpp \
    bindings/js/JSNodeFilterCustom.cpp \
    bindings/js/JSNodeIteratorCustom.cpp \
    bindings/js/JSNodeListCustom.cpp \
    bindings/js/JSOptionConstructor.cpp \
    bindings/js/JSQuarantinedObjectWrapper.cpp \
    bindings/js/JSRGBColor.cpp \
    bindings/js/JSStyleSheetCustom.cpp \
    bindings/js/JSStyleSheetListCustom.cpp \
    bindings/js/JSTextCustom.cpp \
    bindings/js/JSTreeWalkerCustom.cpp \
    bindings/js/JSWebKitCSSMatrixConstructor.cpp \
    bindings/js/JSWebKitPointConstructor.cpp \
    bindings/js/JSXMLHttpRequestConstructor.cpp \
    bindings/js/JSXMLHttpRequestCustom.cpp \
    bindings/js/JSXMLHttpRequestUploadCustom.cpp \
    bindings/js/JSPluginCustom.cpp \
    bindings/js/JSPluginArrayCustom.cpp \
    bindings/js/JSMessageChannelConstructor.cpp \
    bindings/js/JSMessageChannelCustom.cpp \
    bindings/js/JSMessagePortCustom.cpp \
    bindings/js/JSMimeTypeArrayCustom.cpp \
    bindings/js/JSDOMBinding.cpp \
    bindings/js/JSEventListener.cpp \
    bindings/js/JSLazyEventListener.cpp \
    bindings/js/JSPluginElementFunctions.cpp \
    bindings/js/ScriptCachedFrameData.cpp \
    bindings/js/ScriptCallFrame.cpp \
    bindings/js/ScriptCallStack.cpp \
    bindings/js/ScriptController.cpp \
    bindings/js/ScriptEventListener.cpp \
    bindings/js/ScriptFunctionCall.cpp \
    bindings/js/ScriptObject.cpp \
    bindings/js/ScriptObjectQuarantine.cpp \
    bindings/js/ScriptState.cpp \
    bindings/js/ScriptValue.cpp \
    bindings/js/ScheduledAction.cpp \
    bridge/IdentifierRep.cpp \
    bridge/NP_jsobject.cpp \
    bridge/npruntime.cpp \
    bridge/runtime_array.cpp \
    bridge/runtime.cpp \
    bridge/runtime_method.cpp \
    bridge/runtime_object.cpp \
    bridge/runtime_root.cpp \
    bridge/c/c_class.cpp \
    bridge/c/c_instance.cpp \
    bridge/c/c_runtime.cpp \
    bridge/c/c_utility.cpp \
    css/CSSBorderImageValue.cpp \
    css/CSSCanvasValue.cpp \
    css/CSSCharsetRule.cpp \
    css/CSSComputedStyleDeclaration.cpp \
    css/CSSCursorImageValue.cpp \
    css/CSSFontFace.cpp \
    css/CSSFontFaceRule.cpp \
    css/CSSFontFaceSrcValue.cpp \
    css/CSSFontSelector.cpp \
    css/CSSFontFaceSource.cpp \
    css/CSSFunctionValue.cpp \
    css/CSSGradientValue.cpp \
    css/CSSHelper.cpp \
    css/CSSImageValue.cpp \
    css/CSSImageGeneratorValue.cpp \
    css/CSSImportRule.cpp \
    css/CSSInheritedValue.cpp \
    css/CSSInitialValue.cpp \
    css/CSSMediaRule.cpp \
    css/CSSMutableStyleDeclaration.cpp \
    css/CSSPageRule.cpp \
    css/CSSParser.cpp \
    css/CSSParserValues.cpp \
    css/CSSPrimitiveValue.cpp \
    css/CSSProperty.cpp \
    css/CSSPropertyLonghand.cpp \
    css/CSSReflectValue.cpp \
    css/CSSRule.cpp \
    css/CSSRuleList.cpp \
    css/CSSSelector.cpp \
    css/CSSSelectorList.cpp \
    css/CSSSegmentedFontFace.cpp \
    css/CSSStyleDeclaration.cpp \
    css/CSSStyleRule.cpp \
    css/CSSStyleSelector.cpp \
    css/CSSStyleSheet.cpp \
    css/CSSTimingFunctionValue.cpp \
    css/CSSUnicodeRangeValue.cpp \
    css/CSSValueList.cpp \
    css/CSSVariableDependentValue.cpp \
    css/CSSVariablesDeclaration.cpp \
    css/CSSVariablesRule.cpp \
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
    css/WebKitCSSKeyframeRule.cpp \
    css/WebKitCSSKeyframesRule.cpp \
    css/WebKitCSSMatrix.cpp \
    css/WebKitCSSTransformValue.cpp \
    dom/ActiveDOMObject.cpp \
    dom/Attr.cpp \
    dom/Attribute.cpp \
    dom/BeforeTextInsertedEvent.cpp \
    dom/BeforeUnloadEvent.cpp \
    dom/CDATASection.cpp \
    dom/CharacterData.cpp \
    dom/CheckedRadioButtons.cpp \
    dom/ChildNodeList.cpp \
    dom/ClassNames.cpp \
    dom/ClassNodeList.cpp \
    dom/ClientRect.cpp \
    dom/ClientRectList.cpp \
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
    dom/ExceptionBase.cpp \
    dom/ExceptionCode.cpp \
    dom/InputElement.cpp \
    dom/KeyboardEvent.cpp \
    dom/MappedAttribute.cpp \
    dom/MessageChannel.cpp \
    dom/MessageEvent.cpp \
    dom/MessagePort.cpp \
    dom/MessagePortChannel.cpp \
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
    dom/OptionGroupElement.cpp \
    dom/OptionElement.cpp \
    dom/OverflowEvent.cpp \
    dom/Position.cpp \
    dom/PositionIterator.cpp \
    dom/ProcessingInstruction.cpp \
    dom/ProgressEvent.cpp \
    dom/QualifiedName.cpp \
    dom/Range.cpp \
    dom/RegisteredEventListener.cpp \
    dom/ScriptElement.cpp \
    dom/ScriptExecutionContext.cpp \
    dom/SelectElement.cpp \
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
    dom/WebKitAnimationEvent.cpp \
    dom/WebKitTransitionEvent.cpp \
    dom/WheelEvent.cpp \
    dom/XMLTokenizer.cpp \
    dom/XMLTokenizerQt.cpp \
    dom/XMLTokenizerScope.cpp \
    dom/default/PlatformMessagePortChannel.cpp \
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
    editing/RemoveNodeCommand.cpp \
    editing/RemoveNodePreservingChildrenCommand.cpp \
    editing/ReplaceNodeWithSpanCommand.cpp \
    editing/ReplaceSelectionCommand.cpp \
    editing/SelectionController.cpp \
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
    editing/VisibleSelection.cpp \
    editing/visible_units.cpp \
    editing/WrapContentsInDummySpanCommand.cpp \
    history/BackForwardList.cpp \
    history/CachedFrame.cpp \
    history/CachedPage.cpp \
    history/HistoryItem.cpp \
    history/qt/HistoryItemQt.cpp \
    history/PageCache.cpp \
    html/CanvasGradient.cpp \
    html/CanvasPattern.cpp \
    html/CanvasPixelArray.cpp \
    html/CanvasRenderingContext2D.cpp \
    html/CanvasStyle.cpp \
    html/CollectionCache.cpp \
    html/DataGridColumn.cpp \
    html/DataGridColumnList.cpp \
    html/DOMDataGridDataSource.cpp \
    html/File.cpp \
    html/FileList.cpp \
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
    html/HTMLDataGridElement.cpp \
    html/HTMLDataGridCellElement.cpp \
    html/HTMLDataGridColElement.cpp \
    html/HTMLDataGridRowElement.cpp \
    html/HTMLDirectoryElement.cpp \
    html/HTMLDivElement.cpp \
    html/HTMLDListElement.cpp \
    html/HTMLDocument.cpp \
    html/HTMLElement.cpp \
    html/HTMLEmbedElement.cpp \
    html/HTMLFieldSetElement.cpp \
    html/HTMLFontElement.cpp \
    html/HTMLFormCollection.cpp \
    html/HTMLFormElement.cpp \
    html/HTMLFrameElementBase.cpp \
    html/HTMLFrameElement.cpp \
    html/HTMLFrameOwnerElement.cpp \
    html/HTMLFrameSetElement.cpp \
    html/HTMLFormControlElement.cpp \
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
    html/HTMLPlugInImageElement.cpp \
    html/HTMLPreElement.cpp \
    html/HTMLQuoteElement.cpp \
    html/HTMLScriptElement.cpp \
    html/HTMLNoScriptElement.cpp \
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
    html/HTMLTitleElement.cpp \
    html/HTMLTokenizer.cpp \
    html/HTMLUListElement.cpp \
    html/HTMLViewSourceDocument.cpp \
    html/ImageData.cpp \
    html/PreloadScanner.cpp \
    inspector/ConsoleMessage.cpp \
    inspector/InspectorDatabaseResource.cpp \
    inspector/InspectorDOMStorageResource.cpp \
    inspector/InspectorController.cpp \
    inspector/InspectorFrontend.cpp \
    inspector/InspectorResource.cpp \
    inspector/InspectorJSONObject.cpp \
    loader/archive/ArchiveFactory.cpp \
    loader/archive/ArchiveResource.cpp \
    loader/archive/ArchiveResourceCollection.cpp \
    loader/UserStyleSheetLoader.cpp \
    loader/Cache.cpp \
    loader/CachedCSSStyleSheet.cpp \
    loader/CachedFont.cpp \
    loader/CachedImage.cpp \
    loader/CachedResourceClientWalker.cpp \
    loader/CachedResourceHandle.cpp \
    loader/CachedResource.cpp \
    loader/CachedScript.cpp \
    loader/CachedXSLStyleSheet.cpp \
    loader/CrossOriginAccessControl.cpp \
    loader/CrossOriginPreflightResultCache.cpp \
    loader/DocLoader.cpp \
    loader/DocumentLoader.cpp \
    loader/DocumentThreadableLoader.cpp \
    loader/FormState.cpp \
    loader/FrameLoader.cpp \
    loader/FTPDirectoryDocument.cpp \
    loader/FTPDirectoryParser.cpp \
    loader/icon/IconLoader.cpp \
    loader/ImageDocument.cpp \
    loader/ImageLoader.cpp \
    loader/loader.cpp \
    loader/MainResourceLoader.cpp \
    loader/MediaDocument.cpp \
    loader/NavigationAction.cpp \
    loader/NetscapePlugInStreamLoader.cpp \
    loader/PluginDocument.cpp \
    loader/ProgressTracker.cpp \
    loader/Request.cpp \
    loader/ResourceLoader.cpp \
    loader/SubresourceLoader.cpp \
    loader/TextDocument.cpp \
    loader/TextResourceDecoder.cpp \
    loader/ThreadableLoader.cpp \
    page/animation/AnimationBase.cpp \
    page/animation/AnimationController.cpp \
    page/animation/CompositeAnimation.cpp \
    page/animation/ImplicitAnimation.cpp \
    page/animation/KeyframeAnimation.cpp \
    page/BarInfo.cpp \
    page/Chrome.cpp \
    page/Console.cpp \
    page/ContextMenuController.cpp \
    page/Coordinates.cpp \
    page/DOMSelection.cpp \
    page/DOMTimer.cpp \
    page/DOMWindow.cpp \
    page/Navigator.cpp \
    page/NavigatorBase.cpp \
    page/DragController.cpp \
    page/EventHandler.cpp \
    page/FocusController.cpp \
    page/Frame.cpp \
    page/FrameTree.cpp \
    page/FrameView.cpp \
    page/Geolocation.cpp \
    page/Geoposition.cpp \
    page/History.cpp \
    page/Location.cpp \
    page/MouseEventWithHitTestResults.cpp \
    page/Page.cpp \
    page/PageGroup.cpp \
    page/PageGroupLoadDeferrer.cpp \
    page/PrintContext.cpp \
    page/SecurityOrigin.cpp \
    page/Screen.cpp \
    page/Settings.cpp \
    page/WindowFeatures.cpp \
    page/XSSAuditor.cpp \
    plugins/PluginData.cpp \
    plugins/PluginArray.cpp \
    plugins/Plugin.cpp \
    plugins/PluginMainThreadScheduler.cpp \
    plugins/MimeType.cpp \
    plugins/MimeTypeArray.cpp \
    platform/animation/Animation.cpp \
    platform/animation/AnimationList.cpp \
    platform/Arena.cpp \
    platform/text/AtomicString.cpp \
    platform/text/Base64.cpp \
    platform/text/BidiContext.cpp \
    platform/ContentType.cpp \
    platform/ContextMenu.cpp \
    platform/CrossThreadCopier.cpp \
    platform/text/CString.cpp \
    platform/DeprecatedPtrListImpl.cpp \
    platform/DragData.cpp \
    platform/DragImage.cpp \
    platform/FileChooser.cpp \
    platform/GeolocationService.cpp \
    platform/graphics/FontDescription.cpp \
    platform/graphics/FontFamily.cpp \
    platform/graphics/BitmapImage.cpp \
    platform/graphics/Color.cpp \
    platform/graphics/FloatPoint3D.cpp \
    platform/graphics/FloatPoint.cpp \
    platform/graphics/FloatQuad.cpp \
    platform/graphics/FloatRect.cpp \
    platform/graphics/FloatSize.cpp \
    platform/graphics/FontData.cpp \
    platform/graphics/Font.cpp \
    platform/graphics/GeneratedImage.cpp \
    platform/graphics/Gradient.cpp \
    platform/graphics/GraphicsContext.cpp \
    platform/graphics/GraphicsTypes.cpp \
    platform/graphics/Image.cpp \
    platform/graphics/IntRect.cpp \
    platform/graphics/Path.cpp \
    platform/graphics/PathTraversalState.cpp \
    platform/graphics/Pattern.cpp \
    platform/graphics/Pen.cpp \
    platform/graphics/SegmentedFontData.cpp \
    platform/graphics/SimpleFontData.cpp \
    platform/graphics/transforms/TransformationMatrix.cpp \
    platform/graphics/transforms/MatrixTransformOperation.cpp \
    platform/graphics/transforms/Matrix3DTransformOperation.cpp \
    platform/graphics/transforms/PerspectiveTransformOperation.cpp \
    platform/graphics/transforms/RotateTransformOperation.cpp \
    platform/graphics/transforms/ScaleTransformOperation.cpp \
    platform/graphics/transforms/SkewTransformOperation.cpp \
    platform/graphics/transforms/TransformOperations.cpp \
    platform/graphics/transforms/TranslateTransformOperation.cpp \
    platform/KURL.cpp \
    platform/Length.cpp \
    platform/LinkHash.cpp \
    platform/Logging.cpp \
    platform/MIMETypeRegistry.cpp \
    platform/network/AuthenticationChallengeBase.cpp \
    platform/network/Credential.cpp \
    platform/network/FormData.cpp \
    platform/network/FormDataBuilder.cpp \
    platform/network/HTTPHeaderMap.cpp \
    platform/network/HTTPParsers.cpp \
    platform/network/NetworkStateNotifier.cpp \
    platform/network/ProtectionSpace.cpp \
    platform/network/ResourceErrorBase.cpp \
    platform/network/ResourceHandle.cpp \
    platform/network/ResourceRequestBase.cpp \
    platform/network/ResourceResponseBase.cpp \
    platform/text/RegularExpression.cpp \
    platform/Scrollbar.cpp \
    platform/ScrollbarThemeComposite.cpp \
    platform/ScrollView.cpp \
#    platform/SearchPopupMenu.cpp \
    platform/text/SegmentedString.cpp \
    platform/SharedBuffer.cpp \
    platform/text/String.cpp \
    platform/text/StringBuilder.cpp \
    platform/text/StringImpl.cpp \
    platform/text/TextCodec.cpp \
    platform/text/TextCodecLatin1.cpp \
    platform/text/TextCodecUserDefined.cpp \
    platform/text/TextCodecUTF16.cpp \
    platform/text/TextEncoding.cpp \
    platform/text/TextEncodingDetectorNone.cpp \
    platform/text/TextEncodingRegistry.cpp \
    platform/text/TextStream.cpp \
    platform/ThreadGlobalData.cpp \
    platform/ThreadTimers.cpp \
    platform/Timer.cpp \
    platform/text/UnicodeRange.cpp \
    platform/Widget.cpp \
    plugins/PluginDatabase.cpp \
    plugins/PluginInfoStore.cpp \
    plugins/PluginPackage.cpp \
    plugins/PluginStream.cpp \
    plugins/PluginView.cpp \
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
    rendering/RenderApplet.cpp \
    rendering/RenderArena.cpp \
    rendering/RenderBlock.cpp \
    rendering/RenderBox.cpp \
    rendering/RenderBoxModelObject.cpp \
    rendering/RenderBR.cpp \
    rendering/RenderButton.cpp \
    rendering/RenderCounter.cpp \
    rendering/RenderDataGrid.cpp \
    rendering/RenderFieldset.cpp \
    rendering/RenderFileUploadControl.cpp \
    rendering/RenderFlexibleBox.cpp \
    rendering/RenderFrame.cpp \
    rendering/RenderFrameSet.cpp \
    rendering/RenderHTMLCanvas.cpp \
    rendering/RenderImage.cpp \
    rendering/RenderImageGeneratedContent.cpp \
    rendering/RenderInline.cpp \
    rendering/RenderLayer.cpp \
    rendering/RenderLineBoxList.cpp \
    rendering/RenderListBox.cpp \
    rendering/RenderListItem.cpp \
    rendering/RenderListMarker.cpp \
    rendering/RenderMarquee.cpp \
    rendering/RenderMenuList.cpp \
    rendering/RenderObject.cpp \
    rendering/RenderObjectChildList.cpp \
    rendering/RenderPart.cpp \
    rendering/RenderPartObject.cpp \
    rendering/RenderReplaced.cpp \
    rendering/RenderReplica.cpp \
    rendering/RenderScrollbar.cpp \
    rendering/RenderScrollbarPart.cpp \
    rendering/RenderScrollbarTheme.cpp \
    rendering/RenderSlider.cpp \
    rendering/RenderTable.cpp \
    rendering/RenderTableCell.cpp \
    rendering/RenderTableCol.cpp \
    rendering/RenderTableRow.cpp \
    rendering/RenderTableSection.cpp \
    rendering/RenderText.cpp \
    rendering/RenderTextControl.cpp \
    rendering/RenderTextControlMultiLine.cpp \
    rendering/RenderTextControlSingleLine.cpp \
    rendering/RenderTextFragment.cpp \
    rendering/RenderTheme.cpp \
    rendering/RenderTreeAsText.cpp \
    rendering/RenderView.cpp \
    rendering/RenderWidget.cpp \
    rendering/RenderWordBreak.cpp \
    rendering/RootInlineBox.cpp \
    rendering/SVGRenderTreeAsText.cpp \
    rendering/ScrollBehavior.cpp \
    rendering/TextControlInnerElements.cpp \
    rendering/TransformState.cpp \
    rendering/style/BindingURI.cpp \
    rendering/style/ContentData.cpp \
    rendering/style/CounterDirectives.cpp \
    rendering/style/FillLayer.cpp \
    rendering/style/KeyframeList.cpp \
    rendering/style/NinePieceImage.cpp \
    rendering/style/RenderStyle.cpp \
    rendering/style/ShadowData.cpp \
    rendering/style/StyleBackgroundData.cpp \
    rendering/style/StyleBoxData.cpp \
    rendering/style/StyleCachedImage.cpp \
    rendering/style/StyleFlexibleBoxData.cpp \
    rendering/style/StyleGeneratedImage.cpp \
    rendering/style/StyleInheritedData.cpp \
    rendering/style/StyleMarqueeData.cpp \
    rendering/style/StyleMultiColData.cpp \
    rendering/style/StyleRareInheritedData.cpp \
    rendering/style/StyleRareNonInheritedData.cpp \
    rendering/style/StyleSurroundData.cpp \
    rendering/style/StyleTransformData.cpp \
    rendering/style/StyleVisualData.cpp \
    xml/DOMParser.cpp \
    xml/XMLHttpRequest.cpp \
    xml/XMLHttpRequestUpload.cpp \
    xml/XMLSerializer.cpp 

HEADERS += \
    $$PWD/platform/graphics/qt/StillImageQt.h \
    $$PWD/platform/qt/QWebPopup.h \
    $$PWD/../WebKit/qt/WebCoreSupport/FrameLoaderClientQt.h \
    $$PWD/platform/network/qt/QNetworkReplyHandler.h \
    $$PWD/rendering/style/CursorData.h \
    $$PWD/rendering/style/CursorList.h \
    $$PWD/rendering/style/StyleInheritedData.h \
    $$PWD/rendering/style/StyleRareInheritedData.h \
    $$PWD/rendering/style/StyleRareNonInheritedData.h \
    $$PWD/rendering/style/StyleReflection.h


SOURCES += \
    accessibility/qt/AccessibilityObjectQt.cpp \
    bindings/js/ScriptControllerQt.cpp \
    bridge/qt/qt_class.cpp \
    bridge/qt/qt_instance.cpp \
    bridge/qt/qt_runtime.cpp \
    page/qt/DragControllerQt.cpp \
    page/qt/EventHandlerQt.cpp \
    page/qt/FrameQt.cpp \
    platform/graphics/qt/TransformationMatrixQt.cpp \
    platform/graphics/qt/ColorQt.cpp \
    platform/graphics/qt/FontQt.cpp \
    platform/graphics/qt/FontQt43.cpp \
    platform/graphics/qt/FontPlatformDataQt.cpp \
    platform/graphics/qt/FloatPointQt.cpp \
    platform/graphics/qt/FloatRectQt.cpp \
    platform/graphics/qt/GradientQt.cpp \
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
    platform/graphics/qt/PatternQt.cpp \
    platform/graphics/qt/StillImageQt.cpp \
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
    platform/qt/EventLoopQt.cpp \
    platform/qt/FileChooserQt.cpp \
    platform/qt/FileSystemQt.cpp \
    platform/qt/SharedBufferQt.cpp \
    platform/graphics/qt/FontCacheQt.cpp \
    platform/graphics/qt/FontCustomPlatformData.cpp \
    platform/graphics/qt/FontFallbackListQt.cpp \
    platform/graphics/qt/GlyphPageTreeNodeQt.cpp \
    platform/graphics/qt/SimpleFontDataQt.cpp \
    platform/qt/KURLQt.cpp \
    platform/qt/Localizations.cpp \
    platform/qt/MIMETypeRegistryQt.cpp \
    platform/qt/PasteboardQt.cpp \
    platform/qt/PlatformKeyboardEventQt.cpp \
    platform/qt/PlatformMouseEventQt.cpp \
    platform/qt/PlatformScreenQt.cpp \
    platform/qt/PopupMenuQt.cpp \
    platform/qt/QWebPopup.cpp \
    platform/qt/RenderThemeQt.cpp \
    platform/qt/ScrollbarQt.cpp \
    platform/qt/ScrollbarThemeQt.cpp \
    platform/qt/ScrollViewQt.cpp \
    platform/qt/SearchPopupMenuQt.cpp \
    platform/qt/SharedTimerQt.cpp \
    platform/qt/SoundQt.cpp \
    platform/qt/LoggingQt.cpp \
    platform/text/qt/StringQt.cpp \
    platform/qt/TemporaryLinkStubs.cpp \
    platform/text/qt/TextBoundaries.cpp \
    platform/text/qt/TextBreakIteratorQt.cpp \
    platform/text/qt/TextCodecQt.cpp \
    platform/qt/WheelEventQt.cpp \
    platform/qt/WidgetQt.cpp \
    plugins/qt/PluginDataQt.cpp \
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
    ../WebKit/qt/Api/qwebelement.cpp \
    ../WebKit/qt/Api/qwebhistory.cpp \
    ../WebKit/qt/Api/qwebsettings.cpp \
    ../WebKit/qt/Api/qwebhistoryinterface.cpp \
    ../WebKit/qt/Api/qwebpluginfactory.cpp \
    ../WebKit/qt/Api/qwebsecurityorigin.cpp \
    ../WebKit/qt/Api/qwebdatabase.cpp


    win32-*|wince*: SOURCES += platform/win/SystemTimeWin.cpp

    mac {
        SOURCES += \
            platform/text/cf/StringCF.cpp \
            platform/text/cf/StringImplCF.cpp
    }

    win32-* {
        LIBS += -lgdi32
        LIBS += -luser32
        LIBS += -lwinmm
    }
    wince*: LIBS += -lmmtimer

    # Files belonging to the Qt 4.3 build
    lessThan(QT_MINOR_VERSION, 4) {
        HEADERS += \
            $$PWD/../WebKit/qt/Api/qwebnetworkinterface.h \
            $$PWD/../WebKit/qt/Api/qwebnetworkinterface_p.h \
            $$PWD/../WebKit/qt/Api/qcookiejar.h

        SOURCES += \
            ../WebKit/qt/Api/qwebnetworkinterface.cpp \
            ../WebKit/qt/Api/qcookiejar.cpp

        DEFINES += QT_BEGIN_NAMESPACE="" QT_END_NAMESPACE=""
     }

contains(DEFINES, ENABLE_NETSCAPE_PLUGIN_API=1) {

    SOURCES += plugins/npapi.cpp

    unix {
        DEFINES += ENABLE_PLUGIN_PACKAGE_SIMPLE_HASH=1

        mac {
            SOURCES += \
                plugins/mac/PluginPackageMac.cpp \
                plugins/mac/PluginViewMac.cpp
            OBJECTIVE_SOURCES += \
                platform/text/mac/StringImplMac.mm \
                platform/mac/WebCoreNSStringExtras.mm
            INCLUDEPATH += platform/mac
            # Note: XP_MACOSX is defined in npapi.h
        } else {
            !embedded: CONFIG += x11
            SOURCES += \
                plugins/qt/PluginContainerQt.cpp \
                plugins/qt/PluginPackageQt.cpp \
                plugins/qt/PluginViewQt.cpp
            HEADERS += \
                plugins/qt/PluginContainerQt.h
            DEFINES += XP_UNIX
        }
    }

    win32-* {
        INCLUDEPATH += $$PWD/plugins/win

        SOURCES += page/win/PageWin.cpp \
                   plugins/win/PluginDatabaseWin.cpp \
                   plugins/win/PluginPackageWin.cpp \
                   plugins/win/PluginMessageThrottlerWin.cpp \
                   plugins/win/PluginViewWin.cpp

        LIBS += \
            -ladvapi32 \
            -lgdi32 \
            -lshell32 \
            -lshlwapi \
            -luser32 \
            -lversion
    }

} else {
    SOURCES += \
        plugins/PluginPackageNone.cpp \
        plugins/PluginViewNone.cpp
}

contains(DEFINES, ENABLE_CHANNEL_MESSAGING=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_CHANNEL_MESSAGING=1
}

contains(DEFINES, ENABLE_DASHBOARD_SUPPORT=0) {
    DASHBOARDSUPPORTCSSPROPERTIES -= $$PWD/css/DashboardSupportCSSPropertyNames.in
}

contains(DEFINES, ENABLE_SQLITE=1) {
    # somewhat copied from src/plugins/sqldrivers/sqlite/sqlite.pro
    CONFIG(QTDIR_build):system-sqlite {
        LIBS *= $$QT_LFLAGS_SQLITE
        QMAKE_CXXFLAGS *= $$QT_CFLAGS_SQLITE
    } else {
        exists( $${SQLITE3SRCDIR}/sqlite3.c )  {
            # we have source - use it
            CONFIG(release, debug|release):DEFINES *= NDEBUG
            DEFINES += SQLITE_CORE SQLITE_OMIT_LOAD_EXTENSION SQLITE_OMIT_COMPLETE 
            contains(DEFINES, ENABLE_SINGLE_THREADED=1) {
                DEFINES+=SQLITE_THREADSAFE=0
            }
            INCLUDEPATH += $${SQLITE3SRCDIR}
            SOURCES += $${SQLITE3SRCDIR}/sqlite3.c
        } else {
            # fall back to platform library
            INCLUDEPATH += $$[QT_INSTALL_PREFIX]/src/3rdparty/sqlite/
            LIBS += -lsqlite3
        }
    }

    SOURCES += \
        platform/sql/SQLiteAuthorizer.cpp \
        platform/sql/SQLiteDatabase.cpp \
        platform/sql/SQLiteFileSystem.cpp \
        platform/sql/SQLiteStatement.cpp \
        platform/sql/SQLiteTransaction.cpp \
        platform/sql/SQLValue.cpp \
        storage/Database.cpp \
        storage/DatabaseAuthorizer.cpp
}


contains(DEFINES, ENABLE_DATABASE=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_DATABASE=1

    SOURCES += \
        storage/ChangeVersionWrapper.cpp \
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

contains(DEFINES, ENABLE_DOM_STORAGE=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_DOM_STORAGE=1

    HEADERS += \
        storage/LocalStorageTask.h \
        storage/LocalStorageThread.h \
        storage/Storage.h \
        storage/StorageArea.h \
        storage/StorageAreaImpl.h \
        storage/StorageAreaSync.h \
        storage/StorageEvent.h \
        storage/StorageMap.h \
        storage/StorageNamespace.h \
        storage/StorageNamespaceImpl.h \
        storage/StorageSyncManager.h

    SOURCES += \
        bindings/js/JSStorageCustom.cpp \
        storage/LocalStorageTask.cpp \
        storage/LocalStorageThread.cpp \
        storage/Storage.cpp \
        storage/StorageArea.cpp \
        storage/StorageAreaImpl.cpp \
        storage/StorageAreaSync.cpp \
        storage/StorageEvent.cpp \
        storage/StorageMap.cpp \
        storage/StorageNamespace.cpp \
        storage/StorageNamespaceImpl.cpp \
        storage/StorageSyncManager.cpp

    IDL_BINDINGS += \
        storage/Storage.idl \
        storage/StorageEvent.idl
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

contains(DEFINES, ENABLE_WORKERS=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_WORKERS=1

    IDL_BINDINGS += \
        page/WorkerNavigator.idl \
        workers/Worker.idl \
        workers/WorkerContext.idl \
        workers/WorkerLocation.idl

    SOURCES += \
        bindings/js/JSWorkerConstructor.cpp \
        bindings/js/JSWorkerContextBase.cpp \
        bindings/js/JSWorkerContextCustom.cpp \
        bindings/js/JSWorkerCustom.cpp \
        bindings/js/WorkerScriptController.cpp \
        loader/WorkerThreadableLoader.cpp \
        page/WorkerNavigator.cpp \
        workers/Worker.cpp \
        workers/WorkerContext.cpp \
        workers/WorkerLocation.cpp \
        workers/WorkerMessagingProxy.cpp \
        workers/WorkerRunLoop.cpp \
        workers/WorkerThread.cpp \
        workers/WorkerScriptLoader.cpp
}

contains(DEFINES, ENABLE_VIDEO=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_VIDEO=1

    IDL_BINDINGS += \
        html/TimeRanges.idl

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

        HEADERS += \
            platform/graphics/qt/MediaPlayerPrivatePhonon.h

        SOURCES += \
            platform/graphics/qt/MediaPlayerPrivatePhonon.cpp

        # Add phonon manually to prevent it from coming first in
        # the include paths, as Phonon's path.h conflicts with
        # WebCore's Path.h on case-insensitive filesystems.
        qtAddLibrary(phonon)
        INCLUDEPATH -= $$QMAKE_INCDIR_QT/phonon
        INCLUDEPATH += $$QMAKE_INCDIR_QT/phonon
        mac {
            INCLUDEPATH -= $$QMAKE_LIBDIR_QT/phonon.framework/Headers
            INCLUDEPATH += $$QMAKE_LIBDIR_QT/phonon.framework/Headers
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

    SOURCES += \
        xml/NativeXPathNSResolver.cpp \
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
        xml/XPathVariableReference.cpp
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

    IDL_BINDINGS += \
        xml/XSLTProcessor.idl

    SOURCES += \
        bindings/js/JSXSLTProcessorConstructor.cpp \
        bindings/js/JSXSLTProcessorCustom.cpp \
        xml/XSLImportRule.cpp \
        xml/XSLStyleSheet.cpp \
        xml/XSLTExtensions.cpp \
        xml/XSLTProcessor.cpp \
        xml/XSLTUnicodeSort.cpp
}

contains(DEFINES, ENABLE_XBL=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_XBL=1
}

contains(DEFINES, ENABLE_FILTERS=1) {
    SOURCES += \
        platform/graphics/filters/FEBlend.cpp \
        platform/graphics/filters/FEColorMatrix.cpp \
        platform/graphics/filters/FEComponentTransfer.cpp \
        platform/graphics/filters/FEComposite.cpp \
        platform/graphics/filters/FilterEffect.cpp \
        platform/graphics/filters/SourceAlpha.cpp \
        platform/graphics/filters/SourceGraphic.cpp

    FEATURE_DEFINES_JAVASCRIPT += ENABLE_FILTERS=1
}

contains(DEFINES, ENABLE_WML=1) {
    SOURCES += \
        wml/WMLAElement.cpp \
        wml/WMLAccessElement.cpp \
        wml/WMLAnchorElement.cpp \
        wml/WMLBRElement.cpp \
        wml/WMLCardElement.cpp \
        wml/WMLDoElement.cpp \
        wml/WMLDocument.cpp \
        wml/WMLElement.cpp \
        wml/WMLErrorHandling.cpp \
        wml/WMLEventHandlingElement.cpp \
        wml/WMLFormControlElement.cpp \
        wml/WMLFieldSetElement.cpp \
        wml/WMLGoElement.cpp \
        wml/WMLImageElement.cpp \
        wml/WMLImageLoader.cpp \
        wml/WMLInputElement.cpp \
        wml/WMLInsertedLegendElement.cpp \
        wml/WMLIntrinsicEvent.cpp \
        wml/WMLIntrinsicEventHandler.cpp \
        wml/WMLMetaElement.cpp \
        wml/WMLNoopElement.cpp \
        wml/WMLOnEventElement.cpp \
        wml/WMLPElement.cpp \
        wml/WMLOptGroupElement.cpp \
        wml/WMLOptionElement.cpp \
        wml/WMLPageState.cpp \
        wml/WMLPostfieldElement.cpp \
        wml/WMLPrevElement.cpp \
        wml/WMLRefreshElement.cpp \
        wml/WMLSelectElement.cpp \
        wml/WMLSetvarElement.cpp \
        wml/WMLTableElement.cpp \
        wml/WMLTaskElement.cpp \
        wml/WMLTemplateElement.cpp \
        wml/WMLTimerElement.cpp \
        wml/WMLVariables.cpp

    FEATURE_DEFINES_JAVASCRIPT += ENABLE_WML=1

    WML_NAMES = $$PWD/wml/WMLTagNames.in

    wmlnames_a.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}WMLNames.cpp
    wmlnames_a.commands = perl -I$$PWD/bindings/scripts $$PWD/dom/make_names.pl --tags $$PWD/wml/WMLTagNames.in --attrs $$PWD/wml/WMLAttributeNames.in --extraDefines \"$${DEFINES}\" --preprocessor \"$${QMAKE_MOC} -E\" --factory --wrapperFactory --outputDir $$GENERATED_SOURCES_DIR
    wmlnames_a.input = WML_NAMES
    wmlnames_a.dependency_type = TYPE_C
    wmlnames_a.CONFIG = target_predeps
    wmlnames_a.variable_out = GENERATED_SOURCES
    addExtraCompilerWithHeader(wmlnames_a)
    wmlnames_b.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}WMLElementFactory.cpp
    wmlnames_b.commands = @echo -n ''
    wmlnames_b.input = SVG_NAMES
    wmlnames_b.depends = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}WMLNames.cpp
    wmlnames_b.CONFIG = target_predeps
    wmlnames_b.variable_out = GENERATED_SOURCES
    addExtraCompilerWithHeader(wmlnames_b)
}

contains(DEFINES, ENABLE_XHTMLMP=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_XHTMLMP=1
}

contains(DEFINES, ENABLE_SVG=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_SVG=1

    SVG_NAMES = $$PWD/svg/svgtags.in

    XLINK_NAMES = $$PWD/svg/xlinkattrs.in

    IDL_BINDINGS += \
        svg/SVGZoomEvent.idl \
        svg/SVGAElement.idl \
        svg/SVGAltGlyphElement.idl \
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
        svg/SVGHKernElement.idl \
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
        bindings/js/JSSVGElementInstanceCustom.cpp \
        bindings/js/JSSVGLengthCustom.cpp \
        bindings/js/JSSVGMatrixCustom.cpp \
        bindings/js/JSSVGPathSegCustom.cpp \
        bindings/js/JSSVGPathSegListCustom.cpp \
        bindings/js/JSSVGPointListCustom.cpp \
        bindings/js/JSSVGTransformListCustom.cpp \
        css/SVGCSSComputedStyleDeclaration.cpp \
        css/SVGCSSParser.cpp \
        css/SVGCSSStyleSelector.cpp \
        rendering/style/SVGRenderStyle.cpp \
        rendering/style/SVGRenderStyleDefs.cpp \
        svg/SVGZoomEvent.cpp \
        rendering/PointerEventsHitRules.cpp \
        svg/SVGDocumentExtensions.cpp \
        svg/SVGImageLoader.cpp \
        svg/ColorDistance.cpp \
        svg/SVGAElement.cpp \
        svg/SVGAltGlyphElement.cpp \
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
        svg/SVGHKernElement.cpp \
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
        svg/animation/SMILTime.cpp \
        svg/animation/SMILTimeContainer.cpp \
        svg/animation/SVGSMILElement.cpp \
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
        svg/graphics/filters/SVGFETile.cpp \
        svg/graphics/filters/SVGFETurbulence.cpp \
        svg/graphics/filters/SVGFilter.cpp \
        svg/graphics/filters/SVGFilterBuilder.cpp \
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
        rendering/RenderSVGModelObject.cpp \
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


        # GENERATOR 5-C:
        svgnames_a.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}SVGNames.cpp
        svgnames_a.commands = perl -I$$PWD/bindings/scripts $$PWD/dom/make_names.pl --tags $$PWD/svg/svgtags.in --attrs $$PWD/svg/svgattrs.in --extraDefines \"$${DEFINES}\" --preprocessor \"$${QMAKE_MOC} -E\" --factory --wrapperFactory --outputDir $$GENERATED_SOURCES_DIR
        svgnames_a.input = SVG_NAMES
        svgnames_a.dependency_type = TYPE_C
        svgnames_a.CONFIG = target_predeps
        svgnames_a.variable_out = GENERATED_SOURCES
        addExtraCompilerWithHeader(svgnames_a)
        svgnames_b.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}SVGElementFactory.cpp
        svgnames_b.commands = @echo -n ''
        svgnames_b.input = SVG_NAMES
        svgnames_b.depends = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}SVGNames.cpp
        svgnames_b.CONFIG = target_predeps
        svgnames_b.variable_out = GENERATED_SOURCES
        addExtraCompilerWithHeader(svgnames_b)
        svgelementwrapper.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}JSSVGElementWrapperFactory.cpp
        svgelementwrapper.commands = @echo -n ''
        svgelementwrapper.input = SVG_NAMES
        svgelementwrapper.depends = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}SVGNames.cpp
        svgelementwrapper.CONFIG = target_predeps
        svgelementwrapper.variable_out = GENERATED_SOURCES
        addExtraCompiler(svgelementwrapper)
        svgelementwrapper_header.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}JSSVGElementWrapperFactory.h
        svgelementwrapper_header.commands = @echo -n ''
        svgelementwrapper_header.input = SVG_NAMES
        svgelementwrapper_header.depends = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}SVGNames.cpp
        svgelementwrapper_header.CONFIG = target_predeps
        svgelementwrapper_header.variable_out = GENERATED_FILES
        addExtraCompiler(svgelementwrapper_header)

        # GENERATOR 5-D:
        xlinknames.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}XLinkNames.cpp
        xlinknames.commands = perl -I$$PWD/bindings/scripts $$PWD/dom/make_names.pl --attrs $$PWD/svg/xlinkattrs.in --preprocessor \"$${QMAKE_MOC} -E\" --outputDir $$GENERATED_SOURCES_DIR
        xlinknames.input = XLINK_NAMES
        xlinknames.dependency_type = TYPE_C
        xlinknames.CONFIG = target_predeps
        xlinknames.variable_out = GENERATED_SOURCES
        addExtraCompilerWithHeader(xlinknames)

    # GENERATOR 6-A:
    cssprops.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.cpp
    cssprops.input = WALDOCSSPROPS
    cssprops.commands = perl -ne \"print lc\" ${QMAKE_FILE_NAME} $$DASHBOARDSUPPORTCSSPROPERTIES $$SVGCSSPROPERTIES > $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.in && cd $$GENERATED_SOURCES_DIR && perl $$PWD/css/makeprop.pl && $(DEL_FILE) ${QMAKE_FILE_BASE}.strip ${QMAKE_FILE_BASE}.in ${QMAKE_FILE_BASE}.gperf
    cssprops.CONFIG = target_predeps no_link
    cssprops.depend = ${QMAKE_FILE_NAME} DASHBOARDSUPPORTCSSPROPERTIES SVGCSSPROPERTIES
    addExtraCompilerWithHeader(cssprops)

    # GENERATOR 6-B:
    cssvalues.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.c
    cssvalues.input = WALDOCSSVALUES
    cssvalues.commands = perl -ne \"print lc\" ${QMAKE_FILE_NAME} $$SVGCSSVALUES > $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.in && cd $$GENERATED_SOURCES_DIR && perl $$PWD/css/makevalues.pl && $(DEL_FILE) ${QMAKE_FILE_BASE}.in ${QMAKE_FILE_BASE}.strip ${QMAKE_FILE_BASE}.gperf
    cssvalues.CONFIG = target_predeps no_link
    cssvalues.depend = ${QMAKE_FILE_NAME} SVGCSSVALUES
    addExtraCompilerWithHeader(cssvalues)
} else {
    # GENERATOR 6-A:
    cssprops.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.cpp
    cssprops.input = WALDOCSSPROPS
    cssprops.commands = perl -ne \"print lc\" ${QMAKE_FILE_NAME} $$DASHBOARDSUPPORTCSSPROPERTIES > $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.in && cd $$GENERATED_SOURCES_DIR && perl $$PWD/css/makeprop.pl && $(DEL_FILE) ${QMAKE_FILE_BASE}.strip ${QMAKE_FILE_BASE}.in ${QMAKE_FILE_BASE}.gperf
    cssprops.CONFIG = target_predeps no_link
    cssprops.depend = ${QMAKE_FILE_NAME} DASHBOARDSUPPORTCSSPROPERTIES
    addExtraCompilerWithHeader(cssprops)

    # GENERATOR 6-B:
    cssvalues.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.c
    cssvalues.input = WALDOCSSVALUES
    cssvalues.commands = perl -ne \"print lc\" ${QMAKE_FILE_NAME} > $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.in && cd $$GENERATED_SOURCES_DIR && perl $$PWD/css/makevalues.pl && $(DEL_FILE) ${QMAKE_FILE_BASE}.in ${QMAKE_FILE_BASE}.strip ${QMAKE_FILE_BASE}.gperf
    cssvalues.CONFIG = target_predeps no_link
    cssvalues.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}${QMAKE_FILE_BASE}.h
    addExtraCompiler(cssvalues)
}

contains(DEFINES, ENABLE_JAVASCRIPT_DEBUGGER=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_JAVASCRIPT_DEBUGGER=1

    IDL_BINDINGS += \
        inspector/JavaScriptCallFrame.idl

    SOURCES += \
        bindings/js/JSJavaScriptCallFrameCustom.cpp \
        inspector/JavaScriptCallFrame.cpp \
        inspector/JavaScriptDebugServer.cpp \
        inspector/JavaScriptProfile.cpp \
        inspector/JavaScriptProfileNode.cpp
}

contains(DEFINES, ENABLE_OFFLINE_WEB_APPLICATIONS=1) {
    FEATURE_DEFINES_JAVASCRIPT += ENABLE_OFFLINE_WEB_APPLICATIONS=1

IDL_BINDINGS += \
    loader/appcache/DOMApplicationCache.idl

SOURCES += \
    loader/appcache/ApplicationCache.cpp \
    loader/appcache/ApplicationCacheGroup.cpp \
    loader/appcache/ApplicationCacheStorage.cpp \
    loader/appcache/ApplicationCacheResource.cpp \
    loader/appcache/DOMApplicationCache.cpp \
    loader/appcache/ManifestParser.cpp \
    bindings/js/JSDOMApplicationCacheCustom.cpp
}

# GENERATOR 1: IDL compiler
idl.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}JS${QMAKE_FILE_BASE}.cpp
idl.variable_out = GENERATED_SOURCES
idl.input = IDL_BINDINGS
idl.commands = perl -I$$PWD/bindings/scripts $$PWD/bindings/scripts/generate-bindings.pl --defines \"$${FEATURE_DEFINES_JAVASCRIPT}\" --generator JS --include $$PWD/dom --include $$PWD/html --include $$PWD/xml --include $$PWD/svg --outputDir $$GENERATED_SOURCES_DIR --preprocessor \"$${QMAKE_MOC} -E\" ${QMAKE_FILE_NAME}
idl.CONFIG += target_predeps
addExtraCompilerWithHeader(idl)

# GENERATOR 2-A: LUT creator
domlut.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.lut.h
domlut.commands = perl $$PWD/../JavaScriptCore/create_hash_table ${QMAKE_FILE_NAME} -n WebCore > ${QMAKE_FILE_OUT}
domlut.depend = ${QMAKE_FILE_NAME}
domlut.input = DOMLUT_FILES
domlut.CONFIG += no_link
addExtraCompiler(domlut)

# GENERATOR 3: tokenizer (flex)
tokenizer.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.cpp
tokenizer.commands = flex -t < ${QMAKE_FILE_NAME} | perl $$PWD/css/maketokenizer > ${QMAKE_FILE_OUT}
tokenizer.dependency_type = TYPE_C
tokenizer.input = TOKENIZER
tokenizer.CONFIG += target_predeps no_link
addExtraCompiler(tokenizer)

# GENERATOR 4: CSS grammar
cssbison.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.cpp
cssbison.commands = perl $$PWD/css/makegrammar.pl ${QMAKE_FILE_NAME} $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}
cssbison.depend = ${QMAKE_FILE_NAME}
cssbison.input = CSSBISON
cssbison.CONFIG = target_predeps
cssbison.dependency_type = TYPE_C
cssbison.variable_out = GENERATED_SOURCES
addExtraCompilerWithHeader(cssbison)

# GENERATOR 5-A:
htmlnames.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}HTMLNames.cpp
htmlnames.commands = perl -I$$PWD/bindings/scripts $$PWD/dom/make_names.pl --tags $$PWD/html/HTMLTagNames.in --attrs $$PWD/html/HTMLAttributeNames.in --extraDefines \"$${DEFINES}\" --preprocessor \"$${QMAKE_MOC} -E\"  --factory --wrapperFactory --outputDir $$GENERATED_SOURCES_DIR
htmlnames.input = HTML_NAMES
htmlnames.dependency_type = TYPE_C
htmlnames.CONFIG = target_predeps
htmlnames.variable_out = GENERATED_SOURCES
addExtraCompilerWithHeader(htmlnames)

htmlelementfactory.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}HTMLElementFactory.cpp
htmlelementfactory.commands = @echo -n ''
htmlelementfactory.input = HTML_NAMES
htmlelementfactory.depends = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}HTMLNames.cpp
htmlelementfactory.CONFIG = target_predeps
htmlelementfactory.variable_out = GENERATED_SOURCES
htmlelementfactory.clean += ${QMAKE_FILE_OUT}
addExtraCompilerWithHeader(htmlelementfactory)

elementwrapperfactory.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}JSHTMLElementWrapperFactory.cpp
elementwrapperfactory.commands = @echo -n ''
elementwrapperfactory.input = HTML_NAMES
elementwrapperfactory.depends = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}HTMLNames.cpp
elementwrapperfactory.CONFIG = target_predeps
elementwrapperfactory.variable_out = GENERATED_SOURCES
elementwrapperfactory.clean += ${QMAKE_FILE_OUT}
addExtraCompilerWithHeader(elementwrapperfactory)

# GENERATOR 5-B:
xmlnames.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}XMLNames.cpp
xmlnames.commands = perl -I$$PWD/bindings/scripts $$PWD/dom/make_names.pl --attrs $$PWD/xml/xmlattrs.in --preprocessor \"$${QMAKE_MOC} -E\" --outputDir $$GENERATED_SOURCES_DIR
xmlnames.input = XML_NAMES
xmlnames.dependency_type = TYPE_C
xmlnames.CONFIG = target_predeps
xmlnames.variable_out = GENERATED_SOURCES
addExtraCompilerWithHeader(xmlnames)

# GENERATOR 8-A:
entities.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}HTMLEntityNames.c
entities.commands = gperf -a -L ANSI-C -C -G -c -o -t --key-positions="*" -N findEntity -D -s 2 < $$PWD/html/HTMLEntityNames.gperf > $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}HTMLEntityNames.c
entities.input = ENTITIES_GPERF
entities.dependency_type = TYPE_C
entities.CONFIG = target_predeps no_link
entities.clean = ${QMAKE_FILE_OUT}
addExtraCompiler(entities)

# GENERATOR 8-B:
doctypestrings.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.cpp
doctypestrings.input = DOCTYPESTRINGS
doctypestrings.commands = perl -e \"print \'$${LITERAL_HASH}include <string.h>\';\" > ${QMAKE_FILE_OUT} && echo // bogus >> ${QMAKE_FILE_OUT} && gperf -CEot -L ANSI-C --key-positions="*" -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards < ${QMAKE_FILE_NAME} >> ${QMAKE_FILE_OUT}
doctypestrings.dependency_type = TYPE_C
doctypestrings.CONFIG += target_predeps no_link
doctypestrings.clean = ${QMAKE_FILE_OUT}
addExtraCompiler(doctypestrings)

# GENERATOR 8-C:
colordata.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}ColorData.c
colordata.commands = perl -e \"print \'$${LITERAL_HASH}include <string.h>\';\" > ${QMAKE_FILE_OUT} && echo // bogus >> ${QMAKE_FILE_OUT} && gperf -CDEot -L ANSI-C --key-positions="*" -N findColor -D -s 2 < ${QMAKE_FILE_NAME} >> ${QMAKE_FILE_OUT}
colordata.input = COLORDAT_GPERF
colordata.CONFIG = target_predeps no_link
addExtraCompiler(colordata)

# GENERATOR 9:
stylesheets.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}UserAgentStyleSheetsData.cpp
stylesheets.commands = perl $$PWD/css/make-css-file-arrays.pl --preprocessor \"$${QMAKE_MOC} -E\" ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}UserAgentStyleSheets.h ${QMAKE_FILE_OUT} $$STYLESHEETS_EMBED
STYLESHEETS_EMBED_GENERATOR_SCRIPT = $$PWD/css/make-css-file-arrays.pl
stylesheets.input = STYLESHEETS_EMBED_GENERATOR_SCRIPT
stylesheets.depends = $$STYLESHEETS_EMBED
stylesheets.CONFIG = target_predeps
stylesheets.variable_out = GENERATED_SOURCES
stylesheets.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_GENERATED_SOURCES_DIR_SLASH}UserAgentStyleSheets.h
addExtraCompilerWithHeader(stylesheets, $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}UserAgentStyleSheets.h)

# GENERATOR 10: XPATH grammar
xpathbison.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.cpp
xpathbison.commands = bison -d -p xpathyy ${QMAKE_FILE_NAME} -o $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.c && $(MOVE) $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.c $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.cpp && $(MOVE) $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.h $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.h
xpathbison.depend = ${QMAKE_FILE_NAME}
xpathbison.input = XPATHBISON
xpathbison.CONFIG = target_predeps
xpathbison.dependency_type = TYPE_C
xpathbison.variable_out = GENERATED_SOURCES
addExtraCompilerWithHeader(xpathbison)

include($$PWD/../WebKit/qt/Api/headers.pri)
HEADERS += $$WEBKIT_API_HEADERS
!CONFIG(QTDIR_build) {
    target.path = $$[QT_INSTALL_LIBS]
    headers.files = $$WEBKIT_API_HEADERS
    headers.path = $$[QT_INSTALL_HEADERS]/QtWebKit
    prf.files = $$PWD/../WebKit/qt/Api/qtwebkit.prf
    prf.path = $$[QT_INSTALL_PREFIX]/mkspecs/features

    VERSION=$${QT_MAJOR_VERSION}.$${QT_MINOR_VERSION}.$${QT_PATCH_VERSION}

    win32-*|wince* {
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

    mac {
        !static:contains(QT_CONFIG, qt_framework):!CONFIG(webkit_no_framework) {
            !build_pass {
                message("Building QtWebKit as a framework, as that's how Qt was built. You can")
                message("override this by passing CONFIG+=webkit_no_framework to build-webkit.")
            } else {
                debug_and_release:CONFIG(debug, debug|release) {
                    TARGET = $$qtLibraryTarget($$TARGET)
                }
            }

            CONFIG += lib_bundle qt_no_framework_direct_includes qt_framework
            FRAMEWORK_HEADERS.version = Versions
            FRAMEWORK_HEADERS.files = $$WEBKIT_API_HEADERS
            FRAMEWORK_HEADERS.path = Headers
            QMAKE_BUNDLE_DATA += FRAMEWORK_HEADERS
        }

        QMAKE_LFLAGS_SONAME = "$${QMAKE_LFLAGS_SONAME}$${DESTDIR}$${QMAKE_DIR_SEP}"
    }
}

CONFIG(QTDIR_build):isEqual(QT_MAJOR_VERSION, 4):greaterThan(QT_MINOR_VERSION, 4) {
    # start with 4.5
    # Remove the following 2 lines if you want debug information in WebCore
    CONFIG -= separate_debug_info
    CONFIG += no_debug_info
}

