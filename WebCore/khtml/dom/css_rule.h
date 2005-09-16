/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
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
#ifndef _CSS_css_rule_h_
#define _CSS_css_rule_h_

#if !KHTML_NO_CPLUSPLUS_DOM

#include <dom/dom_string.h>
#include <dom/css_stylesheet.h>
#include <dom/css_value.h>

#endif

namespace DOM {

#if !KHTML_NO_CPLUSPLUS_DOM

class CSSRuleImpl;

#endif

/**
 * The <code> CSSRule </code> interface is the abstract base interface
 * for any type of CSS <a
 * href="http://www.w3.org/TR/REC-CSS2/syndata.html#q5"> statement
 * </a> . This includes both <a
 * href="http://www.w3.org/TR/REC-CSS2/syndata.html#q8"> rule sets
 * </a> and <a
 * href="http://www.w3.org/TR/REC-CSS2/syndata.html#at-rules">
 * at-rules </a> . An implementation is expected to preserve all rules
 * specified in a CSS style sheet, even if it is not recognized.
 * Unrecognized rules are represented using the <code> CSSUnknownRule
 * </code> interface.
 *
 */
class CSSRule
{

public:

#if !KHTML_NO_CPLUSPLUS_DOM

    CSSRule();
    CSSRule(const CSSRule &other);
    CSSRule(CSSRuleImpl *impl);
public:

    CSSRule & operator = (const CSSRule &other);

    ~CSSRule();

#endif

    /**
     * An integer indicating which type of rule this is.
     *
     */
    enum RuleType {
        UNKNOWN_RULE = 0,
        STYLE_RULE = 1,
        CHARSET_RULE = 2,
        IMPORT_RULE = 3,
        MEDIA_RULE = 4,
        FONT_FACE_RULE = 5,
        PAGE_RULE = 6,
        QUIRKS_RULE = 100 // Not part of the official DOM
    };

#if !KHTML_NO_CPLUSPLUS_DOM

    /**
     * The type of the rule, as defined above. The expectation is that
     * binding-specific casting methods can be used to cast down from
     * an instance of the <code> CSSRule </code> interface to the
     * specific derived interface implied by the <code> type </code> .
     *
     */
    unsigned short type() const;

    /**
     * The parsable textual representation of the rule. This reflects
     * the current state of the rule and not its initial value.
     *
     */
    DOMString cssText() const;

    /**
     * see @ref cssText
     * @exception DOMException
     *
     *  HIERARCHY_REQUEST_ERR: Raised if the rule cannot be inserted
     * at this point in the style sheet.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this style sheet is
     * readonly.
     *
     * @exception CSSException
     * SYNTAX_ERR: Raised if the specified CSS string value has a
     * syntax error and is unparsable.
     *
     * INVALID_MODIFICATION_ERR: Raised if the specified CSS string value
     * represents a different type of rule than the current one.
     */
    void setCssText( const DOMString & );

    /**
     * The style sheet that contains this rule.
     *
     */
    CSSStyleSheet parentStyleSheet() const;

    /**
     * If this rule is contained inside another rule (e.g. a style
     * rule inside an @media block), this is the containing rule. If
     * this rule is not nested inside any other rules, this returns
     * <code> null </code> .
     *
     */
    CSSRule parentRule() const;

    /**
     * @internal
     * not part of the DOM
     */
    CSSRuleImpl *handle() const;
    bool isNull() const;

protected:
    CSSRuleImpl *impl;

    void assignOther( const CSSRule &other, RuleType thisType );

#endif

};

#if !KHTML_NO_CPLUSPLUS_DOM

class CSSCharsetRuleImpl;

/**
 * The <code> CSSCharsetRule </code> interface a <a href=""> @charset
 * rule </a> in a CSS style sheet. A <code> @charset </code> rule can
 * be used to define the encoding of the style sheet.
 *
 */
class CSSCharsetRule : public CSSRule
{
public:
    CSSCharsetRule();
    CSSCharsetRule(const CSSCharsetRule &other);
    CSSCharsetRule(const CSSRule &other);
    CSSCharsetRule(CSSCharsetRuleImpl *impl);
public:

    CSSCharsetRule & operator = (const CSSCharsetRule &other);
    CSSCharsetRule & operator = (const CSSRule &other);

    ~CSSCharsetRule();

    /**
     * The encoding information used in this <code> @charset </code>
     * rule.
     *
     */
    DOMString encoding() const;

    /**
     * see @ref encoding
     * @exception CSSException
     * SYNTAX_ERR: Raised if the specified encoding value has a syntax
     * error and is unparsable.
     *
     * @exception DOMException
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this encoding rule is
     * readonly.
     *
     */
    void setEncoding( const DOMString & );
};


class CSSFontFaceRuleImpl;
/**
 * The <code> CSSFontFaceRule </code> interface represents a <a
 * href="http://www.w3.org/TR/REC-CSS2/fonts.html#font-descriptions">
 * @font-face rule </a> in a CSS style sheet. The <code> @font-face
 * </code> rule is used to hold a set of font descriptions.
 *
 */
class CSSFontFaceRule : public CSSRule
{
public:
    CSSFontFaceRule();
    CSSFontFaceRule(const CSSFontFaceRule &other);
    CSSFontFaceRule(const CSSRule &other);
    CSSFontFaceRule(CSSFontFaceRuleImpl *impl);
public:

    CSSFontFaceRule & operator = (const CSSFontFaceRule &other);
    CSSFontFaceRule & operator = (const CSSRule &other);

    ~CSSFontFaceRule();

    /**
     * The <a href="http://www.w3.org/TR/REC-CSS2/syndata.html#q8">
     * declaration-block </a> of this rule.
     *
     */
    CSSStyleDeclaration style() const;
};

class CSSImportRuleImpl;
/**
 * The <code> CSSImportRule </code> interface represents a <a
 * href="http://www.w3.org/TR/REC-CSS2/cascade.html#at-import">
 * @import rule </a> within a CSS style sheet. The <code> @import
 * </code> rule is used to import style rules from other style sheets.
 *
 */
class CSSImportRule : public CSSRule
{
public:
    CSSImportRule();
    CSSImportRule(const CSSImportRule &other);
    CSSImportRule(const CSSRule &other);
    CSSImportRule(CSSImportRuleImpl *impl);
public:

    CSSImportRule & operator = (const CSSImportRule &other);
    CSSImportRule & operator = (const CSSRule &other);

    ~CSSImportRule();

    /**
     * The location of the style sheet to be imported. The attribute
     * will not contain the <code> "url(...)" </code> specifier around
     * the URI.
     *
     */
    DOMString href() const;

    /**
     * A list of media types for which this style sheet may be used.
     *
     */
    MediaList media() const;

    /**
     * The style sheet referred to by this rule, if it has been
     * loaded. The value of this attribute is null if the style sheet
     * has not yet been loaded or if it will not be loaded (e.g. if
     * the style sheet is for a media type not supported by the user
     * agent).
     *
     */
    CSSStyleSheet styleSheet() const;
};

class CSSMediaRuleImpl;
/**
 * The <code> CSSMediaRule </code> interface represents a <a
 * href="http://www.w3.org/TR/REC-CSS2/media.html#at-media-rule">
 * @media rule </a> in a CSS style sheet. A <code> @media </code> rule
 * can be used to delimit style rules for specific media types.
 *
 */
class CSSMediaRule : public CSSRule
{
public:
    CSSMediaRule();
    CSSMediaRule(const CSSMediaRule &other);
    CSSMediaRule(const CSSRule &other);
    CSSMediaRule(CSSMediaRuleImpl *impl);
public:

    CSSMediaRule & operator = (const CSSMediaRule &other);
    CSSMediaRule & operator = (const CSSRule &other);

    ~CSSMediaRule();

    /**
     * A list of <a
     * href="http://www.w3.org/TR/REC-CSS2/media.html#media-types">
     * media types </a> for this rule.
     *
     */
    MediaList media() const;

    /**
     * A list of all CSS rules contained within the media block.
     *
     */
    CSSRuleList cssRules() const;

    /**
     * Used to insert a new rule into the media block.
     *
     * @param rule The parsable text representing the rule. For rule
     * sets this contains both the selector and the style declaration.
     * For at-rules, this specifies both the at-identifier and the
     * rule content.
     *
     * @param index The index within the media block's rule collection
     * of the rule before which to insert the specified rule. If the
     * specified index is equal to the length of the media blocks's
     * rule collection, the rule will be added to the end of the media
     * block.
     *
     * @return The index within the media block's rule collection of
     * the newly inserted rule.
     *
     * @exception DOMException
     * HIERARCHY_REQUEST_ERR: Raised if the rule cannot be inserted at
     * the specified index. e.g. if an <code> @import </code> rule is
     * inserted after a standard rule set or other at-rule.
     *
     *  INDEX_SIZE_ERR: Raised if the specified index is not a valid
     * insertion point.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this media rule is
     * readonly.
     *
     * @exception CSSException
     *  SYNTAX_ERR: Raised if the specified rule has a syntax error
     * and is unparsable.
     *
     */
    unsigned insertRule ( const DOMString &rule, unsigned index );

    /**
     * Used to delete a rule from the media block.
     *
     * @param index The index within the media block's rule collection
     * of the rule to remove.
     *
     * @return
     *
     * @exception DOMException
     * INDEX_SIZE_ERR: Raised if the specified index does not
     * correspond to a rule in the media rule list.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this media rule is
     * readonly.
     *
     */
    void deleteRule ( unsigned index );
};


class CSSPageRuleImpl;
/**
 * The <code> CSSPageRule </code> interface represents a <a
 * href="http://www.w3.org/TR/REC-CSS2/page.html#page-box"> @page rule
 * </a> within a CSS style sheet. The <code> @page </code> rule is
 * used to specify the dimensions, orientation, margins, etc. of a
 * page box for paged media.
 *
 */
class CSSPageRule : public CSSRule
{
public:
    CSSPageRule();
    CSSPageRule(const CSSPageRule &other);
    CSSPageRule(const CSSRule &other);
    CSSPageRule(CSSPageRuleImpl *impl);
public:

    CSSPageRule & operator = (const CSSPageRule &other);
    CSSPageRule & operator = (const CSSRule &other);

    ~CSSPageRule();

    /**
     * The parsable textual representation of the page selector for
     * the rule.
     *
     */
    DOMString selectorText() const;

    /**
     * see @ref selectorText
     * @exception CSSException
     * SYNTAX_ERR: Raised if the specified CSS string value has a
     * syntax error and is unparsable.
     *
     * @exception DOMException
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this style sheet is
     * readonly.
     *
     */
    void setSelectorText( const DOMString & );

    /**
     * The <a href="http://www.w3.org/TR/REC-CSS2/syndata.html#q8">
     * declaration-block </a> of this rule.
     *
     */
    CSSStyleDeclaration style() const;
};

class CSSStyleRuleImpl;
/**
 * The <code> CSSStyleRule </code> interface represents a single <a
 * href="http://www.w3.org/TR/REC-CSS2/syndata.html#q8"> rule set </a>
 * in a CSS style sheet.
 *
 */
class CSSStyleRule : public CSSRule
{
public:
    CSSStyleRule();
    CSSStyleRule(const CSSStyleRule &other);
    CSSStyleRule(const CSSRule &other);
    CSSStyleRule(CSSStyleRuleImpl *impl);
public:

    CSSStyleRule & operator = (const CSSStyleRule &other);
    CSSStyleRule & operator = (const CSSRule &other);

    ~CSSStyleRule();

    /**
     * The textual representation of the <a
     * href="http://www.w3.org/TR/REC-CSS2/selector.html"> selector
     * </a> for the rule set. The implementation may have stripped out
     * insignificant whitespace while parsing the selector.
     *
     */
    DOMString selectorText() const;

    /**
     * see @ref selectorText
     * @exception CSSException
     * SYNTAX_ERR: Raised if the specified CSS string value has a
     * syntax error and is unparsable.
     *
     * @exception DOMException
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this style sheet is
     * readonly.
     *
     */
    void setSelectorText( const DOMString & );

    /**
     * The <a href="http://www.w3.org/TR/REC-CSS2/syndata.html#q8">
     * declaration-block </a> of this rule set.
     *
     */
    CSSStyleDeclaration style() const;
};

class CSSUnknownRuleImpl;
/**
 * The <code> CSSUnkownRule </code> interface represents an at-rule
 * not supported by this user agent.
 *
 */
class CSSUnknownRule : public CSSRule
{
public:
    CSSUnknownRule();
    CSSUnknownRule(const CSSUnknownRule &other);
    CSSUnknownRule(const CSSRule &other);
    CSSUnknownRule(CSSUnknownRuleImpl *impl);
public:

    CSSUnknownRule & operator = (const CSSUnknownRule &other);
    CSSUnknownRule & operator = (const CSSRule &other);

    ~CSSUnknownRule();
};


class CSSRuleListImpl;
class StyleListImpl;
/**
 * The <code> CSSRuleList </code> interface provides the abstraction
 * of an ordered collection of CSS rules.
 *
 */
class CSSRuleList
{
public:
    CSSRuleList();
    CSSRuleList(const CSSRuleList &other);
    CSSRuleList(CSSRuleListImpl *i);
    CSSRuleList(StyleListImpl *i);
public:

    CSSRuleList & operator = (const CSSRuleList &other);

    ~CSSRuleList();

    /**
     * The number of <code> CSSRule </code> s in the list. The range
     * of valid child rule indices is <code> 0 </code> to <code>
     * length-1 </code> inclusive.
     *
     */
    unsigned length() const;

    /**
     * Used to retrieve a CSS rule by ordinal index. The order in this
     * collection represents the order of the rules in the CSS style
     * sheet.
     *
     * @param index Index into the collection
     *
     * @return The style rule at the <code> index </code> position in
     * the <code> CSSRuleList </code> , or <code> null </code> if that
     * is not a valid index.
     *
     */
    CSSRule item ( unsigned index );

    /**
     * @internal
     * not part of the DOM
     */
    CSSRuleListImpl *handle() const;
    bool isNull() const;

protected:
    // we just need a pointer to an implementation here.
    CSSRuleListImpl *impl;
};

#endif

} // namespace

#endif
