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
#include "date_object.h"
#include "DateMath.h"
#include "regexp_object.h"
#include <runtime_object.h>
#include <runtime_array.h>
#include <function.h>
#include "PropertyNameArray.h"
#include "qmetatype.h"
#include "qmetaobject.h"
#include "qobject.h"
#include "qstringlist.h"
#include "qdebug.h"
#include "qvarlengtharray.h"
#include "qdatetime.h"
#include <limits.h>

// QtScript has these
Q_DECLARE_METATYPE(QObjectList);
Q_DECLARE_METATYPE(QList<int>);
Q_DECLARE_METATYPE(QVariant);


namespace KJS {
namespace Bindings {

// Debugging
//#define QTWK_RUNTIME_CONVERSION_DEBUG
//#define QTWK_RUNTIME_MATCH_DEBUG

class QWKNoDebug
{
public:
    inline QWKNoDebug(){}
    inline ~QWKNoDebug(){}

    template<typename T>
    inline QWKNoDebug &operator<<(const T &) { return *this; }
};

#ifdef QTWK_RUNTIME_CONVERSION_DEBUG
#define qConvDebug() qDebug()
#else
#define qConvDebug() QWKNoDebug()
#endif

#ifdef QTWK_RUNTIME_MATCH_DEBUG
#define qMatchDebug() qDebug()
#else
#define qMatchDebug() QWKNoDebug()
#endif

typedef enum {
    Variant,
    Number,
    Boolean,
    String,
    Date,
    RegExp,
    Array,
    QObj,
    Object,
    Null
} JSRealType;

static JSRealType valueRealType(ExecState* exec, JSValue* val)
{
    if (val->isNumber())
        return Number;
    else if (val->isString())
        return String;
    else if (val->isBoolean())
        return Boolean;
    else if (val->isNull())
        return Null;
    else if (val->isObject()) {
        JSObject *object = val->toObject(exec);
        if (object->inherits(&ArrayInstance::info))
            return Array;
        else if (object->inherits(&DateInstance::info))
            return Date;
        else if (object->inherits(&RegExpImp::info))
            return RegExp;
        else if (object->inherits(&RuntimeObjectImp::info))
            return QObj;
        return Object;
    }

    return String; // I don't know.
}

QVariant convertValueToQVariant(ExecState* exec, JSValue* value, QMetaType::Type hint, int *distance)
{
    // check magic pointer values before dereferencing value
    if (value == jsNaN() || value == jsUndefined()) {
        if (distance)
            *distance = -1;
        return QVariant();
    }

    JSLock lock;
    JSRealType type = valueRealType(exec, value);
    if (hint == QMetaType::Void) {
        switch(type) {
            case Number:
                hint = QMetaType::Double;
                break;
            case Boolean:
                hint = QMetaType::Bool;
                break;
            case String:
            default:
                hint = QMetaType::QString;
                break;
            case Date:
                hint = QMetaType::QDateTime;
                break;
            case RegExp:
                hint = QMetaType::QRegExp;
                break;
            case QObj:
                hint = QMetaType::QObjectStar;
                break;
            case Array:
                hint = QMetaType::QVariantList;
                break;
        }
    }

    if (value == jsNull() 
        && hint != QMetaType::QObjectStar
        && hint != QMetaType::VoidStar) {
        if (distance)
            *distance = -1;
        return QVariant();
    }

    QVariant ret;
    int dist = -1;
    switch (hint) {
        case QMetaType::Bool:
            ret = QVariant(value->toBoolean(exec));
            if (type == Boolean)
                dist = 0;
            else
                dist = 10;
            break;

        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::Long:
        case QMetaType::ULong:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Short:
        case QMetaType::UShort:
        case QMetaType::Float:
        case QMetaType::Double:
            ret = QVariant(value->toNumber(exec));
            ret.convert((QVariant::Type)hint);
            if (type == Number) {
                switch (hint) {
                case QMetaType::Double:
                    dist = 0;
                    break;
                case QMetaType::Float:
                    dist = 1;
                    break;
                case QMetaType::LongLong:
                case QMetaType::ULongLong:
                    dist = 2;
                    break;
                case QMetaType::Long:
                case QMetaType::ULong:
                    dist = 3;
                    break;
                case QMetaType::Int:
                case QMetaType::UInt:
                    dist = 4;
                    break;
                case QMetaType::Short:
                case QMetaType::UShort:
                    dist = 5;
                    break;
                    break;
                default:
                    dist = 10;
                    break;
                }
            } else {
                dist = 10;
            }
            break;

        case QMetaType::QChar:
            if (type == Number || type == Boolean) {
                ret = QVariant(QChar((ushort)value->toNumber(exec)));
                if (type == Boolean)
                    dist = 3;
                else
                    dist = 6;
            } else {
                UString str = value->toString(exec);
                ret = QVariant(QChar(str.size() ? *(const ushort*)str.rep()->data() : 0));
                if (type == String)
                    dist = 3;
                else
                    dist = 10;
            }
            break;

        case QMetaType::QString: {
            UString ustring = value->toString(exec);
            ret = QVariant(QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size()));
            if (type == String)
                dist = 0;
            else
                dist = 10;
            break;
        }

        case QMetaType::QVariantMap: 
            if (type == Object || type == Array) {
                // Enumerate the contents of the object
                JSObject* object = value->toObject(exec);

                PropertyNameArray properties;
                object->getPropertyNames(exec, properties);
                PropertyNameArray::const_iterator it = properties.begin();

                QVariantMap result;
                int objdist = 0;
                while(it != properties.end()) {
                    if (object->propertyIsEnumerable(exec, *it)) {
                        JSValue* val = object->get(exec, *it);
                        QVariant v = convertValueToQVariant(exec, val, QMetaType::Void, &objdist);
                        if (objdist >= 0) {
                            UString ustring = (*it).ustring();
                            QString id = QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size());
                            result.insert(id, v);
                        }
                    }
                    ++it;
                }
                dist = 1;
                ret = QVariant(result);
            }
            break;

        case QMetaType::QVariantList:
            if (type == Array) {
                JSObject* object = value->toObject(exec);
                ArrayInstance* array = static_cast<ArrayInstance*>(object);

                QVariantList result;
                int len = array->getLength();
                int objdist = 0;
                for (int i = 0; i < len; ++i) {
                    JSValue *val = array->getItem(i);
                    result.append(convertValueToQVariant(exec, val, QMetaType::Void, &objdist));
                    if (objdist == -1)
                        break; // Failed converting a list entry, so fail the array
                }
                if (objdist != -1) {
                    dist = 5;
                    ret = QVariant(result);
                }
            } else {
                // Make a single length array
                QVariantList result;
                int objdist;
                result.append(convertValueToQVariant(exec, value, QMetaType::Void, &objdist));
                if (objdist != -1) {
                    ret = QVariant(result);
                    dist = 10;
                }
            }
            break;

        case QMetaType::QStringList: {
            if (type == Array) {
                JSObject* object = value->toObject(exec);
                ArrayInstance* array = static_cast<ArrayInstance*>(object);

                QStringList result;
                int len = array->getLength();
                for (int i = 0; i < len; ++i) {
                    JSValue* val = array->getItem(i);
                    UString ustring = val->toString(exec);
                    QString qstring = QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size());

                    result.append(qstring);
                }
                dist = 5;
                ret = QVariant(result);
            } else {
                // Make a single length array
                UString ustring = value->toString(exec);
                QString qstring = QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size());
                QStringList result;
                result.append(qstring);
                ret = QVariant(result);
                dist = 10;
            }
            break;
        }

        case QMetaType::QByteArray: {
            UString ustring = value->toString(exec);
            ret = QVariant(QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size()).toLatin1());
            if (type == String)
                dist = 5;
            else
                dist = 10;
            break;
        }

        case QMetaType::QDateTime:
        case QMetaType::QDate:
        case QMetaType::QTime:
            if (type == Date) {
                JSObject* object = value->toObject(exec);
                DateInstance* date = static_cast<DateInstance*>(object);
                GregorianDateTime gdt;
                date->getUTCTime(gdt);
                if (hint == QMetaType::QDateTime) {
                    ret = QDateTime(QDate(gdt.year + 1900, gdt.month + 1, gdt.monthDay), QTime(gdt.hour, gdt.minute, gdt.second), Qt::UTC);
                    dist = 0;
                } else if (hint == QMetaType::QDate) {
                    ret = QDate(gdt.year + 1900, gdt.month + 1, gdt.monthDay);
                    dist = 1;
                } else {
                    ret = QTime(gdt.hour + 1900, gdt.minute, gdt.second);
                    dist = 2;
                }
            } else if (type == Number) {
                double b = value->toNumber(exec);
                GregorianDateTime gdt;
                msToGregorianDateTime(b, true, gdt);
                if (hint == QMetaType::QDateTime) {
                    ret = QDateTime(QDate(gdt.year + 1900, gdt.month + 1, gdt.monthDay), QTime(gdt.hour, gdt.minute, gdt.second), Qt::UTC);
                    dist = 6;
                } else if (hint == QMetaType::QDate) {
                    ret = QDate(gdt.year + 1900, gdt.month + 1, gdt.monthDay);
                    dist = 8;
                } else {
                    ret = QTime(gdt.hour, gdt.minute, gdt.second);
                    dist = 10;
                }
            } else if (type == String) {
                UString ustring = value->toString(exec);
                QString qstring = QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size());

                if (hint == QMetaType::QDateTime) {
                    QDateTime dt = QDateTime::fromString(qstring, Qt::ISODate);
                    if (!dt.isValid())
                        dt = QDateTime::fromString(qstring, Qt::TextDate);
                    if (!dt.isValid())
                        dt = QDateTime::fromString(qstring, Qt::SystemLocaleDate);
                    if (!dt.isValid())
                        dt = QDateTime::fromString(qstring, Qt::LocaleDate);
                    if (dt.isValid()) {
                        ret = dt;
                        dist = 2;
                    }
                } else if (hint == QMetaType::QDate) {
                    QDate dt = QDate::fromString(qstring, Qt::ISODate);
                    if (!dt.isValid())
                        dt = QDate::fromString(qstring, Qt::TextDate);
                    if (!dt.isValid())
                        dt = QDate::fromString(qstring, Qt::SystemLocaleDate);
                    if (!dt.isValid())
                        dt = QDate::fromString(qstring, Qt::LocaleDate);
                    if (dt.isValid()) {
                        ret = dt;
                        dist = 3;
                    }
                } else {
                    QTime dt = QTime::fromString(qstring, Qt::ISODate);
                    if (!dt.isValid())
                        dt = QTime::fromString(qstring, Qt::TextDate);
                    if (!dt.isValid())
                        dt = QTime::fromString(qstring, Qt::SystemLocaleDate);
                    if (!dt.isValid())
                        dt = QTime::fromString(qstring, Qt::LocaleDate);
                    if (dt.isValid()) {
                        ret = dt;
                        dist = 3;
                    }
                }
            }
            break;

        case QMetaType::QRegExp:
            if (type == RegExp) {
/*                JSObject *object = value->toObject(exec);
                RegExpImp *re = static_cast<RegExpImp*>(object);
*/
                // Attempt to convert.. a bit risky
                UString ustring = value->toString(exec);
                QString qstring = QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size());

                // this is of the form '/xxxxxx/i'
                int firstSlash = qstring.indexOf('/');
                int lastSlash = qstring.lastIndexOf('/');
                if (firstSlash >=0 && lastSlash > firstSlash) {
                    QRegExp realRe;

                    realRe.setPattern(qstring.mid(firstSlash + 1, lastSlash - firstSlash - 1));

                    if (qstring.mid(lastSlash + 1).contains('i'))
                        realRe.setCaseSensitivity(Qt::CaseInsensitive);

                    ret = qVariantFromValue(realRe);
                    dist = 0;
                } else {
                    qConvDebug() << "couldn't parse a JS regexp";
                }
            } else if (type == String) {
                UString ustring = value->toString(exec);
                QString qstring = QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size());

                QRegExp re(qstring);
                if (re.isValid()) {
                    ret = qVariantFromValue(re);
                    dist = 10;
                }
            }
            break;

        case QMetaType::QObjectStar:
            if (type == QObj) {
                JSObject* object = value->toObject(exec);
                QtInstance* qtinst = static_cast<QtInstance*>(Instance::getInstance(object, Instance::QtLanguage));
                if (qtinst) {
                    if (qtinst->getObject()) {
                        qConvDebug() << "found instance, with object:" << (void*) qtinst->getObject();
                        ret = qVariantFromValue(qtinst->getObject());
                        qConvDebug() << ret;
                        dist = 0;
                    } else {
                        qConvDebug() << "can't convert deleted qobject";
                    }
                } else {
                    qConvDebug() << "wasn't a qtinstance";
                }
            } else if (type == Null) {
                QObject* nullobj = 0;
                ret = qVariantFromValue(nullobj);
                dist = 0;
            } else {
                qConvDebug() << "previous type was not an object:" << type;
            }
            break;

        case QMetaType::VoidStar:
            if (type == QObj) {
                JSObject* object = value->toObject(exec);
                QtInstance* qtinst = static_cast<QtInstance*>(Instance::getInstance(object, Instance::QtLanguage));
                if (qtinst) {
                    if (qtinst->getObject()) {
                        qConvDebug() << "found instance, with object:" << (void*) qtinst->getObject();
                        ret = qVariantFromValue((void *)qtinst->getObject());
                        qConvDebug() << ret;
                        dist = 0;
                    } else {
                        qConvDebug() << "can't convert deleted qobject";
                    }
                } else {
                    qConvDebug() << "wasn't a qtinstance";
                }
            } else if (type == Null) {
                ret = qVariantFromValue((void*)0);
                dist = 0;
            } else if (type == Number) {
                // I don't think that converting a double to a pointer is a wise
                // move.  Except maybe 0.
                qConvDebug() << "got number for void * - not converting, seems unsafe:" << value->toNumber(exec);
            } else {
                qConvDebug() << "void* - unhandled type" << type;
            }
            break;

        default:
            // Non const type ids
            if (hint == (QMetaType::Type) qMetaTypeId<QObjectList>())
            {
                if (type == Array) {
                    JSObject* object = value->toObject(exec);
                    ArrayInstance* array = static_cast<ArrayInstance *>(object);

                    QObjectList result;
                    int len = array->getLength();
                    for (int i = 0; i < len; ++i) {
                        JSValue *val = array->getItem(i);
                        int itemdist = -1;
                        QVariant item = convertValueToQVariant(exec, val, QMetaType::QObjectStar, &itemdist);
                        if (itemdist >= 0)
                            result.append(item.value<QObject*>());
                        else
                            break;
                    }
                    // If we didn't fail conversion
                    if (result.count() == len) {
                        dist = 5;
                        ret = QVariant::fromValue(result);
                    } else {
                        qConvDebug() << "type conversion failed (wanted" << len << ", got " << result.count() << ")";
                    }
                } else {
                    // Make a single length array
                    QObjectList result;
                    int itemdist = -1;
                    QVariant item = convertValueToQVariant(exec, value, QMetaType::QObjectStar, &itemdist);
                    if (itemdist >= 0) {
                        result.append(item.value<QObject*>());
                        dist = 10;
                        ret = QVariant::fromValue(result);
                    }
                }
                break;
            } else if (hint == (QMetaType::Type) qMetaTypeId<QList<int> >()) {
                if (type == Array) {
                    JSObject* object = value->toObject(exec);
                    ArrayInstance* array = static_cast<ArrayInstance *>(object);

                    QList<int> result;
                    int len = array->getLength();
                    for (int i = 0; i < len; ++i) {
                        JSValue* val = array->getItem(i);
                        int itemdist = -1;
                        QVariant item = convertValueToQVariant(exec, val, QMetaType::Int, &itemdist);
                        if (itemdist >= 0)
                            result.append(item.value<int>());
                        else
                            break;
                    }
                    // If we didn't fail conversion
                    if (result.count() == len) {
                        dist = 5;
                        ret = QVariant::fromValue(result);
                    } else {
                        qConvDebug() << "type conversion failed (wanted" << len << ", got " << result.count() << ")";
                    }
                } else {
                    // Make a single length array
                    QList<int> result;
                    int itemdist = -1;
                    QVariant item = convertValueToQVariant(exec, value, QMetaType::Int, &itemdist);
                    if (itemdist >= 0) {
                        result.append(item.value<int>());
                        dist = 10;
                        ret = QVariant::fromValue(result);
                    }
                }
                break;
            } else if (hint == (QMetaType::Type) qMetaTypeId<QVariant>()) {
                // Well.. we can do anything... just recurse with the autodetect flag
                ret = convertValueToQVariant(exec, value, QMetaType::Void, distance);
                dist = 10;
                break;
            }

            dist = 10;
            break;
    }

    if (!ret.isValid())
        dist = -1;
    if (distance)
        *distance = dist;

    return ret;
}

JSValue* convertQVariantToValue(ExecState* exec, PassRefPtr<RootObject> root, const QVariant& variant)
{
    // Variants with QObject * can be isNull but not a null pointer
    // An empty QString variant is also null
    QMetaType::Type type = (QMetaType::Type) variant.userType();
    if (variant.isNull() &&
        type != QMetaType::QObjectStar &&
        type != QMetaType::VoidStar &&
        type != QMetaType::QWidgetStar &&
        type != QMetaType::QString) {
        return jsNull();
    }

    JSLock lock;

    if (type == QMetaType::Bool)
        return jsBoolean(variant.toBool());

    if (type == QMetaType::Int ||
        type == QMetaType::UInt ||
        type == QMetaType::Long ||
        type == QMetaType::ULong ||
        type == QMetaType::LongLong ||
        type == QMetaType::ULongLong ||
        type == QMetaType::Short ||
        type == QMetaType::UShort ||
        type == QMetaType::Float ||
        type == QMetaType::Double)
        return jsNumber(variant.toDouble());

    if (type == QMetaType::QRegExp) {
        QRegExp re = variant.value<QRegExp>();

        if (re.isValid()) {
            RegExpObjectImp* regExpObj = static_cast<RegExpObjectImp*>(exec->lexicalGlobalObject()->regExpConstructor());
            List args;
            UString uflags;

            if (re.caseSensitivity() == Qt::CaseInsensitive)
                uflags = "i"; // ### Can't do g or m
            UString ustring((KJS::UChar*)re.pattern().utf16(), re.pattern().length());
            args.append(jsString(ustring));
            args.append(jsString(uflags));
            return regExpObj->construct(exec, args);
        }
    }

    if (type == QMetaType::QDateTime ||
        type == QMetaType::QDate ||
        type == QMetaType::QTime) {
        DateObjectImp *dateObj = static_cast<DateObjectImp*>(exec->lexicalGlobalObject()->dateConstructor());
        List args;

        QDate date = QDate::currentDate();
        QTime time(0,0,0); // midnight

        if (type == QMetaType::QDate)
            date = variant.value<QDate>();
        else if (type == QMetaType::QTime)
            time = variant.value<QTime>();
        else {
            QDateTime dt = variant.value<QDateTime>().toLocalTime();
            date = dt.date();
            time = dt.time();
        }

        // Dates specified this way are in local time (we convert DateTimes above)
        args.append(jsNumber(date.year()));
        args.append(jsNumber(date.month() - 1));
        args.append(jsNumber(date.day()));
        args.append(jsNumber(time.hour()));
        args.append(jsNumber(time.minute()));
        args.append(jsNumber(time.second()));
        args.append(jsNumber(time.msec()));
        return dateObj->construct(exec, args);
    }

    if (type == QMetaType::QByteArray) {
        QByteArray ba = variant.value<QByteArray>();
        UString ustring(ba.constData());
        return jsString(ustring);
    }

    if (type == QMetaType::QObjectStar || type == QMetaType::QWidgetStar) {
        QObject* obj = variant.value<QObject*>();
        return Instance::createRuntimeObject(Instance::QtLanguage, obj, root);
    }

    if (type == QMetaType::QVariantMap) {
        // create a new object, and stuff properties into it
        JSObject* ret = new JSObject(exec->lexicalGlobalObject()->objectPrototype());
        QVariantMap map = variant.value<QVariantMap>();
        QVariantMap::const_iterator i = map.constBegin();
        while (i != map.constEnd()) {
            QString s = i.key();
            JSValue* val = convertQVariantToValue(exec, root, i.value());
            if (val)
                ret->put(exec, Identifier((const UChar *)s.constData(), s.length()), val);
            // ### error case?
            ++i;
        }

        return ret;
    }

    // List types
    if (type == QMetaType::QVariantList) {
        QVariantList vl = variant.toList();
        return new RuntimeArray(exec, new QtArray<QVariant>(vl, QMetaType::Void, root));
    } else if (type == QMetaType::QStringList) {
        QStringList sl = variant.value<QStringList>();
        return new RuntimeArray(exec, new QtArray<QString>(sl, QMetaType::QString, root));
    } else if (type == (QMetaType::Type) qMetaTypeId<QObjectList>()) {
        QObjectList ol= variant.value<QObjectList>();
        return new RuntimeArray(exec, new QtArray<QObject*>(ol, QMetaType::QObjectStar, root));
    } else if (type == (QMetaType::Type)qMetaTypeId<QList<int> >()) {
        QList<int> il= variant.value<QList<int> >();
        return new RuntimeArray(exec, new QtArray<int>(il, QMetaType::Int, root));
    }

    if (type == (QMetaType::Type)qMetaTypeId<QVariant>()) {
        QVariant real = variant.value<QVariant>();
        qConvDebug() << "real variant is:" << real;
        return convertQVariantToValue(exec, root, real);
    }

    qConvDebug() << "fallback path for" << variant << variant.userType();

    QString string = variant.toString();
    UString ustring((KJS::UChar*)string.utf16(), string.length());
    return jsString(ustring);
}

// ===============

// Qt-like macros
#define QW_D(Class) Class##Data* d = d_func()
#define QW_DS(Class,Instance) Class##Data* d = Instance->d_func()

QtRuntimeMethod::QtRuntimeMethod(QtRuntimeMethodData* dd, ExecState *exec, const Identifier &ident, PassRefPtr<QtInstance> inst)
    : InternalFunctionImp (static_cast<FunctionPrototype*>(exec->lexicalGlobalObject()->functionPrototype()), ident)
    , d_ptr(dd)
{
    QW_D(QtRuntimeMethod);
    d->m_instance = inst;
}

QtRuntimeMethod::~QtRuntimeMethod()
{
    delete d_ptr;
}

CodeType QtRuntimeMethod::codeType() const
{
    return FunctionCode;
}

Completion QtRuntimeMethod::execute(ExecState*)
{
    return Completion(Normal, jsUndefined());
}

// ===============

QtRuntimeMethodData::~QtRuntimeMethodData()
{
}

QtRuntimeMetaMethodData::~QtRuntimeMetaMethodData()
{

}

QtRuntimeConnectionMethodData::~QtRuntimeConnectionMethodData()
{

}

// ===============

// Type conversion metadata (from QtScript originally)
class QtMethodMatchType
{
public:
    enum Kind {
        Invalid,
        Variant,
        MetaType,
        Unresolved,
        MetaEnum
    };


    QtMethodMatchType()
        : m_kind(Invalid) { }

    Kind kind() const
    { return m_kind; }

    QMetaType::Type typeId() const;

    bool isValid() const
    { return (m_kind != Invalid); }

    bool isVariant() const
    { return (m_kind == Variant); }

    bool isMetaType() const
    { return (m_kind == MetaType); }

    bool isUnresolved() const
    { return (m_kind == Unresolved); }

    bool isMetaEnum() const
    { return (m_kind == MetaEnum); }

    QByteArray name() const;

    int enumeratorIndex() const
    { Q_ASSERT(isMetaEnum()); return m_typeId; }

    static QtMethodMatchType variant()
    { return QtMethodMatchType(Variant); }

    static QtMethodMatchType metaType(int typeId, const QByteArray &name)
    { return QtMethodMatchType(MetaType, typeId, name); }

    static QtMethodMatchType metaEnum(int enumIndex, const QByteArray &name)
    { return QtMethodMatchType(MetaEnum, enumIndex, name); }

    static QtMethodMatchType unresolved(const QByteArray &name)
    { return QtMethodMatchType(Unresolved, /*typeId=*/0, name); }

private:
    QtMethodMatchType(Kind kind, int typeId = 0, const QByteArray &name = QByteArray())
        : m_kind(kind), m_typeId(typeId), m_name(name) { }

    Kind m_kind;
    int m_typeId;
    QByteArray m_name;
};

QMetaType::Type QtMethodMatchType::typeId() const
{
    if (isVariant())
        return (QMetaType::Type) QMetaType::type("QVariant");
    return (QMetaType::Type) (isMetaEnum() ? QMetaType::Int : m_typeId);
}

QByteArray QtMethodMatchType::name() const
{
    if (!m_name.isEmpty())
        return m_name;
    else if (m_kind == Variant)
        return "QVariant";
    return QByteArray();
}

struct QtMethodMatchData
{
    int matchDistance;
    int index;
    QVector<QtMethodMatchType> types;
    QVarLengthArray<QVariant, 10> args;

    QtMethodMatchData(int dist, int idx, QVector<QtMethodMatchType> typs,
                                const QVarLengthArray<QVariant, 10> &as)
        : matchDistance(dist), index(idx), types(typs), args(as) { }
    QtMethodMatchData()
        : index(-1) { }

    bool isValid() const
    { return (index != -1); }

    int firstUnresolvedIndex() const
    {
        for (int i=0; i < types.count(); i++) {
            if (types.at(i).isUnresolved())
                return i;
        }
        return -1;
    }
};

static int indexOfMetaEnum(const QMetaObject *meta, const QByteArray &str)
{
    QByteArray scope;
    QByteArray name;
    int scopeIdx = str.indexOf("::");
    if (scopeIdx != -1) {
        scope = str.left(scopeIdx);
        name = str.mid(scopeIdx + 2);
    } else {
        name = str;
    }
    for (int i = meta->enumeratorCount() - 1; i >= 0; --i) {
        QMetaEnum m = meta->enumerator(i);
        if ((m.name() == name)/* && (scope.isEmpty() || (m.scope() == scope))*/)
            return i;
    }
    return -1;
}

// Helper function for resolving methods
// Largely based on code in QtScript for compatibility reasons
static int findMethodIndex(ExecState* exec,
                           const QMetaObject* meta,
                           const QByteArray& signature,
                           bool allowPrivate,
                           const List& jsArgs,
                           QVarLengthArray<QVariant, 10> &vars,
                           void** vvars,
                           JSObject **pError)
{
    QList<int> matchingIndices;

    bool overloads = !signature.contains('(');

    int count = meta->methodCount();
    for (int i = count - 1; i >= 0; --i) {
        const QMetaMethod m = meta->method(i);

        // Don't choose private methods
        if (m.access() == QMetaMethod::Private && !allowPrivate)
            continue;

        // try and find all matching named methods
        if (m.signature() == signature)
            matchingIndices.append(i);
        else if (overloads) {
            QByteArray rawsignature = m.signature();
            rawsignature.truncate(rawsignature.indexOf('('));
            if (rawsignature == signature)
                matchingIndices.append(i);
        }
    }

    int chosenIndex = -1;
    *pError = 0;
    QVector<QtMethodMatchType> chosenTypes;

    QVarLengthArray<QVariant, 10> args;
    QVector<QtMethodMatchData> candidates;
    QVector<QtMethodMatchData> unresolved;
    QVector<int> tooFewArgs;
    QVector<int> conversionFailed;

    foreach(int index, matchingIndices) {
        QMetaMethod method = meta->method(index);

        QVector<QtMethodMatchType> types;
        bool unresolvedTypes = false;

        // resolve return type
        QByteArray returnTypeName = method.typeName();
        int rtype = QMetaType::type(returnTypeName);
        if ((rtype == 0) && !returnTypeName.isEmpty()) {
            if (returnTypeName == "QVariant") {
                types.append(QtMethodMatchType::variant());
            } else if (returnTypeName.endsWith('*')) {
                types.append(QtMethodMatchType::metaType(QMetaType::VoidStar, returnTypeName));
            } else {
                int enumIndex = indexOfMetaEnum(meta, returnTypeName);
                if (enumIndex != -1)
                    types.append(QtMethodMatchType::metaEnum(enumIndex, returnTypeName));
                else {
                    unresolvedTypes = true;
                    types.append(QtMethodMatchType::unresolved(returnTypeName));
                }
            }
        } else {
            if (returnTypeName == "QVariant")
                types.append(QtMethodMatchType::variant());
            else
                types.append(QtMethodMatchType::metaType(rtype, returnTypeName));
        }

        // resolve argument types
        QList<QByteArray> parameterTypeNames = method.parameterTypes();
        for (int i = 0; i < parameterTypeNames.count(); ++i) {
            QByteArray argTypeName = parameterTypeNames.at(i);
            int atype = QMetaType::type(argTypeName);
            if (atype == 0) {
                if (argTypeName == "QVariant") {
                    types.append(QtMethodMatchType::variant());
                } else {
                    int enumIndex = indexOfMetaEnum(meta, argTypeName);
                    if (enumIndex != -1)
                        types.append(QtMethodMatchType::metaEnum(enumIndex, argTypeName));
                    else {
                        unresolvedTypes = true;
                        types.append(QtMethodMatchType::unresolved(argTypeName));
                    }
                }
            } else {
                if (argTypeName == "QVariant")
                    types.append(QtMethodMatchType::variant());
                else
                    types.append(QtMethodMatchType::metaType(atype, argTypeName));
            }
        }

        if (jsArgs.size() < (types.count() - 1)) {
            qMatchDebug() << "Match:too few args for" << method.signature();
            tooFewArgs.append(index);
            continue;
        }

        if (unresolvedTypes) {
            qMatchDebug() << "Match:unresolved arg types for" << method.signature();
            // remember it so we can give an error message later, if necessary
            unresolved.append(QtMethodMatchData(/*matchDistance=*/INT_MAX, index,
                                                   types, QVarLengthArray<QVariant, 10>()));
            continue;
        }

        // Now convert arguments
        if (args.count() != types.count())
            args.resize(types.count());

        QtMethodMatchType retType = types[0];
        args[0] = QVariant(retType.typeId(), (void *)0); // the return value

        bool converted = true;
        int matchDistance = 0;
        for (int i = 0; converted && i < types.count() - 1; ++i) {
            JSValue* arg = i < jsArgs.size() ? jsArgs[i] : jsUndefined();

            int argdistance = -1;
            QVariant v = convertValueToQVariant(exec, arg, types.at(i+1).typeId(), &argdistance);
            if (argdistance >= 0) {
                matchDistance += argdistance;
                args[i+1] = v;
            } else {
                qMatchDebug() << "failed to convert argument " << i << "type" << types.at(i+1).typeId() << QMetaType::typeName(types.at(i+1).typeId());
                converted = false;
            }
        }

        qMatchDebug() << "Match: " << method.signature() << (converted ? "converted":"failed to convert") << "distance " << matchDistance;

        if (converted) {
            if ((jsArgs.size() == types.count() - 1)
                && (matchDistance == 0)) {
                // perfect match, use this one
                chosenIndex = index;
                break;
            } else {
                QtMethodMatchData metaArgs(matchDistance, index, types, args);
                if (candidates.isEmpty()) {
                    candidates.append(metaArgs);
                } else {
                    QtMethodMatchData otherArgs = candidates.at(0);
                    if ((args.count() > otherArgs.args.count())
                        || ((args.count() == otherArgs.args.count())
                            && (matchDistance <= otherArgs.matchDistance))) {
                        candidates.prepend(metaArgs);
                    } else {
                        candidates.append(metaArgs);
                    }
                }
            }
        } else {
            conversionFailed.append(index);
        }

        if (!overloads)
            break;
    }

    if (chosenIndex == -1 && candidates.count() == 0) {
        // No valid functions at all - format an error message
        if (!conversionFailed.isEmpty()) {
            QString message = QString::fromLatin1("incompatible type of argument(s) in call to %0(); candidates were\n")
                              .arg(QLatin1String(signature));
            for (int i = 0; i < conversionFailed.size(); ++i) {
                if (i > 0)
                    message += QLatin1String("\n");
                QMetaMethod mtd = meta->method(conversionFailed.at(i));
                message += QString::fromLatin1("    %0").arg(QString::fromLatin1(mtd.signature()));
            }
            *pError = throwError(exec, TypeError, message.toLatin1().constData());
        } else if (!unresolved.isEmpty()) {
            QtMethodMatchData argsInstance = unresolved.first();
            int unresolvedIndex = argsInstance.firstUnresolvedIndex();
            Q_ASSERT(unresolvedIndex != -1);
            QtMethodMatchType unresolvedType = argsInstance.types.at(unresolvedIndex);
            QString message = QString::fromLatin1("cannot call %0(): unknown type `%1'")
                .arg(QString::fromLatin1(signature))
                .arg(QLatin1String(unresolvedType.name()));
            *pError = throwError(exec, TypeError, message.toLatin1().constData());
        } else {
            QString message = QString::fromLatin1("too few arguments in call to %0(); candidates are\n")
                              .arg(QLatin1String(signature));
            for (int i = 0; i < tooFewArgs.size(); ++i) {
                if (i > 0)
                    message += QLatin1String("\n");
                QMetaMethod mtd = meta->method(tooFewArgs.at(i));
                message += QString::fromLatin1("    %0").arg(QString::fromLatin1(mtd.signature()));
            }
            *pError = throwError(exec, SyntaxError, message.toLatin1().constData());
        }
    }

    if (chosenIndex == -1 && candidates.count() > 0) {
        QtMethodMatchData metaArgs = candidates.at(0);
        if ((candidates.size() > 1)
            && (metaArgs.args.count() == candidates.at(1).args.count())
            && (metaArgs.matchDistance == candidates.at(1).matchDistance)) {
            // ambiguous call
            QString message = QString::fromLatin1("ambiguous call of overloaded function %0(); candidates were\n")
                                .arg(QLatin1String(signature));
            for (int i = 0; i < candidates.size(); ++i) {
                if (i > 0)
                    message += QLatin1String("\n");
                QMetaMethod mtd = meta->method(candidates.at(i).index);
                message += QString::fromLatin1("    %0").arg(QString::fromLatin1(mtd.signature()));
            }
            *pError = throwError(exec, TypeError, message.toLatin1().constData());
        } else {
            chosenIndex = metaArgs.index;
            args = metaArgs.args;
        }
    }

    if (chosenIndex != -1) {
        /* Copy the stuff over */
        int i;
        vars.resize(args.count());
        for (i=0; i < args.count(); i++) {
            vars[i] = args[i];
            vvars[i] = vars[i].data();
        }
    }

    return chosenIndex;
}

// Signals are not fuzzy matched as much as methods
static int findSignalIndex(const QMetaObject* meta, int initialIndex, QByteArray signature)
{
    int index = initialIndex;
    QMetaMethod method = meta->method(index);
    bool overloads = !signature.contains('(');
    if (overloads && (method.attributes() & QMetaMethod::Cloned)) {
        // find the most general method
        do {
            method = meta->method(--index);
        } while (method.attributes() & QMetaMethod::Cloned);
    }
    return index;
}

QtRuntimeMetaMethod::QtRuntimeMetaMethod(ExecState* exec, const Identifier& ident, PassRefPtr<QtInstance> inst, int index, const QByteArray& signature, bool allowPrivate)
    : QtRuntimeMethod (new QtRuntimeMetaMethodData(), exec, ident, inst)
{
    QW_D(QtRuntimeMetaMethod);
    d->m_signature = signature;
    d->m_index = index;
    d->m_connect = 0;
    d->m_disconnect = 0;
    d->m_allowPrivate = allowPrivate;
}

void QtRuntimeMetaMethod::mark()
{
    QtRuntimeMethod::mark();
    QW_D(QtRuntimeMetaMethod);
    if (d->m_connect)
        d->m_connect->mark();
    if (d->m_disconnect)
        d->m_disconnect->mark();
}

JSValue* QtRuntimeMetaMethod::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    QW_D(QtRuntimeMetaMethod);

    // We're limited to 10 args
    if (args.size() > 10)
        return jsUndefined();

    // We have to pick a method that matches..
    JSLock lock;

    QObject *obj = d->m_instance->getObject();
    if (obj) {
        QVarLengthArray<QVariant, 10> vargs;
        void *qargs[11];

        int methodIndex;
        JSObject* errorObj = 0;
        if ((methodIndex = findMethodIndex(exec, obj->metaObject(), d->m_signature, d->m_allowPrivate, args, vargs, (void **)qargs, &errorObj)) != -1) {
            if (obj->qt_metacall(QMetaObject::InvokeMetaMethod, methodIndex, qargs) >= 0)
                return jsUndefined();

            if (vargs[0].isValid())
                return convertQVariantToValue(exec, d->m_instance->rootObject(), vargs[0]);
        }

        if (errorObj)
            return errorObj;
    } else {
        return throwError(exec, GeneralError, "cannot call function of deleted QObject");
    }

    // void functions return undefined
    return jsUndefined();
}

bool QtRuntimeMetaMethod::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (propertyName == "connect") {
        slot.setCustom(this, connectGetter);
        return true;
    } else if (propertyName == "disconnect") {
        slot.setCustom(this, disconnectGetter);
        return true;
    } else if (propertyName == exec->propertyNames().length) {
        slot.setCustom(this, lengthGetter);
        return true;
    }

    return QtRuntimeMethod::getOwnPropertySlot(exec, propertyName, slot);
}

JSValue *QtRuntimeMetaMethod::lengthGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&)
{
    // QtScript always returns 0
    return jsNumber(0);
}

JSValue *QtRuntimeMetaMethod::connectGetter(ExecState* exec, JSObject*, const Identifier& ident, const PropertySlot& slot)
{
    QtRuntimeMetaMethod* thisObj = static_cast<QtRuntimeMetaMethod*>(slot.slotBase());
    QW_DS(QtRuntimeMetaMethod, thisObj);

    if (!d->m_connect)
        d->m_connect = new QtRuntimeConnectionMethod(exec, ident, true, d->m_instance, d->m_index, d->m_signature);
    return d->m_connect;
}

JSValue* QtRuntimeMetaMethod::disconnectGetter(ExecState* exec, JSObject*, const Identifier& ident, const PropertySlot& slot)
{
    QtRuntimeMetaMethod* thisObj = static_cast<QtRuntimeMetaMethod*>(slot.slotBase());
    QW_DS(QtRuntimeMetaMethod, thisObj);

    if (!d->m_disconnect)
        d->m_disconnect = new QtRuntimeConnectionMethod(exec, ident, false, d->m_instance, d->m_index, d->m_signature);
    return d->m_disconnect;
}

// ===============

QMultiMap<QObject*, QtConnectionObject*> QtRuntimeConnectionMethod::connections;

QtRuntimeConnectionMethod::QtRuntimeConnectionMethod(ExecState* exec, const Identifier& ident, bool isConnect, PassRefPtr<QtInstance> inst, int index, const QByteArray& signature)
    : QtRuntimeMethod (new QtRuntimeConnectionMethodData(), exec, ident, inst)
{
    QW_D(QtRuntimeConnectionMethod);

    d->m_signature = signature;
    d->m_index = index;
    d->m_isConnect = isConnect;
}

JSValue *QtRuntimeConnectionMethod::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    QW_D(QtRuntimeConnectionMethod);

    JSLock lock;

    QObject* sender = d->m_instance->getObject();

    if (sender) {

        JSObject* thisObject = exec->lexicalGlobalObject();
        JSObject* funcObject = 0;

        // QtScript checks signalness first, arguments second
        int signalIndex = -1;

        // Make sure the initial index is a signal
        QMetaMethod m = sender->metaObject()->method(d->m_index);
        if (m.methodType() == QMetaMethod::Signal)
            signalIndex = findSignalIndex(sender->metaObject(), d->m_index, d->m_signature);

        if (signalIndex != -1) {
            if (args.size() == 1) {
                funcObject = args[0]->toObject(exec);
                if (!funcObject->implementsCall()) {
                    if (d->m_isConnect)
                        return throwError(exec, TypeError, "QtMetaMethod.connect: target is not a function");
                    else
                        return throwError(exec, TypeError, "QtMetaMethod.disconnect: target is not a function");
                }
            } else if (args.size() >= 2) {
                if (args[0]->type() == ObjectType) {
                    thisObject = args[0]->toObject(exec);

                    // Get the actual function to call
                    JSObject *asObj = args[1]->toObject(exec);
                    if (asObj->implementsCall()) {
                        // Function version
                        funcObject = asObj;
                    } else {
                        // Convert it to a string
                        UString funcName = args[1]->toString(exec);
                        Identifier funcIdent(funcName);

                        // ### DropAllLocks
                        // This is resolved at this point in QtScript
                        JSValue* val = thisObject->get(exec, funcIdent);
                        JSObject* asFuncObj = val->toObject(exec);

                        if (asFuncObj->implementsCall()) {
                            funcObject = asFuncObj;
                        } else {
                            if (d->m_isConnect)
                                return throwError(exec, TypeError, "QtMetaMethod.connect: target is not a function");
                            else
                                return throwError(exec, TypeError, "QtMetaMethod.disconnect: target is not a function");
                        }
                    }
                } else {
                    if (d->m_isConnect)
                        return throwError(exec, TypeError, "QtMetaMethod.connect: thisObject is not an object");
                    else
                        return throwError(exec, TypeError, "QtMetaMethod.disconnect: thisObject is not an object");
                }
            } else {
                if (d->m_isConnect)
                    return throwError(exec, GeneralError, "QtMetaMethod.connect: no arguments given");
                else
                    return throwError(exec, GeneralError, "QtMetaMethod.disconnect: no arguments given");
            }

            if (d->m_isConnect) {
                // to connect, we need:
                //  target object [from ctor]
                //  target signal index etc. [from ctor]
                //  receiver function [from arguments]
                //  receiver this object [from arguments]

                QtConnectionObject* conn = new QtConnectionObject(d->m_instance, signalIndex, thisObject, funcObject);
                bool ok = QMetaObject::connect(sender, signalIndex, conn, conn->metaObject()->methodOffset());
                if (!ok) {
                    delete conn;
                    QString msg = QString("QtMetaMethod.connect: failed to connect to %1::%2()")
                            .arg(sender->metaObject()->className())
                            .arg(QLatin1String(d->m_signature));
                    return throwError(exec, GeneralError, msg.toLatin1().constData());
                }
                else {
                    // Store connection
                    connections.insert(sender, conn);
                }
            } else {
                // Now to find our previous connection object. Hmm.
                QList<QtConnectionObject*> conns = connections.values(sender);
                bool ret = false;

                foreach(QtConnectionObject* conn, conns) {
                    // Is this the right connection?
                    if (conn->match(sender, signalIndex, thisObject, funcObject)) {
                        // Yep, disconnect it
                        QMetaObject::disconnect(sender, signalIndex, conn, conn->metaObject()->methodOffset());
                        delete conn; // this will also remove it from the map
                        ret = true;
                        break;
                    }
                }

                if (!ret) {
                    QString msg = QString("QtMetaMethod.disconnect: failed to disconnect from %1::%2()")
                            .arg(sender->metaObject()->className())
                            .arg(QLatin1String(d->m_signature));
                    return throwError(exec, GeneralError, msg.toLatin1().constData());
                }
            }
        } else {
            QString msg = QString("QtMetaMethod.%1: %2::%3() is not a signal")
                    .arg(d->m_isConnect ? "connect": "disconnect")
                    .arg(sender->metaObject()->className())
                    .arg(QLatin1String(d->m_signature));
            return throwError(exec, TypeError, msg.toLatin1().constData());
        }
    } else {
        return throwError(exec, GeneralError, "cannot call function of deleted QObject");
    }

    return jsUndefined();
}

bool QtRuntimeConnectionMethod::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (propertyName == exec->propertyNames().length) {
        slot.setCustom(this, lengthGetter);
        return true;
    }

    return QtRuntimeMethod::getOwnPropertySlot(exec, propertyName, slot);
}

JSValue *QtRuntimeConnectionMethod::lengthGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&)
{
    // we have one formal argument, and one optional
    return jsNumber(1);
}

// ===============

QtConnectionObject::QtConnectionObject(PassRefPtr<QtInstance> instance, int signalIndex, JSObject* thisObject, JSObject* funcObject)
    : m_instance(instance)
    , m_signalIndex(signalIndex)
    , m_originalObject(m_instance->getObject())
    , m_thisObject(thisObject)
    , m_funcObject(funcObject)
{
    setParent(m_originalObject);
    ASSERT(JSLock::currentThreadIsHoldingLock()); // so our ProtectedPtrs are safe
}

QtConnectionObject::~QtConnectionObject()
{
    // Remove us from the map of active connections
    QtRuntimeConnectionMethod::connections.remove(m_originalObject, this);
}

static const uint qt_meta_data_QtConnectionObject[] = {

 // content:
       1,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   10, // methods
       0,    0, // properties
       0,    0, // enums/sets

 // slots: signature, parameters, type, tag, flags
      28,   27,   27,   27, 0x0a,

       0        // eod
};

static const char qt_meta_stringdata_QtConnectionObject[] = {
    "KJS::Bindings::QtConnectionObject\0\0execute()\0"
};

const QMetaObject QtConnectionObject::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_QtConnectionObject,
      qt_meta_data_QtConnectionObject, 0 }
};

const QMetaObject *QtConnectionObject::metaObject() const
{
    return &staticMetaObject;
}

void *QtConnectionObject::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_QtConnectionObject))
        return static_cast<void*>(const_cast<QtConnectionObject*>(this));
    return QObject::qt_metacast(_clname);
}

int QtConnectionObject::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: execute(_a); break;
        }
        _id -= 1;
    }
    return _id;
}

void QtConnectionObject::execute(void **argv)
{
    QObject* obj = m_instance->getObject();
    if (obj) {
        const QMetaObject* meta = obj->metaObject();
        const QMetaMethod method = meta->method(m_signalIndex);

        QList<QByteArray> parameterTypes = method.parameterTypes();

        int argc = parameterTypes.count();

        JSLock lock;

        // ### Should the Interpreter/ExecState come from somewhere else?
        RefPtr<RootObject> ro = m_instance->rootObject();
        if (ro) {
            JSGlobalObject* globalobj = ro->globalObject();
            if (globalobj) {
                ExecState* exec = globalobj->globalExec();
                if (exec) {
                    // Build the argument list (up to the formal argument length of the slot)
                    List l;
                    // ### DropAllLocks?
                    int funcArgC = m_funcObject->get(exec, exec->propertyNames().length)->toInt32(exec);
                    int argTotal = qMax(funcArgC, argc);
                    for(int i=0; i < argTotal; i++) {
                        if (i < argc) {
                            int argType = QMetaType::type(parameterTypes.at(i));
                            l.append(convertQVariantToValue(exec, ro, QVariant(argType, argv[i+1])));
                        } else {
                            l.append(jsUndefined());
                        }
                    }
                    // Stuff in the __qt_sender property, if we can
                    if (m_funcObject->inherits(&FunctionImp::info)) {
                        FunctionImp* fimp = static_cast<FunctionImp*>(m_funcObject.get());

                        JSObject* qt_sender = Instance::createRuntimeObject(Instance::QtLanguage, sender(), ro);
                        JSObject* wrapper = new JSObject();
                        wrapper->put(exec, "__qt_sender__", qt_sender);
                        ScopeChain oldsc = fimp->scope();
                        ScopeChain sc = oldsc;
                        sc.push(wrapper);
                        fimp->setScope(sc);
                        fimp->call(exec, m_thisObject, l);
                        fimp->setScope(oldsc);
                    } else
                        m_funcObject->call(exec, m_thisObject, l);
                }
            }
        }
    } else {
        // A strange place to be - a deleted object emitted a signal here.
        qWarning() << "sender deleted, cannot deliver signal";
    }
}

bool QtConnectionObject::match(QObject* sender, int signalIndex, JSObject* thisObject, JSObject *funcObject)
{
    if (m_originalObject == sender && m_signalIndex == signalIndex
        && thisObject == (JSObject*)m_thisObject && funcObject == (JSObject*)m_funcObject)
        return true;
    return false;
}

// ===============

template <typename T> QtArray<T>::QtArray(QList<T> list, QMetaType::Type type, PassRefPtr<RootObject> rootObject)
    : Array(rootObject)
    , m_list(list)
    , m_type(type)
{
    m_length = m_list.count();
}

template <typename T> QtArray<T>::~QtArray ()
{
}

template <typename T> RootObject* QtArray<T>::rootObject() const
{
    return _rootObject && _rootObject->isValid() ? _rootObject.get() : 0;
}

template <typename T> void QtArray<T>::setValueAt(ExecState *exec, unsigned int index, JSValue *aValue) const
{
    // QtScript sets the value, but doesn't forward it to the original source
    // (e.g. if you do 'object.intList[5] = 6', the object is not updated, but the
    // copy of the list is).
    int dist = -1;
    QVariant val = convertValueToQVariant(exec, aValue, m_type, &dist);

    if (dist >= 0) {
        m_list[index] = val.value<T>();
    }
}


template <typename T> JSValue* QtArray<T>::valueAt(ExecState *exec, unsigned int index) const
{
    if (index < m_length) {
        T val = m_list.at(index);
        return convertQVariantToValue(exec, rootObject(), QVariant::fromValue(val));
    }

    return jsUndefined();
}

// ===============

} }
