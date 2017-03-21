/* This file is part of the WebKit open source project.
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef webkitdomautocleanups_h
#define webkitdomautocleanups_h

#include <glib-object.h>

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
#ifndef __GI_SCANNER__

G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMAttr, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMBlob, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMCDATASection, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMCSSRule, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMCSSRuleList, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMCSSStyleDeclaration, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMCSSStyleSheet, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMCSSValue, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMCharacterData, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMClientRect, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMClientRectList, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMComment, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMDOMImplementation, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMDOMWindow, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMDocument, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMDocumentFragment, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMDocumentType, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMEvent, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMEventTarget, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMFile, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMFileList, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLAnchorElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLAppletElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLAreaElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLBRElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLBaseElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLBodyElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLButtonElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLCanvasElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLCollection, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLDListElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLDirectoryElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLDivElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLDocument, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLEmbedElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLFieldSetElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLFontElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLFormElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLFrameElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLFrameSetElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLHRElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLHeadElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLHeadingElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLHtmlElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLIFrameElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLImageElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLInputElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLLIElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLLabelElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLLegendElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLLinkElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLMapElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLMarqueeElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLMenuElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLMetaElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLModElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLOListElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLObjectElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLOptGroupElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLOptionElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLOptionsCollection, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLParagraphElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLParamElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLPreElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLQuoteElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLScriptElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLSelectElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLStyleElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLTableCaptionElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLTableCellElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLTableColElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLTableElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLTableRowElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLTableSectionElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLTextAreaElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLTitleElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMHTMLUListElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMKeyboardEvent, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMMediaList, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMMouseEvent, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMNamedNodeMap, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMNode, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMNodeFilter, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMNodeIterator, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMNodeList, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMObject, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMProcessingInstruction, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMRange, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMStyleSheet, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMStyleSheetList, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMText, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMTreeWalker, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMUIEvent, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMWheelEvent, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMXPathExpression, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMXPathNSResolver, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDOMXPathResult, g_object_unref)

#endif
#endif

#endif
