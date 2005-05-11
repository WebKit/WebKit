/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * This file includes excerpts from the Document Object Model (DOM)
 * Level 2 Specification (Candidate Recommendation)
 * http://www.w3.org/TR/2000/CR-DOM-Level-2-20000510/
 * Copyright © 2000 W3C® (MIT, INRIA, Keio), All Rights Reserved.
 *
 */
#ifndef _CSS_css_stylesheet_h_
#define _CSS_css_stylesheet_h_

#include <dom/dom_string.h>
#include <dom/dom_node.h>
#include <dom/dom_misc.h>


namespace DOM {

#if !KHTML_NO_CPLUSPLUS_DOM

class StyleSheetImpl;
class MediaList;
class NodeImpl;
class DocumentImpl;

/**
 * The <code> StyleSheet </code> interface is the abstract base
 * interface for any type of style sheet. It represents a single style
 * sheet associated with a structured document. In HTML, the
 * StyleSheet interface represents either an external style sheet,
 * included via the HTML <a
 * href="http://www.w3.org/TR/REC-html40/struct/links.html#h-12.3">
 * LINK </a> element, or an inline <a
 * href="http://www.w3.org/TR/REC-html40/present/styles.html#h-14.2.3">
 * STYLE </a> element. In XML, this interface represents an external
 * style sheet, included via a <a
 * href="http://www.w3.org/TR/xml-stylesheet"> style sheet processing
 * instruction </a> .
 *
 */
class StyleSheet
{
public:
    StyleSheet();
    StyleSheet(const StyleSheet &other);
    StyleSheet(StyleSheetImpl *impl);
public:

    StyleSheet & operator = (const StyleSheet &other);

    ~StyleSheet();

    /**
     * This specifies the style sheet language for this style sheet.
     * The style sheet language is specified as a content type (e.g.
     * "text/css"). The content type is often specified in the <code>
     * ownerNode </code> . A list of registered content types can be
     * found at <a
     * href="ftp://ftp.isi.edu/in-notes/iana/assignments/media-types/">
     * ftp://ftp.isi.edu/in-notes/iana/assignments/media-types/ </a> .
     * Also see the <a
     * href="http://www.w3.org/TR/REC-html40/struct/links.html#adef-type-A">
     * type attribute definition </a> for the <code> LINK </code>
     * element in HTML 4.0, and the type pseudo-attribute for the XML
     * <a href="http://www.w3.org/TR/xml-stylesheet"> style sheet
     * processing instruction </a> .
     *
     */
    DOMString type() const;

    /**
     * <code> false </code> if the style sheet is applied to the
     * document. <code> true </code> if it is not. Modifying this
     * attribute may cause a reresolution of style for the document.
     *
     */
    bool disabled() const;

    /**
     * see @ref disabled
     */
    void setDisabled( bool );

    /**
     * The node that associates this style sheet with the document.
     * For HTML, this may be the corresponding <code> LINK </code> or
     * <code> STYLE </code> element. For XML, it may be the linking
     * processing instruction. For style sheets that are included by
     * other style sheets, this attribute has a value of null.
     *
     */
    Node ownerNode() const;

    /**
     * For style sheet languages that support the concept of style
     * sheet inclusion, this attribute represents the including style
     * sheet, if one exists. If the style sheet is a top-level style
     * sheet, or the style sheet language does not support inclusion,
     * the value of the attribute is null.
     *
     */
    StyleSheet parentStyleSheet() const;

    /**
     * If the style sheet is a linked style sheet, the value of its
     * attribute is its location. For inline style sheets, the value
     * of this attribute is null. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/links.html#adef-href">
     * href attribute definition </a> for the <code> LINK </code>
     * element in HTML 4.0, and the href pseudo-attribute for the XML
     * <a href="http://www.w3.org/TR/xml-stylesheet"> style sheet
     * processing instruction </a> .
     *
     */
    DOMString href() const;

    /**
     * The advisory title. The title is often specified in the <code>
     * ownerNode </code> . See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/global.html#adef-title">
     * title attribute definition </a> for the <code> LINK </code>
     * element in HTML 4.0, and the title pseudo-attribute for the XML
     * <a href="http://www.w3.org/TR/xml-stylesheet"> style sheet
     * processing instruction </a> .
     *
     */
    DOMString title() const;

    /**
     * The intended destination media for style information. The media
     * is often specified in the <code> ownerNode </code> . See the <a
     * href="http://www.w3.org/TR/REC-html40/present/styles.html#adef-media">
     * media attribute definition </a> for the <code> LINK </code>
     * element in HTML 4.0, and the media pseudo-attribute for the XML
     * <a href="http://www.w3.org/TR/WD-xml-stylesheet"> style sheet
     * processing instruction </a> .
     *
     */
    MediaList media() const;

    /**
     * @internal
     */
    bool isCSSStyleSheet() const;
    StyleSheetImpl *handle() const { return impl; }
    bool isNull() const;
protected:
    StyleSheetImpl *impl;
};

#endif

/**
 * This exception is raised when a specific CSS operation is impossible
 * to perform.
 */
class CSSException
{

public:

#if !KHTML_NO_CPLUSPLUS_DOM

    CSSException(unsigned short _code) { code = _code; }
    CSSException(const CSSException &other) { code = other.code; }

    CSSException & operator = (const CSSException &other)
        { code = other.code; return *this; }

    virtual ~CSSException() {}
    /**
     * An integer indicating the type of error generated.
     *
     */
    unsigned short   code;

#endif

    enum ExceptionCode
    {
        SYNTAX_ERR                     = 0,
        INVALID_MODIFICATION_ERR       = 1,
        _EXCEPTION_OFFSET              = 1000,
        _EXCEPTION_MAX                 = 1999
    };
};

#if !KHTML_NO_CPLUSPLUS_DOM

class CSSStyleSheetImpl;
class CSSRule;
class CSSRuleList;

/**
 * The <code> CSSStyleSheet </code> interface is a concrete interface
 * used to represent a CSS style sheet i.e. a style sheet whose
 * content type is "text/css".
 *
 */
class CSSStyleSheet : public StyleSheet
{
public:
    CSSStyleSheet();
    CSSStyleSheet(const CSSStyleSheet &other);
    CSSStyleSheet(const StyleSheet &other);
    CSSStyleSheet(CSSStyleSheetImpl *impl);
public:

    CSSStyleSheet & operator = (const CSSStyleSheet &other);
    CSSStyleSheet & operator = (const StyleSheet &other);

    ~CSSStyleSheet();

    /**
     * If this style sheet comes from an <code> @import </code> rule,
     * the <code> ownerRule </code> attribute will contain the <code>
     * CSSImportRule </code> . In that case, the <code> ownerNode
     * </code> attribute in the <code> StyleSheet </code> interface
     * will be <code> null </code> . If the style sheet comes from an
     * element or a processing instruction, the <code> ownerRule
     * </code> attribute will be <code> null </code> and the <code>
     * ownerNode </code> attribute will contain the <code> Node
     * </code> .
     *
     */
    CSSRule ownerRule() const;

    /**
     * The list of all CSS rules contained within the style sheet.
     * This includes both <a
     * href="http://www.w3.org/TR/REC-CSS2/syndata.html#q8"> rule sets
     * </a> and <a
     * href="http://www.w3.org/TR/REC-CSS2/syndata.html#at-rules">
     * at-rules </a> .
     *
     */
    CSSRuleList cssRules() const;

    /**
     * Used to insert a new rule into the style sheet. The new rule
     * now becomes part of the cascade.
     *
     * @param rule The parsable text representing the rule. For rule
     * sets this contains both the selector and the style declaration.
     * For at-rules, this specifies both the at-identifier and the
     * rule content.
     *
     * @param index The index within the style sheet's rule list of
     * the rule before which to insert the specified rule. If the
     * specified index is equal to the length of the style sheet's
     * rule collection, the rule will be added to the end of the style
     * sheet.
     *
     * @return The index within the style sheet's rule collection of
     * the newly inserted rule.
     *
     * @exception DOMException
     * HIERARCHY_REQUEST_ERR: Raised if the rule cannot be inserted at
     * the specified index e.g. if an <code> @import </code> rule is
     * inserted after a standard rule set or other at-rule.
     *
     *  INDEX_SIZE_ERR: Raised if the specified index is not a valid
     * insertion point.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this style sheet is
     * readonly.
     *
     * @exception CSSException
     *  SYNTAX_ERR: Raised if the specified rule has a syntax error
     * and is unparsable.
     *
     */
    unsigned long insertRule ( const DOMString &rule, unsigned long index );

    /**
     * Used to delete a rule from the style sheet.
     *
     * @param index The index within the style sheet's rule list of
     * the rule to remove.
     *
     * @return
     *
     * @exception DOMException
     * INDEX_SIZE_ERR: Raised if the specified index does not
     * correspond to a rule in the style sheet's rule list.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this style sheet is
     * readonly.
     *
     */
    void deleteRule ( unsigned long index );

    /* Microsoft extension. */
    void addRule ( const DOMString &selector, const DOMString &style, long index );
};


class StyleSheetListImpl;
class StyleSheet;

/**
 * The <code> StyleSheetList </code> interface provides the
 * abstraction of an ordered collection of style sheets.
 *
 */
class StyleSheetList
{
public:
    StyleSheetList();
    StyleSheetList(const StyleSheetList &other);
    StyleSheetList(StyleSheetListImpl *impl);
public:

    StyleSheetList & operator = (const StyleSheetList &other);

    ~StyleSheetList();

    /**
     * The number of <code> StyleSheet </code> in the list. The range
     * of valid child stylesheet indices is <code> 0 </code> to <code>
     * length-1 </code> inclusive.
     *
     */
    unsigned long length() const;

    /**
     * Used to retrieve a style sheet by ordinal index.
     *
     * @param index Index into the collection
     *
     * @return The style sheet at the <code> index </code> position in
     * the <code> StyleSheetList </code> , or <code> null </code> if
     * that is not a valid index.
     *
     */
    StyleSheet item ( unsigned long index );

    /**
     * @internal
     */
    StyleSheetListImpl *handle() const;
    bool isNull() const;

protected:
    StyleSheetListImpl *impl;
};


class MediaListImpl;
class CSSRule;
class CSSStyleSheet;

/**
 * The <code> MediaList </code> interface provides the abstraction of
 * an ordered collection of media, without defining or constraining
 * how this collection is implemented. All media are lowercase
 * strings.
 *
 */
class MediaList
{
public:
    MediaList();
    MediaList(const MediaList &other);
    MediaList(MediaListImpl *impl);
public:

    MediaList & operator = (const MediaList &other);

    ~MediaList();

    /**
     * The parsable textual representation of the media list. This is a
     * comma-separated list of media.
     *
     * @exception DOMException
     * SYNTAX_ERR: Raised if the specified string value has a syntax error and
     * is unparsable.
     *
     * NO_MODIFICATION_ALLOWED_ERR: Raised if this media list is readonly.
     */
    DOMString mediaText() const;

    /**
     * see @ref mediaText
     */
    void setMediaText(const DOMString &value);

    /**
     * The number of media in the list. The range of valid media is 0 to length-1 inclusive.
     */
    unsigned long length() const;


    /**
     * Returns the indexth in the list. If index is greater than or equal to
     * the number of media in the list, this returns null.
     *
     * @param index Index into the collection.
     *
     * @return The medium at the indexth position in the MediaList, or null if
     * that is not a valid index.
     */
    DOMString item(unsigned long index) const;

    /**
     * Deletes the medium indicated by oldMedium from the list.
     *
     * @param oldMedium The medium to delete in the media list.
     *
     * @exception DOMException
     * NO_MODIFICATION_ALLOWED_ERR: Raised if this list is readonly.
     *
     * NOT_FOUND_ERR: Raised if oldMedium is not in the list.
     */
    void deleteMedium(const DOMString &oldMedium);

    /**
     * Adds the medium newMedium to the end of the list. If the newMedium is
     * already used, it is first removed.
     *
     * @param newMedium The new medium to add.
     *
     * @exception DOMException
     * INVALID_CHARACTER_ERR: If the medium contains characters that are
     * invalid in the underlying style language.
     *
     * NO_MODIFICATION_ALLOWED_ERR: Raised if this list is readonly.
     */
    void appendMedium(const DOMString &newMedium);

    /**
     * @internal
     */
    MediaListImpl *handle() const;
    bool isNull() const;

protected:
    MediaListImpl *impl;
};

class LinkStyleImpl;

class LinkStyle
{
public:
    LinkStyle();
    LinkStyle(const LinkStyle &other);

    LinkStyle & operator = (const LinkStyle &other);
    LinkStyle & operator = (const Node &other);

    ~LinkStyle();

    StyleSheet sheet();

    bool isNull() const;

protected:
    NodeImpl *node;
    LinkStyleImpl *impl;
};

class DocumentStyleImpl;

class DocumentStyle
{
public:
    DocumentStyle();
    DocumentStyle(const DocumentStyle &other);

    DocumentStyle & operator = (const DocumentStyle &other);
    DocumentStyle & operator = (const Document &other);

    ~DocumentStyle();

    StyleSheetList styleSheets();

    DOMString preferredStylesheetSet();
    DOMString selectedStylesheetSet();
    void setSelectedStylesheetSet(const DOMString& aString);

    bool isNull() const;

protected:
    DocumentImpl *doc;
    DocumentStyleImpl *impl;
};

#endif

}; // namespace

#endif
