/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
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
 *
 */
#ifndef _DOM_RefCountImpl_h_
#define _DOM_RefCountImpl_h_

namespace DOM {

/*
 * This implements the reference counting scheme used for all internal
 * DOM objects.
 *
 * Other objects should overload deleteMe() to fit their needs. The default
 * implementation deletes the object if the ref count drops to 0.
 */
class DomShared
{
public:
  DomShared() : _ref( 0 ) {}
  virtual ~DomShared();

#if KHTML_NO_CPLUSPLUS_DOM
  bool deleteMe() { return true; }
#else
  /* Overload this function if you want a different deletion behaviour
   */
  virtual bool deleteMe();
#endif

  void ref() { _ref++; }
  void deref() { if(_ref) _ref--; if(!_ref && deleteMe()) delete this; }
  bool hasOneRef() const { return _ref == 1; }
  unsigned int refCount() const { return _ref; }

protected:
    // the number of DOMObjects referencing this Node
    // An implementation object will delete itself, if it has
    // no DOMObject referencing it, and deleteMe() returns true.
    unsigned int _ref;
};

}; // namespace

#endif
