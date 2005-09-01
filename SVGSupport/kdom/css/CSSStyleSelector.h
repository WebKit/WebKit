/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml css code by:
    Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
    Copyright (C) 2003 Apple Computer, Inc.

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

#ifndef KDOM_CSSStyleSelector_H
#define KDOM_CSSStyleSelector_H

#include <qptrlist.h>
#include <qvaluevector.h>

#include <kdom/DOMString.h>
#include <kdom/css/RenderStyle.h>

class KURL;
class QPaintDeviceMetrics;

namespace KDOM
{
    class KDOMView;
    class KDOMPart;

    class NodeImpl;
    class CSSProperty;
    class CSSSelector;
    class ElementImpl;
    class KDOMSettings;
    class DocumentImpl;
    class CSSValueImpl;
    class StyleSheetImpl;
    class CSSOrderedRule;
    class CSSStyleRuleImpl;
    class CSSStyleSheetImpl;
    class CSSOrderedProperty;
    class StyleSheetListImpl;
    class CSSStyleSelectorList;
    class CSSOrderedPropertyList;
    class CSSStyleDeclarationImpl;

    /*
     * to remember the source where a rule came from. Differentiates between
     * important and not important rules. This is ordered in the order
     * they have to be applied to the RenderStyle.
     */
    enum Source
    {
        Default = 0,
        NonCSSHint = 1,
        User = 2,
        Author = 3,
        Inline = 4,
        AuthorImportant = 5,
        InlineImportant = 6,
        UserImportant = 7
    };

    /*
     * this class selects a RenderStyle for a given Element based on the
     * collection of stylesheets it contains. This is just a virtual base class
     * for specific implementations of the Selector. At the moment only
     * CSSStyleSelector exists, but someone may wish to implement XSL...
     */
    class StyleSelector
    {
    public:
        StyleSelector() { }
        virtual ~StyleSelector() { }
         virtual RenderStyle *styleForElement(ElementImpl *e) = 0;

        enum State
        {
            None = 0x00,
            Hover = 0x01,
            Focus = 0x02,
            Active = 0x04
        };
    };

    /*
     * the StyleSelector implementation for CSS2/3.
     */
    class CSSStyleSelector : public StyleSelector
    {
    public:
        /**
         * creates a new StyleSelector for a Document.
         * goes through all StyleSheets defined in the document and
         * creates a list of rules it needs to apply to objects
         *
         * Also takes into account special cases for HTML documents,
         * including the defaultStyle (which is html only)
         */
        CSSStyleSelector(DocumentImpl *doc, const QString &userStyleSheet,
                          StyleSheetListImpl *styleSheets,
                         const KURL &url, bool _strictParsing);
        /*
         * same as above but for a single stylesheet.
         */
        CSSStyleSelector(CSSStyleSheetImpl *sheet);

        virtual ~CSSStyleSelector();

        void addSheet(CSSStyleSheetImpl *sheet);
        static void clear();

        virtual RenderStyle *styleForElement(ElementImpl *e);

        QValueVector<int> fontSizes() const { return m_fontSizes; }
        QValueVector<int> fixedFontSizes() const { return m_fixedFontSizes; }

        bool strictParsing;

        struct Encodedurl
        {
            QString host; // including protocol
            QString path;
            QString file;
        } encodedurl;

        void computeFontSizes(QPaintDeviceMetrics *paintDeviceMetrics, int zoomFactor);
        void computeFontSizesFor(QPaintDeviceMetrics *paintDeviceMetrics, int zoomFactor, QValueVector<int> &fontSizes, bool isFixed);

    protected:
        /* checks if the complete selector (which can be build up from a few
         * CSSSelector's with given relationships matches the given Element */
        void checkSelector(int selector, ElementImpl *e);
        
        /* checks if the selector matches the given Element */
        bool checkOneSelector(CSSSelector *selector, ElementImpl *e);

        /* builds up the selectors and properties lists from the CSSStyleSelectorList's */
        void buildLists();
        void clearLists();

        unsigned int addInlineDeclarations(ElementImpl *e, CSSStyleDeclarationImpl *decl, unsigned int numProps);

        virtual void adjustRenderStyle(RenderStyle *, ElementImpl *) { }
        virtual unsigned int addExtraDeclarations(ElementImpl *, unsigned int numProps) { return numProps; }

        DOMStringImpl *getLangAttribute(ElementImpl *e);

        // Helper function (used for instance by khtml's HTMLCSSStyleSelector)
        static Length convertToLength(CSSPrimitiveValueImpl *primitiveValue, RenderStyle *style,
                                      QPaintDeviceMetrics *paintDeviceMetrics, bool *ok = 0);

        static RenderStyle *styleNotYetAvailable;

        CSSStyleSelectorList *defaultStyle;
        CSSStyleSelectorList *defaultQuirksStyle;
        CSSStyleSelectorList *defaultPrintStyle;

        CSSStyleSelectorList *authorStyle;
        CSSStyleSelectorList *userStyle;
        CSSStyleSheetImpl *userSheet;

    protected:
        static QColor colorForCSSValue(int css_value); // Helper

    private:
        void init(const KDOMSettings *settings);

    public: // we need to make the enum public for SelectorCache
        enum SelectorState
        {
            Unknown = 0,
            Applies,
            AppliesPseudo,
            Invalid
        };

        enum SelectorMedia
        {
            MediaAural = 1,
            MediaBraille,
            MediaEmboss,
            MediaHandheld,
            MediaPrint,
            MediaProjection,
            MediaScreen,
            MediaTTY,
            MediaTV
        };

    protected:
        struct SelectorCache
        {
            SelectorState state;
            unsigned int props_size;
            int *props;
        };

        unsigned int selectors_size;
        CSSSelector **selectors;
        SelectorCache *selectorCache;
        unsigned int properties_size;
        CSSOrderedProperty **properties;
        QMemArray<CSSOrderedProperty> inlineProps;
        QString m_medium;
        CSSOrderedProperty **propsToApply;
        CSSOrderedProperty **pseudoProps;
        unsigned int propsToApplySize;
        unsigned int pseudoPropsSize;
    
        RenderStyle::PseudoId dynamicPseudo;

        RenderStyle *style;
        RenderStyle *parentStyle;
        ElementImpl *element;
        NodeImpl *parentNode;

        KDOMView *view;
        KDOMPart *part;
        const KDOMSettings *settings;
        QPaintDeviceMetrics *paintDeviceMetrics;

        QValueVector<int> m_fontSizes;
        QValueVector<int> m_fixedFontSizes;

        bool fontDirty;

        virtual void applyRule(int id, CSSValueImpl *value);
    };

    /*
     * List of properties that get applied to the Element. We need to collect
     * them first and then apply them one by one, because we have to change
     * the apply order. Some properties depend on other one already being
     * applied (for example all properties specifying some length need to
     * have already the correct font size. Same applies to color
     * While sorting them, we have to take care not to mix up the original
     * order.
     */
    class CSSOrderedProperty
    {
    public:
        CSSOrderedProperty(CSSProperty *_prop, unsigned int _selector, bool first,
                           Source source, unsigned int specificity, unsigned int _position)
        : prop(_prop), pseudoId(RenderStyle::NOPSEUDO), selector(_selector), position(_position)
        {
            priority = (!first << 30) | (source << 24) | specificity;
        }

        bool operator<(const CSSOrderedProperty &other) const
        {
            if(priority < other.priority) return true;
            if(priority > other.priority) return false;
            if(position < other.position) return true;
            return false;
        }

        CSSProperty *prop;
        RenderStyle::PseudoId pseudoId;
        unsigned int selector;
        unsigned int position;

        Q_UINT32 priority;
    };

    /*
     * This is the list we will collect all properties we need to apply in.
     * It will get sorted once before applying.
     */
    class CSSOrderedPropertyList : public QPtrList<CSSOrderedProperty>
    {
    public:
        virtual int compareItems(QPtrCollection::Item i1, QPtrCollection::Item i2);
        void append(CSSStyleDeclarationImpl *decl, unsigned int selector,
                    unsigned int specificity, Source regular, Source important);
    };

    class CSSOrderedRule
    {
    public:
        CSSOrderedRule(CSSStyleRuleImpl *_rule, CSSSelector *_selector, int _index);
        ~CSSOrderedRule();

        CSSSelector *selector;
        CSSStyleRuleImpl *rule;
        int index;
    };

    class CSSStyleSelectorList : public QPtrList<CSSOrderedRule>
    {
    public:
        CSSStyleSelectorList();
        virtual ~CSSStyleSelectorList();

        void append(CSSStyleSheetImpl *sheet, DOMStringImpl *medium);
        void collect(QPtrList<CSSSelector> *selectorList, CSSOrderedPropertyList *propList,
                     Source regular, Source important);
    };
}

#endif

// vim:ts=4:noet
