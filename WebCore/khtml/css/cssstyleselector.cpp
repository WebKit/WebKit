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
 */

#include "css/cssstyleselector.h"
#include "rendering/render_style.h"
#include "css/css_stylesheetimpl.h"
#include "css/css_ruleimpl.h"
#include "css/css_valueimpl.h"
#include "css/csshelper.h"
#include "rendering/render_object.h"
#include "html/html_documentimpl.h"
#include "xml/dom_elementimpl.h"
#include "dom/css_rule.h"
#include "dom/css_value.h"
#include "khtml_factory.h"
#include "khtmlpart_p.h"
using namespace khtml;
using namespace DOM;

#include "css/cssproperties.h"
#include "css/cssvalues.h"

#include "misc/khtmllayout.h"
#include "khtml_settings.h"
#include "misc/htmlhashes.h"
#include "misc/helper.h"
#include "misc/loader.h"

#include "rendering/font.h"

#include "khtmlview.h"
#include "khtml_part.h"

#include <kstandarddirs.h>
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
#include <stdlib.h>

namespace khtml {

CSSStyleSelectorList *CSSStyleSelector::defaultStyle = 0;
CSSStyleSelectorList *CSSStyleSelector::defaultPrintStyle = 0;
CSSStyleSheetImpl *CSSStyleSelector::defaultSheet = 0;
RenderStyle* CSSStyleSelector::displayNoneStyle = 0;

static CSSStyleSelector::Encodedurl *encodedurl = 0;

enum PseudoState { PseudoUnknown, PseudoNone, PseudoLink, PseudoVisited};
static PseudoState pseudoState;


CSSStyleSelector::CSSStyleSelector( DocumentImpl* doc, QString userStyleSheet, StyleSheetListImpl *styleSheets,
                                    const KURL &url, bool _strictParsing )
{
    init();

    KHTMLView* view = doc->view();
    strictParsing = _strictParsing;
    settings = view ? view->part()->settings() : 0;
    if(!defaultStyle) loadDefaultStyle(settings);
    m_medium = view ? view->mediaType() : QString("all");

    selectors = 0;
    selectorCache = 0;
    properties = 0;
    userStyle = 0;
    userSheet = 0;
    paintDeviceMetrics = doc->paintDeviceMetrics();

	if(paintDeviceMetrics) // this may be null, not everyone uses khtmlview (Niko)
	    computeFontSizes(paintDeviceMetrics, view ? view->part()->zoomFactor() : 100);

    if ( !userStyleSheet.isEmpty() ) {
        userSheet = new DOM::CSSStyleSheetImpl(doc);
        userSheet->parseString( DOMString( userStyleSheet ) );

        userStyle = new CSSStyleSelectorList();
        userStyle->append( userSheet, m_medium );
    }

    // add stylesheets from document
    authorStyle = new CSSStyleSelectorList();


    QPtrListIterator<StyleSheetImpl> it( styleSheets->styleSheets );
    for ( ; it.current(); ++it ) {
        if ( it.current()->isCSSStyleSheet() ) {
            authorStyle->append( static_cast<CSSStyleSheetImpl*>( it.current() ),
                                 m_medium );
        }
    }

    buildLists();

    //kdDebug( 6080 ) << "number of style sheets in document " << authorStyleSheets.count() << endl;
    //kdDebug( 6080 ) << "CSSStyleSelector: author style has " << authorStyle->count() << " elements"<< endl;

    KURL u = url;

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

CSSStyleSelector::CSSStyleSelector( CSSStyleSheetImpl *sheet )
{
    init();

    if(!defaultStyle) loadDefaultStyle();
    m_medium = sheet->doc()->view()->mediaType();

    authorStyle = new CSSStyleSelectorList();
    authorStyle->append( sheet, m_medium );
}

void CSSStyleSelector::init()
{
    element = 0;
    settings = 0;
    paintDeviceMetrics = 0;
    propsToApply = (CSSOrderedProperty **)malloc(128*sizeof(CSSOrderedProperty *));
    pseudoProps = (CSSOrderedProperty **)malloc(128*sizeof(CSSOrderedProperty *));
    propsToApplySize = 128;
    pseudoPropsSize = 128;
}

CSSStyleSelector::~CSSStyleSelector()
{
    clearLists();
    delete authorStyle;
    delete userStyle;
    delete userSheet;
    free(propsToApply);
    free(pseudoProps);
}

void CSSStyleSelector::addSheet( CSSStyleSheetImpl *sheet )
{
    m_medium = sheet->doc()->view()->mediaType();
    authorStyle->append( sheet, m_medium );
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

    defaultSheet = new DOM::CSSStyleSheetImpl((DOM::CSSStyleSheetImpl * ) 0);
    defaultSheet->parseString( str );

    defaultStyle = new CSSStyleSelectorList();
    defaultStyle->append( defaultSheet );

    defaultPrintStyle = new CSSStyleSelectorList();
    defaultPrintStyle->append( defaultSheet, "print" );
    //kdDebug() << "CSSStyleSelector: default style has " << defaultStyle->count() << " elements"<< endl;
}

void CSSStyleSelector::clear()
{
    delete defaultStyle;
    delete defaultPrintStyle;
    delete defaultSheet;
    delete displayNoneStyle;
    defaultStyle = 0;
    defaultPrintStyle = 0;
    defaultSheet = 0;
    displayNoneStyle = 0;
}

#define MAXFONTSIZES 15

void CSSStyleSelector::computeFontSizes(QPaintDeviceMetrics* paintDeviceMetrics,  int zoomFactor)
{
    computeFontSizesFor(paintDeviceMetrics, zoomFactor, m_fontSizes, false);
    computeFontSizesFor(paintDeviceMetrics, zoomFactor, m_fixedFontSizes, true);
}

void CSSStyleSelector::computeFontSizesFor(QPaintDeviceMetrics* paintDeviceMetrics, int zoomFactor, QValueList<int>& fontSizes, bool isFixed)
{
#ifdef APPLE_CHANGES
    // We don't want to scale the settings by the dpi.
    const float toPix = 1;
#else
    // ### get rid of float / double
    float toPix = paintDeviceMetrics->logicalDpiY()/72.;
    if (toPix  < 96./72.) toPix = 96./72.;
#endif

    fontSizes.clear();
    const float factor = 1.2;
    float scale = 1.0 / (factor*factor*factor);
    float mediumFontSize;
    float minFontSize;
    if (!khtml::printpainter) {
        scale *= zoomFactor / 100.0;
	if (isFixed)
	    mediumFontSize = settings->mediumFixedFontSize() * toPix;
	else
	    mediumFontSize = settings->mediumFontSize() * toPix;
        minFontSize = settings->minFontSize() * toPix;
    }
    else {
        // ## depending on something / configurable ?
        mediumFontSize = 12;
        minFontSize = 6;
    }

    for ( int i = 0; i < MAXFONTSIZES; i++ ) {
        fontSizes << int(KMAX( mediumFontSize * scale + 0.5f, minFontSize));
        scale *= factor;
    }
}

#undef MAXFONTSIZES

static inline void bubbleSort( CSSOrderedProperty **b, CSSOrderedProperty **e )
{
    while( b < e ) {
	bool swapped = FALSE;
        CSSOrderedProperty **y = e+1;
	CSSOrderedProperty **x = e;
        CSSOrderedProperty **swappedPos = 0; // quiet gcc warning
	do {
	    if ( !((**(--x)) < (**(--y))) ) {
		swapped = TRUE;
                swappedPos = x;
                CSSOrderedProperty *tmp = *y;
                *y = *x;
                *x = tmp;
	    }
	} while( x != b );
	if ( !swapped ) break;
        b = swappedPos + 1;
    }
}

RenderStyle *CSSStyleSelector::styleForElement(ElementImpl *e, int state)
{
    if (!e->getDocument()->haveStylesheetsLoaded()) {
        if (!displayNoneStyle) {
            displayNoneStyle = new RenderStyle();
	    displayNoneStyle->setDisplay(NONE);
	    displayNoneStyle->ref();
	}
	return displayNoneStyle;
    }
  
    // set some variables we will need
    dynamicState = state;
    usedDynamicStates = StyleSelector::None;
    ::encodedurl = &encodedurl;
    pseudoState = PseudoUnknown;

    element = e;
    parentNode = e->parentNode();
    parentStyle = ( parentNode && parentNode->renderer()) ? parentNode->renderer()->style() : 0;
    view = element->getDocument()->view();
    part = view->part();
    settings = part->settings();
    paintDeviceMetrics = element->getDocument()->paintDeviceMetrics();

    unsigned int numPropsToApply = 0;
    unsigned int numPseudoProps = 0;

    // try to sort out most style rules as early as possible.
    // ### implement CSS3 namespace support
    int cssTagId = (e->id() & NodeImpl::IdLocalMask);
    int smatch = 0;
    int schecked = 0;

    for ( unsigned int i = 0; i < selectors_size; i++ ) {
	int tag = selectors[i]->tag;
	if ( cssTagId == tag || tag == -1 ) {
	    ++schecked;

	    checkSelector( i, e );

	    if ( selectorCache[i].state == Applies ) {
		++smatch;

		//qDebug("adding property" );
		for ( unsigned int p = 0; p < selectorCache[i].props_size; p += 2 )
		    for ( unsigned int j = 0; j < (unsigned int )selectorCache[i].props[p+1]; ++j ) {
                        if (numPropsToApply >= propsToApplySize ) {
                            propsToApplySize *= 2;
			    propsToApply = (CSSOrderedProperty **)realloc( propsToApply, propsToApplySize*sizeof( CSSOrderedProperty * ) );
			}
			propsToApply[numPropsToApply++] = properties[selectorCache[i].props[p]+j];
		    }
	    } else if ( selectorCache[i].state == AppliesPseudo ) {
		for ( unsigned int p = 0; p < selectorCache[i].props_size; p += 2 )
		    for ( unsigned int j = 0; j < (unsigned int) selectorCache[i].props[p+1]; ++j ) {
                        if (numPseudoProps >= pseudoPropsSize ) {
                            pseudoPropsSize *= 2;
			    pseudoProps = (CSSOrderedProperty **)realloc( pseudoProps, pseudoPropsSize*sizeof( CSSOrderedProperty * ) );
			}
			pseudoProps[numPseudoProps++] = properties[selectorCache[i].props[p]+j];
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
    if(e->m_styleDecls)
	numPropsToApply = addInlineDeclarations( e->m_styleDecls, numPropsToApply );

    bubbleSort( propsToApply, propsToApply+numPropsToApply-1 );
    bubbleSort( pseudoProps, pseudoProps+numPseudoProps-1 );

    RenderStyle *style = new RenderStyle();
    if( parentStyle )
        style->inheritFrom( parentStyle );
    else
	parentStyle = style;

    // This member will be set to true if a rule specifies a font size
    // explicitly.  If they do this, then we don't need to check for a shift
    // in default size caused by a change in generic family. -dwh
    m_fontSizeSpecified = false;

    //qDebug("applying properties, count=%d", propsToApply->count() );

    // we can't apply style rules without a view() and a part. This
    // tends to happen on delayed destruction of widget Renderobjects
    if ( part ) {
        fontDirty = false;

        if (numPropsToApply ) {
            CSSStyleSelector::style = style;
            for (unsigned int i = 0; i < numPropsToApply; ++i) {
		if ( fontDirty && propsToApply[i]->priority >= (1 << 30) ) {
		    // we are past the font properties, time to update to the
		    // correct font
		    checkForGenericFamilyChange(style, parentStyle);
		    CSSStyleSelector::style->htmlFont().update( paintDeviceMetrics );
		    fontDirty = false;
		}
                applyRule( propsToApply[i]->prop );
	    }
	    if ( fontDirty ) {
	        checkForGenericFamilyChange(style, parentStyle);
		CSSStyleSelector::style->htmlFont().update( paintDeviceMetrics );
	    }
        }

        if ( numPseudoProps ) {
	    fontDirty = false;
            //qDebug("%d applying %d pseudo props", e->cssTagId(), pseudoProps->count() );
            for (unsigned int i = 0; i < numPseudoProps; ++i) {
		if ( fontDirty && pseudoProps[i]->priority >= (1 << 30) ) {
		    // we are past the font properties, time to update to the
		    // correct font
		    //We have to do this for all pseudo styles
		    RenderStyle *pseudoStyle = style->pseudoStyle;
		    while ( pseudoStyle ) {
			pseudoStyle->htmlFont().update( paintDeviceMetrics );
			pseudoStyle = pseudoStyle->pseudoStyle;
		    }
		    fontDirty = false;
		}

                RenderStyle *pseudoStyle;
                pseudoStyle = style->getPseudoStyle(pseudoProps[i]->pseudoId);
                if (!pseudoStyle)
                {
                    pseudoStyle = style->addPseudoStyle(pseudoProps[i]->pseudoId);
                    if (pseudoStyle)
                        pseudoStyle->inheritFrom( style );
                }

		CSSStyleSelector::style = pseudoStyle;
                if ( pseudoStyle )
                    applyRule( pseudoProps[i]->prop );
            }

	    if ( fontDirty ) {
		RenderStyle *pseudoStyle = style->pseudoStyle;
		while ( pseudoStyle ) {
		    pseudoStyle->htmlFont().update( paintDeviceMetrics );
		    pseudoStyle = pseudoStyle->pseudoStyle;
		}
	    }
        }
    }

    if ( usedDynamicStates & StyleSelector::Hover )
	style->setHasHover();
    if ( usedDynamicStates & StyleSelector::Active )
	style->setHasActive();

    return style;
}

unsigned int CSSStyleSelector::addInlineDeclarations(DOM::CSSStyleDeclarationImpl *decl,
					    unsigned int numProps )
{
    QPtrList<CSSProperty> *values = decl->values();
    if(!values) return numProps;
    int len = values->count();

    if ( inlineProps.size() < (uint)len )
	inlineProps.resize( len+1 );
    if (numProps + len >= propsToApplySize ) {
        propsToApplySize += propsToApplySize;
        propsToApply = (CSSOrderedProperty **)realloc( propsToApply, propsToApplySize*sizeof( CSSOrderedProperty * ) );
    }

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
	case CSS_PROP_FONT_STYLE:
        case CSS_PROP_FONT_SIZE:
	case CSS_PROP_FONT_WEIGHT:
	case CSS_PROP_FONT_FAMILY:
        case CSS_PROP_FONT:
        case CSS_PROP_COLOR:
        case CSS_PROP_BACKGROUND_IMAGE:
	case CSS_PROP_DISPLAY:
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
	propsToApply[numProps++] = array++;
    }
    return numProps;
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
	    if ( dynamicPseudo != RenderStyle::NOPSEUDO ) {
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

    if((e->id() & NodeImpl::IdLocalMask) != uint(sel->tag) && sel->tag != -1) return false;

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
            int spacePos = value.find(' ', 0);
            if (spacePos == -1) {
                // There is no list, just a single item.  We can avoid
                // allocing QStrings and just treat this as an exact
                // match check.
                if( (strictParsing && strcmp(sel->value, value) ) ||
                     (!strictParsing && strcasecmp(sel->value, value)))
                    return false;
                break;
            }
            QString str = value.string();
            QString selStr = sel->value.string();
            int pos = str.find(selStr, 0, strictParsing);
            if(pos == -1) return false;
            if(pos && str[pos-1] != ' ') return false;
            pos += selStr.length();
            if(pos < (int)str.length() && str[pos] != ' ') return false;
            break;
        }
        case CSSSelector::Contain:
        {
            //kdDebug( 6080 ) << "checking for contains match" << endl;
            QString str = value.string();
            QString selStr = sel->value.string();
            int pos = str.find(selStr, 0, strictParsing);
            if(pos == -1) return false;
            break;
        }
        case CSSSelector::Begin:
        {
            //kdDebug( 6080 ) << "checking for beginswith match" << endl;
            QString str = value.string();
            QString selStr = sel->value.string();
            int pos = str.find(selStr, 0, strictParsing);
            if(pos != 0) return false;
            break;
        }
        case CSSSelector::End:
        {
            //kdDebug( 6080 ) << "checking for endswith match" << endl;
            QString str = value.string();
            QString selStr = sel->value.string();
	    if (strictParsing && !str.endsWith(selStr)) return false;
	    if (!strictParsing) {
	        int pos = str.length() - selStr.length();
		if (pos < 0 || pos != str.find(selStr, pos, false) )
		    return false;
	    }
            break;
        }
        case CSSSelector::Hyphen:
        {
            //kdDebug( 6080 ) << "checking for hyphen match" << endl;
            QString str = value.string();
            QString selStr = sel->value.string();
            if(str.length() < selStr.length()) return false;
            // Check if str begins with selStr:
            if(str.find(selStr, 0, strictParsing) != 0) return false;
            // It does. Check for exact match or following '-':
            if(str.length() != selStr.length()
                && str[selStr.length()] != '-') return false;
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
//	kdDebug() << "CSSOrderedRule::pseudo " << value << endl;
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
	} else if ( value == "before" ) {
            dynamicPseudo = RenderStyle::BEFORE;
            return true;
        } else if ( value == "after" ) {
            dynamicPseudo = RenderStyle::AFTER;
            return true;
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

    QPtrList<CSSSelector> selectorList;
    CSSOrderedPropertyList propertyList;

    if(m_medium == "print" && defaultPrintStyle)
      defaultPrintStyle->collect( &selectorList, &propertyList, Default,
        Default );
    else if(defaultStyle) defaultStyle->collect( &selectorList, &propertyList,
      Default, Default );
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

    unsigned int* offsets = new unsigned int[selectors_size];
    if(properties[0])
	offsets[properties[0]->selector] = 0;
    for(unsigned int p = 1; p < properties_size; ++p) {

	if(!properties[p] || (properties[p]->selector != properties[p - 1]->selector)) {
	    unsigned int sel = properties[p - 1]->selector;
            int* newprops = new int[selectorCache[sel].props_size+2];
            for ( unsigned int i=0; i < selectorCache[sel].props_size; i++ )
                newprops[i] = selectorCache[sel].props[i];

	    newprops[selectorCache[sel].props_size] = offsets[sel];
	    newprops[selectorCache[sel].props_size+1] = p - offsets[sel];
            delete [] selectorCache[sel].props;
            selectorCache[sel].props = newprops;
            selectorCache[sel].props_size += 2;

	    if(properties[p]) {
		sel = properties[p]->selector;
		offsets[sel] = p;
            }
        }
    }
    delete [] offsets;


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
    : QPtrList<CSSOrderedRule>()
{
    setAutoDelete(true);
}
CSSStyleSelectorList::~CSSStyleSelectorList()
{
}

void CSSStyleSelectorList::append( CSSStyleSheetImpl *sheet,
                                   const DOMString &medium )
{
    if(!sheet || !sheet->isCSSStyleSheet()) return;

    // No media implies "all", but if a medialist exists it must
    // contain our current medium
    if( sheet->media() && !sheet->media()->contains( medium ) )
        return; // style sheet not applicable for this medium

    int len = sheet->length();

    for(int i = 0; i< len; i++)
    {
        StyleBaseImpl *item = sheet->item(i);
        if(item->isStyleRule())
        {
            CSSStyleRuleImpl *r = static_cast<CSSStyleRuleImpl *>(item);
            QPtrList<CSSSelector> *s = r->selector();
            for(int j = 0; j < (int)s->count(); j++)
            {
                CSSOrderedRule *rule = new CSSOrderedRule(r, s->at(j), count());
		QPtrList<CSSOrderedRule>::append(rule);
                //kdDebug( 6080 ) << "appending StyleRule!" << endl;
            }
        }
        else if(item->isImportRule())
        {
            CSSImportRuleImpl *import = static_cast<CSSImportRuleImpl *>(item);

            //kdDebug( 6080 ) << "@import: Media: "
            //                << import->media()->mediaText().string() << endl;

            if( !import->media() || import->media()->contains( medium ) )
            {
                CSSStyleSheetImpl *importedSheet = import->styleSheet();
                append( importedSheet, medium );
            }
        }
        else if( item->isMediaRule() )
        {
            CSSMediaRuleImpl *r = static_cast<CSSMediaRuleImpl *>( item );
            CSSRuleListImpl *rules = r->cssRules();

            //DOMString mediaText = media->mediaText();
            //kdDebug( 6080 ) << "@media: Media: "
            //                << r->media()->mediaText().string() << endl;

            if( ( !r->media() || r->media()->contains( medium ) ) && rules)
            {
                // Traverse child elements of the @import rule. Since
                // many elements are not allowed as child we do not use
                // a recursive call to append() here
                for( unsigned j = 0; j < rules->length(); j++ )
                {
                    //kdDebug( 6080 ) << "*** Rule #" << j << endl;

                    CSSRuleImpl *childItem = rules->item( j );
                    if( childItem->isStyleRule() )
                    {
                        // It is a StyleRule, so append it to our list
                        CSSStyleRuleImpl *styleRule =
                                static_cast<CSSStyleRuleImpl *>( childItem );

                        QPtrList<CSSSelector> *s = styleRule->selector();
                        for( int j = 0; j < ( int ) s->count(); j++ )
                        {
                            CSSOrderedRule *orderedRule = new CSSOrderedRule(
                                            styleRule, s->at( j ), count() );
                	    QPtrList<CSSOrderedRule>::append( orderedRule );
                        }
                    }
                    else
                    {
                        //kdDebug( 6080 ) << "Ignoring child rule of "
                        //    "ImportRule: rule is not a StyleRule!" << endl;
                    }
                }   // for rules
            }   // if rules
            else
            {
                //kdDebug( 6080 ) << "CSSMediaRule not rendered: "
                //                << "rule empty or wrong medium!" << endl;
            }
        }
        // ### include other rules
    }
}


void CSSStyleSelectorList::collect( QPtrList<CSSSelector> *selectorList, CSSOrderedPropertyList *propList,
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

int CSSOrderedPropertyList::compareItems(QPtrCollection::Item i1, QPtrCollection::Item i2)
{
    int diff =  static_cast<CSSOrderedProperty *>(i1)->priority
        - static_cast<CSSOrderedProperty *>(i2)->priority;
    return diff ? diff : static_cast<CSSOrderedProperty *>(i1)->position
        - static_cast<CSSOrderedProperty *>(i2)->position;
}

void CSSOrderedPropertyList::append(DOM::CSSStyleDeclarationImpl *decl, uint selector, uint specificity,
				    Source regular, Source important )
{
    QPtrList<CSSProperty> *values = decl->values();
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
	case CSS_PROP_FONT_STYLE:
        case CSS_PROP_FONT_SIZE:
	case CSS_PROP_FONT_WEIGHT:
	case CSS_PROP_FONT_FAMILY:
        case CSS_PROP_FONT:
        case CSS_PROP_COLOR:
        case CSS_PROP_BACKGROUND_IMAGE:
	case CSS_PROP_DISPLAY:
            // these have to be applied first, because other properties use the computed
            // values of these porperties.
	    first = true;
            break;
        default:
            break;
        }

	QPtrList<CSSOrderedProperty>::append(new CSSOrderedProperty(prop, selector,
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
	    l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), Fixed);
	else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
	    l = Length(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE)), Percent);
	else if(type == CSSPrimitiveValue::CSS_NUMBER)
	    l = Length(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER)*100), Percent);
	else if ( ok )
	    *ok = false;
    }
    return l;
}

void CSSStyleSelector::applyRule( DOM::CSSProperty *prop )
{
    CSSValueImpl *value = prop->value();

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
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if( !parentNode ) return;
            style->setBackgroundAttachment(parentStyle->backgroundAttachment());
            return;
        }
        if(!primitiveValue) break;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_FIXED:
            {
                style->setBackgroundAttachment(false);
		// only use slow repaints if we actually have a background pixmap
                if( style->backgroundImage() )
                    view->useSlowRepaints();
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT) {
            if(!parentNode) return;
            style->setBackgroundRepeat(parentStyle->backgroundRepeat());
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            style->setBorderCollapse(parentStyle->borderCollapse());
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
	EBorderStyle s;
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            switch(prop->m_id)
            {
            case CSS_PROP_BORDER_TOP_STYLE:
                s = parentStyle->borderTopStyle();
                break;
            case CSS_PROP_BORDER_RIGHT_STYLE:
                s = parentStyle->borderRightStyle();
                break;
            case CSS_PROP_BORDER_BOTTOM_STYLE:
                s = parentStyle->borderBottomStyle();
                break;
            case CSS_PROP_BORDER_LEFT_STYLE:
                s = parentStyle->borderLeftStyle();
                break;
            case CSS_PROP_OUTLINE_STYLE:
                s = parentStyle->outlineStyle();
                break;
	    default:
                return;
        }
        } else {
	    if(!primitiveValue) return;
	    s = (EBorderStyle) (primitiveValue->getIdent() - CSS_VAL_NONE);
	}
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            style->setCaptionSide(parentStyle->captionSide());
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            style->setClear(parentStyle->clear());
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            style->setDirection(parentStyle->direction());
            break;
        }
        if(!primitiveValue) break;
        style->setDirection( (EDirection) (primitiveValue->getIdent() - CSS_VAL_LTR) );
        return;
    }
    case CSS_PROP_DISPLAY:
    {
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            style->setDisplay(parentStyle->display());
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            style->setFloating(parentStyle->floating());
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
        FontDef fontDef = style->htmlFont().fontDef;
        if(value->cssValueType() == CSSValue::CSS_INHERIT) {
            if(!parentNode) return;
            fontDef.italic = parentStyle->htmlFont().fontDef.italic;
	} else {
	    if(!primitiveValue) return;
	    switch(primitiveValue->getIdent()) {
		case CSS_VAL_OBLIQUE:
		// ### oblique is the same as italic for the moment...
		case CSS_VAL_ITALIC:
		    fontDef.italic = true;
		    break;
		case CSS_VAL_NORMAL:
		    fontDef.italic = false;
		    break;
		default:
		    return;
	    }
	}
        if (style->setFontDef( fontDef ))
	fontDirty = true;
        break;
    }


    case CSS_PROP_FONT_VARIANT:
    {
        if(value->cssValueType() == CSSValue::CSS_INHERIT) {
            if(!parentNode) return;
            style->setFontVariant(parentStyle->fontVariant());
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
        FontDef fontDef = style->htmlFont().fontDef;
        if(value->cssValueType() == CSSValue::CSS_INHERIT) {
            if(!parentNode) return;
            fontDef.weight = parentStyle->htmlFont().fontDef.weight;
        } else {
	    if(!primitiveValue) return;
	    if(primitiveValue->getIdent())
	    {
		switch(primitiveValue->getIdent())
		{
		    // ### we just support normal and bold fonts at the moment...
		    // setWeight can actually accept values between 0 and 99...
		    case CSS_VAL_BOLD:
		    case CSS_VAL_BOLDER:
			fontDef.weight = QFont::Bold;
			break;
		    case CSS_VAL_NORMAL:
		    case CSS_VAL_LIGHTER:
			fontDef.weight = QFont::Normal;
			break;
		    default:
			return;
		}
	    }
	    else
	    {
		// ### fix parsing of 100-900 values in parser, apply them here
	    }
	}
        if (style->setFontDef( fontDef ))
	fontDirty = true;
        break;
    }

    case CSS_PROP_LIST_STYLE_POSITION:
    {
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            style->setListStylePosition(parentStyle->listStylePosition());
            return;
        }
        if(!primitiveValue) return;
        if(primitiveValue->getIdent())
            style->setListStylePosition( (EListStylePosition) (primitiveValue->getIdent() - CSS_VAL_OUTSIDE) );
        return;
    }

    case CSS_PROP_LIST_STYLE_TYPE:
    {
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            style->setListStyleType(parentStyle->listStyleType());
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            style->setOverflow(parentStyle->overflow());
            return;
        }
        if(!primitiveValue) return;
        EOverflow o;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_VISIBLE:
            o = OVISIBLE; break;
        case CSS_VAL_HIDDEN:
            o = OHIDDEN; break;
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            style->setPosition(parentStyle->position());
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
                view->useSlowRepaints();
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
	break;
    case CSS_PROP_UNICODE_BIDI: {
	EUnicodeBidi b = UBNormal;
        if(value->cssValueType() == CSSValue::CSS_INHERIT) {
            if(!parentNode) return;
            b = parentStyle->unicodeBidi();
        } else {
	    switch( primitiveValue->getIdent() ) {
		case CSS_VAL_NORMAL:
		    b = UBNormal; break;
		case CSS_VAL_EMBED:
		    b = Embed; break;
		case CSS_VAL_BIDI_OVERRIDE:
		    b = Override; break;
		default:
		    return;
	    }
	}
	style->setUnicodeBidi( b );
        break;
    }
    case CSS_PROP_TEXT_TRANSFORM:
        {
        if(value->cssValueType() == CSSValue::CSS_INHERIT) {
            if(!parentNode) return;
            style->setTextTransform(parentStyle->textTransform());
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT) {
            if(!parentNode) return;
            style->setVisibility(parentStyle->visibility());
            return;
        }

        switch( primitiveValue->getIdent() ) {
        case CSS_VAL_HIDDEN:
            style->setVisibility( HIDDEN );
            break;
        case CSS_VAL_VISIBLE:
            style->setVisibility( VISIBLE );
            break;
        case CSS_VAL_COLLAPSE:
            style->setVisibility( COLLAPSE );
        default:
            break;
        }
        break;
    }
    case CSS_PROP_WHITE_SPACE:
        if(value->cssValueType() == CSSValue::CSS_INHERIT) {
            if(!parentNode) return;
            style->setWhiteSpace(parentStyle->whiteSpace());
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
	l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), Fixed);
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
	l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), Fixed);
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
        spacing =  primitiveValue->computeLength(style, paintDeviceMetrics);
        style->setBorderSpacing(spacing);
        break;
        }
        // CSS2BorderSpacing
    case CSS_PROP_CURSOR:
        // CSS2Cursor
        if(value->cssValueType() == CSSValue::CSS_INHERIT) {
            if(!parentNode) return;
            style->setCursor(parentStyle->cursor());
            style->setCursorImage(parentStyle->cursorImage());
            return;
        } else if(primitiveValue) {
	    if(primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_URI) {
            	CSSImageValueImpl *image = static_cast<CSSImageValueImpl *>(primitiveValue);
            	//kdDebug( 6080 ) << "setting cursor image to " << image->cssText().string() << endl;
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            switch(prop->m_id)
            {
            case CSS_PROP_BACKGROUND_COLOR:
                col = parentStyle->backgroundColor(); break;
            case CSS_PROP_BORDER_TOP_COLOR:
                col = parentStyle->borderTopColor(); break;
            case CSS_PROP_BORDER_RIGHT_COLOR:
                col = parentStyle->borderRightColor(); break;
            case CSS_PROP_BORDER_BOTTOM_COLOR:
                col = parentStyle->borderBottomColor(); break;
            case CSS_PROP_BORDER_LEFT_COLOR:
                col = parentStyle->borderLeftColor(); break;
            case CSS_PROP_COLOR:
                col = parentStyle->color(); break;
            case CSS_PROP_OUTLINE_COLOR:
		col = parentStyle->outlineColor(); break;
            default:
            return;
        }
        } else {
        if(!primitiveValue) return;
        if(primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_RGBCOLOR)
            col = primitiveValue->getRGBColorValue()->color();
        else
            return;
	}
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
	khtml::CachedImage *image = 0;
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            image = parentStyle->backgroundImage();
        } else {
        if(!primitiveValue) return;
	    image = static_cast<CSSImageValueImpl *>(primitiveValue)->image();
	}
        style->setBackgroundImage(image);
        //kdDebug( 6080 ) << "setting image in style to " << image->image() << endl;
        break;
    }
//     case CSS_PROP_CUE_AFTER:
//     case CSS_PROP_CUE_BEFORE:
//         break;
    case CSS_PROP_LIST_STYLE_IMAGE:
    {
	khtml::CachedImage *image = 0;
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            image = parentStyle->listStyleImage();
        } else {
        if(!primitiveValue) return;
	    image = static_cast<CSSImageValueImpl *>(primitiveValue)->image();
	}
        style->setListStyleImage(image);
        //kdDebug( 6080 ) << "setting image in list to " << image->image() << endl;
        break;
    }

// length
    case CSS_PROP_BORDER_TOP_WIDTH:
    case CSS_PROP_BORDER_RIGHT_WIDTH:
    case CSS_PROP_BORDER_BOTTOM_WIDTH:
    case CSS_PROP_BORDER_LEFT_WIDTH:
    case CSS_PROP_OUTLINE_WIDTH:
    {
	short width = 3;
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            switch(prop->m_id)
            {
            case CSS_PROP_BORDER_TOP_WIDTH:
		    width = parentStyle->borderTopWidth(); break;
            case CSS_PROP_BORDER_RIGHT_WIDTH:
		    width = parentStyle->borderRightWidth(); break;
            case CSS_PROP_BORDER_BOTTOM_WIDTH:
		    width = parentStyle->borderBottomWidth(); break;
            case CSS_PROP_BORDER_LEFT_WIDTH:
		    width = parentStyle->borderLeftWidth(); break;
            case CSS_PROP_OUTLINE_WIDTH:
		    width = parentStyle->outlineWidth(); break;
            default:
            return;
        }
            return;
        } else {
        if(!primitiveValue) break;
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
            width = primitiveValue->computeLength(style, paintDeviceMetrics);
            break;
        default:
            return;
        }
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
	int width = 0;
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            switch(prop->m_id)
            {
            case CSS_PROP_MARKER_OFFSET:
                // ###
                return;
            case CSS_PROP_LETTER_SPACING:
                width = parentStyle->letterSpacing(); break;
            case CSS_PROP_WORD_SPACING:
                width = parentStyle->wordSpacing(); break;
            default:
                return;
            }
        } else {
	    if(!primitiveValue) return;
	    width = primitiveValue->computeLength(style, paintDeviceMetrics);
	}
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
        default: break;
        }
        return;
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
            l = Length ( 0, Static );
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT) {
            if(!parentNode) return;
	    apply = true;
            switch(prop->m_id)
                {
                case CSS_PROP_MAX_WIDTH:
                    l = parentStyle->maxWidth(); break;
                case CSS_PROP_BOTTOM:
                    l = parentStyle->bottom(); break;
                case CSS_PROP_TOP:
                    l = parentStyle->top(); break;
                case CSS_PROP_LEFT:
                    l = parentStyle->left(); break;
                case CSS_PROP_RIGHT:
                    l = parentStyle->right(); break;
                case CSS_PROP_WIDTH:
                    l = parentStyle->width(); break;
                case CSS_PROP_MIN_WIDTH:
                    l = parentStyle->minWidth(); break;
                case CSS_PROP_PADDING_TOP:
                    l = parentStyle->paddingTop(); break;
                case CSS_PROP_PADDING_RIGHT:
                    l = parentStyle->paddingRight(); break;
                case CSS_PROP_PADDING_BOTTOM:
                    l = parentStyle->paddingBottom(); break;
                case CSS_PROP_PADDING_LEFT:
                    l = parentStyle->paddingLeft(); break;
                case CSS_PROP_MARGIN_TOP:
                    l = parentStyle->marginTop(); break;
                case CSS_PROP_MARGIN_RIGHT:
                    l = parentStyle->marginRight(); break;
                case CSS_PROP_MARGIN_BOTTOM:
                    l = parentStyle->marginBottom(); break;
                case CSS_PROP_MARGIN_LEFT:
                    l = parentStyle->marginLeft(); break;
                case CSS_PROP_TEXT_INDENT:
                    l = parentStyle->textIndent(); break;
                default:
                    return;
                }
        } else if(primitiveValue && !apply) {
            int type = primitiveValue->primitiveType();
            if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), Fixed);
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
            default: break;
            }
        return;
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
	    apply = true;
            switch(prop->m_id)
                {
                case CSS_PROP_MAX_HEIGHT:
                    l = parentStyle->maxHeight(); break;
                case CSS_PROP_HEIGHT:
                    l = parentStyle->height(); break;
                case CSS_PROP_MIN_HEIGHT:
                    l = parentStyle->minHeight(); break;
                default:
                    return;
                }
            return;
        }
        if(primitiveValue && !apply)
        {
            int type = primitiveValue->primitiveType();
            if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), Fixed);
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            style->setVerticalAlign(parentStyle->verticalAlign());
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
	    l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), Fixed );
	  else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
	    l = Length( int( primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE) ), Percent );

	  style->setVerticalAlign( LENGTH );
	  style->setVerticalAlignLength( l );
	}
        break;

    case CSS_PROP_FONT_SIZE:
    {
        FontDef fontDef = style->htmlFont().fontDef;
        int oldSize;
        float size = 0;
        int minFontSize = settings->minFontSize();

	// Set this boolean flag to indicate that the font size was specified
	// during the course of rule application for this element. -dwh
	m_fontSizeSpecified = true;

        if(parentNode) {
            oldSize = parentStyle->font().pixelSize();
        } else
            oldSize = m_fontSizes[3];

        if(value->cssValueType() == CSSValue::CSS_INHERIT) {
            size = oldSize;
        } else if(primitiveValue->getIdent()) {
	    // keywords are being used.  Pick the correct default
	    // based off the font family.
	    QValueList<int>& fontSizes = (fontDef.genericFamily == FontDef::eMonospace) ?
					 m_fixedFontSizes : m_fontSizes;
	   
            switch(primitiveValue->getIdent())
            {
            case CSS_VAL_XX_SMALL: size = fontSizes[0]; break;
            case CSS_VAL_X_SMALL:  size = fontSizes[1]; break;
            case CSS_VAL_SMALL:    size = fontSizes[2]; break;
            case CSS_VAL_MEDIUM:   size = fontSizes[3]; break;
            case CSS_VAL_LARGE:    size = fontSizes[4]; break;
            case CSS_VAL_X_LARGE:  size = fontSizes[5]; break;
            case CSS_VAL_XX_LARGE: size = fontSizes[6]; break;
            case CSS_VAL__KONQ_XXX_LARGE:  size = ( fontSizes[6]*5 )/3; break;
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

        } else {
            int type = primitiveValue->primitiveType();
            if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                size = primitiveValue->computeLengthFloat(parentStyle, paintDeviceMetrics);
            else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
                size = (primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE)
                        * parentStyle->font().pixelSize()) / 100;
            else
                return;

            if (!khtml::printpainter && element && element->getDocument()->view())
                size *= element->getDocument()->view()->part()->zoomFactor() / 100.0;
        }

        if(size <= 0) return;

        // we never want to get smaller than the minimum font size to keep fonts readable
        if(size < minFontSize ) size = minFontSize;

        //kdDebug( 6080 ) << "computed raw font size: " << size << endl;

	fontDef.size = int(size);
        if (style->setFontDef( fontDef ))
	    fontDirty = true;
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
        int z_index = 0;
        if(value->cssValueType() == CSSValue::CSS_INHERIT) {
            if(!parentNode) return;
            z_index = parentStyle->zIndex();
        } else {
            if (!primitiveValue)
                return;

            if (primitiveValue->getIdent() == CSS_VAL_AUTO) {
                style->setHasAutoZIndex();
                return;
            }
            
            if (primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
                return; // Error case.
            
            z_index = (int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER);
        }
        
        style->setZIndex(z_index);
        return;
    }

// length, percent, number
    case CSS_PROP_LINE_HEIGHT:
    {
        Length lineHeight;
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            lineHeight = parentStyle->lineHeight();
        } else {
            if(!primitiveValue) return;
            int type = primitiveValue->primitiveType();
            if(primitiveValue->getIdent() == CSS_VAL_NORMAL)
                lineHeight = Length( -100, Percent );
            else if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                lineHeight = Length(primitiveValue->computeLength(style, paintDeviceMetrics), Fixed);
            else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
                lineHeight = Length( ( style->font().pixelSize() * int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE)) ) / 100, Fixed );
            else if(type == CSSPrimitiveValue::CSS_NUMBER)
                lineHeight = Length(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER)*100), Percent);
            else
                return;
	}
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            style->setTextAlign(parentStyle->textAlign());
            return;
        }
        if(!primitiveValue) return;
        if(primitiveValue->getIdent())
            style->setTextAlign( (ETextAlign) (primitiveValue->getIdent() - CSS_VAL__KONQ_AUTO) );
	return;
    }

// rect
    case CSS_PROP__KONQ_JS_CLIP:
    case CSS_PROP_CLIP:
    {
	Length top;
	Length right;
	Length bottom;
	Length left;
	if ( value->cssValueType() == CSSValue::CSS_INHERIT ) {
	    top = parentStyle->clipTop();
	    right = parentStyle->clipRight();
	    bottom = parentStyle->clipBottom();
	    left = parentStyle->clipLeft();
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
// 	qDebug("setting clip top to %d", top.value );
// 	qDebug("setting clip right to %d", right.value );
// 	qDebug("setting clip bottom to %d", bottom.value );
// 	qDebug("setting clip left to %d", left.value );
	style->setClip( top, right, bottom, left );

	style->setJsClipMode( (!strictParsing && prop->m_id == CSS_PROP__KONQ_JS_CLIP) ? true : false );
        // rect, ident
        break;
    }

// lists
    case CSS_PROP_CONTENT:
        // list of string, uri, counter, attr, i
    {
        if (!(style->styleType()==RenderStyle::BEFORE ||
                style->styleType()==RenderStyle::AFTER))
            break;

        if(!value->isValueList()) return;
        CSSValueListImpl *list = static_cast<CSSValueListImpl *>(value);
        int len = list->length();

        for(int i = 0; i < len; i++) {
            CSSValueImpl *item = list->item(i);
            if(!item->isPrimitiveValue()) continue;
            CSSPrimitiveValueImpl *val = static_cast<CSSPrimitiveValueImpl *>(item);
            if(val->primitiveType()==CSSPrimitiveValue::CSS_STRING)
            {
                style->setContent(val->getStringValue());
            }
            else if (val->primitiveType()==CSSPrimitiveValue::CSS_URI)
            {
                CSSImageValueImpl *image = static_cast<CSSImageValueImpl *>(val);
                style->setContent(image->image());
            }

        }
        break;
    }

    case CSS_PROP_COUNTER_INCREMENT:
        // list of CSS2CounterIncrement
    case CSS_PROP_COUNTER_RESET:
        // list of CSS2CounterReset
        break;
    case CSS_PROP_FONT_FAMILY:
        // list of strings and ids
    {
        if(!value->isValueList()) return;
	FontDef fontDef = style->htmlFont().fontDef;
        CSSValueListImpl *list = static_cast<CSSValueListImpl *>(value);
        int len = list->length();
	QString family;
        for(int i = 0; i < len; i++) {
            CSSValueImpl *item = list->item(i);
            if(!item->isPrimitiveValue()) continue;
            CSSPrimitiveValueImpl *val = static_cast<CSSPrimitiveValueImpl *>(item);
            if(!val->primitiveType() == CSSPrimitiveValue::CSS_STRING) return;
            QString face = static_cast<FontFamilyValueImpl *>(val)->fontName();
	    if ( !face.isEmpty() ) {
	        if(face == "serif") {
		    face = settings->serifFontName();
		    fontDef.setGenericFamily(FontDef::eSerif);
		}
		else if(face == "sans-serif") {
		    face = settings->sansSerifFontName();
		    fontDef.setGenericFamily(FontDef::eSansSerif);
		}
		else if( face == "cursive") {
		    face = settings->cursiveFontName();
		    fontDef.setGenericFamily(FontDef::eCursive);
		}
		else if( face == "fantasy") {
		    face = settings->fantasyFontName();
		    fontDef.setGenericFamily(FontDef::eFantasy);
		}
		else if( face == "monospace") {
		    face = settings->fixedFontName();
		    fontDef.setGenericFamily(FontDef::eMonospace);
		}
		else if( face == "konq_default") {
		    // Treat this as though it's a generic family, since we will want
		    // to reset to default sizes when we encounter this (and inherit
		    // from an enclosing different family like monospace.
		    face = settings->stdFontName();
		    fontDef.setGenericFamily(FontDef::eStandard);
		}

		if ( !face.isEmpty() ) {
		    fontDef.family = face;
		    if (style->setFontDef( fontDef )) {
		      fontDirty = true;
		    }
		}
                return;
	    }
        }
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            style->setTextDecoration(parentStyle->textDecoration());
            style->setTextDecorationColor(parentStyle->textDecorationColor());
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
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
            style->setFlowAroundFloats(parentStyle->flowAroundFloats());
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
        if(value->cssValueType() != CSSValue::CSS_INHERIT || !parentNode) return;
        style->setBackgroundColor(parentStyle->backgroundColor());
        style->setBackgroundImage(parentStyle->backgroundImage());
        style->setBackgroundRepeat(parentStyle->backgroundRepeat());
        style->setBackgroundAttachment(parentStyle->backgroundAttachment());
//      style->setBackgroundPosition(parentStyle->backgroundPosition());

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
        if(value->cssValueType() != CSSValue::CSS_INHERIT || !parentNode) return;

        if(prop->m_id == CSS_PROP_BORDER || prop->m_id == CSS_PROP_BORDER_COLOR)
        {
            style->setBorderTopColor(parentStyle->borderTopColor());
            style->setBorderBottomColor(parentStyle->borderBottomColor());
            style->setBorderLeftColor(parentStyle->borderLeftColor());
            style->setBorderRightColor(parentStyle->borderRightColor());
        }
        if(prop->m_id == CSS_PROP_BORDER || prop->m_id == CSS_PROP_BORDER_STYLE)
        {
            style->setBorderTopStyle(parentStyle->borderTopStyle());
            style->setBorderBottomStyle(parentStyle->borderBottomStyle());
            style->setBorderLeftStyle(parentStyle->borderLeftStyle());
            style->setBorderRightStyle(parentStyle->borderRightStyle());
        }
        if(prop->m_id == CSS_PROP_BORDER || prop->m_id == CSS_PROP_BORDER_WIDTH)
        {
            style->setBorderTopWidth(parentStyle->borderTopWidth());
            style->setBorderBottomWidth(parentStyle->borderBottomWidth());
            style->setBorderLeftWidth(parentStyle->borderLeftWidth());
            style->setBorderRightWidth(parentStyle->borderRightWidth());
        }
        return;
    case CSS_PROP_BORDER_TOP:
        if(value->cssValueType() != CSSValue::CSS_INHERIT || !parentNode) return;
        style->setBorderTopColor(parentStyle->borderTopColor());
        style->setBorderTopStyle(parentStyle->borderTopStyle());
        style->setBorderTopWidth(parentStyle->borderTopWidth());
        return;
    case CSS_PROP_BORDER_RIGHT:
        if(value->cssValueType() != CSSValue::CSS_INHERIT || !parentNode) return;
        style->setBorderRightColor(parentStyle->borderRightColor());
        style->setBorderRightStyle(parentStyle->borderRightStyle());
        style->setBorderRightWidth(parentStyle->borderRightWidth());
        return;
    case CSS_PROP_BORDER_BOTTOM:
        if(value->cssValueType() != CSSValue::CSS_INHERIT || !parentNode) return;
        style->setBorderBottomColor(parentStyle->borderBottomColor());
        style->setBorderBottomStyle(parentStyle->borderBottomStyle());
        style->setBorderBottomWidth(parentStyle->borderBottomWidth());
        return;
    case CSS_PROP_BORDER_LEFT:
        if(value->cssValueType() != CSSValue::CSS_INHERIT || !parentNode) return;
        style->setBorderLeftColor(parentStyle->borderLeftColor());
        style->setBorderLeftStyle(parentStyle->borderLeftStyle());
        style->setBorderLeftWidth(parentStyle->borderLeftWidth());
        return;
    case CSS_PROP_MARGIN:
        if(value->cssValueType() != CSSValue::CSS_INHERIT || !parentNode) return;
        style->setMarginTop(parentStyle->marginTop());
        style->setMarginBottom(parentStyle->marginBottom());
        style->setMarginLeft(parentStyle->marginLeft());
        style->setMarginRight(parentStyle->marginRight());
        return;
    case CSS_PROP_PADDING:
        if(value->cssValueType() != CSSValue::CSS_INHERIT || !parentNode) return;
        style->setPaddingTop(parentStyle->paddingTop());
        style->setPaddingBottom(parentStyle->paddingBottom());
        style->setPaddingLeft(parentStyle->paddingLeft());
        style->setPaddingRight(parentStyle->paddingRight());
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


void CSSStyleSelector::checkForGenericFamilyChange(RenderStyle* aStyle, RenderStyle* aParentStyle)
{
  if (m_fontSizeSpecified || !aParentStyle) {
    m_fontSizeSpecified = false;
    return;
  }

  const FontDef& childFont = aStyle->htmlFont().fontDef;
  const FontDef& parentFont = aParentStyle->htmlFont().fontDef;

  if (childFont.genericFamily == parentFont.genericFamily)
    return;

  // For now, lump all families but monospace together.
  if (childFont.genericFamily != FontDef::eMonospace &&
      parentFont.genericFamily != FontDef::eMonospace)
    return;

  // We know the parent is monospace or the child is monospace, and that font
  // size was unspecified.  We want to alter our font size to use the correct
  // "medium" font for our family.
  float size = 0;
  int minFontSize = settings->minFontSize();
  size = (childFont.genericFamily == FontDef::eMonospace) ? m_fixedFontSizes[3] : m_fontSizes[3];
  int isize = (int)size;
  if (isize < minFontSize)
    isize = minFontSize;
  
  FontDef newFontDef(childFont);
  newFontDef.size = isize;
  aStyle->setFontDef(newFontDef);
}

} // namespace khtml
