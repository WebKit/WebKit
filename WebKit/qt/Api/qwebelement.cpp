/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

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

#include "config.h"
#include "qwebelement.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSParser.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleRule.h"
#include "CString.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLElement.h"
#include "JSGlobalObject.h"
#include "JSHTMLElement.h"
#include "JSObject.h"
#include "NodeList.h"
#include "PropertyNameArray.h"
#include "RenderImage.h"
#include "ScriptFunctionCall.h"
#include "StaticNodeList.h"
#include "qt_runtime.h"
#include "qwebframe.h"
#include "qwebframe_p.h"
#include "runtime_root.h"
#include <parser/SourceCode.h>
#include <wtf/Vector.h>

#include <QPainter>

using namespace WebCore;

class QWebElementPrivate {
public:
};

/*!
    \class QWebElement
    \since 4.6
    \brief The QWebElement class provides convenient access to DOM elements in
    a QWebFrame.
    \inmodule QtWebKit

    A QWebElement object allows easy access to the document model, represented
    by a tree-like structure of DOM elements. The root of the tree is called
    the document element and can be accessed using
    QWebFrame::documentElement().

    Specific elements can be accessed using findAll() and findFirst(). These
    elements are identified using CSS selectors. The code snippet below
    demonstrates the use of findAll().

    \snippet webkitsnippets/webelement/main.cpp FindAll

    The first list contains all \c span elements in the document. The second
    list contains \c span elements that are children of \c p, classified with
    \c intro.

    Using findFirst() is more efficient than calling findAll(), and extracting
    the first element only in the list returned.

    Alternatively you can traverse the document manually using firstChild() and
    nextSibling():

    \snippet webkitsnippets/webelement/main.cpp Traversing with QWebElement

    The underlying content of QWebElement is explicitly shared. Creating a copy
    of a QWebElement does not create a copy of the content. Instead, both
    instances point to the same element.

    The element's attributes can be read using attribute() and modified with
    setAttribute().

    The contents of child elements can be converted to plain text with
    toPlainText(); to XHTML using toInnerXml(). To include the element's tag in
    the output, use toOuterXml().

    It is possible to replace the contents of child elements using
    setPlainText() and setInnerXml(). To replace the element itself and its
    contents, use setOuterXml().
*/

/*!
    Constructs a null web element.
*/
QWebElement::QWebElement()
    : d(0)
    , m_element(0)
{
}

/*!
    \internal
*/
QWebElement::QWebElement(WebCore::Element* domElement)
    : d(0)
    , m_element(domElement)
{
    if (m_element)
        m_element->ref();
}

/*!
    \internal
*/
QWebElement::QWebElement(WebCore::Node* node)
    : d(0)
    , m_element(0)
{
    if (node && node->isHTMLElement()) {
        m_element = static_cast<HTMLElement*>(node);
        m_element->ref();
    }
}

/*!
    Constructs a copy of \a other.
*/
QWebElement::QWebElement(const QWebElement &other)
    : d(0)
    , m_element(other.m_element)
{
    if (m_element)
        m_element->ref();
}

/*!
    Assigns \a other to this element and returns a reference to this element.
*/
QWebElement &QWebElement::operator=(const QWebElement &other)
{
    // ### handle "d" assignment
    if (this != &other) {
        Element *otherElement = other.m_element;
        if (otherElement)
            otherElement->ref();
        if (m_element)
            m_element->deref();
        m_element = otherElement;
    }
    return *this;
}

/*!
    Destroys the element. However, the underlying DOM element is not destroyed.
*/
QWebElement::~QWebElement()
{
    delete d;
    if (m_element)
        m_element->deref();
}

bool QWebElement::operator==(const QWebElement& o) const
{
    return m_element == o.m_element;
}

bool QWebElement::operator!=(const QWebElement& o) const
{
    return m_element != o.m_element;
}

/*!
    Returns true if the element is a null element; otherwise returns false.
*/
bool QWebElement::isNull() const
{
    return !m_element;
}

/*!
    Returns a new list of child elements matching the given CSS selector
    \a selectorQuery. If there are no matching elements, an empty list is
    returned.

    \l{http://www.w3.org/TR/REC-CSS2/selector.html#q1}{Standard CSS2 selector}
    syntax is used for the query.

    \note This search is performed recursively.

    \sa findFirst()
*/
QList<QWebElement> QWebElement::findAll(const QString &selectorQuery) const
{
    QList<QWebElement> elements;
    if (!m_element)
        return elements;

    ExceptionCode exception = 0; // ###
    RefPtr<NodeList> nodes = m_element->querySelectorAll(selectorQuery, exception);
    if (!nodes)
        return elements;

    for (unsigned i = 0; i < nodes->length(); ++i) {
        WebCore::Node* n = nodes->item(i);
        elements.append(QWebElement(static_cast<Element*>(n)));
    }

    return elements;
}

/*!
    Returns the first child element that matches the given CSS selector
    \a selectorQuery.

    \l{http://www.w3.org/TR/REC-CSS2/selector.html#q1}{Standard CSS2 selector}
    syntax is used for the query.

    \note This search is performed recursively.

    \sa findAll()
*/
QWebElement QWebElement::findFirst(const QString &selectorQuery) const
{
    if (!m_element)
        return QWebElement();
    ExceptionCode exception = 0; // ###
    return QWebElement(m_element->querySelector(selectorQuery, exception).get());
}

/*!
    Replaces the existing content of this element with \a text.

    This is equivalent to setting the HTML innerText property.

    \sa toPlainText()
*/
void QWebElement::setPlainText(const QString &text)
{
    if (!m_element || !m_element->isHTMLElement())
        return;
    ExceptionCode exception = 0;
    static_cast<HTMLElement*>(m_element)->setInnerText(text, exception);
}

/*!
    Returns the text between the start and the end tag of this
    element.

    This is equivalent to reading the HTML innerText property.

    \sa setPlainText()
*/
QString QWebElement::toPlainText() const
{
    if (!m_element || !m_element->isHTMLElement())
        return QString();
    return static_cast<HTMLElement*>(m_element)->innerText();
}

/*!
    Replaces the contents of this element as well as its own tag with
    \a markup. The string may contain HTML or XML tags, which is parsed and
    formatted before insertion into the document.

    \note This is currently only implemented for (X)HTML elements.

    \sa toOuterXml(), toInnerXml(), setInnerXml()
*/
void QWebElement::setOuterXml(const QString &markup)
{
    if (!m_element || !m_element->isHTMLElement())
        return;

    ExceptionCode exception = 0;

    static_cast<HTMLElement*>(m_element)->setOuterHTML(markup, exception);
}

/*!
    Returns this element converted to XML, including the start and the end
    tags as well as its attributes.

    \note This is currently implemented for (X)HTML elements only.

    \sa setOuterXml(), setInnerXml(), toInnerXml()
*/
QString QWebElement::toOuterXml() const
{
    if (!m_element || !m_element->isHTMLElement())
        return QString();

    return static_cast<HTMLElement*>(m_element)->outerHTML();
}

/*!
    Replaces the contents of this element with \a markup. The string may
    contain HTML or XML tags, which is parsed and formatted before insertion
    into the document.

    \note This is currently implemented for (X)HTML elements only.

    \sa toInnerXml(), toOuterXml(), setOuterXml()
*/
void QWebElement::setInnerXml(const QString &markup)
{
    if (!m_element || !m_element->isHTMLElement())
        return;

    ExceptionCode exception = 0;

    static_cast<HTMLElement*>(m_element)->setInnerHTML(markup, exception);
}

/*!
    Returns the XML content between the element's start and end tags.

    \note This is currently implemented for (X)HTML elements only.

    \sa setInnerXml(), setOuterXml(), toOuterXml()
*/
QString QWebElement::toInnerXml() const
{
    if (!m_element || !m_element->isHTMLElement())
        return QString();

    return static_cast<HTMLElement*>(m_element)->innerHTML();
}

/*!
    Adds an attribute with the given \a name and \a value. If an attribute with
    the same name exists, its value is replaced by \a value.

    \sa attribute(), attributeNS(), setAttributeNS()
*/
void QWebElement::setAttribute(const QString &name, const QString &value)
{
    if (!m_element)
        return;
    ExceptionCode exception = 0;
    m_element->setAttribute(name, value, exception);
}

/*!
    Adds an attribute with the given \a name in \a namespaceUri with \a value.
    If an attribute with the same name exists, its value is replaced by
    \a value.

    \sa attributeNS(), attribute(), setAttribute()
*/
void QWebElement::setAttributeNS(const QString &namespaceUri, const QString &name, const QString &value)
{
    if (!m_element)
        return;
    WebCore::ExceptionCode exception = 0;
    m_element->setAttributeNS(namespaceUri, name, value, exception);
}

/*!
    Returns the attribute with the given \a name. If the attribute does not
    exist, \a defaultValue is returned.

    \sa setAttribute(), setAttributeNS(), attributeNS()
*/
QString QWebElement::attribute(const QString &name, const QString &defaultValue) const
{
    if (!m_element)
        return QString();
    if (m_element->hasAttribute(name))
        return m_element->getAttribute(name);
    else
        return defaultValue;
}

/*!
    Returns the attribute with the given \a name in \a namespaceUri. If the
    attribute does not exist, \a defaultValue is returned.

    \sa setAttributeNS(), setAttribute(), attribute()
*/
QString QWebElement::attributeNS(const QString &namespaceUri, const QString &name, const QString &defaultValue) const
{
    if (!m_element)
        return QString();
    if (m_element->hasAttributeNS(namespaceUri, name))
        return m_element->getAttributeNS(namespaceUri, name);
    else
        return defaultValue;
}

/*!
    Returns true if this element has an attribute with the given \a name;
    otherwise returns false.

    \sa attribute(), setAttribute()
*/
bool QWebElement::hasAttribute(const QString &name) const
{
    if (!m_element)
        return false;
    return m_element->hasAttribute(name);
}

/*!
    Returns true if this element has an attribute with the given \a name, in
    \a namespaceUri; otherwise returns false.

    \sa attributeNS(), setAttributeNS()
*/
bool QWebElement::hasAttributeNS(const QString &namespaceUri, const QString &name) const
{
    if (!m_element)
        return false;
    return m_element->hasAttributeNS(namespaceUri, name);
}

/*!
    Removes the attribute with the given \a name from this element.

    \sa attribute(), setAttribute(), hasAttribute()
*/
void QWebElement::removeAttribute(const QString &name)
{
    if (!m_element)
        return;
    ExceptionCode exception = 0;
    m_element->removeAttribute(name, exception);
}

/*!
    Removes the attribute with the given \a name, in \a namespaceUri, from this
    element.

    \sa attributeNS(), setAttributeNS(), hasAttributeNS()
*/
void QWebElement::removeAttributeNS(const QString &namespaceUri, const QString &name)
{
    if (!m_element)
        return;
    WebCore::ExceptionCode exception = 0;
    m_element->removeAttributeNS(namespaceUri, name, exception);
}

/*!
    Returns true if the element has any attributes defined; otherwise returns
    false;

    \sa attribute(), setAttribute()
*/
bool QWebElement::hasAttributes() const
{
    if (!m_element)
        return false;
    return m_element->hasAttributes();
}

/*!
    Returns true if the element has keyboard input focus; otherwise, returns false

    \sa setFocus()
*/
bool QWebElement::hasFocus() const
{
    if (!m_element)
        return false;
    if (m_element->document())
        return m_element == m_element->document()->focusedNode();
    return false;
}

/*!
    Gives keyboard input focus to this element

    \sa hasFocus()
*/
void QWebElement::setFocus()
{
    if (!m_element)
        return;
    if (m_element->document() && m_element->isFocusable())
        m_element->document()->setFocusedNode(m_element);
}

/*!
    Returns the geometry of this element, relative to its containing frame.

    \sa tagName()
*/
QRect QWebElement::geometry() const
{
    if (!m_element)
        return QRect();
    return m_element->getRect();
}

/*!
    Returns the tag name of this element.

    \sa geometry()
*/
QString QWebElement::tagName() const
{
    if (!m_element)
        return QString();
    return m_element->tagName();
}

/*!
    Returns the namespace prefix of the element. If the element has no\
    namespace prefix, empty string is returned.
*/
QString QWebElement::prefix() const
{
    if (!m_element)
        return QString();
    return m_element->prefix();
}

/*!
    Returns the local name of the element. If the element does not use
    namespaces, an empty string is returned.
*/
QString QWebElement::localName() const
{
    if (!m_element)
        return QString();
    return m_element->localName();
}

/*!
    Returns the namespace URI of this element. If the element has no namespace
    URI, an empty string is returned.
*/
QString QWebElement::namespaceUri() const
{
    if (!m_element)
        return QString();
    return m_element->namespaceURI();
}

/*!
    Returns the parent element of this elemen. If this element is the root
    document element, a null element is returned.
*/
QWebElement QWebElement::parent() const
{
    if (m_element)
        return QWebElement(m_element->parentElement());
    return QWebElement();
}

/*!
    Returns the element's first child.

    \sa lastChild(), previousSibling(), nextSibling()
*/
QWebElement QWebElement::firstChild() const
{
    if (!m_element)
        return QWebElement();
    for (Node* child = m_element->firstChild(); child; child = child->nextSibling()) {
        if (!child->isElementNode())
            continue;
        Element* e = static_cast<Element*>(child);
        return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the element's last child.

    \sa firstChild(), previousSibling(), nextSibling()
*/
QWebElement QWebElement::lastChild() const
{
    if (!m_element)
        return QWebElement();
    for (Node* child = m_element->lastChild(); child; child = child->previousSibling()) {
        if (!child->isElementNode())
            continue;
        Element* e = static_cast<Element*>(child);
        return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the element's next sibling.

    \sa firstChild(), previousSibling(), lastChild()
*/
QWebElement QWebElement::nextSibling() const
{
    if (!m_element)
        return QWebElement();
    for (Node* sib = m_element->nextSibling(); sib; sib = sib->nextSibling()) {
        if (!sib->isElementNode())
            continue;
        Element* e = static_cast<Element*>(sib);
        return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the element's previous sibling.

    \sa firstChild(), nextSibling(), lastChild()
*/
QWebElement QWebElement::previousSibling() const
{
    if (!m_element)
        return QWebElement();
    for (Node* sib = m_element->previousSibling(); sib; sib = sib->previousSibling()) {
        if (!sib->isElementNode())
            continue;
        Element* e = static_cast<Element*>(sib);
        return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the document which this element belongs to.
*/
QWebElement QWebElement::document() const
{
    if (!m_element)
        return QWebElement();
    Document* document = m_element->document();
    if (!document)
        return QWebElement();
    return QWebElement(document->documentElement());
}

/*!
    Returns the web frame which this element is a part of. If the element is a
    null element, null is returned.
*/
QWebFrame *QWebElement::webFrame() const
{
    if (!m_element)
        return 0;

    Document* document = m_element->document();
    if (!document)
        return 0;

    Frame* frame = document->frame();
    if (!frame)
        return 0;
    return QWebFramePrivate::kit(frame);
}

static bool setupScriptContext(WebCore::Element* element, JSC::JSValue& thisValue, ScriptState*& state, ScriptController*& scriptController)
{
    if (!element)
        return false;

    Document* document = element->document();
    if (!document)
        return false;

    Frame* frame = document->frame();
    if (!frame)
        return false;

    scriptController = frame->script();
    if (!scriptController)
        return false;

    state = scriptController->globalObject(mainThreadNormalWorld())->globalExec();
    if (!state)
        return false;

    thisValue = toJS(state, element);
    if (!thisValue)
        return false;

    return true;
}


/*!
    Executes \a scriptSource with this element as \c this object.
*/
QVariant QWebElement::evaluateJavaScript(const QString& scriptSource)
{
    if (scriptSource.isEmpty())
        return QVariant();

    ScriptState* state = 0;
    JSC::JSValue thisValue;
    ScriptController* scriptController = 0;

    if (!setupScriptContext(m_element, thisValue, state, scriptController))
        return QVariant();

    JSC::ScopeChain& scopeChain = state->dynamicGlobalObject()->globalScopeChain();
    JSC::UString script((const UChar*)scriptSource.data(), scriptSource.length());
    JSC::Completion completion = JSC::evaluate(state, scopeChain, JSC::makeSource(script), thisValue);
    if ((completion.complType() != JSC::ReturnValue) && (completion.complType() != JSC::Normal))
        return QVariant();

    JSC::JSValue result = completion.value();
    if (!result)
        return QVariant();

    int distance = 0;
    return JSC::Bindings::convertValueToQVariant(state, result, QMetaType::Void, &distance);
}

/*!
    \enum QWebElement::StyleResolveStrategy

    This enum describes how QWebElement's styleProperty resolves the given
    property name.

    \value InlineStyle Return the property value as it is defined in
           the element, without respecting style inheritance and other CSS
           rules.
    \value CascadedStyle The property's value is determined using the
           inheritance and importance rules defined in the document's
           stylesheet.
    \value ComputedStyle The property's value is the absolute value
           of the style property resolved from the environment.
*/

/*!
    Returns the value of the style with the given \a name using the specified
    \a strategy. If a style with \a name does not exist, an empty string is
    returned.

    In CSS, the cascading part depends on which CSS rule has priority and is
    thus applied. Generally, the last defined rule has priority. Thus, an
    inline style rule has priority over an embedded block style rule, which
    in return has priority over an external style rule.

    If the "!important" declaration is set on one of those, the declaration
    receives highest priority, unless other declarations also use the
    "!important" declaration. Then, the last "!important" declaration takes
    predecence.

    \sa setStyleProperty()
*/

QString QWebElement::styleProperty(const QString &name, StyleResolveStrategy strategy) const
{
    if (!m_element || !m_element->isStyledElement())
        return QString();

    int propID = cssPropertyID(name);

    if (!propID)
        return QString();

    CSSStyleDeclaration* style = static_cast<StyledElement*>(m_element)->style();

    if (strategy == InlineStyle)
        return style->getPropertyValue(propID);

    if (strategy == CascadedStyle) {
        if (style->getPropertyPriority(propID))
            return style->getPropertyValue(propID);

        // We are going to resolve the style property by walking through the
        // list of non-inline matched CSS rules for the element, looking for
        // the highest priority definition.

        // Get an array of matched CSS rules for the given element sorted
        // by importance and inheritance order. This include external CSS
        // declarations, as well as embedded and inline style declarations.

        DOMWindow* domWindow = m_element->document()->frame()->domWindow();
        if (RefPtr<CSSRuleList> rules = domWindow->getMatchedCSSRules(m_element, "")) {
            for (int i = rules->length(); i > 0; --i) {
                CSSStyleRule* rule = static_cast<CSSStyleRule*>(rules->item(i - 1));

                if (rule->style()->getPropertyPriority(propID))
                    return rule->style()->getPropertyValue(propID);

                if (style->getPropertyValue(propID).isEmpty())
                    style = rule->style();
            }
        }

        return style->getPropertyValue(propID);
    }

    if (strategy == ComputedStyle) {
        if (!m_element || !m_element->isStyledElement())
            return QString();

        int propID = cssPropertyID(name);

        RefPtr<CSSComputedStyleDeclaration> style = computedStyle(m_element);
        if (!propID || !style)
            return QString();

        return style->getPropertyValue(propID);
    }

    return QString();
}

/*!
    Sets the value of the inline style with the given \a name to \a value.

    Setting a value, does not necessarily mean that it will become the applied
    value, due to the fact that the style property's value might have been set
    earlier with a higher priority in external or embedded style declarations.

    In order to ensure that the value will be applied, you may have to append
    "!important" to the value.
*/
void QWebElement::setStyleProperty(const QString &name, const QString &value)
{
    if (!m_element || !m_element->isStyledElement())
        return;

    int propID = cssPropertyID(name);
    CSSStyleDeclaration* style = static_cast<StyledElement*>(m_element)->style();
    if (!propID || !style)
        return;

    ExceptionCode exception = 0;
    style->setProperty(name, value, exception);
}

/*!
    Returns the list of classes of this element.
*/
QStringList QWebElement::classes() const
{
    if (!hasAttribute(QLatin1String("class")))
        return QStringList();

    QStringList classes =  attribute(QLatin1String("class")).simplified().split(QLatin1Char(' '), QString::SkipEmptyParts);
#if QT_VERSION >= 0x040500
    classes.removeDuplicates();
#else
    int n = classes.size();
    int j = 0;
    QSet<QString> seen;
    seen.reserve(n);
    for (int i = 0; i < n; ++i) {
        const QString& s = classes.at(i);
        if (seen.contains(s))
            continue;
        seen.insert(s);
        if (j != i)
            classes[j] = s;
        ++j;
    }
    if (n != j)
        classes.erase(classes.begin() + j, classes.end());
#endif
    return classes;
}

/*!
    Returns true if this element has a class with the given \a name; otherwise
    returns false.
*/
bool QWebElement::hasClass(const QString &name) const
{
    QStringList list = classes();
    return list.contains(name);
}

/*!
    Adds the specified class with the given \a name to the element.
*/
void QWebElement::addClass(const QString &name)
{
    QStringList list = classes();
    if (!list.contains(name)) {
        list.append(name);
        QString value = list.join(QLatin1String(" "));
        setAttribute(QLatin1String("class"), value);
    }
}

/*!
    Removes the specified class with the given \a name from the element.
*/
void QWebElement::removeClass(const QString &name)
{
    QStringList list = classes();
    if (list.contains(name)) {
        list.removeAll(name);
        QString value = list.join(QLatin1String(" "));
        setAttribute(QLatin1String("class"), value);
    }
}

/*!
    Adds the specified class with the given \a name if it is not present. If
    the class is already present, it will be removed.
*/
void QWebElement::toggleClass(const QString &name)
{
    QStringList list = classes();
    if (list.contains(name))
        list.removeAll(name);
    else
        list.append(name);

    QString value = list.join(QLatin1String(" "));
    setAttribute(QLatin1String("class"), value);
}

/*!
    Appends the given \a element as the element's last child.

    If \a element is the child of another element, it is re-parented to this
    element. If \a element is a child of this element, then its position in
    the list of children is changed.

    Calling this function on a null element does nothing.

    \sa prependInside(), prependOutside(), appendOutside()
*/
void QWebElement::appendInside(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    ExceptionCode exception = 0;
    m_element->appendChild(element.m_element, exception);
}

/*!
    Appends the result of parsing \a markup as the element's last child.

    Calling this function on a null element does nothing.

    \sa prependInside(), prependOutside(), appendOutside()
*/
void QWebElement::appendInside(const QString &markup)
{
    if (!m_element)
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(markup);

    ExceptionCode exception = 0;
    m_element->appendChild(fragment, exception);
}

/*!
    Prepends \a element as the element's first child.

    If \a element is the child of another element, it is re-parented to this
    element. If \a element is a child of this element, then its position in
    the list of children is changed.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependOutside(), appendOutside()
*/
void QWebElement::prependInside(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    ExceptionCode exception = 0;

    if (m_element->hasChildNodes())
        m_element->insertBefore(element.m_element, m_element->firstChild(), exception);
    else
        m_element->appendChild(element.m_element, exception);
}

/*!
    Prepends the result of parsing \a markup as the element's first child.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependOutside(), appendOutside()
*/
void QWebElement::prependInside(const QString &markup)
{
    if (!m_element)
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(markup);

    ExceptionCode exception = 0;

    if (m_element->hasChildNodes())
        m_element->insertBefore(fragment, m_element->firstChild(), exception);
    else
        m_element->appendChild(fragment, exception);
}


/*!
    Inserts the given \a element before this element.

    If \a element is the child of another element, it is re-parented to the
    parent of this element.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependInside(), appendOutside()
*/
void QWebElement::prependOutside(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    if (!m_element->parent())
        return;

    ExceptionCode exception = 0;
    m_element->parent()->insertBefore(element.m_element, m_element, exception);
}

/*!
    Inserts the result of parsing \a markup before this element.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependInside(), appendOutside()
*/
void QWebElement::prependOutside(const QString &markup)
{
    if (!m_element)
        return;

    if (!m_element->parent())
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(markup);

    ExceptionCode exception = 0;
    m_element->parent()->insertBefore(fragment, m_element, exception);
}

/*!
    Inserts the given \a element after this element.

    If \a element is the child of another element, it is re-parented to the
    parent of this element.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependInside(), prependOutside()
*/
void QWebElement::appendOutside(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    if (!m_element->parent())
        return;

    ExceptionCode exception = 0;
    if (!m_element->nextSibling())
        m_element->parent()->appendChild(element.m_element, exception);
    else
        m_element->parent()->insertBefore(element.m_element, m_element->nextSibling(), exception);
}

/*!
    Inserts the result of parsing \a markup after this element.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependInside(), prependOutside()
*/
void QWebElement::appendOutside(const QString &markup)
{
    if (!m_element)
        return;

    if (!m_element->parent())
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(markup);

    ExceptionCode exception = 0;
    if (!m_element->nextSibling())
        m_element->parent()->appendChild(fragment, exception);
    else
        m_element->parent()->insertBefore(fragment, m_element->nextSibling(), exception);
}

/*!
    Returns a clone of this element.

    The clone may be inserted at any point in the document.

    \sa appendInside(), prependInside(), prependOutside(), appendOutside()
*/
QWebElement QWebElement::clone() const
{
    if (!m_element)
        return QWebElement();

    return QWebElement(m_element->cloneElementWithChildren().get());
}

/*!
    Removes this element from the document and returns a reference to it.

    The element is still valid after removal, and can be inserted into other
    parts of the document.

    \sa removeChildren(), removeFromDocument()
*/
QWebElement &QWebElement::takeFromDocument()
{
    if (!m_element)
        return *this;

    ExceptionCode exception = 0;
    m_element->remove(exception);

    return *this;
}

/*!
    Removes this element from the document and makes it a null element.

    \sa removeChildren(), takeFromDocument()
*/
void QWebElement::removeFromDocument()
{
    if (!m_element)
        return;

    ExceptionCode exception = 0;
    m_element->remove(exception);
    m_element->deref();
    m_element = 0;
}

/*!
    Removes all children from this element.

    \sa removeFromDocument(), takeFromDocument()
*/
void QWebElement::removeChildren()
{
    if (!m_element)
        return;

    m_element->removeAllChildren();
}

static RefPtr<Node> findInsertionPoint(PassRefPtr<Node> root)
{
    RefPtr<Node> node = root;

    // Go as far down the tree as possible.
    while (node->hasChildNodes() && node->firstChild()->isElementNode())
        node = node->firstChild();

    // TODO: Implement SVG support
    if (node->isHTMLElement()) {
        HTMLElement* element = static_cast<HTMLElement*>(node.get());

        // The insert point could be a non-enclosable tag and it can thus
        // never have children, so go one up. Get the parent element, and not
        // note as a root note will always exist.
        if (element->endTagRequirement() == TagStatusForbidden)
            node = node->parentElement();
    }

    return node;
}

/*!
    Encloses the contents of this element with \a element. This element becomes
    the child of the deepest descendant within \a element.

    ### illustration

    \sa encloseWith()
*/
void QWebElement::encloseContentsWith(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    RefPtr<Node> insertionPoint = findInsertionPoint(element.m_element);

    if (!insertionPoint)
        return;

    ExceptionCode exception = 0;

    // reparent children
    for (RefPtr<Node> child = m_element->firstChild(); child;) {
        RefPtr<Node> next = child->nextSibling();
        insertionPoint->appendChild(child, exception);
        child = next;
    }

    if (m_element->hasChildNodes())
        m_element->insertBefore(element.m_element, m_element->firstChild(), exception);
    else
        m_element->appendChild(element.m_element, exception);
}

/*!
    Encloses the contents of this element with the result of parsing \a markup.
    This element becomes the child of the deepest descendant within \a markup.

    \sa encloseWith()
*/
void QWebElement::encloseContentsWith(const QString &markup)
{
    if (!m_element)
        return;

    if (!m_element->parent())
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(markup);

    if (!fragment || !fragment->firstChild())
        return;

    RefPtr<Node> insertionPoint = findInsertionPoint(fragment->firstChild());

    if (!insertionPoint)
        return;

    ExceptionCode exception = 0;

    // reparent children
    for (RefPtr<Node> child = m_element->firstChild(); child;) {
        RefPtr<Node> next = child->nextSibling();
        insertionPoint->appendChild(child, exception);
        child = next;
    }

    if (m_element->hasChildNodes())
        m_element->insertBefore(fragment, m_element->firstChild(), exception);
    else
        m_element->appendChild(fragment, exception);
}

/*!
    Encloses this element with \a element. This element becomes the child of
    the deepest descendant within \a element.

    \sa replace()
*/
void QWebElement::encloseWith(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    RefPtr<Node> insertionPoint = findInsertionPoint(element.m_element);

    if (!insertionPoint)
        return;

    // Keep reference to these two nodes before pulling out this element and
    // wrapping it in the fragment. The reason for doing it in this order is
    // that once the fragment has been added to the document it is empty, so
    // we no longer have access to the nodes it contained.
    Node* parentNode = m_element->parent();
    Node* siblingNode = m_element->nextSibling();

    ExceptionCode exception = 0;
    insertionPoint->appendChild(m_element, exception);

    if (!siblingNode)
        parentNode->appendChild(element.m_element, exception);
    else
        parentNode->insertBefore(element.m_element, siblingNode, exception);
}

/*!
    Encloses this element with the result of parsing \a markup. This element
    becomes the child of the deepest descendant within \a markup.

    \sa replace()
*/
void QWebElement::encloseWith(const QString &markup)
{
    if (!m_element)
        return;

    if (!m_element->parent())
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(markup);

    if (!fragment || !fragment->firstChild())
        return;

    RefPtr<Node> insertionPoint = findInsertionPoint(fragment->firstChild());

    if (!insertionPoint)
        return;

    // Keep reference to these two nodes before pulling out this element and
    // wrapping it in the fragment. The reason for doing it in this order is
    // that once the fragment has been added to the document it is empty, so
    // we no longer have access to the nodes it contained.
    Node* parentNode = m_element->parent();
    Node* siblingNode = m_element->nextSibling();

    ExceptionCode exception = 0;
    insertionPoint->appendChild(m_element, exception);

    if (!siblingNode)
        parentNode->appendChild(fragment, exception);
    else
        parentNode->insertBefore(fragment, siblingNode, exception);
}

/*!
    Replaces this element with \a element.

    This method will not replace the <html>, <head> or <body> elements.

    \sa encloseWith()
*/
void QWebElement::replace(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    appendOutside(element);
    takeFromDocument();
}

/*!
    Replaces this element with the result of parsing \a markup.

    This method will not replace the <html>, <head> or <body> elements.

    \sa encloseWith()
*/
void QWebElement::replace(const QString &markup)
{
    if (!m_element)
        return;

    appendOutside(markup);
    takeFromDocument();
}

/*!
    \internal
    Walk \a node's parents until a valid QWebElement is found.
    For example, a WebCore::Text node is not a valid Html QWebElement, but its
    enclosing p tag is.
*/
QWebElement QWebElement::enclosingElement(WebCore::Node* node)
{
    QWebElement element(node);

    while (element.isNull() && node) {
        node = node->parentNode();
        element = QWebElement(node);
    }
    return element;
}

/*!
    \fn inline bool QWebElement::operator==(const QWebElement& o) const;

    Returns true if this element points to the same underlying DOM object as
    \a o; otherwise returns false.
*/

/*!
    \fn inline bool QWebElement::operator!=(const QWebElement& o) const;

    Returns true if this element points to a different underlying DOM object
    than \a o; otherwise returns false.
*/


/*! 
  Render the element into \a painter .
*/
void QWebElement::render(QPainter* painter)
{
    WebCore::Element* e = m_element;
    Document* doc = e ? e->document() : 0;
    if (!doc)
        return;

    Frame* frame = doc->frame();
    if (!frame || !frame->view() || !frame->contentRenderer())
        return;

    FrameView* view = frame->view();

    view->layoutIfNeededRecursive();

    IntRect rect = e->getRect();

    if (rect.size().isEmpty())
        return;

    GraphicsContext context(painter);

    context.save();
    context.translate(-rect.x(), -rect.y());
    view->setNodeToDraw(e);
    view->paintContents(&context, rect);
    view->setNodeToDraw(0);
    context.restore();
}

