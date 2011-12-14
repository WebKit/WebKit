/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

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

#include "qscriptvalueiterator.h"

#include "qscriptvalue_p.h"
#include "qscriptvalueiterator_p.h"

/*!
    \class QScriptValueIterator

    \brief The QScriptValueIterator class provides a Java-style iterator for QScriptValue.

    \ingroup script


    The QScriptValueIterator constructor takes a QScriptValue as
    argument.  After construction, the iterator is located at the very
    beginning of the sequence of properties. Here's how to iterate over
    all the properties of a QScriptValue:

    \snippet doc/src/snippets/code/src_script_qscriptvalueiterator.cpp 0

    The next() advances the iterator. The name(), value() and flags()
    functions return the name, value and flags of the last item that was
    jumped over.

    If you want to remove properties as you iterate over the
    QScriptValue, use remove(). If you want to modify the value of a
    property, use setValue().

    Note that QScriptValueIterator only iterates over the QScriptValue's
    own properties; i.e. it does not follow the prototype chain. You can
    use a loop like this to follow the prototype chain:

    \snippet doc/src/snippets/code/src_script_qscriptvalueiterator.cpp 1

    Note that QScriptValueIterator will not automatically skip over
    properties that have the QScriptValue::SkipInEnumeration flag set;
    that flag only affects iteration in script code.  If you want, you
    can skip over such properties with code like the following:

    \snippet doc/src/snippets/code/src_script_qscriptvalueiterator.cpp 2

    \sa QScriptValue::property()
*/

/*!
    Constructs an iterator for traversing \a object. The iterator is
    set to be at the front of the sequence of properties (before the
    first property).
*/
QScriptValueIterator::QScriptValueIterator(const QScriptValue& object)
    : d_ptr(new QScriptValueIteratorPrivate(QScriptValuePrivate::get(object)))
{}

/*!
    Destroys the iterator.
*/
QScriptValueIterator::~QScriptValueIterator()
{}

/*!
    Returns true if there is at least one item ahead of the iterator
    (i.e. the iterator is \e not at the back of the property sequence);
    otherwise returns false.

    \sa next(), hasPrevious()
*/
bool QScriptValueIterator::hasNext() const
{
    return d_ptr->hasNext();
}

/*!
    Advances the iterator by one position.

    Calling this function on an iterator located at the back of the
    container leads to undefined results.

    \sa hasNext(), previous(), name()
*/
void QScriptValueIterator::next()
{
    d_ptr->next();
}

/*!
    Returns true if there is at least one item behind the iterator
    (i.e. the iterator is \e not at the front of the property sequence);
    otherwise returns false.

    \sa previous(), hasNext()
*/
bool QScriptValueIterator::hasPrevious() const
{
    return d_ptr->hasPrevious();
}

/*!
    Moves the iterator back by one position.

    Calling this function on an iterator located at the front of the
    container leads to undefined results.

    \sa hasPrevious(), next(), name()
*/
void QScriptValueIterator::previous()
{
    d_ptr->previous();
}

/*!
    Moves the iterator to the front of the QScriptValue (before the
    first property).

    \sa toBack(), next()
*/
void QScriptValueIterator::toFront()
{
    d_ptr->toFront();
}

/*!
    Moves the iterator to the back of the QScriptValue (after the
    last property).

    \sa toFront(), previous()
*/
void QScriptValueIterator::toBack()
{
    d_ptr->toBack();
}

/*!
    Returns the name of the last property that was jumped over using
    next() or previous().

    \sa value(), flags()
*/
QString QScriptValueIterator::name() const
{
    return d_ptr->name();
}

/*!
    Returns the name of the last property that was jumped over using
    next() or previous().
*/
QScriptString QScriptValueIterator::scriptName() const
{
    return QScriptStringPrivate::get(d_ptr->scriptName());
}

/*!
    Returns the value of the last property that was jumped over using
    next() or previous().

    \sa setValue(), name()
*/
QScriptValue QScriptValueIterator::value() const
{
    return QScriptValuePrivate::get(d_ptr->value());
}

/*!
    Sets the \a value of the last property that was jumped over using
    next() or previous().

    \sa value(), name()
*/
void QScriptValueIterator::setValue(const QScriptValue& value)
{
    d_ptr->setValue(QScriptValuePrivate::get(value));
}

/*!
    Removes the last property that was jumped over using next()
    or previous().

    \sa setValue()
*/
void QScriptValueIterator::remove()
{
    d_ptr->remove();
}

/*!
    Returns the flags of the last property that was jumped over using
    next() or previous().

    \sa value()
*/
QScriptValue::PropertyFlags QScriptValueIterator::flags() const
{
    return d_ptr->flags();
}

/*!
    Makes the iterator operate on \a object. The iterator is set to be
    at the front of the sequence of properties (before the first
    property).
*/
QScriptValueIterator& QScriptValueIterator::operator=(QScriptValue& object)
{
    d_ptr = new QScriptValueIteratorPrivate(QScriptValuePrivate::get(object));
    return *this;
}
