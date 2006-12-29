/*
 * Copyright (C) 2006 Trolltech ASA
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "qt_instance.h"

#include "qt_class.h"
#include "qt_runtime.h"
#include "list.h"

#include <qmetaobject.h>
#include <qdebug.h>

namespace KJS {
namespace Bindings {

QtInstance::QtInstance(QObject* o)
    : Instance(),
      _class(0),
      _object(o)
{
    setRootObject(0);
}

QtInstance::~QtInstance() 
{
}

QtInstance::QtInstance(const QtInstance& other)
    : Instance(), _class(0), _object(other._object)
{
    setRootObject(other.rootObject());
}

QtInstance& QtInstance::operator=(const QtInstance& other)
{
    if (this == &other)
        return *this;

    _object = other._object;
    _class = 0;

    return *this;
}

Class* QtInstance::getClass() const
{
    if (!_class)
        _class = QtClass::classForObject(_object);
    return _class;
}

void QtInstance::begin()
{
    // Do nothing.
}

void QtInstance::end()
{
    // Do nothing.
}

bool QtInstance::implementsCall() const
{
    return true;
}

QVariant convertValueToQVariant(ExecState* exec, JSValue* value); 
JSValue* convertQVariantToValue(ExecState* exec, const QVariant& variant);
    
JSValue* QtInstance::invokeMethod(ExecState* exec, const MethodList& methodList, const List& args)
{
    // ### Should we support overloading methods?
    ASSERT(methodList.length() == 1);

    QtMethod* method = static_cast<QtMethod*>(methodList.methodAt(0));

    if (method->metaObject != _object->metaObject()) 
        return jsUndefined();

    QMetaMethod metaMethod = method->metaObject->method(method->index);
    QList<QByteArray> argTypes = metaMethod.parameterTypes();
    if (argTypes.count() != args.size()) 
        return jsUndefined();

    // only void methods work currently
    if (args.size()) 
        return jsUndefined();

    if (_object->qt_metacall(QMetaObject::InvokeMetaMethod, method->index, 0) >= 0) 
        return jsUndefined();

    return jsNull();
}


JSValue* QtInstance::invokeDefaultMethod(ExecState* exec, const List& args)
{
    return jsUndefined();
}


JSValue* QtInstance::defaultValue(JSType hint) const
{
    if (hint == StringType)
        return stringValue();
    if (hint == NumberType)
        return numberValue();
    if (hint == BooleanType)
        return booleanValue();
    return valueOf();
}

JSValue* QtInstance::stringValue() const
{
    char buf[1024];
    snprintf(buf, sizeof(buf), "QObject %p (%s)", _object.operator->(), _object->metaObject()->className());
    return jsString(buf);
}

JSValue* QtInstance::numberValue() const
{
    return jsNumber(0);
}

JSValue* QtInstance::booleanValue() const
{
    // FIXME: Implement something sensible.
    return jsBoolean(false);
}

JSValue* QtInstance::valueOf() const 
{
    return stringValue();
}

}
}
