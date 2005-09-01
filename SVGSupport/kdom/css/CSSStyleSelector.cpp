/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml css code by:
    Copyright(C) 1999-2003 Lars Knoll(knoll@kde.org)
             (C) 2003 Apple Computer, Inc.
             (C) 2004 Allan Sandfeld Jensen(kde@carewolf.com)
             (C) 2004 Germain Garand(germain@ebooksfrance.org)

    This file is part of the KDE project

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <assert.h>
#include <stdlib.h>

#include <kdebug.h>
#include <kconfig.h>
#include <kglobal.h>

#include <qtooltip.h>
#include <qvaluevector.h>
#include <qapplication.h>
#include <qpaintdevicemetrics.h>

#include "Font.h"
#include "KDOMPart.h"
#include "KDOMView.h"
#include "RectImpl.h"
#include "Namespace.h"
#include "DOMString.h"
#include "ElementImpl.h"
#include "CSSRuleImpl.h"
#include "RenderStyle.h"
#include "CSSValueImpl.h"
#include "DocumentImpl.h"
#include "CDFInterface.h"
#include "KDOMSettings.h"
#include "MediaListImpl.h"
#include "RenderStyleDefs.h"
#include "CSSRuleListImpl.h"
#include "CSSMediaRuleImpl.h"
#include "CSSStyleRuleImpl.h"
#include "CSSValueListImpl.h"
#include <kdom/css/impl/CSSStyleSelector.h>
#include "CSSImportRuleImpl.h"
#include "CSSStyleSheetImpl.h"
#include "CSSImageValueImpl.h"
#include "StyleSheetListImpl.h"
#include "DOMImplementationImpl.h"
#include "CSSPrimitiveValueImpl.h"
#include "CSSStyleDeclarationImpl.h"

#include <kdom/css/impl/cssvalues.h>
#include <kdom/css/impl/cssproperties.h>

using namespace KDOM;

// CSS property handling helper macros
#define HANDLE_INHERIT(prop, Prop) \
if(isInherit) \
{\
    style->set##Prop(parentStyle->prop());\
    return;\
}

#define HANDLE_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_INHERIT(prop, Prop) \
else if(isInitial) \
    style->set##Prop(RenderStyle::initial##Prop());

#define HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(prop, Prop, Value) \
HANDLE_INHERIT(prop, Prop) \
else if(isInitial) \
    style->set##Prop(RenderStyle::initial##Value());

#define HANDLE_INHERIT_COND(propID, prop, Prop) \
if(id == propID) \
{\
    style->set##Prop(parentStyle->prop());\
    return;\
}

#define HANDLE_INITIAL_COND(propID, Prop) \
if(id == propID) \
{\
    style->set##Prop(RenderStyle::initial##Prop());\
    return;\
}

#define HANDLE_INITIAL_COND_WITH_VALUE(propID, Prop, Value) \
if(id == propID) \
{\
    style->set##Prop(RenderStyle::initial##Value());\
    return;\
}

namespace KDOM
{
    RenderStyle *CSSStyleSelector::styleNotYetAvailable;

    enum PseudoState { PseudoUnknown, PseudoNone, PseudoLink, PseudoVisited };
    static PseudoState pseudoState;
};

CSSStyleSelector::CSSStyleSelector(DocumentImpl *doc, const QString &userStyleSheet, StyleSheetListImpl *styleSheets, const KURL &url, bool _strictParsing)
{
    KDOMView *view = doc->view();
    init((view ? view->part()->settings() : 0));

    strictParsing = _strictParsing;
    m_medium = view ? view->mediaType() : QString::fromLatin1("all");

    selectors = 0;
    selectorCache = 0;
    properties = 0;
    userStyle = 0;
    userSheet = 0;
    paintDeviceMetrics = doc->paintDeviceMetrics();

    if(paintDeviceMetrics) // this may be null, not everyone uses khtmlview(Niko)
        computeFontSizes(paintDeviceMetrics, /* FIXME (view ? view->part()->zoomFactor() : */ 100);
    
    if(!userStyleSheet.isEmpty())
    {
        userSheet = new CSSStyleSheetImpl(doc);
        userSheet->parseString(DOMString(userStyleSheet).handle());

        userStyle = new CSSStyleSelectorList();
        userStyle->append(userSheet, DOMString(m_medium).handle());
    }

    // add stylesheets from document
    authorStyle = new CSSStyleSelectorList();

    QPtrListIterator<StyleSheetImpl> it(styleSheets->styleSheets);
    for(; it.current(); ++it)
    {
           if(it.current()->isCSSStyleSheet())
            authorStyle->append(static_cast<CSSStyleSheetImpl *>(it.current()), DOMString(m_medium).handle());
    }

    KURL u = url;
    u.setQuery(QString::null);
    u.setRef(QString::null);

    encodedurl.file = u.url();

    int pos = encodedurl.file.findRev('/');
    encodedurl.path = encodedurl.file;
    if(pos > 0)
    {
        encodedurl.path.truncate(pos);
        encodedurl.path += '/';
    }

    u.setPath(QString::null);
    encodedurl.host = u.url();

    // kdDebug() << "CSSStyleSelector::CSSStyleSelector encoded url " << encodedurl.path << endl;
}

CSSStyleSelector::CSSStyleSelector(CSSStyleSheetImpl *sheet)
{
    init(0);

    KDOMView *view = sheet->doc()->view();
    m_medium = view ? view->mediaType() : QString::fromLatin1("all");

    authorStyle = new CSSStyleSelectorList();
    authorStyle->append(sheet, DOMString(m_medium).handle());
}

void CSSStyleSelector::init(const KDOMSettings *_settings)
{
    element = 0;
    settings = _settings;
    paintDeviceMetrics = 0;
    propsToApply = (CSSOrderedProperty **) malloc(128 * sizeof(CSSOrderedProperty *));
    pseudoProps = (CSSOrderedProperty **) malloc(128 * sizeof(CSSOrderedProperty *));
    propsToApplySize = 128;
    pseudoPropsSize = 128;

    defaultStyle = 0;
    defaultPrintStyle = 0;
    defaultQuirksStyle = 0;
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

void CSSStyleSelector::addSheet(CSSStyleSheetImpl *sheet)
{
    KDOMView *view = sheet->doc()->view();
    m_medium = view ? view->mediaType() : QString::fromLatin1("screen");
    authorStyle->append(sheet, DOMString(m_medium).handle());
}

void CSSStyleSelector::clear()
{
    delete styleNotYetAvailable;
    styleNotYetAvailable = 0;
}

#define MAXFONTSIZES 9

void CSSStyleSelector::computeFontSizes(QPaintDeviceMetrics *paintDeviceMetrics, int zoomFactor)
{
    computeFontSizesFor(paintDeviceMetrics, zoomFactor, m_fontSizes, false);
    computeFontSizesFor(paintDeviceMetrics, zoomFactor, m_fixedFontSizes, true);
}

void CSSStyleSelector::computeFontSizesFor(QPaintDeviceMetrics *paintDeviceMetrics, int zoomFactor, QValueVector<int> &fontSizes, bool isFixed)
{
    Q_UNUSED(isFixed);
        
#ifndef APPLE_COMPILE_HACK
    // ### get rid of float / double
    float toPix = paintDeviceMetrics->logicalDpiY() / 72.0;
    if(toPix < (96.0 / 72.0))
        toPix = (96.0 / 72.0);

    fontSizes.resize(MAXFONTSIZES);

    float scale = 1.0;
    static const float fontFactors[] = {3. / 5., 3. / 4., 8. / 9., 1., 6. / 5., 3. / 2., 2., 3., 4.};
    static const float smallFontFactors[] = {3. / 4., 5. / 6., 8. / 9., 1., 6. / 5., 3. / 2., 2., 3., 4.};
    float mediumFontSize, minFontSize, factor;

//    if(!khtml::printpainter)
    {
        scale *= zoomFactor / 100.0;
        mediumFontSize = settings->mediumFontSize() * toPix;
        minFontSize = settings->minFontSize() * toPix;
    }
/* FIXME
    else
    {
        // ## depending on something / configurable ?
        mediumFontSize = 12;
        minFontSize = 6;
    }
*/
    const float *factors = scale * mediumFontSize >= 12.5 ? fontFactors : smallFontFactors;
    for(int i = 0; i < MAXFONTSIZES; i++)
    {
        factor = scale * factors[i];
        fontSizes[i] = int(kMax(mediumFontSize * factor +.5f, minFontSize));
        // kdDebug() << "index: " << i << " factor: " << factors[i] << " font pix size: " << int(kMax(mediumFontSize*factor +.5f, minFontSize)) << endl;
    }
#endif
}

#undef MAXFONTSIZES

static inline void bubbleSort(CSSOrderedProperty **b, CSSOrderedProperty **e)
{
    while(b < e)
    {
        bool swapped = false;
        CSSOrderedProperty **x = e, **y = e + 1;
        CSSOrderedProperty **swappedPos = 0;
        do
        {
            if(!((**(--x)) < (**(--y))))
            {
                swapped = true;
                swappedPos = x;

                CSSOrderedProperty *tmp = *y;
                *y = *x; *x = tmp;
            }
        } while(x != b);

        if(!swapped)
            break;

        b = swappedPos + 1;
    }
}

static inline int nextFontSize(const QValueVector<int> &a, int v, bool smaller)
{
    // return the nearest bigger/smaller value in scale a, when v is in range.
    // otherwise increase/decrease value using a 1.2 fixed ratio
    int m, l = 0, r = a.count()-1;
    while(l <= r)
    {
        m = (l + r) / 2;

        if(a[m] == v)
        {
            return smaller ? (m ? a[m - 1] :
                                  (v * 5) / 6) :
                                  (m + 1 < int(a.count()) ?
                                  a[m + 1] : (v * 6) / 5);
        }
        else if(v < a[m])
            r = m - 1;
        else
            l = m + 1;
    }

    if(!l)
        return smaller ? (v * 5) / 6 : kMin((v * 6) / 5, a[0]);
    if(l == int(a.count()))
        return smaller ? kMax((v * 5) / 6, a[r]) : (v * 6) / 5;

    return smaller ? a[r] : a[l];
}

RenderStyle *CSSStyleSelector::styleForElement(ElementImpl *e)
{
    if(!e->ownerDocument()->haveStylesheetsLoaded() || !e->ownerDocument()->view())
    {
        if(!styleNotYetAvailable)
        {
            styleNotYetAvailable = e->ownerDocument()->implementation()->cdfInterface()->renderStyle();
            styleNotYetAvailable->setDisplay(DS_NONE);
            styleNotYetAvailable->ref();
        }

        return styleNotYetAvailable;
    }

    // set some variables we will need
    pseudoState = PseudoUnknown;
    
    element = e;    
    parentNode = e->parentNode();
    parentStyle = 0;

    if(parentNode && parentNode->nodeType() == ELEMENT_NODE)
        parentStyle = static_cast<ElementImpl *>(parentNode)->renderStyle();

    view = element->ownerDocument()->view();
    part = view->part();
    settings = part->settings();
    paintDeviceMetrics = element->ownerDocument()->paintDeviceMetrics();

    style = e->ownerDocument()->implementation()->cdfInterface()->renderStyle();
    if(parentStyle)
        style->inheritFrom(parentStyle);
    else
        parentStyle = style;

    unsigned int numPropsToApply = 0;
    unsigned int numPseudoProps = 0;

    // try to sort out most style rules as early as possible.
    Q_UINT16 cssTagId = localNamePart(element->id());
    int smatch = 0;
    int schecked = 0;

    for(unsigned int i = 0; i < selectors_size; i++)
    {
        Q_UINT16 tag = localNamePart(selectors[i]->tag);
        if(cssTagId == tag || tag == anyLocalName)
        {
            ++schecked;

            checkSelector( i, e );

            if(selectorCache[i].state == Applies)
            {
                ++smatch;

                // qDebug("adding property" );
                for(unsigned int p = 0; p < selectorCache[i].props_size; p += 2)
                {
                    for(unsigned int j = 0; j < (unsigned int) selectorCache[i].props[p + 1]; ++j)
                    {
                        if(numPropsToApply >= propsToApplySize)
                        {
                            propsToApplySize *= 2;
                            propsToApply = (CSSOrderedProperty **) realloc(propsToApply, propsToApplySize * sizeof(CSSOrderedProperty *));
                        }
                        
                        propsToApply[numPropsToApply++] = properties[selectorCache[i].props[p]+j];
                    }
                }
            }
            else if(selectorCache[i].state == AppliesPseudo)
            {
                for(unsigned int p = 0; p < selectorCache[i].props_size; p += 2)
                {
                    for(unsigned int j = 0; j < (unsigned int) selectorCache[i].props[p + 1]; ++j)
                    {
                        if(numPseudoProps >= pseudoPropsSize)
                        {
                            pseudoPropsSize *= 2;
                            pseudoProps = (CSSOrderedProperty **) realloc(pseudoProps, pseudoPropsSize * sizeof(CSSOrderedProperty *));
                        }

                        pseudoProps[numPseudoProps++] = properties[selectorCache[i].props[p] + j];
                        properties[selectorCache[i].props[p] + j]->pseudoId = (RenderStyle::PseudoId) selectors[i]->pseudoId;
                    }
                }
            }
        }
        else
            selectorCache[i].state = Invalid;
    }

    numPropsToApply = addExtraDeclarations(e, numPropsToApply);

    // inline style declarations, after all others. non css hints count
    // as author rules, and come before all other style sheets, see hack in append()
    numPropsToApply = addInlineDeclarations(e, e->styleRules(), numPropsToApply);

    // qDebug("styleForElement(%s)", e->tagName().string().latin1());
    // qDebug("%d selectors, %d checked,  %d match,  %d properties(of %d)",
    // selectors_size, schecked, smatch, numPropsToApply, properties_size);

    bubbleSort(propsToApply, propsToApply + numPropsToApply - 1);
    bubbleSort(pseudoProps, pseudoProps + numPseudoProps - 1);

    // we can't apply style rules without a view() and a part. This
    // tends to happen on delayed destruction of widget Renderobjects
    if(part)
    {
        fontDirty = false;

        if(numPropsToApply)
        {
            CSSStyleSelector::style = style;
            for(unsigned int i = 0; i < numPropsToApply; ++i)
            {
                if(fontDirty && propsToApply[i]->priority >= (1 << 30))
                {
                    // we are past the font properties, time to update to the
                    // correct font
                    CSSStyleSelector::style->htmlFont().update(paintDeviceMetrics, settings);
                    fontDirty = false;
                }

                CSSProperty *prop = propsToApply[i]->prop;
                applyRule(prop->m_id, prop->value());
            }

            if(fontDirty)
                CSSStyleSelector::style->htmlFont().update(paintDeviceMetrics, settings);
        }

        // Clean up our style object's display and text decorations (among other fixups).
        adjustRenderStyle(style, e);

        if(numPseudoProps)
        {
            fontDirty = false;

            // qDebug("%d applying %d pseudo props", e->cssTagId(), pseudoProps->count());
            for(unsigned int i = 0; i < numPseudoProps; ++i)
            {
                if(fontDirty && pseudoProps[i]->priority >= (1 << 30))
                {
                    // we are past the font properties, time to update to the
                    // correct font (do this for all pseudo styles available!)
                    RenderStyle *pseudoStyle = style->pseudoStyle;
                    while(pseudoStyle)
                    {
                        pseudoStyle->htmlFont().update(paintDeviceMetrics, settings);
                        pseudoStyle = pseudoStyle->pseudoStyle;
                    }

                    fontDirty = false;
                }

                RenderStyle *pseudoStyle;
                pseudoStyle = style->getPseudoStyle(pseudoProps[i]->pseudoId);
                if(!pseudoStyle)
                {
                    pseudoStyle = style->addPseudoStyle(pseudoProps[i]->pseudoId);
                    if(pseudoStyle)
                        pseudoStyle->inheritFrom(style);
                }

                RenderStyle *oldStyle = style;
                RenderStyle *oldParentStyle = parentStyle;
                parentStyle = style;
                style = pseudoStyle;
                if(pseudoStyle)
                {
                    CSSProperty *prop = pseudoProps[i]->prop;
                    applyRule(prop->m_id, prop->value());
                }

                style = oldStyle;
                parentStyle = oldParentStyle;
            }

            if(fontDirty)
            {
                RenderStyle *pseudoStyle = style->pseudoStyle;
                while(pseudoStyle)
                {
                    pseudoStyle->htmlFont().update(paintDeviceMetrics, settings);
                    pseudoStyle = pseudoStyle->pseudoStyle;
                }
            }
        }
    }

    // Now adjust all our pseudo-styles.
    RenderStyle *pseudoStyle = style->pseudoStyle;
    while(pseudoStyle)
    {
        adjustRenderStyle(pseudoStyle, 0);
        pseudoStyle = pseudoStyle->pseudoStyle;
    }

    // Now return the style.
    return style;
}

unsigned int CSSStyleSelector::addInlineDeclarations(ElementImpl *e,
                                                     CSSStyleDeclarationImpl *decl,
                                                     unsigned int numProps)
{
    CSSStyleDeclarationImpl* addDecls = 0;
    Q_UNUSED(e);

    if(!decl && !addDecls)
        return numProps;

    QPtrList<CSSProperty> *values = decl ? decl->values() : 0;
    QPtrList<CSSProperty> *addValues = addDecls ? addDecls->values() : 0;
    if(!values && !addValues)
        return numProps;

    int firstLen = values ? values->count() : 0;
    int secondLen = addValues ? addValues->count() : 0;
    int totalLen = firstLen + secondLen;

    if(inlineProps.size() < (unsigned int) totalLen)
        inlineProps.resize(totalLen + 1);

    if(numProps + totalLen >= propsToApplySize)
    {
        propsToApplySize += propsToApplySize;
        propsToApply = (CSSOrderedProperty **) realloc(propsToApply, propsToApplySize * sizeof(CSSOrderedProperty *));
    }

    CSSOrderedProperty *array = (CSSOrderedProperty *) inlineProps.data();
    for(int i = 0; i < totalLen; i++)
    {
        if(i == firstLen)
            values = addValues;

        CSSProperty *prop = values->at(i >= firstLen ? i - firstLen : i);
        Source source = Inline;

        if(prop->m_important)
            source = InlineImportant;

        if(prop->m_nonCSSHint)
            source = NonCSSHint;

        bool first = (decl->interface() ? decl->interface()->cssPropertyApplyFirst(prop->m_id) : false);

        array->prop = prop;
        array->pseudoId = RenderStyle::NOPSEUDO;
        array->selector = 0;
        array->position = i;
        array->priority = (!first << 30) |(source << 24);
        propsToApply[numProps++] = array++;
    }

    return numProps;
}

static bool subject;

// modified version of the one in kurl.cpp
static void cleanpath(QString &path)
{
    int pos;
    while((pos = path.find(QString::fromLatin1("/../"))) != -1)
    {
        int prev = 0;
        if(pos > 0)
            prev = path.findRev('/', pos -1);

        // don't remove the host, i.e. http://foo.org/../foo.html
        if(prev < 0 || (prev > 3 && path.findRev(QString::fromLatin1("://"), prev - 1) == prev - 2))
            path.remove(pos, 3);
        else // matching directory found ?
            path.remove(prev, pos- prev + 3);
    }

    pos = 0;

    // Don't remove "//" from an anchor identifier. -rjw
    // Set refPos to -2 to mean "I haven't looked for the anchor yet".
    // We don't want to waste a function call on the search for the anchor
    // in the vast majority of cases where there is no "//" in the path.
    int refPos = -2;
    while((pos = path.find(QString::fromLatin1("//"), pos)) != -1)
    {
        if(refPos == -2)
            refPos = path.find('#', 0);
        if(refPos > 0 && pos >= refPos)
            break;

        if(pos == 0 || path[pos - 1] != ':')
            path.remove(pos, 1);
        else
            pos += 2;
    }

    while((pos = path.find(QString::fromLatin1("/./"))) != -1)
        path.remove(pos, 2);
}

static void checkPseudoState(const CSSStyleSelector::Encodedurl &encodedurl, ElementImpl *e)
{
    if(DOMString(e->localName()) != QString::fromLatin1("a"))
    {
        pseudoState = PseudoNone;
        return;
    }

    DOMString attr(e->getAttribute(DOMString("href").handle()));
    if(attr.isNull())
    {
        pseudoState = PseudoNone;
        return;
    }

    QConstString cu(attr.unicode(), attr.length());
    QString u = cu.qstring();
    if(!u.contains(QString::fromLatin1("://")))
    {
        if(u[0] == '/')
            u = encodedurl.host + u;
        else if(u[0] == '#')
            u = encodedurl.file + u;
        else
            u = encodedurl.path + u;

        cleanpath(u);
    }

    // FIXME: link visited logic!

    bool contains = false; // KHTMLFactory::vLinks()->contains(u);
//    if(!contains && u.contains('/') == 2)
//        contains = KHTMLFactory::vLinks()->contains(u+'/');
    pseudoState = contains ? PseudoVisited : PseudoLink;
}

// a helper function for parsing nth-arguments
static inline bool matchNth(int count, const QString& nth)
{
    if(nth.isEmpty())
        return false;

    int a = 0, b = 0;
    if(nth == "odd")
    {
        a = 2;
        b = 1;
    }
    else if(nth == "even")
    {
        a = 2;
        b = 0;
    }
    else
    {
        int n = nth.find('n');
        if(n != -1)
        {
            if(nth[0] == '-')
            {
                if(n == 1)
                    a = -1;
                else
                    a = nth.mid(1, n - 1).toInt();
            }
            else
                a = nth.left(n).toInt();

            int p = nth.find('+');
            if(p != -1)
                b = nth.mid(p + 1).toInt();
            else
            {
                p = nth.find('-');
                b = -nth.mid(p + 1).toInt();
            }                
        }
        else
            b = nth.toInt();
    }

    if(a == 0)
        return count == b;
    else if(a > 0)
    {
        if(count < b)
            return false;
        else
            return (count - b) % a == 0;
    }
    else if(a < 0)
    {
        if(count > b)
            return false;
        else
            return (b - count) % (-a) == 0;
    }

    return false;
}

void CSSStyleSelector::checkSelector(int selIndex, ElementImpl *e)
{
    dynamicPseudo = RenderStyle::NOPSEUDO;

    NodeImpl *n = e;

    selectorCache[selIndex].state = Invalid;
    CSSSelector *sel = selectors[selIndex];

    // we have the subject part of the selector
    subject = true;

    // We track whether or not the rule contains only :hover and :active
    // in a simple selector. If so, we can't allow that to apply to every
    // element on the page.  We assume the author intended to apply the
    // rules only to links.
    bool onlyHoverActive = (sel->tag == anyQName &&
                            (sel->match == CSSSelector::PseudoClass &&
                             (sel->pseudoType() == CSSSelector::PseudoHover ||
                              sel->pseudoType() == CSSSelector::PseudoActive)));

    bool affectedByHover = style->affectedByHoverRules();
    bool affectedByActive = style->affectedByActiveRules();

    // first selector has to match
    if(!checkOneSelector(sel, e))
        return;

    // check the subselectors
    CSSSelector::Relation relation = sel->relation;
    while((sel = sel->tagHistory))
    {
        if(!isElementNode(n))
            return;

        switch(relation)
        {
        case CSSSelector::Descendant:
        {
            bool found = false;
            while(!found)
            {
                subject = false;
                n = n->parentNode();
                if(!n || !isElementNode(n)) return;
                ElementImpl *elem = static_cast<ElementImpl *>(n);
                if(checkOneSelector(sel, elem)) found = true;
            }
            break;
        }
        case CSSSelector::Child:
        {
            subject = false;
            n = n->parentNode();
            if(!strictParsing)
                while(n && isImplicitNode(n)) n = n->parentNode();
            if(!n || !isElementNode(n)) return;
            ElementImpl *elem = static_cast<ElementImpl *>(n);
            if(!checkOneSelector(sel, elem)) return;
            break;
        }
        case CSSSelector::DirectAdjacent:
        {
            subject = false;
            n = n->previousSibling();
            while(n && !isElementNode(n))
                n = n->previousSibling();
            if(!n) return;
            ElementImpl *elem = static_cast<ElementImpl *>(n);
            if(!checkOneSelector(sel, elem)) return;
            break;
        }
        case CSSSelector::IndirectAdjacent:
        {
            subject = false;
            ElementImpl *elem = 0;
            do
            {
                n = n->previousSibling();
                while(n && !isElementNode(n))
                    n = n->previousSibling();
                if(!n) return;
                elem = static_cast<ElementImpl *>(n);
            } while(!checkOneSelector(sel, elem));
            break;
        }
        case CSSSelector::SubSelector:
        {
            if(onlyHoverActive)
                onlyHoverActive = (sel->match == CSSSelector::PseudoClass &&
                                   (sel->pseudoType() == CSSSelector::PseudoHover ||
                                    sel->pseudoType() == CSSSelector::PseudoActive));

            //kdDebug() << "CSSOrderedRule::checkSelector" << endl;
            ElementImpl *elem = static_cast<ElementImpl *>(n);
            // a selector is invalid if something follows :first-xxx
            if(dynamicPseudo != RenderStyle::NOPSEUDO)
                return;

            if(!checkOneSelector(sel, elem)) return;
            //kdDebug() << "CSSOrderedRule::checkSelector: passed" << endl;
            break;
        }
        }
        relation = sel->relation;
    }

    // disallow *:hover, *:active, and *:hover:active except for links
    if(onlyHoverActive && subject)
    {
        if(pseudoState == PseudoUnknown)
            checkPseudoState(encodedurl, e);

        if(pseudoState == PseudoNone)
        {
            if(!affectedByHover && style->affectedByHoverRules())
                style->setAffectedByHoverRules(false);
            if(!affectedByActive && style->affectedByActiveRules())
                style->setAffectedByActiveRules(false);
            return;
        }
    }

    if(dynamicPseudo != RenderStyle::NOPSEUDO)
    {
        selectorCache[selIndex].state = AppliesPseudo;
        selectors[ selIndex ]->pseudoId = dynamicPseudo;
    }
    else
        selectorCache[selIndex].state = Applies;
    //qDebug("selector %d applies", selIndex);
    //selectors[ selIndex ]->print();
}

bool CSSStyleSelector::checkOneSelector(CSSSelector *sel, ElementImpl *e)
{
    if(!e)
        return false;

    if(sel->tag != anyQName)
    {
        int eltID = e->id();
        Q_UINT16 localName = localNamePart(eltID);
        Q_UINT16 ns = namespacePart(eltID);
        Q_UINT16 selLocalName = localNamePart(sel->tag);
        Q_UINT16 selNS = namespacePart(sel->tag);

        /* FIXME: to khtml2  ?
        if(localName <= ID_LAST_TAG && e->isHTMLElement())
            ns = xhtmlNamespace; // FIXME: Really want to move away from this complicated hackery and just
                                 //        switch tags and attr names over to AtomicStrings.
        */

        if((selLocalName != anyLocalName && localName != selLocalName) || (selNS != anyNamespace && ns != selNS))
            return false;
    }

    if(sel->attr)
    {
        DOMString value(e->getAttribute(sel->attr));
        DOMString selValue(sel->value);
        if(value.isEmpty())
            return false; // attribute is not set

        switch(sel->match)
        {
        case CSSSelector::Exact:
        {
            // FIXME: This would need to be overridden by khtml2!
            /* attribute values are case insensitive in all HTML modes,
                even in the strict ones */
            //if(e->ownerDocument()->htmlMode() != DocumentImpl::XHtml)
            //{
                if(strcasecmp(selValue, value))
                    return false;
            //}
            //else
            //{
            //    if(strcmp(selValue, value))
            //        return false;
            //}
            break;
        }
        case CSSSelector::Id:
        {
            if((strictParsing && strcmp(selValue, value)) ||
              (!strictParsing && strcasecmp(selValue, value)))
                return false;

            break;
        }
        case CSSSelector::Set:
            break;
        case CSSSelector::List:
        {
            int spacePos = value.find(' ', 0);
            if(spacePos == -1)
            {
                // There is no list, just a single item.  We can avoid
                // allocing QStrings and just treat this as an exact
                // match check.
                if((strictParsing && strcmp(selValue, value)) ||
                   (!strictParsing && strcasecmp(selValue, value)))
                    return false;
                break;
            }
            // The selector's value can't contain a space, or it's
            // totally bogus.
            spacePos = selValue.find(' ');
            if(spacePos != -1)
                return false;

            QString str = value.string();
            QString selStr = selValue.string();
            const int selStrlen = selStr.length();
            int pos = 0;
            for(;;)
            {
                pos = str.find(selStr, pos, strictParsing);
                if(pos == -1) return false;
                if(pos == 0 || str[pos-1] == ' ')
                {
                    uint endpos = pos + selStrlen;
                    if(endpos >= str.length() || str[endpos] == ' ')
                        break; // We have a match.
                }
                ++pos;
            }
            break;
        }
        case CSSSelector::Contain:
        {
            //kdDebug() << "checking for contains match" << endl;
            QString str = value.string();
            QString selStr = selValue.string();
            int pos = str.find(selStr, 0, strictParsing);
            if(pos == -1) return false;
            break;
        }
        case CSSSelector::Begin:
        {
            //kdDebug() << "checking for beginswith match" << endl;
            QString str = value.string();
            QString selStr = selValue.string();
            int pos = str.find(selStr, 0, strictParsing);
            if(pos != 0) return false;
            break;
        }
        case CSSSelector::End:
        {
            //kdDebug() << "checking for endswith match" << endl;
            QString str = value.string();
            QString selStr = selValue.string();
            if(strictParsing && !str.endsWith(selStr)) return false;
            if(!strictParsing)
            {
                int pos = str.length() - selStr.length();
                if(pos < 0 || pos != str.find(selStr, pos, false))
                    return false;
            }
            break;
        }
        case CSSSelector::Hyphen:
        {
            //kdDebug() << "checking for hyphen match" << endl;
            QString str = value.string();
            QString selStr = selValue.string();
            if(str.length() < selStr.length()) return false;
            // Check if str begins with selStr:
            if(str.find(selStr, 0, strictParsing) != 0) return false;
            // It does. Check for exact match or following '-':
            if(str.length() != selStr.length()
                && str[selStr.length()] != '-') return false;
            break;
        }
        case CSSSelector::PseudoClass:
        case CSSSelector::PseudoElement:
        case CSSSelector::None:
            break;
        }
    }

    if(sel->match == CSSSelector::PseudoClass || sel->match == CSSSelector::PseudoElement)
    {
        // Pseudo elements. We need to check first child here. No dynamic pseudo
        // elements for the moment
        // kdDebug() << "CSSOrderedRule::pseudo " << value << endl;
        switch(sel->pseudoType())
        {
        case CSSSelector::PseudoEmpty:
        {
            // If e is not closed yet we don't know the number of children
            if(!e->closed())
            {
                e->setRestyleSelfLate();
                return false;
            }

            if(!e->firstChild())
                return true;

            break;
        }
        case CSSSelector::PseudoFirstChild:
        {
            // first-child matches the first child that is an element!
            if(e->parentNode() && isElementNode(e->parentNode()))
            {
                NodeImpl *n = e->previousSibling();
                while(n && !isElementNode(n))
                    n = n->previousSibling();

                if(!n)
                    return true;
            }
            break;
        }

        case CSSSelector::PseudoLastChild:
        {
            // last-child matches the last child that is an element!
            if(e->parentNode() && isElementNode(e->parentNode()))
            {
                if(!e->parentNode()->closed())
                {
                    e->setRestyleLate();
                    static_cast<ElementImpl *>(e->parentNode())->setRestyleChildrenLate();
                    return false;
                }

                NodeImpl *n = e->nextSibling();
                while(n && !isElementNode(n))
                    n = n->nextSibling();

                if(!n)
                    return true;
            }
            break;
        }

        case CSSSelector::PseudoOnlyChild:
        {
            // If both first-child and last-child apply, then only-child applies.
            if(e->parentNode() && isElementNode(e->parentNode()))
            {
                if(!e->parentNode()->closed())
                {
                    e->setRestyleLate();
                    static_cast<ElementImpl *>(e->parentNode())->setRestyleChildrenLate();
                    return false;
                }

                NodeImpl *n = e->previousSibling();
                while(n && !isElementNode(n))
                    n = n->previousSibling();

                if(!n)
                {
                    n = e->nextSibling();
                    while(n && !isElementNode(n))
                        n = n->nextSibling();

                    if(!n)
                        return true;
                }
            }
            break;
        }
        case CSSSelector::PseudoNthChild:
        {
            // nth-child matches every (a*n+b)th element!
            if(e->parentNode() && isElementNode(e->parentNode()))
            {
                int count = 1;
                NodeImpl *n = e->previousSibling();
                while(n)
                {
                    if(isElementNode(n)) count++;
                    n = n->previousSibling();
                }

                if(matchNth(count,DOMString(sel->string_arg).string()))
                    return true;
            }
            break;
        }
        case CSSSelector::PseudoNthLastChild:
        {
            if(e->parentNode() && isElementNode(e->parentNode()))
            {
                if(!e->parentNode()->closed())
                {
                    e->setRestyleLate();
                    static_cast<ElementImpl *>(e->parentNode())->setRestyleChildrenLate();
                    return false;
                }

                int count = 1;
                NodeImpl *n = e->nextSibling();
                while(n)
                {
                    if(isElementNode(n)) count++;
                    n = n->nextSibling();
                }

                if(matchNth(count,DOMString(sel->string_arg).string()))
                    return true;
            }
            break;
        }
        case CSSSelector::PseudoFirstOfType:
        {
            // first-of-type matches the first element of its type!
            if(e->parentNode() && isElementNode(e->parentNode()))
            {
                DOMStringImpl *type = e->tagName();
                NodeImpl *n = e->previousSibling();
                while(n)
                {
                    if(isElementNode(n))
                    {
                        if(static_cast<ElementImpl *>(n)->tagName() == type)
                            break;
                    }

                    n = n->previousSibling();
                }

                if(!n)
                    return true;
            }
            break;
        }
        case CSSSelector::PseudoLastOfType:
        {
            // last-child matches the last child that is an element!
            if(e->parentNode() && isElementNode(e->parentNode()))
            {
                if(!e->parentNode()->closed())
                {
                    e->setRestyleLate();
                    static_cast<ElementImpl *>(e->parentNode())->setRestyleChildrenLate();
                    return false;
                }

                DOMStringImpl *type = e->tagName();
                NodeImpl* n = e->nextSibling();
                while(n)
                {
                    if(isElementNode(n))
                    {
                        if(static_cast<ElementImpl*>(n)->tagName() == type)
                            break;
                    }

                    n = n->nextSibling();
                }

                if(!n)
                    return true;
            }
            break;
        }
        case CSSSelector::PseudoOnlyOfType:
        {
            // If both first-of-type and last-of-type apply, then only-of-type applies.
            if(e->parentNode() && isElementNode(e->parentNode()))
            {
                if(!e->parentNode()->closed())
                {
                    e->setRestyleLate();
                    static_cast<ElementImpl*>(e->parentNode())->setRestyleChildrenLate();
                    return false;
                }

                DOMString type(e->tagName());
                NodeImpl *n = e->previousSibling();
                while(n && !(isElementNode(n) && DOMString(static_cast<ElementImpl *>(n)->tagName()) == type))
                    n = n->previousSibling();

                if(!n)
                {
                    n = e->nextSibling();
                    while(n && !(isElementNode(n) && DOMString(static_cast<ElementImpl *>(n)->tagName()) == type))
                        n = n->nextSibling();

                    if(!n)
                        return true;
                }
            }
            break;
        }
        case CSSSelector::PseudoNthOfType:
        {
            // nth-of-type matches every (a*n+b)th element of this type!
            if(e->parentNode() && isElementNode(e->parentNode()))
            {
                int count = 1;
                DOMStringImpl *type = e->tagName();
                NodeImpl *n = e->previousSibling();
                while(n)
                {
                    if(isElementNode(n) && static_cast<ElementImpl*>(n)->tagName() == type) count++;
                    n = n->previousSibling();
                }

                if(matchNth(count,DOMString(sel->string_arg).string()))
                    return true;
            }
            break;
        }
        case CSSSelector::PseudoNthLastOfType:
        {
            if(e->parentNode() && isElementNode(e->parentNode()))
            {
                if(!e->parentNode()->closed())
                {
                    e->setRestyleLate();
                    static_cast<ElementImpl*>(e->parentNode())->setRestyleChildrenLate();
                    return false;
                }

                int count = 1;
                DOMStringImpl *type = e->tagName();
                NodeImpl *n = e->nextSibling();
                while(n)
                {
                    if(isElementNode(n) && static_cast<ElementImpl*>(n)->tagName() == type) count++;
                    n = n->nextSibling();
                }

                if(matchNth(count,DOMString(sel->string_arg).string()))
                    return true;
            }
            break;
        }
        case CSSSelector::PseudoTarget:
        {
            if(e == e->ownerDocument()->getCSSTarget())
                return true;
            break;
        }
        case CSSSelector::PseudoLink:
        {
            if(pseudoState == PseudoUnknown)
                checkPseudoState(encodedurl, e);
            if(pseudoState == PseudoLink)
                return true;
            break;
        }
        case CSSSelector::PseudoVisited:
        {
            if(pseudoState == PseudoUnknown)
                checkPseudoState(encodedurl, e);
            if(pseudoState == PseudoVisited)
                return true;
            break;
        }
        case CSSSelector::PseudoHover:
        {
            // If we're in quirks mode, then hover should never match anchors with no
            // href.  This is important for sites like wsj.com.
            if(strictParsing || DOMString(e->tagName()) != "a" || e->hasAnchor())
            {
                if(element == e)
                    style->setAffectedByHoverRules(true);

                // FIXME: hover logic
                /*if(e->renderer())
                {
                    if(element != e)
                        e->renderer()->style()->setAffectedByHoverRules(true);
                    if(e->renderer()->mouseInside())
                        return true;
                }*/
            }
            break;
        }
        case CSSSelector::PseudoFocus:
        {
            if(e && e->focused())
                return true;

            break;
        }
        case CSSSelector::PseudoActive:
        {
            // If we're in quirks mode, then :active should never match anchors with no
            // href.
            if(strictParsing || DOMString(e->tagName()) != "a" || e->hasAnchor())
            {
                if(element == e)
                    style->setAffectedByActiveRules(true);

                // FIXME: active logic
                /*else if (e->renderer())
                    e->renderer()->style()->setAffectedByActiveRules(true);
                    if (e->active())
                        return true;
                */
            }
            break;
        }
        case CSSSelector::PseudoRoot:
        {
            if(e == e->ownerDocument()->documentElement())
                return true;
            break;
        }
        case CSSSelector::PseudoLang:
        {
            DOMString value(getLangAttribute(e));
            if(value.isNull()) return false;
            QString langAttr = value.string();
            QString langSel = DOMString(sel->string_arg).string();
            return langAttr.startsWith(langSel);
        }
        case CSSSelector::PseudoNot:
        {
            // check the simple selector
            for(CSSSelector *subSel = sel->simpleSelector; subSel != 0; subSel = subSel->tagHistory)
            {
                // :not cannot nest.  I don't really know why this is a
                // restriction in CSS3, but it is, so let's honor it.
                if(subSel->simpleSelector)
                    break;

                if(!checkOneSelector(subSel, e))
                    return true;
            }
            break;
        }
        /* FIXME: This has to be handled in khtml2
        case CSSSelector::PseudoEnabled:
        {
            if(e->isGenericFormElement())
            {
                HTMLGenericFormElementImpl *form;
                form = static_cast<HTMLGenericFormElementImpl*>(e);
                return !form->disabled();
            }
            break;
        }
        case CSSSelector::PseudoDisabled:
        {
            if(e->isGenericFormElement())
            {
                HTMLGenericFormElementImpl *form;
                form = static_cast<HTMLGenericFormElementImpl*>(e);
                return form->disabled();
            }
            break;
        }
        */
        case CSSSelector::PseudoContains:
        {
            if(!e->closed())
            {
                e->setRestyleSelfLate();
                return false;
            }

            DOMString s(e->textContent());
            QString selStr = DOMString(sel->string_arg).string();
            return s.string().contains(selStr);
        }
        case CSSSelector::PseudoChecked:
        case CSSSelector::PseudoIndeterminate:
        case CSSSelector::PseudoEnabled:
        case CSSSelector::PseudoDisabled:
            /* not supported for now */
        case CSSSelector::PseudoOther:
            break;

        // Pseudo-elements:
        case CSSSelector::PseudoFirstLine:
        {
            if(subject)
            {
                dynamicPseudo = RenderStyle::FIRST_LINE;
                return true;
            }
            break;
        }
        case CSSSelector::PseudoFirstLetter:
        {
            if(subject)
            {
                dynamicPseudo = RenderStyle::FIRST_LETTER;
                return true;
            }
            break;
        }
        case CSSSelector::PseudoSelection:
        {
            dynamicPseudo = RenderStyle::SELECTION;
            return true;
        }
        case CSSSelector::PseudoBefore:
        {
            dynamicPseudo = RenderStyle::BEFORE;
            return true;
        }
        case CSSSelector::PseudoAfter:
        {
            dynamicPseudo = RenderStyle::AFTER;
            return true;
        }
        case CSSSelector::PseudoNotParsed:
        {
            Q_ASSERT(false);
            break;
        }
        }

        return false;
    }

    // ### add the rest of the checks...
    return true;
}

void CSSStyleSelector::clearLists()
{
    delete []selectors;

    if(selectorCache)
    {
        for(unsigned int i = 0; i < selectors_size; i++)
            delete []selectorCache[i].props;

        delete []selectorCache;
    }

    if(properties)
    {
        CSSOrderedProperty **prop = properties;
        while(*prop)
        {
            delete(*prop);
            prop++;
        }

        delete []properties;
    }

    selectors = 0;
    properties = 0;
    selectorCache = 0;
}


void CSSStyleSelector::buildLists()
{
    clearLists();
    // collect all selectors and Properties in lists. Then transfer them to the array for faster lookup.

    QPtrList<CSSSelector> selectorList;
    CSSOrderedPropertyList propertyList;

    if(m_medium == "print" && defaultPrintStyle)
        defaultPrintStyle->collect(&selectorList, &propertyList, Default, Default);
    else if(defaultStyle)
        defaultStyle->collect(&selectorList, &propertyList, Default, Default);

    if(!strictParsing && defaultQuirksStyle)
        defaultQuirksStyle->collect(&selectorList, &propertyList, Default, Default);

    if(userStyle)
        userStyle->collect(&selectorList, &propertyList, User, UserImportant);

    if(authorStyle)
        authorStyle->collect(&selectorList, &propertyList, Author, AuthorImportant);

    selectors_size = selectorList.count();
    selectors = new CSSSelector *[selectors_size];
    CSSSelector *s = selectorList.first();
    CSSSelector **sel = selectors;
    while(s) { *sel = s; s = selectorList.next(); ++sel; }

    selectorCache = new SelectorCache[selectors_size];
    for(unsigned int i = 0; i < selectors_size; i++)
    {
        selectorCache[i].state = Unknown;
        selectorCache[i].props_size = 0;
        selectorCache[i].props = 0;
    }

    // presort properties. Should make the sort() calls in styleForElement faster.
    propertyList.sort();
    properties_size = propertyList.count() + 1;
    properties = new CSSOrderedProperty *[properties_size];
    CSSOrderedProperty *p = propertyList.first();
    CSSOrderedProperty **prop = properties;
    while(p) { *prop = p; p = propertyList.next(); ++prop; }
    *prop = 0;

    unsigned int* offsets = new unsigned int[selectors_size];
    if(properties[0])
        offsets[properties[0]->selector] = 0;

    for(unsigned int p = 1; p < properties_size; ++p)
    {
        if(!properties[p] ||(properties[p]->selector != properties[p - 1]->selector))
        {
            unsigned int sel = properties[p - 1]->selector;
            int *newprops = new int[selectorCache[sel].props_size + 2];
            for(unsigned int i = 0; i < selectorCache[sel].props_size; i++)
                newprops[i] = selectorCache[sel].props[i];

            newprops[selectorCache[sel].props_size] = offsets[sel];
            newprops[selectorCache[sel].props_size + 1] = p - offsets[sel];
            delete []selectorCache[sel].props;

            selectorCache[sel].props = newprops;
            selectorCache[sel].props_size += 2;

            if(properties[p])
            {
                sel = properties[p]->selector;
                offsets[sel] = p;
            }
        }
    }

    delete []offsets;
}

// ----------------------------------------------------------------------

CSSOrderedRule::CSSOrderedRule(CSSStyleRuleImpl *r, CSSSelector *s, int _index)
{
    rule = r;
    if(rule)
        r->ref();

    index = _index;
    selector = s;
}

CSSOrderedRule::~CSSOrderedRule()
{
    if(rule)
        rule->deref();
}

// -----------------------------------------------------------------

CSSStyleSelectorList::CSSStyleSelectorList() : QPtrList<CSSOrderedRule>()
{
    setAutoDelete(true);
}

CSSStyleSelectorList::~CSSStyleSelectorList()
{
}

void CSSStyleSelectorList::append(CSSStyleSheetImpl *sheet, DOMStringImpl *medium)
{
    if(!sheet || !sheet->isCSSStyleSheet())
        return;

    // No media implies "all", but if a medialist exists it must
    // contain our current medium
    if(sheet->media() && !sheet->media()->contains(medium))
        return; // style sheet not applicable for this medium

    int len = sheet->length();

    for(int i = 0; i< len; i++)
    {
        StyleBaseImpl *item = sheet->item(i);
        if(item->isStyleRule())
        {
            CSSStyleRuleImpl *r = static_cast<CSSStyleRuleImpl *>(item);
            QPtrList<CSSSelector> *s = r->selector();
            for(int j = 0; j <(int)s->count(); j++)
            {
                CSSOrderedRule *rule = new CSSOrderedRule(r, s->at(j), count());
                QPtrList<CSSOrderedRule>::append(rule);
                //kdDebug() << "appending StyleRule!" << endl;
            }
        }
        else if(item->isImportRule())
        {
            CSSImportRuleImpl *import = static_cast<CSSImportRuleImpl *>(item);

            //kdDebug() << "@import: Media: "
            //                << import->media()->mediaText().string() << endl;

            if(!import->media() || import->media()->contains(medium))
            {
                CSSStyleSheetImpl *importedSheet = import->styleSheet();
                append(importedSheet, medium);
            }
        }
        else if(item->isMediaRule())
        {
            CSSMediaRuleImpl *r = static_cast<CSSMediaRuleImpl *>(item);
            CSSRuleListImpl *rules = r->cssRules();

            //DOMString mediaText = media->mediaText();
            //kdDebug() << "@media: Media: "
            //                << r->media()->mediaText().string() << endl;

            if((!r->media() || r->media()->contains(medium)) && rules)
            {
                // Traverse child elements of the @import rule. Since
                // many elements are not allowed as child we do not use
                // a recursive call to append() here
                for(unsigned j = 0; j < rules->length(); j++)
                {
                    //kdDebug() << "*** Rule #" << j << endl;

                    CSSRuleImpl *childItem = rules->item(j);
                    if(childItem->isStyleRule())
                    {
                        // It is a StyleRule, so append it to our list
                        CSSStyleRuleImpl *styleRule = static_cast<CSSStyleRuleImpl *>(childItem);

                        QPtrList<CSSSelector> *s = styleRule->selector();
                        for(int j = 0; j <(int) s->count(); j++)
                        {
                            CSSOrderedRule *orderedRule = new CSSOrderedRule(styleRule, s->at(j), count());
                            QPtrList<CSSOrderedRule>::append(orderedRule);
                        }
                    }
                    else
                    {
                        //kdDebug() << "Ignoring child rule of "
                        //    "ImportRule: rule is not a StyleRule!" << endl;
                    }
                }   // for rules
            }   // if rules
            else
            {
                //kdDebug() << "CSSMediaRule not rendered: "
                //                << "rule empty or wrong medium!" << endl;
            }
        }
        // ### include other rules
    }
}

void CSSStyleSelectorList::collect(QPtrList<CSSSelector> *selectorList, CSSOrderedPropertyList *propList, Source regular, Source important)
{
    CSSOrderedRule *r = first();
    while(r)
    {
        CSSSelector *sel = selectorList->first();
        int selectorNum = 0;
        while(sel)
        {
            if(*sel == *(r->selector))
                break;

            sel = selectorList->next();
            selectorNum++;
        }
        if(!sel)
            selectorList->append(r->selector);

        propList->append(r->rule->declaration(), selectorNum, r->selector->specificity(), regular, important);
        r = next();
    }
}

// -------------------------------------------------------------------------

int CSSOrderedPropertyList::compareItems(QPtrCollection::Item i1, QPtrCollection::Item i2)
{
    int diff = static_cast<CSSOrderedProperty *>(i1)->priority -
               static_cast<CSSOrderedProperty *>(i2)->priority;

    return diff ? diff : static_cast<CSSOrderedProperty *>(i1)->position -
                         static_cast<CSSOrderedProperty *>(i2)->position;
}

void CSSOrderedPropertyList::append(CSSStyleDeclarationImpl *decl,
                                    unsigned int selector, unsigned int specificity,
                                    Source regular, Source important)
{
    QPtrList<CSSProperty> *values = decl->values();
    if(!values)
        return;

    int len = values->count();
    for(int i = 0; i < len; i++)
    {
        CSSProperty *prop = values->at(i);
        Source source = regular;

        if(prop->m_important) source = important;
        if(prop->m_nonCSSHint) source = NonCSSHint;

        bool first = ((decl && decl->interface()) ? decl->interface()->cssPropertyApplyFirst(prop->m_id) : false);
        QPtrList<CSSOrderedProperty>::append(new CSSOrderedProperty(prop, selector, first, source, specificity, count()));
    }
}

// -------------------------------------------------------------------------------------
// this is mostly boring stuff on how to apply a certain rule to the renderstyle...
Length CSSStyleSelector::convertToLength(CSSPrimitiveValueImpl *primitiveValue, RenderStyle *style,
                                         QPaintDeviceMetrics *paintDeviceMetrics, bool *ok)
{
    Length l;
    if(!primitiveValue)
    {
        if(ok)
        *ok = false;
    }
    else
    {
        int type = primitiveValue->primitiveType();
        if(type > CSS_PERCENTAGE && type < CSS_DEG)
            l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), LT_FIXED);
        else if(type == CSS_PERCENTAGE)
            l = Length(int(primitiveValue->getFloatValue(CSS_PERCENTAGE)), LT_PERCENT);
        else if(type == CSS_NUMBER)
            l = Length(int(primitiveValue->getFloatValue(CSS_NUMBER) * 100), LT_PERCENT);
        else if(type == CSS_HTML_RELATIVE)
            l = Length(int(primitiveValue->getFloatValue(CSS_HTML_RELATIVE)), LT_RELATIVE);
        else if(ok)
            *ok = false;
    }
    return l;
}


// color mapping code
struct colorMap
{
    int css_value;
    QRgb color;
};

static const colorMap cmap[] =
{
    { CSS_VAL_AQUA, 0xFF00FFFF },
    { CSS_VAL_BLACK, 0xFF000000 },
    { CSS_VAL_BLUE, 0xFF0000FF },
    { CSS_VAL_CRIMSON, 0xFFDC143C },
    { CSS_VAL_FUCHSIA, 0xFFFF00FF },
    { CSS_VAL_GRAY, 0xFF808080 },
    { CSS_VAL_GREEN, 0xFF008000  },
    { CSS_VAL_INDIGO, 0xFF4B0082 },
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
    { CSS_VAL_INVERT, 0x00000002 },
   // { CSS_VAL_TRANSPARENT, 0x00000000 },
    { CSS_VAL_GREY, 0xff808080 },
    { 0, 0 }
};

struct uiColors
{
    int css_value;
    const char *configGroup;
    const char *configEntry;
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
    // Dark shadow for three-dimensional display elements(for edges facing away from the light source).
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
    // Light color for three-dimensional display elements(for edges facing the light source).
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

QColor CSSStyleSelector::colorForCSSValue(int css_value)
{
    // try the regular ones first
    const colorMap *col = cmap;
    while(col->css_value && col->css_value != css_value)
    ++col;
    if(col->css_value)
    return col->color;

    const uiColors *uicol = uimap;
    while(uicol->css_value && uicol->css_value != css_value)
    ++uicol;
#ifndef APPLE_COMPILE_HACK
    if(!uicol->css_value) {
    if(css_value == CSS_VAL_INFOBACKGROUND)
        return QToolTip::palette().inactive().background();
    else if(css_value == CSS_VAL_INFOTEXT)
        return QToolTip::palette().inactive().foreground();
    else if(css_value == CSS_VAL_BACKGROUND) {
        KConfig bckgrConfig(QString::fromLatin1("kdesktoprc"), true, false); // No multi-screen support
        bckgrConfig.setGroup("Desktop0");
        // Desktop background.
        return bckgrConfig.readColorEntry("Color1", &qApp->palette().disabled().background());
    }
    return QColor();
    }

    const QPalette &pal = qApp->palette();
    QColor c = pal.color(uicol->group, uicol->role);
    if(uicol->configEntry) {
    KConfig *globalConfig = KGlobal::config();
    globalConfig->setGroup(uicol->configGroup);
    c = globalConfig->readColorEntry(uicol->configEntry, &c);
    }

    return c;
#else
    return QColor();
#endif
}

void CSSStyleSelector::applyRule(int id, CSSValueImpl *value)
{
//    kdDebug() << "[CSSStyleSelector::applyRule] Applying property " << id << " value \"" << value << "\"" << endl;

    CSSPrimitiveValueImpl *primitiveValue = 0;
    if(value->isPrimitiveValue())
        primitiveValue = static_cast<CSSPrimitiveValueImpl *>(value);

    Length l;
    bool apply = false;

    bool isInherit = (parentNode && value->cssValueType() == CSS_INHERIT);
    bool isInitial = (value->cssValueType() == CSS_INITIAL) ||
                      (!parentNode && value->cssValueType() == CSS_INHERIT);

    // What follows is a list that maps the CSS properties into their
    // corresponding front-end RenderStyle values. Shorthands (e.g. border,
    // background) occur in this list as well and are only hit when mapping
    // "inherit" or "initial" into front-end values.
    switch(id)
    {
    // ident only properties
    case CSS_PROP_BACKGROUND_ATTACHMENT:
    {
        HANDLE_INHERIT_AND_INITIAL(backgroundAttachment, BackgroundAttachment)
        if(!primitiveValue)
            break;

        switch(primitiveValue->getIdent())
        {
            case CSS_VAL_FIXED:
            {
                style->setBackgroundAttachment(false);
                
                // only use slow repaints if we actually have a background pixmap

                /* FIXME move to khtml2
                if(style->backgroundImage())
                    view->useSlowRepaints();
                */
                
                break;
            }
            case CSS_VAL_SCROLL:
            {
                style->setBackgroundAttachment(true);
                break;
            }
            default:
                return;
        }
    }
    case CSS_PROP_BACKGROUND_REPEAT:
    {
        HANDLE_INHERIT_AND_INITIAL(backgroundRepeat, BackgroundRepeat)
        if(!primitiveValue)
            return;

        switch(primitiveValue->getIdent())
        {
            case CSS_VAL_REPEAT:
            {
                style->setBackgroundRepeat(BR_REPEAT);
                break;
            }
            case CSS_VAL_REPEAT_X:
            {
                style->setBackgroundRepeat(BR_REPEAT_X);
                break;
            }
            case CSS_VAL_REPEAT_Y:
            {
                style->setBackgroundRepeat(BR_REPEAT_Y);
                break;
            }
            case CSS_VAL_NO_REPEAT:
            {
                style->setBackgroundRepeat(BR_NO_REPEAT);
                break;
            }
            default:
                return;
        }
    }

    case CSS_PROP_BORDER_COLLAPSE:
    {
        HANDLE_INHERIT_AND_INITIAL(borderCollapse, BorderCollapse)
        if(!primitiveValue)
            break;

        switch(primitiveValue->getIdent())
        {
            case CSS_VAL_COLLAPSE:
            {
                style->setBorderCollapse(true);
                break;
            }
            case CSS_VAL_SEPARATE:
            {
                style->setBorderCollapse(false);
                break;
            }
            default:
                return;
        }
    }
    case CSS_PROP_BORDER_TOP_STYLE:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderTopStyle, BorderTopStyle, BorderStyle)
        if(!primitiveValue)
            return;

        style->setBorderTopStyle((EBorderStyle) (primitiveValue->getIdent() - CSS_VAL__KHTML_NATIVE));
        break;
    }

    case CSS_PROP_BORDER_RIGHT_STYLE:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderRightStyle, BorderRightStyle, BorderStyle)
        if(!primitiveValue)
            return;

        style->setBorderRightStyle((EBorderStyle) (primitiveValue->getIdent() - CSS_VAL__KHTML_NATIVE));
        break;
    }
    case CSS_PROP_BORDER_BOTTOM_STYLE:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderBottomStyle, BorderBottomStyle, BorderStyle)
        if(!primitiveValue)
            return;

        style->setBorderBottomStyle((EBorderStyle) (primitiveValue->getIdent() - CSS_VAL__KHTML_NATIVE));
        break;
    }
    case CSS_PROP_BORDER_LEFT_STYLE:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderLeftStyle, BorderLeftStyle, BorderStyle)
        if(!primitiveValue)
            return;
        
        style->setBorderLeftStyle((EBorderStyle) (primitiveValue->getIdent() - CSS_VAL__KHTML_NATIVE));
        break;
    }
    case CSS_PROP_OUTLINE_STYLE:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(outlineStyle, OutlineStyle, BorderStyle)
        if(!primitiveValue)
            return;

        style->setOutlineStyle((EBorderStyle) (primitiveValue->getIdent() - CSS_VAL__KHTML_NATIVE));
        break;
    }
    case CSS_PROP_CAPTION_SIDE:
    {
        HANDLE_INHERIT_AND_INITIAL(captionSide, CaptionSide)
        if(!primitiveValue)
            break;

        ECaptionSide c = RenderStyle::initialCaptionSide();
        switch(primitiveValue->getIdent())
        {
            case CSS_VAL_LEFT:
            {
                c = CS_LEFT;
                break;
            }
            case CSS_VAL_RIGHT:
            {
                c = CS_RIGHT;
                break;
            }
            case CSS_VAL_TOP:
            {
                c = CS_TOP;
                break;
            }
            case CSS_VAL_BOTTOM:
            {
                c = CS_BOTTOM;
                break;
            }
            default:
                return;
        }

        style->setCaptionSide(c);
        return;
    }
    case CSS_PROP_CLEAR:
    {
        HANDLE_INHERIT_AND_INITIAL(clear, Clear)
        if(!primitiveValue)
            break;

        EClear c = CL_NONE;
        switch(primitiveValue->getIdent())
        {
            case CSS_VAL_LEFT:
            {
                c = CL_LEFT;
                break;
            }
            case CSS_VAL_RIGHT:
            {
                c = CL_RIGHT;
                break;
            }
            case CSS_VAL_BOTH:
            {
                c = CL_BOTH;
                break;
            }
            case CSS_VAL_NONE:
            {
                c = CL_NONE;
                break;
            }
            default:
                return;
        }
        
        style->setClear(c);
        return;
    }
    case CSS_PROP_DIRECTION:
    {
        HANDLE_INHERIT_AND_INITIAL(direction, Direction)
        if(!primitiveValue)
            break;

        style->setDirection((EDirection)(primitiveValue->getIdent() - CSS_VAL_LTR));
        return;
    }
    case CSS_PROP_DISPLAY:
    {
        HANDLE_INHERIT_AND_INITIAL(display, Display)
        if(!primitiveValue)
            break;

        int id = primitiveValue->getIdent();
        style->setDisplay(id == CSS_VAL_NONE ? DS_NONE : EDisplay(id - CSS_VAL_INLINE));
        break;
    }
    case CSS_PROP_EMPTY_CELLS:
    {
        HANDLE_INHERIT(emptyCells, EmptyCells);
        if(!primitiveValue)
            break;

        int id = primitiveValue->getIdent();
        if(id == CSS_VAL_SHOW)
            style->setEmptyCells(EC_SHOW);
        else if(id == CSS_VAL_HIDE)
            style->setEmptyCells(EC_HIDE);

        break;
    }
    case CSS_PROP_FLOAT:
    {
        HANDLE_INHERIT_AND_INITIAL(floating, Floating)
        if(!primitiveValue)
            return;

        EFloat f;
        switch(primitiveValue->getIdent())
        {
            case CSS_VAL_LEFT:
            {
                f = FL_LEFT;
                break;
            }                
            case CSS_VAL_RIGHT:
            {
                f = FL_RIGHT;
                break;
            }
            case CSS_VAL_NONE:
            case CSS_VAL_CENTER:  //Non standart CSS-Value
            {
                f = FL_NONE;
                break;
            }
            default:
                return;
        }

        if(f != FL_NONE && style->display() == DS_LIST_ITEM)
            style->setDisplay(DS_BLOCK);

        style->setFloating(f);
        break;
    }
    case CSS_PROP_FONT_STYLE:
    {
        FontDef fontDef = style->htmlFont().fontDef();
        if(isInherit)
            fontDef.italic = parentStyle->htmlFont().fontDef().italic;
        else if(isInitial)
            fontDef.italic = false;
        else
        {
            if(!primitiveValue)
                return;

            switch(primitiveValue->getIdent())
            {
                case CSS_VAL_OBLIQUE: // ### oblique is the same as italic for the moment...
                case CSS_VAL_ITALIC:
                {
                    fontDef.italic = true;
                    break;
                }
                case CSS_VAL_NORMAL:
                {
                    fontDef.italic = false;
                    break;
                }
                default:
                    return;
            }
        }

        fontDirty |= style->setFontDef(fontDef);
        break;
    }
    case CSS_PROP_FONT_VARIANT:
    {
        FontDef fontDef = style->htmlFont().fontDef();
        if(isInherit)
            fontDef.smallCaps = parentStyle->htmlFont().fontDef().weight;
        else if(isInitial)
            fontDef.smallCaps = false;
        else
        {
            if(!primitiveValue)
                return;

            int id = primitiveValue->getIdent();
            if(id == CSS_VAL_NORMAL)
                fontDef.smallCaps = false;
            else if(id == CSS_VAL_SMALL_CAPS)
                fontDef.smallCaps = true;
            else
                return;
        }

        fontDirty |= style->setFontDef(fontDef);
        break;
    }
    case CSS_PROP_FONT_WEIGHT:
    {
        FontDef fontDef = style->htmlFont().fontDef();

        if(isInherit)
            fontDef.weight = parentStyle->htmlFont().fontDef().weight;
        else if(isInitial)
            fontDef.weight = QFont::Normal;
        else
        {
            if(!primitiveValue)
                return;

            if(primitiveValue->getIdent())
            {
                switch(primitiveValue->getIdent())
                {
                    // ### we just support normal and bold fonts at the moment...
                    // setWeight can actually accept values between 0 and 99...
                    case CSS_VAL_BOLD:
                    case CSS_VAL_BOLDER:
                    case CSS_VAL_600:
                    case CSS_VAL_700:
                    case CSS_VAL_800:
                    case CSS_VAL_900:
                    {
                        fontDef.weight = QFont::Bold;
                        break;
                    }
                    case CSS_VAL_NORMAL:
                    case CSS_VAL_LIGHTER:
                    case CSS_VAL_100:
                    case CSS_VAL_200:
                    case CSS_VAL_300:
                    case CSS_VAL_400:
                    case CSS_VAL_500:
                    {
                        fontDef.weight = QFont::Normal;
                        break;
                    }
                    default:
                        return;
                }
            }
            else
            {
                // ### fix parsing of 100-900 values in parser, apply them here
            }
        }

        fontDirty |= style->setFontDef(fontDef);
        break;
    }
    case CSS_PROP_LIST_STYLE_POSITION:
    {
        HANDLE_INHERIT_AND_INITIAL(listStylePosition, ListStylePosition)
        if(!primitiveValue)
            return;

        if(primitiveValue->getIdent())
            style->setListStylePosition((EListStylePosition) (primitiveValue->getIdent() - CSS_VAL_OUTSIDE));

        return;
    }
    case CSS_PROP_LIST_STYLE_TYPE:
    {
        HANDLE_INHERIT_AND_INITIAL(listStyleType, ListStyleType)
        if(!primitiveValue)
            return;

        if(primitiveValue->getIdent())
        {
            EListStyleType t;
            int id = primitiveValue->getIdent();
            if(id == CSS_VAL_NONE)  // important!!
                t = LT_NONE;
            else
                t = EListStyleType(id - CSS_VAL_DISC);

            style->setListStyleType(t);
        }

        return;
    }
    case CSS_PROP_OVERFLOW:
    {
        HANDLE_INHERIT_AND_INITIAL(overflow, Overflow)
        if(!primitiveValue)
            return;

        EOverflow o;
        switch(primitiveValue->getIdent())
        {
            case CSS_VAL_VISIBLE:
            {
                o = OF_VISIBLE;
                break;
            }                
            case CSS_VAL_HIDDEN:
            {
                o = OF_HIDDEN;
                break;
            }
            case CSS_VAL_SCROLL:
            {
                o = OF_SCROLL;
                break;
            }
            case CSS_VAL_AUTO:
            {
                o = OF_AUTO;
                break;
            }
            default:
                return;
        }

        style->setOverflow(o);
        return;
    }
    case CSS_PROP_PAGE_BREAK_BEFORE:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakBefore, PageBreakBefore, PageBreakBefore)
        if(!primitiveValue)
            return;

        switch(primitiveValue->getIdent())
        {
            case CSS_VAL_AUTO:
            {
                style->setPageBreakBefore(PB_AUTO);
                break;
            }
            case CSS_VAL_LEFT:
            case CSS_VAL_RIGHT:
            case CSS_VAL_ALWAYS:
            {
                style->setPageBreakBefore(PB_ALWAYS); // CSS2.1: "Conforming user agents may map left/right to always."
                break;
            }
            case CSS_VAL_AVOID:
            {
                style->setPageBreakBefore(PB_AVOID);
                break;
            }
        }

        break;
    }
    case CSS_PROP_PAGE_BREAK_AFTER:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakAfter, PageBreakAfter, PageBreakAfter)
        if(!primitiveValue)
            return;

        switch(primitiveValue->getIdent())
        {
            case CSS_VAL_AUTO:
            {
                style->setPageBreakAfter(PB_AUTO);
                break;
            }
            case CSS_VAL_LEFT:
            case CSS_VAL_RIGHT:
            case CSS_VAL_ALWAYS:
            {
                style->setPageBreakAfter(PB_ALWAYS); // CSS2.1: "Conforming user agents may map left/right to always."
                break;
            }
            case CSS_VAL_AVOID:
            {
                style->setPageBreakAfter(PB_AVOID);
                break;
            }
        }

        break;
    }
    case CSS_PROP_PAGE_BREAK_INSIDE:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakInside, PageBreakInside, PageBreakInside)
        if(!primitiveValue)
            return;

        if(primitiveValue->getIdent() == CSS_VAL_AUTO)
            style->setPageBreakInside(PB_AUTO);
        else if(primitiveValue->getIdent() == CSS_VAL_AVOID)
            style->setPageBreakInside(PB_AVOID);

        return;
    }
    case CSS_PROP_POSITION:
    {
        HANDLE_INHERIT_AND_INITIAL(position, Position)
        if(!primitiveValue)
            return;

        EPosition p;
        switch(primitiveValue->getIdent())
        {
            case CSS_VAL_STATIC:
            {
                p = PS_STATIC;
                break;
            }                
            case CSS_VAL_RELATIVE:
            {
                p = PS_RELATIVE;
                break;
            }
            case CSS_VAL_ABSOLUTE:
            {
                p = PS_ABSOLUTE;
                break;
            }
            case CSS_VAL_FIXED:
            {
                /* FIXME move to khtml2
                view->useSlowRepaints();
                */
                p = PS_FIXED;
                break;
            }
            default:
                return;
        }
        
        style->setPosition(p);
        return;
    }
    case CSS_PROP_TABLE_LAYOUT:
    {
        HANDLE_INHERIT_AND_INITIAL(tableLayout, TableLayout)

        if(!primitiveValue)
            return;

        ETableLayout l = RenderStyle::initialTableLayout();
        switch(primitiveValue->getIdent())
        {
            case CSS_VAL_FIXED:
            {
                l = TL_FIXED;
                // fall through
            }
            case CSS_VAL_AUTO:
                style->setTableLayout(l);
            default:
                break;
        }

        break;
    }
    case CSS_PROP_UNICODE_BIDI:
    {
        HANDLE_INHERIT_AND_INITIAL(unicodeBidi, UnicodeBidi)
        if(!primitiveValue)
            break;

        switch(primitiveValue->getIdent())
        {
            case CSS_VAL_NORMAL:
            {
                style->setUnicodeBidi(UB_NORMAL);
                break;
            }
            case CSS_VAL_EMBED:
            {
                style->setUnicodeBidi(UB_EMBED);
                break;
            }
            case CSS_VAL_BIDI_OVERRIDE:
            {
                style->setUnicodeBidi(UB_OVERRIDE);
                break;
            }
            default:
                return;
        }

        break;
    }
    case CSS_PROP_TEXT_TRANSFORM:
    {
        HANDLE_INHERIT_AND_INITIAL(textTransform, TextTransform)

        if(!primitiveValue)
            break;

        if(!primitiveValue->getIdent())
            return;

        ETextTransform tt;
        switch(primitiveValue->getIdent())
        {
            case CSS_VAL_CAPITALIZE:
            {
                tt = TT_CAPITALIZE;
                break;
            }
            case CSS_VAL_UPPERCASE:
            {
                tt = TT_UPPERCASE;
                break;
            }
            case CSS_VAL_LOWERCASE:
            {
                tt = TT_LOWERCASE;
                break;
            }
            case CSS_VAL_NONE:
            default:
            {
                tt = TT_NONE;
                break;
            }
        }

        style->setTextTransform(tt);
        break;
    }
    case CSS_PROP_VISIBILITY:
    {
        HANDLE_INHERIT_AND_INITIAL(visibility, Visibility)

        if(!primitiveValue)
            break;

        switch(primitiveValue->getIdent())
        {
            case CSS_VAL_HIDDEN:
            {
                style->setVisibility(VS_HIDDEN);
                break;
            }
            case CSS_VAL_VISIBLE:
            {
                style->setVisibility(VS_VISIBLE);
                break;
            }
            case CSS_VAL_COLLAPSE:
                style->setVisibility(VS_COLLAPSE);
            default:
                break;
        }

        break;
    }

    case CSS_PROP_WHITE_SPACE:
    {
        HANDLE_INHERIT_AND_INITIAL(whiteSpace, WhiteSpace)

        if(!primitiveValue)
            break;

        if(!primitiveValue->getIdent())
            return;

        EWhiteSpace s;
        switch(primitiveValue->getIdent())
        {
            case CSS_VAL__KHTML_NOWRAP:
            {
                s = WS_KHTML_NOWRAP;
                break;
            }
            case CSS_VAL_NOWRAP:
            {
                s = WS_NOWRAP;
                break;
            }
            case CSS_VAL_PRE:
            {
                s = WS_PRE;
                break;
            }
            case CSS_VAL_PRE_WRAP:
            {
                s = WS_PRE_WRAP;
                break;
            }
            case CSS_VAL_PRE_LINE:
            {
                s = WS_PRE_LINE;
                break;
            }
            case CSS_VAL_NORMAL:
            default:
            {
                s = WS_NORMAL;
                break;
            }
        }

        style->setWhiteSpace(s);
        break;
    }
    case CSS_PROP_BACKGROUND_POSITION:
    {
        if(isInherit)
        {
            style->setBackgroundXPosition(parentStyle->backgroundXPosition());
            style->setBackgroundYPosition(parentStyle->backgroundYPosition());
        }
        else if(isInitial)
        {
            style->setBackgroundXPosition(RenderStyle::initialBackgroundXPosition());
            style->setBackgroundYPosition(RenderStyle::initialBackgroundYPosition());
        }

        break;
    }
    case CSS_PROP_BACKGROUND_POSITION_X:
    {
        HANDLE_INHERIT_AND_INITIAL(backgroundXPosition, BackgroundXPosition)
        if(!primitiveValue)
            break;

        Length l;
        int type = primitiveValue->primitiveType();
        if(type > CSS_PERCENTAGE && type < CSS_DEG)
            l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), LT_FIXED);
        else if(type == CSS_PERCENTAGE)
            l = Length((int) primitiveValue->getFloatValue(CSS_PERCENTAGE), LT_PERCENT);
        else
            return;

        style->setBackgroundXPosition(l);
        break;
    }
    case CSS_PROP_BACKGROUND_POSITION_Y:
    {
        HANDLE_INHERIT_AND_INITIAL(backgroundYPosition, BackgroundYPosition)
        if(!primitiveValue)
            break;

        Length l;
        int type = primitiveValue->primitiveType();
        if(type > CSS_PERCENTAGE && type < CSS_DEG)
            l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), LT_FIXED);
        else if(type == CSS_PERCENTAGE)
            l = Length((int) primitiveValue->getFloatValue(CSS_PERCENTAGE), LT_PERCENT);
        else
            return;

        style->setBackgroundYPosition(l);
        break;
    }
    case CSS_PROP_BORDER_SPACING:
    {
        if(value->cssValueType() != CSS_INHERIT || !parentNode)
            return;

        style->setBorderHSpacing(parentStyle->borderHSpacing());
        style->setBorderVSpacing(parentStyle->borderVSpacing());
        break;
    }
    case CSS_PROP__KHTML_BORDER_HORIZONTAL_SPACING:
    {
        HANDLE_INHERIT_AND_INITIAL(borderHSpacing, BorderHSpacing)
        if(!primitiveValue)
            break;

        short spacing =  primitiveValue->computeLength(style, paintDeviceMetrics);
        style->setBorderHSpacing(spacing);
        break;
    }
    case CSS_PROP__KHTML_BORDER_VERTICAL_SPACING:
    {
        HANDLE_INHERIT_AND_INITIAL(borderVSpacing, BorderVSpacing)
        if(!primitiveValue)
            break;

        short spacing =  primitiveValue->computeLength(style, paintDeviceMetrics);
        style->setBorderVSpacing(spacing);
        break;
    }
    case CSS_PROP_CURSOR:
    {
        HANDLE_INHERIT_AND_INITIAL(cursor, Cursor)
        if(primitiveValue)
            style->setCursor((ECursor) (primitiveValue->getIdent() - CSS_VAL_AUTO));

        break;
    }
    // colors || inherit
    case CSS_PROP_BACKGROUND_COLOR:
    case CSS_PROP_BORDER_TOP_COLOR:
    case CSS_PROP_BORDER_RIGHT_COLOR:
    case CSS_PROP_BORDER_BOTTOM_COLOR:
    case CSS_PROP_BORDER_LEFT_COLOR:
    case CSS_PROP_COLOR:
    case CSS_PROP_OUTLINE_COLOR:
    {
        QColor col;
        if(isInherit)
        {
            HANDLE_INHERIT_COND(CSS_PROP_BACKGROUND_COLOR, backgroundColor, BackgroundColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_TOP_COLOR, borderTopColor, BorderTopColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_BOTTOM_COLOR, borderBottomColor, BorderBottomColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_RIGHT_COLOR, borderRightColor, BorderRightColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_LEFT_COLOR, borderLeftColor, BorderLeftColor)
            HANDLE_INHERIT_COND(CSS_PROP_COLOR, color, Color)
            HANDLE_INHERIT_COND(CSS_PROP_OUTLINE_COLOR, outlineColor, OutlineColor)
            return;
        }
        else if(isInitial)
        {
            // The border/outline colors will just map to the invalid color |col| above.  This will have the
            // effect of forcing the use of the currentColor when it comes time to draw the borders (and of
            // not painting the background since the color won't be valid).
            if(id == CSS_PROP_COLOR)
                col = RenderStyle::initialColor();
        }
        else
        {
            if(!primitiveValue)
                return;

            int ident = primitiveValue->getIdent();
            if(ident)
            {
                if(ident == CSS_VAL__KHTML_TEXT)
                    col = QColor(); // FIXME element->getDocument()->textColor();
                else if(ident == CSS_VAL_TRANSPARENT && id != CSS_PROP_BORDER_TOP_COLOR    &&
                        id != CSS_PROP_BORDER_RIGHT_COLOR && id != CSS_PROP_BORDER_BOTTOM_COLOR &&
                        id != CSS_PROP_BORDER_LEFT_COLOR)
                {
                    col = QColor();
                }
                else
                    col = colorForCSSValue(ident);
            }
            else if(primitiveValue->primitiveType() == CSS_RGBCOLOR)
            {
                if(qAlpha(primitiveValue->getQRGBColorValue()))
                    col.setRgb(primitiveValue->getQRGBColorValue());
            }
            else
                return;
        }
    
        // kdDebug( 6080 ) << "applying color " << col.isValid() << endl;
        switch(id)
        {
            case CSS_PROP_BACKGROUND_COLOR:
            {
                style->setBackgroundColor(col);
                break;
            }
            case CSS_PROP_BORDER_TOP_COLOR:
            {
                style->setBorderTopColor(col);
                break;
            }
            case CSS_PROP_BORDER_RIGHT_COLOR:
            {
                style->setBorderRightColor(col);
                break;
            }
            case CSS_PROP_BORDER_BOTTOM_COLOR:
            {
                style->setBorderBottomColor(col);
                break;
            }
            case CSS_PROP_BORDER_LEFT_COLOR:
            {
                style->setBorderLeftColor(col);
                break;
            }
            case CSS_PROP_COLOR:
            {
                style->setColor(col);
                break;
            }
            case CSS_PROP_OUTLINE_COLOR:
            {
                style->setOutlineColor(col);
                break;
            }
            default:
                return;
        }
    
        return;
    }

    // uri || inherit
    case CSS_PROP_BACKGROUND_IMAGE:
    {
        HANDLE_INHERIT_AND_INITIAL(backgroundImage, BackgroundImage)
        if(!primitiveValue)
            return;

        style->setBackgroundImage(static_cast<CSSImageValueImpl *>(primitiveValue)->image());

        // kdDebug(6080) << "setting image in style to " << image << endl;
        break;
    }
    case CSS_PROP_LIST_STYLE_IMAGE:
    {
        HANDLE_INHERIT_AND_INITIAL(listStyleImage, ListStyleImage)
        if(!primitiveValue)
            return;

        style->setListStyleImage(static_cast<CSSImageValueImpl *>(primitiveValue)->image());

        // kdDebug(6080) << "setting image in list to " << image->image() << endl;
        break;
    }

    // length
    case CSS_PROP_BORDER_TOP_WIDTH:
    case CSS_PROP_BORDER_RIGHT_WIDTH:
    case CSS_PROP_BORDER_BOTTOM_WIDTH:
    case CSS_PROP_BORDER_LEFT_WIDTH:
    case CSS_PROP_OUTLINE_WIDTH:
    {
        if(isInherit)
        {
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_TOP_WIDTH, borderTopWidth, BorderTopWidth)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_RIGHT_WIDTH, borderRightWidth, BorderRightWidth)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_BOTTOM_WIDTH, borderBottomWidth, BorderBottomWidth)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_LEFT_WIDTH, borderLeftWidth, BorderLeftWidth)
            HANDLE_INHERIT_COND(CSS_PROP_OUTLINE_WIDTH, outlineWidth, OutlineWidth)
            return;
        }
        else if(isInitial)
        {
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_TOP_WIDTH, BorderTopWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_RIGHT_WIDTH, BorderRightWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_BOTTOM_WIDTH, BorderBottomWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_LEFT_WIDTH, BorderLeftWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_OUTLINE_WIDTH, OutlineWidth, BorderWidth)
            return;
        }

        if(!primitiveValue)
            break;

        short width = 3;
        switch(primitiveValue->getIdent())
        {
            case CSS_VAL_THIN:
            {
                width = 1;
                break;
            }
            case CSS_VAL_MEDIUM:
            {
                width = 3;
                break;
            }
            case CSS_VAL_THICK:
            {
                width = 5;
                break;
            }
            case CSS_VAL_INVALID:
            {
                double widthd = primitiveValue->computeLengthFloat(style, paintDeviceMetrics);
                width = (int) widthd;

                // somewhat resemble Mozilla's granularity
                // this makes border-width: 0.5pt borders visible
                if(width == 0 && widthd >= 0.025)
                    width++;

                break;
            }
            default:
                return;
        }

        if(width < 0)
            return;

        switch(id)
        {
            case CSS_PROP_BORDER_TOP_WIDTH:
            {
                style->setBorderTopWidth(width);
                break;
            }
            case CSS_PROP_BORDER_RIGHT_WIDTH:
            {
                style->setBorderRightWidth(width);
                break;
            }
            case CSS_PROP_BORDER_BOTTOM_WIDTH:
            {
                style->setBorderBottomWidth(width);
                break;
            }
            case CSS_PROP_BORDER_LEFT_WIDTH:
            {
                style->setBorderLeftWidth(width);
                break;
            }
            case CSS_PROP_OUTLINE_WIDTH:
            {
                style->setOutlineWidth(width);
                break;
            }
            default:
                return;
        }
        
        return;
    }

    case CSS_PROP_LETTER_SPACING:
    case CSS_PROP_WORD_SPACING:
    {
        if(isInherit)
        {
            HANDLE_INHERIT_COND(CSS_PROP_LETTER_SPACING, letterSpacing, LetterSpacing)
            HANDLE_INHERIT_COND(CSS_PROP_WORD_SPACING, wordSpacing, WordSpacing)
            return;
        }
        else if (isInitial)
        {
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_LETTER_SPACING, LetterSpacing, LetterSpacing)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_WORD_SPACING, WordSpacing, WordSpacing)
            return;
        }

        if(!primitiveValue)
            return;

        int width = 0;
        if(primitiveValue->getIdent() != CSS_VAL_NORMAL)
            width = primitiveValue->computeLength(style, paintDeviceMetrics);

        switch(id)
        {
            case CSS_PROP_LETTER_SPACING:
            {
                style->setLetterSpacing(width);
                break;
            }
            case CSS_PROP_WORD_SPACING:
            {
                style->setWordSpacing(width);
                break;
            }
            // ### needs the definitions in renderstyle
            default:
                break;
        }
        
        return;
    }

    // length, percent
    case CSS_PROP_MAX_WIDTH:
    {
        // +none +inherit
        if(primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE)
            apply = true;

        /* nobreak */
    }
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
    {
        // +inherit +auto
        if(id != CSS_PROP_MAX_WIDTH && primitiveValue && primitiveValue->getIdent() == CSS_VAL_AUTO)
        {
            // kdDebug(6080) << "found value=auto" << endl;
            apply = true;
        }

        /* nobreak */
    }
    case CSS_PROP_PADDING_TOP:
    case CSS_PROP_PADDING_RIGHT:
    case CSS_PROP_PADDING_BOTTOM:
    case CSS_PROP_PADDING_LEFT:
    case CSS_PROP_TEXT_INDENT:
    {
        // +inherit
        if(isInherit)
        {
            HANDLE_INHERIT_COND(CSS_PROP_MAX_WIDTH, maxWidth, MaxWidth)
            HANDLE_INHERIT_COND(CSS_PROP_BOTTOM, bottom, Bottom)
            HANDLE_INHERIT_COND(CSS_PROP_TOP, top, Top)
            HANDLE_INHERIT_COND(CSS_PROP_LEFT, left, Left)
            HANDLE_INHERIT_COND(CSS_PROP_RIGHT, right, Right)
            HANDLE_INHERIT_COND(CSS_PROP_WIDTH, width, Width)
            HANDLE_INHERIT_COND(CSS_PROP_MIN_WIDTH, minWidth, MinWidth)
            HANDLE_INHERIT_COND(CSS_PROP_PADDING_TOP, paddingTop, PaddingTop)
            HANDLE_INHERIT_COND(CSS_PROP_PADDING_RIGHT, paddingRight, PaddingRight)
            HANDLE_INHERIT_COND(CSS_PROP_PADDING_BOTTOM, paddingBottom, PaddingBottom)
            HANDLE_INHERIT_COND(CSS_PROP_PADDING_LEFT, paddingLeft, PaddingLeft)
            HANDLE_INHERIT_COND(CSS_PROP_MARGIN_TOP, marginTop, MarginTop)
            HANDLE_INHERIT_COND(CSS_PROP_MARGIN_RIGHT, marginRight, MarginRight)
            HANDLE_INHERIT_COND(CSS_PROP_MARGIN_BOTTOM, marginBottom, MarginBottom)
            HANDLE_INHERIT_COND(CSS_PROP_MARGIN_LEFT, marginLeft, MarginLeft)
            HANDLE_INHERIT_COND(CSS_PROP_TEXT_INDENT, textIndent, TextIndent)
            return;
        }
        else if(isInitial)
        {
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MAX_WIDTH, MaxWidth, MaxSize)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BOTTOM, Bottom, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_TOP, Top, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_LEFT, Left, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_RIGHT, Right, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_WIDTH, Width, Size)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MIN_WIDTH, MinWidth, MinSize)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_PADDING_TOP, PaddingTop, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_PADDING_RIGHT, PaddingRight, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_PADDING_BOTTOM, PaddingBottom, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_PADDING_LEFT, PaddingLeft, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MARGIN_TOP, MarginTop, Margin)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MARGIN_RIGHT, MarginRight, Margin)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MARGIN_BOTTOM, MarginBottom, Margin)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MARGIN_LEFT, MarginLeft, Margin)
            HANDLE_INITIAL_COND(CSS_PROP_TEXT_INDENT, TextIndent)
            return;
        }

        if(primitiveValue && !apply)
        {
            int type = primitiveValue->primitiveType();
            if(type > CSS_PERCENTAGE && type < CSS_DEG)
            {
                // Handle our quirky margin units if we have them.
                l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), LT_FIXED, primitiveValue->isQuirkValue());
            }
            else if(type == CSS_PERCENTAGE)
                l = Length((int) primitiveValue->floatValue(CSS_PERCENTAGE), LT_PERCENT);
            else if(type == CSS_HTML_RELATIVE)
                l = Length((int) primitiveValue->floatValue(CSS_HTML_RELATIVE), LT_RELATIVE);
            else
                return;

            apply = true;
        }

        if(!apply)
            return;

        switch(id)
        {
            case CSS_PROP_MAX_WIDTH:
            {
                style->setMaxWidth(l); 
                break;
            }
            case CSS_PROP_BOTTOM:
            {
                style->setBottom(l); 
                break;
            }
            case CSS_PROP_TOP:
            {
                style->setTop(l); 
                break;
            }
            case CSS_PROP_LEFT:
            {
                style->setLeft(l); 
                break;
            }
            case CSS_PROP_RIGHT:
            {
                style->setRight(l); 
                break;
            }
            case CSS_PROP_WIDTH:
            {
                style->setWidth(l); 
                break;
            }
            case CSS_PROP_MIN_WIDTH:
            {
                style->setMinWidth(l); 
                break;
            }
            case CSS_PROP_PADDING_TOP:
            {
                style->setPaddingTop(l); 
                break;
            }
            case CSS_PROP_PADDING_RIGHT:
            {
                style->setPaddingRight(l); 
                break;
            }
            case CSS_PROP_PADDING_BOTTOM:
            {
                style->setPaddingBottom(l); 
                break;
            }
            case CSS_PROP_PADDING_LEFT:
            {
                style->setPaddingLeft(l); 
                break;
            }
            case CSS_PROP_MARGIN_TOP:
            {
                style->setMarginTop(l); 
                break;
            }
            case CSS_PROP_MARGIN_RIGHT:
            {
                style->setMarginRight(l); 
                break;
            }
            case CSS_PROP_MARGIN_BOTTOM:
            {
                style->setMarginBottom(l); 
                break;
            }
            case CSS_PROP_MARGIN_LEFT:
            {
                style->setMarginLeft(l); 
                break;
            }
            case CSS_PROP_TEXT_INDENT:
            {
                style->setTextIndent(l); 
                break;
            }
            default: 
                break;
        }

        return;
    }
    case CSS_PROP_MAX_HEIGHT:
    {
        if(primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE)
            apply = true;

        /* nobreak */
    }
    case CSS_PROP_HEIGHT:
    case CSS_PROP_MIN_HEIGHT:
    {
        if(id != CSS_PROP_MAX_HEIGHT && primitiveValue && primitiveValue->getIdent() == CSS_VAL_AUTO)
            apply = true;

        if(isInherit)
        {
            HANDLE_INHERIT_COND(CSS_PROP_MAX_HEIGHT, maxHeight, MaxHeight)
            HANDLE_INHERIT_COND(CSS_PROP_HEIGHT, height, Height)
            HANDLE_INHERIT_COND(CSS_PROP_MIN_HEIGHT, minHeight, MinHeight)
            return;
        }
        else if(isInitial)
        {
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MAX_HEIGHT, MaxHeight, MaxSize)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_HEIGHT, Height, Size)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MIN_HEIGHT, MinHeight, MinSize)
            return;
        }

        if(primitiveValue && !apply)
        {
            int type = primitiveValue->primitiveType();
            if(type > CSS_PERCENTAGE && type < CSS_DEG)
                l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), LT_FIXED);
            else if(type == CSS_PERCENTAGE)
                l = Length((int) primitiveValue->floatValue(CSS_PERCENTAGE), LT_PERCENT);
            else
                return;

            apply = true;
        }

        if(!apply)
            return;

        switch(id)
        {
            case CSS_PROP_MAX_HEIGHT:
            {
                style->setMaxHeight(l); 
                break;
            }
            case CSS_PROP_HEIGHT:
            {
                style->setHeight(l); 
                break;
            }
            case CSS_PROP_MIN_HEIGHT:
            {
                style->setMinHeight(l); 
                break;
            }
            default:
                return;
        }

        return;
    }
    case CSS_PROP_VERTICAL_ALIGN:
    {
        HANDLE_INHERIT_AND_INITIAL(verticalAlign, VerticalAlign)
        if(!primitiveValue)
            return;

        if(primitiveValue->getIdent())
        {
            EVerticalAlign align;

            switch(primitiveValue->getIdent())
            {
                case CSS_VAL_TOP:
                {
                    align = VA_TOP; 
                    break;
                }
                case CSS_VAL_BOTTOM:
                {
                    align = VA_BOTTOM; 
                    break;
                }
                case CSS_VAL_MIDDLE:
                {
                    align = VA_MIDDLE; 
                    break;
                }
                case CSS_VAL_BASELINE:
                {
                    align = VA_BASELINE; 
                    break;
                }
                case CSS_VAL_TEXT_BOTTOM:
                {    
                    align = VA_TEXT_BOTTOM; 
                    break;
                }
                case CSS_VAL_TEXT_TOP:
                {
                    align = VA_TEXT_TOP; 
                    break;
                }
                case CSS_VAL_SUB:
                {
                    align = VA_SUB;
                    break;
                }
                case CSS_VAL_SUPER:
                {
                    align = VA_SUPER;
                    break;
                }
                case CSS_VAL__KHTML_BASELINE_MIDDLE:
                {
                    align = VA_BASELINE_MIDDLE;
                    break;
                }
                default:
                    return;
            }

            style->setVerticalAlign(align);
            return;
        }
        else
        {
            int type = primitiveValue->primitiveType();
            
            Length l;
            if(type > CSS_PERCENTAGE && type < CSS_DEG)
                l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), LT_FIXED);
            else if(type == CSS_PERCENTAGE)
                l = Length((int) primitiveValue->floatValue(CSS_PERCENTAGE), LT_PERCENT);

            style->setVerticalAlign(VA_LENGTH);
            style->setVerticalAlignLength(l);
        }
        
        break;
    }
    case CSS_PROP_FONT_SIZE:
    {
#ifndef APPLE_COMPILE_HACK
        FontDef fontDef = style->htmlFont().fontDef();
        int oldSize;
        int size = 0;

        float toPix = paintDeviceMetrics->logicalDpiY() / 72.;
        if(toPix < 96. / 72.)
            toPix = 96. / 72.;

        int minFontSize = int(settings->minFontSize() * toPix);

        if(parentNode)
            oldSize = parentStyle->font().pixelSize();
        else
            oldSize = m_fontSizes[3];

        if(isInherit)
            size = oldSize;
        else if(isInitial)
            size = m_fontSizes[3];
        else if(primitiveValue->getIdent())
        {
            // keywords are being used.  Pick the correct default
            // based off the font family.
            const QValueVector<int> &fontSizes = m_fontSizes;
            switch(primitiveValue->getIdent())
            {
                case CSS_VAL_XX_SMALL:
                {
                    size = int(fontSizes[0]);
                    break;
                }
                case CSS_VAL_X_SMALL:
                {
                    size = int(fontSizes[1]);
                    break;
                }
                case CSS_VAL_SMALL:
                {
                    size = int(fontSizes[2]); 
                    break;
                }
                case CSS_VAL_MEDIUM:
                {
                    size = int(fontSizes[3]); 
                    break;
                }
                case CSS_VAL_LARGE:
                {
                    size = int(fontSizes[4]); 
                    break;
                }
                case CSS_VAL_X_LARGE:
                {
                    size = int(fontSizes[5]); 
                    break;
                }
                case CSS_VAL_XX_LARGE:
                {
                    size = int(fontSizes[6]); 
                    break;
                }
                case CSS_VAL__KHTML_XXX_LARGE:
                {
                    size = int(fontSizes[7]); 
                    break;
                }
                case CSS_VAL_LARGER:
                {
                    size = nextFontSize(fontSizes, oldSize, false);
                    break;
                }
                case CSS_VAL_SMALLER:
                {
                    size = nextFontSize(fontSizes, oldSize, true);
                    break;
                }
                default:
                    return;
            }
        }
        else
        {
            int type = primitiveValue->primitiveType();
            if(type > CSS_PERCENTAGE && type < CSS_DEG)
            {
                if(/* FIXME !khtml::printpainter && */ type != CSS_EMS && type != CSS_EXS &&
                    element /* FIXME && element->getDocument()->view() */)
                {
                    size = int(primitiveValue->computeLengthFloat(parentStyle, paintDeviceMetrics) *
                           /* element->getDocument()->view()->part()->zoomFactor()  */ 100) / 100;
                }
                else
                    size = int(primitiveValue->computeLengthFloat(parentStyle, paintDeviceMetrics));
            }
            else if(type == CSS_PERCENTAGE)
                size = int(primitiveValue->floatValue(CSS_PERCENTAGE) * parentStyle->font().pixelSize()) / 100;
            else
                return;
        }
        
        if(size < 1)
            return;

        // we never want to get smaller than the minimum font size to keep fonts readable
        if(size < minFontSize)
            size = minFontSize;

        kdDebug() << "computed raw font size: " << size << endl;

        fontDef.size = size;
        fontDirty |= style->setFontDef(fontDef);
#endif
        return;
    }
    case CSS_PROP_Z_INDEX:
    {
        HANDLE_INHERIT(zIndex, ZIndex)
        else if (isInitial)
        {
            style->setHasAutoZIndex();
            return;
        }

        if(!primitiveValue)
            return;

        if(primitiveValue->getIdent() == CSS_VAL_AUTO)
        {
            style->setHasAutoZIndex();
            return;
        }

        if(primitiveValue->primitiveType() != CSS_NUMBER)
            return; // Error case.

        style->setZIndex((int) primitiveValue->floatValue(CSS_NUMBER));
        return;
    }

    case CSS_PROP_WIDOWS:
    {
        HANDLE_INHERIT_AND_INITIAL(widows, Widows)
        if(!primitiveValue || primitiveValue->primitiveType() != CSS_NUMBER)
            return;

        style->setWidows((int) primitiveValue->floatValue(CSS_NUMBER));
        break;
    }
    case CSS_PROP_ORPHANS:
    {
        HANDLE_INHERIT_AND_INITIAL(orphans, Orphans)
        if(!primitiveValue || primitiveValue->primitiveType() != CSS_NUMBER)
            return;

        style->setOrphans((int) primitiveValue->floatValue(CSS_NUMBER));
        break;
    }

    // length, percent, number
    case CSS_PROP_LINE_HEIGHT:
    {
        HANDLE_INHERIT_AND_INITIAL(lineHeight, LineHeight)
        if(!primitiveValue)
            return;
        
        Length lineHeight;
        int type = primitiveValue->primitiveType();
        if(primitiveValue->getIdent() == CSS_VAL_NORMAL)
            lineHeight = Length(-100, LT_PERCENT);
        else if(type > CSS_PERCENTAGE && type < CSS_DEG)
            lineHeight = Length(primitiveValue->computeLength(style, paintDeviceMetrics), LT_FIXED);
        else if(type == CSS_PERCENTAGE)
            lineHeight = Length((style->font().pixelSize() * int(primitiveValue->floatValue(CSS_PERCENTAGE))) / 100, LT_FIXED);
        else if(type == CSS_NUMBER)
            lineHeight = Length(int(primitiveValue->floatValue(CSS_NUMBER) * 100), LT_PERCENT);
        else
            return;

        style->setLineHeight(lineHeight);
        return;
    }

    // string
    case CSS_PROP_TEXT_ALIGN:
    {
        HANDLE_INHERIT_AND_INITIAL(textAlign, TextAlign)
        if(!primitiveValue)
            return;

        if(primitiveValue->getIdent())
            style->setTextAlign((ETextAlign) (primitiveValue->getIdent() - CSS_VAL__KHTML_AUTO));

        return;
    }

    // rect
    case CSS_PROP_CLIP:
    {
        Length top;
        Length right;
        Length bottom;
        Length left;
        bool hasClip = true;
        if(isInherit)
        {
            if(parentStyle->hasClip())
            {
                top = parentStyle->clipTop();
                right = parentStyle->clipRight();
                bottom = parentStyle->clipBottom();
                left = parentStyle->clipLeft();
            }
            else
            {
                hasClip = false;
                top = right = bottom = left = Length();
            }
        }
        else if(isInitial)
        {
            hasClip = false;
            top = right = bottom = left = Length();
        }
        else if(!primitiveValue)
            break;
        else if(primitiveValue->primitiveType() == CSS_RECT)
        {
            RectImpl *rect = primitiveValue->getRectValue();
            if(!rect)
                break;

            top = convertToLength(rect->top(), style, paintDeviceMetrics);
            right = convertToLength(rect->right(), style, paintDeviceMetrics);
            bottom = convertToLength(rect->bottom(), style, paintDeviceMetrics);
            left = convertToLength(rect->left(), style, paintDeviceMetrics);
        }
        else if(primitiveValue->getIdent() != CSS_VAL_AUTO)
            break;

        //     qDebug("setting clip top to %d", top.value);
        //     qDebug("setting clip right to %d", right.value);
        //     qDebug("setting clip bottom to %d", bottom.value);
        //     qDebug("setting clip left to %d", left.value);
        style->setClip(top, right, bottom, left);
        style->setHasClip(hasClip);

        // rect, ident
        break;
    }

    // lists
    case CSS_PROP_CONTENT: // list of string, uri, counter, attr, i
    {
        // FIXME: In CSS3, it will be possible to inherit content.  In CSS2 it is not.
        // This note is a reminder that eventually "inherit" needs to be supported.
        if(!(style->styleType() == RenderStyle::BEFORE || style->styleType() == RenderStyle::AFTER))
            break;

        if(isInitial)
        {
            if(style->contentData())
                style->contentData()->clearContent();

            return;
        }

        if(!value->isValueList())
            return;

        CSSValueListImpl *list = static_cast<CSSValueListImpl *>(value);
        int len = list->length();

        for(int i = 0; i < len; i++)
        {
            CSSValueImpl *item = list->item(i);
            if(!item->isPrimitiveValue())
                continue;

            CSSPrimitiveValueImpl *val = static_cast<CSSPrimitiveValueImpl *>(item);
            if(val->primitiveType() == CSS_STRING)
                style->setContent(val->getDOMStringValue(), i != 0);
            else if(val->primitiveType() == CSS_ATTR)
            {
                int attrID = element->ownerDocument()->getId(NodeImpl::AttributeId, val->getDOMStringValue(), false);
                if(attrID)
                    style->setContent(DOMString(element->getAttribute(attrID)).handle(), i != 0);
            }
            else if(val->primitiveType() == CSS_URI)
            {
                CSSImageValueImpl *image = static_cast<CSSImageValueImpl *>(val);
                style->setContent(image->image(), i != 0);
            }
            else if(val->primitiveType() == CSS_COUNTER)
                style->setContent(val->getCounterValue(), i != 0);
            else if(val->primitiveType() == CSS_IDENT)
            {
                EQuoteContent quote = QC_NO_QUOTE;
                switch (val->getIdent())
                {
                    case CSS_VAL_OPEN_QUOTE:
                    {
                        quote = QC_OPEN_QUOTE;
                        break;
                    }
                    case CSS_VAL_NO_OPEN_QUOTE:
                    {
                        quote = QC_NO_OPEN_QUOTE;
                        break;
                    }
                    case CSS_VAL_CLOSE_QUOTE:
                    {
                        quote = QC_CLOSE_QUOTE;
                        break;
                    }
                    case CSS_VAL_NO_CLOSE_QUOTE:
                    {
                        quote = QC_NO_CLOSE_QUOTE;
                        break;
                    }
                    default:
                        assert(false);
                }

                style->setContent(quote, i != 0);
            }
        }

        break;
    }
    case CSS_PROP_COUNTER_INCREMENT:
    {
        if(!value->isValueList())
            return;

        CSSValueListImpl *list = static_cast<CSSValueListImpl *>(value);
        style->setCounterIncrement(list);
        break;
    }
    case CSS_PROP_COUNTER_RESET:
    {
        if(!value->isValueList())
            return;

        CSSValueListImpl *list = static_cast<CSSValueListImpl *>(value);
        style->setCounterReset(list);
        break;
    }
    case CSS_PROP_FONT_FAMILY: // list of strings and ids
    {
#ifndef APPLE_COMPILE_HACK
        if(isInherit)
        {
            FontDef parentFontDef = parentStyle->htmlFont().fontDef();
            FontDef fontDef = style->htmlFont().fontDef();
            fontDef.family = parentFontDef.family;

            if(style->setFontDef(fontDef))
                fontDirty = true;
            
            return;
        }
        else if(isInitial)
        {
            FontDef fontDef = style->htmlFont().fontDef();
            FontDef initialDef = FontDef();
            fontDef.family = QString::null;
            
            if(style->setFontDef(fontDef))
                fontDirty = true;

            return;
        }
        
        if(!value->isValueList())
            return;
        
        FontDef fontDef = style->htmlFont().fontDef();
        CSSValueListImpl *list = static_cast<CSSValueListImpl *>(value);

        int len = list->length();
        for(int i = 0; i < len; i++)
        {
            CSSValueImpl *item = list->item(i);
            if(!item->isPrimitiveValue())
                continue;

            CSSPrimitiveValueImpl *val = static_cast<CSSPrimitiveValueImpl *>(item);
            QString face;
            if(val->primitiveType() == CSS_STRING)
                face = static_cast<FontFamilyValueImpl *>(val)->fontName();
            else if(val->primitiveType() == CSS_IDENT)
            {
                switch(val->getIdent())
                {
                    case CSS_VAL_SERIF:
                    {
                        face = settings->serifFontName();
                        break;
                    }
                    case CSS_VAL_SANS_SERIF:
                    {
                        face = settings->sansSerifFontName();
                        break;
                    }
                    case CSS_VAL_CURSIVE:
                    {
                        face = settings->cursiveFontName();
                        break;
                    }
                    case CSS_VAL_FANTASY:
                    {
                        face = settings->fantasyFontName();
                        break;
                    }
                    case CSS_VAL_MONOSPACE:
                    {
                        face = settings->fixedFontName();
                        break;
                    }
                    default:
                        return;
                }
            }
            else
                return;
            
            if(!face.isEmpty())
            {
                fontDef.family = face;
                fontDirty |= style->setFontDef(fontDef);
                return;
            }
        }
#endif
        break;
    }
    case CSS_PROP_QUOTES:
    {
        HANDLE_INHERIT_AND_INITIAL(quotes, Quotes)
        if(primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE)
        {
            // set a set of empty quotes
            QuotesValueImpl *quotes = new QuotesValueImpl();
            style->setQuotes(quotes);
        }
        else
        {
            QuotesValueImpl *quotes = static_cast<QuotesValueImpl *>(value);
            style->setQuotes(quotes);
        }
        
        break;
    }
    case CSS_PROP_SIZE:
    {
        // ### look up
        break;
    }
    case CSS_PROP_TEXT_DECORATION:
    {
        // list of ident
        HANDLE_INHERIT_AND_INITIAL(textDecoration, TextDecoration)
        int t = RenderStyle::initialTextDecoration();
        if(!primitiveValue)
            return;

        if(primitiveValue->getIdent() != CSS_VAL_NONE)
        {
            if(!value->isValueList())
                return;

            CSSValueListImpl *list = static_cast<CSSValueListImpl *>(value);
            int len = list->length();
            for(int i = 0; i < len; i++)
            {
                CSSValueImpl *item = list->item(i);
                if(!item->isPrimitiveValue())
                    continue;

                primitiveValue = static_cast<CSSPrimitiveValueImpl *>(item);
                switch(primitiveValue->getIdent())
                {
                    case CSS_VAL_NONE:
                    {
                        t = TD_NONE; 
                        break;
                    }
                    case CSS_VAL_UNDERLINE:
                    {
                        t |= TD_UNDERLINE; 
                        break;
                    }
                    case CSS_VAL_OVERLINE:
                    {
                        t |= TD_OVERLINE; 
                        break;
                    }
                    case CSS_VAL_LINE_THROUGH:
                    {
                        t |= TD_LINE_THROUGH; 
                        break;
                    }
                    case CSS_VAL_BLINK:
                    {
                        t |= TD_BLINK; 
                        break;
                    }
                    default:
                        return;
                }
            }
        }

        style->setTextDecoration(ETextDecoration(t));
        break;
    }

    case CSS_PROP_TEXT_SHADOW:
    {
        if(isInherit)
        {
            style->setTextShadow(parentStyle->textShadow() ? new ShadowData(*parentStyle->textShadow()) : 0);
            return;
        }
        else if (isInitial)
        {
            style->setTextShadow(0);
            return;
        }

        if(primitiveValue) // none
        {
            style->setTextShadow(0);
            return;
        }

        if(!value->isValueList())
            return;

        CSSValueListImpl *list = static_cast<CSSValueListImpl *>(value);
        int len = list->length();
        for(int i = 0; i < len; i++)
        {
            ShadowValueImpl *item = static_cast<ShadowValueImpl *>(list->item(i));

            int x = item->x->computeLength(style, paintDeviceMetrics);
            int y = item->y->computeLength(style, paintDeviceMetrics);
            int blur = item->blur ? item->blur->computeLength(style, paintDeviceMetrics) : 0;

            const QRgb transparentColor = 0x00000000;
            QColor col = transparentColor;

            if(item->color)
            {
                int ident = item->color->getIdent();
                if(ident)
                    col = colorForCSSValue(ident);
                else if(item->color->primitiveType() == CSS_RGBCOLOR)
                    col.setRgb(item->color->getQRGBColorValue());
            }

            ShadowData *shadowData = new ShadowData(x, y, blur, col);
            style->setTextShadow(shadowData, i != 0);
        }

        break;
    }
    case CSS_PROP_OPACITY:
    {
        HANDLE_INHERIT_AND_INITIAL(opacity, Opacity)
        if(!primitiveValue || primitiveValue->primitiveType() != CSS_NUMBER)
            return; // Error case.

        // Clamp opacity to the range 0-1
        style->setOpacity(kMin(1.0f, kMax(0.0f, (float) primitiveValue->floatValue(CSS_NUMBER))));
        break;
    }
    case CSS_PROP_OUTLINE_OFFSET:
    {
        HANDLE_INHERIT_AND_INITIAL(outlineOffset, OutlineOffset)

        int offset = primitiveValue->computeLength(style, paintDeviceMetrics);
        if(offset < 0)
            return;

        style->setOutlineOffset(offset);
        break;
    }
    case CSS_PROP_BOX_SIZING:
    {
        HANDLE_INHERIT(boxSizing, BoxSizing)
        if(!primitiveValue)
            return;

        if(primitiveValue->getIdent() == CSS_VAL_CONTENT_BOX)
            style->setBoxSizing(BS_CONTENT_BOX);
        else if(primitiveValue->getIdent() == CSS_VAL_BORDER_BOX)
            style->setBoxSizing(BS_BORDER_BOX);

        break;
    }

    // shorthand properties
    case CSS_PROP_BACKGROUND:
    {
        if(isInherit)
        {
            style->setBackgroundColor(parentStyle->backgroundColor());
            style->setBackgroundImage(parentStyle->backgroundImage());
            style->setBackgroundRepeat(parentStyle->backgroundRepeat());
            style->setBackgroundAttachment(parentStyle->backgroundAttachment());
            style->setBackgroundXPosition(parentStyle->backgroundXPosition());
            style->setBackgroundYPosition(parentStyle->backgroundYPosition());
        }
        else if(isInitial)
        {
            style->setBackgroundColor(QColor());
            style->setBackgroundImage(RenderStyle::initialBackgroundImage());
            style->setBackgroundRepeat(RenderStyle::initialBackgroundRepeat());
            style->setBackgroundAttachment(RenderStyle::initialBackgroundAttachment());
            style->setBackgroundXPosition(RenderStyle::initialBackgroundXPosition());
            style->setBackgroundYPosition(RenderStyle::initialBackgroundYPosition());
        }
        
        break;
    }
    case CSS_PROP_BORDER:
    case CSS_PROP_BORDER_STYLE:
    case CSS_PROP_BORDER_WIDTH:
    case CSS_PROP_BORDER_COLOR:
    {
        if(id == CSS_PROP_BORDER || id == CSS_PROP_BORDER_COLOR)
        {
            if(isInherit)
            {
                style->setBorderTopColor(parentStyle->borderTopColor());
                style->setBorderBottomColor(parentStyle->borderBottomColor());
                style->setBorderLeftColor(parentStyle->borderLeftColor());
                style->setBorderRightColor(parentStyle->borderRightColor());
            }
            else if (isInitial)
            {
                style->setBorderTopColor(QColor()); // Reset to invalid color so currentColor is used instead.
                style->setBorderBottomColor(QColor());
                style->setBorderLeftColor(QColor());
                style->setBorderRightColor(QColor());
            }
        }
    
        if(id == CSS_PROP_BORDER || id == CSS_PROP_BORDER_STYLE)
        {
            if(isInherit)
            {
                style->setBorderTopStyle(parentStyle->borderTopStyle());
                style->setBorderBottomStyle(parentStyle->borderBottomStyle());
                style->setBorderLeftStyle(parentStyle->borderLeftStyle());
                style->setBorderRightStyle(parentStyle->borderRightStyle());
            }
            else if(isInitial)
            {
                style->setBorderTopStyle(RenderStyle::initialBorderStyle());
                style->setBorderBottomStyle(RenderStyle::initialBorderStyle());
                style->setBorderLeftStyle(RenderStyle::initialBorderStyle());
                style->setBorderRightStyle(RenderStyle::initialBorderStyle());
            }
        }

        if(id == CSS_PROP_BORDER || id == CSS_PROP_BORDER_WIDTH)
        {
            if(isInherit)
            {
                style->setBorderTopWidth(parentStyle->borderTopWidth());
                style->setBorderBottomWidth(parentStyle->borderBottomWidth());
                style->setBorderLeftWidth(parentStyle->borderLeftWidth());
                style->setBorderRightWidth(parentStyle->borderRightWidth());
            }
            else if(isInitial)
            {
                style->setBorderTopWidth(RenderStyle::initialBorderWidth());
                style->setBorderBottomWidth(RenderStyle::initialBorderWidth());
                style->setBorderLeftWidth(RenderStyle::initialBorderWidth());
                style->setBorderRightWidth(RenderStyle::initialBorderWidth());
            }
        }
        
        return;
    }
    case CSS_PROP_BORDER_TOP:
    {
        if(isInherit)
        {
            style->setBorderTopColor(parentStyle->borderTopColor());
            style->setBorderTopStyle(parentStyle->borderTopStyle());
            style->setBorderTopWidth(parentStyle->borderTopWidth());
        }
        else if(isInitial)
            style->resetBorderTop();

        return;
    }
    case CSS_PROP_BORDER_RIGHT:
    {
        if(isInherit)
        {
            style->setBorderRightColor(parentStyle->borderRightColor());
            style->setBorderRightStyle(parentStyle->borderRightStyle());
            style->setBorderRightWidth(parentStyle->borderRightWidth());
        }
        else if(isInitial)
            style->resetBorderRight();

        return;
    }
    case CSS_PROP_BORDER_BOTTOM:
    {
        if(isInherit)
        {
            style->setBorderBottomColor(parentStyle->borderBottomColor());
            style->setBorderBottomStyle(parentStyle->borderBottomStyle());
            style->setBorderBottomWidth(parentStyle->borderBottomWidth());
        }
        else if(isInitial)
            style->resetBorderBottom();

        return;
    }
    case CSS_PROP_BORDER_LEFT:
    {
        if(isInherit)
        {
            style->setBorderLeftColor(parentStyle->borderLeftColor());
            style->setBorderLeftStyle(parentStyle->borderLeftStyle());
            style->setBorderLeftWidth(parentStyle->borderLeftWidth());
        }
        else if(isInitial)
            style->resetBorderLeft();

        return;
    }
    case CSS_PROP_MARGIN:
    {
        if(isInherit)
        {
            style->setMarginTop(parentStyle->marginTop());
            style->setMarginBottom(parentStyle->marginBottom());
            style->setMarginLeft(parentStyle->marginLeft());
            style->setMarginRight(parentStyle->marginRight());
        }
        else if(isInitial)
            style->resetMargin();

        return;
    }
    case CSS_PROP_PADDING:
    {
        if(isInherit)
        {
            style->setPaddingTop(parentStyle->paddingTop());
            style->setPaddingBottom(parentStyle->paddingBottom());
            style->setPaddingLeft(parentStyle->paddingLeft());
            style->setPaddingRight(parentStyle->paddingRight());
        }
        else if(isInitial)
            style->resetPadding();

        return;
    }
    case CSS_PROP_FONT:
    {
        if(isInherit)
        {
            FontDef fontDef = parentStyle->htmlFont().fontDef();
            style->setLineHeight(parentStyle->lineHeight());
            fontDirty |= style->setFontDef(fontDef);
        }
        else if(isInitial)
        {
            FontDef fontDef;
            style->setLineHeight(RenderStyle::initialLineHeight());
            if(style->setFontDef(fontDef))
                fontDirty = true;
        }
        else if(value->isFontValue())
        {
            FontValueImpl *font = static_cast<FontValueImpl *>(value);
            if(!font->style || !font->variant || !font->weight ||
               !font->size || !font->lineHeight || !font->family)
                return;

            applyRule(CSS_PROP_FONT_STYLE, font->style);
            applyRule(CSS_PROP_FONT_VARIANT, font->variant);
            applyRule(CSS_PROP_FONT_WEIGHT, font->weight);
            applyRule(CSS_PROP_FONT_SIZE, font->size);

            // Line-height can depend on font().pixelSize(), so we have to update the font
            // before we evaluate line-height, e.g., font: 1em/1em.  FIXME: Still not
            // good enough: style="font:1em/1em; font-size:36px" should have a line-height of 36px.
            if(fontDirty)
                CSSStyleSelector::style->htmlFont().update(paintDeviceMetrics, settings);

            applyRule(CSS_PROP_LINE_HEIGHT, font->lineHeight);
            applyRule(CSS_PROP_FONT_FAMILY, font->family);
        }
        
        return;
    }
    case CSS_PROP_LIST_STYLE:
    {
        if(isInherit)
        {
            style->setListStyleType(parentStyle->listStyleType());
            style->setListStyleImage(parentStyle->listStyleImage());
            style->setListStylePosition(parentStyle->listStylePosition());
        }
        else if(isInitial)
        {
            style->setListStyleType(RenderStyle::initialListStyleType());
            style->setListStyleImage(RenderStyle::initialListStyleImage());
            style->setListStylePosition(RenderStyle::initialListStylePosition());
        }

        break;
    }
    case CSS_PROP_OUTLINE:
    {
        if(isInherit)
        {
            style->setOutlineWidth(parentStyle->outlineWidth());
            style->setOutlineColor(parentStyle->outlineColor());
            style->setOutlineStyle(parentStyle->outlineStyle());
        }
        else if(isInitial)
            style->resetOutline();

        break;
    }    
    default:
        return;
    }
}

DOMStringImpl *CSSStyleSelector::getLangAttribute(ElementImpl *e)
{
    return e->getAttributeNS(DOMString(NS_XML).handle(),
                             DOMString("xml:lang").handle());
}

// vim:ts=4:noet
