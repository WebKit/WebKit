// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003 Apple Computer, Inc
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef _KJS_REFERENCE_H_
#define _KJS_REFERENCE_H_

#include "identifier.h"
#include "object.h"

namespace KJS {

  class JSObject;

  class Reference {
    friend class ReferenceList;
    friend class ReferenceListIterator;
  public:
    Reference(JSObject *b, const Identifier& p);
    Reference(JSObject *b, unsigned p);
    
    /**
     * Performs the GetPropertyName type conversion operation on this value
     * (ECMA 8.7)
     */
    Identifier getPropertyName(ExecState *exec) const;

    /**
     * Performs the GetValue type conversion operation on this value
     * (ECMA 8.7.1)
     */
    JSValue *getValue(ExecState *exec) const;

    /**
     * Performs the PutValue type conversion operation on this value
     * (ECMA 8.7.1)
     */
    bool deleteValue(ExecState *exec);

  protected:
    JSValue *base;

  private:
    Reference() { }

    unsigned propertyNameAsNumber;
    bool propertyNameIsNumber;
    mutable Identifier prop;
  };

}

#endif
