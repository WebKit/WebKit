/**
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
#include "html/html_elementimpl.h"
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
CSSStyleSelectorList *CSSStyleSelector::defaultQuirksStyle = 0;
CSSStyleSelectorList *CSSStyleSelector::defaultPrintStyle = 0;
CSSStyleSheetImpl *CSSStyleSelector::defaultSheet = 0;
RenderStyle* CSSStyleSelector::styleNotYetAvailable = 0;
CSSStyleSheetImpl *CSSStyleSelector::quirksSheet = 0;

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
            authorStyle->append( static_cast<CSSStyleSheetImpl*>( it.current() ), m_medium );
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

    {
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

        // Collect only strict-mode rules.
        defaultStyle = new CSSStyleSelectorList();
        defaultStyle->append( defaultSheet, "screen" );

        defaultPrintStyle = new CSSStyleSelectorList();
        defaultPrintStyle->append( defaultSheet, "print" );
    }
    {
        QFile f(locate( "data", "khtml/css/quirks.css" ) );
        f.open(IO_ReadOnly);

        QCString file( f.size()+1 );
        int readbytes = f.readBlock( file.data(), f.size() );
        f.close();
        if ( readbytes >= 0 )
            file[readbytes] = '\0';

        QString style = QString::fromLatin1( file.data() );
        DOMString str(style);

        quirksSheet = new DOM::CSSStyleSheetImpl((DOM::CSSStyleSheetImpl * ) 0);
        quirksSheet->parseString( str );

        // Collect only quirks-mode rules.
        defaultQuirksStyle = new CSSStyleSelectorList();
        defaultQuirksStyle->append( quirksSheet, "screen" );
    }

    //kdDebug() << "CSSStyleSelector: default style has " << defaultStyle->count() << " elements"<< endl;
}

void CSSStyleSelector::clear()
{
    delete defaultStyle;
    delete defaultQuirksStyle;
    delete defaultPrintStyle;
    delete defaultSheet;
    delete styleNotYetAvailable;
    defaultStyle = 0;
    defaultQuirksStyle = 0;
    defaultPrintStyle = 0;
    defaultSheet = 0;
    styleNotYetAvailable = 0;
}

#define MAXFONTSIZES 15

void CSSStyleSelector::computeFontSizes(QPaintDeviceMetrics* paintDeviceMetrics,  int zoomFactor)
{
    computeFontSizesFor(paintDeviceMetrics, zoomFactor, m_fontSizes, false);
    computeFontSizesFor(paintDeviceMetrics, zoomFactor, m_fixedFontSizes, true);
}

void CSSStyleSelector::computeFontSizesFor(QPaintDeviceMetrics* paintDeviceMetrics, int zoomFactor, QValueList<int>& fontSizes, bool isFixed)
{
#if APPLE_CHANGES
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
        CSSOrderedProperty **swappedPos = 0;
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

RenderStyle *CSSStyleSelector::styleForElement(ElementImpl *e)
{
    if (!e->getDocument()->haveStylesheetsLoaded()) {
        if (!styleNotYetAvailable) {
            styleNotYetAvailable = new RenderStyle();
            styleNotYetAvailable->setDisplay(NONE);
            styleNotYetAvailable->ref();
        }
        return styleNotYetAvailable;
    }
  
    // set some variables we will need
    ::encodedurl = &encodedurl;
    pseudoState = PseudoUnknown;

    element = e;
    parentNode = e->parentNode();
    parentStyle = ( parentNode && parentNode->renderer()) ? parentNode->renderer()->style() : 0;
    view = element->getDocument()->view();
    isXMLDoc = !element->getDocument()->isHTMLDocument();
    part = view->part();
    settings = part->settings();
    paintDeviceMetrics = element->getDocument()->paintDeviceMetrics();

    style = new RenderStyle();
    if( parentStyle )
        style->inheritFrom( parentStyle );
    else
        parentStyle = style;
    
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
    numPropsToApply = addInlineDeclarations( e, e->m_styleDecls, numPropsToApply );
            
    bubbleSort( propsToApply, propsToApply+numPropsToApply-1 );
    bubbleSort( pseudoProps, pseudoProps+numPseudoProps-1 );

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
                DOM::CSSProperty *prop = propsToApply[i]->prop;
                applyRule( prop->m_id, prop->value() );
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

                RenderStyle* oldStyle = style;
		style = pseudoStyle;
                if ( pseudoStyle ) {
                    DOM::CSSProperty *prop = pseudoProps[i]->prop;
                    applyRule( prop->m_id, prop->value() );
                }
                style = oldStyle;
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

    // Mutate the display to BLOCK or TABLE for certain cases, e.g., if someone attempts to
    // position or float an inline, compact, or run-in.  Cache the original display, since it
    // may be needed for positioned elements that have to compute their static normal flow
    // positions.
    style->setOriginalDisplay(style->display());
    if (style->display() != NONE && style->display() != BLOCK && style->display() != TABLE &&
        (style->position() == ABSOLUTE || style->position() == FIXED || style->floating() != FNONE)) {
        if (style->display() == INLINE_TABLE)
            style->setDisplay(TABLE);
        else if (style->display() == LIST_ITEM) {
            // It is a WinIE bug that floated list items lose their bullets, so we'll emulate the quirk,
            // but only in quirks mode.
            if (!strictParsing && style->floating() != FNONE)
                style->setDisplay(BLOCK);
        }
        else
            style->setDisplay(BLOCK);
    }

    // Finally update our text decorations in effect, but don't allow text-decoration to percolate through
    // tables, inline blocks, inline tables, or run-ins.
    if (style->display() == TABLE || style->display() == INLINE_TABLE || style->display() == RUN_IN)
        // || style->display() == INLINE_BLOCK) FIXME!
        style->setTextDecorationsInEffect(style->textDecoration());
    else
        style->addToTextDecorationsInEffect(style->textDecoration());

    // Now return the style.
    return style;
}

unsigned int CSSStyleSelector::addInlineDeclarations(DOM::ElementImpl* e,
                                                     DOM::CSSStyleDeclarationImpl *decl,
                                                     unsigned int numProps)
{
    CSSStyleDeclarationImpl* addDecls = 0;
    if (e->id() == ID_TD || e->id() == ID_TH)     // For now only TableCellElement implements the
        addDecls = e->getAdditionalStyleDecls();  // virtual function for shared cell rules.

    if (!decl && !addDecls)
        return numProps;

    QPtrList<CSSProperty>* values = decl ? decl->values() : 0;
    QPtrList<CSSProperty>* addValues = addDecls ? addDecls->values() : 0;
    if (!values && !addValues)
        return numProps;
    
    int firstLen = values ? values->count() : 0;
    int secondLen = addValues ? addValues->count() : 0;
    int totalLen = firstLen + secondLen;

    if (inlineProps.size() < (uint)totalLen)
        inlineProps.resize(totalLen + 1);
    
    if (numProps + totalLen >= propsToApplySize ) {
        propsToApplySize += propsToApplySize;
        propsToApply = (CSSOrderedProperty **)realloc( propsToApply, propsToApplySize*sizeof( CSSOrderedProperty * ) );
    }

    CSSOrderedProperty *array = (CSSOrderedProperty *)inlineProps.data();
    for(int i = 0; i < totalLen; i++)
    {
        if (i == firstLen)
            values = addValues;

        CSSProperty *prop = values->at(i >= firstLen ? i - firstLen : i);
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

    // Don't remove "//" from an anchor identifier. -rjw
    // Set refPos to -2 to mean "I haven't looked for the anchor yet".
    // We don't want to waste a function call on the search for the the anchor
    // in the vast majority of cases where there is no "//" in the path.
    int refPos = -2;
    while ( (pos = path.find( "//", pos )) != -1) {
        if (refPos == -2)
            refPos = path.find("#", 0);
        if (refPos > 0 && pos >= refPos)
            break;

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
    if( e->id() != ID_A ) {
        pseudoState = PseudoNone;
        return;
    }
    DOMString attr = e->getAttribute(ATTR_HREF);
    if( attr.isNull() ) {
        pseudoState = PseudoNone;
        return;
    }
    QConstString cu(attr.unicode(), attr.length());
    QString u = cu.string();
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

void CSSStyleSelector::checkSelector(int selIndex, DOM::ElementImpl *e)
{
    dynamicPseudo = RenderStyle::NOPSEUDO;
    
    NodeImpl *n = e;

    selectorCache[ selIndex ].state = Invalid;
    CSSSelector *sel = selectors[ selIndex ];

    // we have the subject part of the selector
    subject = true;

    // We track whether or not the rule contains only :hover and :active in a simple selector. If
    // so, we can't allow that to apply to every element on the page.  We assume the author intended
    // to apply the rules only to links.
    bool onlyHoverActive = (sel->tag == -1 &&
                            (sel->match == CSSSelector::Pseudo &&
                              (sel->pseudoType() == CSSSelector::PseudoHover ||
                               sel->pseudoType() == CSSSelector::PseudoActive)));
    bool affectedByHover = style->affectedByHoverRules();
    bool affectedByActive = style->affectedByActiveRules();
    
    // first selector has to match
    if(!checkOneSelector(sel, e)) return;

    // check the subselectors
    CSSSelector::Relation relation = sel->relation;
    while((sel = sel->tagHistory))
    {
        if (!n->isElementNode()) return;
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
            if (!strictParsing)
                while (n && n->implicitNode()) n = n->parentNode();
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
            if (onlyHoverActive)
                onlyHoverActive = (sel->match == CSSSelector::Pseudo &&
                                   (sel->pseudoType() == CSSSelector::PseudoHover ||
                                    sel->pseudoType() == CSSSelector::PseudoActive));
            
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

    // disallow *:hover, *:active, and *:hover:active except for links
    if (onlyHoverActive && subject) {
        if (pseudoState == PseudoUnknown)
            checkPseudoState( e );

        if (pseudoState == PseudoNone) {
            if (!affectedByHover && style->affectedByHoverRules())
                style->setAffectedByHoverRules(false);
            if (!affectedByActive && style->affectedByActiveRules())
                style->setAffectedByActiveRules(false);
            return;
        }
    }

    if ( dynamicPseudo != RenderStyle::NOPSEUDO ) {
	selectorCache[selIndex].state = AppliesPseudo;
	selectors[ selIndex ]->pseudoId = dynamicPseudo;
    } else
	selectorCache[ selIndex ].state = Applies;
    //qDebug( "selector %d applies", selIndex );
    //selectors[ selIndex ]->print();
    return;
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
        case CSSSelector::Id:
	    if( (isXMLDoc && strcmp(sel->value, value) ) ||
                (!isXMLDoc && strcasecmp(sel->value, value)))
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
                if( (isXMLDoc && strcmp(sel->value, value) ) ||
                     (!isXMLDoc && strcasecmp(sel->value, value)))
                    return false;
                break;
            }

            // The selector's value can't contain a space, or it's totally bogus.
            spacePos = sel->value.find(' ');
            if (spacePos != -1)
                return false;
            
            QString str = value.string();
            QString selStr = sel->value.string();
            int pos = str.find(selStr, 0, isXMLDoc);
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
            int pos = str.find(selStr, 0, isXMLDoc);
            if(pos == -1) return false;
            break;
        }
        case CSSSelector::Begin:
        {
            //kdDebug( 6080 ) << "checking for beginswith match" << endl;
            QString str = value.string();
            QString selStr = sel->value.string();
            int pos = str.find(selStr, 0, isXMLDoc);
            if(pos != 0) return false;
            break;
        }
        case CSSSelector::End:
        {
            //kdDebug( 6080 ) << "checking for endswith match" << endl;
            QString str = value.string();
            QString selStr = sel->value.string();
	    if (isXMLDoc && !str.endsWith(selStr)) return false;
	    if (!isXMLDoc) {
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
            if(str.find(selStr, 0, isXMLDoc) != 0) return false;
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
//	kdDebug() << "CSSOrderedRule::pseudo " << value << endl;
	switch (sel->pseudoType()) {
            case CSSSelector::PseudoEmpty:
                if (!e->firstChild())
                    return true;
                break;
            case CSSSelector::PseudoFirstChild: {
                // first-child matches the first child that is an element!
                if (e->parentNode()) {
                    DOM::NodeImpl* n = e->previousSibling();
                    while ( n && !n->isElementNode() )
                        n = n->previousSibling();
                    if ( !n )
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoLastChild: {
                // last-child matches the last child that is an element!
                if (e->parentNode()) {
                    DOM::NodeImpl* n = e->nextSibling();
                    while ( n && !n->isElementNode() )
                        n = n->nextSibling();
                    if ( !n )
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoOnlyChild: {
                // If both first-child and last-child apply, then only-child applies.
                if (e->parentNode()) {
                    DOM::NodeImpl* n = e->previousSibling();
                    while ( n && !n->isElementNode() )
                        n = n->previousSibling();
                    if ( !n ) {
                        n = e->nextSibling();
                        while ( n && !n->isElementNode() )
                            n = n->nextSibling();
                        if ( !n )
                            return true;
                    }
                }
                break;
            }
            case CSSSelector::PseudoFirstLine:
                if ( subject ) {
                    dynamicPseudo=RenderStyle::FIRST_LINE;
                    return true;
                }
                break;
            case CSSSelector::PseudoFirstLetter:
                if ( subject ) {
                    dynamicPseudo=RenderStyle::FIRST_LETTER;
                    return true;
                }
                break;
            case CSSSelector::PseudoTarget:
                if (e == e->getDocument()->getCSSTarget())
                    return true;
                break;
            case CSSSelector::PseudoLink:
                if ( pseudoState == PseudoUnknown )
                    checkPseudoState( e );
                if ( pseudoState == PseudoLink )
                    return true;
                break;
            case CSSSelector::PseudoVisited:
                if ( pseudoState == PseudoUnknown )
                    checkPseudoState( e );
                if ( pseudoState == PseudoVisited )
                    return true;
                break;
            case CSSSelector::PseudoHover: {
                // If we're in quirks mode, then hover should never match anchors with no
                // href.  This is important for sites like wsj.com.
                if (strictParsing || e->id() != ID_A || e->hasAnchor()) {
                    if (element == e)
                        style->setAffectedByHoverRules(true);
                    if (e->renderer()) {
                        if (element != e)
                            e->renderer()->style()->setAffectedByHoverRules(true);
                        if (e->renderer()->mouseInside())
                            return true;
                    }
                }
                break;
            }
            case CSSSelector::PseudoFocus:
                if (e && e->focused()) {
                    return true;
                }
                break;
            case CSSSelector::PseudoActive:
                // If we're in quirks mode, then :active should never match anchors with no
                // href. 
                if (strictParsing || e->id() != ID_A || e->hasAnchor()) {
                    if (element == e)
                        style->setAffectedByActiveRules(true);
                    else if (e->renderer())
                        e->renderer()->style()->setAffectedByActiveRules(true);
                    if (e->active())
                        return true;
                }
                break;
            case CSSSelector::PseudoRoot:
                if (e == e->getDocument()->documentElement())
                    return true;
                break;
            case CSSSelector::PseudoNot: {
                // check the simple selector
                for (CSSSelector* subSel = sel->simpleSelector; subSel;
                     subSel = subSel->tagHistory) {
                    // :not cannot nest.  I don't really know why this is a restriction in CSS3,
                    // but it is, so let's honor it.
                    if (subSel->simpleSelector)
                        break;
                    if (!checkOneSelector(subSel, e))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoSelection:
                dynamicPseudo = RenderStyle::SELECTION;
                return true;
            case CSSSelector::PseudoBefore:
                dynamicPseudo = RenderStyle::BEFORE;
                return true;
            case CSSSelector::PseudoAfter:
                dynamicPseudo = RenderStyle::AFTER;
                return true;
                
            case CSSSelector::PseudoNotParsed:
                assert(false);
                break;
            case CSSSelector::PseudoLang:
                /* not supported for now */
            case CSSSelector::PseudoOther:
                break;
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
      
    if (!strictParsing && defaultQuirksStyle)
        defaultQuirksStyle->collect( &selectorList, &propertyList, Default, Default );
        
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
        else if (type == CSSPrimitiveValue::CSS_HTML_RELATIVE)
            l = Length(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_HTML_RELATIVE)), Relative);
	else if ( ok )
	    *ok = false;
    }
    return l;
}


// color mapping code
struct colorMap {
    int css_value;
    QRgb color;
};

static const colorMap cmap[] = {
    { CSS_VAL_AQUA, 0xFF00FFFF },
    { CSS_VAL_BLACK, 0xFF000000 },
    { CSS_VAL_BLUE, 0xFF0000FF },
    { CSS_VAL_FUCHSIA, 0xFFFF00FF },
    { CSS_VAL_GRAY, 0xFF808080 },
    { CSS_VAL_GREEN, 0xFF008000  },
    { CSS_VAL_LIME, 0xFF00FF00 },
    { CSS_VAL_MAROON, 0xFF800000 },
    { CSS_VAL_NAVY, 0xFF000080 },
    { CSS_VAL_OLIVE, 0xFF808000  },
    { CSS_VAL_ORANGE, 0xFFFFA500 },
    { CSS_VAL_PURPLE, 0xFF800080 },
    { CSS_VAL_RED, 0xFFFF0000 },
    { CSS_VAL_SILVER, 0xFFC0C0C0 },
    { CSS_VAL_TEAL, 0xFF008080  },
    { CSS_VAL_WHITE, 0xFFFFFFFF },
    { CSS_VAL_YELLOW, 0xFFFFFF00 },
    { CSS_VAL_INVERT, invertedColor },
    { CSS_VAL_GREY, 0xff808080 },
    { 0, 0 }
};

struct uiColors {
    int css_value;
    const char * configGroup;
    const char * configEntry;
QPalette::ColorGroup group;
QColorGroup::ColorRole role;
};

const char * const wmgroup = "WM";
const char * const generalgroup = "General";

/* Mapping system settings to CSS 2
* Tried hard to get an appropriate mapping - schlpbch
*/
static const uiColors uimap[] = {
    // Active window border.
    { CSS_VAL_ACTIVEBORDER, wmgroup, "background", QPalette::Active, QColorGroup::Light },
    // Active window caption.
    { CSS_VAL_ACTIVECAPTION, wmgroup, "background", QPalette::Active, QColorGroup::Text },
    // Text in caption, size box, and scrollbar arrow box.
    { CSS_VAL_CAPTIONTEXT, wmgroup, "activeForeground", QPalette::Active, QColorGroup::Text },
    // Face color for three-dimensional display elements.
    { CSS_VAL_BUTTONFACE, wmgroup, 0, QPalette::Inactive, QColorGroup::Button },
    // Dark shadow for three-dimensional display elements (for edges facing away from the light source).
    { CSS_VAL_BUTTONHIGHLIGHT, wmgroup, 0, QPalette::Inactive, QColorGroup::Light },
    // Shadow color for three-dimensional display elements.
    { CSS_VAL_BUTTONSHADOW, wmgroup, 0, QPalette::Inactive, QColorGroup::Shadow },
    // Text on push buttons.
    { CSS_VAL_BUTTONTEXT, wmgroup, "buttonForeground", QPalette::Inactive, QColorGroup::ButtonText },
    // Dark shadow for three-dimensional display elements.
    { CSS_VAL_THREEDDARKSHADOW, wmgroup, 0, QPalette::Inactive, QColorGroup::Dark },
    // Face color for three-dimensional display elements.
    { CSS_VAL_THREEDFACE, wmgroup, 0, QPalette::Inactive, QColorGroup::Button },
    // Highlight color for three-dimensional display elements.
    { CSS_VAL_THREEDHIGHLIGHT, wmgroup, 0, QPalette::Inactive, QColorGroup::Light },
    // Light color for three-dimensional display elements (for edges facing the light source).
    { CSS_VAL_THREEDLIGHTSHADOW, wmgroup, 0, QPalette::Inactive, QColorGroup::Midlight },
    // Dark shadow for three-dimensional display elements.
    { CSS_VAL_THREEDSHADOW, wmgroup, 0, QPalette::Inactive, QColorGroup::Shadow },

    // Inactive window border.
    { CSS_VAL_INACTIVEBORDER, wmgroup, "background", QPalette::Disabled, QColorGroup::Background },
    // Inactive window caption.
    { CSS_VAL_INACTIVECAPTION, wmgroup, "inactiveBackground", QPalette::Disabled, QColorGroup::Background },
    // Color of text in an inactive caption.
    { CSS_VAL_INACTIVECAPTIONTEXT, wmgroup, "inactiveForeground", QPalette::Disabled, QColorGroup::Text },
    { CSS_VAL_GRAYTEXT, wmgroup, 0, QPalette::Disabled, QColorGroup::Text },

    // Menu background
    { CSS_VAL_MENU, generalgroup, "background", QPalette::Inactive, QColorGroup::Background },
    // Text in menus
    { CSS_VAL_MENUTEXT, generalgroup, "foreground", QPalette::Inactive, QColorGroup::Background },

    // Text of item(s) selected in a control.
    { CSS_VAL_HIGHLIGHT, generalgroup, "selectBackground", QPalette::Inactive, QColorGroup::Background },

    // Text of item(s) selected in a control.
    { CSS_VAL_HIGHLIGHTTEXT, generalgroup, "selectForeground", QPalette::Inactive, QColorGroup::Background },

    // Background color of multiple document interface.
    { CSS_VAL_APPWORKSPACE, generalgroup, "background", QPalette::Inactive, QColorGroup::Text },

    // Scroll bar gray area.
    { CSS_VAL_SCROLLBAR, generalgroup, "background", QPalette::Inactive, QColorGroup::Background },

    // Window background.
    { CSS_VAL_WINDOW, generalgroup, "windowBackground", QPalette::Inactive, QColorGroup::Background },
    // Window frame.
    { CSS_VAL_WINDOWFRAME, generalgroup, "windowBackground", QPalette::Inactive, QColorGroup::Background },
    // WindowText
    { CSS_VAL_WINDOWTEXT, generalgroup, "windowForeground", QPalette::Inactive, QColorGroup::Text },
    { CSS_VAL_TEXT, generalgroup, 0, QPalette::Inactive, QColorGroup::Text },
    { 0, 0, 0, QPalette::NColorGroups, QColorGroup::NColorRoles }
};

static QColor colorForCSSValue( int css_value )
{
    // try the regular ones first
    const colorMap *col = cmap;
    while ( col->css_value && col->css_value != css_value )
        ++col;
    if ( col->css_value )
        return col->color;

    const uiColors *uicol = uimap;
    while ( uicol->css_value && uicol->css_value != css_value )
        ++uicol;
#if !APPLE_CHANGES
    if ( !uicol->css_value ) {
        if ( css_value == CSS_VAL_INFOBACKGROUND )
            return QToolTip::palette().inactive().background();
        else if ( css_value == CSS_VAL_INFOTEXT )
            return QToolTip::palette().inactive().foreground();
        else if ( css_value == CSS_VAL_BACKGROUND ) {
            KConfig bckgrConfig("kdesktoprc", true, false); // No multi-screen support
            bckgrConfig.setGroup("Desktop0");
            // Desktop background.
            return bckgrConfig.readColorEntry("Color1", &qApp->palette().disabled().background());
        }
        return khtml::invalidColor;
    }
#endif
    
    const QPalette &pal = qApp->palette();
    QColor c = pal.color( uicol->group, uicol->role );
#if !APPLE_CHANGES
    if ( uicol->configEntry ) {
        KConfig *globalConfig = KGlobal::config();
        globalConfig->setGroup( uicol->configGroup );
        c = globalConfig->readColorEntry( uicol->configEntry, &c );
    }
#endif
    
    return c;
};


void CSSStyleSelector::applyRule( int id, DOM::CSSValueImpl *value )
{
    //kdDebug( 6080 ) << "applying property " << prop->m_id << endl;

    CSSPrimitiveValueImpl *primitiveValue = 0;
    if(value->isPrimitiveValue()) primitiveValue = static_cast<CSSPrimitiveValueImpl *>(value);

    Length l;
    bool apply = false;

    // here follows a long list, defining how to aplly certain properties to the style object.
    // rather boring stuff...
    switch(id)
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
            switch(id)
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
        switch(id)
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
	} else if ( id == CSS_VAL_INLINE_BLOCK ) {
	    // inline-block is not supported at the moment, so we just ignore it.
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
        FontDef fontDef = style->htmlFont().fontDef;
        if(value->cssValueType() == CSSValue::CSS_INHERIT) {
            if(!parentNode) return;
            fontDef.smallCaps = parentStyle->htmlFont().fontDef.weight;
        } else {
            if(!primitiveValue) return;
            int id = primitiveValue->getIdent();
            if ( id == CSS_VAL_NORMAL )
                fontDef.smallCaps = false;
            else if ( id == CSS_VAL_SMALL_CAPS )
                fontDef.smallCaps = true;
            else
                return;
        }
        if (style->setFontDef( fontDef ))
            fontDirty = true;
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
                switch(primitiveValue->getIdent()) {
                    // ### we just support normal and bold fonts at the moment...
                    // setWeight can actually accept values between 0 and 99...
                    case CSS_VAL_BOLD:
                    case CSS_VAL_BOLDER:
                    case CSS_VAL_600:
                    case CSS_VAL_700:
                    case CSS_VAL_800:
                    case CSS_VAL_900:
                        fontDef.weight = QFont::Bold;
                        break;
                    case CSS_VAL_NORMAL:
                    case CSS_VAL_LIGHTER:
                    case CSS_VAL_100:
                    case CSS_VAL_200:
                    case CSS_VAL_300:
                    case CSS_VAL_400:
                    case CSS_VAL_500:
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
            o = OSCROLL; break;
        case CSS_VAL_AUTO:
            o = OAUTO; break;
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
     case CSS_PROP_TABLE_LAYOUT: {
        if(value->cssValueType() == CSSValue::CSS_INHERIT) {
            if(!parentNode) return;
            style->setTableLayout(parentStyle->tableLayout());
            return;
        }

        if ( !primitiveValue->getIdent() )
            return;

        ETableLayout l = TAUTO;
        switch( primitiveValue->getIdent() ) {
            case CSS_VAL_FIXED:
                l = TFIXED;
                // fall through
            case CSS_VAL_AUTO:
                style->setTableLayout( l );
            default:
                break;
        }
        break;
    }
        
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
        case CSS_VAL__KONQ_NOWRAP:
            s = KONQ_NOWRAP;
            break;
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
            return;
        } else if(primitiveValue) {
            style->setCursor( (ECursor) (primitiveValue->getIdent() - CSS_VAL_AUTO) );
        }
        break;        
//    case CSS_PROP_PLAY_DURING:
        // CSS2PlayDuring
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
        bool transparentBorder = false;
        QColor col;
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            switch(id)
            {
            case CSS_PROP_BACKGROUND_COLOR:
                col = parentStyle->backgroundColor(); break;
            case CSS_PROP_BORDER_TOP_COLOR:
                col = parentStyle->borderTopColor();
                transparentBorder = parentStyle->borderTopIsTransparent();
                break;
            case CSS_PROP_BORDER_RIGHT_COLOR:
                col = parentStyle->borderRightColor();
                transparentBorder = parentStyle->borderRightIsTransparent();
                break;
            case CSS_PROP_BORDER_BOTTOM_COLOR:
                col = parentStyle->borderBottomColor();
                transparentBorder = parentStyle->borderBottomIsTransparent();
                break;
            case CSS_PROP_BORDER_LEFT_COLOR:
                col = parentStyle->borderLeftColor();
                transparentBorder = parentStyle->borderLeftIsTransparent();
                break;
            case CSS_PROP_COLOR:
                col = parentStyle->color(); break;
            case CSS_PROP_OUTLINE_COLOR:
		col = parentStyle->outlineColor(); break;
            default:
            return;
        }
        } else {
            if(!primitiveValue )
                return;
            int ident = primitiveValue->getIdent();
            if ( ident ) {
                if ( ident == CSS_VAL__KONQ_TEXT )
                    col = element->getDocument()->textColor();
                else if ( ident == CSS_VAL_TRANSPARENT ) {
                    col = QColor();
                    transparentBorder = true;
                }
                else
                    col = colorForCSSValue( ident );
            } else if ( primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_RGBCOLOR ) {
#if !APPLE_CHANGES
                if(qAlpha(primitiveValue->getRGBColorValue()))
#endif
                    col.setRgb(primitiveValue->getRGBColorValue());
            } else {
                return;
            }
        }
        //kdDebug( 6080 ) << "applying color " << col.isValid() << endl;
        switch(id)
        {
        case CSS_PROP_BACKGROUND_COLOR:
            style->setBackgroundColor(col); break;
        case CSS_PROP_BORDER_TOP_COLOR:
            style->setBorderTopColor(col, transparentBorder); break;
        case CSS_PROP_BORDER_RIGHT_COLOR:
            style->setBorderRightColor(col, transparentBorder); break;
        case CSS_PROP_BORDER_BOTTOM_COLOR:
            style->setBorderBottomColor(col, transparentBorder); break;
        case CSS_PROP_BORDER_LEFT_COLOR:
            style->setBorderLeftColor(col, transparentBorder); break;
        case CSS_PROP_COLOR:
            style->setColor(col); break;
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
            switch(id)
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
        switch(id)
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
            switch(id)
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
        } else if(primitiveValue && primitiveValue->getIdent() == CSS_VAL_NORMAL){
            width = 0;
        } else {
	    if(!primitiveValue) return;
	    width = primitiveValue->computeLength(style, paintDeviceMetrics);
	}
        switch(id)
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
    case CSS_PROP_BOTTOM:
    case CSS_PROP_WIDTH:
    case CSS_PROP_MIN_WIDTH:
    case CSS_PROP_MARGIN_TOP:
    case CSS_PROP_MARGIN_RIGHT:
    case CSS_PROP_MARGIN_BOTTOM:
    case CSS_PROP_MARGIN_LEFT:
        // +inherit +auto
        if(id != CSS_PROP_MAX_WIDTH && primitiveValue &&
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
            switch(id)
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
                // Handle our quirky margin units if we have them.
                l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), Fixed, 
                           primitiveValue->isQuirkValue());
            else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
                l = Length((int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE), Percent);
            else if (type == CSSPrimitiveValue::CSS_HTML_RELATIVE)
                l = Length(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_HTML_RELATIVE)), Relative);
            else
                return;
            if (id == CSS_PROP_PADDING_LEFT || id == CSS_PROP_PADDING_RIGHT ||
                id == CSS_PROP_PADDING_TOP || id == CSS_PROP_PADDING_BOTTOM)
                // Padding can't be negative
                apply = !((l.isFixed() || l.isPercent()) && l.width(100) < 0);
            else
                apply = true;
        }
        if(!apply) return;
        switch(id)
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
        if(id != CSS_PROP_MAX_HEIGHT && primitiveValue &&
           primitiveValue->getIdent() == CSS_VAL_AUTO)
            apply = true;
        if(value->cssValueType() == CSSValue::CSS_INHERIT)
        {
            if(!parentNode) return;
	    apply = true;
            switch(id)
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
        switch(id)
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
        float oldSize;
        float size = 0;
        int minFontSize = settings->minFontSize();

        if(parentNode) {
            oldSize = parentStyle->htmlFont().fontDef.floatSize();
        } else
            oldSize = m_fontSizes[3];

        if (value->cssValueType() == CSSValue::CSS_INHERIT) {
            size = oldSize;
        } else if (primitiveValue->getIdent()) {
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
                // FIXME: Larger/smaller should actually apply a scale to the logical size.  It 		// should not simply be transforming the current size.
                size = oldSize * 1.2;
                break;
            case CSS_VAL_SMALLER:
                size = oldSize / 1.2;
                break;
            default:
                return;
            }

            // This is a "logical" font size in the sense that it is relative to some UA default.
            // Since the UA default can vary depending on the font family (e.g., monospace could be 11pt
            // but serif could be 20pt), we don't set our size specified bit.
            if (primitiveValue->getIdent() != CSS_VAL_LARGER &&
                primitiveValue->getIdent() != CSS_VAL_SMALLER) {
                // FIXME: Technically this should be logical too and apply a scale
                // to logical sizes when no explicit size is specified,
                // but until the above simplistic 1.2 mult/division calculation is
                // improved, we have to ignore these cases. 
                fontDef.sizeSpecified = false;
                fontDef.logicalSize = primitiveValue->getIdent() - CSS_VAL_XX_SMALL;
            }
            else
                fontDef.sizeSpecified = true;

        } else {
            fontDef.sizeSpecified = true;
            int type = primitiveValue->primitiveType();
            if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG) {
                size = primitiveValue->computeLengthFloat(parentStyle, paintDeviceMetrics);
                if (!khtml::printpainter && element && element->getDocument()->view())
                    size *= element->getDocument()->view()->part()->zoomFactor() / 100.0;
            } else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
                size = (primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE)
                        * oldSize) / 100;
            else
                return;
        }

        if(size <= 0) return;

        // we never want to get smaller than the minimum font size to keep fonts readable
        if(size < minFontSize ) size = minFontSize;

        //kdDebug( 6080 ) << "computed raw font size: " << size << endl;

        fontDef.setSize(size);
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
            else if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG) {
                double multiplier = 1.0;
                // Scale for the font zoom factor only for types other than "em" and "ex", since those are
                // already based on the font size.
                if (type != CSSPrimitiveValue::CSS_EMS && type != CSSPrimitiveValue::CSS_EXS && view && view->part()) {
                    multiplier = view->part()->zoomFactor() / 100.0;
                }
                lineHeight = Length(primitiveValue->computeLength(style, paintDeviceMetrics, multiplier), Fixed);
            } else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
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
    style->setHasClip();
    
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
                style->setContent(val->getStringValue(), i != 0);
            }
            else if (val->primitiveType()==CSSPrimitiveValue::CSS_ATTR)
            {
                // FIXME: Should work with generic XML attributes also, and not
                // just the hardcoded HTML set.  Can a namespace be specified for
                // an attr(foo)?
                int attrID = element->getDocument()->attrId(0, val->getStringValue(), false);
                if (attrID)
                    style->setContent(element->getAttribute(attrID).implementation(), i != 0);
            }
            else if (val->primitiveType()==CSSPrimitiveValue::CSS_URI)
            {
                CSSImageValueImpl *image = static_cast<CSSImageValueImpl *>(val);
                style->setContent(image->image(), i != 0);
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
        KWQFontFamily &firstFamily = fontDef.firstFamily();
        KWQFontFamily *currFamily = 0;
        
        for(int i = 0; i < len; i++) {
            CSSValueImpl *item = list->item(i);
            if(!item->isPrimitiveValue()) continue;
            CSSPrimitiveValueImpl *val = static_cast<CSSPrimitiveValueImpl *>(item);
            QString face;
            if( val->primitiveType() == CSSPrimitiveValue::CSS_STRING )
                face = static_cast<FontFamilyValueImpl *>(val)->fontName();
            else if (val->primitiveType() == CSSPrimitiveValue::CSS_IDENT) {
                switch (val->getIdent()) {
                    case CSS_VAL__KONQ_BODY:
                        face = settings->stdFontName();
                        break;
                    case CSS_VAL_SERIF:
                        face = settings->serifFontName();
                        fontDef.setGenericFamily(FontDef::eSerif);
                        break;
                    case CSS_VAL_SANS_SERIF:
                        face = settings->sansSerifFontName();
                        fontDef.setGenericFamily(FontDef::eSansSerif);
                        break;
                    case CSS_VAL_CURSIVE:
                        face = settings->cursiveFontName();
                        fontDef.setGenericFamily(FontDef::eCursive);
                        break;
                    case CSS_VAL_FANTASY:
                        face = settings->fantasyFontName();
                        fontDef.setGenericFamily(FontDef::eFantasy);
                        break;
                    case CSS_VAL_MONOSPACE:
                        face = settings->fixedFontName();
                        fontDef.setGenericFamily(FontDef::eMonospace);
                        break;
                }
            }
    
            if ( !face.isEmpty() ) {
                if (!currFamily) {
                    // Filling in the first family.
                    firstFamily.setFamily(face);
                    currFamily = &firstFamily;
                }
                else {
                    KWQFontFamily *newFamily = new KWQFontFamily;
                    newFamily->setFamily(face);
                    currFamily->appendFamily(newFamily);
                    currFamily = newFamily;
                }
                
                if (style->setFontDef( fontDef )) {
                    fontDirty = true;
                }
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
    case CSS_PROP_BORDER:
    case CSS_PROP_BORDER_STYLE:
    case CSS_PROP_BORDER_WIDTH:
    case CSS_PROP_BORDER_COLOR:
        if(value->cssValueType() != CSSValue::CSS_INHERIT || !parentNode) return;

        if(id == CSS_PROP_BORDER || id == CSS_PROP_BORDER_COLOR)
        {
            style->setBorderTopColor(parentStyle->borderTopColor());
            style->setBorderBottomColor(parentStyle->borderBottomColor());
            style->setBorderLeftColor(parentStyle->borderLeftColor());
            style->setBorderRightColor(parentStyle->borderRightColor());
        }
        if(id == CSS_PROP_BORDER || id == CSS_PROP_BORDER_STYLE)
        {
            style->setBorderTopStyle(parentStyle->borderTopStyle());
            style->setBorderBottomStyle(parentStyle->borderBottomStyle());
            style->setBorderLeftStyle(parentStyle->borderLeftStyle());
            style->setBorderRightStyle(parentStyle->borderRightStyle());
        }
        if(id == CSS_PROP_BORDER || id == CSS_PROP_BORDER_WIDTH)
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
        if ( value->cssValueType() == CSSValue::CSS_INHERIT ) {
            if ( !parentNode )
                return;
            FontDef fontDef = parentStyle->htmlFont().fontDef;
            style->setLineHeight( parentStyle->lineHeight() );
            if (style->setFontDef( fontDef ))
                fontDirty = true;
        } else if ( value->isFontValue() ) {
            FontValueImpl *font = static_cast<FontValueImpl *>(value);
            if ( !font->style || !font->variant || !font->weight ||
                 !font->size || !font->lineHeight || !font->family )
                return;
            applyRule( CSS_PROP_FONT_STYLE, font->style );
            applyRule( CSS_PROP_FONT_VARIANT, font->variant );
            applyRule( CSS_PROP_FONT_WEIGHT, font->weight );
            applyRule( CSS_PROP_FONT_SIZE, font->size );

            // Line-height can depend on font().pixelSize(), so we have to update the font
            // before we evaluate line-height, e.g., font: 1em/1em.  FIXME: Still not
            // good enough: style="font:1em/1em; font-size:36px" should have a line-height of 36px.
            if (fontDirty)
                CSSStyleSelector::style->htmlFont().update( paintDeviceMetrics );
            
            applyRule( CSS_PROP_LINE_HEIGHT, font->lineHeight );
            applyRule( CSS_PROP_FONT_FAMILY, font->family );
        }
        return;
        
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
  const FontDef& childFont = aStyle->htmlFont().fontDef;
  
  if (childFont.sizeSpecified || !aParentStyle)
    return;

  const FontDef& parentFont = aParentStyle->htmlFont().fontDef;

  if (childFont.genericFamily == parentFont.genericFamily)
    return;

  // For now, lump all families but monospace together.
  if (childFont.genericFamily != FontDef::eMonospace &&
      parentFont.genericFamily != FontDef::eMonospace)
    return;

  // We know the parent is monospace or the child is monospace, and that font
  // size was unspecified.  We want to alter our font size to use the correct
  // logicalSize font for our family.
  int minFontSize = settings->minFontSize();
  const QValueList<int>& fontSizes = (childFont.genericFamily == FontDef::eMonospace) ? m_fixedFontSizes : m_fontSizes;
  int size = 0;
  if (childFont.logicalSize < 0 || childFont.logicalSize > 7) // Should never happen; I'm being paranoid.
      size = fontSizes[3]; 
  if (childFont.logicalSize == 7) // KONQ_XX_LARGE
      size = (fontSizes[6]*5)/3;
  else
      size = fontSizes[childFont.logicalSize]; 
  if (size < minFontSize)
    size = minFontSize;
  
  FontDef newFontDef(childFont);
  newFontDef.setSize(size);
  aStyle->setFontDef(newFontDef);
}

} // namespace khtml
