/**
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
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
 * $Id$
 */
#include "cssstyleselector.h"
#include "rendering/render_style.h"
#include "css_stylesheetimpl.h"
#include "css_ruleimpl.h"
#include "css_valueimpl.h"
#include "csshelper.h"
#include "html/html_documentimpl.h"
#include "xml/dom_elementimpl.h"
#include "dom/css_rule.h"
#include "dom/css_value.h"
#include "khtml_factory.h"
using namespace khtml;
using namespace DOM;

#include "cssproperties.h"
#include "cssvalues.h"

#include "misc/khtmllayout.h"
#include "khtml_settings.h"
#include "misc/htmlhashes.h"
#include "misc/helper.h"

#include "khtmlview.h"
#include "khtml_part.h"
#include "khtml_settings.h"

#include <kstddirs.h>
#include <kcharsets.h>
#include <kglobal.h>
#include <qfile.h>
#include <qfontdatabase.h>
#include <qfontinfo.h>
#include <qvaluelist.h>
#include <qstring.h>
#include <kdebug.h>
#include <kurl.h>
#include <qdatetime.h>
#include <assert.h>
#include <qpaintdevicemetrics.h>
#include <qintcache.h>

CSSStyleSelectorList *CSSStyleSelector::defaultStyle = 0;
CSSStyleSheetImpl *CSSStyleSelector::defaultSheet = 0;

enum PseudoState { PseudoUnknown, PseudoNone, PseudoLink, PseudoVisited};
static PseudoState pseudoState;

static int dynamicState;
static RenderStyle::PseudoId dynamicPseudo;
static int usedDynamicStates;
static int selectorDynamicState;
static CSSStyleSelector::Encodedurl *encodedurl;


CSSStyleSelector::CSSStyleSelector(DocumentImpl * doc)
{
    strictParsing = doc->parseMode() == DocumentImpl::Strict;
    if(!defaultStyle) loadDefaultStyle(doc->view()?doc->view()->part()->settings():0);

    selectors = 0;
    selectorCache = 0;
    properties = 0;
    userStyle = 0;
    userSheet = 0;


    if ( !doc->userStyleSheet().isEmpty() ) {
        userSheet = new DOM::CSSStyleSheetImpl((DOM::CSSStyleSheetImpl *)0);
        userSheet->parseString( DOMString( doc->userStyleSheet() ) );

        userStyle = new CSSStyleSelectorList();
        userStyle->append(userSheet);
    }

    // add stylesheets from document
    authorStyle = new CSSStyleSelectorList();
    StyleSheetListImpl* ss = doc->styleSheets();
    for ( QListIterator<StyleSheetImpl> it( ss->styleSheets ); it.current(); ++it )
 	    authorStyle->append( it.current());

    buildLists();

    //kdDebug( 6080 ) << "number of style sheets in document " << authorStyleSheets.count() << endl;
    //kdDebug( 6080 ) << "CSSStyleSelector: author style has " << authorStyle->count() << " elements"<< endl;

    KURL u = doc->baseURL().string();
    u.setQuery( QString::null );
    u.setRef( QString::null );
    encodedurl.file = u.url();
    int pos = encodedurl.file.findRev('/');
    encodedurl.path = encodedurl.file;
    if ( pos > 0 ) {
	encodedurl.path.truncate( pos );
	encodedurl.path += '/';
    }
    u.setPath( QString::null );
    encodedurl.host = u.url();

    //kdDebug() << "CSSStyleSelector::CSSStyleSelector encoded url " << encodedurl.path << endl;
}

CSSStyleSelector::CSSStyleSelector(StyleSheetImpl *sheet)
{
    if(!defaultStyle) loadDefaultStyle();

    authorStyle = new CSSStyleSelectorList();
    authorStyle->append(sheet);
}

CSSStyleSelector::~CSSStyleSelector()
{
    clearLists();
    delete authorStyle;
    delete userStyle;
    delete userSheet;
}

void CSSStyleSelector::addSheet(StyleSheetImpl *sheet)
{
    authorStyle->append(sheet);
}

void CSSStyleSelector::loadDefaultStyle(const KHTMLSettings *s)
{
    if(defaultStyle) return;

    QFile f(locate( "data", "khtml/css/html4.css" ) );
    f.open(IO_ReadOnly);

    QCString file( f.size()+1 );
    int readbytes = f.readBlock( file.data(), f.size() );
    f.close();
    if ( readbytes >= 0 )
        file[readbytes] = '\0';

    QString style = QString::fromLatin1( file.data() );
    if(s)
	style += s->settingsToCSS();
    DOMString str(style);

    defaultSheet = new DOM::CSSStyleSheetImpl((DOM::CSSStyleSheetImpl *)0);
    defaultSheet->parseString( str );

    defaultStyle = new CSSStyleSelectorList();
    defaultStyle->append(defaultSheet);
    //kdDebug() << "CSSStyleSelector: default style has " << defaultStyle->count() << " elements"<< endl;
}

void CSSStyleSelector::clear()
{
    delete defaultStyle;
    delete defaultSheet;
    defaultStyle = 0;
    defaultSheet = 0;
}

static bool strictParsing;

RenderStyle *CSSStyleSelector::styleForElement(ElementImpl *e, int state)
{
    // this is a bit hacky, but who cares....
    ::strictParsing = strictParsing;
    ::dynamicState = state;
    ::usedDynamicStates = StyleSelector::None;
    ::encodedurl = &encodedurl;
    ::pseudoState = PseudoUnknown;

    CSSOrderedPropertyList *propsToApply = new CSSOrderedPropertyList;
    CSSOrderedPropertyList *pseudoProps = new CSSOrderedPropertyList;

    // try to sort out most style rules as early as possible.
    int id = e->id();
    int smatch = 0;
    int schecked = 0;

    for ( unsigned int i = 0; i < selectors_size; i++ ) {
	int tag = selectors[i]->tag;
	if ( id == tag || tag == -1 ) {
	    ++schecked;

	    checkSelector( i, e );

	    if ( selectorCache[i].state == Applies ) {
		++smatch;

		//qDebug("adding property" );
		for ( unsigned int p = 0; p < selectorCache[i].props_size; p += 2 )
		    for ( unsigned int j = 0; j < (unsigned int )selectorCache[i].props[p+1]; ++j )
			static_cast<QList<CSSOrderedProperty>*>(propsToApply)->append( properties[selectorCache[i].props[p]+j] );
	    } else if ( selectorCache[i].state == AppliesPseudo ) {
		for ( unsigned int p = 0; p < selectorCache[i].props_size; p += 2 )
		    for ( unsigned int j = 0; j < (unsigned int) selectorCache[i].props[p+1]; ++j ) {
			static_cast<QList<CSSOrderedProperty>*>(pseudoProps)->append(  properties[selectorCache[i].props[p]+j] );
			properties[selectorCache[i].props[p]+j]->pseudoId = (RenderStyle::PseudoId) selectors[i]->pseudoId;
		    }
	    }
	}
	else
	    selectorCache[i].state = Invalid;

    }
    //qDebug( "styleForElement( %s )", e->tagName().string().latin1() );
    //qDebug( "%d selectors, %d checked,  %d match,  %d properties ( of %d )",
    //selectors_size, schecked, smatch, propsToApply->count(), properties_size );

    // inline style declarations, after all others. non css hints
    // count as author rules, and come before all other style sheets, see hack in append()
    if(e->styleRules())
	addInlineDeclarations( e->styleRules(), propsToApply );

    propsToApply->sort();
    pseudoProps->sort();

    RenderStyle* style = new RenderStyle();
    if(e->parentNode()) {
        assert(e->parentNode()->style() != 0);
        style->inheritFrom(e->parentNode()->style());
    }

    //qDebug("applying properties, count=%d", propsToApply->count() );

    // we can't apply style rules without a view(). This
    // tends to happen on delayed destruction of widget Renderobjects
    KHTMLView* v = e->ownerDocument()->view();
    if ( v && v->part() ) {
        if ( propsToApply->count() != 0 ) {
            CSSOrderedProperty *ordprop = propsToApply->first();
            while( ordprop ) {
                //qDebug("property %d has spec %x", ordprop->prop->m_id, ordprop->priority );
                applyRule( style, ordprop->prop, e );
                ordprop = propsToApply->next();
            }
        }

        if ( pseudoProps->count() != 0 ) {
            //qDebug("%d applying %d pseudo props", e->id(), pseudoProps->count() );
            CSSOrderedProperty *ordprop = pseudoProps->first();
            while( ordprop ) {
                RenderStyle *pseudoStyle;
                pseudoStyle = style->addPseudoStyle(ordprop->pseudoId);

                if ( pseudoStyle )
                    applyRule(pseudoStyle, ordprop->prop, e);
                ordprop = pseudoProps->next();
            }
        }
    }

    if ( usedDynamicStates & StyleSelector::Hover )
	style->setHasHover();
    if ( usedDynamicStates & StyleSelector::Focus )
	style->setHasFocus();
    if ( usedDynamicStates & StyleSelector::Active )
	style->setHasActive();

    delete propsToApply;
    delete pseudoProps;

    return style;
}

void CSSStyleSelector::addInlineDeclarations(DOM::CSSStyleDeclarationImpl *decl,
					     CSSOrderedPropertyList *list )
{
    QList<CSSProperty> *values = decl->values();
    if(!values) return;
    int len = values->count();

    if ( inlineProps.size() < (uint)len )
	inlineProps.resize( len+1 );

    CSSOrderedProperty *array = (CSSOrderedProperty *)inlineProps.data();
    for(int i = 0; i < len; i++)
    {
        CSSProperty *prop = values->at(i);
	Source source = Inline;

        if( prop->m_bImportant ) source = InlineImportant;
	if( prop->nonCSSHint ) source = NonCSSHint;

	bool first;
        // give special priority to font-xxx, color properties
        switch(prop->m_id)
        {
        case CSS_PROP_FONT_SIZE:
        case CSS_PROP_FONT:
        case CSS_PROP_COLOR:
        case CSS_PROP_BACKGROUND_IMAGE:
            // these have to be applied first, because other properties use the computed
            // values of these porperties.
	    first = true;
            break;
        default:
            first = false;
            break;
        }

	array->prop = prop;
	array->pseudoId = RenderStyle::NOPSEUDO;
	array->selector = 0;
	array->position = i;
	array->priority = (!first << 30) | (source << 24);
	static_cast<QList<CSSOrderedProperty>*>(list)->append( array );
	array++;
    }
}

static bool subject;

void CSSStyleSelector::checkSelector(int selIndex, DOM::ElementImpl *e)
{
    dynamicPseudo = RenderStyle::NOPSEUDO;
    selectorDynamicState = StyleSelector::None;
    NodeImpl *n = e;

    selectorCache[ selIndex ].state = Invalid;
    CSSSelector *sel = selectors[ selIndex ];

    // we have the subject part of the selector
    subject = true;

    // hack. We can't allow :hover, as it would trigger a complete relayout with every mouse event.
    bool single = false;
    if ( sel->tag == -1 )
	single = true;
    
    // first selector has to match
    if(!checkOneSelector(sel, e)) return;

    // check the subselectors
    CSSSelector::Relation relation = sel->relation;
    while((sel = sel->tagHistory))
    {
	single = false;
        if(!n->isElementNode()) return;
        switch(relation)
        {
        case CSSSelector::Descendant:
        {
            bool found = false;
            while(!found)
            {
		subject = false;
                n = n->parentNode();
                if(!n || !n->isElementNode()) return;
                ElementImpl *elem = static_cast<ElementImpl *>(n);
                if(checkOneSelector(sel, elem)) found = true;
            }
            break;
        }
        case CSSSelector::Child:
        {
		subject = false;
            n = n->parentNode();
            if(!n || !n->isElementNode()) return;
            ElementImpl *elem = static_cast<ElementImpl *>(n);
            if(!checkOneSelector(sel, elem)) return;
            break;
        }
        case CSSSelector::Sibling:
        {
		subject = false;
            n = n->previousSibling();
	    while( n && !n->isElementNode() )
		n = n->previousSibling();
            if( !n ) return;
            ElementImpl *elem = static_cast<ElementImpl *>(n);
            if(!checkOneSelector(sel, elem)) return;
            break;
        }
        case CSSSelector::SubSelector:
	{
	    //kdDebug() << "CSSOrderedRule::checkSelector" << endl;
	    ElementImpl *elem = static_cast<ElementImpl *>(n);
	    // a selector is invalid if something follows :first-xxx
	    if ( dynamicPseudo == RenderStyle::FIRST_LINE ||
		 dynamicPseudo == RenderStyle::FIRST_LETTER ) {
		return;
	    }
	    if(!checkOneSelector(sel, elem)) return;
	    //kdDebug() << "CSSOrderedRule::checkSelector: passed" << endl;
	    break;
	}
        }
        relation = sel->relation;
    }
    // disallow *:hover
    if ( single && selectorDynamicState & StyleSelector::Hover )
	return;
    usedDynamicStates |= selectorDynamicState;
    if ((selectorDynamicState & dynamicState) != selectorDynamicState)
	return;
    if ( dynamicPseudo != RenderStyle::NOPSEUDO ) {
	selectorCache[selIndex].state = AppliesPseudo;
	selectors[ selIndex ]->pseudoId = dynamicPseudo;
    } else
	selectorCache[ selIndex ].state = Applies;
    //qDebug( "selector %d applies", selIndex );
    //selectors[ selIndex ]->print();
    return;
}

// modified version of the one in kurl.cpp
static void cleanpath(QString &path)
{
    int pos;
    while ( (pos = path.find( "/../" )) != -1 ) {
	int prev = 0;
	if ( pos > 0 )
	    prev = path.findRev( "/", pos -1 );
        // don't remove the host, i.e. http://foo.org/../foo.html
        if (prev < 0 || (prev > 3 && path.findRev("://", prev-1) == prev-2))
            path.remove( pos, 3);
        else
            // matching directory found ?
            path.remove( prev, pos- prev + 3 );
    }
    pos = 0;
    while ( (pos = path.find( "//", pos )) != -1) {
	if ( pos == 0 || path[pos-1] != ':' )
	    path.remove( pos, 1 );
	else
	    pos += 2;
    }
    while ( (pos = path.find( "/./" )) != -1)
	path.remove( pos, 2 );
    //kdDebug() << "checkPseudoState " << path << endl;
}

static void checkPseudoState( DOM::ElementImpl *e )
{
    DOMString attr;
    if( e->id() != ID_A || (attr = e->getAttribute(ATTR_HREF)).isNull() ) {
	pseudoState = PseudoNone;
	return;
    }
    QString u = attr.string();
    if ( !u.contains("://") ) {
	if ( u[0] == '/' )
	    u = encodedurl->host + u;
	else if ( u[0] == '#' )
	    u = encodedurl->file + u;
	else
	    u = encodedurl->path + u;
	cleanpath( u );
    }
    //completeURL( attr.string() );
    pseudoState = KHTMLFactory::vLinks()->contains( u ) ? PseudoVisited : PseudoLink;
}

bool CSSStyleSelector::checkOneSelector(DOM::CSSSelector *sel, DOM::ElementImpl *e)
{

    if(!e)
        return false;

    if(e->id() != sel->tag && sel->tag != -1) return false;

    if(sel->attr)
    {
        DOMString value = e->getAttribute(sel->attr);
        if(value.isNull()) return false; // attribute is not set

        switch(sel->match)
        {
        case CSSSelector::Exact:
	    if( (strictParsing && strcmp(sel->value, value) ) ||
                (!strictParsing && strcasecmp(sel->value, value)))
                return false;
            break;
        case CSSSelector::Set:
            break;
        case CSSSelector::List:
        {
            //kdDebug( 6080 ) << "checking for list match" << endl;
            QString str = value.string();
            QString selStr = sel->value.string();
            int pos = str.find(selStr, 0, strictParsing);
            if(pos == -1) return false;
            if(pos && str[pos-1] != ' ') return false;
            pos += selStr.length();
            if(pos < (int)str.length() && str[pos] != ' ') return false;
            break;
        }
        case CSSSelector::Hyphen:
        {
            // ### still doesn't work. FIXME
            //kdDebug( 6080 ) << "checking for hyphen match" << endl;
            QString str = value.string();
            if(str.find(sel->value.string(), 0, strictParsing) != 0) return false;
            // ### could be "bla , sdfdsf" too. Parse out spaces
            int pos = sel->value.length() + 1;
            while(pos < (int)str.length() && sel->value[pos] == ' ') pos++;
            if(pos < (int)str.length() && sel->value[pos] != ',') return false;
            break;
        }
        case CSSSelector::Pseudo:
        case CSSSelector::None:
            break;
        }
    }
    if(sel->match == CSSSelector::Pseudo)
    {
        // Pseudo elements. We need to check first child here. No dynamic pseudo
        // elements for the moment
	const QString& value = sel->value.string();
	//kdDebug() << "CSSOrderedRule::pseudo " << value << endl;
	if(value == "first-child") {
	    // first-child matches the first child that is an element!
	    DOM::NodeImpl *n = e->parentNode()->firstChild();
	    while( n && !n->isElementNode() )
		n = n->nextSibling();
	    if( n == e )
		return true;
	} else if ( value == "first-line" && subject ) {
	    dynamicPseudo=RenderStyle::FIRST_LINE;
	    return true;
	} else if ( value == "first-letter" && subject ) {
	    dynamicPseudo=RenderStyle::FIRST_LETTER;
	    return true;
	} else if( value == "link") {
	    if ( pseudoState == PseudoUnknown )
		checkPseudoState( e );
	    if ( pseudoState == PseudoLink ) {
		return true;
	    }
	} else if ( value == "visited" ) {
	    if ( pseudoState == PseudoUnknown )
		checkPseudoState( e );
	    if ( pseudoState == PseudoVisited )
		return true;
	} else if ( value == "hover" ) {
	    selectorDynamicState |= StyleSelector::Hover;
	    // dynamic pseudos have to be sorted out in checkSelector, so we if it could in some state apply
	    // to the element.
	    return true;
	} else if ( value == "focus" ) {
	    selectorDynamicState |= StyleSelector::Focus;
	    return true;
	} else if ( value == "active" ) {
	    if ( pseudoState == PseudoUnknown )
		checkPseudoState( e );
	    if ( pseudoState != PseudoNone ) {
		selectorDynamicState |= StyleSelector::Active;
		return true;
	    }
	}
	return false;
    }
    // ### add the rest of the checks...
    return true;
}

void CSSStyleSelector::clearLists()
{
    if ( selectors ) delete [] selectors;
    if ( selectorCache ) {
        for ( unsigned int i = 0; i < selectors_size; i++ )
            if ( selectorCache[i].props )
                delete [] selectorCache[i].props;

        delete [] selectorCache;
    }
    if ( properties ) {
	CSSOrderedProperty **prop = properties;
	while ( *prop ) {
	    delete (*prop);
	    prop++;
	}
        delete [] properties;
    }
    selectors = 0;
    properties = 0;
    selectorCache = 0;
}


void CSSStyleSelector::buildLists()
{
    clearLists();
    // collect all selectors and Properties in lists. Then transer them to the array for faster lookup.

    QList<CSSSelector> selectorList;
    CSSOrderedPropertyList propertyList;

    if(defaultStyle) defaultStyle->collect( &selectorList, &propertyList, Default, Default );
    if(userStyle) userStyle->collect(&selectorList, &propertyList, User, UserImportant );
    if(authorStyle) authorStyle->collect(&selectorList, &propertyList, Author, AuthorImportant );

    selectors_size = selectorList.count();
    selectors = new CSSSelector *[selectors_size];
    CSSSelector *s = selectorList.first();
    CSSSelector **sel = selectors;
    while ( s ) {
	*sel = s;
	s = selectorList.next();
	++sel;
    }

    selectorCache = new SelectorCache[selectors_size];
    for ( unsigned int i = 0; i < selectors_size; i++ ) {
        selectorCache[i].state = Unknown;
        selectorCache[i].props_size = 0;
        selectorCache[i].props = 0;
    }

    // presort properties. Should make the sort() calls in styleForElement faster.
    propertyList.sort();
    properties_size = propertyList.count() + 1;
    properties = new CSSOrderedProperty *[ properties_size ];
    CSSOrderedProperty *p = propertyList.first();
    CSSOrderedProperty **prop = properties;
    while ( p ) {
	*prop = p;
	p = propertyList.next();
	++prop;
    }
    *prop = 0;

    // This algorithm sucks badly. but hey, its performance shouldn't matter much ( Dirk )
    for ( unsigned int sel = 0; sel < selectors_size; ++sel ) {
        prop = properties;
        int len = 0;
        int offset = 0;
        bool matches = properties[0] ? properties[0]->selector == sel : false;
        for ( unsigned int p = 0; p < properties_size; ++p ) {
            if ( !properties[p] || ( matches != ( properties[p]->selector == sel ) )) {
                if ( matches ) {
                    int* newprops = new int[selectorCache[sel].props_size+2];
                    for ( unsigned int i=0; i < selectorCache[sel].props_size; i++ )
                        newprops[i] = selectorCache[sel].props[i];
                    newprops[selectorCache[sel].props_size] = offset;
                    newprops[selectorCache[sel].props_size+1] = len;
                    delete [] selectorCache[sel].props;
                    selectorCache[sel].props = newprops;
                    selectorCache[sel].props_size += 2;
                    matches = false;
                }
                else {
                    matches = true;
                    offset = p;
                    len = 0;
                }
            }
            ++len;
        }
    }

#if 0
    // and now the same for the selector map
    for ( unsigned int sel = 0; sel < selectors_size; ++sel ) {
        kdDebug( 6080 ) << "trying for sel: " << sel << endl;
        int len = 0;
        int offset = 0;
        bool matches = false;
        for ( unsigned int i = 0; i < selectors_size; i++ ) {
            int tag = selectors[i]->tag;
            if ( sel != tag && tag != -1 )
                selectorCache[i].state = Invalid;
            else
                selectorCache[i].state = Unknown;

            if ( matches != ( selectorCache[i].state == Unknown ) ) {
                if ( matches ) {
                    kdDebug( 6080 ) << "new: offs: " << offset << " len: " << len << endl;
                    matches = false;
                }
                else {
                    matches = true;
//                    offset = p-selectors;
                    len = 0;
                }
            }
            ++len;
        }
    }
#endif
}


// ----------------------------------------------------------------------


CSSOrderedRule::CSSOrderedRule(DOM::CSSStyleRuleImpl *r, DOM::CSSSelector *s, int _index)
{
    rule = r;
    if(rule) r->ref();
    index = _index;
    selector = s;
}

CSSOrderedRule::~CSSOrderedRule()
{
    if(rule) rule->deref();
}

// -----------------------------------------------------------------

CSSStyleSelectorList::CSSStyleSelectorList()
    : QList<CSSOrderedRule>()
{
    setAutoDelete(true);
}
CSSStyleSelectorList::~CSSStyleSelectorList()
{
}

void CSSStyleSelectorList::append(StyleSheetImpl *sheet)
{

    if(!sheet || !sheet->isCSSStyleSheet()) return;

    int len = sheet->length();

    for(int i = 0; i< len; i++)
    {
        StyleBaseImpl *item = sheet->item(i);
        if(item->isStyleRule())
        {
            CSSStyleRuleImpl *r = static_cast<CSSStyleRuleImpl *>(item);
            QList<CSSSelector> *s = r->selector();
            for(int j = 0; j < (int)s->count(); j++)
            {
                CSSOrderedRule *rule = new CSSOrderedRule(r, s->at(j), count());
		QList<CSSOrderedRule>::append(rule);
                //kdDebug( 6080 ) << "appending StyleRule!" << endl;
            }
        }
        else if(item->isImportRule())
        {
            CSSImportRuleImpl *import = static_cast<CSSImportRuleImpl *>(item);
            // ### check media type
            StyleSheetImpl *importedSheet = import->styleSheet();
            append(importedSheet);
        }
        // ### include media, import rules and other
    }
}


void CSSStyleSelectorList::collect( QList<CSSSelector> *selectorList, CSSOrderedPropertyList *propList,
				    Source regular, Source important )
{
    CSSOrderedRule *r = first();
    while( r ) {
	CSSSelector *sel = selectorList->first();
	int selectorNum = 0;
	while( sel ) {
	    if ( *sel == *(r->selector) )
		break;
	    sel = selectorList->next();
	    selectorNum++;
	}
	if ( !sel )
	    selectorList->append( r->selector );
//	else
//	    qDebug("merged one selector");
	propList->append(r->rule->declaration(), selectorNum, r->selector->specificity(), regular, important );
	r = next();
    }
}

// -------------------------------------------------------------------------

int CSSOrderedPropertyList::compareItems(QCollection::Item i1, QCollection::Item i2)
{
    int diff =  static_cast<CSSOrderedProperty *>(i1)->priority
        - static_cast<CSSOrderedProperty *>(i2)->priority;
    return diff ? diff : static_cast<CSSOrderedProperty *>(i1)->position
        - static_cast<CSSOrderedProperty *>(i2)->position;
}

void CSSOrderedPropertyList::append(DOM::CSSStyleDeclarationImpl *decl, uint selector, uint specificity,
				    Source regular, Source important )
{
    QList<CSSProperty> *values = decl->values();
    if(!values) return;
    int len = values->count();
    for(int i = 0; i < len; i++)
    {
        CSSProperty *prop = values->at(i);
	Source source = regular;

	if( prop->m_bImportant ) source = important;
	if( prop->nonCSSHint ) source = NonCSSHint;

	bool first = false;
        // give special priority to font-xxx, color properties
        switch(prop->m_id)
        {
        case CSS_PROP_FONT_SIZE:
        case CSS_PROP_FONT:
        case CSS_PROP_COLOR:
        case CSS_PROP_BACKGROUND_IMAGE:
            // these have to be applied first, because other properties use the computed
            // values of these porperties.
	    first = true;
            break;
        default:
            break;
        }

	QList<CSSOrderedProperty>::append(new CSSOrderedProperty(prop, selector,
								 first, source, specificity,
								 count() ));
    }
}

// -------------------------------------------------------------------------------------
// this is mostly boring stuff on how to apply a certain rule to the renderstyle...

static Length convertToLength( CSSPrimitiveValueImpl *primitiveValue, RenderStyle *style, QPaintDeviceMetrics *paintDeviceMetrics, bool *ok = 0 )
{
    Length l;
    if ( !primitiveValue ) {
	if ( *ok )
	    *ok = false;
    } else {
	int type = primitiveValue->primitiveType();
	if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
	    l = Length(computeLength(primitiveValue, style, paintDeviceMetrics), Fixed);
	else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
	    l = Length(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE)), Percent);
	else if(type == CSSPrimitiveValue::CSS_NUMBER)
	    l = Length(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER)*100), Percent);
	else if ( *ok )
	    *ok = false;
    }
    return l;
}

void khtml::applyRule(khtml::RenderStyle *style, DOM::CSSProperty *prop, DOM::ElementImpl *e)
{
    CSSValueImpl *value = prop->value();

    QPaintDeviceMetrics *paintDeviceMetrics = e->ownerDocument()->paintDeviceMetrics();

    //kdDebug( 6080 ) << "applying property " << prop->m_id << endl;

    CSSPrimitiveValueImpl *primitiveValue = 0;
    if(value->isPrimitiveValue()) primitiveValue = static_cast<CSSPrimitiveValueImpl *>(value);

    Length l;
    bool apply = false;

    // here follows a long list, defining how to aplly certain properties to the style object.
    // rather boring stuff...
    switch(prop->m_id)
    {
// ident only properties
    case CSS_PROP_BACKGROUND_ATTACHMENT:
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setBackgroundAttachment(e->parentNode()->style()->backgroundAttachment());
            return;
        }
        if(!primitiveValue) break;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_FIXED:
            {
                style->setBackgroundAttachment(false);
                DocumentImpl *doc = e->ownerDocument();
		// only use slow repaints if we actually have a background pixmap
                if( style->backgroundImage() )
                    doc->view()->useSlowRepaints();
                break;
            }
        case CSS_VAL_SCROLL:
            style->setBackgroundAttachment(true);
            break;
        default:
            return;
        }
    case CSS_PROP_BACKGROUND_REPEAT:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT) {
            if(!e->parentNode()) return;
            style->setBackgroundRepeat(e->parentNode()->style()->backgroundRepeat());
            return;
        }
        if(!primitiveValue) return;
	switch(primitiveValue->getIdent())
	{
	case CSS_VAL_REPEAT:
	    style->setBackgroundRepeat( REPEAT );
	    break;
	case CSS_VAL_REPEAT_X:
	    style->setBackgroundRepeat( REPEAT_X );
	    break;
	case CSS_VAL_REPEAT_Y:
	    style->setBackgroundRepeat( REPEAT_Y );
	    break;
	case CSS_VAL_NO_REPEAT:
	    style->setBackgroundRepeat( NO_REPEAT );
	    break;
	default:
	    return;
	}
    }
    case CSS_PROP_BORDER_COLLAPSE:
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setBorderCollapse(e->parentNode()->style()->borderCollapse());
            break;
        }
        if(!primitiveValue) break;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_COLLAPSE:
            style->setBorderCollapse(true);
            break;
        case CSS_VAL_SCROLL:
            style->setBorderCollapse(false);
            break;
        default:
            return;
        }

    case CSS_PROP_BORDER_TOP_STYLE:
    case CSS_PROP_BORDER_RIGHT_STYLE:
    case CSS_PROP_BORDER_BOTTOM_STYLE:
    case CSS_PROP_BORDER_LEFT_STYLE:
    case CSS_PROP_OUTLINE_STYLE:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            switch(prop->m_id)
            {
            case CSS_PROP_BORDER_TOP_STYLE:
                style->setBorderTopStyle(e->parentNode()->style()->borderTopStyle());
                return;
            case CSS_PROP_BORDER_RIGHT_STYLE:
                style->setBorderRightStyle(e->parentNode()->style()->borderRightStyle());
                return;
            case CSS_PROP_BORDER_BOTTOM_STYLE:
                style->setBorderBottomStyle(e->parentNode()->style()->borderBottomStyle());
                return;
            case CSS_PROP_BORDER_LEFT_STYLE:
                style->setBorderLeftStyle(e->parentNode()->style()->borderLeftStyle());
                return;
            case CSS_PROP_OUTLINE_STYLE:
                style->setOutlineStyle(e->parentNode()->style()->outlineStyle());
                return;
            }
        }
        if(!primitiveValue) return;
	EBorderStyle s = (EBorderStyle) (primitiveValue->getIdent() - CSS_VAL_NONE);

        switch(prop->m_id)
        {
        case CSS_PROP_BORDER_TOP_STYLE:
            style->setBorderTopStyle(s); return;
        case CSS_PROP_BORDER_RIGHT_STYLE:
            style->setBorderRightStyle(s); return;
        case CSS_PROP_BORDER_BOTTOM_STYLE:
            style->setBorderBottomStyle(s); return;
        case CSS_PROP_BORDER_LEFT_STYLE:
            style->setBorderLeftStyle(s); return;
        case CSS_PROP_OUTLINE_STYLE:
            style->setOutlineStyle(s); return;
        default:
            return;
        }
        return;
    }
    case CSS_PROP_CAPTION_SIDE:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setCaptionSide(e->parentNode()->style()->captionSide());
            break;
        }
        if(!primitiveValue) break;
        ECaptionSide c = CAPTOP;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_LEFT:
            c = CAPLEFT; break;
        case CSS_VAL_RIGHT:
            c = CAPRIGHT; break;
        case CSS_VAL_TOP:
            c = CAPTOP; break;
        case CSS_VAL_BOTTOM:
            c = CAPBOTTOM; break;
        default:
            return;
        }
        style->setCaptionSide(c);
        return;
    }
    case CSS_PROP_CLEAR:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setClear(e->parentNode()->style()->clear());
            break;
        }
        if(!primitiveValue) break;
        EClear c = CNONE;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_LEFT:
            c = CLEFT; break;
        case CSS_VAL_RIGHT:
            c = CRIGHT; break;
        case CSS_VAL_BOTH:
            c = CBOTH; break;
        default:
            return;
        }
        style->setClear(c);
        return;
    }
    case CSS_PROP_DIRECTION:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setDirection(e->parentNode()->style()->direction());
            break;
        }
        if(!primitiveValue) break;
        style->setDirection( (EDirection) (primitiveValue->getIdent() - CSS_VAL_LTR) );
        return;
    }
    case CSS_PROP_DISPLAY:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setDisplay(e->parentNode()->style()->display());
            break;
        }
        if(!primitiveValue) break;
	int id = primitiveValue->getIdent();
	EDisplay d;
	if ( id == CSS_VAL_NONE) {
	    d = NONE;
	} else if ( id == CSS_VAL_RUN_IN || id == CSS_VAL_COMPACT ||
		    id == CSS_VAL_MARKER ) {
	    // these are not supported at the moment, so we just ignore them.
	    return;
	} else {
	    d = EDisplay(primitiveValue->getIdent() - CSS_VAL_INLINE);
	}

        style->setDisplay(d);
        //kdDebug( 6080 ) << "setting display to " << d << endl;

        break;
    }

    case CSS_PROP_EMPTY_CELLS:
        break;
    case CSS_PROP_FLOAT:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setFloating(e->parentNode()->style()->floating());
            return;
        }
        if(!primitiveValue) return;
        EFloat f;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_LEFT:
            f = FLEFT; break;
        case CSS_VAL_RIGHT:
            f = FRIGHT; break;
        case CSS_VAL_NONE:
        case CSS_VAL_CENTER:  //Non standart CSS-Value
            f = FNONE; break;
        default:
            return;
        }
        if (f!=FNONE && style->display()==LIST_ITEM)
            style->setDisplay(BLOCK);

        style->setFloating(f);
        break;
    }

        break;
    case CSS_PROP_FONT_STRETCH:
        break;

    case CSS_PROP_FONT_STYLE:
    {
        QFont f = style->font();
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            f.setItalic(e->parentNode()->style()->font().italic());
            style->setFont(f);
            return;
        }
        if(!primitiveValue) return;
        switch(primitiveValue->getIdent())
        {
            // ### oblique is the same as italic for the moment...
        case CSS_VAL_OBLIQUE:
        case CSS_VAL_ITALIC:
            f.setItalic(true);
            break;
        case CSS_VAL_NORMAL:
            f.setItalic(false);
            break;
        default:
            return;
        }
        //KGlobal::charsets()->setQFont(f, e->ownerDocument()->view()->part()->settings()->charset);
        style->setFont(f);
        break;
    }


    case CSS_PROP_FONT_VARIANT:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setFontVariant(e->parentNode()->style()->fontVariant());
            return;
        }
        if(!primitiveValue) return;
        switch(primitiveValue->getIdent()) {
	    case CSS_VAL_NORMAL:
		style->setFontVariant( FVNORMAL ); break;
	    case CSS_VAL_SMALL_CAPS:
		style->setFontVariant( SMALL_CAPS ); break;
	    default:
            return;
        }
	break;
    }

    case CSS_PROP_FONT_WEIGHT:
    {
        QFont f = style->font();
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            f.setWeight(e->parentNode()->style()->font().weight());
            //KGlobal::charsets()->setQFont(f, e->ownerDocument()->view()->part()->settings()->charset);
            style->setFont(f);
            return;
        }
        if(!primitiveValue) return;
        if(primitiveValue->getIdent())
        {
            switch(primitiveValue->getIdent())
            {
                // ### we just support normal and bold fonts at the moment...
                // setWeight can actually accept values between 0 and 99...
            case CSS_VAL_BOLD:
            case CSS_VAL_BOLDER:
                f.setWeight(QFont::Bold);
                break;
            case CSS_VAL_NORMAL:
            case CSS_VAL_LIGHTER:
                f.setWeight(QFont::Normal);
                break;
            default:
                return;
            }
        }
        else
        {
            // ### fix parsing of 100-900 values in parser, apply them here
        }
        //KGlobal::charsets()->setQFont(f, e->ownerDocument()->view()->part()->settings()->charset);
        style->setFont(f);
        break;
    }

    case CSS_PROP_LIST_STYLE_POSITION:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setListStylePosition(e->parentNode()->style()->listStylePosition());
            return;
        }
        if(!primitiveValue) return;
        if(primitiveValue->getIdent())
            style->setListStylePosition( (EListStylePosition) (primitiveValue->getIdent() - CSS_VAL_OUTSIDE) );
        return;
    }

    case CSS_PROP_LIST_STYLE_TYPE:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setListStyleType(e->parentNode()->style()->listStyleType());
            return;
        }
        if(!primitiveValue) return;
        if(primitiveValue->getIdent())
        {
            EListStyleType t;
	    int id = primitiveValue->getIdent();
	    if ( id == CSS_VAL_NONE) { // important!!
	      t = LNONE;
	    } else {
	      t = EListStyleType(id - CSS_VAL_DISC);
	    }
            style->setListStyleType(t);
        }
        return;
    }

    case CSS_PROP_OVERFLOW:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setPosition(e->parentNode()->style()->position());
            return;
        }
        if(!primitiveValue) return;
        EOverflow o;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_VISIBLE:
            o = OVISIBLE; break;
        case CSS_VAL_HIDDEN:
            o = OHIDDEN; kdDebug() << "overflow:hidden" << endl; break;
        case CSS_VAL_SCROLL:
            o = SCROLL; break;
        case CSS_VAL_AUTO:
            o = AUTO; break;
        default:
            return;
        }
        style->setOverflow(o);
        return;
    }
    break;
    case CSS_PROP_PAGE:
    case CSS_PROP_PAGE_BREAK_AFTER:
    case CSS_PROP_PAGE_BREAK_BEFORE:
    case CSS_PROP_PAGE_BREAK_INSIDE:
//    case CSS_PROP_PAUSE_AFTER:
//    case CSS_PROP_PAUSE_BEFORE:
        break;

    case CSS_PROP_POSITION:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setPosition(e->parentNode()->style()->position());
            return;
        }
        if(!primitiveValue) return;
        EPosition p;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_STATIC:
            p = STATIC; break;
        case CSS_VAL_RELATIVE:
            p = RELATIVE; break;
        case CSS_VAL_ABSOLUTE:
            p = ABSOLUTE; break;
        case CSS_VAL_FIXED:
            {
                DocumentImpl *doc = e->ownerDocument();
                doc->view()->useSlowRepaints();
                p = FIXED;
                break;
            }
        default:
            return;
        }
        style->setPosition(p);
        return;
    }

//     case CSS_PROP_SPEAK:
//     case CSS_PROP_SPEAK_HEADER:
//     case CSS_PROP_SPEAK_NUMERAL:
//     case CSS_PROP_SPEAK_PUNCTUATION:
    case CSS_PROP_TABLE_LAYOUT:
    case CSS_PROP_UNICODE_BIDI:
        break;
    case CSS_PROP_TEXT_TRANSFORM:
        {
        if(value->valueType() == CSSValue::CSS_INHERIT) {
            if(!e->parentNode()) return;
            style->setTextTransform(e->parentNode()->style()->textTransform());
            return;
        }

        if(!primitiveValue->getIdent()) return;

        ETextTransform tt;
        switch(primitiveValue->getIdent()) {
        case CSS_VAL_CAPITALIZE:  tt = CAPITALIZE;  break;
        case CSS_VAL_UPPERCASE:   tt = UPPERCASE;   break;
        case CSS_VAL_LOWERCASE:   tt = LOWERCASE;   break;
        case CSS_VAL_NONE:
        default:                  tt = TTNONE;      break;
        }
        style->setTextTransform(tt);
        break;
        }

    case CSS_PROP_VISIBILITY:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT) {
            if(!e->parentNode()) return;
            style->setVisiblity(e->parentNode()->style()->visiblity());
            return;
        }

        switch( primitiveValue->getIdent() ) {
        case CSS_VAL_HIDDEN:
            style->setVisiblity( HIDDEN );
            break;
        case CSS_VAL_VISIBLE:
            style->setVisiblity( VISIBLE );
            break;
        case CSS_VAL_COLLAPSE:
            style->setVisiblity( COLLAPSE );
        default:
            break;
        }
        break;
    }
    case CSS_PROP_WHITE_SPACE:
        if(value->valueType() == CSSValue::CSS_INHERIT) {
            if(!e->parentNode()) return;
            style->setWhiteSpace(e->parentNode()->style()->whiteSpace());
            return;
        }

        if(!primitiveValue->getIdent()) return;

        EWhiteSpace s;
        switch(primitiveValue->getIdent()) {
        case CSS_VAL_NOWRAP:
            s = NOWRAP;
            break;
        case CSS_VAL_PRE:
            s = PRE;
            break;
        case CSS_VAL_NORMAL:
        default:
            s = NORMAL;
            break;
        }
        style->setWhiteSpace(s);
        break;


// special properties (css_extensions)
//    case CSS_PROP_AZIMUTH:
        // CSS2Azimuth
    case CSS_PROP_BACKGROUND_POSITION:
        // CSS2BackgroundPosition
        break;
    case CSS_PROP_BACKGROUND_POSITION_X:
      {
      if(!primitiveValue) break;
      Length l;
      int type = primitiveValue->primitiveType();
      if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
	l = Length(computeLength(primitiveValue, style, paintDeviceMetrics), Fixed);
      else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
	l = Length((int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE), Percent);
      else
	return;
      style->setBackgroundXPosition(l);
      break;
      }
    case CSS_PROP_BACKGROUND_POSITION_Y:
      {
      if(!primitiveValue) break;
      Length l;
      int type = primitiveValue->primitiveType();
      if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
	l = Length(computeLength(primitiveValue, style, paintDeviceMetrics), Fixed);
      else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
	l = Length((int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE), Percent);
      else
	return;
      style->setBackgroundYPosition(l);
      break;
      }
    case CSS_PROP_BORDER_SPACING:
        {
        if(!primitiveValue) break;
        short spacing = 0;
        spacing = computeLength(primitiveValue, style, paintDeviceMetrics);
        style->setBorderSpacing(spacing);
        break;
        }
        // CSS2BorderSpacing
    case CSS_PROP_CURSOR:
        // CSS2Cursor
        if(value->valueType() == CSSValue::CSS_INHERIT) {
            if(!e->parentNode()) return;
            style->setCursor(e->parentNode()->style()->cursor());
            return;
        } else if(primitiveValue) {
	    if(primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_URI) {
            	CSSImageValueImpl *image = static_cast<CSSImageValueImpl *>(primitiveValue);
            	kdDebug( 6080 ) << "setting cursor image to " << image->cssText().string() << endl;
            	style->setCursorImage(image->image());
            } else {
		style->setCursor( (ECursor) (primitiveValue->getIdent() - CSS_VAL_AUTO) );
	    }
        }
        break;
//    case CSS_PROP_PLAY_DURING:
        // CSS2PlayDuring
    case CSS_PROP_TEXT_SHADOW:
        // list of CSS2TextShadow
        break;

// colors || inherit
    case CSS_PROP_BACKGROUND_COLOR:
    case CSS_PROP_BORDER_TOP_COLOR:
    case CSS_PROP_BORDER_RIGHT_COLOR:
    case CSS_PROP_BORDER_BOTTOM_COLOR:
    case CSS_PROP_BORDER_LEFT_COLOR:
    case CSS_PROP_COLOR:
    case CSS_PROP_OUTLINE_COLOR:
        // this property is an extension used to get HTML4 <font> right.
    case CSS_PROP_TEXT_DECORATION_COLOR:
        // ie scrollbar styling
    case CSS_PROP_SCROLLBAR_FACE_COLOR:
    case CSS_PROP_SCROLLBAR_SHADOW_COLOR:
    case CSS_PROP_SCROLLBAR_HIGHLIGHT_COLOR:
    case CSS_PROP_SCROLLBAR_3DLIGHT_COLOR:
    case CSS_PROP_SCROLLBAR_DARKSHADOW_COLOR:
    case CSS_PROP_SCROLLBAR_TRACK_COLOR:
    case CSS_PROP_SCROLLBAR_ARROW_COLOR:

    {
        QColor col;
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            switch(prop->m_id)
            {
            case CSS_PROP_BACKGROUND_COLOR:
                style->setBackgroundColor(e->parentNode()->style()->backgroundColor()); break;
            case CSS_PROP_BORDER_TOP_COLOR:
                style->setBorderTopColor(e->parentNode()->style()->borderTopColor()); break;
            case CSS_PROP_BORDER_RIGHT_COLOR:
                style->setBorderRightColor(e->parentNode()->style()->borderRightColor()); break;
            case CSS_PROP_BORDER_BOTTOM_COLOR:
                style->setBorderBottomColor(e->parentNode()->style()->borderBottomColor()); break;
            case CSS_PROP_BORDER_LEFT_COLOR:
                style->setBorderLeftColor(e->parentNode()->style()->borderLeftColor()); break;
            case CSS_PROP_COLOR:
                style->setColor(e->parentNode()->style()->color()); break;
            case CSS_PROP_OUTLINE_COLOR:
		style->setOutlineColor(e->parentNode()->style()->outlineColor()); break;
            default:
                // ###
                break;
            }
            return;
        }
        if(!primitiveValue) return;
        if(primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_RGBCOLOR)
            col = primitiveValue->getRGBColorValue()->color();
        else
            return;
        //kdDebug( 6080 ) << "applying color " << col.isValid() << endl;
        switch(prop->m_id)
        {
        case CSS_PROP_BACKGROUND_COLOR:
            style->setBackgroundColor(col); break;
        case CSS_PROP_BORDER_TOP_COLOR:
            style->setBorderTopColor(col); break;
        case CSS_PROP_BORDER_RIGHT_COLOR:
            style->setBorderRightColor(col); break;
        case CSS_PROP_BORDER_BOTTOM_COLOR:
            style->setBorderBottomColor(col); break;
        case CSS_PROP_BORDER_LEFT_COLOR:
            style->setBorderLeftColor(col); break;
        case CSS_PROP_COLOR:
            style->setColor(col); break;
        case CSS_PROP_TEXT_DECORATION_COLOR:
            style->setTextDecorationColor(col); break;
        case CSS_PROP_OUTLINE_COLOR:
            style->setOutlineColor(col); break;
        case CSS_PROP_SCROLLBAR_FACE_COLOR:
            style->setPaletteColor(QPalette::Active, QColorGroup::Button, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::Button, col);
            break;
        case CSS_PROP_SCROLLBAR_SHADOW_COLOR:
            style->setPaletteColor(QPalette::Active, QColorGroup::Shadow, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::Shadow, col);
            break;
        case CSS_PROP_SCROLLBAR_HIGHLIGHT_COLOR:
            style->setPaletteColor(QPalette::Active, QColorGroup::Light, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::Light, col);
            break;
        case CSS_PROP_SCROLLBAR_3DLIGHT_COLOR:
            break;
        case CSS_PROP_SCROLLBAR_DARKSHADOW_COLOR:
            style->setPaletteColor(QPalette::Active, QColorGroup::Dark, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::Dark, col);
            break;
        case CSS_PROP_SCROLLBAR_TRACK_COLOR:
            style->setPaletteColor(QPalette::Active, QColorGroup::Base, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::Base, col);
            style->setPaletteColor(QPalette::Active, QColorGroup::Mid, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::Mid, col);
            style->setPaletteColor(QPalette::Active, QColorGroup::Background, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::Background, col);
            break;
        case CSS_PROP_SCROLLBAR_ARROW_COLOR:
            style->setPaletteColor(QPalette::Active, QColorGroup::ButtonText, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::ButtonText, col);
            break;
        default:
            return;
        }
        return;
    }
    break;
// uri || inherit
    case CSS_PROP_BACKGROUND_IMAGE:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setBackgroundImage(e->parentNode()->style()->backgroundImage());
            break;
        }
        if(!primitiveValue) return;
        CSSImageValueImpl *image = static_cast<CSSImageValueImpl *>(primitiveValue);
        style->setBackgroundImage(image->image());
        //kdDebug( 6080 ) << "setting image in style to " << image->image() << endl;
        break;
    }
//     case CSS_PROP_CUE_AFTER:
//     case CSS_PROP_CUE_BEFORE:
//         break;
    case CSS_PROP_LIST_STYLE_IMAGE:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setListStyleImage(e->parentNode()->style()->listStyleImage());
            break;
        }
        if(!primitiveValue) return;
        CSSImageValueImpl *image = static_cast<CSSImageValueImpl *>(primitiveValue);
        style->setListStyleImage(image->image());
        kdDebug( 6080 ) << "setting image in list to " << image->image() << endl;
        break;
    }

// length
    case CSS_PROP_BORDER_TOP_WIDTH:
    case CSS_PROP_BORDER_RIGHT_WIDTH:
    case CSS_PROP_BORDER_BOTTOM_WIDTH:
    case CSS_PROP_BORDER_LEFT_WIDTH:
    case CSS_PROP_OUTLINE_WIDTH:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            switch(prop->m_id)
            {
            case CSS_PROP_BORDER_TOP_WIDTH:
                style->setBorderTopWidth(e->parentNode()->style()->borderTopWidth()); break;
            case CSS_PROP_BORDER_RIGHT_WIDTH:
                style->setBorderRightWidth(e->parentNode()->style()->borderRightWidth()); break;
            case CSS_PROP_BORDER_BOTTOM_WIDTH:
                style->setBorderBottomWidth(e->parentNode()->style()->borderBottomWidth()); break;
            case CSS_PROP_BORDER_LEFT_WIDTH:
                style->setBorderLeftWidth(e->parentNode()->style()->borderLeftWidth()); break;
            case CSS_PROP_OUTLINE_WIDTH:
                style->setOutlineWidth(e->parentNode()->style()->outlineWidth()); break;
            default:
                // ###
                break;
            }
            return;
        }
        if(!primitiveValue) break;
        short width = 3; // medium is default value
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_THIN:
            width = 1;
            break;
        case CSS_VAL_MEDIUM:
            width = 3;
            break;
        case CSS_VAL_THICK:
            width = 5;
            break;
        case CSS_VAL_INVALID:
            width = computeLength(primitiveValue, style, paintDeviceMetrics);
            break;
        default:
            return;
        }
        if(width < 0) return;
        switch(prop->m_id)
        {
        case CSS_PROP_BORDER_TOP_WIDTH:
            style->setBorderTopWidth(width);
            break;
        case CSS_PROP_BORDER_RIGHT_WIDTH:
            style->setBorderRightWidth(width);
            break;
        case CSS_PROP_BORDER_BOTTOM_WIDTH:
            style->setBorderBottomWidth(width);
            break;
        case CSS_PROP_BORDER_LEFT_WIDTH:
            style->setBorderLeftWidth(width);
            break;
        case CSS_PROP_OUTLINE_WIDTH:
	    style->setOutlineWidth(width);
            break;
        default:
            return;
        }
        return;
    }

    case CSS_PROP_MARKER_OFFSET:
    case CSS_PROP_LETTER_SPACING:
    case CSS_PROP_WORD_SPACING:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            switch(prop->m_id)
            {
            case CSS_PROP_MARKER_OFFSET:
                // ###
                break;
            case CSS_PROP_LETTER_SPACING:
                style->setLetterSpacing(e->parentNode()->style()->letterSpacing()); break;
            case CSS_PROP_WORD_SPACING:
                style->setWordSpacing(e->parentNode()->style()->wordSpacing()); break;
            default:
                // ###
                break;
            }
            return;
        }
        if(!primitiveValue) return;
        int width = computeLength(primitiveValue, style, paintDeviceMetrics);
// reason : letter or word spacing may be negative.
//      if( width < 0 ) return;
        switch(prop->m_id)
        {
        case CSS_PROP_LETTER_SPACING:
            style->setLetterSpacing(width);
            break;
        case CSS_PROP_WORD_SPACING:
            style->setWordSpacing(width);
            break;
            // ### needs the definitions in renderstyle
        case CSS_PROP_MARKER_OFFSET:
        default:
            return;
        }
    }

// length, percent
    case CSS_PROP_MAX_WIDTH:
        // +none +inherit
        if(primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE)
            apply = true;
    case CSS_PROP_TOP:
    case CSS_PROP_LEFT:
    case CSS_PROP_RIGHT:
        // http://www.w3.org/Style/css2-updates/REC-CSS2-19980512-errata
        // introduces static-position value for top, left & right
        if(prop->m_id != CSS_PROP_MAX_WIDTH && primitiveValue &&
           primitiveValue->getIdent() == CSS_VAL_STATIC_POSITION)
        {
            //kdDebug( 6080 ) << "found value=static-position" << endl;
            l = Length ( 0, Static);
            apply = true;
        }
    case CSS_PROP_BOTTOM:
    case CSS_PROP_WIDTH:
    case CSS_PROP_MIN_WIDTH:
    case CSS_PROP_MARGIN_TOP:
    case CSS_PROP_MARGIN_RIGHT:
    case CSS_PROP_MARGIN_BOTTOM:
    case CSS_PROP_MARGIN_LEFT:
        // +inherit +auto
        if(prop->m_id != CSS_PROP_MAX_WIDTH && primitiveValue &&
           primitiveValue->getIdent() == CSS_VAL_AUTO)
        {
            //kdDebug( 6080 ) << "found value=auto" << endl;
            apply = true;
        }
    case CSS_PROP_PADDING_TOP:
    case CSS_PROP_PADDING_RIGHT:
    case CSS_PROP_PADDING_BOTTOM:
    case CSS_PROP_PADDING_LEFT:
    case CSS_PROP_TEXT_INDENT:
        // +inherit
    {
        if(value->valueType() == CSSValue::CSS_INHERIT) {
            if(!e->parentNode()) return;
            switch(prop->m_id)
                {
                case CSS_PROP_MAX_WIDTH:
                    style->setMaxWidth(e->parentNode()->style()->maxWidth()); break;
                case CSS_PROP_BOTTOM:
                    style->setBottom(e->parentNode()->style()->bottom()); break;
                case CSS_PROP_TOP:
                    style->setTop(e->parentNode()->style()->top()); break;
                case CSS_PROP_LEFT:
                    style->setLeft(e->parentNode()->style()->left()); break;
                case CSS_PROP_RIGHT:
                    style->setRight(e->parentNode()->style()->right()); break;
                case CSS_PROP_WIDTH:
                    style->setWidth(e->parentNode()->style()->width()); break;
                case CSS_PROP_MIN_WIDTH:
                    style->setMinWidth(e->parentNode()->style()->minWidth()); break;
                case CSS_PROP_PADDING_TOP:
                    style->setPaddingTop(e->parentNode()->style()->paddingTop()); break;
                case CSS_PROP_PADDING_RIGHT:
                    style->setPaddingRight(e->parentNode()->style()->paddingRight()); break;
                case CSS_PROP_PADDING_BOTTOM:
                    style->setPaddingBottom(e->parentNode()->style()->paddingBottom()); break;
                case CSS_PROP_PADDING_LEFT:
                    style->setPaddingLeft(e->parentNode()->style()->paddingLeft()); break;
                case CSS_PROP_MARGIN_TOP:
                    style->setMarginTop(e->parentNode()->style()->marginTop()); break;
                case CSS_PROP_MARGIN_RIGHT:
                    style->setMarginRight(e->parentNode()->style()->marginRight()); break;
                case CSS_PROP_MARGIN_BOTTOM:
                    style->setMarginBottom(e->parentNode()->style()->marginBottom()); break;
                case CSS_PROP_MARGIN_LEFT:
                    style->setMarginLeft(e->parentNode()->style()->marginLeft()); break;
                case CSS_PROP_TEXT_INDENT:
                    style->setTextIndent(e->parentNode()->style()->textIndent()); break;
                default:
                    return;
                }
        } else if(primitiveValue && !apply) {
            int type = primitiveValue->primitiveType();
            if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                l = Length(computeLength(primitiveValue, style, paintDeviceMetrics), Fixed);
            else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
                l = Length((int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE), Percent);
            else
                return;
            apply = true;
        }
        if(!apply) return;
        switch(prop->m_id)
            {
            case CSS_PROP_MAX_WIDTH:
                style->setMaxWidth(l); break;
            case CSS_PROP_BOTTOM:
                style->setBottom(l); break;
            case CSS_PROP_TOP:
                style->setTop(l); break;
            case CSS_PROP_LEFT:
                style->setLeft(l); break;
            case CSS_PROP_RIGHT:
                style->setRight(l); break;
            case CSS_PROP_WIDTH:
                style->setWidth(l); break;
            case CSS_PROP_MIN_WIDTH:
                style->setMinWidth(l); break;
            case CSS_PROP_PADDING_TOP:
                style->setPaddingTop(l); break;
            case CSS_PROP_PADDING_RIGHT:
                style->setPaddingRight(l); break;
            case CSS_PROP_PADDING_BOTTOM:
                style->setPaddingBottom(l); break;
            case CSS_PROP_PADDING_LEFT:
                style->setPaddingLeft(l); break;
            case CSS_PROP_MARGIN_TOP:
                style->setMarginTop(l); break;
            case CSS_PROP_MARGIN_RIGHT:
                style->setMarginRight(l); break;
            case CSS_PROP_MARGIN_BOTTOM:
                style->setMarginBottom(l); break;
            case CSS_PROP_MARGIN_LEFT:
                style->setMarginLeft(l); break;
            case CSS_PROP_TEXT_INDENT:
                style->setTextIndent(l); break;
            default:
                return;
            }
    }

    case CSS_PROP_MAX_HEIGHT:
        // +inherit +none !can be calculted directly!
        if(primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE)
            apply = true;
    case CSS_PROP_HEIGHT:
    case CSS_PROP_MIN_HEIGHT:
        // +inherit +auto !can be calculted directly!
        if(!prop->m_id == CSS_PROP_MAX_HEIGHT && primitiveValue &&
           primitiveValue->getIdent() == CSS_VAL_AUTO)
            apply = true;
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            switch(prop->m_id)
                {
                case CSS_PROP_MAX_HEIGHT:
                    style->setMaxHeight(e->parentNode()->style()->maxHeight()); break;
                case CSS_PROP_HEIGHT:
                    style->setHeight(e->parentNode()->style()->height()); break;
                case CSS_PROP_MIN_HEIGHT:
                    style->setMinHeight(e->parentNode()->style()->minHeight()); break;
                default:
                    return;
                }
            return;
        }
        if(primitiveValue && !apply)
        {
            int type = primitiveValue->primitiveType();
            if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                l = Length(computeLength(primitiveValue, style, paintDeviceMetrics), Fixed);
            else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
            {
                // ### compute from parents height!!!
                l = Length((int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE), Percent);
            }
            else
                return;
            apply = true;
        }
        if(!apply) return;
        switch(prop->m_id)
        {
        case CSS_PROP_MAX_HEIGHT:
            style->setMaxHeight(l); break;
        case CSS_PROP_HEIGHT:
            style->setHeight(l); break;
        case CSS_PROP_MIN_HEIGHT:
            style->setMinHeight(l); break;
        default:
            return;
        }
        return;

        break;

    case CSS_PROP_VERTICAL_ALIGN:
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setVerticalAlign(e->parentNode()->style()->verticalAlign());
            return;
        }
        if(!primitiveValue) return;
        if(primitiveValue->getIdent()) {

	  khtml::EVerticalAlign align;

	  switch(primitiveValue->getIdent())
	    {
		case CSS_VAL_TOP:
		    align = TOP; break;
		case CSS_VAL_BOTTOM:
		    align = BOTTOM; break;
		case CSS_VAL_MIDDLE:
		    align = MIDDLE; break;
		case CSS_VAL_BASELINE:
		    align = BASELINE; break;
		case CSS_VAL_TEXT_BOTTOM:
		    align = TEXT_BOTTOM; break;
		case CSS_VAL_TEXT_TOP:
		    align = TEXT_TOP; break;
		case CSS_VAL_SUB:
		    align = SUB; break;
		case CSS_VAL_SUPER:
		    align = SUPER; break;
		case CSS_VAL__KONQ_BASELINE_MIDDLE:
		    align = BASELINE_MIDDLE; break;
		default:
		    return;
	    }
	  style->setVerticalAlign(align);
	  return;
        } else {
	  int type = primitiveValue->primitiveType();
	  Length l;
	  if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
	    l = Length( computeLength(primitiveValue, style, paintDeviceMetrics), Fixed );
	  else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
	    l = Length( int( primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE) ), Percent );

	  style->setVerticalAlign( LENGTH );
	  style->setVerticalAlignLength( l );
	}
        break;

    case CSS_PROP_FONT_SIZE:
    {
        QFont f = style->font();
        int oldSize;
        float size = 0;
        int minFontSize = e->ownerDocument()->view()->part()->settings()->minFontSize();

        float toPix = 1.; // fallback
        if ( !khtml::printpainter )
            toPix = paintDeviceMetrics->logicalDpiY()/72.;
        if ( !khtml::printpainter && toPix < 96./72. )
            toPix = 96./72.;

        QValueList<int> standardSizes = e->ownerDocument()->view()->part()->fontSizes();
        if(e->parentNode()) {
            oldSize = e->parentNode()->style()->font().pixelSize();
        } else {
            oldSize = (int)(standardSizes[3]*toPix);
        }

        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            size = oldSize;
        }
        else if(primitiveValue->getIdent())
        {
            switch(primitiveValue->getIdent())
            {
            case CSS_VAL_XX_SMALL:
                size = standardSizes[0]*toPix; break;
            case CSS_VAL_X_SMALL:
                size = standardSizes[1]*toPix; break;
            case CSS_VAL_SMALL:
                size = standardSizes[2]*toPix; break;
            case CSS_VAL_MEDIUM:
                size = standardSizes[3]*toPix; break;
            case CSS_VAL_LARGE:
                size = standardSizes[4]*toPix; break;
            case CSS_VAL_X_LARGE:
                size = standardSizes[5]*toPix; break;
            case CSS_VAL_XX_LARGE:
                size = standardSizes[6]*toPix; break;
            case CSS_VAL__KONQ_XXX_LARGE:
                size = ( standardSizes[6]*toPix*5 )/3; break;
            case CSS_VAL_LARGER:
                // ### use the next bigger standardSize!!!
                size = oldSize * 1.2;
                break;
            case CSS_VAL_SMALLER:
                size = oldSize / 1.2;
                break;
            default:
                return;
            }

        }
        else
        {
            int type = primitiveValue->primitiveType();
            RenderStyle *parentStyle = style; // use the current style as fallback in case we have no parent
            if(e->parentNode())
                parentStyle = e->parentNode()->style();
            if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                size = computeLengthFloat(primitiveValue, parentStyle, paintDeviceMetrics);
            else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
                size = (primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE)
                                  * parentStyle->font().pixelSize()) / 100;
            else
                return;
        }

        if(size <= 0) return;

        // we never want to get smaller than the minimum font size to keep fonts readable
        if(size < minFontSize ) size = minFontSize;

        //kdDebug( 6080 ) << "computed raw font size: " << size << endl;

        const KHTMLSettings *s = e->ownerDocument()->view()->part()->settings();

        setFontSize( f, (int)size, s, paintDeviceMetrics );

        //KGlobal::charsets()->setQFont(f, e->ownerDocument()->view()->part()->settings()->charset);
        style->setFont(f);
        return;
    }

// angle
//    case CSS_PROP_ELEVATION:

// number
//     case CSS_PROP_FONT_SIZE_ADJUST:
//     case CSS_PROP_ORPHANS:
//     case CSS_PROP_PITCH_RANGE:
//     case CSS_PROP_RICHNESS:
//     case CSS_PROP_SPEECH_RATE:
//     case CSS_PROP_STRESS:
//     case CSS_PROP_WIDOWS:
        break;
    case CSS_PROP_Z_INDEX:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setZIndex(e->parentNode()->style()->zIndex());
            return;
        }
        if(!primitiveValue ||
           primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return;
        style->setZIndex((int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER));
        return;
    }

// length, percent, number
    case CSS_PROP_LINE_HEIGHT:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setLineHeight(e->parentNode()->style()->lineHeight());
            return;
        }
        Length lineHeight;
        if(!primitiveValue) return;
        int type = primitiveValue->primitiveType();
        if(primitiveValue->getIdent() == CSS_VAL_NORMAL)
            lineHeight = Length(100, Percent);
        else if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                lineHeight = Length(computeLength(primitiveValue, style, paintDeviceMetrics), Fixed);
        else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
            lineHeight = Length(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE)), Percent);
        else if(type == CSSPrimitiveValue::CSS_NUMBER)
            lineHeight = Length(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER)*100), Percent);
        else
            return;
        style->setLineHeight(lineHeight);
        return;
    }

// number, percent
//    case CSS_PROP_VOLUME:

// frequency
//    case CSS_PROP_PITCH:
//        break;

// string
    case CSS_PROP_TEXT_ALIGN:
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setTextAlign(e->parentNode()->style()->textAlign());
            return;
        }
        if(!primitiveValue) return;
        if(primitiveValue->getIdent())
        {
            style->setTextAlign( (ETextAlign) (primitiveValue->getIdent() - CSS_VAL_LEFT) );
        }
	return;
    }
// rect
    case CSS_PROP_CLIP:
    {
	Length top;
	Length right;
	Length bottom;
	Length left;
	if ( value->valueType() == CSSValue::CSS_INHERIT ) {
	    RenderStyle *inherited = e->parentNode()->style();
	    top = inherited->clipTop();
	    right = inherited->clipRight();
	    bottom = inherited->clipBottom();
	    left = inherited->clipLeft();
	} else if ( !primitiveValue ) {
	    break;
	} else if ( primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_RECT ) {
	    RectImpl *rect = primitiveValue->getRectValue();
	    if ( !rect )
		break;
	    top = convertToLength( rect->top(), style, paintDeviceMetrics );
	    right = convertToLength( rect->right(), style, paintDeviceMetrics );
	    bottom = convertToLength( rect->bottom(), style, paintDeviceMetrics );
	    left = convertToLength( rect->left(), style, paintDeviceMetrics );
	    
	} else if ( primitiveValue->getIdent() != CSS_VAL_AUTO ) {
	    break;
	}
// 	qDebug("setting top to %d", top.value );
// 	qDebug("setting right to %d", right.value );
// 	qDebug("setting bottom to %d", bottom.value );
// 	qDebug("setting left to %d", left.value );
	style->setClipTop( top );
	style->setClipRight( right );
	style->setClipBottom( bottom );
	style->setClipLeft( left );
	    
        // rect, ident
        break;
    }
    
// lists
    case CSS_PROP_CONTENT:
        // list of string, uri, counter, attr, i
    case CSS_PROP_COUNTER_INCREMENT:
        // list of CSS2CounterIncrement
    case CSS_PROP_COUNTER_RESET:
        // list of CSS2CounterReset
        break;
    case CSS_PROP_FONT_FAMILY:
        // list of strings and ids
    {
// 	QTime qt;
// 	qt.start();
        if(!value->isValueList()) return;
        CSSValueListImpl *list = static_cast<CSSValueListImpl *>(value);
        int len = list->length();
	const KHTMLSettings *s = e->ownerDocument()->view()->part()->settings();
	QString available = s->availableFamilies();
	QFont f = style->font();
	QString family;
	//kdDebug(0) << "searching for font... available:" << available << endl;
        for(int i = 0; i < len; i++)
        {
            CSSValueImpl *item = list->item(i);
            if(!item->isPrimitiveValue()) continue;
            CSSPrimitiveValueImpl *val = static_cast<CSSPrimitiveValueImpl *>(item);
            if(!val->primitiveType() == CSSPrimitiveValue::CSS_STRING) return;
            DOMStringImpl *str = val->getStringValue();
            QString face = QConstString(str->s, str->l).string().lower();
	    // a languge tag is often added in braces at the end. Remove it.
	    face = face.replace(QRegExp(" \\(.*\\)$"), "");
            //kdDebug(0) << "searching for face '" << face << "'" << endl;
            if(face == "serif")
                face = s->serifFontName();
            else if(face == "sans-serif")
                face = s->sansSerifFontName();
            else if( face == "cursive")
                face = s->cursiveFontName();
            else if( face == "fantasy")
                face = s->fantasyFontName();
            else if( face == "monospace")
                face = s->fixedFontName();
            else if( face == "konq_default")
                face = s->stdFontName();

	    int pos;
	    if( (pos = available.find( face, 0, false)) == -1 ) {
		QString str = face;
                int p = face.find(' ');
                // Arial Blk --> Arial
                // MS Sans Serif --> Sans Serif
                if(p > 0 && (int)str.length() - p > p)
                    str = str.mid( p+1 );
                else
                    str.truncate( p );
		pos = available.find( str, 0, false);
	    }

	    if ( pos != -1 ) {
		int pos1 = available.findRev( ',', pos ) + 1;
		pos = available.find( ',', pos );
		if ( pos == -1 )
		    pos = available.length();
		family = available.mid( pos1, pos - pos1 );
		f.setFamily( family );
		KGlobal::charsets()->setQFont(f, s->charset() );
		//kdDebug() << "font charset is " << f.charSet() << " script = " << s->script() << endl;
		if ( s->charset() == s->script() || KGlobal::charsets()->supportsScript( f, s->script() ) ) {
		    //kdDebug() << "=====> setting font family to " << family << endl;
		    style->setFont(f);
		    return;
		}
	    }
//            kdDebug( 6080 ) << "no match for font family " << face << ", got " << f.family() << endl;
        }
//	kdDebug() << "khtml::setFont: time=" << qt.elapsed() << endl;
        break;
    }
    case CSS_PROP_QUOTES:
        // list of strings or i
    case CSS_PROP_SIZE:
        // ### look up
      break;
    case CSS_PROP_TEXT_DECORATION:
        // list of ident
        // ### no list at the moment
    {
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setTextDecoration(e->parentNode()->style()->textDecoration());
            style->setTextDecorationColor(e->parentNode()->style()->textDecorationColor());
            return;
        }
        int t = TDNONE;
        if(primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE) {
	    // do nothing
	} else {
	    if(!value->isValueList()) return;
	    CSSValueListImpl *list = static_cast<CSSValueListImpl *>(value);
	    int len = list->length();
	    for(int i = 0; i < len; i++)
	    {
		CSSValueImpl *item = list->item(i);
		if(!item->isPrimitiveValue()) continue;
		primitiveValue = static_cast<CSSPrimitiveValueImpl *>(item);
		switch(primitiveValue->getIdent())
		{
		    case CSS_VAL_NONE:
			t = TDNONE; break;
		    case CSS_VAL_UNDERLINE:
			t |= UNDERLINE; break;
		    case CSS_VAL_OVERLINE:
			t |= OVERLINE; break;
		    case CSS_VAL_LINE_THROUGH:
			t |= LINE_THROUGH; break;
		    case CSS_VAL_BLINK:
			t |= BLINK; break;
		    default:
			return;
		}
	    }
        }
	style->setTextDecoration(t);
	style->setTextDecorationColor(style->color());
        break;
    }

    case CSS_PROP__KONQ_FLOW_MODE:
        if(value->valueType() == CSSValue::CSS_INHERIT)
        {
            if(!e->parentNode()) return;
            style->setFlowAroundFloats(e->parentNode()->style()->flowAroundFloats());
            return;
        }
        if(!primitiveValue) return;
        if(primitiveValue->getIdent())
        {
            style->setFlowAroundFloats( primitiveValue->getIdent() == CSS_VAL__KONQ_AROUND_FLOATS );
            return;
        }
        break;
//     case CSS_PROP_VOICE_FAMILY:
//         // list of strings and i
//         break;

// shorthand properties
    case CSS_PROP_BACKGROUND:
        if(value->valueType() != CSSValue::CSS_INHERIT || !e->parentNode()) return;
        style->setBackgroundColor(e->parentNode()->style()->backgroundColor());
        style->setBackgroundImage(e->parentNode()->style()->backgroundImage());
        style->setBackgroundRepeat(e->parentNode()->style()->backgroundRepeat());
        style->setBackgroundAttachment(e->parentNode()->style()->backgroundAttachment());
//      style->setBackgroundPosition(e->parentNode()->style()->backgroundPosition());

        break;
    case CSS_PROP_BORDER_COLOR:
        if(primitiveValue && primitiveValue->getIdent() == CSS_VAL_TRANSPARENT)
        {
            style->setBorderTopColor(QColor());
            style->setBorderBottomColor(QColor());
            style->setBorderLeftColor(QColor());
            style->setBorderRightColor(QColor());
            return;
        }
    case CSS_PROP_BORDER:
    case CSS_PROP_BORDER_STYLE:
    case CSS_PROP_BORDER_WIDTH:
        if(value->valueType() != CSSValue::CSS_INHERIT || !e->parentNode()) return;

        if(prop->m_id == CSS_PROP_BORDER || prop->m_id == CSS_PROP_BORDER_COLOR)
        {
            style->setBorderTopColor(e->parentNode()->style()->borderTopColor());
            style->setBorderBottomColor(e->parentNode()->style()->borderBottomColor());
            style->setBorderLeftColor(e->parentNode()->style()->borderLeftColor());
            style->setBorderRightColor(e->parentNode()->style()->borderRightColor());
        }
        if(prop->m_id == CSS_PROP_BORDER || prop->m_id == CSS_PROP_BORDER_STYLE)
        {
            style->setBorderTopStyle(e->parentNode()->style()->borderTopStyle());
            style->setBorderBottomStyle(e->parentNode()->style()->borderBottomStyle());
            style->setBorderLeftStyle(e->parentNode()->style()->borderLeftStyle());
            style->setBorderRightStyle(e->parentNode()->style()->borderRightStyle());
        }
        if(prop->m_id == CSS_PROP_BORDER || prop->m_id == CSS_PROP_BORDER_WIDTH)
        {
            style->setBorderTopWidth(e->parentNode()->style()->borderTopWidth());
            style->setBorderBottomWidth(e->parentNode()->style()->borderBottomWidth());
            style->setBorderLeftWidth(e->parentNode()->style()->borderLeftWidth());
            style->setBorderRightWidth(e->parentNode()->style()->borderRightWidth());
        }
        return;
    case CSS_PROP_BORDER_TOP:
        if(value->valueType() != CSSValue::CSS_INHERIT || !e->parentNode()) return;
        style->setBorderTopColor(e->parentNode()->style()->borderTopColor());
        style->setBorderTopStyle(e->parentNode()->style()->borderTopStyle());
        style->setBorderTopWidth(e->parentNode()->style()->borderTopWidth());
        return;
    case CSS_PROP_BORDER_RIGHT:
        if(value->valueType() != CSSValue::CSS_INHERIT || !e->parentNode()) return;
        style->setBorderRightColor(e->parentNode()->style()->borderRightColor());
        style->setBorderRightStyle(e->parentNode()->style()->borderRightStyle());
        style->setBorderRightWidth(e->parentNode()->style()->borderRightWidth());
        return;
    case CSS_PROP_BORDER_BOTTOM:
        if(value->valueType() != CSSValue::CSS_INHERIT || !e->parentNode()) return;
        style->setBorderBottomColor(e->parentNode()->style()->borderBottomColor());
        style->setBorderBottomStyle(e->parentNode()->style()->borderBottomStyle());
        style->setBorderBottomWidth(e->parentNode()->style()->borderBottomWidth());
        return;
    case CSS_PROP_BORDER_LEFT:
        if(value->valueType() != CSSValue::CSS_INHERIT || !e->parentNode()) return;
        style->setBorderLeftColor(e->parentNode()->style()->borderLeftColor());
        style->setBorderLeftStyle(e->parentNode()->style()->borderLeftStyle());
        style->setBorderLeftWidth(e->parentNode()->style()->borderLeftWidth());
        return;
    case CSS_PROP_MARGIN:
        if(value->valueType() != CSSValue::CSS_INHERIT || !e->parentNode()) return;
        style->setMarginTop(e->parentNode()->style()->marginTop());
        style->setMarginBottom(e->parentNode()->style()->marginBottom());
        style->setMarginLeft(e->parentNode()->style()->marginLeft());
        style->setMarginRight(e->parentNode()->style()->marginRight());
        return;
    case CSS_PROP_PADDING:
        if(value->valueType() != CSSValue::CSS_INHERIT || !e->parentNode()) return;
        style->setPaddingTop(e->parentNode()->style()->paddingTop());
        style->setPaddingBottom(e->parentNode()->style()->paddingBottom());
        style->setPaddingLeft(e->parentNode()->style()->paddingLeft());
        style->setPaddingRight(e->parentNode()->style()->paddingRight());
        return;


//     case CSS_PROP_CUE:
    case CSS_PROP_FONT:
    case CSS_PROP_LIST_STYLE:
    case CSS_PROP_OUTLINE:
//    case CSS_PROP_PAUSE:
        break;
    default:
        return;
    }
}

