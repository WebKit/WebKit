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
#include "qt_runtime.h"
#include "qt_instance.h"
#include "object.h"
#include "array_instance.h"

#include "qmetaobject.h"
#include "qobject.h"
#include "qstringlist.h"
#include "qdebug.h"

namespace KJS {
namespace Bindings {

// Variant value must be released with NPReleaseVariantValue()
QVariant convertValueToQVariant(ExecState* exec, JSValue* value, QVariant::Type hint)
{
    // check magic pointer values before dereferencing value
    if (value == jsNull() || value == jsNaN() || value == jsUndefined())
        return QVariant();

    JSLock lock;
    JSType type = value->type();
    if (hint == QVariant::Invalid) {
        switch(type) {
        case NullType:
        case UndefinedType:
        case UnspecifiedType:
        case StringType:
        case GetterSetterType:
            hint = QVariant::String;
            break;
        case NumberType:
            hint = QVariant::Double;
            break;
        case BooleanType:
            hint = QVariant::Bool;
            break;
        case ObjectType: {
            JSObject *object = value->toObject(exec);
            if (object->inherits(&ArrayInstance::info)) 
                hint = QVariant::List;
            else
                hint = QVariant::String;
        }
        }
    }

    switch (hint) {
    case QVariant::Bool:
        return value->toBoolean(exec);
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::LongLong:
    case QVariant::ULongLong:
    case QVariant::Double: {
        QVariant v(value->toNumber(exec));
        v.convert(hint);
        return v;
    }
    case QVariant::Char:
        if (type == NumberType || type == BooleanType) {
            return QChar((ushort)value->toNumber(exec));
        } else {
            UString str = value->toString(exec);
            return QChar(str.size() ? *(const ushort*)str.rep()->data() : 0);
        }
    
//     case QVariant::Map:
//     case QVariant::List:
    case QVariant::String: {
        UString ustring = value->toString(exec);
        return QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size());
    }
    case QVariant::StringList: {
        if (type != ObjectType)
            return QVariant();
        JSObject *object = value->toObject(exec);
        if (!object->inherits(&ArrayInstance::info))
            return QVariant();
        ArrayInstance *array = static_cast<ArrayInstance *>(object);

        QStringList result;
        int len = array->getLength();
        for (int i = 0; i < len; ++i) {
            JSValue *val = array->getItem(i);
            UString ustring = val->toString(exec);
            QString qstring = QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size());
            
            result.append(qstring);
        }
        return result;
    }        
    case QVariant::ByteArray: {
        UString ustring = value->toString(exec);
        return QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size()).toLatin1();
    }
    default:
        break;
    }
    return QVariant();
}

JSValue* convertQVariantToValue(ExecState*, const QVariant& variant)
{
    if (variant.isNull())
        return jsNull();

    JSLock lock;
    QVariant::Type type = variant.type();

    if (type == QVariant::Bool)
        return jsBoolean(variant.toBool());
    if (type == QVariant::Invalid)
        return jsUndefined();
    if (type == QVariant::Int ||
        type == QVariant::UInt ||
        type == QVariant::LongLong ||
        type == QVariant::ULongLong ||
        type == QVariant::Double)
        return jsNumber(variant.toDouble());
    QString string = variant.toString();
    if (string.isNull())
        return jsUndefined();
    
    UString ustring((KJS::UChar*)string.utf16(), string.length());
    return jsString(ustring);
}
    
const char* QtField::name() const
{
    return property.name();
}

JSValue* QtField::valueFromInstance(ExecState* exec, const Instance* inst) const
{
    qDebug() << "valueFromInstance";
    const QtInstance* instance = static_cast<const QtInstance*>(inst);
    QObject* obj = instance->getObject();
    QVariant val = property.read(obj);
    return convertQVariantToValue(exec, val);
}

void QtField::setValueToInstance(ExecState* exec, const Instance* inst, JSValue* aValue) const
{
    qDebug() << "setValueToInstance";
    const QtInstance* instance = static_cast<const QtInstance*>(inst);
    QObject* obj = instance->getObject();
    
    QVariant val = convertValueToQVariant(exec, aValue, QVariant::Invalid);
    property.write(obj, val);
}

} }
