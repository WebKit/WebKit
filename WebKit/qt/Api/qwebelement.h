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

#ifndef QWEBELEMENT_H
#define QWEBELEMENT_H

#include <QString>
#include <QRect>
#include <QVariant>
#include <QExplicitlySharedDataPointer>

#include "qwebkitglobal.h"
namespace WebCore {
    class Element;
    class Node;
}

class QWebFrame;
class QWebElementPrivate;

class QWEBKIT_EXPORT QWebElement {
public:
    QWebElement();
    QWebElement(const QWebElement&);
    QWebElement &operator=(const QWebElement&);
    ~QWebElement();

    bool operator==(const QWebElement& o) const;
    bool operator!=(const QWebElement& o) const;

    bool isNull() const;

    QList<QWebElement> findAll(const QString& selectorQuery) const;
    QWebElement findFirst(const QString& selectorQuery) const;

    void setPlainText(const QString& text);
    QString toPlainText() const;

    void setOuterXml(const QString& markup);
    QString toOuterXml() const;

    void setInnerXml(const QString& markup);
    QString toInnerXml() const;

    void setAttribute(const QString& name, const QString& value);
    void setAttributeNS(const QString& namespaceUri, const QString& name, const QString& value);
    QString attribute(const QString& name, const QString& defaultValue = QString()) const;
    QString attributeNS(const QString& namespaceUri, const QString& name, const QString& defaultValue = QString()) const;
    bool hasAttribute(const QString& name) const;
    bool hasAttributeNS(const QString& namespaceUri, const QString& name) const;
    void removeAttribute(const QString& name);
    void removeAttributeNS(const QString& namespaceUri, const QString& name);
    bool hasAttributes() const;

    QStringList classes() const;
    bool hasClass(const QString& name) const;
    void addClass(const QString& name);
    void removeClass(const QString& name);
    void toggleClass(const QString& name);

    QRect geometry() const;

    QString tagName() const;
    QString prefix() const;
    QString localName() const;
    QString namespaceUri() const;

    QWebElement parent() const;
    QWebElement firstChild() const;
    QWebElement lastChild() const;
    QWebElement nextSibling() const;
    QWebElement previousSibling() const;
    QWebElement document() const;
    QWebFrame *webFrame() const;

    // TODO: Add QList<QWebElement> overloads
    // docs need example snippet
    void appendInside(const QString& markup);
    void appendInside(const QWebElement& element);

    // docs need example snippet
    void prependInside(const QString& markup);
    void prependInside(const QWebElement& element);

    // docs need example snippet
    void appendOutside(const QString& markup);
    void appendOutside(const QWebElement& element);

    // docs need example snippet
    void prependOutside(const QString& markup);
    void prependOutside(const QWebElement& element);

    // docs need example snippet
    void encloseContentsWith(const QWebElement& element);
    void encloseContentsWith(const QString& markup);
    void encloseWith(const QString& markup);
    void encloseWith(const QWebElement& element);

    void replace(const QString& markup);
    void replace(const QWebElement& element);

    QWebElement clone() const;
    QWebElement& takeFromDocument();
    void removeFromDocument();
    void removeChildren();

    QVariant evaluateScript(const QString& scriptSource);

    QVariant callFunction(const QString& functionName, const QVariantList& arguments = QVariantList());
    QStringList functions() const;

    QVariant scriptableProperty(const QString& name) const;
    void setScriptableProperty(const QString& name, const QVariant& value);
    QStringList scriptableProperties() const;

    enum ResolveRule { IgnoreCascadingStyles, RespectCascadingStyles };
    QString styleProperty(const QString& name, ResolveRule = IgnoreCascadingStyles) const;

    enum StylePriority { NormalStylePriority, DeclaredStylePriority, ImportantStylePriority };
    void setStyleProperty(const QString& name, const QString& value, StylePriority = DeclaredStylePriority);

    QString computedStyleProperty(const QString& name) const;

private:
    explicit QWebElement(WebCore::Element*);
    explicit QWebElement(WebCore::Node*);

    static QWebElement enclosingElement(WebCore::Node*);

    friend class QWebFrame;
    friend class QWebHitTestResult;
    friend class QWebHitTestResultPrivate;
    friend class QWebPage;

    QWebElementPrivate* d;
    WebCore::Element* m_element;
};

#endif // QWEBELEMENT_H
