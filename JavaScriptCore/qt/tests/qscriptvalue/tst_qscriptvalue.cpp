/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "tst_qscriptvalue.h"
#include <QtCore/qnumeric.h>

tst_QScriptValue::tst_QScriptValue()
    : m_engine(0)
{
}

tst_QScriptValue::~tst_QScriptValue()
{
    delete m_engine;
}

void tst_QScriptValue::ctor()
{
    QScriptEngine eng;
    {
        QScriptValue v;
        QCOMPARE(v.isValid(), false);
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v(&eng, QScriptValue::UndefinedValue);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isUndefined(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.engine(), &eng);
    }
    {
        QScriptValue v(&eng, QScriptValue::NullValue);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNull(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.engine(), &eng);
    }
    {
        QScriptValue v(&eng, false);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isBoolean(), true);
        QCOMPARE(v.isBool(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toBoolean(), false);
        QCOMPARE(v.engine(), &eng);
    }
    {
        QScriptValue v(&eng, int(1));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), &eng);
    }
    {
        QScriptValue v(int(0x43211234));
        QVERIFY(v.isNumber());
        QCOMPARE(v.toInt32(), 0x43211234);
    }
    {
        QScriptValue v(&eng, uint(1));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), &eng);
    }
    {
        QScriptValue v(uint(0x43211234));
        QVERIFY(v.isNumber());
        QCOMPARE(v.toUInt32(), uint(0x43211234));
    }
    {
        QScriptValue v(&eng, 1.0);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), &eng);
    }
    {
        QScriptValue v(12345678910.5);
        QVERIFY(v.isNumber());
        QCOMPARE(v.toNumber(), 12345678910.5);
    }
    {
        QScriptValue v(&eng, "ciao");
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isString(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toString(), QLatin1String("ciao"));
        QCOMPARE(v.engine(), &eng);
    }
    {
        QScriptValue v(&eng, QString("ciao"));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isString(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toString(), QLatin1String("ciao"));
        QCOMPARE(v.engine(), &eng);
    }
    // copy constructor, operator=
    {
        QScriptValue v(&eng, 1.0);
        QScriptValue v2(v);
        QCOMPARE(v2.strictlyEquals(v), true);
        QCOMPARE(v2.engine(), &eng);

        QScriptValue v3(v);
        QCOMPARE(v3.strictlyEquals(v), true);
        QCOMPARE(v3.strictlyEquals(v2), true);
        QCOMPARE(v3.engine(), &eng);

        QScriptValue v4(&eng, 2.0);
        QCOMPARE(v4.strictlyEquals(v), false);
        v3 = v4;
        QCOMPARE(v3.strictlyEquals(v), false);
        QCOMPARE(v3.strictlyEquals(v4), true);

        v2 = QScriptValue();
        QCOMPARE(v2.strictlyEquals(v), false);
        QCOMPARE(v.toNumber(), 1.0);

        QScriptValue v5(v);
        QCOMPARE(v5.strictlyEquals(v), true);
        v = QScriptValue();
        QCOMPARE(v5.strictlyEquals(v), false);
        QCOMPARE(v5.toNumber(), 1.0);
    }

    // constructors that take no engine argument
    {
        QScriptValue v(QScriptValue::UndefinedValue);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isUndefined(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v(QScriptValue::NullValue);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNull(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v(false);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isBoolean(), true);
        QCOMPARE(v.isBool(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toBoolean(), false);
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v(int(1));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v(uint(1));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v(1.0);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v("ciao");
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isString(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toString(), QLatin1String("ciao"));
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v(QString("ciao"));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isString(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toString(), QLatin1String("ciao"));
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    // copy constructor, operator=
    {
        QScriptValue v(1.0);
        QScriptValue v2(v);
        QCOMPARE(v2.strictlyEquals(v), true);
        QCOMPARE(v2.engine(), (QScriptEngine*)0);

        QScriptValue v3(v);
        QCOMPARE(v3.strictlyEquals(v), true);
        QCOMPARE(v3.strictlyEquals(v2), true);
        QCOMPARE(v3.engine(), (QScriptEngine*)0);

        QScriptValue v4(2.0);
        QCOMPARE(v4.strictlyEquals(v), false);
        v3 = v4;
        QCOMPARE(v3.strictlyEquals(v), false);
        QCOMPARE(v3.strictlyEquals(v4), true);

        v2 = QScriptValue();
        QCOMPARE(v2.strictlyEquals(v), false);
        QCOMPARE(v.toNumber(), 1.0);

        QScriptValue v5(v);
        QCOMPARE(v5.strictlyEquals(v), true);
        v = QScriptValue();
        QCOMPARE(v5.strictlyEquals(v), false);
        QCOMPARE(v5.toNumber(), 1.0);
    }

    // 0 engine
    QVERIFY(QScriptValue(0, QScriptValue::UndefinedValue).isUndefined());
    QVERIFY(QScriptValue(0, QScriptValue::NullValue).isNull());
    QVERIFY(QScriptValue(0, false).isBool());
    QVERIFY(QScriptValue(0, int(1)).isNumber());
    QVERIFY(QScriptValue(0, uint(1)).isNumber());
    QVERIFY(QScriptValue(0, 1.0).isNumber());
    QVERIFY(QScriptValue(0, "ciao").isString());
    QVERIFY(QScriptValue(0, QString("ciao")).isString());
}

void tst_QScriptValue::getPropertySimple_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QString>("desc");
    QTest::addColumn<bool>("isArrayIndex");

    QTest::newRow("new Array()")
            << QString::fromAscii("new Array()")
            << QString::fromAscii("length")
            << QString::fromAscii("0")
            << false;
    QTest::newRow("new Object().length")
            << QString::fromAscii("new Object()")
            << QString::fromAscii("length")
            << QString::fromAscii("") // Undefined is an invalid property.
            << false;
    QTest::newRow("new Object().toString")
            << QString::fromAscii("new Object()")
            << QString::fromAscii("toString")
            << QString::fromAscii("function toString() {\n    [native code]\n}")
            << false;
    QTest::newRow("[1,2,3,4]")
            << QString::fromAscii("[1,2,3,'s',4]")
            << QString::fromAscii("2")
            << QString::fromAscii("3")
            << true;
    QTest::newRow("[1,3,'a','b']")
            << QString::fromAscii("[1,3,'a','b']")
            << QString::fromAscii("3")
            << QString::fromAscii("b")
            << true;
    QTest::newRow("[4,5]")
            << QString::fromAscii("[4,5]")
            << QString::fromAscii("123")
            << QString::fromAscii("") // Undefined is an invalid property.
            << true;
    QTest::newRow("[1,3,4]")
            << QString::fromAscii("[1,3,4]")
            << QString::fromAscii("abc")
            << QString::fromAscii("") // Undefined is an invalid property.
            << true;
}

void tst_QScriptValue::getPropertySimple()
{
    QFETCH(QString, code);
    QFETCH(QString, propertyName);
    QFETCH(QString, desc);

    QScriptEngine engine;
    QScriptValue object = engine.evaluate(code);
    QVERIFY(object.isValid());
    {
        QScriptValue property = object.property(propertyName);
        QCOMPARE(property.toString(), desc);
    }
    {
        QScriptString name = engine.toStringHandle(propertyName);
        QScriptValue property = object.property(name);
        QCOMPARE(property.toString(), desc);
    }
    {
        bool ok;
        quint32 idx = engine.toStringHandle(propertyName).toArrayIndex(&ok);
        if (ok) {
            QScriptValue property = object.property(idx);
            QCOMPARE(property.toString(), desc);
        }
    }
}

void tst_QScriptValue::setPropertySimple()
{
    QScriptEngine engine;
    {
        QScriptValue invalid;
        QScriptValue property(1234);

        invalid.setProperty("aaa", property);
        invalid.setProperty(13, property);
        invalid.setProperty(engine.toStringHandle("aaa"), property);

        QVERIFY(!invalid.property("aaa").isValid());
        QVERIFY(!invalid.property(13).isValid());
        QVERIFY(!invalid.property(engine.toStringHandle("aaa")).isValid());
    }
    {
        QScriptValue object = engine.newObject();
        QScriptValue property;

        object.setProperty(13, property);
        object.setProperty("aaa", property);
        object.setProperty(engine.toStringHandle("aaa"), property);

        QVERIFY(!object.property(13).isValid());
        QVERIFY(!object.property("aaa").isValid());
        QVERIFY(!object.property(engine.toStringHandle("aaa")).isValid());
    }
    {
        // Check if setting an invalid property works as deleteProperty.
        QScriptValue object = engine.evaluate("o = {13: 0, 'aaa': 3, 'bbb': 1}");
        QScriptValue property;

        QVERIFY(object.property(13).isValid());
        QVERIFY(object.property("aaa").isValid());
        QVERIFY(object.property(engine.toStringHandle("aaa")).isValid());

        object.setProperty(13, property);
        object.setProperty("aaa", property);
        object.setProperty(engine.toStringHandle("bbb"), property);

        QVERIFY(!object.property(13).isValid());
        QVERIFY(!object.property("aaa").isValid());
        QVERIFY(!object.property(engine.toStringHandle("aaa")).isValid());
    }
    {
        QScriptValue object = engine.evaluate("new Object");
        QVERIFY(object.isObject());
        QScriptValue property = object.property("foo");
        QVERIFY(!property.isValid());
        property = QScriptValue(2);
        object.setProperty("foo", property);
        QVERIFY(object.property("foo").isNumber());
        QVERIFY(object.property("foo").toNumber() == 2);
    }
    {
        QScriptValue o1 = engine.evaluate("o1 = new Object; o1");
        QScriptValue o2 = engine.evaluate("o2 = new Object; o2");
        QVERIFY(engine.evaluate("o1.__proto__ = o2; o1.__proto__ === o2").toBool());
        QVERIFY(engine.evaluate("o2.foo = 22; o1.foo == 22").toBool());
        QVERIFY(o1.property("foo").toString() == "22");
        o2.setProperty("foo", QScriptValue(&engine, 456.0));
        QVERIFY(engine.evaluate("o1.foo == 456").toBool());
        QVERIFY(o1.property("foo").isNumber());
    }
}

void tst_QScriptValue::getPropertyResolveFlag()
{
    QScriptEngine engine;
    QScriptValue object1 = engine.evaluate("o1 = new Object();");
    QScriptValue object2 = engine.evaluate("o2 = new Object(); o1.__proto__ = o2; o2");
    QScriptValue number(&engine, 456.0);
    QVERIFY(object1.isObject());
    QVERIFY(object2.isObject());
    QVERIFY(number.isNumber());

    object2.setProperty("propertyInPrototype", number);
    QVERIFY(object2.property("propertyInPrototype").isNumber());
    // default is ResolvePrototype
    QCOMPARE(object1.property("propertyInPrototype").strictlyEquals(number), true);
    QCOMPARE(object1.property("propertyInPrototype", QScriptValue::ResolvePrototype)
             .strictlyEquals(number), true);
    QCOMPARE(object1.property("propertyInPrototype", QScriptValue::ResolveLocal).isValid(), false);
}

void tst_QScriptValue::getSetProperty()
{
    QScriptEngine eng;

    QScriptValue object = eng.newObject();

    QScriptValue str = QScriptValue(&eng, "bar");
    object.setProperty("foo", str);
    QCOMPARE(object.property("foo").toString(), str.toString());

    QScriptValue num = QScriptValue(&eng, 123.0);
    object.setProperty("baz", num);
    QCOMPARE(object.property("baz").toNumber(), num.toNumber());

    QScriptValue strstr = QScriptValue("bar");
    QCOMPARE(strstr.engine(), (QScriptEngine *)0);
    object.setProperty("foo", strstr);
    QCOMPARE(object.property("foo").toString(), strstr.toString());
    QCOMPARE(strstr.engine(), &eng); // the value has been bound to the engine

    QScriptValue numnum = QScriptValue(123.0);
    object.setProperty("baz", numnum);
    QCOMPARE(object.property("baz").toNumber(), numnum.toNumber());

    QScriptValue inv;
    inv.setProperty("foo", num);
    QCOMPARE(inv.property("foo").isValid(), false);

    QScriptValue array = eng.newArray();
    array.setProperty(0, num);
    QCOMPARE(array.property(0).toNumber(), num.toNumber());
    QCOMPARE(array.property("0").toNumber(), num.toNumber());
    QCOMPARE(array.property("length").toUInt32(), quint32(1));
    array.setProperty(1, str);
    QCOMPARE(array.property(1).toString(), str.toString());
    QCOMPARE(array.property("1").toString(), str.toString());
    QCOMPARE(array.property("length").toUInt32(), quint32(2));
    array.setProperty("length", QScriptValue(&eng, 1));
    QCOMPARE(array.property("length").toUInt32(), quint32(1));
    QCOMPARE(array.property(1).isValid(), false);

    // task 162051 -- detecting whether the property is an array index or not
    QVERIFY(eng.evaluate("a = []; a['00'] = 123; a['00']").strictlyEquals(QScriptValue(&eng, 123)));
    QVERIFY(eng.evaluate("a.length").strictlyEquals(QScriptValue(&eng, 0)));
    QVERIFY(eng.evaluate("a.hasOwnProperty('00')").strictlyEquals(QScriptValue(&eng, true)));
    QVERIFY(eng.evaluate("a.hasOwnProperty('0')").strictlyEquals(QScriptValue(&eng, false)));
    QVERIFY(eng.evaluate("a[0]").isUndefined());
    QVERIFY(eng.evaluate("a[0.5] = 456; a[0.5]").strictlyEquals(QScriptValue(&eng, 456)));
    QVERIFY(eng.evaluate("a.length").strictlyEquals(QScriptValue(&eng, 0)));
    QVERIFY(eng.evaluate("a.hasOwnProperty('0.5')").strictlyEquals(QScriptValue(&eng, true)));
    QVERIFY(eng.evaluate("a[0]").isUndefined());
    QVERIFY(eng.evaluate("a[0] = 789; a[0]").strictlyEquals(QScriptValue(&eng, 789)));
    QVERIFY(eng.evaluate("a.length").strictlyEquals(QScriptValue(&eng, 1)));

    // task 183072 -- 0x800000000 is not an array index
    eng.evaluate("a = []; a[0x800000000] = 123");
    QVERIFY(eng.evaluate("a.length").strictlyEquals(QScriptValue(&eng, 0)));
    QVERIFY(eng.evaluate("a[0]").isUndefined());
    QVERIFY(eng.evaluate("a[0x800000000]").strictlyEquals(QScriptValue(&eng, 123)));

    QScriptEngine otherEngine;
    QScriptValue otherNum = QScriptValue(&otherEngine, 123);
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::setProperty() failed: cannot set value created in a different engine");
    object.setProperty("oof", otherNum);
    QCOMPARE(object.property("oof").isValid(), false);

    // test ResolveMode
    QScriptValue object2 = eng.newObject();
    object.setPrototype(object2);
    QScriptValue num2 = QScriptValue(&eng, 456.0);
    object2.setProperty("propertyInPrototype", num2);
    // default is ResolvePrototype
    QCOMPARE(object.property("propertyInPrototype")
             .strictlyEquals(num2), true);
    QCOMPARE(object.property("propertyInPrototype", QScriptValue::ResolvePrototype)
             .strictlyEquals(num2), true);
    QCOMPARE(object.property("propertyInPrototype", QScriptValue::ResolveLocal)
             .isValid(), false);
    QEXPECT_FAIL("", "QScriptValue::ResolveScope is not implemented", Continue);
    QCOMPARE(object.property("propertyInPrototype", QScriptValue::ResolveScope)
             .strictlyEquals(num2), false);
    QCOMPARE(object.property("propertyInPrototype", QScriptValue::ResolveFull)
             .strictlyEquals(num2), true);

    // test property removal (setProperty(QScriptValue()))
    QScriptValue object3 = eng.newObject();
    object3.setProperty("foo", num);
    QCOMPARE(object3.property("foo").strictlyEquals(num), true);
    object3.setProperty("bar", str);
    QCOMPARE(object3.property("bar").strictlyEquals(str), true);
    object3.setProperty("foo", QScriptValue());
    QCOMPARE(object3.property("foo").isValid(), false);
    QCOMPARE(object3.property("bar").strictlyEquals(str), true);
    object3.setProperty("foo", num);
    QCOMPARE(object3.property("foo").strictlyEquals(num), true);
    QCOMPARE(object3.property("bar").strictlyEquals(str), true);
    object3.setProperty("bar", QScriptValue());
    QCOMPARE(object3.property("bar").isValid(), false);
    QCOMPARE(object3.property("foo").strictlyEquals(num), true);
    object3.setProperty("foo", QScriptValue());
    object3.setProperty("foo", QScriptValue());

    eng.globalObject().setProperty("object3", object3);
    QCOMPARE(eng.evaluate("object3.hasOwnProperty('foo')")
             .strictlyEquals(QScriptValue(&eng, false)), true);
    object3.setProperty("foo", num);
    QCOMPARE(eng.evaluate("object3.hasOwnProperty('foo')")
             .strictlyEquals(QScriptValue(&eng, true)), true);
    eng.globalObject().setProperty("object3", QScriptValue());
    QCOMPARE(eng.evaluate("this.hasOwnProperty('object3')")
             .strictlyEquals(QScriptValue(&eng, false)), true);

    eng.globalObject().setProperty("object", object);

    // ReadOnly
    object.setProperty("readOnlyProperty", num, QScriptValue::ReadOnly);
    // QCOMPARE(object.propertyFlags("readOnlyProperty"), QScriptValue::ReadOnly);
    QCOMPARE(object.property("readOnlyProperty").strictlyEquals(num), true);
    eng.evaluate("object.readOnlyProperty = !object.readOnlyProperty");
    QCOMPARE(object.property("readOnlyProperty").strictlyEquals(num), true);
    // Should still be part of enumeration.
    {
        QScriptValue ret = eng.evaluate(
            "found = false;"
            "for (var p in object) {"
            "  if (p == 'readOnlyProperty') {"
            "    found = true; break;"
            "  }"
            "} found");
        QCOMPARE(ret.strictlyEquals(QScriptValue(&eng, true)), true);
    }
    // should still be deletable
    {
        QScriptValue ret = eng.evaluate("delete object.readOnlyProperty");
        QCOMPARE(ret.strictlyEquals(QScriptValue(&eng, true)), true);
        QCOMPARE(object.property("readOnlyProperty").isValid(), false);
    }

    // Undeletable
    object.setProperty("undeletableProperty", num, QScriptValue::Undeletable);
    // QCOMPARE(object.propertyFlags("undeletableProperty"), QScriptValue::Undeletable);
    QCOMPARE(object.property("undeletableProperty").strictlyEquals(num), true);
    {
        QScriptValue ret = eng.evaluate("delete object.undeletableProperty");
        QCOMPARE(ret.strictlyEquals(QScriptValue(&eng, true)), false);
        QCOMPARE(object.property("undeletableProperty").strictlyEquals(num), true);
    }
    // should still be writable
    eng.evaluate("object.undeletableProperty = object.undeletableProperty + 1");
    QCOMPARE(object.property("undeletableProperty").toNumber(), num.toNumber() + 1);
    // should still be part of enumeration
    {
        QScriptValue ret = eng.evaluate(
            "found = false;"
            "for (var p in object) {"
            "  if (p == 'undeletableProperty') {"
            "    found = true; break;"
            "  }"
            "} found");
        QCOMPARE(ret.strictlyEquals(QScriptValue(&eng, true)), true);
    }
    // should still be deletable from C++
    object.setProperty("undeletableProperty", QScriptValue());
    QEXPECT_FAIL("", "With JSC-based back-end, undeletable properties can't be deleted from C++", Continue);
    QVERIFY(!object.property("undeletableProperty").isValid());
    // QEXPECT_FAIL("", "With JSC-based back-end, undeletable properties can't be deleted from C++", Continue);
    // QCOMPARE(object.propertyFlags("undeletableProperty"), 0);

    // SkipInEnumeration
    object.setProperty("dontEnumProperty", num, QScriptValue::SkipInEnumeration);
    // QCOMPARE(object.propertyFlags("dontEnumProperty"), QScriptValue::SkipInEnumeration);
    QCOMPARE(object.property("dontEnumProperty").strictlyEquals(num), true);
    // should not be part of enumeration
    {
        QScriptValue ret = eng.evaluate(
            "found = false;"
            "for (var p in object) {"
            "  if (p == 'dontEnumProperty') {"
            "    found = true; break;"
            "  }"
            "} found");
        QCOMPARE(ret.strictlyEquals(QScriptValue(&eng, false)), true);
    }
    // should still be writable
    eng.evaluate("object.dontEnumProperty = object.dontEnumProperty + 1");
    QCOMPARE(object.property("dontEnumProperty").toNumber(), num.toNumber() + 1);
    // should still be deletable
    {
        QScriptValue ret = eng.evaluate("delete object.dontEnumProperty");
        QCOMPARE(ret.strictlyEquals(QScriptValue(&eng, true)), true);
        QCOMPARE(object.property("dontEnumProperty").isValid(), false);
    }

    // change flags
    object.setProperty("flagProperty", str);
    // QCOMPARE(object.propertyFlags("flagProperty"), static_cast<QScriptValue::PropertyFlags>(0));

    object.setProperty("flagProperty", str, QScriptValue::ReadOnly);
    // QCOMPARE(object.propertyFlags("flagProperty"), QScriptValue::ReadOnly);

    // object.setProperty("flagProperty", str, object.propertyFlags("flagProperty") | QScriptValue::SkipInEnumeration);
    // QCOMPARE(object.propertyFlags("flagProperty"), QScriptValue::ReadOnly | QScriptValue::SkipInEnumeration);

    object.setProperty("flagProperty", str, QScriptValue::KeepExistingFlags);
    // QCOMPARE(object.propertyFlags("flagProperty"), QScriptValue::ReadOnly | QScriptValue::SkipInEnumeration);

    object.setProperty("flagProperty", str, QScriptValue::UserRange);
    // QCOMPARE(object.propertyFlags("flagProperty"), QScriptValue::UserRange);

    // flags of property in the prototype
    {
        QScriptValue object2 = eng.newObject();
        object2.setPrototype(object);
        // QCOMPARE(object2.propertyFlags("flagProperty", QScriptValue::ResolveLocal), 0);
        // QCOMPARE(object2.propertyFlags("flagProperty"), QScriptValue::UserRange);
    }

    // using interned strings
    QScriptString foo = eng.toStringHandle("foo");

    object.setProperty(foo, QScriptValue());
    QVERIFY(!object.property(foo).isValid());

    object.setProperty(foo, num);
    QVERIFY(object.property(foo).strictlyEquals(num));
    QVERIFY(object.property("foo").strictlyEquals(num));
    // QVERIFY(object.propertyFlags(foo) == 0);
}

void tst_QScriptValue::toStringSimple_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("result");

    QTest::newRow("string") << QString::fromAscii("'hello'") << QString::fromAscii("hello");
    QTest::newRow("string utf") << QString::fromUtf8("'ąśćżźółńę'") << QString::fromUtf8("ąśćżźółńę");
    QTest::newRow("expression") << QString::fromAscii("1 + 4") << QString::fromAscii("5");
    QTest::newRow("null") << QString::fromAscii("null") << QString::fromAscii("null");
    QTest::newRow("boolean") << QString::fromAscii("false") << QString::fromAscii("false");
    QTest::newRow("undefined") << QString::fromAscii("undefined") << QString::fromAscii("undefined");
    QTest::newRow("object") << QString::fromAscii("new Object") << QString::fromAscii("[object Object]");
}

/* Test conversion to string from different JSC types */
void tst_QScriptValue::toStringSimple()
{
    QFETCH(QString, code);
    QFETCH(QString, result);

    QScriptEngine engine;
    QCOMPARE(engine.evaluate(code).toString(), result);
}

void tst_QScriptValue::copyConstructor_data()
{
    QScriptEngine engine;
    QScriptValue nnumber(123);
    QScriptValue nstring("ping");
    QScriptValue number(engine.evaluate("1"));
    QScriptValue string(engine.evaluate("'foo'"));
    QScriptValue object(engine.evaluate("new Object"));
    QScriptValue undefined(engine.evaluate("undefined"));
    QScriptValue null(engine.evaluate("null"));

    QTest::addColumn<QScriptValue>("value");
    QTest::addColumn<QString>("result");

    QTest::newRow("native number") << nnumber << QString::number(123);
    QTest::newRow("native string") << nstring << QString("ping");
    QTest::newRow("number") << number << QString::fromAscii("1");
    QTest::newRow("string") << string << QString::fromAscii("foo");
    QTest::newRow("object") << object << QString::fromAscii("[object Object]");
    QTest::newRow("undefined") << undefined << QString::fromAscii("undefined");
    QTest::newRow("null") << null << QString::fromAscii("null");
}

void tst_QScriptValue::copyConstructor()
{
    QFETCH(QScriptValue, value);
    QFETCH(QString, result);

    QVERIFY(value.isValid());
    QScriptValue tmp(value);
    QVERIFY(tmp.isValid());
    QCOMPARE(tmp.toString(), result);
}

void tst_QScriptValue::assignOperator_data()
{
    copyConstructor_data();
}

void tst_QScriptValue::assignOperator()
{
    QFETCH(QScriptValue, value);
    QFETCH(QString, result);

    QScriptValue tmp;
    tmp = value;
    QVERIFY(tmp.isValid());
    QCOMPARE(tmp.toString(), result);
}

/* Test internal data sharing between a diffrenet QScriptValue. */
void tst_QScriptValue::dataSharing()
{
    QScriptEngine engine;
    QScriptValue v1;
    QScriptValue v2(v1);

    v1 = engine.evaluate("1"); // v1 == 1 ; v2 invalid.
    QVERIFY(v1.isValid());
    QVERIFY(!v2.isValid());

    v2 = v1; // v1 == 1; v2 == 1.
    QVERIFY(v1.isValid());
    QVERIFY(v2.isValid());

    v1 = engine.evaluate("obj = new Date"); // v1 == [object Date] ; v2 == 1.
    QVERIFY(v1.isValid());
    QVERIFY(v2.isValid());
    QVERIFY(v2.toString() != v1.toString());

    // TODO add object manipulation (v1 and v2 point to the same object).
}

void tst_QScriptValue::constructors_data()
{
    QScriptEngine engine;

    QTest::addColumn<QScriptValue>("value");
    QTest::addColumn<QString>("string");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<bool>("object");

    QTest::newRow("invalid") << QScriptValue() << QString() << false << false;
    QTest::newRow("number") << QScriptValue(-21) << QString::number(-21) << true << false;
    QTest::newRow("bool") << QScriptValue(true) << QString::fromAscii("true") << true << false;
    QTest::newRow("double") << QScriptValue(21.12) << QString::number(21.12) << true << false;
    QTest::newRow("string") << QScriptValue("AlaMaKota") << QString::fromAscii("AlaMaKota") << true << false;
    QTest::newRow("object") << engine.evaluate("new Object") << QString::fromAscii("[object Object]") << true << true;
    QTest::newRow("null") << QScriptValue(QScriptValue::NullValue)<< QString::fromAscii("null") << true << false;
    QTest::newRow("undef") << QScriptValue(QScriptValue::UndefinedValue)<< QString::fromAscii("undefined") << true << false;
}

void tst_QScriptValue::constructors()
{
    QFETCH(QScriptValue, value);
    QFETCH(QString, string);
    QFETCH(bool, valid);
    QFETCH(bool, object);

    QCOMPARE(value.isValid(), valid);
    QCOMPARE(value.toString(), string);
    QCOMPARE(value.isObject(), object);
}

void tst_QScriptValue::call()
{
    QScriptEngine engine;
    QScriptValue ping = engine.evaluate("( function() {return 'ping';} )");
    QScriptValue incr = engine.evaluate("( function(i) {return i + 1;} )");
    QScriptValue one(1);
    QScriptValue five(5);
    QScriptValue result;

    QVERIFY(one.isValid());
    QVERIFY(five.isValid());

    QVERIFY(ping.isValid());
    QVERIFY(ping.isFunction());
    result = ping.call();
    QVERIFY(result.isValid());
    QCOMPARE(result.toString(), QString::fromUtf8("ping"));

    QVERIFY(incr.isValid());
    QVERIFY(incr.isFunction());
    result = incr.call(QScriptValue(), QScriptValueList() << one);
    QVERIFY(result.isValid());
    QCOMPARE(result.toString(), QString("2"));

    QCOMPARE(incr.call(QScriptValue(), QScriptValueList() << five).toString(), QString::fromAscii("6"));

    QVERIFY(incr.call().isValid()); // Exception.
}

void tst_QScriptValue::getSetPrototype()
{
    QScriptEngine engine;
    QScriptValue object = engine.evaluate("new Object()");
    QScriptValue object2 = engine.evaluate("new Object()");
    object2.setPrototype(object);
    QCOMPARE(object2.prototype().strictlyEquals(object), true);

    QScriptValue inv;
    inv.setPrototype(object);
    QCOMPARE(inv.prototype().isValid(), false);

    QScriptEngine otherEngine;
    QScriptValue object3 = otherEngine.evaluate("new Object()");
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::setPrototype() failed: cannot set a prototype created in a different engine");
    object2.setPrototype(object3);
    QCOMPARE(object2.prototype().strictlyEquals(object), true);

    // cyclic prototypes
    {
        QScriptValue ret = engine.evaluate("o = { }; p = { }; o.__proto__ = p; p.__proto__ = o");
        QCOMPARE(ret.isError(), true);
        QCOMPARE(ret.toString(), QLatin1String("Error: cyclic __proto__ value"));
    }
    {
        QScriptValue ret = engine.evaluate("p.__proto__ = { }");
        QCOMPARE(ret.isError(), false);
    }

    QScriptValue old = object.prototype();
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::setPrototype() failed: cyclic prototype value");
    object.setPrototype(object);
    QCOMPARE(object.prototype().strictlyEquals(old), true);

    object2.setPrototype(object);
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::setPrototype() failed: cyclic prototype value");
    object.setPrototype(object2);
    QCOMPARE(object.prototype().strictlyEquals(old), true);
}

void tst_QScriptValue::toObjectSimple()
{
    QScriptEngine eng;

    QScriptValue undefined = eng.undefinedValue();
    QCOMPARE(undefined.toObject().isValid(), false);
    QScriptValue null = eng.nullValue();
    QCOMPARE(null.toObject().isValid(), false);
    QCOMPARE(QScriptValue().toObject().isValid(), false);

    QScriptValue falskt = QScriptValue(&eng, false);
    {
        QScriptValue tmp = falskt.toObject();
        QCOMPARE(tmp.isObject(), true);
        QCOMPARE(falskt.isObject(), false);
        QCOMPARE(tmp.toNumber(), falskt.toNumber());
    }

    QScriptValue sant = QScriptValue(&eng, true);
    {
        QScriptValue tmp = sant.toObject();
        QCOMPARE(tmp.isObject(), true);
        QCOMPARE(sant.isObject(), false);
        QCOMPARE(tmp.toNumber(), sant.toNumber());
    }

    QScriptValue number = QScriptValue(&eng, 123.0);
    {
        QScriptValue tmp = number.toObject();
        QCOMPARE(tmp.isObject(), true);
        QCOMPARE(number.isObject(), false);
        QCOMPARE(tmp.toNumber(), number.toNumber());
    }

    QScriptValue str = QScriptValue(&eng, QString("ciao"));
    {
        QScriptValue tmp = str.toObject();
        QCOMPARE(tmp.isObject(), true);
        QCOMPARE(str.isObject(), false);
        QCOMPARE(tmp.toString(), str.toString());
    }


    QScriptValue object = eng.evaluate("new Object");
    {
        QScriptValue tmp = object.toObject();
        QVERIFY(tmp.strictlyEquals(object));
        QCOMPARE(tmp.isObject(), true);
    }


    // V2 constructors: in this case, you have to use QScriptEngine::toObject()
    {
        QScriptValue undefined = QScriptValue(QScriptValue::UndefinedValue);
        QVERIFY(!undefined.toObject().isValid());
        QVERIFY(!eng.toObject(undefined).isValid());
        QVERIFY(!undefined.engine());

        QScriptValue null = QScriptValue(QScriptValue::NullValue);
        QVERIFY(!null.toObject().isValid());
        QVERIFY(!eng.toObject(null).isValid());
        QVERIFY(!null.engine());

        QScriptValue falskt = QScriptValue(false);
        QVERIFY(!falskt.toObject().isValid());
        QCOMPARE(falskt.isObject(), false);
        QVERIFY(!falskt.engine());
        {
            QScriptValue tmp = eng.toObject(falskt);
            QVERIFY(tmp.isObject());
            QVERIFY(tmp.toBool());
            QVERIFY(!falskt.isObject());
        }

        QScriptValue sant = QScriptValue(true);
        QVERIFY(!sant.toObject().isValid());
        QCOMPARE(sant.isObject(), false);
        QVERIFY(!sant.engine());
        {
            QScriptValue tmp = eng.toObject(sant);
            QVERIFY(tmp.isObject());
            QVERIFY(tmp.toBool());
            QVERIFY(!sant.isObject());
        }

        QScriptValue number = QScriptValue(123.0);
        QVERIFY(!number.toObject().isValid());
        QVERIFY(!number.engine());
        QCOMPARE(number.isObject(), false);
        {
            QScriptValue tmp = eng.toObject(number);
            QVERIFY(tmp.isObject());
            QCOMPARE(tmp.toInt32(), number.toInt32());
            QVERIFY(!number.isObject());
        }

        QScriptValue str = QScriptValue(QString::fromLatin1("ciao"));
        QVERIFY(!str.toObject().isValid());
        QVERIFY(!str.engine());
        QCOMPARE(str.isObject(), false);
        {
            QScriptValue tmp = eng.toObject(str);
            QVERIFY(tmp.isObject());
            QCOMPARE(tmp.toString(), QString::fromLatin1("ciao"));
            QVERIFY(!str.isObject());
        }
    }
}

void tst_QScriptValue::setProperty_data()
{
    QTest::addColumn<QScriptValue>("property");
    QTest::addColumn<int>("flag");

    QTest::newRow("int + keepExistingFlags") << QScriptValue(123456) << static_cast<int>(QScriptValue::KeepExistingFlags);
    QTest::newRow("int + undeletable") << QScriptValue(123456) << static_cast<int>(QScriptValue::Undeletable);
    QTest::newRow("int + readOnly") << QScriptValue(123456) << static_cast<int>(QScriptValue::ReadOnly);
    QTest::newRow("int + readOnly|undeletable") << QScriptValue(123456) << static_cast<int>(QScriptValue::ReadOnly | QScriptValue::Undeletable);
    QTest::newRow("int + skipInEnumeration") << QScriptValue(123456) << static_cast<int>(QScriptValue::SkipInEnumeration);
    QTest::newRow("int + skipInEnumeration|readOnly") << QScriptValue(123456) << static_cast<int>(QScriptValue::SkipInEnumeration | QScriptValue::ReadOnly);
    QTest::newRow("int + skipInEnumeration|undeletable") << QScriptValue(123456) << static_cast<int>(QScriptValue::SkipInEnumeration | QScriptValue::Undeletable);
    QTest::newRow("int + skipInEnumeration|readOnly|undeletable") << QScriptValue(123456) << static_cast<int>(QScriptValue::SkipInEnumeration | QScriptValue::ReadOnly | QScriptValue::Undeletable);
}

void tst_QScriptValue::setProperty()
{
    QFETCH(QScriptValue, property);
    QFETCH(int, flag);
    QScriptValue::PropertyFlags flags = static_cast<QScriptValue::PropertyFlag>(flag);

    QScriptEngine engine;
    QScriptValue object = engine.evaluate("o = new Object; o");
    QScriptValue proto = engine.evaluate("p = new Object; o.__proto__ = p; p");
    engine.evaluate("o.defined1 = 1");
    engine.evaluate("o.defined2 = 1");
    engine.evaluate("o[5] = 1");
    engine.evaluate("p.overloaded1 = 1");
    engine.evaluate("o.overloaded1 = 2");
    engine.evaluate("p[6] = 1");
    engine.evaluate("o[6] = 2");
    engine.evaluate("p.overloaded2 = 1");
    engine.evaluate("o.overloaded2 = 2");
    engine.evaluate("p.overloaded3 = 1");
    engine.evaluate("o.overloaded3 = 2");
    engine.evaluate("p[7] = 1");
    engine.evaluate("o[7] = 2");
    engine.evaluate("p.overloaded4 = 1");
    engine.evaluate("o.overloaded4 = 2");

    // tries to set undefined property directly on object.
    object.setProperty(QString::fromAscii("undefined1"), property, flags);
    QVERIFY(engine.evaluate("o.undefined1").strictlyEquals(property));
    object.setProperty(engine.toStringHandle("undefined2"), property, flags);
    QVERIFY(object.property("undefined2").strictlyEquals(property));
    object.setProperty(4, property, flags);
    QVERIFY(object.property(4).strictlyEquals(property));

    // tries to set defined property directly on object
    object.setProperty("defined1", property, flags);
    QVERIFY(engine.evaluate("o.defined1").strictlyEquals(property));
    object.setProperty(engine.toStringHandle("defined2"), property, flags);
    QVERIFY(object.property("defined2").strictlyEquals(property));
    object.setProperty(5, property, flags);
    QVERIFY(object.property(5).strictlyEquals(property));

    // tries to set overloaded property directly on object
    object.setProperty("overloaded1", property, flags);
    QVERIFY(engine.evaluate("o.overloaded1").strictlyEquals(property));
    object.setProperty(engine.toStringHandle("overloaded2"), property, flags);
    QVERIFY(object.property("overloaded2").strictlyEquals(property));
    object.setProperty(6, property, flags);
    QVERIFY(object.property(6).strictlyEquals(property));

    // tries to set overloaded property directly on prototype
    proto.setProperty("overloaded3", property, flags);
    QVERIFY(!engine.evaluate("o.overloaded3").strictlyEquals(property));
    proto.setProperty(engine.toStringHandle("overloaded4"), property, flags);
    QVERIFY(!object.property("overloaded4").strictlyEquals(property));
    proto.setProperty(7, property, flags);
    QVERIFY(!object.property(7).strictlyEquals(property));

    // tries to set undefined property directly on prototype
    proto.setProperty("undefined3", property, flags);
    QVERIFY(engine.evaluate("o.undefined3").strictlyEquals(property));
    proto.setProperty(engine.toStringHandle("undefined4"), property, flags);
    QVERIFY(object.property("undefined4").strictlyEquals(property));
    proto.setProperty(8, property, flags);
    QVERIFY(object.property(8).strictlyEquals(property));

    bool readOnly = flags & QScriptValue::ReadOnly;
    bool skipInEnumeration = flags & QScriptValue::SkipInEnumeration;
    bool undeletable = flags & QScriptValue::Undeletable;

    QVERIFY(readOnly == engine.evaluate("!Object.getOwnPropertyDescriptor(o, '4').writable").toBool());
    QEXPECT_FAIL("int + readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(readOnly == engine.evaluate("!Object.getOwnPropertyDescriptor(o, '5').writable").toBool());
    QEXPECT_FAIL("int + readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(readOnly == engine.evaluate("!Object.getOwnPropertyDescriptor(o, '6').writable").toBool());
    QEXPECT_FAIL("int + readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(readOnly == engine.evaluate("!Object.getOwnPropertyDescriptor(p, '7').writable").toBool());
    QVERIFY(readOnly == engine.evaluate("!Object.getOwnPropertyDescriptor(p, '8').writable").toBool());
    QVERIFY(readOnly == engine.evaluate("!Object.getOwnPropertyDescriptor(o, 'undefined1').writable").toBool());
    QVERIFY(readOnly == engine.evaluate("!Object.getOwnPropertyDescriptor(o, 'undefined2').writable").toBool());
    QVERIFY(readOnly == engine.evaluate("!Object.getOwnPropertyDescriptor(p, 'undefined3').writable").toBool());
    QVERIFY(readOnly == engine.evaluate("!Object.getOwnPropertyDescriptor(p, 'undefined4').writable").toBool());
    QEXPECT_FAIL("int + readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(readOnly == engine.evaluate("!Object.getOwnPropertyDescriptor(o, 'defined1').writable").toBool());
    QEXPECT_FAIL("int + readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(readOnly == engine.evaluate("!Object.getOwnPropertyDescriptor(o, 'defined2').writable").toBool());
    QVERIFY(engine.evaluate("!Object.getOwnPropertyDescriptor(p, 'undefined1').writable").toBool());
    QVERIFY(engine.evaluate("!Object.getOwnPropertyDescriptor(p, 'undefined1').writable").toBool());
    QEXPECT_FAIL("int + readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(readOnly == engine.evaluate("!Object.getOwnPropertyDescriptor(p, 'overloaded3').writable").toBool());
    QEXPECT_FAIL("int + readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(readOnly == engine.evaluate("!Object.getOwnPropertyDescriptor(p, 'overloaded4').writable").toBool());
    QEXPECT_FAIL("int + readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(readOnly == engine.evaluate("!Object.getOwnPropertyDescriptor(o, 'overloaded1').writable").toBool());
    QEXPECT_FAIL("int + readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(readOnly == engine.evaluate("!Object.getOwnPropertyDescriptor(o, 'overloaded2').writable").toBool());
    QVERIFY(!engine.evaluate("!Object.getOwnPropertyDescriptor(o, 'overloaded3').writable").toBool());
    QVERIFY(!engine.evaluate("!Object.getOwnPropertyDescriptor(o, 'overloaded4').writable").toBool());

    QVERIFY(undeletable == engine.evaluate("!Object.getOwnPropertyDescriptor(o, '4').configurable").toBool());
    QEXPECT_FAIL("int + undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(undeletable == engine.evaluate("!Object.getOwnPropertyDescriptor(o, '5').configurable").toBool());
    QEXPECT_FAIL("int + undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(undeletable == engine.evaluate("!Object.getOwnPropertyDescriptor(o, '6').configurable").toBool());
    QEXPECT_FAIL("int + undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(undeletable == engine.evaluate("!Object.getOwnPropertyDescriptor(p, '7').configurable").toBool());
    QVERIFY(undeletable == engine.evaluate("!Object.getOwnPropertyDescriptor(p, '8').configurable").toBool());
    QVERIFY(undeletable == engine.evaluate("!Object.getOwnPropertyDescriptor(o, 'undefined1').configurable").toBool());
    QVERIFY(undeletable == engine.evaluate("!Object.getOwnPropertyDescriptor(o, 'undefined2').configurable").toBool());
    QVERIFY(undeletable == engine.evaluate("!Object.getOwnPropertyDescriptor(p, 'undefined3').configurable").toBool());
    QVERIFY(undeletable == engine.evaluate("!Object.getOwnPropertyDescriptor(p, 'undefined4').configurable").toBool());
    QEXPECT_FAIL("int + undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(undeletable == engine.evaluate("!Object.getOwnPropertyDescriptor(o, 'defined1').configurable").toBool());
    QEXPECT_FAIL("int + undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(undeletable == engine.evaluate("!Object.getOwnPropertyDescriptor(o, 'defined2').configurable").toBool());
    QEXPECT_FAIL("int + undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(undeletable == engine.evaluate("!Object.getOwnPropertyDescriptor(o, 'overloaded1').configurable").toBool());
    QEXPECT_FAIL("int + undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(undeletable == engine.evaluate("!Object.getOwnPropertyDescriptor(o, 'overloaded2').configurable").toBool());
    QVERIFY(engine.evaluate("Object.getOwnPropertyDescriptor(p, 'overloaded1').configurable").toBool());
    QVERIFY(engine.evaluate("Object.getOwnPropertyDescriptor(p, 'overloaded2').configurable").toBool());
    QVERIFY(engine.evaluate("Object.getOwnPropertyDescriptor(o, 'overloaded3').configurable").toBool());
    QVERIFY(engine.evaluate("Object.getOwnPropertyDescriptor(o, 'overloaded4').configurable").toBool());
    QEXPECT_FAIL("int + undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(undeletable == engine.evaluate("!Object.getOwnPropertyDescriptor(p, 'overloaded3').configurable").toBool());
    QEXPECT_FAIL("int + undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(undeletable == engine.evaluate("!Object.getOwnPropertyDescriptor(p, 'overloaded4').configurable").toBool());

    QVERIFY(skipInEnumeration != engine.evaluate("Object.getOwnPropertyDescriptor(o, '4').enumerable").toBool());
    QEXPECT_FAIL("int + skipInEnumeration", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(skipInEnumeration != engine.evaluate("Object.getOwnPropertyDescriptor(o, '5').enumerable").toBool());
    QEXPECT_FAIL("int + skipInEnumeration", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(skipInEnumeration != engine.evaluate("Object.getOwnPropertyDescriptor(o, '6').enumerable").toBool());
    QEXPECT_FAIL("int + skipInEnumeration", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(skipInEnumeration != engine.evaluate("Object.getOwnPropertyDescriptor(p, '7').enumerable").toBool());
    QVERIFY(skipInEnumeration != engine.evaluate("Object.getOwnPropertyDescriptor(p, '8').enumerable").toBool());
    QVERIFY(skipInEnumeration != engine.evaluate("Object.getOwnPropertyDescriptor(o, 'undefined1').enumerable").toBool());
    QVERIFY(skipInEnumeration != engine.evaluate("Object.getOwnPropertyDescriptor(o, 'undefined2').enumerable").toBool());
    QVERIFY(skipInEnumeration != engine.evaluate("Object.getOwnPropertyDescriptor(p, 'undefined3').enumerable").toBool());
    QVERIFY(skipInEnumeration != engine.evaluate("Object.getOwnPropertyDescriptor(p, 'undefined4').enumerable").toBool());
    QEXPECT_FAIL("int + skipInEnumeration", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(skipInEnumeration != engine.evaluate("Object.getOwnPropertyDescriptor(o, 'overloaded1').enumerable").toBool());
    QEXPECT_FAIL("int + skipInEnumeration", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(skipInEnumeration != engine.evaluate("Object.getOwnPropertyDescriptor(o, 'overloaded2').enumerable").toBool());
    QVERIFY(engine.evaluate("p.propertyIsEnumerable('overloaded1')").toBool());
    QVERIFY(engine.evaluate("p.propertyIsEnumerable('overloaded2')").toBool());
    QVERIFY(engine.evaluate("o.propertyIsEnumerable('overloaded3')").toBool());
    QVERIFY(engine.evaluate("o.propertyIsEnumerable('overloaded4')").toBool());
    QEXPECT_FAIL("int + skipInEnumeration", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(skipInEnumeration != engine.evaluate("p.propertyIsEnumerable('overloaded3')").toBool());
    QEXPECT_FAIL("int + skipInEnumeration", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(skipInEnumeration != engine.evaluate("p.propertyIsEnumerable('overloaded4')").toBool());
    QEXPECT_FAIL("int + skipInEnumeration", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(skipInEnumeration != engine.evaluate("o.propertyIsEnumerable('defined1')").toBool());
    QEXPECT_FAIL("int + skipInEnumeration", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QEXPECT_FAIL("int + skipInEnumeration|readOnly|undeletable", "WebKit bug: 40613 (The JSObjectSetProperty doesn't overwrite property flags)", Continue);
    QVERIFY(skipInEnumeration != engine.evaluate("o.propertyIsEnumerable('defined2')").toBool());
}

void tst_QScriptValue::propertyFlag_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("flag");

    QTest::newRow("?Cr@jzi!%$") << "?Cr@jzi!%$" << static_cast<int>(0);
    QTest::newRow("ReadOnly") << "ReadOnly" << static_cast<int>(QScriptValue::ReadOnly);
    QTest::newRow("Undeletable") << "Undeletable" << static_cast<int>(QScriptValue::Undeletable);
    QTest::newRow("SkipInEnumeration") << "SkipInEnumeration" << static_cast<int>(QScriptValue::SkipInEnumeration);
    QTest::newRow("ReadOnly | Undeletable") << "ReadOnly_Undeletable" << static_cast<int>(QScriptValue::ReadOnly | QScriptValue::Undeletable);
    QTest::newRow("ReadOnly | SkipInEnumeration") << "ReadOnly_SkipInEnumeration" << static_cast<int>(QScriptValue::ReadOnly | QScriptValue::SkipInEnumeration);
    QTest::newRow("Undeletable | SkipInEnumeration") << "Undeletable_SkipInEnumeration" << static_cast<int>(QScriptValue::Undeletable | QScriptValue::SkipInEnumeration);
    QTest::newRow("ReadOnly | Undeletable | SkipInEnumeration") << "ReadOnly_Undeletable_SkipInEnumeration" << static_cast<int>(QScriptValue::ReadOnly | QScriptValue::Undeletable | QScriptValue::SkipInEnumeration);
}

void tst_QScriptValue::propertyFlag()
{
    QScriptEngine engine;
    QFETCH(QString, name);
    QFETCH(int, flag);
    const QScriptString nameHandle = engine.toStringHandle(name);
    const QString protoName = "proto" + name;
    const QScriptString protoNameHandle = engine.toStringHandle(protoName);

    QScriptValue proto = engine.newObject();
    QScriptValue object = engine.newObject();
    object.setPrototype(proto);

    proto.setProperty(protoName, QScriptValue(124816), QScriptValue::PropertyFlag(flag));
    object.setProperty(name, QScriptValue(124816), QScriptValue::PropertyFlag(flag));

    // Check using QString name
    QCOMPARE(object.propertyFlags(name), QScriptValue::PropertyFlag(flag));
    QCOMPARE(object.propertyFlags(protoName, QScriptValue::ResolvePrototype), QScriptValue::PropertyFlag(flag));
    QVERIFY(!object.propertyFlags(protoName, QScriptValue::ResolveLocal));

    // Check using QScriptString name
    QCOMPARE(object.propertyFlags(nameHandle), QScriptValue::PropertyFlag(flag));
    QCOMPARE(object.propertyFlags(protoNameHandle, QScriptValue::ResolvePrototype), QScriptValue::PropertyFlag(flag));
    QVERIFY(!object.propertyFlags(protoNameHandle, QScriptValue::ResolveLocal));
}

void tst_QScriptValue::globalObjectChanges()
{
    // API functionality shouldn't depend on Global Object.
    QScriptEngine engine;
    QScriptValue array = engine.newArray();
    QScriptValue error = engine.evaluate("new Error");
    QScriptValue object = engine.newObject();

    object.setProperty("foo", 512);

    // Remove properties form global object.
    engine.evaluate("delete Object; delete Error; delete Array;");

    QVERIFY(array.isArray());
    QVERIFY(error.isError());
    QVERIFY(object.isObject());

    QVERIFY(object.property("foo").isValid());
    QVERIFY(object.property("foo", QScriptValue::ResolveLocal).isValid());
    object.setProperty("foo", QScriptValue());
    QVERIFY(!object.property("foo").isValid());
    QVERIFY(!object.property("foo", QScriptValue::ResolveLocal).isValid());
}

void tst_QScriptValue::assignAndCopyConstruct_data()
{
    QTest::addColumn<QScriptValue>("value");
    if (m_engine)
        delete m_engine;
    m_engine = new QScriptEngine;
    // Copy & assign code is the same for all types, so it is enough to check only a few value.
    for (unsigned i = 0; i < 10; ++i) {
        QPair<QString, QScriptValue> testcase = initScriptValues(i);
        QTest::newRow(testcase.first.toAscii().constData()) << testcase.second;
    }
}

void tst_QScriptValue::assignAndCopyConstruct()
{
    QFETCH(QScriptValue, value);
    QScriptValue copy(value);
    QEXPECT_FAIL("QScriptValue(QScriptValue::NullValue)", "FIXME: WebKit bug 43038", Abort);
    QEXPECT_FAIL("QScriptValue(QScriptValue::UndefinedValue)", "FIXME: WebKit bug 43038", Abort);
    QCOMPARE(copy.strictlyEquals(value), !value.isNumber() || !qIsNaN(value.toNumber()));
    QCOMPARE(copy.engine(), value.engine());

    QScriptValue assigned = copy;
    QCOMPARE(assigned.strictlyEquals(value), !copy.isNumber() || !qIsNaN(copy.toNumber()));
    QCOMPARE(assigned.engine(), assigned.engine());

    QScriptValue other(!value.toBool());
    assigned = other;
    QVERIFY(!assigned.strictlyEquals(copy));
    QVERIFY(assigned.strictlyEquals(other));
    QCOMPARE(assigned.engine(), other.engine());
}

QTEST_MAIN(tst_QScriptValue)
