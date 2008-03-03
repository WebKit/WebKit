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
#include "runtime_method.h"
#include "protect.h"

#include <qbytearray.h>
#include <qmetaobject.h>
#include <qpointer.h>

namespace KJS {
namespace Bindings {

class QtInstance;

class QtField : public Field {
public:

    typedef enum {
        MetaProperty,
        DynamicProperty,
        ChildObject
    } QtFieldType;

    QtField(const QMetaProperty &p)
        : m_type(MetaProperty), m_property(p)
        {}

    QtField(const QByteArray &b)
        : m_type(DynamicProperty), m_dynamicProperty(b)
        {}

    QtField(QObject *child)
        : m_type(ChildObject), m_childObject(child)
        {}

    virtual JSValue* valueFromInstance(ExecState*, const Instance*) const;
    virtual void setValueToInstance(ExecState*, const Instance*, JSValue*) const;
    virtual const char* name() const;
    QtFieldType fieldType() const {return m_type;}
private:
    QtFieldType m_type;
    QByteArray m_dynamicProperty;
    QMetaProperty m_property;
    QPointer<QObject> m_childObject;
};


class QtMethod : public Method
{
public:
    QtMethod(const QMetaObject *mo, int i, const QByteArray &ident, int numParameters)
        : m_metaObject(mo),
          m_index(i),
          m_identifier(ident),
          m_nParams(numParameters)
        { }

    virtual const char* name() const { return m_identifier.constData(); }
    virtual int numParameters() const { return m_nParams; }

private:
    friend class QtInstance;
    const QMetaObject *m_metaObject;
    int m_index;
    QByteArray m_identifier;
    int m_nParams;
};


template <typename T> class QtArray : public Array
{
public:
    QtArray(QList<T> list, QMetaType::Type type, PassRefPtr<RootObject>);
    virtual ~QtArray();

    RootObject* rootObject() const;

    virtual void setValueAt(ExecState *exec, unsigned int index, JSValue *aValue) const;
    virtual JSValue *valueAt(ExecState *exec, unsigned int index) const;
    virtual unsigned int getLength() const {return m_length;}

private:
    mutable QList<T> m_list; // setValueAt is const!
    unsigned int m_length;
    QMetaType::Type m_type;
};

// Based on RuntimeMethod

// Extra data classes (to avoid the CELL_SIZE limit on JS objects)

class QtRuntimeMethodData {
    public:
        virtual ~QtRuntimeMethodData();
        RefPtr<QtInstance> m_instance;
};

class QtRuntimeConnectionMethod;
class QtRuntimeMetaMethodData : public QtRuntimeMethodData {
    public:
        ~QtRuntimeMetaMethodData();
        QByteArray m_signature;
        bool m_allowPrivate;
        int m_index;
        QtRuntimeConnectionMethod *m_connect;
        QtRuntimeConnectionMethod *m_disconnect;
};

class QtRuntimeConnectionMethodData : public QtRuntimeMethodData {
    public:
        ~QtRuntimeConnectionMethodData();
        QByteArray m_signature;
        int m_index;
        bool m_isConnect;
};

// Common base class (doesn't really do anything interesting)
class QtRuntimeMethod : public InternalFunctionImp
{
public:
    virtual ~QtRuntimeMethod();

    virtual CodeType codeType() const;
    virtual Completion execute(ExecState *exec);

protected:
    QtRuntimeMethodData *d_func() const {return d_ptr;}
    QtRuntimeMethod(QtRuntimeMethodData *dd, ExecState *exec, const Identifier &n, PassRefPtr<QtInstance> inst);
    QtRuntimeMethodData *d_ptr;
};

class QtRuntimeMetaMethod : public QtRuntimeMethod
{
public:
    QtRuntimeMetaMethod(ExecState *exec, const Identifier &n, PassRefPtr<QtInstance> inst, int index, const QByteArray& signature, bool allowPrivate);

    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);

    virtual void mark();

protected:
    QtRuntimeMetaMethodData* d_func() const {return reinterpret_cast<QtRuntimeMetaMethodData*>(d_ptr);}

private:
    static JSValue *lengthGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
    static JSValue *connectGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
    static JSValue *disconnectGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
};

class QtConnectionObject;
class QtRuntimeConnectionMethod : public QtRuntimeMethod
{
public:
    QtRuntimeConnectionMethod(ExecState *exec, const Identifier &n, bool isConnect, PassRefPtr<QtInstance> inst, int index, const QByteArray& signature );

    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);

protected:
    QtRuntimeConnectionMethodData* d_func() const {return reinterpret_cast<QtRuntimeConnectionMethodData*>(d_ptr);}

private:
    static JSValue *lengthGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
    static QMultiMap<QObject *, QtConnectionObject *> connections;
    friend class QtConnectionObject;
};

class QtConnectionObject: public QObject
{
public:
    QtConnectionObject(PassRefPtr<QtInstance> instance, int signalIndex, JSObject* thisObject, JSObject* funcObject);
    ~QtConnectionObject();

    static const QMetaObject staticMetaObject;
    virtual const QMetaObject *metaObject() const;
    virtual void *qt_metacast(const char *);
    virtual int qt_metacall(QMetaObject::Call, int, void **argv);

    bool match(QObject *sender, int signalIndex, JSObject* thisObject, JSObject *funcObject);

    // actual slot:
    void execute(void **argv);

private:
    RefPtr<QtInstance> m_instance;
    int m_signalIndex;
    QObject* m_originalObject; // only used as a key, not dereferenced
    ProtectedPtr<JSObject> m_thisObject;
    ProtectedPtr<JSObject> m_funcObject;
};

} // namespace Bindings
} // namespace KJS

#endif
