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
#include "CSSParser.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "FrameView.h"
#include "HTMLElement.h"
#include "JSGlobalObject.h"
#include "JSHTMLElement.h"
#include "JSObject.h"
#include "NodeList.h"
#include "PropertyNameArray.h"
#include "ScriptFunctionCall.h"
#include "StaticNodeList.h"
#include "qt_runtime.h"
#include "qwebframe.h"
#include "qwebframe_p.h"
#include "runtime_root.h"
#include <wtf/Vector.h>

using namespace WebCore;

class QWebElementPrivate
{
public:
};

/*!
    \class QWebElement
    \since 4.6
    \brief The QWebElement class provides convenience access to DOM elements in a QWebFrame.
    \preliminary

    QWebElement is the main class to provide easy access to the tree structure of DOM elements.
    The underlying data is explicitly shared. Creating a copy of a QWebElement object does not
    create a copy of the underlying element, both instances point to the same element.

    The element's attributes can be read using attribute() and changed using setAttribute().

    The content of the child elements can be converted to plain text using toPlainText() and to 
    x(html) using toXml(), and it is possible to replace the content using setPlainText() and setXml().

    Depending on the type of the underlying element there may be extra functionality available, not
    covered through QWebElement's API. For example a HTML form element can be triggered to submit the
    entire form. These list of these functions is available through functions() and they can be called
    directly using callFunction().
*/

/*!
    Constructs a null web element.
*/
QWebElement::QWebElement()
    : d(0), m_element(0)
{
}

/*!
    \internal
*/
QWebElement::QWebElement(WebCore::Element *domElement)
    : d(0), m_element(domElement)
{
    if (m_element)
        m_element->ref();
}

/*!
    Constructs a copy of \a other.
*/
QWebElement::QWebElement(const QWebElement &other)
    : d(0), m_element(other.m_element)
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
    Destroys the element. The underlying DOM element is not destroyed.
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
    Returns true if the element is a null element; false otherwise.
*/
bool QWebElement::isNull() const
{
    return !m_element;
}

/*!
    Returns a new collection of elements that are children of this element
    and that match the given CSS selector \a query.
*/
QWebElementCollection QWebElement::findAll(const QString &query) const
{
    return QWebElementCollection(*this, query);
}

/*!
    Returns the first child element that matches the given CSS selector \a query.
*/
QWebElement QWebElement::findFirst(const QString &query) const
{
    if (!m_element)
        return QWebElement();
    ExceptionCode exception = 0; // ###
    return QWebElement(m_element->querySelector(query, exception).get());
}

/*!
    Replaces the existing content of this element with \a text.

    This is equivalent to setting the HTML innerText property.
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
*/
QString QWebElement::toPlainText() const
{
    if (!m_element || !m_element->isHTMLElement())
        return QString();
    return static_cast<HTMLElement*>(m_element)->innerText();
}

/*!
    Replaces the existing content of this element with \a markup.
    The string may contain HTML or XML tags, which is parsed and formatted
    before insertion into the document.

    If \a scope is InnerXml this is equivalent to setting the HTML innerHTML
    property, and similarily for OuterXml.

    \note This is currently only implemented for (X)HTML elements.
*/
void QWebElement::setXml(XmlScope scope, const QString &markup)
{
    if (!m_element || !m_element->isHTMLElement())
        return;

    ExceptionCode exception = 0;

    switch (scope) {
    case InnerXml:
        static_cast<HTMLElement*>(m_element)->setInnerHTML(markup, exception);
        break;
    case OuterXml:
        static_cast<HTMLElement*>(m_element)->setOuterHTML(markup, exception);
        break;
    }
}

/*!
    Returns the XML between the start and the end tag of this
    element.

    If \a scope is InnerXml this is equivalent to reading the HTML
    innerHTML property, and similarily for OuterXml.

    \note This is currently only implemented for (X)HTML elements.
*/
QString QWebElement::toXml(XmlScope scope) const
{
    if (!m_element || !m_element->isHTMLElement())
        return QString();

    switch (scope) {
    case InnerXml:
        return static_cast<HTMLElement*>(m_element)->innerHTML();
    case OuterXml:
        return static_cast<HTMLElement*>(m_element)->outerHTML();
    }
}

/*!
    Adds an attribute called \a name with the value \a value. If an attribute
    with the same name exists, its value is replaced by \a value.
*/
void QWebElement::setAttribute(const QString &name, const QString &value)
{
    if (!m_element)
        return;
    ExceptionCode exception = 0;
    m_element->setAttribute(name, value, exception);
}

/*!
    Adds an attribute called \a name in the namespace described with \a namespaceUri
    with the value \a value. If an attribute with the same name exists, its value is
    replaced by \a value.
*/
void QWebElement::setAttributeNS(const QString &namespaceUri, const QString &name, const QString &value)
{
    if (!m_element)
        return;
    WebCore::ExceptionCode exception = 0;
    m_element->setAttributeNS(namespaceUri, name, value, exception);
}

/*!
    Returns the attributed called \a name. If the attribute does not exist \a defaultValue is
    returned.
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
    Returns the attributed called \a name in the namespace described with \a namespaceUri.
    If the attribute does not exist \a defaultValue is returned.
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
    Returns true if this element has an attribute called \a name; otherwise returns false.
*/
bool QWebElement::hasAttribute(const QString &name) const
{
    if (!m_element)
        return false;
    return m_element->hasAttribute(name);
}

/*!
    Returns true if this element has an attribute called \a name in the namespace described
    with \a namespaceUri; otherwise returns false.
*/
bool QWebElement::hasAttributeNS(const QString &namespaceUri, const QString &name) const
{
    if (!m_element)
        return false;
    return m_element->hasAttributeNS(namespaceUri, name);
}

/*!
    Removes the attribute called \a name from this element.
*/
void QWebElement::removeAttribute(const QString &name)
{
    if (!m_element)
        return;
    ExceptionCode exception = 0;
    m_element->removeAttribute(name, exception);
}

/*!
    Removes the attribute called \a name in the namespace described with \a namespaceUri
    from this element.
*/
void QWebElement::removeAttributeNS(const QString &namespaceUri, const QString &name)
{
    if (!m_element)
        return;
    WebCore::ExceptionCode exception = 0;
    m_element->removeAttributeNS(namespaceUri, name, exception);
}

/*!
    Returns true if the element has any attributes defined; otherwise returns false;
*/
bool QWebElement::hasAttributes() const
{
    if (!m_element)
        return false;
    return m_element->hasAttributes();
}

/*!
    Returns the geometry of this element, relative to its containing frame.
*/
QRect QWebElement::geometry() const
{
    if (!m_element)
        return QRect();
    return m_element->getRect();
}

/*!
    Returns the tag name of this element.
*/
QString QWebElement::tagName() const
{
    if (!m_element)
        return QString();
    return m_element->tagName();
}

/*!
    Returns the namespace prefix of the element or an empty string if the element has no namespace prefix.
*/
QString QWebElement::prefix() const
{
    if (!m_element)
        return QString();
    return m_element->prefix();
}

/*!
    If the element uses namespaces, this function returns the local name of the element;
    otherwise it returns an empty string.
*/
QString QWebElement::localName() const
{
    if (!m_element)
        return QString();
    return m_element->localName();
}

/*!
    Returns the namespace URI of this element or an empty string if the element has no namespace URI.
*/
QString QWebElement::namespaceURI() const
{
    if (!m_element)
        return QString();
    return m_element->namespaceURI();
}

/*!
    Returns the parent element of this element.
*/
QWebElement QWebElement::parent() const
{
    if (m_element)
        return QWebElement(m_element->parentElement());
    return QWebElement();
}

/*!
    Returns the first child element of this element with tag name
    \a tagName if \a tagName is non-empty; otherwise returns the
    first child element. Returns a null element if no such child exists.

    \sa lastChild() previousSibling() nextSibling()
*/
QWebElement QWebElement::firstChild(const QString &tagName) const
{
    if (!m_element)
        return QWebElement();
    String tag = tagName;
    for (Node* child = m_element->firstChild(); child; child = child->nextSibling()) {
        if (!child->isElementNode())
            continue;
        Element* e = static_cast<Element*>(child);
        if (tagName.isEmpty() || equalIgnoringCase(e->tagName(), tag))
            return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the last child element of this element with tag name
    \a tagName if \a tagName is non-empty; otherwise returns the
    last child element. Returns a null element if no such child exists.

    \sa firstChild() previousSibling() nextSibling()
*/
QWebElement QWebElement::lastChild(const QString &tagName) const
{
    if (!m_element)
        return QWebElement();
    String tag = tagName;
    for (Node* child = m_element->lastChild(); child; child = child->previousSibling()) {
        if (!child->isElementNode())
            continue;
        Element* e = static_cast<Element*>(child);
        if (tagName.isEmpty() || equalIgnoringCase(e->tagName(), tag))
            return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the next sibling element of this element with tag name
    \a tagName if \a tagName is non-empty; otherwise returns any next sibling
    element. Returns a null element if no such sibling exists.

    \sa firstChild() previousSibling() lastChild()
*/
QWebElement QWebElement::nextSibling(const QString &tagName) const
{
    if (!m_element)
        return QWebElement();
    String tag = tagName;
    for (Node* sib = m_element->nextSibling(); sib; sib = sib->nextSibling()) {
        if (!sib->isElementNode())
            continue;
        Element* e = static_cast<Element*>(sib);
        if (tagName.isEmpty() || equalIgnoringCase(e->tagName(), tag))
            return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the previous sibling element of this element with tag name
    \a tagName if \a tagName is non-empty; otherwise returns any previous sibling
    element. Returns a null element if no such sibling exists.

    \sa firstChild() nextSibling() lastChild()
*/
QWebElement QWebElement::previousSibling(const QString &tagName) const
{
    if (!m_element)
        return QWebElement();
    String tag = tagName;
    for (Node* sib = m_element->previousSibling(); sib; sib = sib->previousSibling()) {
        if (!sib->isElementNode())
            continue;
        Element* e = static_cast<Element*>(sib);
        if (tagName.isEmpty() || equalIgnoringCase(e->tagName(), tag))
            return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the document this element belongs to.
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
    Returns the web frame this elements is a part of. If the element is
    a null element null is returned.
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

static bool setupScriptObject(WebCore::Element* element, ScriptObject& object, ScriptState*& state, ScriptController*& scriptController)
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

    state = scriptController->globalObject()->globalExec();

    JSC::JSValuePtr thisValue = toJS(state, element);
    if (!thisValue)
        return false;

    JSC::JSObject* thisObject = thisValue.toObject(state);
    if (!thisObject)
        return false;

    object = ScriptObject(thisObject);
    return true;
}

/*!
    Calls the function with the given \a name and \a arguments.

    The underlying DOM element that QWebElement wraps may have dedicated functions depending
    on its type. For example a form element can have the "submit" function, that would submit
    the form to the destination specified in the HTML.

    \sa scriptFunctions()
*/
QVariant QWebElement::callScriptFunction(const QString &name, const QVariantList &arguments)
{
    ScriptState* state = 0;
    ScriptObject thisObject;
    ScriptController* scriptController = 0;

    if (!setupScriptObject(m_element, thisObject, state, scriptController))
        return QVariant();

    ScriptFunctionCall functionCall(state, thisObject, name);

    for (QVariantList::ConstIterator it = arguments.constBegin(), end = arguments.constEnd();
         it != end; ++it)
        functionCall.appendArgument(JSC::Bindings::convertQVariantToValue(state, scriptController->bindingRootObject(), *it));

    bool hadException = false;
    ScriptValue result = functionCall.call(hadException);
    if (hadException)
        return QVariant();

    int distance = 0;
    return JSC::Bindings::convertValueToQVariant(state, result.jsValue(), QMetaType::Void, &distance);
}

/*!
    Returns a list of function names this element supports.

    The function names returned are the same functions that are callable from the DOM
    element's JavaScript binding.

    \sa callScriptFunction()
*/
QStringList QWebElement::scriptFunctions() const
{
    ScriptState* state = 0;
    ScriptObject thisObject;
    ScriptController* scriptController = 0;

    if (!setupScriptObject(m_element, thisObject, state, scriptController))
        return QStringList();

    JSC::JSObject* object = thisObject.jsObject();
    if (!object)
        return QStringList();

    QStringList names;

    // Enumerate the contents of the object
    JSC::PropertyNameArray properties(state);
    object->getPropertyNames(state, properties);
    for (JSC::PropertyNameArray::const_iterator it = properties.begin();
         it != properties.end(); ++it) {

        JSC::JSValuePtr property = object->get(state, *it);
        if (!property)
            continue;

        JSC::JSObject* function = property.toObject(state);
        if (!function)
            continue;

        JSC::CallData callData;
        JSC::CallType callType = function->getCallData(callData);
        if (callType == JSC::CallTypeNone)
            continue;

        JSC::UString ustring = (*it).ustring();
        names << QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size());
    }

    if (state->hadException())
        state->clearException();

    return names;
}

/*!
    Returns the value of the element's \a name property.

    If no such property exists, the returned variant is invalid.

    The return property has the same value as the corresponding property
    in the element's JavaScript binding with the same name.

    Information about all available properties is provided through scriptProperties().

    \sa setScriptProperty(), scriptProperties()
*/
QVariant QWebElement::scriptProperty(const QString &name) const
{
    ScriptState* state = 0;
    ScriptObject thisObject;
    ScriptController *scriptController = 0;

    if (!setupScriptObject(m_element, thisObject, state, scriptController))
        return QVariant();

    String wcName(name);
    JSC::JSValuePtr property = thisObject.jsObject()->get(state, JSC::Identifier(state, wcName));

    // ###
    if (state->hadException())
        state->clearException();

    int distance = 0;
    return JSC::Bindings::convertValueToQVariant(state, property, QMetaType::Void, &distance);
}

/*!
    Sets the value of the element's \a name property to \a value.

    Information about all available properties is provided through scriptProperties().

    Setting the property will affect the corresponding property
    in the element's JavaScript binding with the same name.

    \sa scriptProperty(), scriptProperties()
*/
void QWebElement::setScriptProperty(const QString &name, const QVariant &value)
{
    ScriptState* state = 0;
    ScriptObject thisObject;
    ScriptController* scriptController = 0;

    if (!setupScriptObject(m_element, thisObject, state, scriptController))
        return;

    JSC::JSValuePtr jsValue = JSC::Bindings::convertQVariantToValue(state, scriptController->bindingRootObject(), value);
    if (!jsValue)
        return;

    String wcName(name);
    JSC::PutPropertySlot slot;
    thisObject.jsObject()->put(state, JSC::Identifier(state, wcName), jsValue, slot);
    if (state->hadException())
        state->clearException();
}

/*!
    Returns a list of property names this element supports.

    The function names returned are the same properties that are accessible from the DOM
    element's JavaScript binding.
*/
QStringList QWebElement::scriptProperties() const
{
    if (!m_element)
        return QStringList();

    Document* document = m_element->document();
    if (!document)
        return QStringList();

    Frame* frame = document->frame();
    if (!frame)
        return QStringList();

    ScriptController* script = frame->script();
    JSC::ExecState* exec = script->globalObject()->globalExec();

    JSC::JSValuePtr thisValue = toJS(exec, m_element);
    if (!thisValue)
        return QStringList();

    JSC::JSObject* object = thisValue.toObject(exec);
    if (!object)
        return QStringList();

    QStringList names;

    // Enumerate the contents of the object
    JSC::PropertyNameArray properties(exec);
    object->getPropertyNames(exec, properties);
    for (JSC::PropertyNameArray::const_iterator it = properties.begin();
         it != properties.end(); ++it) {

        JSC::JSValuePtr property = object->get(exec, *it);
        if (!property)
            continue;

        JSC::JSObject* function = property.toObject(exec);
        if (!function)
            continue;

        JSC::CallData callData;
        JSC::CallType callType = function->getCallData(callData);
        if (callType != JSC::CallTypeNone)
            continue;

        JSC::UString ustring = (*it).ustring();
        names << QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size());
    }

    if (exec->hadException())
        exec->clearException();

    return names;
}

/*!
    Returns the value of the style named \a name or an empty string if the style has no such name.
*/
QString QWebElement::styleProperty(const QString &name) const
{
    if (!m_element || !m_element->isStyledElement())
        return QString();

    int propID = cssPropertyID(name);
    CSSStyleDeclaration* style = static_cast<StyledElement*>(m_element)->style();
    if (!propID || !style)
        return QString();

    // TODO: use computed style for fallback

    return style->getPropertyValue(propID);
}

/*!
    Sets the value of the style named \a name to \a value.
*/
void QWebElement::setStyleProperty(const QString &name, const QString &value)
{
    if (!m_element || !m_element->isStyledElement())
        return;

    int propID = cssPropertyID(name);
    CSSStyleDeclaration* style = static_cast<StyledElement*>(m_element)->style();
    if (!propID || !style)
        return;

    // TODO: what about computed style?

    ExceptionCode exception = 0;
    style->setProperty(name, value, exception);
}

/*!
    Returns the list of classes of this element.
*/
QStringList QWebElement::classes() const
{
    if (!hasAttribute("class"))
        return QStringList();

    QStringList classes =  attribute("class").simplified().split(' ', QString::SkipEmptyParts);
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
    Returns true if this element has a class called \a name; otherwise returns false.
*/
bool QWebElement::hasClass(const QString &name) const
{
    QStringList list = classes();
    return list.contains(name);
}

/*!
    Adds the specified class \a name to the element.
*/
void QWebElement::addClass(const QString &name)
{
    QStringList list = classes();
    if (!list.contains(name)) {
        list.append(name);
        QString value = list.join(" ");
        setAttribute("class", value);
    }
}

/*!
    Removes the specified class \a name from the element.
*/
void QWebElement::removeClass(const QString &name)
{
    QStringList list = classes();
    if (list.contains(name)) {
        list.removeAll(name);
        QString value = list.join(" ");
        setAttribute("class", value);
    }
}

/*!
    Adds the specified class \a name if it is not present,
    removes it if it is already present.
*/
void QWebElement::toggleClass(const QString &name)
{
    QStringList list = classes();
    if (list.contains(name))
        list.removeAll(name);
    else
        list.append(name);

    QString value = list.join(" ");
    setAttribute("class", value);
}

/*!
    Adds the specified class \a name if \a enabled is true or
    removes it if \a enabled is false.
*/
void QWebElement::toggleClass(const QString &name, bool enabled)
{
    QStringList list = classes();
    if (list.contains(name)) {
        if (!enabled) {
            list.removeAll(name);
            QString value = list.join(" ");
            setAttribute("class", value);
        }
    } else {
        if (enabled) {
            list.append(name);
            QString value = list.join(" ");
            setAttribute("class", value);
        }
    }
}

/*!
    Appends \a element as the element's last child.

    If \a element is the child of another element, it is re-parented
    to this element. If \a element is a child of this element, then
    its position in the list of children is changed.

    Calling this function on a null element does nothing.

    \sa prepend(), insertBefore(), insertAfter()
*/
void QWebElement::append(QWebElement element)
{
    if (!m_element || element.isNull())
        return;

    ExceptionCode exception = 0;
    m_element->appendChild(element.m_element, exception);
}

/*!
    Appends the result of parsing \a html as the element's last child.

    Calling this function on a null element does nothing.

    \sa prepend(), insertBefore(), insertAfter()
*/
void QWebElement::append(const QString &html)
{
    if (!m_element)
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(html);

    ExceptionCode exception = 0;
    m_element->appendChild(fragment, exception);
}

/*!
    Prepends \a element as the element's first child.

    If \a element is the child of another element, it is re-parented
    to this element. If \a element is a child of this element, then
    its position in the list of children is changed.

    Calling this function on a null element does nothing.

    \sa append(), insertBefore(), insertAfter()
*/
void QWebElement::prepend(QWebElement element)
{
    if (!m_element || element.isNull())
        return;

    ExceptionCode exception = 0;
    m_element->insertBefore(element.m_element, m_element->firstChild(), exception);
}

/*!
    Prepends the result of parsing \a html as the element's first child.

    Calling this function on a null element does nothing.

    \sa append(), insertBefore(), insertAfter()
*/
void QWebElement::prepend(const QString &html)
{
    if (!m_element)
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(html);

    ExceptionCode exception = 0;
    m_element->insertBefore(fragment, m_element->firstChild(), exception);
}


/*!
    Inserts \a element before this element.

    If \a element is the child of another element, it is re-parented
    to the parent of this element.

    Calling this function on a null element does nothing.

    \sa append(), prepend(), insertAfter()
*/
void QWebElement::insertBefore(QWebElement element)
{
    if (!m_element || element.isNull())
        return;

    if (!m_element->parent())
        return;

    ExceptionCode exception = 0;
    m_element->parent()->insertBefore(element.m_element, m_element, exception);
}

/*!
    Inserts the result of parsing \a html before this element.

    Calling this function on a null element does nothing.

    \sa append(), prepend(), insertAfter()
*/
void QWebElement::insertBefore(const QString &html)
{
    if (!m_element)
        return;

    if (!m_element->parent())
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(html);

    ExceptionCode exception = 0;
    m_element->parent()->insertBefore(fragment, m_element, exception);
}

/*!
    Inserts \a element after this element.

    If \a element is the child of another element, it is re-parented
    to the parent of this element.

    Calling this function on a null element does nothing.

    \sa append(), prepend(), insertBefore()
*/
void QWebElement::insertAfter(QWebElement element)
{
    if (!m_element || element.isNull())
        return;

    if (!m_element->parent())
        return;

    if (!m_element->nextSibling())
        return;

    ExceptionCode exception = 0;
    m_element->parent()->insertBefore(element.m_element, m_element->nextSibling(), exception);
}

/*!
    Inserts the result of parsing \a html after this element.

    Calling this function on a null element does nothing.

    \sa append(), prepend(), insertBefore()
*/
void QWebElement::insertAfter(const QString &html)
{
    if (!m_element)
        return;

    if (!m_element->parent())
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(html);

    ExceptionCode exception = 0;
    m_element->parent()->insertBefore(fragment, m_element->nextSibling(), exception);
}

/*!
    Returns a clone of this element.

    The clone may be inserted at any point in the document.

    \sa append(), prepend(), insertBefore(), insertAfter()
*/
QWebElement QWebElement::clone()
{
    if (!m_element)
        return QWebElement();

    return QWebElement(m_element->cloneElementWithChildren().get());
}

/*!
    Removes this element from the document.

    The element is still valid after removal, and can be inserted into
    other parts of the document.

    \sa clear()
*/
QWebElement &QWebElement::remove()
{
    if (!m_element)
        return *this;

    ExceptionCode exception = 0;
    m_element->remove(exception);

    return *this;
}

/*!
    Removes all children from this element.

    \sa remove()
*/
void QWebElement::clear()
{
    if (!m_element)
        return;

    m_element->removeAllChildren();
}

/*!
    Wraps this element in \a element as the last child.

    \sa replaceWith()
*/
void QWebElement::wrap(QWebElement element)
{
    if (!m_element || element.isNull())
        return;

    insertAfter(element);
    element.append(*this);
}

/*!
    Wraps this element in the result of parsing \a html,
    as the last child.

    \sa replaceWith()
*/
void QWebElement::wrap(const QString &html)
{
    if (!m_element)
        return;

    if (!m_element->parent())
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(html);

    if (!fragment || !fragment->firstChild())
        return;

    // Keep reference to these two nodes before pulling out this element and
    // wrapping it in the fragment. The reason for doing it in this order is
    // that once the fragment has been added to the document it is empty, so
    // we no longer have access to the nodes it contained.
    Node* parentNode = m_element->parent();
    Node* siblingNode = m_element->nextSibling();

    ExceptionCode exception = 0;
    fragment->firstChild()->appendChild(m_element, exception);
    parentNode->insertBefore(fragment, siblingNode, exception);
}

/*!
    Replaces this element with \a element.

    It is not possible to replace the <html>, <head>, or <body>
    elements using this method.

    \sa wrap()
*/
void QWebElement::replaceWith(QWebElement element)
{
    if (!m_element || element.isNull())
        return;

    insertAfter(element);
    remove();
}

/*!
    Replaces this element with the result of parsing \a html.

    It is not possible to replace the <html>, <head>, or <body>
    elements using this method.

    \sa wrap()
*/
void QWebElement::replaceWith(const QString &html)
{
    if (!m_element)
        return;

    insertAfter(html);
    remove();
}

/*!
    \fn inline bool QWebElement::operator==(const QWebElement& o) const;

    Returns true if this element points to the same underlying DOM object than \a o; otherwise returns false.
*/

/*!
    \fn inline bool QWebElement::operator!=(const QWebElement& o) const;

    Returns true if this element points to a different underlying DOM object than \a o; otherwise returns false.
*/

class QWebElementCollectionPrivate : public QSharedData
{
public:
    static QWebElementCollectionPrivate* create(const PassRefPtr<Node> &context, const QString &query);

    RefPtr<NodeList> m_result;

private:
    inline QWebElementCollectionPrivate() {}
};

QWebElementCollectionPrivate* QWebElementCollectionPrivate::create(const PassRefPtr<Node> &context, const QString &query)
{
    if (!context)
        return 0;

    // Let WebKit do the hard work hehehe
    ExceptionCode exception = 0; // ###
    RefPtr<NodeList> nodes = context->querySelectorAll(query, exception);
    if (!nodes)
        return 0;

    QWebElementCollectionPrivate* priv = new QWebElementCollectionPrivate;
    priv->m_result = nodes;
    return priv;
}

/*!
    \class QWebElementCollection
    \since 4.6
    \brief The QWebElementCollection class represents a collection of web elements.
    \preliminary

    Elements in a document can be selected using QWebElement::findAll() or using the
    QWebElement constructor. The collection is composed by choosing all elements in the
    document that match a specified CSS selector expression.

    The number of selected elements is provided through the count() property. Individual
    elements can be retrieved by index using at().

    It is also possible to iterate through all elements in the collection using Qt's foreach
    macro:

    \code
        QWebElementCollection collection = document.findAll("p");
        foreach (QWebElement paraElement, collection) {
            ...
        }
    \endcode
*/

/*!
    Constructs an empty collection.
*/
QWebElementCollection::QWebElementCollection()
{
}

/*!
    Constructs a copy of \a other.
*/
QWebElementCollection::QWebElementCollection(const QWebElementCollection &other)
    : d(other.d)
{
}

/*!
    Constructs a collection of elements from the list of child elements of \a contextElement that
    match the specified CSS selector \a query.
*/
QWebElementCollection::QWebElementCollection(const QWebElement &contextElement, const QString &query)
{
    d = QExplicitlySharedDataPointer<QWebElementCollectionPrivate>(QWebElementCollectionPrivate::create(contextElement.m_element, query));
}

/*!
    Assigns \a other to this collection and returns a reference to this collection.
*/
QWebElementCollection &QWebElementCollection::operator=(const QWebElementCollection &other)
{
    d = other.d;
    return *this;
}

/*!
    Destroys the collection.
*/
QWebElementCollection::~QWebElementCollection()
{
}

/*! \fn QWebElementCollection &QWebElementCollection::operator+=(const QWebElementCollection &other)

    Appends the items of the \a other list to this list and returns a
    reference to this list.

    \sa operator+(), append()
*/

/*!
    Returns a collection that contains all the elements of this collection followed
    by all the elements in the \a other collection. Duplicates may occur in the result.

    \sa operator+=()
*/
QWebElementCollection QWebElementCollection::operator+(const QWebElementCollection &other) const
{
    QWebElementCollection n = *this; n.d.detach(); n += other; return n;
}

/*!
    Extends the collection by appending all items of \a other.

    The resulting collection may include duplicate elements.

    \sa operator+=()
*/
void QWebElementCollection::append(const QWebElementCollection &other)
{
    if (!d) {
        *this = other;
        return;
    }
    if (!other.d)
        return;
    Vector<RefPtr<Node> > nodes;
    RefPtr<NodeList> results[] = { d->m_result, other.d->m_result };
    nodes.reserveInitialCapacity(results[0]->length() + results[1]->length());

    for (int i = 0; i < 2; ++i) {
        int j = 0;
        Node* n = results[i]->item(j);
        while (n) {
            nodes.append(n);
            n = results[i]->item(++j);
        }
    }

    d->m_result = StaticNodeList::adopt(nodes);
}

/*!
    Returns the number of elements in the collection.
*/
int QWebElementCollection::count() const
{
    if (!d)
        return 0;
    return d->m_result->length();
}

/*!
    Returns the element at index position \a i in the collection.
*/
QWebElement QWebElementCollection::at(int i) const
{
    if (!d)
        return QWebElement();
    Node* n = d->m_result->item(i);
    return QWebElement(static_cast<Element*>(n));
}

/*!
    \fn const QWebElement QWebElementCollection::operator[](int position) const

    Returns the element at the specified \a position in the collection.
*/

/*! \fn QWebElement QWebElementCollection::first() const

    Returns the first element in the collection.

    \sa last(), operator[](), at(), count()
*/

/*! \fn QWebElement QWebElementCollection::last() const

    Returns the last element in the collection.

    \sa first(), operator[](), at(), count()
*/

/*!
    Returns a QList object with the elements contained in this collection.
*/
QList<QWebElement> QWebElementCollection::toList() const
{
    if (!d)
        return QList<QWebElement>();
    QList<QWebElement> elements;
    int i = 0;
    Node* n = d->m_result->item(i);
    while (n) {
        if (n->isElementNode())
            elements.append(QWebElement(static_cast<Element*>(n)));
        n = d->m_result->item(++i);
    }
    return elements;
}

/*!
    \fn QWebElementCollection::const_iterator QWebElementCollection::begin() const

    Returns an STL-style iterator pointing to the first element in the collection.

    \sa end()
*/

/*!
    \fn QWebElementCollection::const_iterator QWebElementCollection::end() const

    Returns an STL-style iterator pointing to the imaginary element after the
    last element in the list.

    \sa begin()
*/

/*!
    \class QWebElementCollection::const_iterator
    \since 4.6
    \brief The QWebElementCollection::const_iterator class provides an STL-style const iterator for QWebElementCollection.

    QWebElementCollection provides STL style const iterators for fast low-level access to the elements.

    QWebElementCollection::const_iterator allows you to iterate over a QWebElementCollection.

    The default QWebElementCollection::const_iterator constructors creates an uninitialized iterator. You must initialize
    it using a QWebElementCollection function like QWebElementCollection::begin() or QWebElementCollection::end() before you
    can start iterating.
*/

/*!
    \fn QWebElementCollection::const_iterator::const_iterator()

    Constructs an uninitialized iterator.

    Functions like operator*() and operator++() should not be called on
    an uninitialized iterator. Use operator=() to assign a value
    to it before using it.

    \sa QWebElementCollection::begin()
*/

/*!
    \fn QWebElementCollection::const_iterator::const_iterator(const const_iterator &other)

    Constructs a copy of \a other.
*/

/*!
    \fn QWebElementCollection::const_iterator::const_iterator(const QWebElementCollection *collection, int index)
    \internal
*/

/*!
    \fn const QWebElement QWebElementCollection::const_iterator::operator*() const

    Returns the current element.
*/

/*!
    \fn bool QWebElementCollection::const_iterator::operator==(const const_iterator &other) const

    Returns true if \a other points to the same item as this iterator;
    otherwise returns false.

    \sa operator!=()
*/

/*!
    \fn bool QWebElementCollection::const_iterator::operator!=(const const_iterator &other) const

    Returns true if \a other points to a different element than this;
    iterator; otherwise returns false.

    \sa operator==()
*/

/*!
    \fn QWebElementCollection::const_iterator &QWebElementCollection::const_iterator::operator++()

    The prefix ++ operator (\c{++it}) advances the iterator to the next element in the collection
    and returns an iterator to the new current element.

    Calling this function on QWebElementCollection::end() leads to undefined results.

    \sa operator--()
*/

/*!
    \fn QWebElementCollection::const_iterator QWebElementCollection::const_iterator::operator++(int)

    \overload

    The postfix ++ operator (\c{it++}) advances the iterator to the next element in the collection
    and returns an iterator to the previously current element.

    Calling this function on QWebElementCollection::end() leads to undefined results.
*/

/*!
    \fn QWebElementCollection::const_iterator &QWebElementCollection::const_iterator::operator--()

    The prefix -- operator (\c{--it}) makes the preceding element current and returns an
    iterator to the new current element.

    Calling this function on QWebElementCollection::begin() leads to undefined results.

    \sa operator++()
*/

/*!
    \fn QWebElementCollection::const_iterator QWebElementCollection::const_iterator::operator--(int)

    \overload

    The postfix -- operator (\c{it--}) makes the preceding element current and returns
    an iterator to the previously current element.
*/

/*!
    \fn QWebElementCollection::const_iterator &QWebElementCollection::const_iterator::operator+=(int j)

    Advances the iterator by \a j elements. If \a j is negative, the iterator goes backward.

    \sa operator-=(), operator+()
*/

/*!
    \fn QWebElementCollection::const_iterator &QWebElementCollection::const_iterator::operator-=(int j)

    Makes the iterator go back by \a j elements. If \a j is negative, the iterator goes forward.

    \sa operator+=(), operator-()
*/

/*!
    \fn QWebElementCollection::const_iterator QWebElementCollection::const_iterator::operator+(int j) const

    Returns an iterator to the element at \a j positions forward from this iterator. If \a j
    is negative, the iterator goes backward.

    \sa operator-(), operator+=()
*/

/*!
    \fn QWebElementCollection::const_iterator QWebElementCollection::const_iterator::operator-(int j) const

    Returns an iterator to the element at \a j positiosn backward from this iterator.
    If \a j is negative, the iterator goes forward.

    \sa operator+(), operator-=()
*/

/*!
    \fn int QWebElementCollection::const_iterator::operator-(const_iterator other) const

    Returns the number of elements between the item point to by \a other
    and the element pointed to by this iterator.
*/

/*!
    \fn bool QWebElementCollection::const_iterator::operator<(const const_iterator &other) const

    Returns true if the element pointed to by this iterator is less than the element pointed to
    by the \a other iterator.
*/

/*!
    \fn bool QWebElementCollection::const_iterator::operator<=(const const_iterator &other) const

    Returns true if the element pointed to by this iterator is less than or equal to the
    element pointed to by the \a other iterator.
*/

/*!
    \fn bool QWebElementCollection::const_iterator::operator>(const const_iterator &other) const

    Returns true if the element pointed to by this iterator is greater than the element pointed to
    by the \a other iterator.
*/

/*!
    \fn bool QWebElementCollection::const_iterator::operator>=(const const_iterator &other) const

    Returns true if the element pointed to by this iterator is greater than or equal to the
    element pointed to by the \a other iterator.
*/
