/*
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef qt_instance_h
#define qt_instance_h

#include "BridgeJSC.h"
#include <QPointer>
#include <QStack>
#include "Weak.h"
#include "runtime_root.h"
#include <qhash.h>
#include <qset.h>

namespace JSC {

namespace Bindings {

class QtClass;
class QtField;
class QtRuntimeMetaMethod;

class QtInstance : public Instance {
public:
    enum ValueOwnership {
        QtOwnership,
        ScriptOwnership,
        AutoOwnership
    };

    ~QtInstance();

    virtual Class* getClass() const;
    virtual RuntimeObject* newRuntimeObject(ExecState*);

    virtual void begin();
    virtual void end();

    virtual JSValue valueOf(ExecState*) const;
    virtual JSValue defaultValue(ExecState*, PreferredPrimitiveType) const;

    virtual JSValue getMethod(ExecState*, PropertyName);
    virtual JSValue invokeMethod(ExecState*, RuntimeMethod*);

    virtual void getPropertyNames(ExecState*, PropertyNameArray&);

    JSValue stringValue(ExecState* exec) const;
    JSValue numberValue(ExecState* exec) const;
    JSValue booleanValue() const;

    QObject* getObject() const { return m_object.data(); }
    QObject* hashKey() const { return m_hashkey; }

    static PassRefPtr<QtInstance> getQtInstance(QObject*, PassRefPtr<RootObject>, ValueOwnership);

    virtual bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
    virtual void put(JSObject*, ExecState*, PropertyName, JSValue, PutPropertySlot&);

    void removeUnusedMethods();

    static QtInstance* getInstance(JSObject*);

    class QtSenderStack {
    public:
        QObject* top() const { return m_stack.isEmpty() ? 0 : m_stack.top(); }
        void push(QObject* object) { m_stack.push(object); }
        void pop() { Q_ASSERT(!m_stack.isEmpty()); m_stack.pop(); }
    private:
        QStack<QObject*> m_stack;
    };

    // Used to implement '__qt_sender__'.
    static QtSenderStack* qtSenderStack();

private:

    class QtWeakObjectReference {
    public:
        QtWeakObjectReference(JSObject* reference)
            : m_reference(reference)
        {
        }

        QtWeakObjectReference(const QtWeakObjectReference& source)
            : m_reference(source.m_reference.get())
        {
        }

        QtWeakObjectReference()
            : m_reference()
        {
        }

        QtWeakObjectReference& operator=(const QtWeakObjectReference& source)
        {
            m_reference = PassWeak<JSObject>(source.m_reference.get());
            return *this;
        }

        JSObject* get() const
        {
            return m_reference.get();
        }

    private:
        Weak<JSObject> m_reference;
    };

    static PassRefPtr<QtInstance> create(QObject *instance, PassRefPtr<RootObject> rootObject, ValueOwnership ownership)
    {
        return adoptRef(new QtInstance(instance, rootObject, ownership));
    }

    friend class QtClass;
    friend class QtField;
    QtInstance(QObject*, PassRefPtr<RootObject>, ValueOwnership); // Factory produced only..
    mutable QtClass* m_class;
    QPointer<QObject> m_object;
    QObject* m_hashkey;
    mutable QHash<QByteArray, QtWeakObjectReference> m_methods;
    mutable QHash<QString, QtField*> m_fields;
    ValueOwnership m_ownership;
};

} // namespace Bindings

} // namespace JSC

#endif
