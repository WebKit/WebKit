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

#ifndef BINDINGS_QT_INSTANCE_H_
#define BINDINGS_QT_INSTANCE_H_

#include "runtime.h"
#include <qpointer.h>

class QObject;

namespace KJS {

namespace Bindings {

class QtClass;

class QtInstance : public Instance
{
public:
    QtInstance(QObject* instance);
        
    ~QtInstance ();
    
    virtual Class* getClass() const;
    
    QtInstance (const QtInstance &other);

    QtInstance &operator=(const QtInstance &other);
    
    virtual void begin();
    virtual void end();
    
    virtual JSValue* valueOf() const;
    virtual JSValue* defaultValue (JSType hint) const;

    virtual bool implementsCall() const;
    
    virtual JSValue* invokeMethod (ExecState *exec, const MethodList &method, const List &args);
    virtual JSValue* invokeDefaultMethod (ExecState *exec, const List &args);

    JSValue* stringValue() const;
    JSValue* numberValue() const;
    JSValue* booleanValue() const;
    
    QObject* getObject() const { return _object; }

private:
    mutable QtClass* _class;
    QPointer<QObject> _object;
};

} // namespace Bindings

} // namespace KJS

#endif
