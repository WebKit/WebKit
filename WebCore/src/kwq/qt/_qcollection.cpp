/****************************************************************************
** $Id$
**
** Implementation of base class for all collection classes
**
** Created : 920820
**
** Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
**
** This file is part of the tools module of the Qt GUI Toolkit.
**
** This file may be distributed under the terms of the Q Public License
** as defined by Trolltech AS of Norway and appearing in the file
** LICENSE.QPL included in the packaging of this file.
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** Licensees holding valid Qt Enterprise Edition or Qt Professional Edition
** licenses may use this file in accordance with the Qt Commercial License
** Agreement provided with the Software.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.trolltech.com/pricing.html or email sales@trolltech.com for
**   information about Qt Commercial License Agreements.
** See http://www.trolltech.com/qpl/ for QPL licensing information.
** See http://www.trolltech.com/gpl/ for GPL licensing information.
**
** Contact info@trolltech.com if any conditions of this licensing are
** not clear to you.
**
**********************************************************************/

#include "_qcollection.h"

// NOT REVISED
/*!
  \class QCollection qcollection.h
  \brief The QCollection class is the base class of all Qt collections.

  \ingroup collection
  \ingroup tools

  The QCollection class is an abstract base class for the Qt \link
  collection.html collection classes\endlink QDict, QList etc. via QGDict,
  QGList etc.

  A QCollection knows only about the number of objects in the
  collection and the deletion strategy (see setAutoDelete()).

  A collection is implemented using the \c Item (generic collection
  item) type, which is a \c void*.  The template classes that create
  the real collections cast the \c Item to the required type.

  \sa \link collection.html Collection Classes\endlink
*/


/*! \enum QCollection::Item

  This type is the generic "item" in a QCollection.
*/


/*!
  \fn QCollection::QCollection()

  Constructs a collection. The constructor is protected because
  QCollection is an abstract class.
*/

/*!
  \fn QCollection::QCollection( const QCollection & source )

  Constructs a copy of \a source with autoDelete() set to FALSE. The
  constructor is protected because QCollection is an abstract class.

  Note that if \a source has autoDelete turned on, copying it is a
  good way to get memory leaks, reading freed memory, or both.
*/

/*!
  \fn QCollection::~QCollection()
  Destroys the collection. The destructor is protected because QCollection
  is an abstract class.
*/


/*!
  \fn bool QCollection::autoDelete() const
  Returns the setting of the auto-delete option (default is FALSE).
  \sa setAutoDelete()
*/

/*!
  \fn void QCollection::setAutoDelete( bool enable )

  Sets the auto-delete option of the collection.

  Enabling auto-delete (\e enable is TRUE) will delete objects that
  are removed from the collection.  This can be useful if the
  collection has the only reference to the objects.  (Note that the
  object can still be copied using the copy constructor - copying such
  objects is a good way to get memory leaks, reading freed memory or
  both.)

  Disabling auto-delete (\e enable is FALSE) will \e not delete objects
  that are removed from the collection.	 This is useful if the objects
  are part of many collections.

  The default setting is FALSE.

  \sa autoDelete()
*/


/*!
  \fn virtual uint QCollection::count() const
  Returns the number of objects in the collection.
*/

/*!
  \fn virtual void QCollection::clear()
  Removes all objects from the collection.  The objects will be deleted
  if auto-delete has been enabled.
  \sa setAutoDelete()
*/


/*!
  Virtual function that creates a copy of an object that is about to
  be inserted into the collection.

  The default implementation returns the \e d pointer, i.e. no copy
  is made.

  This function is seldom reimplemented in the collection template
  classes. It is not common practice to make a copy of something
  that is being inserted.

  \sa deleteItem()
*/

QCollection::Item QCollection::newItem( Item d )
{
    return d;					// just return reference
}

/*!
  Virtual function that deletes an item that is about to be removed from
  the collection.

  The default implementation deletes \e d pointer if and only if
  auto-delete has been enabled.

  This function is always reimplemented in the collection template
  classes.

  \warning If you reimplement this function you must also reimplement
  the destructor and call the virtual function clear() from your
  destructor.  This is due to the way virtual functions and
  destructors work in C++: virtual functions in derived classes cannot
  be called from a destructor.  If you do not do this your
  deleteItem() function will not be called when the container is
  destructed.

  \sa newItem(), setAutoDelete()
*/

void QCollection::deleteItem( Item d )
{
    if ( del_item )
#if defined(Q_DELETING_VOID_UNDEFINED)
	delete (char *)d;			// default operation
#else
	delete d;				// default operation
#endif
}
