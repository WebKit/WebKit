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
#include "identifier.h"

#include "qt_class.h"
#include "qt_instance.h"
#include "qt_runtime.h"

#include <qmetaobject.h>
#include <qdebug.h>

namespace KJS {
namespace Bindings {

QtClass::QtClass(const QMetaObject* mo)
    : metaObject(mo)
{
}

QtClass::~QtClass()
{
}
    
typedef HashMap<const QMetaObject*, QtClass*> ClassesByMetaObject;
static ClassesByMetaObject* classesByMetaObject = 0;

QtClass* QtClass::classForObject(QObject* o)
{
    if (!classesByMetaObject)
        classesByMetaObject = new ClassesByMetaObject;

    const QMetaObject* mo = o->metaObject();
    QtClass* aClass = classesByMetaObject->get(mo);
    if (!aClass) {
        aClass = new QtClass(mo);
        classesByMetaObject->set(mo, aClass);
    }

    return aClass;
}

const char* QtClass::name() const
{
    return metaObject->className();
}

MethodList QtClass::methodsNamed(const Identifier& identifier, Instance*) const
{
    MethodList methodList;

    const char* name = identifier.ascii();

    int count = metaObject->methodCount();
    for (int i = 0; i < count; ++i) {
        const QMetaMethod m = metaObject->method(i);
        if (m.methodType() == QMetaMethod::Signal)
            continue;
        if (m.access() != QMetaMethod::Public)
            continue;
        QByteArray signature = m.signature();
        signature.truncate(signature.indexOf('('));
        if (signature == name) {
            Method* method = new QtMethod(metaObject, i, signature, m.parameterTypes().size()); 
            methodList.append(method);
            break;
        }
    }
    
    return methodList;
}


Field* QtClass::fieldNamed(const Identifier& identifier, Instance*) const
{
    int index = metaObject->indexOfProperty(identifier.ascii());
    if (index < 0)
        return 0;

    return new QtField(metaObject->property(index));
}

}
}

