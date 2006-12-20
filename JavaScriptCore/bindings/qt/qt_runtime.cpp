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

#include "qmetaobject.h"
#include "qobject.h"
#include "qdebug.h"

namespace KJS {
namespace Bindings {

// Variant value must be released with NPReleaseVariantValue()
QVariant convertValueToQVariant(ExecState* exec, JSValue* value)
{
    JSType type = value->type();

    if (type == StringType) {
        UString ustring = value->toString(exec);
        return QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size());
    } else if (type == NumberType) {
        return value->toNumber(exec);
    } else if (type == BooleanType) {
        return value->toBoolean(exec);
    } else if (type == UnspecifiedType) {
        return QVariant();
    } else if (type == NullType) {
        return QVariant();
    } else if (type == ObjectType) {
        return QVariant(); // #####
    }
    return QVariant();
}

JSValue* convertQVariantToValue(ExecState*, const QVariant& variant)
{
    if (variant.isNull())
        return jsNull();

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
    
    QVariant val = convertValueToQVariant(exec, aValue);
    property.write(obj, val);
}

} }
