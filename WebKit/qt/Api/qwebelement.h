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
}

class QWebFrame;
class QWebElementCollection;
class QWebElementPrivate;

class QWEBKIT_EXPORT QWebElement
{
public:
    QWebElement();
    QWebElement(const QWebElement &);
    QWebElement &operator=(const QWebElement &);
    ~QWebElement();

    bool operator==(const QWebElement& o) const;
    bool operator!=(const QWebElement& o) const;

    bool isNull() const;

    QWebElementCollection findAll(const QString &query) const;
    QWebElement findFirst(const QString &query) const;

    void setPlainText(const QString &text);
    QString toPlainText() const;

    enum XmlScope {
        InnerXml,
        OuterXml
    };

    void setXml(XmlScope scope, const QString &markup);
    QString toXml(XmlScope scope) const;

    void setAttribute(const QString &name, const QString &value);
    void setAttributeNS(const QString &namespaceUri, const QString &name, const QString &value);
    QString attribute(const QString &name, const QString &defaultValue = QString()) const;
    QString attributeNS(const QString &namespaceUri, const QString &name, const QString &defaultValue = QString()) const;
    bool hasAttribute(const QString &name) const;
    bool hasAttributeNS(const QString &namespaceUri, const QString &name) const;
    void removeAttribute(const QString &name);
    void removeAttributeNS(const QString &namespaceUri, const QString &name);
    bool hasAttributes() const;

    QStringList classes() const;
    bool hasClass(const QString &name) const;
    void addClass(const QString &name);
    void removeClass(const QString &name);
    void toggleClass(const QString &name);
    void toggleClass(const QString &name, bool enabled);

    QRect geometry() const;

    QString tagName() const;
    QString prefix() const;
    QString localName() const;
    QString namespaceURI() const;

    QWebElement parent() const;
    QWebElement firstChild(const QString &tagName = QString()) const;
    QWebElement lastChild(const QString &tagName = QString()) const;
    QWebElement nextSibling(const QString &tagName = QString()) const;
    QWebElement previousSibling(const QString &tagName = QString()) const;
    QWebElement document() const;
    QWebFrame *webFrame() const;

    // TODO: Add QWebElementCollection overloads
    void append(const QString &markup);
    void append(QWebElement element);

    void prepend(const QString &markup);
    void prepend(QWebElement element);

    void insertBefore(const QString &markup);
    void insertBefore(QWebElement element);

    void insertAfter(const QString &markup);
    void insertAfter(QWebElement element);

    void wrap(const QString &markup);
    void wrap(QWebElement element);

    void replaceWith(const QString &markup);
    void replaceWith(QWebElement element);

    QWebElement clone();
    QWebElement &remove();
    void clear();

    QVariant callScriptFunction(const QString &name, const QVariantList &arguments = QVariantList());
    QStringList scriptFunctions() const;

    QVariant scriptProperty(const QString &name) const;
    void setScriptProperty(const QString &name, const QVariant &value);
    QStringList scriptProperties() const;

    QString styleProperty(const QString &name) const;
    void setStyleProperty(const QString &name, const QString &value);

    QString computedStyleProperty(const QString &name) const;

private:
    QWebElement(WebCore::Element *domElement);

    friend class QWebFrame;
    friend class QWebElementCollection;
    friend class QWebHitTestResult;

    QWebElementPrivate *d;
    WebCore::Element *m_element;
};

class QWebElementCollectionPrivate;

class QWEBKIT_EXPORT QWebElementCollection
{
public:
    QWebElementCollection();
    QWebElementCollection(const QWebElement &contextElement, const QString &query);
    QWebElementCollection(const QWebElementCollection &);
    QWebElementCollection &operator=(const QWebElementCollection &);
    ~QWebElementCollection();

    QWebElementCollection operator+(const QWebElementCollection &other) const;
    inline QWebElementCollection &operator+=(const QWebElementCollection &other)
    {
        append(other); return *this;
    }

    void append(const QWebElementCollection &collection);

    int count() const;
    QWebElement at(int i) const;

    inline QWebElement first() const { return at(0); }
    inline QWebElement last() const { return at(count() - 1); }

    QList<QWebElement> toList() const;

    class const_iterator {
       public:
           int i;
           const QWebElementCollection *s;

           inline const_iterator(const QWebElementCollection *collection, int index) : i(index), s(collection) {}
           inline const_iterator(const const_iterator &o) : i(o.i), s(o.s) {}

           inline const QWebElement operator*() const { return s->at(i); }

           inline bool operator==(const const_iterator& o) const { return i == o.i && s == o.s; }
           inline bool operator!=(const const_iterator& o) const { return i != o.i || s != o.s; }
           inline bool operator<(const const_iterator& o) const { return i < o.i; }
           inline bool operator<=(const const_iterator& o) const { return i <= o.i; }
           inline bool operator>(const const_iterator& o) const { return i > o.i; }
           inline bool operator>=(const const_iterator& o) const { return i >= o.i; }

           inline const_iterator &operator++() { ++i; return *this; }
           inline const_iterator operator++(int) { const_iterator n(s, i); ++i; return n; }
           inline const_iterator &operator--() { i--; return *this; }
           inline const_iterator operator--(int) { const_iterator n(s, i); i--; return n; }
           inline const_iterator &operator+=(int j) { i += j; return *this; }
           inline const_iterator &operator-=(int j) { i -= j; return *this; }
           inline const_iterator operator+(int j) const { return const_iterator(s, i + j); }
           inline const_iterator operator-(int j) const { return const_iterator(s, i - j); }
           inline int operator-(const_iterator j) const { return i - j.i; }
       private:
            inline const_iterator() : i(0), s(0) {}
    };
    friend class const_iterator;

    inline const_iterator begin() const { return const_iterator(this, 0); }
    inline const_iterator end() const { return const_iterator(this, count()); }
    inline const QWebElement operator[](int i) const { return at(i); }

private:
    QExplicitlySharedDataPointer<QWebElementCollectionPrivate> d;
};

#endif // QWEBELEMENT_H
