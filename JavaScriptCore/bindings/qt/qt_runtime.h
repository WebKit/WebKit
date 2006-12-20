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

#ifndef BINDINGS_QT_RUNTIME_H_
#define BINDINGS_QT_RUNTIME_H_

#include "runtime.h"
#include <qbytearray.h>
#include <qmetaobject.h>

namespace KJS {
namespace Bindings {

class QtInstance;

class QtField : public Field {
public:
    QtField(const QMetaProperty &p)
        : property(p)
        {}

    virtual JSValue* valueFromInstance(ExecState*, const Instance*) const;
    virtual void setValueToInstance(ExecState*, const Instance*, JSValue*) const;
    virtual const char* name() const;
    virtual RuntimeType type() const { return ""; }

private:
    QMetaProperty property;
};


class QtMethod : public Method
{
public:
    QtMethod(const QMetaObject *mo, int i, const QByteArray &ident, int numParameters)
        : metaObject(mo),
          index(i),
          identifier(ident),
          nParams(numParameters)
        { }

    virtual const char* name() const { return identifier.constData(); }
    virtual int numParameters() const { return nParams; }

private:
    friend class QtInstance;
    const QMetaObject *metaObject;
    int index;
    QByteArray identifier;
    int nParams;
};

} // namespace Bindings
} // namespace KJS

#endif
