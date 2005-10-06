/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003 Apple Computer, Inc.
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

#include "config.h"
#include "value.h"

#include "object.h"
#include "types.h"
#include "interpreter.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "internal.h"
#include "collector.h"
#include "operations.h"
#include "error_object.h"
#include "nodes.h"

namespace KJS {

AllocatedValueImp *ConstantValues::undefined = NULL;
AllocatedValueImp *ConstantValues::null = NULL;
AllocatedValueImp *ConstantValues::jsTrue = NULL;
AllocatedValueImp *ConstantValues::jsFalse = NULL;

static const double D16 = 65536.0;
static const double D32 = 4294967296.0;

void *AllocatedValueImp::operator new(size_t size)
{
    return Collector::allocate(size);
}

bool AllocatedValueImp::getUInt32(unsigned&) const
{
    return false;
}

// ECMA 9.4
double ValueImp::toInteger(ExecState *exec) const
{
    uint32_t i;
    if (getUInt32(i))
        return i;
    return roundValue(exec, const_cast<ValueImp*>(this));
}

int32_t ValueImp::toInt32(ExecState *exec) const
{
    uint32_t i;
    if (getUInt32(i))
        return i;

    double d = roundValue(exec, const_cast<ValueImp*>(this));
    if (isNaN(d) || isInf(d))
        return 0;
    double d32 = fmod(d, D32);

    if (d32 >= D32 / 2)
        d32 -= D32;
    else if (d32 < -D32 / 2)
        d32 += D32;

    return static_cast<int32_t>(d32);
}

uint32_t ValueImp::toUInt32(ExecState *exec) const
{
    uint32_t i;
    if (getUInt32(i))
        return i;

    double d = roundValue(exec, const_cast<ValueImp*>(this));
    if (isNaN(d) || isInf(d))
        return 0;
    double d32 = fmod(d, D32);

    if (d32 < 0)
        d32 += D32;

    return static_cast<uint32_t>(d32);
}

uint16_t ValueImp::toUInt16(ExecState *exec) const
{
    uint32_t i;
    if (getUInt32(i))
        return i;

    double d = roundValue(exec, const_cast<ValueImp*>(this));
    if (isNaN(d) || isInf(d))
        return 0;
    double d16 = fmod(d, D16);

    if (d16 < 0)
        d16 += D16;

    return static_cast<uint16_t>(d16);
}

ObjectImp *ValueImp::toObject(ExecState *exec) const
{
    if (SimpleNumber::is(this))
        return static_cast<const NumberImp *>(this)->NumberImp::toObject(exec);
    return downcast()->toObject(exec);
}

bool AllocatedValueImp::getBoolean(bool &booleanValue) const
{
    if (!isBoolean())
        return false;
    booleanValue = static_cast<const BooleanImp *>(this)->value();
    return true;
}

bool AllocatedValueImp::getNumber(double &numericValue) const
{
    if (!isNumber())
        return false;
    numericValue = static_cast<const NumberImp *>(this)->value();
    return true;
}

double AllocatedValueImp::getNumber() const
{
    return isNumber() ? static_cast<const NumberImp *>(this)->value() : NaN;
}

bool AllocatedValueImp::getString(UString &stringValue) const
{
    if (!isString())
        return false;
    stringValue = static_cast<const StringImp *>(this)->value();
    return true;
}

UString AllocatedValueImp::getString() const
{
    return isString() ? static_cast<const StringImp *>(this)->value() : UString();
}

ObjectImp *AllocatedValueImp::getObject()
{
    return isObject() ? static_cast<ObjectImp *>(this) : 0;
}

const ObjectImp *AllocatedValueImp::getObject() const
{
    return isObject() ? static_cast<const ObjectImp *>(this) : 0;
}

AllocatedValueImp *jsString(const char *s)
{
    return new StringImp(s ? s : "");
}

AllocatedValueImp *jsString(const UString &s)
{
    return s.isNull() ? new StringImp("") : new StringImp(s);
}

ValueImp *jsNumber(double d)
{
  ValueImp *v = SimpleNumber::make(d);
  return v ? v : new NumberImp(d);
}

void ConstantValues::init()
{
    undefined = new UndefinedImp();
    null = new NullImp();
    jsTrue = new BooleanImp(true);
    jsFalse = new BooleanImp(false);
}

void ConstantValues::clear()
{
    undefined = NULL;
    null = NULL;
    jsTrue = NULL;
    jsFalse = NULL;
}

void ConstantValues::mark()
{
    if (AllocatedValueImp *v = undefined)
        if (!v->marked())
            v->mark();
    if (AllocatedValueImp *v = null)
        if (!v->marked())
            v->mark();
    if (AllocatedValueImp *v = jsTrue)
        if (!v->marked())
            v->mark();
    if (AllocatedValueImp *v = jsFalse)
        if (!v->marked())
            v->mark();
}

}
