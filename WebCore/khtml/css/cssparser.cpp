/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 *               2001 Andreas Schlapbach (schlpbch@iam.unibe.ch)
 *               2001 Dirk Mueller (mueller@kde.org)
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
 */

//#define CSS_DEBUG
//#define CSS_AURAL
//#define CSS_DEBUG_BCKGR

#include <assert.h>

#include "css/css_stylesheetimpl.h"
#include "css/css_ruleimpl.h"
#include "css/css_valueimpl.h"
#include "css/csshelper.h"

#include "dom/css_stylesheet.h"
#include "dom/css_rule.h"
#include "dom/dom_string.h"
#include "dom/dom_exception.h"

#include "xml/dom_nodeimpl.h"
#include "html/html_documentimpl.h"
#include "khtml_part.h"
#include "khtmlview.h"

using namespace DOM;

#include <kdebug.h>
#include <kglobal.h>
#include <kglobalsettings.h> // For system fonts
#include <kapplication.h>

#include "misc/htmlhashes.h"
#include "misc/helper.h"

//
// The following file defines the function
//     const struct props *findProp(const char *word, int len)
//
// with 'props->id' a CSS property in the range from CSS_PROP_MIN to
// (and including) CSS_PROP_TOTAL-1
#include "cssproperties.c"
#include "cssvalues.c"


static QPtrList<CSSProperty>* m_propList = 0;
static bool m_bImportant = FALSE;
static bool m_bnonCSSHint = FALSE;

int DOM::getPropertyID(const char *tagStr, int len)
{
    const struct props *propsPtr = findProp(tagStr, len);
    if (!propsPtr)
        return 0;

    return propsPtr->id;
}

// ------------------------------------------------------------------------------------------------------

void StyleBaseImpl::checkLoaded()
{
    if(m_parent) m_parent->checkLoaded();
}

DOMString StyleBaseImpl::baseURL()
{
    // try to find the style sheet. If found look for it's url.
    // If it has none, look for the parentsheet, or the parentNode and
    // try to find out about their url
    StyleBaseImpl *b = this;
    while(b && !b->isStyleSheet())
        b = b->m_parent;

    if(!b) return DOMString();

    StyleSheetImpl *sheet = static_cast<StyleSheetImpl *>(b);
    if(!sheet->href().isNull())
        return sheet->href();

    // find parent
    if(sheet->parent()) return sheet->parent()->baseURL();

    if(!sheet->ownerNode()) return DOMString();

    DocumentImpl *doc = sheet->ownerNode()->getDocument();

    return doc->baseURL();
}

/*
 * parsing functions for stylesheets
 */

const QChar *
StyleBaseImpl::parseSpace(const QChar *curP, const QChar *endP)
{
  bool sc = false;     // possible start comment?
  bool ec = false;     // possible end comment?
  bool ic = false;     // in comment?

  while (curP < endP)
  {
      if (ic)
      {
          if (ec && (*curP == '/'))
              ic = false;
          else if (*curP == '*')
              ec = true;
          else
              ec = false;
      }
      else if (sc && (*curP == '*'))
      {
          ic = true;
      }
      else if (*curP == '/')
      {
          sc = true;
      }
      //else if (!isspace(*curP))
      else if (!(curP->isSpace()))
      {
          return(curP);
      }
      else
      {
          sc = false;
      }
      curP++;
  }

  return(0);
}

/*
 * ParseToChar
 *
 * Search for an expected character.  Deals with escaped characters,
 * quoted strings, and pairs of braces/parens/brackets.
 */
const QChar *
StyleBaseImpl::parseToChar(const QChar *curP, const QChar *endP, QChar c, bool chkws, bool endAtBlock)
{
    //kdDebug( 6080 ) << "parsetochar: \"" << QString(curP, endP-curP) << "\" searching " << c << " ws=" << chkws << endl;

    bool sq = false; /* in single quote? */
    bool dq = false; /* in double quote? */
    bool esc = false; /* escape mode? */

    while (curP < endP)
    {
        if (esc)
            esc = false;
        else if (*curP == '\\')
            esc = true;
        else if (!sq && (*curP == '"'))
            dq = !dq;
        else if (!dq && (*curP == '\''))
            sq = !sq;
        else if (!sq && !dq && *curP == c)
            return(curP);
        else if (!sq && !dq && chkws && curP->isSpace())
            return(curP);
        else if(!sq && !dq ) {
            if (*curP == '{') {
                if(endAtBlock)
                    return curP;
                curP = parseToChar(curP + 1, endP, '}', false);
                if (!curP)
                    return(0);
            } else if (*curP == '(') {
                curP = parseToChar(curP + 1, endP, ')', false);
                if (!curP)
                    return(0);
            } else if (*curP == '[') {
                curP = parseToChar(curP + 1, endP, ']', false);
                if (!curP)
                    return(0);
            }
        }
        curP++;
    }

    return(0);
}

CSSRuleImpl *
StyleBaseImpl::parseAtRule(const QChar *&curP, const QChar *endP)
{
    curP++;
    const QChar *startP = curP;
    while( *curP != ' ' && *curP != '{' && *curP != '\'')
        curP++;

    QString rule(startP, curP-startP);
    rule = rule.lower();

    //kdDebug( 6080 ) << "rule = '" << rule << "'" << endl;

    if(rule == "import")
    {
        // load stylesheet and pass it over
        curP = parseSpace(curP, endP);
        if(!curP) return 0;
        startP = curP++;
        curP = parseToChar(startP, endP, ';', true);
        // Do not allow @import statements after explicity inlined
        // declarations.  They should simply be ignored per CSS-1
        // specification section 3.0.
        if( !curP || hasInlinedDecl ) return 0;
        DOMString url = khtml::parseURL(DOMString(startP, curP - startP));
        startP = curP;
        if(*curP != ';')
            curP = parseToChar(startP, endP, ';', false, true);
        if(!curP) return 0;

        DOMString mediaList = DOMString( startP, curP - startP);
        // ### check if at the beginning of the stylesheet (no style rule
        //     before the import rule)
#ifdef CSS_DEBUG
        kdDebug( 6080 ) << "import rule = " << url.string() << ", mediaList = "
                        << mediaList.string() << endl;
#endif
        // ignore block following @import rule
        if( *curP == '{' ) {
            curP++;
            curP = parseToChar(curP, endP, '}', false);
            if(curP)
                curP++;
        }
        if(!this->isCSSStyleSheet()) return 0;

        return new CSSImportRuleImpl( this, url, mediaList );
    }
    else if(rule == "charset")
    {
        // ### invoke decoder
        startP = curP++;
        curP = parseToChar(startP, endP, ';', false);
#ifdef CSS_DEBUG
        kdDebug( 6080 ) << "charset = " << QString(startP, curP - startP) << endl;
#endif
    }
    else if(rule == "font-face")
    {
        startP = curP++;
        curP = parseToChar(startP, endP, '{', false);
        if ( !curP || curP >= endP ) return 0;
        curP++;
        curP = parseToChar(curP, endP, '}', false);
#ifdef CSS_DEBUG
        kdDebug( 6080 ) << "font rule = " << QString(startP, curP - startP) << endl;
#endif
    }
    else if(rule == "media")
    {
        startP = curP++;
        curP = parseToChar(startP, endP, '{', false);
	//qDebug("mediaList = '%s'", mediaList.latin1() );
        if ( !curP || curP >= endP ) return 0;
	DOMString mediaList = DOMString( startP, curP - startP);
        curP++;
	startP = curP;
	if ( curP >= endP ) return 0;
        curP = parseToChar(curP, endP, '}', false);
	if ( !curP || startP >= curP )
	    return 0;
#ifdef CSS_DEBUG
        kdDebug( 6080 ) << "media rule = " << QString(startP, curP - startP)
                        << ", mediaList = " << mediaList.string() << endl;
#endif
        return new CSSMediaRuleImpl( this, startP, curP, mediaList );
    }
    else if(rule == "page")
    {
        startP = curP++;
        curP = parseToChar(startP, endP, '{', false);
        if ( !curP || curP >= endP ) return 0;
        curP++;
        curP = parseToChar(curP, endP, '}', false);
#ifdef CSS_DEBUG
        kdDebug( 6080 ) << "page rule = " << QString(startP, curP - startP) << endl;
#endif
    }


    return 0;
}

static DOMString getValue( const QChar *curP, const QChar *endP, const QChar *&endVal)
{
    //QString selecString( curP, endP - curP );
    //kdDebug( 6080 ) << "getValue = \"" << selecString << "\"" << endl;
    endVal = curP;
    endVal++; // ignore first char (could be the ':' form the pseudo classes)
    while( endVal < endP && *endVal != '.' && *endVal != ':' && *endVal != '[' )
        endVal++;
    const QChar *end = endVal;
    if(endVal == endP)
        endVal = 0;
    return DOMString( curP, end - curP);
}

CSSSelector *
StyleBaseImpl::parseSelector2(const QChar *curP, const QChar *endP,
                              CSSSelector::Relation relation)
{
    CSSSelector *cs = new CSSSelector();
#ifdef CSS_DEBUG
    QString selecString( curP, endP - curP );
    kdDebug( 6080 ) << "selectString = \"" << selecString << "\"" << endl;
#endif
    const QChar *endVal = 0;

    if (*curP == '#' && (curP < endP && !((*(curP+1)).isDigit())))
    {
        cs->tag = -1;
        cs->attr = ATTR_ID;
        cs->match = CSSSelector::Exact;
        cs->value = getValue( curP+1, endP, endVal);
    }
    else if (*curP == '.' && curP < endP && ( !strictParsing || !(*(curP+1)).isDigit() ) )
    {
        cs->tag = -1;
        cs->attr = ATTR_CLASS;
        cs->match = CSSSelector::List;
        cs->value = getValue( curP+1, endP, endVal);
    }
    else if (*curP == ':'  && (curP < endP && !((*(curP+1)).isDigit())))
    {
        // pseudo attributes (:link, :hover, ...), they are case insensitive.
        cs->tag = -1;
        cs->match = CSSSelector::Pseudo;
        cs->value = getValue(curP+1, endP, endVal);
        cs->value = cs->value.implementation()->lower();
    }
    else
    {
        const QChar *startP = curP;
        QString tag;
        while (curP < endP)
        {
            if (*curP =='#' && (curP < endP && !((*(curP+1)).isDigit())))
            {
                tag = QString( startP, curP-startP );
                cs->attr = ATTR_ID;
                cs->match = CSSSelector::Exact;
                cs->value = getValue(curP+1, endP, endVal);
                break;
            }
            else if (*curP == '.' && curP < endP && ( !strictParsing || !(*(curP+1)).isDigit() ) )
            {
                tag = QString( startP, curP - startP );
                cs->attr = ATTR_CLASS;
                cs->match = CSSSelector::List;
                cs->value = getValue(curP+1, endP, endVal);
                break;
            }
            else if (*curP == ':'  && (curP < endP && !((*(curP+1)).isDigit())))
            {
                // pseudo attributes (:link, :hover, ...), they are case insensitive.
                tag = QString( startP, curP - startP );
                cs->match = CSSSelector::Pseudo;
                cs->value = getValue(curP+1, endP, endVal);
                cs->value = cs->value.implementation()->lower();
                break;
            }
            else if (*curP == '[')
            {
                tag = QString( startP, curP - startP );
                curP++;
                if ( curP >= endP ) {
                    delete cs;
                    return 0;
                }
#ifdef CSS_DEBUG
                kdDebug( 6080 ) << "tag = " << tag << endl;
#endif
                const QChar *closebracket = parseToChar(curP, endP, ']', false);
                if (!closebracket)
                {
                    kdWarning()<<"error in css: closing bracket not found!"<<endl;
		    delete cs;
                    return 0;
                }
                QString attr;
                const QChar *equal = parseToChar(curP, closebracket, '=', false);
                if(!equal)
                {
                    attr = QString( curP, closebracket - curP );
                    attr = attr.stripWhiteSpace();
#ifdef CSS_DEBUG
                    kdDebug( 6080 ) << "attr = '" << attr << "'" << endl;
#endif
                    cs->match = CSSSelector::Set;
                    endVal = closebracket + 1;
                    // ### fixme we ignore everything after [..]
                    if( endVal == endP )
                        endVal = 0;
                }
                else
                {
                    // check relation: = / ~= / |=
		    // CSS3: ^= / $= / *=
                    if(*(equal-1) == '~')
                    {
                        attr = QString( curP, equal - curP - 1 );
                        cs->match = CSSSelector::List;
                    }
                    else if(*(equal-1) == '|')
                    {
                        attr = QString( curP, equal - curP - 1 );
                        cs->match = CSSSelector::Hyphen;
                    }
                    else if(*(equal-1) == '^')
                    {
                        attr = QString( curP, equal - curP - 1);
                        cs->match = CSSSelector::Begin;
                    }
                    else if(*(equal-1) == '$')
                    {
                        attr = QString( curP, equal - curP - 1);
                        cs->match = CSSSelector::End;
                    }
                    else if(*(equal-1) == '*')
                    {
                        attr = QString( curP, equal - curP - 1);
                        cs->match = CSSSelector::Contain;
                    }
                    else
                    {
                        attr = QString(curP, equal - curP );
                        cs->match = CSSSelector::Exact;
                    }
                }
                {
                    attr = attr.stripWhiteSpace();
                    StyleBaseImpl *root = this;
                    DocumentImpl *doc = 0;
                    while (root->parent())
                        root = root->parent();
                    if (root->isCSSStyleSheet())
                        doc = static_cast<CSSStyleSheetImpl*>(root)->doc();

                    if ( doc ) {
                        if (doc->isHTMLDocument())
                            attr = attr.lower();
                        const DOMString dattr(attr);
                        cs->attr = doc->attrId(0, dattr.implementation(), false);
                    }
                    else {
                        cs->attr = khtml::getAttrID(attr.lower().ascii(), attr.length());
                        // this case should never happen - only when loading
                        // the default stylesheet - which must not contain unknown attributes
                        assert(cs->attr);
                    }
                    if (!cs->attr) {
                        delete cs;
                        return 0;
                    }
                }
                if(equal)
                {
                    equal++;
                    while(equal < endP && *equal == ' ')
                        equal++;
                    if(equal >= endP ) {
                        delete cs;
                        return 0;
                    }
                    endVal = equal;
                    bool hasQuote = false;
                    if(*equal == '\'') {
                        equal++;
                        endVal++;
                        while(endVal < endP && *endVal != '\'')
                            endVal++;
                        hasQuote = true;
                    } else if(*equal == '\"') {
                        equal++;
                        endVal++;
                        while(endVal < endP && *endVal != '\"')
                            endVal++;
                        hasQuote = true;
                    } else {
                      while(endVal < endP && *endVal != ']')
                        endVal++;
                    }
                    cs->value = DOMString(equal, endVal - equal);
                    if ( hasQuote ) {
                      while( endVal < endP - 1 && *endVal != ']' )
                        endVal++;
                    }
                    endVal++;
                    // ### fixme we ignore everything after [..]
                    if( endVal == endP )
                        endVal = 0;
                }
                break;
            }
            else
            {
                curP++;
            }
        }
        if (curP == endP)
        {
            tag = QString( startP, curP - startP );
        }
        if(tag == "*")
        {
            //kdDebug( 6080 ) << "found '*' selector" << endl;
            cs->tag = -1;
        }
        else {
            StyleBaseImpl *root = this;
            DocumentImpl *doc = 0;
            while (root->parent())
                root = root->parent();
            if (root->isCSSStyleSheet())
                doc = static_cast<CSSStyleSheetImpl*>(root)->doc();

            if ( doc ) {
                if (doc->isHTMLDocument())
                    tag = tag.lower();
                const DOMString dtag(tag);
                cs->tag = doc->tagId(0, dtag.implementation(), false);
            }
            else {
                cs->tag = khtml::getTagID(tag.lower().ascii(), tag.length());
                // this case should never happen - only when loading
                // the default stylesheet - which must not contain unknown tags
                assert(cs->tag);
            }
            if (!cs->tag) {
                delete cs;
                return 0;
            }
        }
   }
#ifdef CSS_DEBUG
   kdDebug( 6080 ) << "[Selector: tag=" << cs->tag << " Attribute=" << cs->attr << " match=" << (int)cs->match << " value=" << cs->value.string() << " specificity=" << cs->specificity() << "]" << endl;
#endif


   //stack->print();
   if( endVal ) {
       // lets be recursive
       relation = CSSSelector::SubSelector;
       CSSSelector *stack = parseSelector2(endVal, endP, relation);
       cs->tagHistory = stack;
       cs->relation = relation;
   }

   return cs;
}

CSSSelector *
StyleBaseImpl::parseSelector1(const QChar *curP, const QChar *endP)
{
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "selector1 is \'" << QString(curP, endP-curP) << "\'" << endl;
#endif

    CSSSelector *selecStack=0;

    curP = parseSpace(curP, endP);
    if (!curP)
        return(0);

    CSSSelector::Relation relation = CSSSelector::Descendant;

    const QChar *startP = curP;
    while (curP && curP <= endP)
    {
        if ((curP == endP) || curP->isSpace() || *curP == '+' || *curP == '>')
        {
            CSSSelector *newsel = parseSelector2(startP, curP, relation);
            if (!newsel) {
                delete selecStack;
                return 0;
            }
            CSSSelector *end = newsel;
            while( end->tagHistory )
                end = end->tagHistory;
            end->tagHistory = selecStack;
            end->relation = relation;
            selecStack = newsel;

            curP = parseSpace(curP, endP);
            if (!curP) {
#ifdef CSS_DEBUG
                kdDebug( 6080 ) << "selector stack is:" << endl;
                selecStack->print();
                kdDebug( 6080 ) << endl;
#endif
                return(selecStack);
            }
            relation = CSSSelector::Descendant;
            if(*curP == '+')
            {
                relation = CSSSelector::Sibling;
                curP++;
                curP = parseSpace(curP, endP);
            }
            else if(*curP == '>')
            {
#ifdef CSS_DEBUG
                kdDebug( 6080 ) << "child selector" << endl;
#endif
                relation = CSSSelector::Child;
                curP++;
                curP = parseSpace(curP, endP);
            }
            //if(selecStack)
            //    selecStack->print();
            startP = curP;
        }
        else
        {
            curP++;
        }
    }
#ifdef CSS_DEBUG
    selecStack->print();
#endif
    return(selecStack);
}

QPtrList<CSSSelector> *
StyleBaseImpl::parseSelector(const QChar *curP, const QChar *endP)
{
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "selector is \'" << QString(curP, endP-curP) << "\'" << endl;
#endif

    QPtrList<CSSSelector> *slist  = 0;
    const QChar *startP;

    while (curP < endP)
    {
        startP = curP;
        curP = parseToChar(curP, endP, ',', false);
        if (!curP)
            curP = endP;

        CSSSelector *selector = parseSelector1(startP, curP);
        if (selector)
        {
            if (!slist)
            {
                slist = new QPtrList<CSSSelector>;
                slist->setAutoDelete(true);
            }
            slist->append(selector);
        }
        else
        {
#ifdef CSS_DEBUG
            kdDebug( 6080 ) << "invalid selector" << endl;
#endif
            // invalid selector, delete
            delete slist;
            return 0;
        }
        curP++;
    }
    return slist;
}


void StyleBaseImpl::parseProperty(const QChar *curP, const QChar *endP)
{
    m_bnonCSSHint = false;
    m_bImportant = false;
    // Get rid of space in front of the declaration

    curP = parseSpace(curP, endP);
    if (!curP)
        return;

    // Search for the required colon or white space
    const QChar *colon = parseToChar(curP, endP, ':', true);
    if (!colon)
        return;

    const QString propName( curP, colon - curP );
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "Property-name = \"" << propName << "\"" << endl;
#endif

    // May have only reached white space before
    if (*colon != ':')
    {
        // Search for the required colon
        colon = parseToChar(curP, endP, ':', false);
        if (!colon)
            return;
    }
    curP = colon+1;
    // remove space in front of the value
    while(curP < endP && *curP == ' ')
        curP++;
    if ( curP >= endP )
        return;

    // search for !important
    const QChar *exclam = parseToChar(curP, endP, '!', false);
    if(exclam)
    {
        //const QChar *imp = parseSpace(exclam+1, endP);
        QString s(exclam+1, endP - exclam - 1);
        s = s.stripWhiteSpace();
        s = s.lower();
        if(s != "important")
            return;
        m_bImportant = true;
        endP = exclam;
#ifdef CSS_DEBUG
        kdDebug( 6080 ) << "important property!" << endl;
#endif
    }

    // remove space after the value;
    while (endP > curP)
    {
        //if (!isspace(*(endP-1)))
        if (!((endP-1)->isSpace()))
            break;
        endP--;
    }

#ifdef CSS_DEBUG
    QString propVal( curP , endP - curP );
    kdDebug( 6080 ) << "Property-value = \"" << propVal.latin1() << "\"" << endl;
#endif

    const struct props *propPtr = findProp(propName.lower().ascii(), propName.length());
    if (!propPtr)
    {
#ifdef CSS_DEBUG
        kdDebug( 6080 ) << "Unknown property" << propName << endl;
#endif
         return;
    }

    unsigned int numProps = m_propList->count();
    if(!parseValue(curP, endP, propPtr->id)) {
#ifdef CSS_DEBUG
        kdDebug(6080) << "invalid property, removing added properties from m_propList" << endl;
#endif
        while(m_propList->count() > numProps)
            m_propList->removeLast();
    }
}

QPtrList<CSSProperty> *StyleBaseImpl::parseProperties(const QChar *curP, const QChar *endP)
{
    m_propList = new QPtrList<CSSProperty>;
    m_propList->setAutoDelete(true);
    while (curP < endP)
    {
        const QChar *startP = curP;
        curP = parseToChar(curP, endP, ';', false);
        if (!curP)
            curP = endP;

#ifdef CSS_DEBUG
        QString propVal( startP , curP - startP );
        kdDebug( 6080 ) << "Property = \"" << propVal.latin1() << "\"" << endl;
#endif

        parseProperty(startP, curP);
        curP++;
    }
    if(!m_propList->isEmpty()) {
        return m_propList;
    } else {
#ifdef CSS_DEBUG
        kdDebug( 6080 ) << "empty property list" << endl;
#endif
        delete m_propList;
        return 0;
    }
}

static const QChar *getNext( const QChar *curP, const QChar *endP, bool &last )
{
    last = false;
    const QChar *nextP = curP;
    bool ignoreSpace = false;
    while(nextP < endP) {
	if ( *nextP == '(' ) {
	    ignoreSpace = true;
	} else if ( *nextP == ')' ) {
	    ignoreSpace = false;
	}
	if ( *nextP == ' ' && !ignoreSpace )
            return nextP;
	if ( *nextP == ';')
            break;
	nextP++;
    }
    last = true;
    return nextP;
}
// ------------------- begin font property ---------------------
/*
  Parser for the font property of CSS.  See
  http://www.w3.org/TR/REC-CSS2/fonts.html#propdef-font for details.

  Written by Jasmin Blanchette (jasmin@trolltech.com) on 2000-08-16.
*/

class FontParser {
public:
    enum { TOK_NONE, TOK_EOI, TOK_SLASH, TOK_COMMA, TOK_STRING, TOK_SYMBOL };

    QChar m_yyChar;
    QString m_yyIn, m_yyStr;
    unsigned int m_yyPos;
    int m_yyTok;
    bool strictParsing;

    QChar getChar() {
      return ( m_yyPos == m_yyIn.length() ) ? QChar('\0') : m_yyIn.unicode()[m_yyPos++];
    }

    void startTokenizer( const QString& str, bool _strictParsing ) {
      m_yyIn = str.simplifyWhiteSpace();
#ifdef CSS_DEBUG
      kdDebug( 6080 ) << "startTokenizer: [" << m_yyIn << "]" << endl;
#endif
      m_yyPos = 0;
      m_yyChar = getChar();
      strictParsing = _strictParsing;
      m_yyTok = TOK_NONE;
    }

    int getToken();

    bool match( int tok )
    {
      if ( m_yyTok == tok ) {
	m_yyTok = getToken();
	return true;
      }
      return false;
    }

    bool matchFontStyle( QString *fstyle )
    {
      if ( m_yyTok == TOK_SYMBOL ) {
	const struct css_value *cssval = findValue(m_yyStr.latin1(), m_yyStr.length());
	if (cssval) {
	  int id = cssval->id;
	  if ( id == CSS_VAL_NORMAL || id == CSS_VAL_ITALIC ||
	       id == CSS_VAL_OBLIQUE || id == CSS_VAL_INHERIT ) {
	    *fstyle = m_yyStr;
	    m_yyTok = getToken();
	    return true;
	  }
	}
      }
      return false;
    }

    bool matchFontVariant( QString *fvariant )
    {
      if ( m_yyTok == TOK_SYMBOL ) {
	const struct css_value *cssval = findValue(m_yyStr.latin1(), m_yyStr.length());
	if (cssval) {
	  int id = cssval->id;
	  if (id == CSS_VAL_NORMAL || id == CSS_VAL_SMALL_CAPS || id == CSS_VAL_INHERIT) {
	    *fvariant = m_yyStr;
	    m_yyTok = getToken();
	    return true;
	  }
        }
      }
      return false;
    }

    bool matchFontWeight( QString *fweight )
    {
      if ( m_yyTok == TOK_SYMBOL ) {
	const struct css_value *cssval = findValue(m_yyStr.latin1(), m_yyStr.length());
	if (cssval) {
	  int id = cssval->id;
	  if ((id >= CSS_VAL_NORMAL && id <= CSS_VAL_900) || id == CSS_VAL_INHERIT ) {
	    *fweight = m_yyStr;
	    m_yyTok = getToken();
	    return true;
	  }
	}
      }
      return false ;
    }

    bool matchFontSize( QString *fsize )
    {
      if ( m_yyTok == TOK_SYMBOL ) {
	*fsize = m_yyStr;
	m_yyTok = getToken();
	return true;
      }
      return false;
    }

    bool matchLineHeight( QString *lheight )
    {
      if ( m_yyTok == TOK_SYMBOL ) {
	*lheight = m_yyStr;
	m_yyTok = getToken();
	return true;
      }
      return false;
    }

    bool matchNameFamily( QString *ffamily )
    {
#ifdef CSS_DEBUG
      kdDebug( 6080 ) << "matchNameFamily: [" << *ffamily << "]" << endl;
#endif
      bool matched = false;
      if ( m_yyTok == TOK_SYMBOL || ( m_yyTok == TOK_STRING && !strictParsing ) ) {
	// accept quoted "serif" only in non strict mode.
	*ffamily = m_yyStr;
	// unquoted courier new should return courier new
	while( (m_yyTok = getToken()) == TOK_SYMBOL ) {
	  *ffamily += " " + m_yyStr;
	}
	matched = true;
      } else if ( m_yyTok == TOK_STRING ) {
          //  kdDebug( 6080 ) << "[" << m_yyStr << "]" << endl;
	const struct css_value *cssval = findValue(m_yyStr.latin1(), m_yyStr.length());
	if (!cssval || !(cssval->id >= CSS_VAL_SERIF && cssval->id <= CSS_VAL_MONOSPACE)) {
	  *ffamily = m_yyStr;
	  m_yyTok = getToken();
	  matched = true;
	}
      }
      return matched;
    }

    bool matchFontFamily( QString *ffamily )
    {
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "matchFontFamily: [" << *ffamily << "]" << endl;
#endif
      QStringList t;
      if ( !matchFontFamily( &t ) )
	return false;

      *ffamily = t.join(", ");
      return TRUE;
    }

    bool matchFontFamily ( QStringList *ffamily );
    bool matchRealFont( QString *fstyle, QString *fvariant, QString *fweight,
			QString *fsize, QString *lheight, QString *ffamily );
};

int FontParser::getToken()
{
    m_yyStr = QString::null;

    if ( m_yyChar == '\0' )
	return TOK_EOI;
    if ( m_yyChar == ' ' )
	m_yyChar = getChar();

    if ( m_yyChar == '/' ) {
	m_yyChar = getChar();
	return TOK_SLASH;
    } else if ( m_yyChar == ',' ) {
	m_yyChar = getChar();
	return TOK_COMMA;
    } else if ( m_yyChar == '"' ) {
	m_yyChar = getChar();
	while ( m_yyChar != '"' && m_yyChar != '\0' ) {
	    m_yyStr += m_yyChar;
	    m_yyChar = getChar();
	}
	m_yyChar = getChar();
	return TOK_STRING;
    } else if ( m_yyChar == '\'' ) {
	m_yyChar = getChar();
	while ( m_yyChar != '\'' && m_yyChar != '\0' ) {
	    m_yyStr += m_yyChar;
	    m_yyChar = getChar();
	}
	m_yyChar = getChar();
	return TOK_STRING;
    } else {
	while ( m_yyChar != '/' && m_yyChar != ',' && m_yyChar != '\0' && m_yyChar != ' ') {
	    m_yyStr += m_yyChar;
	    m_yyChar = getChar();
	}
	return TOK_SYMBOL;
    }
}


bool FontParser::matchFontFamily ( QStringList *ffamily )
{
      if ( m_yyTok == TOK_NONE )
	m_yyTok = getToken();
#if 0
      // ###
      if ( m_yyTok == TOK_STRING && m_yyStr == "inherit" ) {
	ffamily->clear();
	m_yyTok = getToken();
	return TRUE;
      }
#endif

      QString name;
      do {
	if ( !matchNameFamily(&name) )
	  return FALSE;
	ffamily->append( name );
      } while ( match(TOK_COMMA) );

      return true;
}

bool FontParser::matchRealFont( QString *fstyle, QString *fvariant, QString *fweight,
			QString *fsize, QString *lheight, QString *ffamily )
{
      bool metFstyle = matchFontStyle( fstyle );
      bool metFvariant = matchFontVariant( fvariant );
      matchFontWeight( fweight );
      if ( !metFstyle )
	metFstyle = matchFontStyle( fstyle );
      if ( !metFvariant )
	matchFontVariant( fvariant );
      if ( !metFstyle )
	matchFontStyle( fstyle );

      if ( !matchFontSize(fsize) )
	return FALSE;
      if ( match(TOK_SLASH) ) {
	if ( !matchLineHeight(lheight) )
	  return FALSE;
      }
      if ( !matchFontFamily(ffamily) )
	return FALSE;
      return true;
}

bool StyleBaseImpl::parseFont(const QChar *curP, const QChar *endP)
{
  QString str( curP, endP - curP );
  QString fstyle, fvariant, fweight, fsize, lheight, ffamily;

  FontParser fontParser;
  fontParser.startTokenizer( str, strictParsing );

  //kdDebug( 6080 ) << str << endl;
  const struct css_value *cssval = findValue(fontParser.m_yyIn.latin1(), fontParser.m_yyIn.length());

  if (cssval) {
      //kdDebug( 6080 ) << "System fonts requested: [" << str << "]" << endl;
    QFont sysFont;
    switch (cssval->id) {
    case CSS_VAL_MENU:
      sysFont = KGlobalSettings::menuFont();
      break;
    case CSS_VAL_CAPTION:
      sysFont = KGlobalSettings::windowTitleFont();
      break;
    case CSS_VAL_STATUS_BAR:
    case CSS_VAL_ICON:
    case CSS_VAL_MESSAGE_BOX:
    case CSS_VAL_SMALL_CAPTION:
    default:
      sysFont = KGlobalSettings::generalFont();
      break;
    }
    if (sysFont.italic()) {
      fstyle = "italic";
    } else {
      fstyle = "normal";
    }
    if (sysFont.bold()) {
      fweight = "bold";
    } else {
      fweight = "normal";
    }
    fsize = QString::number( sysFont.pixelSize() ) + "px";
    ffamily = sysFont.family();

  } else {
    fontParser.m_yyTok = fontParser.getToken();
    if (!(fontParser.matchRealFont(&fstyle, &fvariant, &fweight, &fsize, &lheight, &ffamily)))
      {
	return false;
      }
  }
//   kdDebug(6080) << "[" << fstyle << "] [" << fvariant << "] [" << fweight << "] ["
//   		<< fsize << "] / [" << lheight << "] [" << ffamily << "]" << endl;

  if(!fstyle.isNull())
    parseValue(fstyle.unicode(), fstyle.unicode()+fstyle.length(), CSS_PROP_FONT_STYLE);
  if(!fvariant.isNull())
    parseValue(fvariant.unicode(), fvariant.unicode()+fvariant.length(), CSS_PROP_FONT_VARIANT);
  if(!fweight.isNull())
    parseValue(fweight.unicode(), fweight.unicode()+fweight.length(), CSS_PROP_FONT_WEIGHT);
  if(!fsize.isNull())
    parseValue(fsize.unicode(), fsize.unicode()+fsize.length(), CSS_PROP_FONT_SIZE);
  if(!lheight.isNull())
    parseValue(lheight.unicode(), lheight.unicode()+lheight.length(), CSS_PROP_LINE_HEIGHT);
  if(!ffamily.isNull())
    parseValue(ffamily.unicode(), ffamily.unicode()+ffamily.length(), CSS_PROP_FONT_FAMILY);
  return true;
}

// ---------------- end font property --------------------------

bool StyleBaseImpl::parseValue( const QChar *curP, const QChar *endP, int propId,
                                bool important, bool nonCSSHint, QPtrList<CSSProperty> *propList)
{
  m_bImportant = important;
  m_bnonCSSHint = nonCSSHint;
  m_propList = propList;
  return parseValue(curP, endP, propId);
}

bool StyleBaseImpl::parseValue( const QChar *curP, const QChar *endP, int propId)
{
  if (curP==endP) {return 0; /* e.g.: width="" */}

  QString value(curP, endP - curP);
  value = value.lower().stripWhiteSpace();
#ifdef CSS_DEBUG
  kdDebug( 6080 ) << "id [" << getPropertyName(propId).string() << "] parseValue [" << value << "]" << endl;
#endif
  int len = value.length();
  const char *val = value.latin1();

  CSSValueImpl *parsedValue = 0;

  // We are using this so often
  const struct css_value *cssval = findValue(val, len);
  if (cssval && cssval->id == CSS_VAL_INHERIT) {
    parsedValue = new CSSInheritedValueImpl(); // inherited value
  } else {
    switch(propId)
      {
	/* The comment to the left defines all valid value of this properties as defined
     * in CSS 2, Appendix F. Property index
     */

	/* All the CSS properties are not supported by the renderer at the moment.
	 * Note that all the CSS2 Aural properties are only checked, if CSS_AURAL is defined
	 * (see parseAuralValues). As we don't support them at all this seems reasonable.
	 */

      case CSS_PROP_SIZE:                 // <length>{1,2} | auto | portrait | landscape | inherit
      case CSS_PROP_QUOTES:               // [<string> <string>]+ | none | inherit
      case CSS_PROP_TEXT_SHADOW:          // none | [<color> || <length> <length> <length>? ,]*
	//    [<color> || <length> <length> <length>?] | inherit

      case CSS_PROP_UNICODE_BIDI:         // normal | embed | bidi-override | inherit
      case CSS_PROP_WHITE_SPACE:          // normal | pre | nowrap | inherit
      case CSS_PROP_FONT_STRETCH:
        // normal | wider | narrower | ultra-condensed | extra-condensed | condensed |
        // semi-condensed |  semi-expanded | expanded | extra-expanded | ultra-expanded |
        // inherit
      case CSS_PROP_PAGE:                 // <identifier> | auto // ### CHECK
      case CSS_PROP_PAGE_BREAK_AFTER:     // auto | always | avoid | left | right | inherit
      case CSS_PROP_PAGE_BREAK_BEFORE:    // auto | always | avoid | left | right | inherit
      case CSS_PROP_PAGE_BREAK_INSIDE:    // avoid | auto | inherit
      case CSS_PROP_POSITION:             // static | relative | absolute | fixed | inherit
      case CSS_PROP_EMPTY_CELLS:          // show | hide | inherit
      case CSS_PROP_TABLE_LAYOUT:         // auto | fixed | inherit
	{
	  const struct css_value *cssval = findValue(val, len);
	  if (cssval)
	    {
	      parsedValue = new CSSPrimitiveValueImpl(cssval->id);
	    }
	  // ### To be done
	  break;
	}

      case CSS_PROP_CONTENT:              // [ <string> | <uri> | <counter> | attr(X) | open-quote |
	// close-quote | no-open-quote | no-close-quote ]+ | inherit
        {
            parsedValue = parseContent(curP,endP);
        }

      case CSS_PROP__KONQ_JS_CLIP:
      case CSS_PROP_CLIP:                 // <shape> | auto | inherit
      {
	  int i;
	  if ( cssval && cssval->id == CSS_VAL_AUTO )
	      parsedValue = new CSSPrimitiveValueImpl( cssval->id );
	  else {
	      // only shape in CSS2 is rect( top right bottom left )
	      QString str = QConstString( const_cast<QChar*>( curP ), endP - curP ).string();
	      // the CSS specs are not really clear if there should be commas in here or not. We accept both spaces and commas.
	      QChar *uc = (QChar *)str.unicode();
	      int len = str.length();
	      while( len ) {
		  if ( *uc == ',' )
		      *uc = ' ';
		  uc++;
		  len--;
	      }
	      str = str.simplifyWhiteSpace();
	      if ( str.find( "rect", 0, false ) != 0 )
		  break;
	      int pos = str.find( '(', 4 );
	      int end = str.findRev( ')' );
	      if ( end <= pos )
		  break;
	      str = str.mid( pos + 1, end - pos - 1 );
	      str = str.simplifyWhiteSpace();
	      str += " ";
	      //qDebug("rect = '%s'", str.latin1() );

	      pos = 0;
	      RectImpl *rect = new RectImpl();
	      for ( i = 0; i < 4; i++ ) {
		  int space;
		  space = str.find( ' ', pos );
		  const QChar *start = str.unicode() + pos;
		  const QChar *end = str.unicode() + space;
		  //qDebug("part: from=%d, to=%d", pos, space );
		  if ( start >= end )
		      goto cleanup;
		  CSSPrimitiveValueImpl *length = 0;
		  if ( str.find( "auto", pos, FALSE ) == pos )
		      length = new CSSPrimitiveValueImpl( CSS_VAL_AUTO );
		  else
		      length = parseUnit( start, end, LENGTH );
		  if ( !length )
		      goto cleanup;
		  switch ( i ) {
		      case 0:
			  rect->setTop( length );
			  break;
		      case 1:
			  rect->setRight( length );
			  break;
		      case 2:
			  rect->setBottom( length );
			  break;
		      case 3:
			  rect->setLeft( length );
			  break;
		  }
		  pos = space + 1;
	      }
	      parsedValue = new CSSPrimitiveValueImpl( rect );
	      //qDebug(" passed rectangle parsing");
	      break;

	  cleanup:
	      //qDebug(" rectangle parsing failed, i=%d", i);
	      delete rect;
	  }
	  break;
      }

      /* Start of supported CSS properties with validation. This is needed for parseShortHand to work
       * correctly and allows optimization in khtml::applyRule(..)
       */
      case CSS_PROP_CAPTION_SIDE:         // top | bottom | left | right | inherit
	{
	  if (cssval) {
	    int id = cssval->id;
	    if (id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT || id == CSS_VAL_TOP || id == CSS_VAL_BOTTOM) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
	    }
	  }
	  break;
	}
      case CSS_PROP_BORDER_COLLAPSE:      // collapse | separate | inherit
	{
	  if (cssval) {
	    int id = cssval->id;
	    if ( id == CSS_VAL_COLLAPSE || id == CSS_VAL_SEPARATE ) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
	    }
	  }
	  break;
	}
      case CSS_PROP_VISIBILITY:           // visible | hidden | collapse | inherit
	{
	  if (cssval) {
	    int id = cssval->id;
	    if (id == CSS_VAL_VISIBLE || id == CSS_VAL_HIDDEN || id == CSS_VAL_COLLAPSE) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
	    }
	  }
	  break;
	}
      case CSS_PROP_OVERFLOW:             // visible | hidden | scroll | auto | inherit
	{
	  if (cssval) {
	    int id = cssval->id;
	    if ( id == CSS_VAL_VISIBLE || id == CSS_VAL_HIDDEN || id == CSS_VAL_SCROLL || id == CSS_VAL_AUTO ) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
	    }
	  }
	  break;
	}
      case CSS_PROP_LIST_STYLE_POSITION:  // inside | outside | inherit
	{
	  if (cssval) {
	    int id = cssval->id;
	    if ( id == CSS_VAL_INSIDE || id == CSS_VAL_OUTSIDE ) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
	    }
	  }
	  break;
	}
      case CSS_PROP_LIST_STYLE_TYPE:
        // disc | circle | square | decimal | decimal-leading-zero | lower-roman |
        // upper-roman | lower-greek | lower-alpha | lower-latin | upper-alpha |
        // upper-latin | hebrew | armenian | georgian | cjk-ideographic | hiragana |
        // katakana | hiragana-iroha | katakana-iroha | none | inherit
	{
	  if (cssval) {
            int id = cssval->id;
            if ((id >= CSS_VAL_DISC && id <= CSS_VAL_KATAKANA_IROHA) || id == CSS_VAL_NONE) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
            }
	  }
	  break;
	}
      case CSS_PROP_DISPLAY:
        // inline | block | list-item | run-in | compact | -konq-ruler | marker | table |
        // inline-table | table-row-group | table-header-group | table-footer-group | table-row |
        // table-column-group | table-column | table-cell | table-caption | none | inherit
	{
	  if (cssval) {
            int id = cssval->id;
            if ((id >= CSS_VAL_INLINE && id <= CSS_VAL_TABLE_CAPTION) || id == CSS_VAL_NONE) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
            }
	  }
	  break;
	}
      case CSS_PROP_DIRECTION:            // ltr | rtl | inherit
	{
	  if (cssval) {
	    int id = cssval->id;
	    if ( id == CSS_VAL_LTR || id == CSS_VAL_RTL ) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
	    }
	  }
	  break;
	}
      case CSS_PROP_TEXT_TRANSFORM:       // capitalize | uppercase | lowercase | none | inherit
	{
	  if (cssval) {
	    int id = cssval->id;
	    if ((id >= CSS_VAL_CAPITALIZE && id <= CSS_VAL_LOWERCASE) || id == CSS_VAL_NONE) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
	    }
	  }
	  break;
	}
      case CSS_PROP_FLOAT:                // left | right | none | inherit + center for buggy CSS
	{
	  if (cssval) {
            int id = cssval->id;
            if (id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT
		|| id == CSS_VAL_NONE || id == CSS_VAL_CENTER) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
	    }
	  }
	  break;
	}
      case CSS_PROP_CLEAR:                // none | left | right | both | inherit
	{
	  if (cssval) {
            int id = cssval->id;
            if (id == CSS_VAL_NONE || id == CSS_VAL_LEFT
		|| id == CSS_VAL_RIGHT|| id == CSS_VAL_BOTH) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
	    }
	  }
	  break;
	}
      case CSS_PROP_TEXT_ALIGN:
    	// left | right | center | justify | konq_center | <string> | inherit
	{
	  if (cssval) {
            int id = cssval->id;
            if (id >= CSS_VAL__KONQ_AUTO && id <= CSS_VAL__KONQ_CENTER) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
	      break;
            }
	  } else {
            parsedValue = new CSSPrimitiveValueImpl(DOMString(curP, endP - curP),
			                            CSSPrimitiveValue::CSS_STRING);
	  }
	  break;
	}
      case CSS_PROP_OUTLINE_STYLE:        // <border-style> | inherit
      case CSS_PROP_BORDER_TOP_STYLE:     //// <border-style> | inherit
      case CSS_PROP_BORDER_RIGHT_STYLE:   //   Defined as:    none | hidden | dotted | dashed |
      case CSS_PROP_BORDER_BOTTOM_STYLE:  //   solid | double | groove | ridge | inset | outset
      case CSS_PROP_BORDER_LEFT_STYLE:    ////
	{
	  if (cssval) {
            int id = cssval->id;
            if (id >= CSS_VAL_NONE && id <= CSS_VAL_RIDGE) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
            }
	  }
	  break;
	}
      case CSS_PROP_FONT_WEIGHT:  // normal | bold | bolder | lighter | 100 | 200 | 300 | 400 |
	// 500 | 600 | 700 | 800 | 900 | inherit
	{
	  if (cssval) {
            int id = cssval->id;
            if (id) {
	      if (id >= CSS_VAL_NORMAL && id <= CSS_VAL_LIGHTER) {
		// Allready correct id
	      } else if (id >= CSS_VAL_100 && id <= CSS_VAL_500) {
		id = CSS_VAL_NORMAL;
	      } else if (id >= CSS_VAL_600 && id <= CSS_VAL_900) {
		id = CSS_VAL_BOLD;
	      }
	      parsedValue = new CSSPrimitiveValueImpl(id);
	    }
	  }
	  break;
	}
      case CSS_PROP_BACKGROUND_REPEAT:    // repeat | repeat-x | repeat-y | no-repeat | inherit
	{
#ifdef CSS_DEBUG_BCKGR
	  kdDebug( 6080 ) << "CSS_PROP_BACKGROUND_REPEAT: " << val << endl;
#endif
	  if (cssval) {
            int id = cssval->id;
            if ( id >= CSS_VAL_REPEAT && id <= CSS_VAL_NO_REPEAT ) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
            }
	  }
	  break;
	}
      case CSS_PROP_BACKGROUND_ATTACHMENT: // scroll | fixed
	{
#ifdef CSS_DEBUG_BCKGR
	  kdDebug( 6080 ) << "CSS_PROP_BACKGROUND_ATTACHEMENT: " << val << endl;
#endif
	  if (cssval) {
            int id = cssval->id;
            if ( id == CSS_VAL_SCROLL || id == CSS_VAL_FIXED ) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
            }
	  }
	  break;
	}
      case CSS_PROP_BACKGROUND_POSITION:
	{
#ifdef CSS_DEBUG_BCKGR
	  kdDebug( 6080 ) << "CSS_PROP_BACKGROUND_POSITION: " << val << endl;
#endif
	  /* Problem: center is ambigous
	   * In case of 'center' center defines X and Y coords
	   * In case of 'center top', center defines the Y coord
	   * in case of 'center left', center defines the X coord
	   */
	  bool isLast;
	  const QChar* nextP = getNext(curP, endP, isLast);
	  QConstString property1(const_cast<QChar*>( curP ), nextP - curP);
	  const struct css_value *cssval1 = findValue( property1.string().ascii(),
						       property1.string().length());
	  if ( !cssval1 ) {
            int properties[2] = { CSS_PROP_BACKGROUND_POSITION_X,
				  CSS_PROP_BACKGROUND_POSITION_Y };
            return parseShortHand(curP, endP, properties, 2);
	  }
	  const struct css_value *cssval2 = 0;
#ifdef CSS_DEBUG
	  kdDebug( 6080 ) << "prop 1: [" << property1.string() << "]" << " isLast: " << isLast <<  endl;
#endif
	  if ( !isLast) {
            curP = nextP+1;
            nextP = getNext(curP, endP, isLast);
            QConstString property2(const_cast<QChar*>( curP ), nextP - curP);
            cssval2 = findValue( property2.string().ascii(), property2.string().length());
#ifdef CSS_DEBUG
            kdDebug( 6080 ) << "prop 2: [" << property2.string() << "]" << " isLast: " << isLast <<  endl;
#endif
	  }

	  int valX = -1;
	  int valY = -1;
	  int id1 = cssval1 ? cssval1->id : -1;
	  int id2 = cssval2 ? cssval2->id : CSS_VAL_CENTER;
	  // id1 will influence X and id2 will influence Y
	  if ( id2 == CSS_VAL_LEFT || id2 == CSS_VAL_RIGHT ||
	       id1 == CSS_VAL_TOP  || id1 == CSS_VAL_BOTTOM) {
            int h = id1; id1 = id2; id2 = h;
	  }

	  switch( id1 ) {
	  case CSS_VAL_LEFT:   valX = 0;   break;
	  case CSS_VAL_CENTER: valX = 50;  break;
	  case CSS_VAL_RIGHT:  valX = 100; break;
	  default: break;
	  }

	  switch ( id2 ) {
	  case CSS_VAL_TOP:    valY = 0;   break;
	  case CSS_VAL_CENTER: valY = 50;  break;
	  case CSS_VAL_BOTTOM: valY = 100; break;
	  default: break;
	  }

#ifdef CSS_DEBUG
	  kdDebug( 6080 ) << "valX: " << valX << " valY: " << valY <<  endl;
#endif
	  /* CSS 14.2
	   * Keywords cannot be combined with percentage values or length values.
	   * -> No mix between keywords and other units.
	   */
	  if (valX !=-1 && valY !=-1) {
            setParsedValue( CSS_PROP_BACKGROUND_POSITION_X,
			    new CSSPrimitiveValueImpl(valX, CSSPrimitiveValue::CSS_PERCENTAGE));
	    setParsedValue( CSS_PROP_BACKGROUND_POSITION_Y,
			    new CSSPrimitiveValueImpl(valY, CSSPrimitiveValue::CSS_PERCENTAGE));
	    return true;
	  }
	  break;
	}
      case CSS_PROP_BACKGROUND_POSITION_X:
      case CSS_PROP_BACKGROUND_POSITION_Y:
	{
#ifdef CSS_DEBUG
	  kdDebug( 6080 ) << "CSS_PROP_BACKGROUND_POSITION_{X|Y}: " << val << endl;
#endif
	  parsedValue = parseUnit(curP, endP, PERCENT | NUMBER | LENGTH);
	  break;
	}
      case CSS_PROP_BORDER_SPACING:
	{
	  // ### should be able to have two values
	  parsedValue = parseUnit(curP, endP, LENGTH | NONNEGATIVE);
	  break;
	}
      case CSS_PROP_OUTLINE_COLOR:        // <color> | invert | inherit
	{
#ifdef CSS_DEBUG
	  kdDebug( 6080 ) << "CSS_PROP_OUTLINE_COLOR: " << val << endl;
#endif
	  // outline has "invert" as additional keyword. we handle
	  // it as invalid color and add a special case during rendering
	  if (cssval && cssval->id == CSS_VAL_INVERT) {
            parsedValue = new CSSPrimitiveValueImpl( QColor() );
            break;
	  }
	  // Break is explictly missing, looking for <color>
	}
      case CSS_PROP_BACKGROUND_COLOR:     // <color> | transparent | inherit
	{
#ifdef CSS_DEBUG_BCKGR
	  kdDebug( 6080 ) << "CSS_PROP_BACKGROUND_COLOR: " << val << endl;
#endif
	  if (cssval && cssval->id == CSS_VAL_TRANSPARENT) {
            parsedValue = new CSSPrimitiveValueImpl( QColor() );
            break;
	  }
	  // Break is explictly missing, looking for <color>
	}
      case CSS_PROP_COLOR:                // <color> | inherit
      case CSS_PROP_BORDER_TOP_COLOR:     // <color> | inherit
      case CSS_PROP_BORDER_RIGHT_COLOR:   // <color> | inherit
      case CSS_PROP_BORDER_BOTTOM_COLOR:  // <color> | inherit
      case CSS_PROP_BORDER_LEFT_COLOR:    // <color> | inherit
      case CSS_PROP_TEXT_DECORATION_COLOR:        //
      case CSS_PROP_SCROLLBAR_FACE_COLOR:         // IE5.5
      case CSS_PROP_SCROLLBAR_SHADOW_COLOR:       // IE5.5
      case CSS_PROP_SCROLLBAR_HIGHLIGHT_COLOR:    // IE5.5
      case CSS_PROP_SCROLLBAR_3DLIGHT_COLOR:      // IE5.5
      case CSS_PROP_SCROLLBAR_DARKSHADOW_COLOR:   // IE5.5
      case CSS_PROP_SCROLLBAR_TRACK_COLOR:        // IE5.5
      case CSS_PROP_SCROLLBAR_ARROW_COLOR:        // IE5.5
	{
	  const QString val2( value.stripWhiteSpace() );
	  //kdDebug(6080) << "parsing color " << val2 << endl;
	  QColor c;
	  khtml::setNamedColor(c, val2);
	  if(!c.isValid() && (val2 != "transparent" ) && !val2.isEmpty() ) return false;
	  //kdDebug( 6080 ) << "color is: " << c.red() << ", " << c.green() << ", " << c.blue() << endl;
	  parsedValue = new CSSPrimitiveValueImpl(c);
	  break;
	}
      case CSS_PROP_BACKGROUND_IMAGE:     // <uri> | none | inherit
#ifdef CSS_DEBUG_BCKGR
	{
	  kdDebug( 6080 ) << "CSS_PROP_BACKGROUND_IMAGE: " << val << endl;
	}
#endif
      case CSS_PROP_CURSOR:
    	// [ [<uri> ,]* [ auto | crosshair | default | pointer | move | e-resize | ne-resize |
	// nw-resize | // n-resize | se-resize | sw-resize | s-resize | w-resize | text |
	// wait | help ] ] | inherit
	{
	  if (cssval) {
            int id = cssval->id;
            if (id >= CSS_VAL_AUTO && id <= CSS_VAL_HELP) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
	      break;
	    }
	  } else {
	    // Break is explictly missing, looking for <uri>
	    // ### Only supports parsing the first uri
	  }
	}
      case CSS_PROP_LIST_STYLE_IMAGE:     // <uri> | none | inherit
	{
	  if (cssval && cssval->id == CSS_VAL_NONE)
	    {
	      parsedValue = new CSSImageValueImpl();
#ifdef CSS_DEBUG
	      kdDebug( 6080 ) << "empty image " << endl;
#endif
	    } else {
	      const QString str(value.stripWhiteSpace()); // ### Optimize
	      if (str.left(4).lower() == "url(") {
                  DOMString value(curP, endP - curP);
                  value = khtml::parseURL(value);
                  if (!value.isEmpty())
                    parsedValue = new CSSImageValueImpl(DOMString(KURL(baseURL().string(), value.string()).url()), this);
#ifdef CSS_DEBUG
		kdDebug( 6080 ) << "image, url=" << value.string() << " base=" << baseURL().string() << endl;
#endif
	      }
	    }
	  break;
	}
      case CSS_PROP_OUTLINE_WIDTH:        // <border-width> | inherit
      case CSS_PROP_BORDER_TOP_WIDTH:     //// <border-width> | inherit
      case CSS_PROP_BORDER_RIGHT_WIDTH:   //   Which is defined as
      case CSS_PROP_BORDER_BOTTOM_WIDTH:  //   thin | medium | thick | <length>
      case CSS_PROP_BORDER_LEFT_WIDTH:    ////
	{
	  if (cssval) {
            int id = cssval->id;
            if (id == CSS_VAL_THIN || id == CSS_VAL_MEDIUM || id == CSS_VAL_THICK) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
            }
	  } else {
            parsedValue = parseUnit(curP, endP, LENGTH|NONNEGATIVE);
	  }
	  break;
	}
      case CSS_PROP_MARKER_OFFSET:        // <length> | auto | inherit
	{
	  if (cssval && cssval->id == CSS_VAL_AUTO) {
            parsedValue = new CSSPrimitiveValueImpl(cssval->id);
	  } else {
            parsedValue = parseUnit(curP, endP, LENGTH);
	  }
	  break;
	}
      case CSS_PROP_LETTER_SPACING:       // normal | <length> | inherit
      case CSS_PROP_WORD_SPACING:         // normal | <length> | inherit
	{
	  if (cssval) {
            if (cssval->id == CSS_VAL_NORMAL) {
	      parsedValue = new CSSPrimitiveValueImpl(cssval->id);
            }
	  } else {
            parsedValue = parseUnit(curP, endP, LENGTH);
	  }
	  break;
	}
      case CSS_PROP_PADDING_TOP:          //// <padding-width> | inherit
      case CSS_PROP_PADDING_RIGHT:        //   Which is defined as
      case CSS_PROP_PADDING_BOTTOM:       //   <length> | <percentage>
      case CSS_PROP_PADDING_LEFT:         ////
	{
	  parsedValue = parseUnit(curP, endP, LENGTH | PERCENT|NONNEGATIVE);
	  break;
	}
      case CSS_PROP_TEXT_INDENT:          // <length> | <percentage> | inherit
      case CSS_PROP_MIN_HEIGHT:           // <length> | <percentage> | inherit
      case CSS_PROP_MIN_WIDTH:            // <length> | <percentage> | inherit
	{
	  parsedValue = parseUnit(curP, endP, LENGTH | PERCENT);
	  break;
	}
      case CSS_PROP_FONT_SIZE:
    	// <absolute-size> | <relative-size> | <length> | <percentage> | inherit
	{
	  if (cssval) {
            int id = cssval->id;
            if (id >= CSS_VAL_XX_SMALL && id <= CSS_VAL_LARGER) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
	      break;
            }
	  } else {
            parsedValue = parseUnit(curP, endP, LENGTH | PERCENT | NONNEGATIVE);
	  }
	  break;
	}
      case CSS_PROP_FONT_STYLE:           // normal | italic | oblique | inherit
	{
	  if (cssval) {
            int id = cssval->id;
            if ( id == CSS_VAL_NORMAL || id == CSS_VAL_ITALIC || id == CSS_VAL_OBLIQUE) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
            }
	  }
	  break;
	}
      case CSS_PROP_FONT_VARIANT:         // normal | small-caps | inherit
	{
	  if (cssval) {
            int id = cssval->id;
            if ( id == CSS_VAL_NORMAL || id == CSS_VAL_SMALL_CAPS) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
            }
	  }
	  break;
	}
      case CSS_PROP_VERTICAL_ALIGN:
    	// baseline | sub | super | top | text-top | middle | bottom | text-bottom |
	// <percentage> | <length> | inherit
	{
	  if (cssval) {
            int id = cssval->id;
            if ( id >= CSS_VAL_BASELINE && id <= CSS_VAL__KONQ_BASELINE_MIDDLE ) {
	      parsedValue = new CSSPrimitiveValueImpl(id);
            }
	  } else {
            parsedValue = parseUnit(curP, endP, LENGTH | PERCENT );
	  }
	  break;
	}
      case CSS_PROP_MAX_HEIGHT:           // <length> | <percentage> | none | inherit
      case CSS_PROP_MAX_WIDTH:            // <length> | <percentage> | none | inherit
	{
	  if (cssval && cssval->id == CSS_VAL_NONE) {
            parsedValue = new CSSPrimitiveValueImpl(cssval->id);
	  } else {
            parsedValue = parseUnit(curP, endP, LENGTH | PERCENT );
	  }
	  break;
	}
      case CSS_PROP_HEIGHT:               // <length> | <percentage> | auto | inherit
      case CSS_PROP_WIDTH:                // <length> | <percentage> | auto | inherit
	{
	  if (cssval && cssval->id == CSS_VAL_AUTO) {
            parsedValue = new CSSPrimitiveValueImpl(cssval->id);
	  } else {
            parsedValue = parseUnit(curP, endP, LENGTH | PERCENT | NONNEGATIVE );
	  }
	  break;
	}
      case CSS_PROP_BOTTOM:               // <length> | <percentage> | auto | inherit
      case CSS_PROP_LEFT:                 // <length> | <percentage> | auto | inherit
      case CSS_PROP_RIGHT:                // <length> | <percentage> | auto | inherit
      case CSS_PROP_TOP:                  // <length> | <percentage> | auto | inherit
      case CSS_PROP_MARGIN_TOP:           //// <margin-width> | inherit
      case CSS_PROP_MARGIN_RIGHT:         //   Which is defined as
      case CSS_PROP_MARGIN_BOTTOM:        //   <length> | <percentage> | auto | inherit
      case CSS_PROP_MARGIN_LEFT:          ////
	{
	  if (cssval && cssval->id == CSS_VAL_AUTO) {
            parsedValue = new CSSPrimitiveValueImpl(cssval->id);
	  } else {
            parsedValue = parseUnit(curP, endP, LENGTH | PERCENT );
	  }
	  break;
	}
      case CSS_PROP_FONT_SIZE_ADJUST:     // <number> | none | inherit
	// ### not supported later on
	{
	  if (cssval && cssval->id == CSS_VAL_NONE) {
            parsedValue = new CSSPrimitiveValueImpl(cssval->id);
	  } else {
            parsedValue = parseUnit(curP, endP, NUMBER);
	  }
	  break;
	}
      case CSS_PROP_Z_INDEX:              // auto | <integer> | inherit
	{
	  if (cssval && cssval->id == CSS_VAL_AUTO) {
            parsedValue = new CSSPrimitiveValueImpl(cssval->id);
            break;
	  }
	  // break explicitly missing, looking for <number>
	}
      case CSS_PROP_ORPHANS:              // <integer> | inherit
      case CSS_PROP_WIDOWS:               // <integer> | inherit
	// ### not supported later on
	{
	  parsedValue = parseUnit(curP, endP, INTEGER);
	  break;
	}
      case CSS_PROP_LINE_HEIGHT:          // normal | <number> | <length> | <percentage> | inherit
	{
	  if (cssval && cssval->id == CSS_VAL_NORMAL) {
            parsedValue = new CSSPrimitiveValueImpl(cssval->id);
	  } else {
            parsedValue = parseUnit(curP, endP, NUMBER | LENGTH | PERCENT | NONNEGATIVE);
	  }
	  break;
	}
      case CSS_PROP_COUNTER_INCREMENT:    // [ <identifier> <integer>? ]+ | none | inherit
      case CSS_PROP_COUNTER_RESET:        // [ <identifier> <integer>? ]+ | none | inherit
	{
	  if (cssval && cssval->id == CSS_VAL_NONE) {
            parsedValue = new CSSPrimitiveValueImpl(cssval->id);
	  } else {
            CSSValueListImpl *list = new CSSValueListImpl;
            int pos=0, pos2;
            while( 1 )
	      {
                pos2 = value.find(',', pos);
                QString face = value.mid(pos, pos2-pos);
                face = face.stripWhiteSpace();
                if(face.length() == 0) break;
                // ### single quoted is missing...
                if(face[0] == '\"') face.remove(0, 1);
                if(face[face.length()-1] == '\"') face = face.left(face.length()-1);
                //kdDebug( 6080 ) << "found face '" << face << "'" << endl;
                list->append(new CSSPrimitiveValueImpl(DOMString(face), CSSPrimitiveValue::CSS_STRING));
                pos = pos2 + 1;
                if(pos2 == -1) break;
	      }
            //kdDebug( 6080 ) << "got " << list->length() << " faces" << endl;
            if(list->length())
	      parsedValue = list;
            else
	      delete list;
            break;
	  }
	}
      case CSS_PROP_FONT_FAMILY:
    	// [[ <family-name> | <generic-family> ],]* [<family-name> | <generic-family>] | inherit
	{
	  // css2 compatible parsing...
	  FontParser fp;
	  fp.startTokenizer( value, strictParsing );
	  QStringList families;
	  if ( !fp.matchFontFamily( &families ) )
            return false;
          CSSValueListImpl *list = new CSSValueListImpl;
	  for ( QStringList::Iterator it = families.begin(); it != families.end(); ++it ) {
            if( *it != QString::null ) {
	      list->append( new FontFamilyValueImpl(*it) );
	      //kdDebug() << "StyleBaseImpl::parsefont: family='" << *it << "'" << endl;
            }
	  }
	  //kdDebug( 6080 ) << "got " << list->length() << " faces" << endl;
	  if(list->length())
            parsedValue = list;
	  else
            delete list;
	  break;
	}
      case CSS_PROP_TEXT_DECORATION:
    	// none | [ underline || overline || line-through || blink ] | inherit
	{
	    if (cssval && cssval->id == CSS_VAL_NONE) {
	      parsedValue = new CSSPrimitiveValueImpl(cssval->id);
	    } else {
	      CSSValueListImpl *list = new CSSValueListImpl;
	      value.simplifyWhiteSpace();
	      //kdDebug( 6080 ) << "text-decoration: '" << value << "'" << endl;
	      int pos=0, pos2;
	      while( 1 )
		{
		  pos2 = value.find(' ', pos);
		  QString decoration = value.mid(pos, pos2-pos);
		  decoration = decoration.stripWhiteSpace();
		  //kdDebug( 6080 ) << "found decoration '" << decoration << "'" << endl;
		  const struct css_value *cssval = findValue(decoration.lower().ascii(),
							     decoration.length());
		  if (cssval) {
		    list->append(new CSSPrimitiveValueImpl(cssval->id));
		  }
		  pos = pos2 + 1;
		  if(pos2 == -1) break;
		}
	      //kdDebug( 6080 ) << "got " << list->length() << "d decorations" << endl;
	      if(list->length()) {
                parsedValue = list;
	      } else {
                delete list;
	      }
	    }
	  break;
	}
      case CSS_PROP__KONQ_FLOW_MODE:
      {
	  if (cssval->id==CSS_VAL__KONQ_NORMAL || cssval->id==CSS_VAL__KONQ_AROUND_FLOATS)
              parsedValue = new CSSPrimitiveValueImpl(cssval->id);
	  break;
      }
      /* shorthand properties */
      case CSS_PROP_BACKGROUND:
    	// ['background-color' || 'background-image' ||'background-repeat' ||
	// 'background-attachment' || 'background-position'] | inherit
	{
#ifdef CSS_DEBUG_BCKGR
	  kdDebug(6080) << "CSS_PROP_BACKGROUND" << endl;
#endif
	  const int properties[5] = { CSS_PROP_BACKGROUND_IMAGE, CSS_PROP_BACKGROUND_REPEAT,
                                      CSS_PROP_BACKGROUND_ATTACHMENT, CSS_PROP_BACKGROUND_POSITION,
                                      CSS_PROP_BACKGROUND_COLOR };
	  return parseShortHand(curP, endP, properties, 5);

	  //return parseBackground(curP, endP);
	}
      case CSS_PROP_BORDER:
 	// [ 'border-width' || 'border-style' || <color> ] | inherit
	{
	  const int properties[3] = { CSS_PROP_BORDER_WIDTH, CSS_PROP_BORDER_STYLE,
				      CSS_PROP_BORDER_COLOR };
	  return parseShortHand(curP, endP, properties, 3);
	}
      case CSS_PROP_BORDER_TOP:
    	// [ 'border-top-width' || 'border-style' || <color> ] | inherit
	{
	  const int properties[3] = { CSS_PROP_BORDER_TOP_WIDTH, CSS_PROP_BORDER_TOP_STYLE,
				      CSS_PROP_BORDER_TOP_COLOR};
	  return parseShortHand(curP, endP, properties, 3);
	}
      case CSS_PROP_BORDER_RIGHT:
    	// [ 'border-right-width' || 'border-style' || <color> ] | inherit
	{
	  const int properties[3] = { CSS_PROP_BORDER_RIGHT_WIDTH, CSS_PROP_BORDER_RIGHT_STYLE,
				      CSS_PROP_BORDER_RIGHT_COLOR };
	  return parseShortHand(curP, endP, properties, 3);
	}
      case CSS_PROP_BORDER_BOTTOM:
    	// [ 'border-bottom-width' || 'border-style' || <color> ] | inherit
	{
	  const int properties[3] = { CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_PROP_BORDER_BOTTOM_STYLE,
				      CSS_PROP_BORDER_BOTTOM_COLOR };
	  return parseShortHand(curP, endP, properties, 3);
	}
      case CSS_PROP_BORDER_LEFT:
    	// [ 'border-left-width' || 'border-style' || <color> ] | inherit
	{
	  const int properties[3] = { CSS_PROP_BORDER_LEFT_WIDTH, CSS_PROP_BORDER_LEFT_STYLE,
				      CSS_PROP_BORDER_LEFT_COLOR };
	  return parseShortHand(curP, endP, properties, 3);
	}
      case CSS_PROP_OUTLINE:
    	// [ 'outline-color' || 'outline-style' || 'outline-width' ] | inherit
	{
	  const int properties[3] = { CSS_PROP_OUTLINE_WIDTH, CSS_PROP_OUTLINE_STYLE,
                                      CSS_PROP_OUTLINE_COLOR };
	  return parseShortHand(curP, endP, properties, 3);
	}
      case CSS_PROP_BORDER_COLOR:
    	// <color>{1,4} | transparent | inherit
	{
	  const struct css_value *cssval = findValue(val, len);
	  if (cssval && cssval->id == CSS_VAL_TRANSPARENT)
	    {
	      // set border colors to invalid
	      parsedValue = new CSSPrimitiveValueImpl(CSS_VAL_TRANSPARENT);
	      break;
	    }
	  const int properties[4] = { CSS_PROP_BORDER_TOP_COLOR, CSS_PROP_BORDER_RIGHT_COLOR,
				      CSS_PROP_BORDER_BOTTOM_COLOR, CSS_PROP_BORDER_LEFT_COLOR };
	  return parse4Values(curP, endP, properties);
	}
      case CSS_PROP_BORDER_WIDTH:
    	// <border-width>{1,4} | inherit
	{
	  const int properties[4] = { CSS_PROP_BORDER_TOP_WIDTH, CSS_PROP_BORDER_RIGHT_WIDTH,
				      CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_PROP_BORDER_LEFT_WIDTH };
	  return parse4Values(curP, endP, properties);
	}
      case CSS_PROP_BORDER_STYLE:
    	// <border-style>{1,4} | inherit
	{
	  const int properties[4] = { CSS_PROP_BORDER_TOP_STYLE, CSS_PROP_BORDER_RIGHT_STYLE,
				      CSS_PROP_BORDER_BOTTOM_STYLE, CSS_PROP_BORDER_LEFT_STYLE };
	  return parse4Values(curP, endP, properties);
	}
      case CSS_PROP_MARGIN:
    	// <margin-width>{1,4} | inherit
	{
	  const int properties[4] = { CSS_PROP_MARGIN_TOP, CSS_PROP_MARGIN_RIGHT,
				      CSS_PROP_MARGIN_BOTTOM, CSS_PROP_MARGIN_LEFT };
	  return parse4Values(curP, endP, properties);
	}
      case CSS_PROP_PADDING:
    	// <padding-width>{1,4} | inherit
	{
	  const int properties[4] = { CSS_PROP_PADDING_TOP, CSS_PROP_PADDING_RIGHT,
				      CSS_PROP_PADDING_BOTTOM, CSS_PROP_PADDING_LEFT };
	  return parse4Values(curP, endP, properties);
	}
      case CSS_PROP_FONT:
    	// [ [ 'font-style' || 'font-variant' || 'font-weight' ]? 'font-size' [ / 'line-height' ]?
	// 'font-family' ] | caption | icon | menu | message-box | small-caption | status-bar | inherit
	{
	  return parseFont(curP, endP);
	}
      case CSS_PROP_LIST_STYLE:
	{
	  const int properties[3] = { CSS_PROP_LIST_STYLE_TYPE, CSS_PROP_LIST_STYLE_POSITION,
				      CSS_PROP_LIST_STYLE_IMAGE };
	  return  parseShortHand(curP, endP, properties, 3);
	}

      default:
	{
#ifdef CSS_DEBUG
	  kdDebug( 6080 ) << "illegal or CSS2 Aural property: " << val << endl;
#endif
	}
      }
  }
  if ( parsedValue ) {
    setParsedValue(propId, parsedValue);
    return true;
  } else {
#ifndef CSS_AURAL
    return false;
#endif
#ifdef CSS_AURAL
    return parseAuralValue(curP, endP, propId);
#endif
  }
}

#ifdef CSS_AURAL
/* parseAuralValue */
bool StyleBaseImpl::parseAuralValue( const QChar *curP, const QChar *endP, int propId )
{
    QString value(curP, endP - curP);
    value = value.lower();
    const char *val = value.ascii();

    CSSValueImpl *parsedValue = 0;
    kdDebug( 6080 ) << "parseAuralValue: " << value << " val: " << val <<  endl;

    /* AURAL Properies */
    switch(propId)
    {
    case CSS_PROP_AZIMUTH:              // <angle> | [[ left-side | far-left | left | center-left | center |
                                        // center-right | right | far-right | right-side ] || behind ] |
                                        // leftwards | rightwards | inherit
    case CSS_PROP_PAUSE_AFTER:          // <time> | <percentage> | inherit
    case CSS_PROP_PAUSE_BEFORE:         // <time> | <percentage> | inherit
    case CSS_PROP_PAUSE:                // [ [<time> | <percentage>]{1,2} ] | inherit
    case CSS_PROP_PLAY_DURING:          // <uri> mix? repeat? | auto | none | inherit
    case CSS_PROP_VOICE_FAMILY:         // [[<specific-voice> | <generic-voice> ],]*
                                        // [<specific-voice> | <generic-voice> ] | inherit
    {
      // ### TO BE DONE
        break;
    }
    case CSS_PROP_CUE:                  // [ 'cue-before' || 'cue-after' ] | inherit
    {
        const int properties[2] = {
                CSS_PROP_CUE_BEFORE,
                CSS_PROP_CUE_AFTER};
        return parse2Values(curP, endP, properties);
    }
    case CSS_PROP_CUE_AFTER:            // <uri> | none | inherit
    case CSS_PROP_CUE_BEFORE:           // <uri> | none | inherit
    {
        const struct css_value *cssval = findValue(val, endP - curP);
        if (cssval) {
            if (cssval->id == CSS_VAL_NONE) {
                parsedValue = new CSSPrimitiveValueImpl(cssval->id);
            }
        } else {
            DOMString value(curP, endP - curP);
            value = khtml::parseURL(value);
            parsedValue = new CSSPrimitiveValueImpl(
                DOMString(KURL(baseURL(), value).url()),
                CSSPrimitiveValue::CSS_URI);
        }
        break;
    }
    case CSS_PROP_ELEVATION:            // <angle> | below | level | above | higher | lower
                                        // | inherit
    {
        const struct css_value *cssval = findValue(val, endP - curP);
        if (cssval) {
            int id = cssval->id;
            if (id == CSS_VAL_BELOW || id == CSS_VAL_LEVEL || id == CSS_VAL_ABOVE ||
                id == CSS_VAL_HIGHER || id == CSS_VAL_LOWER) {
                parsedValue = new CSSPrimitiveValueImpl(id);
                break;
            }
        }
        parsedValue = parseUnit(curP, endP, ANGLE );
        break;
    }
    case CSS_PROP_SPEECH_RATE:          // <number> | x-slow | slow | medium | fast |
                                        // x-fast | faster | slower | inherit
    {
        const struct css_value *cssval = findValue(val, endP - curP);
        if (cssval) {
            int id = cssval->id;
            if (id == CSS_VAL_X_SLOW || id == CSS_VAL_SLOW || id == CSS_VAL_MEDIUM ||
                id == CSS_VAL_FAST || id == CSS_VAL_X_FAST || id == CSS_VAL_FASTER ||
                id == CSS_VAL_SLOWER) {
                parsedValue = new CSSPrimitiveValueImpl(id);
                break;
            }
        } else {
          parsedValue = parseUnit(curP, endP, NUMBER );
        }
        break;
    }
    case CSS_PROP_VOLUME:               // <number> | <percentage> | silent | x-soft | soft |
                                        // medium | loud | x-loud | inherit
    {
        const struct css_value *cssval = findValue(val, endP - curP);
        if (cssval) {
            int id = cssval->id;
            if (id == CSS_VAL_SILENT || id == CSS_VAL_X_SOFT || id == CSS_VAL_SOFT ||
                id == CSS_VAL_MEDIUM || id == CSS_VAL_X_LOUD || id == CSS_VAL_X_LOUD) {
                parsedValue = new CSSPrimitiveValueImpl(id);
            }
        } else {
            parsedValue = parseUnit(curP, endP, PERCENT | NUMBER);
        }
        break;
    }
    case CSS_PROP_PITCH:                 // <frequency> | x-low | low | medium | high | x-high | inherit
    {
        const struct css_value *cssval = findValue(val, endP - curP);
        if (cssval) {
            int id = cssval->id;
            if (id == CSS_VAL_X_LOW || id == CSS_VAL_LOW || id == CSS_VAL_MEDIUM ||
                id == CSS_VAL_HIGH || id == CSS_VAL_X_HIGH ) {
                parsedValue = new CSSPrimitiveValueImpl(id);
            }
        } else {
            parsedValue = parseUnit(curP, endP, FREQUENCY);
        }
        break;
    }
    case CSS_PROP_SPEAK:                // normal | none | spell-out | inherit
    case CSS_PROP_SPEAK_HEADER:         // once | always | inherit
    case CSS_PROP_SPEAK_NUMERAL:        // digits | continuous | inherit
    case CSS_PROP_SPEAK_PUNCTUATION:    // code | none | inherit
    {
        const struct css_value *cssval = findValue(val, endP - curP);
        if (cssval)
        {
            parsedValue = new CSSPrimitiveValueImpl(cssval->id);
        }
        break;
    }
    case CSS_PROP_PITCH_RANGE:          // <number> | inherit
    case CSS_PROP_RICHNESS:             // <number> | inherit
    case CSS_PROP_STRESS:               // <number> | inherit
    {
        parsedValue = parseUnit(curP, endP, NUMBER);
        break;
    }
    default:
    {
        kdDebug( 6080 ) << "illegal property: " << val << endl;
    }
   }
   if ( parsedValue ) {
     setParsedValue( propId, parsedValue );
        return true;
   }
   return false;
}
#endif

void StyleBaseImpl::setParsedValue(int propId, const CSSValueImpl *parsedValue,
				   bool important, bool nonCSSHint, QPtrList<CSSProperty> *propList)
{
  m_bImportant = important;
  m_bnonCSSHint = nonCSSHint;
  m_propList = propList;
  setParsedValue( propId, parsedValue);
}

void StyleBaseImpl::setParsedValue( int propId, const CSSValueImpl *parsedValue)
{
    QPtrListIterator<CSSProperty> propIt(*m_propList);
    propIt.toLast(); // just remove the top one - not sure what should happen if we have multiple instances of the property
    while (propIt.current() &&
           ( propIt.current()->m_id != propId || propIt.current()->nonCSSHint != m_bnonCSSHint ||
             propIt.current()->m_bImportant != m_bImportant) )
        --propIt;
    if (propIt.current())
        m_propList->removeRef(propIt.current());

    CSSProperty *prop = new CSSProperty();
    prop->m_id = propId;
    prop->setValue((CSSValueImpl *) parsedValue);
    prop->m_bImportant = m_bImportant;
    prop->nonCSSHint = m_bnonCSSHint;

    m_propList->append(prop);
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "added property: " << getPropertyName(propId).string()
                    // non implemented yet << ", value: " << parsedValue->cssText().string()
                    << " important: " << prop->m_bImportant
                    << " nonCSS: " << prop->nonCSSHint << endl;
#endif
}

bool StyleBaseImpl::parseShortHand(const QChar *curP, const QChar *endP, const int *properties, int num)
{
    /* We try to match as many properties as possible
     * We setup an array of booleans to mark which property has been found,
     * and we try to search for properties until it makes no longer any sense
     */

    bool isLast = false;
    bool foundAnything = false;
    bool fnd[6]; //Trust me ;)
    for( int i = 0; i < num; i++ )
    	fnd[i] = false;

#ifdef CSS_DEBUG
    kdDebug(6080) << "PSH: parsing \"" << QString(curP, endP - curP) << "\" num=" << num << endl;
#endif

    for (int j = 0; j < num; ++j) {
        const QChar *nextP = getNext( curP, endP, isLast );
        //kdDebug(6080) << "in PSH: \"" << QString(curP, nextP - curP) << "\"" << endl;
        foundAnything = false;
        for (int propIndex = 0; propIndex < num; ++propIndex) {
            if (!fnd[propIndex]) {
		// We have to tread this seperately
		//kdDebug(6080) << "LOOKING FOR: " << getPropertyName(properties[propIndex]).string() << endl;
		bool found = false;
		if (!isLast && properties[propIndex] == CSS_PROP_BACKGROUND_POSITION)
		    found = parseBackgroundPosition(curP, nextP, endP);
		else
		    found = parseValue(curP, nextP, properties[propIndex]);

		if (found) {
		    fnd[propIndex] = foundAnything = true;
#ifdef CSS_DEBUG
		    kdDebug(6080) << "FOUND: " << getPropertyName(properties[propIndex]).string() << ": "
				  << QString(curP, nextP - curP)  << endl;
#endif
                    break;
		}
	    }
	}
        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!foundAnything)
            return foundAnything;

        do {
            nextP++;
            curP = nextP;

            // oh, less parameteres than we expected
            if (curP >= endP)
                return foundAnything;
        } while (curP->isSpace());
    }
    return foundAnything;
}

/*
 * Problem (again): the ambiguity of 'background-position'
 * from: http://www.w3.org/Style/CSS/Test/current/sec537.htm

	BODY {background: green url(oransqr.gif) repeat-x center top fixed;}
	.one {background: lime url(oransqr.gif) repeat-y 100% 0%;}
	.two {background: lime url(oransqr.gif) repeat-y center top;}
	.three {background: lime url(oransqr.gif) repeat-x left top;}
*/

bool StyleBaseImpl::parseBackgroundPosition(const QChar *curP, const QChar *&nextP, const QChar *endP)
{
    // We first need to check if the property has two values.
    // if this fails we try one value only.

    const QChar *bckgrNextP = nextP;
    while (bckgrNextP->isSpace() && bckgrNextP < endP) { bckgrNextP++; }
    bool dummy;
    bckgrNextP = getNext(bckgrNextP, endP, dummy);
    //kdDebug(6080) << "BKCGR: 2: \"" << QString(curP, bckgrNextP - curP) << "\"" << endl;

    bool found = parseValue(curP, bckgrNextP, CSS_PROP_BACKGROUND_POSITION);
    if (!found) {
	// We have not found a pair of Background-Positions, see if we have a single value

    	//kdDebug(6080) << "BKCGR: Single: \"" << QString(curP, nextP - curP) << "\"" << endl;
    	found = parseValue(curP, nextP, CSS_PROP_BACKGROUND_POSITION);
    } else {
	// Moving on
	nextP = bckgrNextP;
    }
    //kdDebug(6080) << "found background property!" << endl;
    return found;
}


CSSValueImpl* StyleBaseImpl::parseContent(const QChar *curP, const QChar *endP)
{
    CSSValueListImpl* values = new CSSValueListImpl();


    while (curP < endP) {
        const QChar *nextP = curP;
        bool esc = false;
        bool sq = false;
        bool dq = false;
        while ( nextP < endP ) {
            if (esc)
                esc = false;
            else if (*nextP == '\\')
                esc = true;
            else if (!sq && (*nextP == '"')) {
                if (dq) break;
                dq = true;
            }
            else if (!dq && (*nextP == '\'')) {
                if (sq) break;
                sq = true;
            }
            else if (!sq && !dq && nextP->isSpace())
                break;
            nextP++;
        }
        QString str = QConstString(curP, nextP-curP).string();
        CSSValueImpl* parsedValue=0;
        if (str.startsWith("url("))
        {
            // url
	    DOMString value(curP, endP - curP);
	    value = khtml::parseURL(value);
            parsedValue = new CSSImageValueImpl(
            DOMString(KURL(baseURL().string(), value.string()).url()), this);
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "content, url=" << value.string() << " base=" << baseURL().string() << endl;
#endif
        }
        else if (str.startsWith("attr("))
        {
            // attr
        }
        else if (str.startsWith("counter("))
        {
            // counter
        }
        else if (str.startsWith("open-quote"))
        {
            // open-quote
        }
        else if (str.startsWith("close-quote"))
        {
            // open-quote
        }
        else if (str.startsWith("no-open-quote"))
        {
            // no-open-quote
        }
        else if (str.startsWith("no-close-quote"))
        {
            // no-close-quote
        }
        else if (str.length() && (str[0] == '\'' || str[0] == '"'))
        {
            // string
            int l = str.length();
            QString strstr;
            for (int i = 0; i < l; ++i) {
                if (i < l - 1 && str[i] == '\\') {
                    if (str[i+1] == 'a')
                        strstr += '\n';
                    else
                        strstr += str[i+1];
                    ++i;
                    continue;
                }
                strstr += str[i];
            }
            parsedValue = new CSSPrimitiveValueImpl(DOMString(strstr), CSSPrimitiveValue::CSS_STRING);
        }
        if (parsedValue)
            values->append(parsedValue);

        // skip over whitespace
        for (curP = ++nextP ; curP < endP && curP->isSpace(); ++curP)
            ;
    }
    return values;
}


QPtrList<QChar> StyleBaseImpl::splitShorthandProperties(const QChar *curP, const QChar *endP)
{
    bool last = false;
    QPtrList<QChar> list;
    while(!last) {
        const QChar *nextP = curP;
        while(!(nextP->isSpace())) {
            nextP++;
            if(nextP >= endP) {
                last = true;
                break;
            }
        }
        list.append(curP);
        list.append(nextP);
        if ( last ) break;
        while(nextP->isSpace()) { // skip over WS between tokens
            nextP++;
            curP = nextP;
            if(curP >= endP) {
                last = true;
                break;
            }
        }
    }
    return list;
}

#ifdef CSS_AURAL
// used for shorthand properties xxx{1,2}
bool StyleBaseImpl::parse2Values( const QChar *curP, const QChar *endP, const int *properties)
{
    QPtrList<QChar> list = splitShorthandProperties(curP, endP);
    switch(list.count())
    {
    case 2:
        if(!parseValue(list.at(0), list.at(1), properties[0])) return false;
        setParsedValue(properties[1], m_propList->last()->value() );
        return true;
    case 4:
        if(!parseValue(list.at(0), list.at(1), properties[0])) return false;
        if(!parseValue(list.at(2), list.at(3), properties[1])) return false;
        return true;
    default:
        return false;
    }
}
#endif

// used for shorthand properties xxx{1,4}
bool StyleBaseImpl::parse4Values( const QChar *curP, const QChar *endP, const int *properties)
{
    /* From the CSS 2 specs, 8.3
     * If there is only one value, it applies to all sides. If there are two values, the top and
     * bottom margins are set to the first value and the right and left margins are set to the second.
     * If there are three values, the top is set to the first value, the left and right are set to the
     * second, and the bottom is set to the third. If there are four values, they apply to the top,
     * right, bottom, and left, respectively.
     */

    QPtrList<QChar> list = splitShorthandProperties(curP, endP);
    switch(list.count())
    {
    case 2:
        if(!parseValue(list.at(0), list.at(1), properties[0])) return false;
        setParsedValue(properties[1], m_propList->last()->value());
        setParsedValue(properties[2], m_propList->last()->value());
        setParsedValue(properties[3], m_propList->last()->value());
        return true;
    case 4:
        if(!parseValue(list.at(0), list.at(1), properties[0])) return false;
        setParsedValue(properties[2], m_propList->last()->value());
        if(!parseValue(list.at(2), list.at(3), properties[1])) return false;
        setParsedValue(properties[3], m_propList->last()->value());
        return true;
    case 6:
        if(!parseValue(list.at(0), list.at(1), properties[0])) return false;
        if(!parseValue(list.at(2), list.at(3), properties[1])) return false;
        setParsedValue(properties[3], m_propList->last()->value());
        if(!parseValue(list.at(4), list.at(5), properties[2])) return false;
        return true;
    case 8:
        if(!parseValue(list.at(0), list.at(1), properties[0])) return false;
        if(!parseValue(list.at(2), list.at(3), properties[1])) return false;
        if(!parseValue(list.at(4), list.at(5), properties[2])) return false;
        if(!parseValue(list.at(6), list.at(7), properties[3])) return false;
        return true;
    default:
        return false;
    }
}

CSSPrimitiveValueImpl *
StyleBaseImpl::parseUnit(const QChar * curP, const QChar *endP, int allowedUnits)
{
    // Everything should be lowercase -> preprocess
    //assert(QString(curP, endP-curP).lower()==QString(curP,endP-curP));

    if (curP==endP || *curP=='"') {return 0; /* e.g.: width=""  length="ffffff" */}

    endP--;
    while(*endP == ' ' && endP > curP) endP--;
    const QChar *split = endP;
    // splt up number and unit
    while( (*split < '0' || *split > '9') && *split != '.' && split > curP)
        split--;
    split++;

    QString s(curP, split-curP);

    bool isInt = false;
    if(s.find('.') == -1) isInt = true;

    bool ok;
    float value = s.toFloat(&ok);
    if ( !ok || (value < 0 && (allowedUnits & NONNEGATIVE)) )
	return 0;

    if(split > endP) // no unit
    {
        if(allowedUnits & NUMBER)
            return new CSSPrimitiveValueImpl(value, CSSPrimitiveValue::CSS_NUMBER);

        if(allowedUnits & INTEGER && isInt) // ### DOM CSS doesn't seem to define something for integer
            return new CSSPrimitiveValueImpl(value, CSSPrimitiveValue::CSS_NUMBER);

        // ### according to the css specs only 0 is allowed without unit.
        // there are however too many web pages out there using CSS without units
        // cause ie and ns allow them. We do so if the document is not using a strict dtd
        if(( allowedUnits & LENGTH ) && (value == 0 || !strictParsing) )
            return new CSSPrimitiveValueImpl(value, CSSPrimitiveValue::CSS_PX);

        return 0;
    }

    CSSPrimitiveValue::UnitTypes type = CSSPrimitiveValue::CSS_UNKNOWN;
    StyleBaseImpl::Units unit = StyleBaseImpl::UNKNOWN;

    switch(split->latin1())
    {
    case '%':
        type = CSSPrimitiveValue::CSS_PERCENTAGE;
        unit =StyleBaseImpl:: PERCENT;
        break;
    case 'e':
        split++;
        if(split > endP) break;
        switch(split->latin1())
	{
	case 'm':
            type = CSSPrimitiveValue::CSS_EMS;
            unit = StyleBaseImpl::LENGTH;
            break;
	case 'x':
            type = CSSPrimitiveValue::CSS_EXS;
            unit = StyleBaseImpl::LENGTH;
            break;
        }
        break;
    case 'p':
        split++;
        if(split > endP) break;
	switch(split->latin1())
	{
	case 'x':
            type = CSSPrimitiveValue::CSS_PX;
            unit = StyleBaseImpl::LENGTH;
            break;
	case 't':
            type = CSSPrimitiveValue::CSS_PT;
            unit = StyleBaseImpl::LENGTH;
            break;
	case 'c':
            type = CSSPrimitiveValue::CSS_PC;
            unit = StyleBaseImpl::LENGTH;
            break;
        }
        break;
    case 'c':
        split++;
        if(split > endP) break;
        if(split->latin1() == 'm')
        {
            type = CSSPrimitiveValue::CSS_CM;
            unit = StyleBaseImpl::LENGTH;
        }
        break;
    case 'm':
        split++;
        if(split > endP) break;
        switch(split->latin1())
	{
	case 'm':
            type = CSSPrimitiveValue::CSS_MM;
            unit = StyleBaseImpl::LENGTH;
            break;
	case 's':
            type = CSSPrimitiveValue::CSS_MS;
            unit = StyleBaseImpl::TIME;
            break;
        }
        break;
    case 'i':
        split++;
        if(split > endP) break;
        if(split->latin1() == 'n')
        {
            type = CSSPrimitiveValue::CSS_IN;
            unit = StyleBaseImpl::LENGTH;
        }
        break;
    case 'd':
        type = CSSPrimitiveValue::CSS_DEG;
        unit = StyleBaseImpl::ANGLE;
        break;
    case 'r':
        type = CSSPrimitiveValue::CSS_RAD;
        unit = StyleBaseImpl::ANGLE;
        break;
    case 'g':
        type = CSSPrimitiveValue::CSS_GRAD;
        unit = StyleBaseImpl::ANGLE;
        break;
    case 's':
        type = CSSPrimitiveValue::CSS_S;
        unit = StyleBaseImpl::TIME;
        break;
    case 'h':
        type = CSSPrimitiveValue::CSS_HZ;
        unit = StyleBaseImpl::FREQUENCY;
        break;
    case 'k':
        type = CSSPrimitiveValue::CSS_KHZ;
        unit = StyleBaseImpl::FREQUENCY;
        break;
    }

    if(unit & allowedUnits)
    {
#ifdef CSS_DEBUG
        kdDebug( 6080 ) << "found allowed number " << value << ", unit " << type << endl;
#endif
        return new CSSPrimitiveValueImpl(value, type);
    }

    return 0;
}

CSSRuleImpl *
StyleBaseImpl::parseStyleRule(const QChar *&curP, const QChar *endP)
{
    //kdDebug( 6080 ) << "style rule is \'" << QString(curP, endP-curP) << "\'" << endl;

    const QChar *startP;
    QPtrList<CSSSelector> *slist;
    QPtrList<CSSProperty> *plist;

    startP = curP;
    curP = parseToChar(startP, endP, '{', false);
    if (!curP)
        return(0);
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "selector is \'" << QString(startP, curP-startP) << "\'" << endl;
#endif

    slist = parseSelector(startP, curP );

    curP++; // need to get past the '{' from above

    startP = curP;
    curP = parseToChar(startP, endP, '}', false);

    if (!curP)
    {
        delete slist;
        return(0);
    }
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "rules are \'" << QString(startP, curP-startP) << "\'" << endl;
#endif

    plist = parseProperties(startP, curP );

    curP++; // need to get past the '}' from above

    if (!plist || !slist)
    {
        // Useless rule
        delete slist;
        delete plist;
#ifdef CSS_DEBUG
        kdDebug( 6080 ) << "bad style rule" << endl;
#endif
        return 0;
    }

    // return the newly created rule
    CSSStyleRuleImpl *rule = new CSSStyleRuleImpl(this);
    CSSStyleDeclarationImpl *decl = new CSSStyleDeclarationImpl(rule, plist);

    rule->setSelector(slist);
    rule->setDeclaration(decl);
    // ### set selector and value
    return rule;
}

CSSRuleImpl *
StyleBaseImpl::parseRule(const QChar *&curP, const QChar *endP)
{
    const QChar *startP;

    curP = parseSpace( curP, endP );

    if (!strictParsing) {
	// allow ; between rules (not part of spec)
	while (curP && (curP->isSpace() || *curP == ';'))
	    curP++;
    }

    startP = curP;
    CSSRuleImpl *rule = 0;

    if(!curP) return 0;
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "parse rule: current = " << curP->latin1() << endl;
#endif

    if (*curP == '@' && curP != endP && ( (curP+1)->isLetter() || (curP+1)->unicode() > 0xa0 )  )
    {
        rule = parseAtRule(curP, endP);
    }
    else
    {
        rule = parseStyleRule(curP, endP);
        if( rule )
            hasInlinedDecl = true;  // set flag to true iff we have a valid inlined decl.
    }

    if(curP) curP++;
    return rule;
}

/* Generate a sort of Normal Form for CSS.
 * Remove comments, it is guaranteed that there will not be more then one space between
 * tokens and all the tokens within curly braces are lower case (except text
 * within quotes and url tags). Space is replaced with QChar(' ') and removed where
 * it's not necessary.
 *
 * 4.1.3 Characters and case
 *
 * The following rules always hold:
 *
 *  All CSS style sheets are case-insensitive, except for parts that are not under
 *  the control of CSS. For example, the case-sensitivity of values of the HTML
 *  attributes "id" and "class", of font names, and of URIs lies outside the scope
 *  of this specification. Note in particular that element names are case-insensitive
 *  in HTML, but case-sensitive in XML.
 */

const QString StyleBaseImpl::preprocess(const QString &str, bool justOneRule)
{
  // ### use DOMString here to avoid coversions
  QString processed;

  bool sq = false;	// Within single quote
  bool dq = false;	// Within double quote
  bool bracket = false;	// Within brackets, e.g. url(ThisIsStupid)
  bool comment = false; // Within comment
  bool skipgarbage = !justOneRule; // skip <!-- and ---> only in specifc places
  bool firstChar = false; // Beginning of comment either /* or */
  bool space = true;    // Last token was space
  int curlyBracket = 0; // Within curlyBrackets -> lower
  hasInlinedDecl = false; // reset the inlined decl. flag

  const QChar *ch = str.unicode();
  const QChar *last = ch + str.length();

#ifdef CSS_DEBUG
  kdDebug(6080) << "---Before---" << endl;
  kdDebug(6080) << str << endl;
  float orgLength = str.length();
  kdDebug(6080) << "Length: " << orgLength << endl;
#endif

  while(ch < last) {
//       qDebug("current: *%s*, sq=%d dq=%d b=%d c=%d fC=%d space=%d cB=%d sg=%d",
//              QConstString(ch, kMin(last-ch, 10)).string().latin1(), sq, dq, bracket, comment, firstChar, space, curlyBracket, skipgarbage);
    if( !comment && !sq && *ch == '"' ) {
      dq = !dq;
      processed += *ch;
      space = skipgarbage = false;
    } else if ( !comment && !dq && *ch == '\'') {
      skipgarbage = sq;
      sq = !sq;
      processed += *ch;
      space = false;
    } else if ( !comment && !dq && !sq && *ch == '(') {
      bracket = true;
      processed += *ch;
      space = true;  // Explictly true
      skipgarbage = false;
    } else if ( !comment && !dq && !sq && *ch == ')') {
      bracket = false;
      processed += *ch;
      processed += QChar(' '); // Adding a space after this token
      space = true;
      skipgarbage = false;
    } else if ( !comment && !dq && !sq && *ch == '{') {
      ++curlyBracket;
      processed += *ch;
      space = true;  // Explictly true
      skipgarbage = true;
    } else if ( !comment && !dq && !sq && *ch == '}') {
      --curlyBracket;
      processed += *ch;
      processed += QChar(' '); // Adding a space after this token
      space = true;
      skipgarbage = true;
    } else if ( !comment && skipgarbage && !dq && !sq && (*ch == '-') && ((ch+2) < last)  /* SGML Comment */
                && (*(ch+1) == '-') && (*(ch+2) == '>')) {
        ch += 2; // skip -->
    } else if ( !comment && skipgarbage && !dq && !sq && (*ch == '<') && ((ch+3) < last)  /* SGML Comment */
                && (*(ch+1) == '!') && (*(ch+2) == '-') && (*(ch+3) == '-')) {
        ch += 3; // skip <!--
    } else if ( comment ) {
      if ( firstChar && *ch == '/' ) {
          comment = false;
          firstChar = false;
          skipgarbage = true;
      } else {
	firstChar = ( *ch == '*' );
      }
    } else if ( !sq && !dq && !bracket ) {
      // check for comment
      if ( firstChar ) {
	if ( *ch == '*' ) {
	  comment = true;
	} else {
	  processed += '/';
	  if (curlyBracket > 0) {
	    processed += ch->lower();
	  } else {
	    processed += *ch;
	  }
	  space = ch->isSpace();
	}
	firstChar = false;
      } else if ( *ch == '/' ) {
	firstChar = true; // Slash added only if next is not '*'
      } else if ( *ch == ',' || *ch == ';') {
	processed += *ch;
	processed += QChar(' '); // Adding a space after these tokens
	space = true;
             skipgarbage = true;
      } else {
          if (!ch->isSpace())
              skipgarbage = false;
	goto addChar;
      }
    } else {
        skipgarbage = ch->isSpace();
      goto addChar;
    }
  end:
    ++ch;
  }

#ifdef CSS_DEBUG
  kdDebug(6080) << "---After ---" << endl;
  kdDebug(6080) << "[" << processed << "]" << endl;
  kdDebug(6080) << "------------" << endl;
  kdDebug(6080) << "Length: " << processed.length() << ", reduced size by: "
		<< 100.0 - (100.0 * (processed.length()/orgLength)) << "%" << endl;
  kdDebug(6080) << "------------" << endl;
#endif

  return processed;

 addChar:
  if ( !sq && !dq && !bracket ) {
    if (!(space && ch->isSpace())) { // Don't add more than one space
      if (ch->isSpace()) {
	processed += QChar(' '); // Normalize whitespace
      } else {
	if (curlyBracket > 0 || justOneRule) {
	  processed += ch->lower();
	} else {
	  processed += *ch;
	}
      }
    }
    space = ch->isSpace();
  } else {
    processed += *ch; // We're within quotes or brackets, leave untouched
  }
  goto end;
}

// ------------------------------------------------------------------------------

StyleListImpl::~StyleListImpl()
{
    StyleBaseImpl *n;

    if(!m_lstChildren) return;

    for( n = m_lstChildren->first(); n != 0; n = m_lstChildren->next() )
    {
        n->setParent(0);
        if( !n->refCount() ) delete n;
    }
    delete m_lstChildren;
}

// --------------------------------------------------------------------------------

void CSSSelector::print(void)
{
    kdDebug( 6080 ) << "[Selector: tag = " <<       tag << ", attr = \"" << attr << "\", match = \"" << match << "\" value = \"" << value.string().latin1() << "\" relation = " << (int)relation << endl;
    if ( tagHistory )
        tagHistory->print();
}

unsigned int CSSSelector::specificity()
{
    if ( nonCSSHint )
        return 0;

    int s = (tag != -1);
    switch(match)
    {
    case Exact:
        if(attr == ATTR_ID)
        {
            s += 0x10000;
            break;
        }
    case Set:
    case List:
    case Hyphen:
    case Pseudo:
    case Contain:
    case Begin:
    case End:
        s += 0x100;
    case None:
        break;
    }
    if(tagHistory)
        s += tagHistory->specificity();
    // make sure it doesn't overflow
    return s & 0xffffff;
}

bool CSSSelector::operator == ( const CSSSelector &other )
{
    const CSSSelector *sel1 = this;
    const CSSSelector *sel2 = &other;

    while ( sel1 && sel2 ) {
	if ( sel1->tag != sel2->tag || sel1->attr != sel2->attr ||
	     sel1->relation != sel2->relation || sel1->match != sel2->match ||
	     sel1->nonCSSHint != sel2->nonCSSHint ||
	     sel1->value != sel2->value )
	    return false;
	sel1 = sel1->tagHistory;
	sel2 = sel2->tagHistory;
    }
    if ( sel1 || sel2 )
	return false;
    return true;
}

DOMString CSSSelector::selectorText() const
{
    DOMString str;
    const CSSSelector* cs = this;
    if ( cs->tag == -1 && cs->attr == ATTR_ID && cs->match == CSSSelector::Exact )
    {
        str = "#";
        str += cs->value;
    }
    else if ( cs->tag == -1 && cs->attr == ATTR_CLASS && cs->match == CSSSelector::List )
    {
        str = ".";
        str += cs->value;
    }
    else if ( cs->tag == -1 && cs->match == CSSSelector::Pseudo )
    {
        str = ":";
        str += cs->value;
    }
    else
    {
        if ( cs->tag == -1 )
            str = "*";
        else
            str = getTagName( cs->tag );
        if ( cs->attr == ATTR_ID && cs->match == CSSSelector::Exact )
        {
            str += "#";
            str += cs->value;
        }
        else if ( cs->attr == ATTR_CLASS && cs->match == CSSSelector::List )
        {
            str += ".";
            str += cs->value;
        }
        else if ( cs->match == CSSSelector::Pseudo )
        {
            str += ":";
            str += cs->value;
        }
        // optional attribute
        if ( cs->attr ) {
            DOMString attrName = getAttrName( cs->attr );
            str += "[";
            str += attrName;
            switch (cs->match) {
            case CSSSelector::Exact:
                str += "=";
                break;
            case CSSSelector::Set:
                str += " "; /// ## correct?
                       break;
            case CSSSelector::List:
                str += "~=";
                break;
            case CSSSelector::Hyphen:
                str += "|=";
                break;
            case CSSSelector::Begin:
                str += "^=";
                break;
            case CSSSelector::End:
                str += "$=";
                break;
            case CSSSelector::Contain:
                str += "*=";
                break;
            default:
                kdWarning(6080) << "Unhandled case in CSSStyleRuleImpl::selectorText : match=" << cs->match << endl;
            }
            str += "\"";
            str += cs->value;
            str += "\"]";
        }
    }
    if ( cs->tagHistory ) {
        DOMString tagHistoryText = cs->tagHistory->selectorText();
        if ( cs->relation == Sibling )
            str = tagHistoryText + " + " + str;
        else if ( cs->relation == Child )
            str = tagHistoryText + " > " + str;
        else if ( cs->relation == SubSelector )
            str += tagHistoryText; // the ":" is provided by selectorText()
        else // Descendant
            str = tagHistoryText + " " + str;
    }
    return str;
}

// ----------------------------------------------------------------------------

