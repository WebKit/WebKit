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

#include "config.h"

#include "qscriptengine.h"

#include "qscriptengine_p.h"
#include "qscriptprogram_p.h"
#include "qscriptsyntaxcheckresult_p.h"
#include "qscriptvalue_p.h"
#include <QtCore/qdatetime.h>
#include <QtCore/qnumeric.h>

/*!
    Constructs a QScriptEngine object.

    The globalObject() is initialized to have properties as described in ECMA-262, Section 15.1.
*/
QScriptEngine::QScriptEngine()
    : d_ptr(new QScriptEnginePrivate(this))
{
}

/*!
    Destroys this QScriptEngine.
*/
QScriptEngine::~QScriptEngine()
{
}

/*!
  Checks the syntax of the given \a program. Returns a
  QScriptSyntaxCheckResult object that contains the result of the check.
*/
QScriptSyntaxCheckResult QScriptEngine::checkSyntax(const QString &program)
{
    // FIXME This is not optimal.
    // The JSC C API needs a context to perform a syntax check, it means that a QScriptEnginePrivate
    // had to be created. This function is static so we have to create QScriptEnginePrivate for each
    // call. We can't remove the "static" for compatibility reason, at least up to Qt5.
    // QScriptSyntaxCheckResultPrivate takes ownership of newly created engine. The engine will be
    // kept as long as it is needed for lazy evaluation of properties of
    // the QScriptSyntaxCheckResultPrivate.
    QScriptEnginePrivate* engine = new QScriptEnginePrivate(/* q_ptr */ 0);
    return QScriptSyntaxCheckResultPrivate::get(engine->checkSyntax(program));
}

/*!
    Evaluates \a program, using \a lineNumber as the base line number,
    and returns the result of the evaluation.

    The script code will be evaluated in the current context.

    The evaluation of \a program can cause an exception in the
    engine; in this case the return value will be the exception
    that was thrown (typically an \c{Error} object). You can call
    hasUncaughtException() to determine if an exception occurred in
    the last call to evaluate().

    \a lineNumber is used to specify a starting line number for \a
    program; line number information reported by the engine that pertain
    to this evaluation (e.g. uncaughtExceptionLineNumber()) will be
    based on this argument. For example, if \a program consists of two
    lines of code, and the statement on the second line causes a script
    exception, uncaughtExceptionLineNumber() would return the given \a
    lineNumber plus one. When no starting line number is specified, line
    numbers will be 1-based.

    \a fileName is used for error reporting. For example in error objects
    the file name is accessible through the "fileName" property if it's
    provided with this function.
*/
QScriptValue QScriptEngine::evaluate(const QString& program, const QString& fileName, int lineNumber)
{
    return QScriptValuePrivate::get(d_ptr->evaluate(program, fileName, lineNumber));
}

QScriptValue QScriptEngine::evaluate(const QScriptProgram& program)
{
    return QScriptValuePrivate::get(d_ptr->evaluate(QScriptProgramPrivate::get(program)));
}

/*!
    Returns true if the last script evaluation resulted in an uncaught
    exception; otherwise returns false.

    The exception state is cleared when evaluate() is called.

    \sa uncaughtException(), uncaughtExceptionLineNumber(),
      uncaughtExceptionBacktrace()
*/
bool QScriptEngine::hasUncaughtException() const
{
    return d_ptr->hasUncaughtException();
}

/*!
    Returns the current uncaught exception, or an invalid QScriptValue
    if there is no uncaught exception.

    The exception value is typically an \c{Error} object; in that case,
    you can call toString() on the return value to obtain an error
    message.

    \sa hasUncaughtException(), uncaughtExceptionLineNumber(),
      uncaughtExceptionBacktrace()
*/
QScriptValue QScriptEngine::uncaughtException() const
{
    return QScriptValuePrivate::get(d_ptr->uncaughtException());
}

/*!
    Clears any uncaught exceptions in this engine.

    \sa hasUncaughtException()
*/
void QScriptEngine::clearExceptions()
{
    d_ptr->clearExceptions();
}

/*!
    Returns the line number where the last uncaught exception occurred.

    Line numbers are 1-based, unless a different base was specified as
    the second argument to evaluate().

    \sa hasUncaughtException(), uncaughtExceptionBacktrace()
*/
int QScriptEngine::uncaughtExceptionLineNumber() const
{
    return d_ptr->uncaughtExceptionLineNumber();
}

/*!
    Returns a human-readable backtrace of the last uncaught exception.

    Each line is of the form \c{<function-name>(<arguments>)@<file-name>:<line-number>}.

    \sa uncaughtException()
*/
QStringList QScriptEngine::uncaughtExceptionBacktrace() const
{
    return d_ptr->uncaughtExceptionBacktrace();
}

/*!
    Runs the garbage collector.

    The garbage collector will attempt to reclaim memory by locating and disposing of objects that are
    no longer reachable in the script environment.

    Normally you don't need to call this function; the garbage collector will automatically be invoked
    when the QScriptEngine decides that it's wise to do so (i.e. when a certain number of new objects
    have been created). However, you can call this function to explicitly request that garbage
    collection should be performed as soon as possible.

    \sa reportAdditionalMemoryCost()
*/
void QScriptEngine::collectGarbage()
{
    d_ptr->collectGarbage();
}

/*!
  Reports an additional memory cost of the given \a size, measured in
  bytes, to the garbage collector.

  This function can be called to indicate that a JavaScript object has
  memory associated with it that isn't managed by Qt Script itself.
  Reporting the additional cost makes it more likely that the garbage
  collector will be triggered.

  Note that if the additional memory is shared with objects outside
  the scripting environment, the cost should not be reported, since
  collecting the JavaScript object would not cause the memory to be
  freed anyway.

  Negative \a size values are ignored, i.e. this function can't be
  used to report that the additional memory has been deallocated.

  \sa collectGarbage()
*/
void QScriptEngine::reportAdditionalMemoryCost(int cost)
{
    d_ptr->reportAdditionalMemoryCost(cost);
}

/*!
  Returns a handle that represents the given string, \a str.

  QScriptString can be used to quickly look up properties, and
  compare property names, of script objects.

  \sa QScriptValue::property()
*/
QScriptString QScriptEngine::toStringHandle(const QString& str)
{
    return QScriptStringPrivate::get(d_ptr->toStringHandle(str));
}

/*!
  Converts the given \a value to an object, if such a conversion is
  possible; otherwise returns an invalid QScriptValue. The conversion
  is performed according to the following table:

    \table
    \header \o Input Type \o Result
    \row    \o Undefined  \o An invalid QScriptValue.
    \row    \o Null       \o An invalid QScriptValue.
    \row    \o Boolean    \o A new Boolean object whose internal value is set to the value of the boolean.
    \row    \o Number     \o A new Number object whose internal value is set to the value of the number.
    \row    \o String     \o A new String object whose internal value is set to the value of the string.
    \row    \o Object     \o The result is the object itself (no conversion).
    \endtable

    \sa newObject()
*/
QScriptValue QScriptEngine::toObject(const QScriptValue& value)
{
    return QScriptValuePrivate::get(QScriptValuePrivate::get(value)->toObject(d_ptr.data()));
}

/*!
  Returns a QScriptValue of the primitive type Null.

  \sa undefinedValue()
*/
QScriptValue QScriptEngine::nullValue()
{
    return QScriptValue(this, QScriptValue::NullValue);
}

/*!
  Returns a QScriptValue of the primitive type Undefined.

  \sa nullValue()
*/
QScriptValue QScriptEngine::undefinedValue()
{
    return QScriptValue(this, QScriptValue::UndefinedValue);
}

/*!
    Creates a QScriptValue that wraps a native (C++) function. \a fun
    must be a C++ function with signature QScriptEngine::FunctionSignature.
    \a length is the number of arguments that \a fun expects; this becomes
    the \c{length} property of the created QScriptValue.

    Note that \a length only gives an indication of the number of
    arguments that the function expects; an actual invocation of a
    function can include any number of arguments. You can check the
    \l{QScriptContext::argumentCount()}{argumentCount()} of the
    QScriptContext associated with the invocation to determine the
    actual number of arguments passed.

    A \c{prototype} property is automatically created for the resulting
    function object, to provide for the possibility that the function
    will be used as a constructor.

    By combining newFunction() and the property flags
    QScriptValue::PropertyGetter and QScriptValue::PropertySetter, you
    can create script object properties that behave like normal
    properties in script code, but are in fact accessed through
    functions (analogous to how properties work in \l{Qt's Property
    System}). Example:

    \snippet doc/src/snippets/code/src_script_qscriptengine.cpp 11

    When the property \c{foo} of the script object is subsequently
    accessed in script code, \c{getSetFoo()} will be invoked to handle
    the access.  In this particular case, we chose to store the "real"
    value of \c{foo} as a property of the accessor function itself; you
    are of course free to do whatever you like in this function.

    In the above example, a single native function was used to handle
    both reads and writes to the property; the argument count is used to
    determine if we are handling a read or write. You can also use two
    separate functions; just specify the relevant flag
    (QScriptValue::PropertyGetter or QScriptValue::PropertySetter) when
    setting the property, e.g.:

    \snippet doc/src/snippets/code/src_script_qscriptengine.cpp 12

    \sa QScriptValue::call()
*/
QScriptValue QScriptEngine::newFunction(QScriptEngine::FunctionSignature fun, int length)
{
    return QScriptValuePrivate::get(d_ptr->newFunction(fun, 0, length));
}

/*!
    Creates a constructor function from \a fun, with the given \a length.
    The \c{prototype} property of the resulting function is set to be the
    given \a prototype. The \c{constructor} property of \a prototype is
    set to be the resulting function.

    When a function is called as a constructor (e.g. \c{new Foo()}), the
    `this' object associated with the function call is the new object
    that the function is expected to initialize; the prototype of this
    default constructed object will be the function's public
    \c{prototype} property. If you always want the function to behave as
    a constructor (e.g. \c{Foo()} should also create a new object), or
    if you need to create your own object rather than using the default
    `this' object, you should make sure that the prototype of your
    object is set correctly; either by setting it manually, or, when
    wrapping a custom type, by having registered the defaultPrototype()
    of that type. Example:

    \snippet doc/src/snippets/code/src_script_qscriptengine.cpp 9

    To wrap a custom type and provide a constructor for it, you'd typically
    do something like this:

    \snippet doc/src/snippets/code/src_script_qscriptengine.cpp 10
*/
QScriptValue QScriptEngine::newFunction(QScriptEngine::FunctionSignature fun, const QScriptValue& prototype, int length)
{
    return QScriptValuePrivate::get(d_ptr->newFunction(fun, QScriptValuePrivate::get(prototype), length));
}

/*!
    \internal
    \since 4.4
*/
QScriptValue QScriptEngine::newFunction(QScriptEngine::FunctionWithArgSignature fun, void* arg)
{
    return QScriptValuePrivate::get(d_ptr->newFunction(fun, arg));
}

/*!
  Creates a QtScript object of class Object.

  The prototype of the created object will be the Object
  prototype object.

  \sa newArray(), QScriptValue::setProperty()
*/
QScriptValue QScriptEngine::newObject()
{
    return QScriptValuePrivate::get(d_ptr->newObject());
}

/*!
  Creates a QtScript object of class Array with the given \a length.

  \sa newObject()
*/
QScriptValue QScriptEngine::newArray(uint length)
{
    return QScriptValuePrivate::get(d_ptr->newArray(length));
}

/*!
    Creates a QtScript object of class Date with the given \a value
    (the number of milliseconds since 01 January 1970, UTC).
*/
QScriptValue QScriptEngine::newDate(qsreal value)
{
    return QScriptValuePrivate::get(d_ptr->newDate(value));
}

/*!
    Creates a QtScript object of class Date from the given \a value.

    \sa QScriptValue::toDateTime()
*/
QScriptValue QScriptEngine::newDate(const QDateTime& value)
{
    if (value.isValid())
        return QScriptValuePrivate::get(d_ptr->newDate(qsreal(value.toMSecsSinceEpoch())));
    return QScriptValuePrivate::get(d_ptr->newDate(qSNaN()));
}

/*!
  Returns this engine's Global Object.

  By default, the Global Object contains the built-in objects that are
  part of \l{ECMA-262}, such as Math, Date and String. Additionally,
  you can set properties of the Global Object to make your own
  extensions available to all script code. Non-local variables in
  script code will be created as properties of the Global Object, as
  well as local variables in global code.
*/
QScriptValue QScriptEngine::globalObject() const
{
    return QScriptValuePrivate::get(d_ptr->globalObject());
}

/*!
    \typedef QScriptEngine::FunctionSignature
    \relates QScriptEngine

    The function signature \c{QScriptValue f(QScriptContext *, QScriptEngine *)}.

    A function with such a signature can be passed to
    QScriptEngine::newFunction() to wrap the function.
*/

/*!
    \typedef QScriptEngine::FunctionWithArgSignature
    \relates QScriptEngine

    The function signature \c{QScriptValue f(QScriptContext *, QScriptEngine *, void *)}.

    A function with such a signature can be passed to
    QScriptEngine::newFunction() to wrap the function.
*/
