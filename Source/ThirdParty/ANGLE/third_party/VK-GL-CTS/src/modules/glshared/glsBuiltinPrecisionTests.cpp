/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Precision and range tests for GLSL builtins and types.
 *
 *//*--------------------------------------------------------------------*/

#include "glsBuiltinPrecisionTests.hpp"

#include "deMath.h"
#include "deMemory.h"
#include "deDefs.hpp"
#include "deRandom.hpp"
#include "deSTLUtil.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deArrayUtil.hpp"

#include "tcuCommandLine.hpp"
#include "tcuFloatFormat.hpp"
#include "tcuInterval.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "tcuMatrix.hpp"
#include "tcuResultCollector.hpp"

#include "gluContextInfo.hpp"
#include "gluVarType.hpp"
#include "gluRenderContext.hpp"
#include "glwDefs.hpp"

#include "glsShaderExecUtil.hpp"

#include <cmath>
#include <string>
#include <sstream>
#include <iostream>
#include <map>
#include <utility>
#include <limits>

// Uncomment this to get evaluation trace dumps to std::cerr
// #define GLS_ENABLE_TRACE

// set this to true to dump even passing results
#define GLS_LOG_ALL_RESULTS false

enum
{
    // Computing reference intervals can take a non-trivial amount of time, especially on
    // platforms where toggling floating-point rounding mode is slow (emulated arm on x86).
    // As a workaround watchdog is kept happy by touching it periodically during reference
    // interval computation.
    TOUCH_WATCHDOG_VALUE_FREQUENCY = 4096
};

namespace deqp
{
namespace gls
{
namespace BuiltinPrecisionTests
{

using std::map;
using std::ostream;
using std::ostringstream;
using std::pair;
using std::set;
using std::string;
using std::vector;

using de::MovePtr;
using de::Random;
using de::SharedPtr;
using de::UniquePtr;
using tcu::FloatFormat;
using tcu::Interval;
using tcu::Matrix;
using tcu::MessageBuilder;
using tcu::TestCase;
using tcu::TestLog;
using tcu::Vector;
namespace matrix = tcu::matrix;
using gls::ShaderExecUtil::Symbol;
using glu::ContextInfo;
using glu::DataType;
using glu::Precision;
using glu::RenderContext;
using glu::ShaderType;
using glu::VarType;

typedef TestCase::IterateResult IterateResult;

using namespace glw;
using namespace tcu;

/*--------------------------------------------------------------------*//*!
 * \brief Generic singleton creator.
 *
 * instance<T>() returns a reference to a unique default-constructed instance
 * of T. This is mainly used for our GLSL function implementations: each
 * function is implemented by an object, and each of the objects has a
 * distinct class. It would be extremely toilsome to maintain a separate
 * context object that contained individual instances of the function classes,
 * so we have to resort to global singleton instances.
 *
 *//*--------------------------------------------------------------------*/
template <typename T>
const T &instance(void)
{
    static const T s_instance = T();
    return s_instance;
}

/*--------------------------------------------------------------------*//*!
 * \brief A placeholder type for unused template parameters.
 *
 * In the precision tests we are dealing with functions of different arities.
 * To minimize code duplication, we only define templates with the maximum
 * number of arguments, currently four. If a function's arity is less than the
 * maximum, Void us used as the type for unused arguments.
 *
 * Although Voids are not used at run-time, they still must be compilable, so
 * they must support all operations that other types do.
 *
 *//*--------------------------------------------------------------------*/
struct Void
{
    typedef Void Element;
    enum
    {
        SIZE = 0,
    };

    template <typename T>
    explicit Void(const T &)
    {
    }
    Void(void)
    {
    }
    operator double(void) const
    {
        return TCU_NAN;
    }

    // These are used to make Voids usable as containers in container-generic code.
    Void &operator[](int)
    {
        return *this;
    }
    const Void &operator[](int) const
    {
        return *this;
    }
};

ostream &operator<<(ostream &os, Void)
{
    return os << "()";
}

//! Returns true for all other types except Void
template <typename T>
bool isTypeValid(void)
{
    return true;
}
template <>
bool isTypeValid<Void>(void)
{
    return false;
}

//! Utility function for getting the name of a data type.
//! This is used in vector and matrix constructors.
template <typename T>
const char *dataTypeNameOf(void)
{
    return glu::getDataTypeName(glu::dataTypeOf<T>());
}

template <>
const char *dataTypeNameOf<Void>(void)
{
    DE_FATAL("Impossible");
    return nullptr;
}

//! A hack to get Void support for VarType.
template <typename T>
VarType getVarTypeOf(Precision prec = glu::PRECISION_LAST)
{
    return glu::varTypeOf<T>(prec);
}

template <>
VarType getVarTypeOf<Void>(Precision)
{
    DE_FATAL("Impossible");
    return VarType();
}

/*--------------------------------------------------------------------*//*!
 * \brief Type traits for generalized interval types.
 *
 * We are trying to compute sets of acceptable values not only for
 * float-valued expressions but also for compound values: vectors and
 * matrices. We approximate a set of vectors as a vector of intervals and
 * likewise for matrices.
 *
 * We now need generalized operations for each type and its interval
 * approximation. These are given in the type Traits<T>.
 *
 * The type Traits<T>::IVal is the approximation of T: it is `Interval` for
 * scalar types, and a vector or matrix of intervals for container types.
 *
 * To allow template inference to take place, there are function wrappers for
 * the actual operations in Traits<T>. Hence we can just use:
 *
 * makeIVal(someFloat)
 *
 * instead of:
 *
 * Traits<float>::doMakeIVal(value)
 *
 *//*--------------------------------------------------------------------*/

template <typename T>
struct Traits;

//! Create container from elementwise singleton values.
template <typename T>
typename Traits<T>::IVal makeIVal(const T &value)
{
    return Traits<T>::doMakeIVal(value);
}

//! Elementwise union of intervals.
template <typename T>
typename Traits<T>::IVal unionIVal(const typename Traits<T>::IVal &a, const typename Traits<T>::IVal &b)
{
    return Traits<T>::doUnion(a, b);
}

//! Returns true iff every element of `ival` contains the corresponding element of `value`.
template <typename T>
bool contains(const typename Traits<T>::IVal &ival, const T &value)
{
    return Traits<T>::doContains(ival, value);
}

//! Returns true iff every element of `ival` contains corresponding element of `value` within the warning interval
template <typename T>
bool containsWarning(const typename Traits<T>::IVal &ival, const T &value)
{
    return Traits<T>::doContainsWarning(ival, value);
}

//! Print out an interval with the precision of `fmt`.
template <typename T>
void printIVal(const FloatFormat &fmt, const typename Traits<T>::IVal &ival, ostream &os)
{
    Traits<T>::doPrintIVal(fmt, ival, os);
}

template <typename T>
string intervalToString(const FloatFormat &fmt, const typename Traits<T>::IVal &ival)
{
    ostringstream oss;
    printIVal<T>(fmt, ival, oss);
    return oss.str();
}

//! Print out a value with the precision of `fmt`.
template <typename T>
void printValue(const FloatFormat &fmt, const T &value, ostream &os)
{
    Traits<T>::doPrintValue(fmt, value, os);
}

template <typename T>
string valueToString(const FloatFormat &fmt, const T &val)
{
    ostringstream oss;
    printValue(fmt, val, oss);
    return oss.str();
}

//! Approximate `value` elementwise to the float precision defined in `fmt`.
//! The resulting interval might not be a singleton if rounding in both
//! directions is allowed.
template <typename T>
typename Traits<T>::IVal round(const FloatFormat &fmt, const T &value)
{
    return Traits<T>::doRound(fmt, value);
}

template <typename T>
typename Traits<T>::IVal convert(const FloatFormat &fmt, const typename Traits<T>::IVal &value)
{
    return Traits<T>::doConvert(fmt, value);
}

//! Common traits for scalar types.
template <typename T>
struct ScalarTraits
{
    typedef Interval IVal;

    static Interval doMakeIVal(const T &value)
    {
        // Thankfully all scalar types have a well-defined conversion to `double`,
        // hence Interval can represent their ranges without problems.
        return Interval(double(value));
    }

    static Interval doUnion(const Interval &a, const Interval &b)
    {
        return a | b;
    }

    static bool doContains(const Interval &a, T value)
    {
        return a.contains(double(value));
    }

    static bool doContainsWarning(const Interval &a, T value)
    {
        return a.containsWarning(double(value));
    }

    static Interval doConvert(const FloatFormat &fmt, const IVal &ival)
    {
        return fmt.convert(ival);
    }

    static Interval doRound(const FloatFormat &fmt, T value)
    {
        return fmt.roundOut(double(value), false);
    }
};

template <>
struct Traits<float> : ScalarTraits<float>
{
    static void doPrintIVal(const FloatFormat &fmt, const Interval &ival, ostream &os)
    {
        os << fmt.intervalToHex(ival);
    }

    static void doPrintValue(const FloatFormat &fmt, const float &value, ostream &os)
    {
        os << fmt.floatToHex(value);
    }
};

template <>
struct Traits<bool> : ScalarTraits<bool>
{
    static void doPrintValue(const FloatFormat &, const float &value, ostream &os)
    {
        os << (value != 0.0f ? "true" : "false");
    }

    static void doPrintIVal(const FloatFormat &, const Interval &ival, ostream &os)
    {
        os << "{";
        if (ival.contains(false))
            os << "false";
        if (ival.contains(false) && ival.contains(true))
            os << ", ";
        if (ival.contains(true))
            os << "true";
        os << "}";
    }
};

template <>
struct Traits<int> : ScalarTraits<int>
{
    static void doPrintValue(const FloatFormat &, const int &value, ostream &os)
    {
        os << value;
    }

    static void doPrintIVal(const FloatFormat &, const Interval &ival, ostream &os)
    {
        os << "[" << int(ival.lo()) << ", " << int(ival.hi()) << "]";
    }
};

//! Common traits for containers, i.e. vectors and matrices.
//! T is the container type itself, I is the same type with interval elements.
template <typename T, typename I>
struct ContainerTraits
{
    typedef typename T::Element Element;
    typedef I IVal;

    static IVal doMakeIVal(const T &value)
    {
        IVal ret;

        for (int ndx = 0; ndx < T::SIZE; ++ndx)
            ret[ndx] = makeIVal(value[ndx]);

        return ret;
    }

    static IVal doUnion(const IVal &a, const IVal &b)
    {
        IVal ret;

        for (int ndx = 0; ndx < T::SIZE; ++ndx)
            ret[ndx] = unionIVal<Element>(a[ndx], b[ndx]);

        return ret;
    }

    static bool doContains(const IVal &ival, const T &value)
    {
        for (int ndx = 0; ndx < T::SIZE; ++ndx)
            if (!contains(ival[ndx], value[ndx]))
                return false;

        return true;
    }

    static bool doContainsWarning(const IVal &ival, const T &value)
    {
        for (int ndx = 0; ndx < T::SIZE; ++ndx)
            if (!containsWarning(ival[ndx], value[ndx]))
                return false;

        return true;
    }

    static void doPrintIVal(const FloatFormat &fmt, const IVal ival, ostream &os)
    {
        os << "(";

        for (int ndx = 0; ndx < T::SIZE; ++ndx)
        {
            if (ndx > 0)
                os << ", ";

            printIVal<Element>(fmt, ival[ndx], os);
        }

        os << ")";
    }

    static void doPrintValue(const FloatFormat &fmt, const T &value, ostream &os)
    {
        os << dataTypeNameOf<T>() << "(";

        for (int ndx = 0; ndx < T::SIZE; ++ndx)
        {
            if (ndx > 0)
                os << ", ";

            printValue<Element>(fmt, value[ndx], os);
        }

        os << ")";
    }

    static IVal doConvert(const FloatFormat &fmt, const IVal &value)
    {
        IVal ret;

        for (int ndx = 0; ndx < T::SIZE; ++ndx)
            ret[ndx] = convert<Element>(fmt, value[ndx]);

        return ret;
    }

    static IVal doRound(const FloatFormat &fmt, T value)
    {
        IVal ret;

        for (int ndx = 0; ndx < T::SIZE; ++ndx)
            ret[ndx] = round(fmt, value[ndx]);

        return ret;
    }
};

template <typename T, int Size>
struct Traits<Vector<T, Size>> : ContainerTraits<Vector<T, Size>, Vector<typename Traits<T>::IVal, Size>>
{
};

template <typename T, int Rows, int Cols>
struct Traits<Matrix<T, Rows, Cols>>
    : ContainerTraits<Matrix<T, Rows, Cols>, Matrix<typename Traits<T>::IVal, Rows, Cols>>
{
};

//! Void traits. These are just dummies, but technically valid: a Void is a
//! unit type with a single possible value.
template <>
struct Traits<Void>
{
    typedef Void IVal;

    static Void doMakeIVal(const Void &value)
    {
        return value;
    }
    static Void doUnion(const Void &, const Void &)
    {
        return Void();
    }
    static bool doContains(const Void &, Void)
    {
        return true;
    }
    static bool doContainsWarning(const Void &, Void)
    {
        return true;
    }
    static Void doRound(const FloatFormat &, const Void &value)
    {
        return value;
    }
    static Void doConvert(const FloatFormat &, const Void &value)
    {
        return value;
    }

    static void doPrintValue(const FloatFormat &, const Void &, ostream &os)
    {
        os << "()";
    }

    static void doPrintIVal(const FloatFormat &, const Void &, ostream &os)
    {
        os << "()";
    }
};

//! This is needed for container-generic operations.
//! We want a scalar type T to be its own "one-element vector".
template <typename T, int Size>
struct ContainerOf
{
    typedef Vector<T, Size> Container;
};

template <typename T>
struct ContainerOf<T, 1>
{
    typedef T Container;
};
template <int Size>
struct ContainerOf<Void, Size>
{
    typedef Void Container;
};

// This is a kludge that is only needed to get the ExprP::operator[] syntactic sugar to work.
template <typename T>
struct ElementOf
{
    typedef typename T::Element Element;
};
template <>
struct ElementOf<float>
{
    typedef void Element;
};
template <>
struct ElementOf<bool>
{
    typedef void Element;
};
template <>
struct ElementOf<int>
{
    typedef void Element;
};

/*--------------------------------------------------------------------*//*!
 *
 * \name Abstract syntax for expressions and statements.
 *
 * We represent GLSL programs as syntax objects: an Expr<T> represents an
 * expression whose GLSL type corresponds to the C++ type T, and a Statement
 * represents a statement.
 *
 * To ease memory management, we use shared pointers to refer to expressions
 * and statements. ExprP<T> is a shared pointer to an Expr<T>, and StatementP
 * is a shared pointer to a Statement.
 *
 * \{
 *
 *//*--------------------------------------------------------------------*/

class ExprBase;
class ExpandContext;
class Statement;
class StatementP;
class FuncBase;
template <typename T>
class ExprP;
template <typename T>
class Variable;
template <typename T>
class VariableP;
template <typename T>
class DefaultSampling;

typedef set<const FuncBase *> FuncSet;

template <typename T>
VariableP<T> variable(const string &name);
StatementP compoundStatement(const vector<StatementP> &statements);

/*--------------------------------------------------------------------*//*!
 * \brief A variable environment.
 *
 * An Environment object maintains the mapping between variables of the
 * abstract syntax tree and their values.
 *
 * \todo [2014-03-28 lauri] At least run-time type safety.
 *
 *//*--------------------------------------------------------------------*/
class Environment
{
public:
    template <typename T>
    void bind(const Variable<T> &variable, const typename Traits<T>::IVal &value)
    {
        uint8_t *const data = new uint8_t[sizeof(value)];

        deMemcpy(data, &value, sizeof(value));
        de::insert(m_map, variable.getName(), SharedPtr<uint8_t>(data, de::ArrayDeleter<uint8_t>()));
    }

    template <typename T>
    typename Traits<T>::IVal &lookup(const Variable<T> &variable) const
    {
        uint8_t *const data = de::lookup(m_map, variable.getName()).get();

        return *reinterpret_cast<typename Traits<T>::IVal *>(data);
    }

private:
    map<string, SharedPtr<uint8_t>> m_map;
};

/*--------------------------------------------------------------------*//*!
 * \brief Evaluation context.
 *
 * The evaluation context contains everything that separates one execution of
 * an expression from the next. Currently this means the desired floating
 * point precision and the current variable environment.
 *
 *//*--------------------------------------------------------------------*/
struct EvalContext
{
    EvalContext(const FloatFormat &format_, Precision floatPrecision_, Environment &env_, int callDepth_ = 0)
        : format(format_)
        , floatPrecision(floatPrecision_)
        , env(env_)
        , callDepth(callDepth_)
    {
    }

    FloatFormat format;
    Precision floatPrecision;
    Environment &env;
    int callDepth;
};

/*--------------------------------------------------------------------*//*!
 * \brief Simple incremental counter.
 *
 * This is used to make sure that different ExpandContexts will not produce
 * overlapping temporary names.
 *
 *//*--------------------------------------------------------------------*/
class Counter
{
public:
    Counter(int count = 0) : m_count(count)
    {
    }
    int operator()(void)
    {
        return m_count++;
    }

private:
    int m_count;
};

/*--------------------------------------------------------------------*//*!
 * \brief A statement or declaration.
 *
 * Statements have no values. Instead, they are executed for their side
 * effects only: the execute() method should modify at least one variable in
 * the environment.
 *
 * As a bit of a kludge, a Statement object can also represent a declaration:
 * when it is evaluated, it can add a variable binding to the environment
 * instead of modifying a current one.
 *
 *//*--------------------------------------------------------------------*/
class Statement
{
public:
    virtual ~Statement(void)
    {
    }
    //! Execute the statement, modifying the environment of `ctx`
    void execute(EvalContext &ctx) const
    {
        this->doExecute(ctx);
    }
    void print(ostream &os) const
    {
        this->doPrint(os);
    }
    //! Add the functions used in this statement to `dst`.
    void getUsedFuncs(FuncSet &dst) const
    {
        this->doGetUsedFuncs(dst);
    }

protected:
    virtual void doPrint(ostream &os) const         = 0;
    virtual void doExecute(EvalContext &ctx) const  = 0;
    virtual void doGetUsedFuncs(FuncSet &dst) const = 0;
};

ostream &operator<<(ostream &os, const Statement &stmt)
{
    stmt.print(os);
    return os;
}

/*--------------------------------------------------------------------*//*!
 * \brief Smart pointer for statements (and declarations)
 *
 *//*--------------------------------------------------------------------*/
class StatementP : public SharedPtr<const Statement>
{
public:
    typedef SharedPtr<const Statement> Super;

    StatementP(void)
    {
    }
    explicit StatementP(const Statement *ptr) : Super(ptr)
    {
    }
    StatementP(const Super &ptr) : Super(ptr)
    {
    }
};

class ExpandContext
{
public:
    ExpandContext(Counter &symCounter) : m_symCounter(symCounter)
    {
    }
    ExpandContext(const ExpandContext &parent) : m_symCounter(parent.m_symCounter)
    {
    }

    template <typename T>
    VariableP<T> genSym(const string &baseName)
    {
        return variable<T>(baseName + de::toString(m_symCounter()));
    }

    void addStatement(const StatementP &stmt)
    {
        m_statements.push_back(stmt);
    }

    vector<StatementP> getStatements(void) const
    {
        return m_statements;
    }

private:
    Counter &m_symCounter;
    vector<StatementP> m_statements;
};

/*--------------------------------------------------------------------*//*!
 * \brief
 *
 * A statement that modifies a variable or a declaration that binds a variable.
 *
 *//*--------------------------------------------------------------------*/
template <typename T>
class VariableStatement : public Statement
{
public:
    VariableStatement(const VariableP<T> &variable, const ExprP<T> &value, bool isDeclaration)
        : m_variable(variable)
        , m_value(value)
        , m_isDeclaration(isDeclaration)
    {
    }

protected:
    void doPrint(ostream &os) const
    {
        if (m_isDeclaration)
            os << glu::declare(getVarTypeOf<T>(), m_variable->getName());
        else
            os << m_variable->getName();

        os << " = " << *m_value << ";\n";
    }

    void doExecute(EvalContext &ctx) const
    {
        if (m_isDeclaration)
            ctx.env.bind(*m_variable, m_value->evaluate(ctx));
        else
            ctx.env.lookup(*m_variable) = m_value->evaluate(ctx);
    }

    void doGetUsedFuncs(FuncSet &dst) const
    {
        m_value->getUsedFuncs(dst);
    }

    VariableP<T> m_variable;
    ExprP<T> m_value;
    bool m_isDeclaration;
};

template <typename T>
StatementP variableStatement(const VariableP<T> &variable, const ExprP<T> &value, bool isDeclaration)
{
    return StatementP(new VariableStatement<T>(variable, value, isDeclaration));
}

template <typename T>
StatementP variableDeclaration(const VariableP<T> &variable, const ExprP<T> &definiens)
{
    return variableStatement(variable, definiens, true);
}

template <typename T>
StatementP variableAssignment(const VariableP<T> &variable, const ExprP<T> &value)
{
    return variableStatement(variable, value, false);
}

/*--------------------------------------------------------------------*//*!
 * \brief A compound statement, i.e. a block.
 *
 * A compound statement is executed by executing its constituent statements in
 * sequence.
 *
 *//*--------------------------------------------------------------------*/
class CompoundStatement : public Statement
{
public:
    CompoundStatement(const vector<StatementP> &statements) : m_statements(statements)
    {
    }

protected:
    void doPrint(ostream &os) const
    {
        os << "{\n";

        for (size_t ndx = 0; ndx < m_statements.size(); ++ndx)
            os << *m_statements[ndx];

        os << "}\n";
    }

    void doExecute(EvalContext &ctx) const
    {
        for (size_t ndx = 0; ndx < m_statements.size(); ++ndx)
            m_statements[ndx]->execute(ctx);
    }

    void doGetUsedFuncs(FuncSet &dst) const
    {
        for (size_t ndx = 0; ndx < m_statements.size(); ++ndx)
            m_statements[ndx]->getUsedFuncs(dst);
    }

    vector<StatementP> m_statements;
};

StatementP compoundStatement(const vector<StatementP> &statements)
{
    return StatementP(new CompoundStatement(statements));
}

//! Common base class for all expressions regardless of their type.
class ExprBase
{
public:
    virtual ~ExprBase(void)
    {
    }
    void printExpr(ostream &os) const
    {
        this->doPrintExpr(os);
    }

    //! Output the functions that this expression refers to
    void getUsedFuncs(FuncSet &dst) const
    {
        this->doGetUsedFuncs(dst);
    }

protected:
    virtual void doPrintExpr(ostream &) const
    {
    }
    virtual void doGetUsedFuncs(FuncSet &) const
    {
    }
};

//! Type-specific operations for an expression representing type T.
template <typename T>
class Expr : public ExprBase
{
public:
    typedef T Val;
    typedef typename Traits<T>::IVal IVal;

    IVal evaluate(const EvalContext &ctx) const;

protected:
    virtual IVal doEvaluate(const EvalContext &ctx) const = 0;
};

//! Evaluate an expression with the given context, optionally tracing the calls to stderr.
template <typename T>
typename Traits<T>::IVal Expr<T>::evaluate(const EvalContext &ctx) const
{
#ifdef GLS_ENABLE_TRACE
    static const FloatFormat highpFmt(-126, 127, 23, true, tcu::MAYBE, tcu::YES, tcu::MAYBE);
    EvalContext newCtx(ctx.format, ctx.floatPrecision, ctx.env, ctx.callDepth + 1);
    const IVal ret = this->doEvaluate(newCtx);

    if (isTypeValid<T>())
    {
        std::cerr << string(ctx.callDepth, ' ');
        this->printExpr(std::cerr);
        std::cerr << " -> " << intervalToString<T>(highpFmt, ret) << std::endl;
    }
    return ret;
#else
    return this->doEvaluate(ctx);
#endif
}

template <typename T>
class ExprPBase : public SharedPtr<const Expr<T>>
{
public:
};

ostream &operator<<(ostream &os, const ExprBase &expr)
{
    expr.printExpr(os);
    return os;
}

/*--------------------------------------------------------------------*//*!
 * \brief Shared pointer to an expression of a container type.
 *
 * Container types (i.e. vectors and matrices) support the subscription
 * operator. This class provides a bit of syntactic sugar to allow us to use
 * the C++ subscription operator to create a subscription expression.
 *//*--------------------------------------------------------------------*/
template <typename T>
class ContainerExprPBase : public ExprPBase<T>
{
public:
    ExprP<typename T::Element> operator[](int i) const;
};

template <typename T>
class ExprP : public ExprPBase<T>
{
};

// We treat Voids as containers since the unused parameters in generalized
// vector functions are represented as Voids.
template <>
class ExprP<Void> : public ContainerExprPBase<Void>
{
};

template <typename T, int Size>
class ExprP<Vector<T, Size>> : public ContainerExprPBase<Vector<T, Size>>
{
};

template <typename T, int Rows, int Cols>
class ExprP<Matrix<T, Rows, Cols>> : public ContainerExprPBase<Matrix<T, Rows, Cols>>
{
};

template <typename T>
ExprP<T> exprP(void)
{
    return ExprP<T>();
}

template <typename T>
ExprP<T> exprP(const SharedPtr<const Expr<T>> &ptr)
{
    ExprP<T> ret;
    static_cast<SharedPtr<const Expr<T>> &>(ret) = ptr;
    return ret;
}

template <typename T>
ExprP<T> exprP(const Expr<T> *ptr)
{
    return exprP(SharedPtr<const Expr<T>>(ptr));
}

/*--------------------------------------------------------------------*//*!
 * \brief A shared pointer to a variable expression.
 *
 * This is just a narrowing of ExprP for the operations that require a variable
 * instead of an arbitrary expression.
 *
 *//*--------------------------------------------------------------------*/
template <typename T>
class VariableP : public SharedPtr<const Variable<T>>
{
public:
    typedef SharedPtr<const Variable<T>> Super;
    explicit VariableP(const Variable<T> *ptr) : Super(ptr)
    {
    }
    VariableP(void)
    {
    }
    VariableP(const Super &ptr) : Super(ptr)
    {
    }

    operator ExprP<T>(void) const
    {
        SharedPtr<const Expr<T>> ptr = *this;
        return exprP(ptr);
    }
};

/*--------------------------------------------------------------------*//*!
 * \name Syntactic sugar operators for expressions.
 *
 * @{
 *
 * These operators allow the use of C++ syntax to construct GLSL expressions
 * containing operators: e.g. "a+b" creates an addition expression with
 * operands a and b, and so on.
 *
 *//*--------------------------------------------------------------------*/
ExprP<float> operator-(const ExprP<float> &arg0);
ExprP<float> operator+(const ExprP<float> &arg0, const ExprP<float> &arg1);
ExprP<float> operator-(const ExprP<float> &arg0, const ExprP<float> &arg1);
ExprP<float> operator*(const ExprP<float> &arg0, const ExprP<float> &arg1);
ExprP<float> operator/(const ExprP<float> &arg0, const ExprP<float> &arg1);
template <int Size>
ExprP<Vector<float, Size>> operator-(const ExprP<Vector<float, Size>> &arg0);
template <int Size>
ExprP<Vector<float, Size>> operator*(const ExprP<Vector<float, Size>> &arg0, const ExprP<float> &arg1);
template <int Size>
ExprP<Vector<float, Size>> operator*(const ExprP<Vector<float, Size>> &arg0, const ExprP<Vector<float, Size>> &arg1);
template <int Size>
ExprP<Vector<float, Size>> operator-(const ExprP<Vector<float, Size>> &arg0, const ExprP<Vector<float, Size>> &arg1);
template <int Left, int Mid, int Right>
ExprP<Matrix<float, Left, Right>> operator*(const ExprP<Matrix<float, Left, Mid>> &left,
                                            const ExprP<Matrix<float, Mid, Right>> &right);
template <int Rows, int Cols>
ExprP<Vector<float, Rows>> operator*(const ExprP<Vector<float, Cols>> &left,
                                     const ExprP<Matrix<float, Rows, Cols>> &right);
template <int Rows, int Cols>
ExprP<Vector<float, Cols>> operator*(const ExprP<Matrix<float, Rows, Cols>> &left,
                                     const ExprP<Vector<float, Rows>> &right);
template <int Rows, int Cols>
ExprP<Matrix<float, Rows, Cols>> operator*(const ExprP<Matrix<float, Rows, Cols>> &left, const ExprP<float> &right);
template <int Rows, int Cols>
ExprP<Matrix<float, Rows, Cols>> operator+(const ExprP<Matrix<float, Rows, Cols>> &left,
                                           const ExprP<Matrix<float, Rows, Cols>> &right);
template <int Rows, int Cols>
ExprP<Matrix<float, Rows, Cols>> operator-(const ExprP<Matrix<float, Rows, Cols>> &mat);

//! @}

/*--------------------------------------------------------------------*//*!
 * \brief Variable expression.
 *
 * A variable is evaluated by looking up its range of possible values from an
 * environment.
 *//*--------------------------------------------------------------------*/
template <typename T>
class Variable : public Expr<T>
{
public:
    typedef typename Expr<T>::IVal IVal;

    Variable(const string &name) : m_name(name)
    {
    }
    string getName(void) const
    {
        return m_name;
    }

protected:
    void doPrintExpr(ostream &os) const
    {
        os << m_name;
    }
    IVal doEvaluate(const EvalContext &ctx) const
    {
        return ctx.env.lookup<T>(*this);
    }

private:
    string m_name;
};

template <typename T>
VariableP<T> variable(const string &name)
{
    return VariableP<T>(new Variable<T>(name));
}

template <typename T>
VariableP<T> bindExpression(const string &name, ExpandContext &ctx, const ExprP<T> &expr)
{
    VariableP<T> var = ctx.genSym<T>(name);
    ctx.addStatement(variableDeclaration(var, expr));
    return var;
}

/*--------------------------------------------------------------------*//*!
 * \brief Constant expression.
 *
 * A constant is evaluated by rounding it to a set of possible values allowed
 * by the current floating point precision.
 *//*--------------------------------------------------------------------*/
template <typename T>
class Constant : public Expr<T>
{
public:
    typedef typename Expr<T>::IVal IVal;

    Constant(const T &value) : m_value(value)
    {
    }

protected:
    void doPrintExpr(ostream &os) const
    {
        os << m_value;
    }
    IVal doEvaluate(const EvalContext &) const
    {
        return makeIVal(m_value);
    }

private:
    T m_value;
};

template <typename T>
ExprP<T> constant(const T &value)
{
    return exprP(new Constant<T>(value));
}

//! Return a reference to a singleton void constant.
const ExprP<Void> &voidP(void)
{
    static const ExprP<Void> singleton = constant(Void());

    return singleton;
}

/*--------------------------------------------------------------------*//*!
 * \brief Four-element tuple.
 *
 * This is used for various things where we need one thing for each possible
 * function parameter. Currently the maximum supported number of parameters is
 * four.
 *//*--------------------------------------------------------------------*/
template <typename T0 = Void, typename T1 = Void, typename T2 = Void, typename T3 = Void>
struct Tuple4
{
    explicit Tuple4(const T0 e0 = T0(), const T1 e1 = T1(), const T2 e2 = T2(), const T3 e3 = T3())
        : a(e0)
        , b(e1)
        , c(e2)
        , d(e3)
    {
    }

    T0 a;
    T1 b;
    T2 c;
    T3 d;
};

/*--------------------------------------------------------------------*//*!
 * \brief Function signature.
 *
 * This is a purely compile-time structure used to bundle all types in a
 * function signature together. This makes passing the signature around in
 * templates easier, since we only need to take and pass a single Sig instead
 * of a bunch of parameter types and a return type.
 *
 *//*--------------------------------------------------------------------*/
template <typename R, typename P0 = Void, typename P1 = Void, typename P2 = Void, typename P3 = Void>
struct Signature
{
    typedef R Ret;
    typedef P0 Arg0;
    typedef P1 Arg1;
    typedef P2 Arg2;
    typedef P3 Arg3;
    typedef typename Traits<Ret>::IVal IRet;
    typedef typename Traits<Arg0>::IVal IArg0;
    typedef typename Traits<Arg1>::IVal IArg1;
    typedef typename Traits<Arg2>::IVal IArg2;
    typedef typename Traits<Arg3>::IVal IArg3;

    typedef Tuple4<const Arg0 &, const Arg1 &, const Arg2 &, const Arg3 &> Args;
    typedef Tuple4<const IArg0 &, const IArg1 &, const IArg2 &, const IArg3 &> IArgs;
    typedef Tuple4<ExprP<Arg0>, ExprP<Arg1>, ExprP<Arg2>, ExprP<Arg3>> ArgExprs;
};

typedef vector<const ExprBase *> BaseArgExprs;

/*--------------------------------------------------------------------*//*!
 * \brief Type-independent operations for function objects.
 *
 *//*--------------------------------------------------------------------*/
class FuncBase
{
public:
    virtual ~FuncBase(void)
    {
    }
    virtual string getName(void) const = 0;
    //! Name of extension that this function requires, or empty.
    virtual string getRequiredExtension(const RenderContext &) const
    {
        return "";
    }
    virtual void print(ostream &, const BaseArgExprs &) const = 0;
    //! Index of output parameter, or -1 if none of the parameters is output.
    virtual int getOutParamIndex(void) const
    {
        return -1;
    }

    void printDefinition(ostream &os) const
    {
        doPrintDefinition(os);
    }

    void getUsedFuncs(FuncSet &dst) const
    {
        this->doGetUsedFuncs(dst);
    }

protected:
    virtual void doPrintDefinition(ostream &os) const = 0;
    virtual void doGetUsedFuncs(FuncSet &dst) const   = 0;
};

typedef Tuple4<string, string, string, string> ParamNames;

/*--------------------------------------------------------------------*//*!
 * \brief Function objects.
 *
 * Each Func object represents a GLSL function. It can be applied to interval
 * arguments, and it returns the an interval that is a conservative
 * approximation of the image of the GLSL function over the argument
 * intervals. That is, it is given a set of possible arguments and it returns
 * the set of possible values.
 *
 *//*--------------------------------------------------------------------*/
template <typename Sig_>
class Func : public FuncBase
{
public:
    typedef Sig_ Sig;
    typedef typename Sig::Ret Ret;
    typedef typename Sig::Arg0 Arg0;
    typedef typename Sig::Arg1 Arg1;
    typedef typename Sig::Arg2 Arg2;
    typedef typename Sig::Arg3 Arg3;
    typedef typename Sig::IRet IRet;
    typedef typename Sig::IArg0 IArg0;
    typedef typename Sig::IArg1 IArg1;
    typedef typename Sig::IArg2 IArg2;
    typedef typename Sig::IArg3 IArg3;
    typedef typename Sig::Args Args;
    typedef typename Sig::IArgs IArgs;
    typedef typename Sig::ArgExprs ArgExprs;

    void print(ostream &os, const BaseArgExprs &args) const
    {
        this->doPrint(os, args);
    }

    IRet apply(const EvalContext &ctx, const IArg0 &arg0 = IArg0(), const IArg1 &arg1 = IArg1(),
               const IArg2 &arg2 = IArg2(), const IArg3 &arg3 = IArg3()) const
    {
        return this->applyArgs(ctx, IArgs(arg0, arg1, arg2, arg3));
    }
    IRet applyArgs(const EvalContext &ctx, const IArgs &args) const
    {
        return this->doApply(ctx, args);
    }
    ExprP<Ret> operator()(const ExprP<Arg0> &arg0 = voidP(), const ExprP<Arg1> &arg1 = voidP(),
                          const ExprP<Arg2> &arg2 = voidP(), const ExprP<Arg3> &arg3 = voidP()) const;

    const ParamNames &getParamNames(void) const
    {
        return this->doGetParamNames();
    }

protected:
    virtual IRet doApply(const EvalContext &, const IArgs &) const = 0;
    virtual void doPrint(ostream &os, const BaseArgExprs &args) const
    {
        os << getName() << "(";

        if (isTypeValid<Arg0>())
            os << *args[0];

        if (isTypeValid<Arg1>())
            os << ", " << *args[1];

        if (isTypeValid<Arg2>())
            os << ", " << *args[2];

        if (isTypeValid<Arg3>())
            os << ", " << *args[3];

        os << ")";
    }

    virtual const ParamNames &doGetParamNames(void) const
    {
        static ParamNames names("a", "b", "c", "d");
        return names;
    }
};

template <typename Sig>
class Apply : public Expr<typename Sig::Ret>
{
public:
    typedef typename Sig::Ret Ret;
    typedef typename Sig::Arg0 Arg0;
    typedef typename Sig::Arg1 Arg1;
    typedef typename Sig::Arg2 Arg2;
    typedef typename Sig::Arg3 Arg3;
    typedef typename Expr<Ret>::Val Val;
    typedef typename Expr<Ret>::IVal IVal;
    typedef Func<Sig> ApplyFunc;
    typedef typename ApplyFunc::ArgExprs ArgExprs;

    Apply(const ApplyFunc &func, const ExprP<Arg0> &arg0 = voidP(), const ExprP<Arg1> &arg1 = voidP(),
          const ExprP<Arg2> &arg2 = voidP(), const ExprP<Arg3> &arg3 = voidP())
        : m_func(func)
        , m_args(arg0, arg1, arg2, arg3)
    {
    }

    Apply(const ApplyFunc &func, const ArgExprs &args) : m_func(func), m_args(args)
    {
    }

protected:
    void doPrintExpr(ostream &os) const
    {
        BaseArgExprs args;
        args.push_back(m_args.a.get());
        args.push_back(m_args.b.get());
        args.push_back(m_args.c.get());
        args.push_back(m_args.d.get());
        m_func.print(os, args);
    }

    IVal doEvaluate(const EvalContext &ctx) const
    {
        return m_func.apply(ctx, m_args.a->evaluate(ctx), m_args.b->evaluate(ctx), m_args.c->evaluate(ctx),
                            m_args.d->evaluate(ctx));
    }

    void doGetUsedFuncs(FuncSet &dst) const
    {
        m_func.getUsedFuncs(dst);
        m_args.a->getUsedFuncs(dst);
        m_args.b->getUsedFuncs(dst);
        m_args.c->getUsedFuncs(dst);
        m_args.d->getUsedFuncs(dst);
    }

    const ApplyFunc &m_func;
    ArgExprs m_args;
};

template <typename T>
class Alternatives : public Func<Signature<T, T, T>>
{
public:
    typedef typename Alternatives::Sig Sig;

protected:
    typedef typename Alternatives::IRet IRet;
    typedef typename Alternatives::IArgs IArgs;

    virtual string getName(void) const
    {
        return "alternatives";
    }
    virtual void doPrintDefinition(std::ostream &) const
    {
    }
    void doGetUsedFuncs(FuncSet &) const
    {
    }

    virtual IRet doApply(const EvalContext &, const IArgs &args) const
    {
        return unionIVal<T>(args.a, args.b);
    }

    virtual void doPrint(ostream &os, const BaseArgExprs &args) const
    {
        os << "{" << *args[0] << " | " << *args[1] << "}";
    }
};

template <typename Sig>
ExprP<typename Sig::Ret> createApply(const Func<Sig> &func, const typename Func<Sig>::ArgExprs &args)
{
    return exprP(new Apply<Sig>(func, args));
}

template <typename Sig>
ExprP<typename Sig::Ret> createApply(const Func<Sig> &func, const ExprP<typename Sig::Arg0> &arg0 = voidP(),
                                     const ExprP<typename Sig::Arg1> &arg1 = voidP(),
                                     const ExprP<typename Sig::Arg2> &arg2 = voidP(),
                                     const ExprP<typename Sig::Arg3> &arg3 = voidP())
{
    return exprP(new Apply<Sig>(func, arg0, arg1, arg2, arg3));
}

template <typename Sig>
ExprP<typename Sig::Ret> Func<Sig>::operator()(const ExprP<typename Sig::Arg0> &arg0,
                                               const ExprP<typename Sig::Arg1> &arg1,
                                               const ExprP<typename Sig::Arg2> &arg2,
                                               const ExprP<typename Sig::Arg3> &arg3) const
{
    return createApply(*this, arg0, arg1, arg2, arg3);
}

template <typename F>
ExprP<typename F::Ret> app(const ExprP<typename F::Arg0> &arg0 = voidP(), const ExprP<typename F::Arg1> &arg1 = voidP(),
                           const ExprP<typename F::Arg2> &arg2 = voidP(), const ExprP<typename F::Arg3> &arg3 = voidP())
{
    return createApply(instance<F>(), arg0, arg1, arg2, arg3);
}

template <typename F>
typename F::IRet call(const EvalContext &ctx, const typename F::IArg0 &arg0 = Void(),
                      const typename F::IArg1 &arg1 = Void(), const typename F::IArg2 &arg2 = Void(),
                      const typename F::IArg3 &arg3 = Void())
{
    return instance<F>().apply(ctx, arg0, arg1, arg2, arg3);
}

template <typename T>
ExprP<T> alternatives(const ExprP<T> &arg0, const ExprP<T> &arg1)
{
    return createApply<typename Alternatives<T>::Sig>(instance<Alternatives<T>>(), arg0, arg1);
}

template <typename Sig>
class ApplyVar : public Apply<Sig>
{
public:
    typedef typename Sig::Ret Ret;
    typedef typename Sig::Arg0 Arg0;
    typedef typename Sig::Arg1 Arg1;
    typedef typename Sig::Arg2 Arg2;
    typedef typename Sig::Arg3 Arg3;
    typedef typename Expr<Ret>::Val Val;
    typedef typename Expr<Ret>::IVal IVal;
    typedef Func<Sig> ApplyFunc;
    typedef typename ApplyFunc::ArgExprs ArgExprs;

    ApplyVar(const ApplyFunc &func, const VariableP<Arg0> &arg0, const VariableP<Arg1> &arg1,
             const VariableP<Arg2> &arg2, const VariableP<Arg3> &arg3)
        : Apply<Sig>(func, arg0, arg1, arg2, arg3)
    {
    }

protected:
    IVal doEvaluate(const EvalContext &ctx) const
    {
        const Variable<Arg0> &var0 = static_cast<const Variable<Arg0> &>(*this->m_args.a);
        const Variable<Arg1> &var1 = static_cast<const Variable<Arg1> &>(*this->m_args.b);
        const Variable<Arg2> &var2 = static_cast<const Variable<Arg2> &>(*this->m_args.c);
        const Variable<Arg3> &var3 = static_cast<const Variable<Arg3> &>(*this->m_args.d);
        return this->m_func.apply(ctx, ctx.env.lookup(var0), ctx.env.lookup(var1), ctx.env.lookup(var2),
                                  ctx.env.lookup(var3));
    }
};

template <typename Sig>
ExprP<typename Sig::Ret> applyVar(const Func<Sig> &func, const VariableP<typename Sig::Arg0> &arg0,
                                  const VariableP<typename Sig::Arg1> &arg1, const VariableP<typename Sig::Arg2> &arg2,
                                  const VariableP<typename Sig::Arg3> &arg3)
{
    return exprP(new ApplyVar<Sig>(func, arg0, arg1, arg2, arg3));
}

template <typename Sig_>
class DerivedFunc : public Func<Sig_>
{
public:
    typedef typename DerivedFunc::ArgExprs ArgExprs;
    typedef typename DerivedFunc::IRet IRet;
    typedef typename DerivedFunc::IArgs IArgs;
    typedef typename DerivedFunc::Ret Ret;
    typedef typename DerivedFunc::Arg0 Arg0;
    typedef typename DerivedFunc::Arg1 Arg1;
    typedef typename DerivedFunc::Arg2 Arg2;
    typedef typename DerivedFunc::Arg3 Arg3;
    typedef typename DerivedFunc::IArg0 IArg0;
    typedef typename DerivedFunc::IArg1 IArg1;
    typedef typename DerivedFunc::IArg2 IArg2;
    typedef typename DerivedFunc::IArg3 IArg3;

protected:
    void doPrintDefinition(ostream &os) const
    {
        const ParamNames &paramNames = this->getParamNames();

        initialize();

        os << dataTypeNameOf<Ret>() << " " << this->getName() << "(";
        if (isTypeValid<Arg0>())
            os << dataTypeNameOf<Arg0>() << " " << paramNames.a;
        if (isTypeValid<Arg1>())
            os << ", " << dataTypeNameOf<Arg1>() << " " << paramNames.b;
        if (isTypeValid<Arg2>())
            os << ", " << dataTypeNameOf<Arg2>() << " " << paramNames.c;
        if (isTypeValid<Arg3>())
            os << ", " << dataTypeNameOf<Arg3>() << " " << paramNames.d;
        os << ")\n{\n";

        for (size_t ndx = 0; ndx < m_body.size(); ++ndx)
            os << *m_body[ndx];
        os << "return " << *m_ret << ";\n";
        os << "}\n";
    }

    IRet doApply(const EvalContext &ctx, const IArgs &args) const
    {
        Environment funEnv;
        IArgs &mutArgs = const_cast<IArgs &>(args);
        IRet ret;

        initialize();

        funEnv.bind(*m_var0, args.a);
        funEnv.bind(*m_var1, args.b);
        funEnv.bind(*m_var2, args.c);
        funEnv.bind(*m_var3, args.d);

        {
            EvalContext funCtx(ctx.format, ctx.floatPrecision, funEnv, ctx.callDepth);

            for (size_t ndx = 0; ndx < m_body.size(); ++ndx)
                m_body[ndx]->execute(funCtx);

            ret = m_ret->evaluate(funCtx);
        }

        // \todo [lauri] Store references instead of values in environment
        const_cast<IArg0 &>(mutArgs.a) = funEnv.lookup(*m_var0);
        const_cast<IArg1 &>(mutArgs.b) = funEnv.lookup(*m_var1);
        const_cast<IArg2 &>(mutArgs.c) = funEnv.lookup(*m_var2);
        const_cast<IArg3 &>(mutArgs.d) = funEnv.lookup(*m_var3);

        return ret;
    }

    void doGetUsedFuncs(FuncSet &dst) const
    {
        initialize();
        if (dst.insert(this).second)
        {
            for (size_t ndx = 0; ndx < m_body.size(); ++ndx)
                m_body[ndx]->getUsedFuncs(dst);
            m_ret->getUsedFuncs(dst);
        }
    }

    virtual ExprP<Ret> doExpand(ExpandContext &ctx, const ArgExprs &args_) const = 0;

    // These are transparently initialized when first needed. They cannot be
    // initialized in the constructor because they depend on the doExpand
    // method of the subclass.

    mutable VariableP<Arg0> m_var0;
    mutable VariableP<Arg1> m_var1;
    mutable VariableP<Arg2> m_var2;
    mutable VariableP<Arg3> m_var3;
    mutable vector<StatementP> m_body;
    mutable ExprP<Ret> m_ret;

private:
    void initialize(void) const
    {
        if (!m_ret)
        {
            const ParamNames &paramNames = this->getParamNames();
            Counter symCounter;
            ExpandContext ctx(symCounter);
            ArgExprs args;

            args.a = m_var0 = variable<Arg0>(paramNames.a);
            args.b = m_var1 = variable<Arg1>(paramNames.b);
            args.c = m_var2 = variable<Arg2>(paramNames.c);
            args.d = m_var3 = variable<Arg3>(paramNames.d);

            m_ret  = this->doExpand(ctx, args);
            m_body = ctx.getStatements();
        }
    }
};

template <typename Sig>
class PrimitiveFunc : public Func<Sig>
{
public:
    typedef typename PrimitiveFunc::Ret Ret;
    typedef typename PrimitiveFunc::ArgExprs ArgExprs;

protected:
    void doPrintDefinition(ostream &) const
    {
    }
    void doGetUsedFuncs(FuncSet &) const
    {
    }
};

template <typename T>
class Cond : public PrimitiveFunc<Signature<T, bool, T, T>>
{
public:
    typedef typename Cond::IArgs IArgs;
    typedef typename Cond::IRet IRet;

    string getName(void) const
    {
        return "_cond";
    }

protected:
    void doPrint(ostream &os, const BaseArgExprs &args) const
    {
        os << "(" << *args[0] << " ? " << *args[1] << " : " << *args[2] << ")";
    }

    IRet doApply(const EvalContext &, const IArgs &iargs) const
    {
        IRet ret;

        if (iargs.a.contains(true))
            ret = unionIVal<T>(ret, iargs.b);

        if (iargs.a.contains(false))
            ret = unionIVal<T>(ret, iargs.c);

        return ret;
    }
};

template <typename T>
class CompareOperator : public PrimitiveFunc<Signature<bool, T, T>>
{
public:
    typedef typename CompareOperator::IArgs IArgs;
    typedef typename CompareOperator::IArg0 IArg0;
    typedef typename CompareOperator::IArg1 IArg1;
    typedef typename CompareOperator::IRet IRet;

protected:
    void doPrint(ostream &os, const BaseArgExprs &args) const
    {
        os << "(" << *args[0] << getSymbol() << *args[1] << ")";
    }

    Interval doApply(const EvalContext &, const IArgs &iargs) const
    {
        const IArg0 &arg0 = iargs.a;
        const IArg1 &arg1 = iargs.b;
        IRet ret;

        if (canSucceed(arg0, arg1))
            ret |= true;
        if (canFail(arg0, arg1))
            ret |= false;

        return ret;
    }

    virtual string getSymbol(void) const                        = 0;
    virtual bool canSucceed(const IArg0 &, const IArg1 &) const = 0;
    virtual bool canFail(const IArg0 &, const IArg1 &) const    = 0;
};

template <typename T>
class LessThan : public CompareOperator<T>
{
public:
    string getName(void) const
    {
        return "lessThan";
    }

protected:
    string getSymbol(void) const
    {
        return "<";
    }

    bool canSucceed(const Interval &a, const Interval &b) const
    {
        return (a.lo() < b.hi());
    }

    bool canFail(const Interval &a, const Interval &b) const
    {
        return !(a.hi() < b.lo());
    }
};

template <typename T>
ExprP<bool> operator<(const ExprP<T> &a, const ExprP<T> &b)
{
    return app<LessThan<T>>(a, b);
}

template <typename T>
ExprP<T> cond(const ExprP<bool> &test, const ExprP<T> &consequent, const ExprP<T> &alternative)
{
    return app<Cond<T>>(test, consequent, alternative);
}

/*--------------------------------------------------------------------*//*!
 *
 * @}
 *
 *//*--------------------------------------------------------------------*/

class FloatFunc1 : public PrimitiveFunc<Signature<float, float>>
{
protected:
    Interval doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        return this->applyMonotone(ctx, iargs.a);
    }

    Interval applyMonotone(const EvalContext &ctx, const Interval &iarg0) const
    {
        Interval ret;

        TCU_INTERVAL_APPLY_MONOTONE1(ret, arg0, iarg0, val,
                                     TCU_SET_INTERVAL(val, point, point = this->applyPoint(ctx, arg0)));

        ret |= innerExtrema(ctx, iarg0);
        ret &= (this->getCodomain() | TCU_NAN);

        return ctx.format.convert(ret);
    }

    virtual Interval innerExtrema(const EvalContext &, const Interval &) const
    {
        return Interval(); // empty interval, i.e. no extrema
    }

    virtual Interval applyPoint(const EvalContext &ctx, double arg0) const
    {
        const double exact = this->applyExact(arg0);
        const double prec  = this->precision(ctx, exact, arg0);
        const double wprec = this->warningPrecision(ctx, exact, arg0);
        Interval ioutput   = exact + Interval(-prec, prec);
        ioutput.warning(exact - wprec, exact + wprec);
        return ioutput;
    }

    virtual double applyExact(double) const
    {
        TCU_THROW(InternalError, "Cannot apply");
    }

    virtual Interval getCodomain(void) const
    {
        return Interval::unbounded(true);
    }

    virtual double precision(const EvalContext &ctx, double, double) const = 0;

    virtual double warningPrecision(const EvalContext &ctx, double exact, double arg0) const
    {
        return precision(ctx, exact, arg0);
    }
};

class CFloatFunc1 : public FloatFunc1
{
public:
    CFloatFunc1(const string &name, DoubleFunc1 &func) : m_name(name), m_func(func)
    {
    }

    string getName(void) const
    {
        return m_name;
    }

protected:
    double applyExact(double x) const
    {
        return m_func(x);
    }

    const string m_name;
    DoubleFunc1 &m_func;
};

class FloatFunc2 : public PrimitiveFunc<Signature<float, float, float>>
{
protected:
    Interval doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        return this->applyMonotone(ctx, iargs.a, iargs.b);
    }

    Interval applyMonotone(const EvalContext &ctx, const Interval &xi, const Interval &yi) const
    {
        Interval reti;

        TCU_INTERVAL_APPLY_MONOTONE2(reti, x, xi, y, yi, ret,
                                     TCU_SET_INTERVAL(ret, point, point = this->applyPoint(ctx, x, y)));
        reti |= innerExtrema(ctx, xi, yi);
        reti &= (this->getCodomain() | TCU_NAN);

        return ctx.format.convert(reti);
    }

    virtual Interval innerExtrema(const EvalContext &, const Interval &, const Interval &) const
    {
        return Interval(); // empty interval, i.e. no extrema
    }

    virtual Interval applyPoint(const EvalContext &ctx, double x, double y) const
    {
        const double exact = this->applyExact(x, y);
        const double prec  = this->precision(ctx, exact, x, y);

        return exact + Interval(-prec, prec);
    }

    virtual double applyExact(double, double) const
    {
        TCU_THROW(InternalError, "Cannot apply");
    }

    virtual Interval getCodomain(void) const
    {
        return Interval::unbounded(true);
    }

    virtual double precision(const EvalContext &ctx, double ret, double x, double y) const = 0;
};

class CFloatFunc2 : public FloatFunc2
{
public:
    CFloatFunc2(const string &name, DoubleFunc2 &func) : m_name(name), m_func(func)
    {
    }

    string getName(void) const
    {
        return m_name;
    }

protected:
    double applyExact(double x, double y) const
    {
        return m_func(x, y);
    }

    const string m_name;
    DoubleFunc2 &m_func;
};

class InfixOperator : public FloatFunc2
{
protected:
    virtual string getSymbol(void) const = 0;

    void doPrint(ostream &os, const BaseArgExprs &args) const
    {
        os << "(" << *args[0] << " " << getSymbol() << " " << *args[1] << ")";
    }

    Interval applyPoint(const EvalContext &ctx, double x, double y) const
    {
        const double exact = this->applyExact(x, y);

        // Allow either representable number on both sides of the exact value,
        // but require exactly representable values to be preserved.
        return ctx.format.roundOut(exact, !std::isinf(x) && !std::isinf(y));
    }

    double precision(const EvalContext &, double, double, double) const
    {
        return 0.0;
    }
};

class FloatFunc3 : public PrimitiveFunc<Signature<float, float, float, float>>
{
protected:
    Interval doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        return this->applyMonotone(ctx, iargs.a, iargs.b, iargs.c);
    }

    Interval applyMonotone(const EvalContext &ctx, const Interval &xi, const Interval &yi, const Interval &zi) const
    {
        Interval reti;
        TCU_INTERVAL_APPLY_MONOTONE3(reti, x, xi, y, yi, z, zi, ret,
                                     TCU_SET_INTERVAL(ret, point, point = this->applyPoint(ctx, x, y, z)));
        return ctx.format.convert(reti);
    }

    virtual Interval applyPoint(const EvalContext &ctx, double x, double y, double z) const
    {
        const double exact = this->applyExact(x, y, z);
        const double prec  = this->precision(ctx, exact, x, y, z);
        return exact + Interval(-prec, prec);
    }

    virtual double applyExact(double, double, double) const
    {
        TCU_THROW(InternalError, "Cannot apply");
    }

    virtual double precision(const EvalContext &ctx, double result, double x, double y, double z) const = 0;
};

// We define syntactic sugar functions for expression constructors. Since
// these have the same names as ordinary mathematical operations (sin, log
// etc.), it's better to give them a dedicated namespace.
namespace Functions
{

using namespace tcu;

class Add : public InfixOperator
{
public:
    string getName(void) const
    {
        return "add";
    }
    string getSymbol(void) const
    {
        return "+";
    }

    Interval doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        // Fast-path for common case
        if (iargs.a.isOrdinary(ctx.format.getMaxValue()) && iargs.b.isOrdinary(ctx.format.getMaxValue()))
        {
            Interval ret;
            TCU_SET_INTERVAL_BOUNDS(ret, sum, sum = iargs.a.lo() + iargs.b.lo(), sum = iargs.a.hi() + iargs.b.hi());
            return ctx.format.convert(ctx.format.roundOut(ret, true));
        }
        return this->applyMonotone(ctx, iargs.a, iargs.b);
    }

protected:
    double applyExact(double x, double y) const
    {
        return x + y;
    }
};

class Mul : public InfixOperator
{
public:
    string getName(void) const
    {
        return "mul";
    }
    string getSymbol(void) const
    {
        return "*";
    }

    Interval doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        Interval a = iargs.a;
        Interval b = iargs.b;

        // Fast-path for common case
        if (a.isOrdinary(ctx.format.getMaxValue()) && b.isOrdinary(ctx.format.getMaxValue()))
        {
            Interval ret;
            if (a.hi() < 0)
            {
                a = -a;
                b = -b;
            }
            if (a.lo() >= 0 && b.lo() >= 0)
            {
                TCU_SET_INTERVAL_BOUNDS(ret, prod, prod = iargs.a.lo() * iargs.b.lo(),
                                        prod = iargs.a.hi() * iargs.b.hi());
                return ctx.format.convert(ctx.format.roundOut(ret, true));
            }
            if (a.lo() >= 0 && b.hi() <= 0)
            {
                TCU_SET_INTERVAL_BOUNDS(ret, prod, prod = iargs.a.hi() * iargs.b.lo(),
                                        prod = iargs.a.lo() * iargs.b.hi());
                return ctx.format.convert(ctx.format.roundOut(ret, true));
            }
        }
        return this->applyMonotone(ctx, iargs.a, iargs.b);
    }

protected:
    double applyExact(double x, double y) const
    {
        return x * y;
    }

    Interval innerExtrema(const EvalContext &, const Interval &xi, const Interval &yi) const
    {
        if (((xi.contains(-TCU_INFINITY) || xi.contains(TCU_INFINITY)) && yi.contains(0.0)) ||
            ((yi.contains(-TCU_INFINITY) || yi.contains(TCU_INFINITY)) && xi.contains(0.0)))
            return Interval(TCU_NAN);

        return Interval();
    }
};

class Sub : public InfixOperator
{
public:
    string getName(void) const
    {
        return "sub";
    }
    string getSymbol(void) const
    {
        return "-";
    }

    Interval doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        // Fast-path for common case
        if (iargs.a.isOrdinary(ctx.format.getMaxValue()) && iargs.b.isOrdinary(ctx.format.getMaxValue()))
        {
            Interval ret;

            TCU_SET_INTERVAL_BOUNDS(ret, diff, diff = iargs.a.lo() - iargs.b.hi(), diff = iargs.a.hi() - iargs.b.lo());
            return ctx.format.convert(ctx.format.roundOut(ret, true));
        }
        else
        {
            return this->applyMonotone(ctx, iargs.a, iargs.b);
        }
    }

protected:
    double applyExact(double x, double y) const
    {
        return x - y;
    }
};

class Negate : public FloatFunc1
{
public:
    string getName(void) const
    {
        return "_negate";
    }
    void doPrint(ostream &os, const BaseArgExprs &args) const
    {
        os << "-" << *args[0];
    }

protected:
    double precision(const EvalContext &, double, double) const
    {
        return 0.0;
    }
    double applyExact(double x) const
    {
        return -x;
    }
};

class Div : public InfixOperator
{
public:
    string getName(void) const
    {
        return "div";
    }

protected:
    string getSymbol(void) const
    {
        return "/";
    }

    Interval innerExtrema(const EvalContext &, const Interval &nom, const Interval &den) const
    {
        Interval ret;

        if (den.contains(0.0))
        {
            if (nom.contains(0.0))
                ret |= TCU_NAN;

            if (nom.lo() < 0.0 || nom.hi() > 0.0)
                ret |= Interval::unbounded();
        }

        return ret;
    }

    double applyExact(double x, double y) const
    {
        return x / y;
    }

    Interval applyPoint(const EvalContext &ctx, double x, double y) const
    {
        Interval ret = FloatFunc2::applyPoint(ctx, x, y);

        if (!std::isinf(x) && !std::isinf(y) && y != 0.0)
        {
            const Interval dst = ctx.format.convert(ret);
            if (dst.contains(-TCU_INFINITY))
                ret |= -ctx.format.getMaxValue();
            if (dst.contains(+TCU_INFINITY))
                ret |= +ctx.format.getMaxValue();
        }

        return ret;
    }

    double precision(const EvalContext &ctx, double ret, double, double den) const
    {
        const FloatFormat &fmt = ctx.format;

        // \todo [2014-03-05 lauri] Check that the limits in GLSL 3.10 are actually correct.
        // For now, we assume that division's precision is 2.5 ULP when the value is within
        // [2^MINEXP, 2^MAXEXP-1]

        if (den == 0.0)
            return 0.0; // Result must be exactly inf
        else if (de::inBounds(deAbs(den), deLdExp(1.0, fmt.getMinExp()), deLdExp(1.0, fmt.getMaxExp() - 1)))
            return fmt.ulp(ret, 2.5);
        else
            return TCU_INFINITY; // Can be any number, but must be a number.
    }
};

class InverseSqrt : public FloatFunc1
{
public:
    string getName(void) const
    {
        return "inversesqrt";
    }

protected:
    double applyExact(double x) const
    {
        return 1.0 / deSqrt(x);
    }

    double precision(const EvalContext &ctx, double ret, double x) const
    {
        return x <= 0 ? TCU_NAN : ctx.format.ulp(ret, 2.0);
    }

    Interval getCodomain(void) const
    {
        return Interval(0.0, TCU_INFINITY);
    }
};

class ExpFunc : public CFloatFunc1
{
public:
    ExpFunc(const string &name, DoubleFunc1 &func) : CFloatFunc1(name, func)
    {
    }

protected:
    double precision(const EvalContext &ctx, double ret, double x) const
    {
        switch (ctx.floatPrecision)
        {
        case glu::PRECISION_HIGHP:
            return ctx.format.ulp(ret, 3.0 + 2.0 * deAbs(x));
        case glu::PRECISION_MEDIUMP:
            return ctx.format.ulp(ret, 2.0 + 2.0 * deAbs(x));
        case glu::PRECISION_LOWP:
            return ctx.format.ulp(ret, 2.0);
        default:
            DE_FATAL("Impossible");
        }
        return 0;
    }

    Interval getCodomain(void) const
    {
        return Interval(0.0, TCU_INFINITY);
    }
};

class Exp2 : public ExpFunc
{
public:
    Exp2(void) : ExpFunc("exp2", deExp2)
    {
    }
};
class Exp : public ExpFunc
{
public:
    Exp(void) : ExpFunc("exp", deExp)
    {
    }
};

ExprP<float> exp2(const ExprP<float> &x)
{
    return app<Exp2>(x);
}
ExprP<float> exp(const ExprP<float> &x)
{
    return app<Exp>(x);
}

class LogFunc : public CFloatFunc1
{
public:
    LogFunc(const string &name, DoubleFunc1 &func) : CFloatFunc1(name, func)
    {
    }

protected:
    double precision(const EvalContext &ctx, double ret, double x) const
    {
        if (x <= 0)
            return TCU_NAN;

        switch (ctx.floatPrecision)
        {
        case glu::PRECISION_HIGHP:
            return (0.5 <= x && x <= 2.0) ? deLdExp(1.0, -21) : ctx.format.ulp(ret, 3.0);
        case glu::PRECISION_MEDIUMP:
            return (0.5 <= x && x <= 2.0) ? deLdExp(1.0, -7) : ctx.format.ulp(ret, 2.0);
        case glu::PRECISION_LOWP:
            return ctx.format.ulp(ret, 2.0);
        default:
            DE_FATAL("Impossible");
        }

        return 0;
    }

    // OpenGL API Issue #57 "Clarifying the required ULP precision for GLSL built-in log()". Agreed that
    // implementations will be allowed 4 ULPs for HIGHP Log/Log2, but CTS should generate a quality warning.
    double warningPrecision(const EvalContext &ctx, double ret, double x) const
    {
        if (ctx.floatPrecision == glu::PRECISION_HIGHP && x > 0)
        {
            return (0.5 <= x && x <= 2.0) ? deLdExp(1.0, -21) : ctx.format.ulp(ret, 4.0);
        }
        else
        {
            return precision(ctx, ret, x);
        }
    }
};

class Log2 : public LogFunc
{
public:
    Log2(void) : LogFunc("log2", deLog2)
    {
    }
};
class Log : public LogFunc
{
public:
    Log(void) : LogFunc("log", deLog)
    {
    }
};

ExprP<float> log2(const ExprP<float> &x)
{
    return app<Log2>(x);
}
ExprP<float> log(const ExprP<float> &x)
{
    return app<Log>(x);
}

#define DEFINE_CONSTRUCTOR1(CLASS, TRET, NAME, T0) \
    ExprP<TRET> NAME(const ExprP<T0> &arg0)        \
    {                                              \
        return app<CLASS>(arg0);                   \
    }

#define DEFINE_DERIVED1(CLASS, TRET, NAME, T0, ARG0, EXPANSION)                   \
    class CLASS : public DerivedFunc<Signature<TRET, T0>> /* NOLINT(CLASS) */     \
    {                                                                             \
    public:                                                                       \
        string getName(void) const                                                \
        {                                                                         \
            return #NAME;                                                         \
        }                                                                         \
                                                                                  \
    protected:                                                                    \
        ExprP<TRET> doExpand(ExpandContext &, const CLASS::ArgExprs &args_) const \
        {                                                                         \
            const ExprP<float> &ARG0 = args_.a;                                   \
            return EXPANSION;                                                     \
        }                                                                         \
    };                                                                            \
    DEFINE_CONSTRUCTOR1(CLASS, TRET, NAME, T0)

#define DEFINE_DERIVED_FLOAT1(CLASS, NAME, ARG0, EXPANSION) DEFINE_DERIVED1(CLASS, float, NAME, float, ARG0, EXPANSION)

#define DEFINE_CONSTRUCTOR2(CLASS, TRET, NAME, T0, T1)             \
    ExprP<TRET> NAME(const ExprP<T0> &arg0, const ExprP<T1> &arg1) \
    {                                                              \
        return app<CLASS>(arg0, arg1);                             \
    }

#define DEFINE_DERIVED2(CLASS, TRET, NAME, T0, Arg0, T1, Arg1, EXPANSION)         \
    class CLASS : public DerivedFunc<Signature<TRET, T0, T1>> /* NOLINT(CLASS) */ \
    {                                                                             \
    public:                                                                       \
        string getName(void) const                                                \
        {                                                                         \
            return #NAME;                                                         \
        }                                                                         \
                                                                                  \
    protected:                                                                    \
        ExprP<TRET> doExpand(ExpandContext &, const ArgExprs &args_) const        \
        {                                                                         \
            const ExprP<T0> &Arg0 = args_.a;                                      \
            const ExprP<T1> &Arg1 = args_.b;                                      \
            return EXPANSION;                                                     \
        }                                                                         \
    };                                                                            \
    DEFINE_CONSTRUCTOR2(CLASS, TRET, NAME, T0, T1)

#define DEFINE_DERIVED_FLOAT2(CLASS, NAME, Arg0, Arg1, EXPANSION) \
    DEFINE_DERIVED2(CLASS, float, NAME, float, Arg0, float, Arg1, EXPANSION)

#define DEFINE_CONSTRUCTOR3(CLASS, TRET, NAME, T0, T1, T2)                                \
    ExprP<TRET> NAME(const ExprP<T0> &arg0, const ExprP<T1> &arg1, const ExprP<T2> &arg2) \
    {                                                                                     \
        return app<CLASS>(arg0, arg1, arg2);                                              \
    }

#define DEFINE_DERIVED3(CLASS, TRET, NAME, T0, ARG0, T1, ARG1, T2, ARG2, EXPANSION)   \
    class CLASS : public DerivedFunc<Signature<TRET, T0, T1, T2>> /* NOLINT(CLASS) */ \
    {                                                                                 \
    public:                                                                           \
        string getName(void) const                                                    \
        {                                                                             \
            return #NAME;                                                             \
        }                                                                             \
                                                                                      \
    protected:                                                                        \
        ExprP<TRET> doExpand(ExpandContext &, const ArgExprs &args_) const            \
        {                                                                             \
            const ExprP<T0> &ARG0 = args_.a;                                          \
            const ExprP<T1> &ARG1 = args_.b;                                          \
            const ExprP<T2> &ARG2 = args_.c;                                          \
            return EXPANSION;                                                         \
        }                                                                             \
    };                                                                                \
    DEFINE_CONSTRUCTOR3(CLASS, TRET, NAME, T0, T1, T2)

#define DEFINE_DERIVED_FLOAT3(CLASS, NAME, ARG0, ARG1, ARG2, EXPANSION) \
    DEFINE_DERIVED3(CLASS, float, NAME, float, ARG0, float, ARG1, float, ARG2, EXPANSION)

#define DEFINE_CONSTRUCTOR4(CLASS, TRET, NAME, T0, T1, T2, T3)                                                   \
    ExprP<TRET> NAME(const ExprP<T0> &arg0, const ExprP<T1> &arg1, const ExprP<T2> &arg2, const ExprP<T3> &arg3) \
    {                                                                                                            \
        return app<CLASS>(arg0, arg1, arg2, arg3);                                                               \
    }

DEFINE_DERIVED_FLOAT1(Sqrt, sqrt, x, (x == 0.0f ? constant(1.0f) : (constant(1.0f) / app<InverseSqrt>(x))))
DEFINE_DERIVED_FLOAT2(Pow, pow, x, y, exp2(y *log2(x)))
DEFINE_DERIVED_FLOAT1(Radians, radians, d, (constant(DE_PI) / constant(180.0f)) * d)
DEFINE_DERIVED_FLOAT1(Degrees, degrees, r, (constant(180.0f) / constant(DE_PI)) * r)

class TrigFunc : public CFloatFunc1
{
public:
    TrigFunc(const string &name, DoubleFunc1 &func, const Interval &loEx, const Interval &hiEx)
        : CFloatFunc1(name, func)
        , m_loExtremum(loEx)
        , m_hiExtremum(hiEx)
    {
    }

protected:
    Interval innerExtrema(const EvalContext &, const Interval &angle) const
    {
        const double lo   = angle.lo();
        const double hi   = angle.hi();
        const int loSlope = doGetSlope(lo);
        const int hiSlope = doGetSlope(hi);

        // Detect the high and low values the function can take between the
        // interval endpoints.
        if (angle.length() >= 2.0 * DE_PI_DOUBLE)
        {
            // The interval is longer than a full cycle, so it must get all possible values.
            return m_hiExtremum | m_loExtremum;
        }
        else if (loSlope == 1 && hiSlope == -1)
        {
            // The slope can change from positive to negative only at the maximum value.
            return m_hiExtremum;
        }
        else if (loSlope == -1 && hiSlope == 1)
        {
            // The slope can change from negative to positive only at the maximum value.
            return m_loExtremum;
        }
        else if (loSlope == hiSlope && deIntSign(applyExact(hi) - applyExact(lo)) * loSlope == -1)
        {
            // The slope has changed twice between the endpoints, so both extrema are included.
            return m_hiExtremum | m_loExtremum;
        }

        return Interval();
    }

    Interval getCodomain(void) const
    {
        // Ensure that result is always within [-1, 1], or NaN (for +-inf)
        return Interval(-1.0, 1.0) | TCU_NAN;
    }

    double precision(const EvalContext &ctx, double ret, double arg) const
    {
        if (ctx.floatPrecision == glu::PRECISION_HIGHP)
        {
            // Use precision from OpenCL fast relaxed math
            if (-DE_PI_DOUBLE <= arg && arg <= DE_PI_DOUBLE)
            {
                return deLdExp(1.0, -11);
            }
            else
            {
                // "larger otherwise", let's pick |x| * 2^-12 , which is slightly over
                // 2^-11 at x == pi.
                return deLdExp(deAbs(arg), -12);
            }
        }
        else if (ctx.floatPrecision == glu::PRECISION_MEDIUMP)
        {
            if (-DE_PI_DOUBLE <= arg && arg <= DE_PI_DOUBLE)
            {
                // from OpenCL half-float extension specification
                return ctx.format.ulp(ret, 2.0);
            }
            else
            {
                // |x| * 2^-10, slightly larger than 2 ULP at x == pi
                return deLdExp(deAbs(arg), -10);
            }
        }
        else
        {
            DE_ASSERT(ctx.floatPrecision == glu::PRECISION_LOWP);

            // from OpenCL half-float extension specification
            return ctx.format.ulp(ret, 2.0);
        }
    }

    virtual int doGetSlope(double angle) const = 0;

    Interval m_loExtremum;
    Interval m_hiExtremum;
};

class Sin : public TrigFunc
{
public:
    Sin(void) : TrigFunc("sin", deSin, -1.0, 1.0)
    {
    }

protected:
    int doGetSlope(double angle) const
    {
        return deIntSign(deCos(angle));
    }
};

ExprP<float> sin(const ExprP<float> &x)
{
    return app<Sin>(x);
}

class Cos : public TrigFunc
{
public:
    Cos(void) : TrigFunc("cos", deCos, -1.0, 1.0)
    {
    }

protected:
    int doGetSlope(double angle) const
    {
        return -deIntSign(deSin(angle));
    }
};

ExprP<float> cos(const ExprP<float> &x)
{
    return app<Cos>(x);
}

DEFINE_DERIVED_FLOAT1(Tan, tan, x, sin(x) * (constant(1.0f) / cos(x)))

class ASin : public CFloatFunc1
{
public:
    ASin(void) : CFloatFunc1("asin", deAsin)
    {
    }

protected:
    double precision(const EvalContext &ctx, double, double x) const
    {
        if (!de::inBounds(x, -1.0, 1.0))
            return TCU_NAN;

        if (ctx.floatPrecision == glu::PRECISION_HIGHP)
        {
            // Absolute error of 2^-11
            return deLdExp(1.0, -11);
        }
        else
        {
            // Absolute error of 2^-8
            return deLdExp(1.0, -8);
        }
    }
};

class ArcTrigFunc : public CFloatFunc1
{
public:
    ArcTrigFunc(const string &name, DoubleFunc1 &func, double precisionULPs, const Interval &domain,
                const Interval &codomain)
        : CFloatFunc1(name, func)
        , m_precision(precisionULPs)
        , m_domain(domain)
        , m_codomain(codomain)
    {
    }

protected:
    double precision(const EvalContext &ctx, double ret, double x) const
    {
        if (!m_domain.contains(x))
            return TCU_NAN;

        if (ctx.floatPrecision == glu::PRECISION_HIGHP)
        {
            // Use OpenCL's fast relaxed math precision
            return ctx.format.ulp(ret, m_precision);
        }
        else
        {
            // Use OpenCL half-float spec
            return ctx.format.ulp(ret, 2.0);
        }
    }

    // We could implement getCodomain with m_codomain, but choose not to,
    // because it seems too strict with trascendental constants like pi.

    const double m_precision;
    const Interval m_domain;
    const Interval m_codomain;
};

class ACos : public ArcTrigFunc
{
public:
    ACos(void) : ArcTrigFunc("acos", deAcos, 4096.0, Interval(-1.0, 1.0), Interval(0.0, DE_PI_DOUBLE))
    {
    }
};

class ATan : public ArcTrigFunc
{
public:
    ATan(void)
        : ArcTrigFunc("atan", deAtanOver, 4096.0, Interval::unbounded(),
                      Interval(-DE_PI_DOUBLE * 0.5, DE_PI_DOUBLE * 0.5))
    {
    }
};

class ATan2 : public CFloatFunc2
{
public:
    ATan2(void) : CFloatFunc2("atan", deAtan2)
    {
    }

protected:
    Interval innerExtrema(const EvalContext &ctx, const Interval &yi, const Interval &xi) const
    {
        Interval ret;

        if (yi.contains(0.0))
        {
            if (xi.contains(0.0))
                ret |= TCU_NAN;
            if (xi.intersects(Interval(-TCU_INFINITY, 0.0)))
                ret |= Interval(-DE_PI_DOUBLE, DE_PI_DOUBLE);
        }

        if (!yi.isFinite(ctx.format.getMaxValue()) || !xi.isFinite(ctx.format.getMaxValue()))
        {
            // Infinities may not be supported, allow anything, including NaN
            ret |= TCU_NAN;
        }

        return ret;
    }

    double precision(const EvalContext &ctx, double ret, double, double) const
    {
        if (ctx.floatPrecision == glu::PRECISION_HIGHP)
            return ctx.format.ulp(ret, 4096.0);
        else
            return ctx.format.ulp(ret, 2.0);
    }

    // Codomain could be [-pi, pi], but that would probably be too strict.
};

DEFINE_DERIVED_FLOAT1(Sinh, sinh, x, (exp(x) - exp(-x)) / constant(2.0f))
DEFINE_DERIVED_FLOAT1(Cosh, cosh, x, (exp(x) + exp(-x)) / constant(2.0f))
DEFINE_DERIVED_FLOAT1(Tanh, tanh, x, sinh(x) / cosh(x))

// These are not defined as derived forms in the GLSL ES spec, but
// that gives us a reasonable precision.
DEFINE_DERIVED_FLOAT1(ASinh, asinh, x, log(x + sqrt(x * x + constant(1.0f))))
DEFINE_DERIVED_FLOAT1(ACosh, acosh, x,
                      log(x +
                          sqrt(alternatives((x + constant(1.0f)) * (x - constant(1.0f)), (x * x - constant(1.0f))))))
DEFINE_DERIVED_FLOAT1(ATanh, atanh, x, constant(0.5f) * log((constant(1.0f) + x) / (constant(1.0f) - x)))

template <typename T>
class GetComponent : public PrimitiveFunc<Signature<typename T::Element, T, int>>
{
public:
    typedef typename GetComponent::IRet IRet;

    string getName(void) const
    {
        return "_getComponent";
    }

    void print(ostream &os, const BaseArgExprs &args) const
    {
        os << *args[0] << "[" << *args[1] << "]";
    }

protected:
    IRet doApply(const EvalContext &, const typename GetComponent::IArgs &iargs) const
    {
        IRet ret;

        for (int compNdx = 0; compNdx < T::SIZE; ++compNdx)
        {
            if (iargs.b.contains(compNdx))
                ret = unionIVal<typename T::Element>(ret, iargs.a[compNdx]);
        }

        return ret;
    }
};

template <typename T>
ExprP<typename T::Element> getComponent(const ExprP<T> &container, int ndx)
{
    DE_ASSERT(0 <= ndx && ndx < T::SIZE);
    return app<GetComponent<T>>(container, constant(ndx));
}

template <typename T>
string vecNamePrefix(void);
template <>
string vecNamePrefix<float>(void)
{
    return "";
}
template <>
string vecNamePrefix<int>(void)
{
    return "i";
}
template <>
string vecNamePrefix<bool>(void)
{
    return "b";
}

template <typename T, int Size>
string vecName(void)
{
    return vecNamePrefix<T>() + "vec" + de::toString(Size);
}

template <typename T, int Size>
class GenVec;

template <typename T>
class GenVec<T, 1> : public DerivedFunc<Signature<T, T>>
{
public:
    typedef typename GenVec<T, 1>::ArgExprs ArgExprs;

    string getName(void) const
    {
        return "_" + vecName<T, 1>();
    }

protected:
    ExprP<T> doExpand(ExpandContext &, const ArgExprs &args) const
    {
        return args.a;
    }
};

template <typename T>
class GenVec<T, 2> : public PrimitiveFunc<Signature<Vector<T, 2>, T, T>>
{
public:
    typedef typename GenVec::IRet IRet;
    typedef typename GenVec::IArgs IArgs;

    string getName(void) const
    {
        return vecName<T, 2>();
    }

protected:
    IRet doApply(const EvalContext &, const IArgs &iargs) const
    {
        return IRet(iargs.a, iargs.b);
    }
};

template <typename T>
class GenVec<T, 3> : public PrimitiveFunc<Signature<Vector<T, 3>, T, T, T>>
{
public:
    typedef typename GenVec::IRet IRet;
    typedef typename GenVec::IArgs IArgs;

    string getName(void) const
    {
        return vecName<T, 3>();
    }

protected:
    IRet doApply(const EvalContext &, const IArgs &iargs) const
    {
        return IRet(iargs.a, iargs.b, iargs.c);
    }
};

template <typename T>
class GenVec<T, 4> : public PrimitiveFunc<Signature<Vector<T, 4>, T, T, T, T>>
{
public:
    typedef typename GenVec::IRet IRet;
    typedef typename GenVec::IArgs IArgs;

    string getName(void) const
    {
        return vecName<T, 4>();
    }

protected:
    IRet doApply(const EvalContext &, const IArgs &iargs) const
    {
        return IRet(iargs.a, iargs.b, iargs.c, iargs.d);
    }
};

template <typename T, int Rows, int Columns>
class GenMat;

template <typename T, int Rows>
class GenMat<T, Rows, 2> : public PrimitiveFunc<Signature<Matrix<T, Rows, 2>, Vector<T, Rows>, Vector<T, Rows>>>
{
public:
    typedef typename GenMat::Ret Ret;
    typedef typename GenMat::IRet IRet;
    typedef typename GenMat::IArgs IArgs;

    string getName(void) const
    {
        return dataTypeNameOf<Ret>();
    }

protected:
    IRet doApply(const EvalContext &, const IArgs &iargs) const
    {
        IRet ret;
        ret[0] = iargs.a;
        ret[1] = iargs.b;
        return ret;
    }
};

template <typename T, int Rows>
class GenMat<T, Rows, 3>
    : public PrimitiveFunc<Signature<Matrix<T, Rows, 3>, Vector<T, Rows>, Vector<T, Rows>, Vector<T, Rows>>>
{
public:
    typedef typename GenMat::Ret Ret;
    typedef typename GenMat::IRet IRet;
    typedef typename GenMat::IArgs IArgs;

    string getName(void) const
    {
        return dataTypeNameOf<Ret>();
    }

protected:
    IRet doApply(const EvalContext &, const IArgs &iargs) const
    {
        IRet ret;
        ret[0] = iargs.a;
        ret[1] = iargs.b;
        ret[2] = iargs.c;
        return ret;
    }
};

template <typename T, int Rows>
class GenMat<T, Rows, 4>
    : public PrimitiveFunc<
          Signature<Matrix<T, Rows, 4>, Vector<T, Rows>, Vector<T, Rows>, Vector<T, Rows>, Vector<T, Rows>>>
{
public:
    typedef typename GenMat::Ret Ret;
    typedef typename GenMat::IRet IRet;
    typedef typename GenMat::IArgs IArgs;

    string getName(void) const
    {
        return dataTypeNameOf<Ret>();
    }

protected:
    IRet doApply(const EvalContext &, const IArgs &iargs) const
    {
        IRet ret;
        ret[0] = iargs.a;
        ret[1] = iargs.b;
        ret[2] = iargs.c;
        ret[3] = iargs.d;
        return ret;
    }
};

template <typename T, int Rows>
ExprP<Matrix<T, Rows, 2>> mat2(const ExprP<Vector<T, Rows>> &arg0, const ExprP<Vector<T, Rows>> &arg1)
{
    return app<GenMat<T, Rows, 2>>(arg0, arg1);
}

template <typename T, int Rows>
ExprP<Matrix<T, Rows, 3>> mat3(const ExprP<Vector<T, Rows>> &arg0, const ExprP<Vector<T, Rows>> &arg1,
                               const ExprP<Vector<T, Rows>> &arg2)
{
    return app<GenMat<T, Rows, 3>>(arg0, arg1, arg2);
}

template <typename T, int Rows>
ExprP<Matrix<T, Rows, 4>> mat4(const ExprP<Vector<T, Rows>> &arg0, const ExprP<Vector<T, Rows>> &arg1,
                               const ExprP<Vector<T, Rows>> &arg2, const ExprP<Vector<T, Rows>> &arg3)
{
    return app<GenMat<T, Rows, 4>>(arg0, arg1, arg2, arg3);
}

template <int Rows, int Cols>
class MatNeg : public PrimitiveFunc<Signature<Matrix<float, Rows, Cols>, Matrix<float, Rows, Cols>>>
{
public:
    typedef typename MatNeg::IRet IRet;
    typedef typename MatNeg::IArgs IArgs;

    string getName(void) const
    {
        return "_matNeg";
    }

protected:
    void doPrint(ostream &os, const BaseArgExprs &args) const
    {
        os << "-(" << *args[0] << ")";
    }

    IRet doApply(const EvalContext &, const IArgs &iargs) const
    {
        IRet ret;

        for (int col = 0; col < Cols; ++col)
        {
            for (int row = 0; row < Rows; ++row)
                ret[col][row] = -iargs.a[col][row];
        }

        return ret;
    }
};

template <typename T, typename Sig>
class CompWiseFunc : public PrimitiveFunc<Sig>
{
public:
    typedef Func<Signature<T, T, T>> ScalarFunc;

    string getName(void) const
    {
        return doGetScalarFunc().getName();
    }

protected:
    void doPrint(ostream &os, const BaseArgExprs &args) const
    {
        doGetScalarFunc().print(os, args);
    }

    virtual const ScalarFunc &doGetScalarFunc(void) const = 0;
};

template <int Rows, int Cols>
class CompMatFuncBase
    : public CompWiseFunc<float,
                          Signature<Matrix<float, Rows, Cols>, Matrix<float, Rows, Cols>, Matrix<float, Rows, Cols>>>
{
public:
    typedef typename CompMatFuncBase::IRet IRet;
    typedef typename CompMatFuncBase::IArgs IArgs;

protected:
    IRet doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        IRet ret;

        for (int col = 0; col < Cols; ++col)
        {
            for (int row = 0; row < Rows; ++row)
                ret[col][row] = this->doGetScalarFunc().apply(ctx, iargs.a[col][row], iargs.b[col][row]);
        }

        return ret;
    }
};

template <typename F, int Rows, int Cols>
class CompMatFunc : public CompMatFuncBase<Rows, Cols>
{
protected:
    const typename CompMatFunc::ScalarFunc &doGetScalarFunc(void) const
    {
        return instance<F>();
    }
};

class ScalarMatrixCompMult : public Mul
{
public:
    string getName(void) const
    {
        return "matrixCompMult";
    }

    void doPrint(ostream &os, const BaseArgExprs &args) const
    {
        Func<Sig>::doPrint(os, args);
    }
};

template <int Rows, int Cols>
class MatrixCompMult : public CompMatFunc<ScalarMatrixCompMult, Rows, Cols>
{
};

template <int Rows, int Cols>
class ScalarMatFuncBase
    : public CompWiseFunc<float, Signature<Matrix<float, Rows, Cols>, Matrix<float, Rows, Cols>, float>>
{
public:
    typedef typename ScalarMatFuncBase::IRet IRet;
    typedef typename ScalarMatFuncBase::IArgs IArgs;

protected:
    IRet doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        IRet ret;

        for (int col = 0; col < Cols; ++col)
        {
            for (int row = 0; row < Rows; ++row)
                ret[col][row] = this->doGetScalarFunc().apply(ctx, iargs.a[col][row], iargs.b);
        }

        return ret;
    }
};

template <typename F, int Rows, int Cols>
class ScalarMatFunc : public ScalarMatFuncBase<Rows, Cols>
{
protected:
    const typename ScalarMatFunc::ScalarFunc &doGetScalarFunc(void) const
    {
        return instance<F>();
    }
};

template <typename T, int Size>
struct GenXType;

template <typename T>
struct GenXType<T, 1>
{
    static ExprP<T> genXType(const ExprP<T> &x)
    {
        return x;
    }
};

template <typename T>
struct GenXType<T, 2>
{
    static ExprP<Vector<T, 2>> genXType(const ExprP<T> &x)
    {
        return app<GenVec<T, 2>>(x, x);
    }
};

template <typename T>
struct GenXType<T, 3>
{
    static ExprP<Vector<T, 3>> genXType(const ExprP<T> &x)
    {
        return app<GenVec<T, 3>>(x, x, x);
    }
};

template <typename T>
struct GenXType<T, 4>
{
    static ExprP<Vector<T, 4>> genXType(const ExprP<T> &x)
    {
        return app<GenVec<T, 4>>(x, x, x, x);
    }
};

//! Returns an expression of vector of size `Size` (or scalar if Size == 1),
//! with each element initialized with the expression `x`.
template <typename T, int Size>
ExprP<typename ContainerOf<T, Size>::Container> genXType(const ExprP<T> &x)
{
    return GenXType<T, Size>::genXType(x);
}

typedef GenVec<float, 2> FloatVec2;
DEFINE_CONSTRUCTOR2(FloatVec2, Vec2, vec2, float, float)

typedef GenVec<float, 3> FloatVec3;
DEFINE_CONSTRUCTOR3(FloatVec3, Vec3, vec3, float, float, float)

typedef GenVec<float, 4> FloatVec4;
DEFINE_CONSTRUCTOR4(FloatVec4, Vec4, vec4, float, float, float, float)

template <int Size>
class Dot : public DerivedFunc<Signature<float, Vector<float, Size>, Vector<float, Size>>>
{
public:
    typedef typename Dot::ArgExprs ArgExprs;

    string getName(void) const
    {
        return "dot";
    }

protected:
    ExprP<float> doExpand(ExpandContext &, const ArgExprs &args) const
    {
        ExprP<float> op[Size];
        // Precompute all products.
        for (int ndx = 0; ndx < Size; ++ndx)
            op[ndx] = args.a[ndx] * args.b[ndx];

        int idx[Size];
        //Prepare an array of indices.
        for (int ndx = 0; ndx < Size; ++ndx)
            idx[ndx] = ndx;

        ExprP<float> res = op[0];
        // Compute the first dot alternative: SUM(a[i]*b[i]), i = 0 .. Size-1
        for (int ndx = 1; ndx < Size; ++ndx)
            res = res + op[ndx];

        // Generate all permutations of indices and
        // using a permutation compute a dot alternative.
        // Generates all possible variants fo summation of products in the dot product expansion expression.
        do
        {
            ExprP<float> alt = constant(0.0f);
            for (int ndx = 0; ndx < Size; ++ndx)
                alt = alt + op[idx[ndx]];
            res = alternatives(res, alt);
        } while (std::next_permutation(idx, idx + Size));

        return res;
    }
};

template <>
class Dot<1> : public DerivedFunc<Signature<float, float, float>>
{
public:
    string getName(void) const
    {
        return "dot";
    }

    ExprP<float> doExpand(ExpandContext &, const ArgExprs &args) const
    {
        return args.a * args.b;
    }
};

template <int Size>
ExprP<float> dot(const ExprP<Vector<float, Size>> &x, const ExprP<Vector<float, Size>> &y)
{
    return app<Dot<Size>>(x, y);
}

ExprP<float> dot(const ExprP<float> &x, const ExprP<float> &y)
{
    return app<Dot<1>>(x, y);
}

template <int Size>
class Length : public DerivedFunc<Signature<float, typename ContainerOf<float, Size>::Container>>
{
public:
    typedef typename Length::ArgExprs ArgExprs;

    string getName(void) const
    {
        return "length";
    }

protected:
    ExprP<float> doExpand(ExpandContext &, const ArgExprs &args) const
    {
        return sqrt(dot(args.a, args.a));
    }
};

template <int Size>
ExprP<float> length(const ExprP<typename ContainerOf<float, Size>::Container> &x)
{
    return app<Length<Size>>(x);
}

template <int Size>
class Distance
    : public DerivedFunc<
          Signature<float, typename ContainerOf<float, Size>::Container, typename ContainerOf<float, Size>::Container>>
{
public:
    typedef typename Distance::Ret Ret;
    typedef typename Distance::ArgExprs ArgExprs;

    string getName(void) const
    {
        return "distance";
    }

protected:
    ExprP<Ret> doExpand(ExpandContext &, const ArgExprs &args) const
    {
        return length<Size>(args.a - args.b);
    }
};

// cross

class Cross : public DerivedFunc<Signature<Vec3, Vec3, Vec3>>
{
public:
    string getName(void) const
    {
        return "cross";
    }

protected:
    ExprP<Vec3> doExpand(ExpandContext &, const ArgExprs &x) const
    {
        return vec3(x.a[1] * x.b[2] - x.b[1] * x.a[2], x.a[2] * x.b[0] - x.b[2] * x.a[0],
                    x.a[0] * x.b[1] - x.b[0] * x.a[1]);
    }
};

DEFINE_CONSTRUCTOR2(Cross, Vec3, cross, Vec3, Vec3)

template <int Size>
class Normalize
    : public DerivedFunc<
          Signature<typename ContainerOf<float, Size>::Container, typename ContainerOf<float, Size>::Container>>
{
public:
    typedef typename Normalize::Ret Ret;
    typedef typename Normalize::ArgExprs ArgExprs;

    string getName(void) const
    {
        return "normalize";
    }

protected:
    ExprP<Ret> doExpand(ExpandContext &, const ArgExprs &args) const
    {
        return args.a / length<Size>(args.a);
    }
};

template <int Size>
class FaceForward
    : public DerivedFunc<
          Signature<typename ContainerOf<float, Size>::Container, typename ContainerOf<float, Size>::Container,
                    typename ContainerOf<float, Size>::Container, typename ContainerOf<float, Size>::Container>>
{
public:
    typedef typename FaceForward::Ret Ret;
    typedef typename FaceForward::ArgExprs ArgExprs;

    string getName(void) const
    {
        return "faceforward";
    }

protected:
    ExprP<Ret> doExpand(ExpandContext &, const ArgExprs &args) const
    {
        return cond(dot(args.c, args.b) < constant(0.0f), args.a, -args.a);
    }
};

template <int Size, typename Ret, typename Arg0, typename Arg1>
struct ApplyReflect
{
    static ExprP<Ret> apply(ExpandContext &ctx, const ExprP<Arg0> &i, const ExprP<Arg1> &n)
    {
        const ExprP<float> dotNI = bindExpression("dotNI", ctx, dot(n, i));

        return i - alternatives((n * dotNI) * constant(2.0f), n * (dotNI * constant(2.0f)));
    }
};

template <typename Ret, typename Arg0, typename Arg1>
struct ApplyReflect<1, Ret, Arg0, Arg1>
{
    static ExprP<Ret> apply(ExpandContext &, const ExprP<Arg0> &i, const ExprP<Arg1> &n)
    {
        return i - alternatives(alternatives((n * (n * i)) * constant(2.0f), n * ((n * i) * constant(2.0f))),
                                (n * n) * (i * constant(2.0f)));
    }
};

template <int Size>
class Reflect
    : public DerivedFunc<
          Signature<typename ContainerOf<float, Size>::Container, typename ContainerOf<float, Size>::Container,
                    typename ContainerOf<float, Size>::Container>>
{
public:
    typedef typename Reflect::Ret Ret;
    typedef typename Reflect::Arg0 Arg0;
    typedef typename Reflect::Arg1 Arg1;
    typedef typename Reflect::ArgExprs ArgExprs;

    string getName(void) const
    {
        return "reflect";
    }

protected:
    ExprP<Ret> doExpand(ExpandContext &ctx, const ArgExprs &args) const
    {
        const ExprP<Arg0> &i = args.a;
        const ExprP<Arg1> &n = args.b;

        return ApplyReflect<Size, Ret, Arg0, Arg1>::apply(ctx, i, n);
    }
};

template <int Size>
class Refract
    : public DerivedFunc<
          Signature<typename ContainerOf<float, Size>::Container, typename ContainerOf<float, Size>::Container,
                    typename ContainerOf<float, Size>::Container, float>>
{
public:
    typedef typename Refract::Ret Ret;
    typedef typename Refract::Arg0 Arg0;
    typedef typename Refract::Arg1 Arg1;
    typedef typename Refract::ArgExprs ArgExprs;

    string getName(void) const
    {
        return "refract";
    }

protected:
    ExprP<Ret> doExpand(ExpandContext &ctx, const ArgExprs &args) const
    {
        const ExprP<Arg0> &i     = args.a;
        const ExprP<Arg1> &n     = args.b;
        const ExprP<float> &eta  = args.c;
        const ExprP<float> dotNI = bindExpression("dotNI", ctx, dot(n, i));
        const ExprP<float> k1 =
            bindExpression("k1", ctx, constant(1.0f) - eta * eta * (constant(1.0f) - dotNI * dotNI));

        const ExprP<float> k2 =
            bindExpression("k2", ctx, (((dotNI * (-dotNI)) + constant(1.0f)) * eta) * (-eta) + constant(1.0f));
        const ExprP<float> k = bindExpression("k", ctx, alternatives(k1, k2));

        return cond(k < constant(0.0f), genXType<float, Size>(constant(0.0f)), i * eta - n * (eta * dotNI + sqrt(k)));
    }
};

class PreciseFunc1 : public CFloatFunc1
{
public:
    PreciseFunc1(const string &name, DoubleFunc1 &func) : CFloatFunc1(name, func)
    {
    }

protected:
    double precision(const EvalContext &, double, double) const
    {
        return 0.0;
    }
};

class Abs : public PreciseFunc1
{
public:
    Abs(void) : PreciseFunc1("abs", deAbs)
    {
    }
};

class Sign : public PreciseFunc1
{
public:
    Sign(void) : PreciseFunc1("sign", deSign)
    {
    }
};

class Floor : public PreciseFunc1
{
public:
    Floor(void) : PreciseFunc1("floor", deFloor)
    {
    }
};

class Trunc : public PreciseFunc1
{
public:
    Trunc(void) : PreciseFunc1("trunc", deTrunc)
    {
    }
};

class Round : public FloatFunc1
{
public:
    string getName(void) const
    {
        return "round";
    }

protected:
    Interval applyPoint(const EvalContext &, double x) const
    {
        double truncated   = 0.0;
        const double fract = deModf(x, &truncated);
        Interval ret;

        if (fabs(fract) <= 0.5)
            ret |= truncated;
        if (fabs(fract) >= 0.5)
            ret |= truncated + deSign(fract);

        return ret;
    }

    double precision(const EvalContext &, double, double) const
    {
        return 0.0;
    }
};

class RoundEven : public PreciseFunc1
{
public:
    RoundEven(void) : PreciseFunc1("roundEven", deRoundEven)
    {
    }
};

class Ceil : public PreciseFunc1
{
public:
    Ceil(void) : PreciseFunc1("ceil", deCeil)
    {
    }
};

DEFINE_DERIVED_FLOAT1(Fract, fract, x, x - app<Floor>(x))

class PreciseFunc2 : public CFloatFunc2
{
public:
    PreciseFunc2(const string &name, DoubleFunc2 &func) : CFloatFunc2(name, func)
    {
    }

protected:
    double precision(const EvalContext &, double, double, double) const
    {
        return 0.0;
    }
};

DEFINE_DERIVED_FLOAT2(Mod, mod, x, y, x - y * app<Floor>(x / y))

class Modf : public PrimitiveFunc<Signature<float, float, float>>
{
public:
    string getName(void) const
    {
        return "modf";
    }

protected:
    IRet doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        Interval fracIV;
        Interval &wholeIV = const_cast<Interval &>(iargs.b);
        double intPart    = 0;

        TCU_INTERVAL_APPLY_MONOTONE1(fracIV, x, iargs.a, frac, frac = deModf(x, &intPart));
        TCU_INTERVAL_APPLY_MONOTONE1(wholeIV, x, iargs.a, whole, deModf(x, &intPart); whole = intPart);

        if (!iargs.a.isFinite(ctx.format.getMaxValue()))
        {
            // Behavior on modf(Inf) not well-defined, allow anything as a fractional part
            // See Khronos bug 13907
            fracIV |= TCU_NAN;
        }

        return fracIV;
    }

    int getOutParamIndex(void) const
    {
        return 1;
    }
};

int compare(const EvalContext &ctx, double x, double y)
{
    if (ctx.format.hasSubnormal() != tcu::YES)
    {
        const int minExp           = ctx.format.getMinExp();
        const int fractionBits     = ctx.format.getFractionBits();
        const double minQuantum    = deLdExp(1.0f, minExp - fractionBits);
        const double minNormalized = deLdExp(1.0f, minExp);
        const double maxSubnormal  = minNormalized - minQuantum;
        const double minSubnormal  = -maxSubnormal;

        if (minSubnormal <= x && x <= maxSubnormal && minSubnormal <= y && y <= maxSubnormal)
            return 0;
    }

    if (x < y)
        return -1;
    if (y < x)
        return 1;
    return 0;
}

class MinMaxFunc : public FloatFunc2
{
public:
    MinMaxFunc(const string &name, int sign) : m_name(name), m_sign(sign)
    {
    }

    string getName(void) const
    {
        return m_name;
    }

protected:
    Interval applyPoint(const EvalContext &ctx, double x, double y) const
    {
        const int cmp = compare(ctx, x, y) * m_sign;

        if (cmp > 0)
            return x;
        if (cmp < 0)
            return y;

        // An implementation without subnormals may not be able to distinguish
        // between x and y even when they're not equal in host arithmetic.
        return Interval(x, y);
    }

    double precision(const EvalContext &, double, double, double) const
    {
        return 0.0;
    }

    const string m_name;
    const int m_sign;
};

class Min : public MinMaxFunc
{
public:
    Min(void) : MinMaxFunc("min", -1)
    {
    }
};
class Max : public MinMaxFunc
{
public:
    Max(void) : MinMaxFunc("max", 1)
    {
    }
};

class Clamp : public FloatFunc3
{
public:
    string getName(void) const
    {
        return "clamp";
    }

protected:
    Interval applyPoint(const EvalContext &ctx, double x, double minVal, double maxVal) const
    {
        if (minVal > maxVal)
            return TCU_NAN;

        const int cmpMin    = compare(ctx, x, minVal);
        const int cmpMax    = compare(ctx, x, maxVal);
        const int cmpMinMax = compare(ctx, minVal, maxVal);

        if (cmpMin < 0)
        {
            if (cmpMinMax < 0)
                return minVal;
            else
                return Interval(minVal, maxVal);
        }
        if (cmpMax > 0)
        {
            if (cmpMinMax < 0)
                return maxVal;
            else
                return Interval(minVal, maxVal);
        }

        Interval result = x;
        if (cmpMin == 0)
            result |= minVal;
        if (cmpMax == 0)
            result |= maxVal;
        return result;
    }

    double precision(const EvalContext &, double, double, double minVal, double maxVal) const
    {
        return minVal > maxVal ? TCU_NAN : 0.0;
    }
};

ExprP<float> clamp(const ExprP<float> &x, const ExprP<float> &minVal, const ExprP<float> &maxVal)
{
    return app<Clamp>(x, minVal, maxVal);
}

DEFINE_DERIVED_FLOAT3(Mix, mix, x, y, a, alternatives((x * (constant(1.0f) - a)) + y * a, x + (y - x) * a))

static double step(double edge, double x)
{
    return x < edge ? 0.0 : 1.0;
}

class Step : public PreciseFunc2
{
public:
    Step(void) : PreciseFunc2("step", step)
    {
    }
};

class SmoothStep : public DerivedFunc<Signature<float, float, float, float>>
{
public:
    string getName(void) const
    {
        return "smoothstep";
    }

protected:
    ExprP<Ret> doExpand(ExpandContext &ctx, const ArgExprs &args) const
    {
        const ExprP<float> &edge0 = args.a;
        const ExprP<float> &edge1 = args.b;
        const ExprP<float> &x     = args.c;
        const ExprP<float> tExpr  = clamp((x - edge0) / (edge1 - edge0), constant(0.0f), constant(1.0f));
        const ExprP<float> t      = bindExpression("t", ctx, tExpr);

        return (t * t * (constant(3.0f) - constant(2.0f) * t));
    }
};

class FrExp : public PrimitiveFunc<Signature<float, float, int>>
{
public:
    string getName(void) const
    {
        return "frexp";
    }

protected:
    IRet doApply(const EvalContext &, const IArgs &iargs) const
    {
        IRet ret;
        const IArg0 &x  = iargs.a;
        IArg1 &exponent = const_cast<IArg1 &>(iargs.b);

        if (x.hasNaN() || x.contains(TCU_INFINITY) || x.contains(-TCU_INFINITY))
        {
            // GLSL (in contrast to IEEE) says that result of applying frexp
            // to infinity is undefined
            ret      = Interval::unbounded() | TCU_NAN;
            exponent = Interval(-deLdExp(1.0, 31), deLdExp(1.0, 31) - 1);
        }
        else if (!x.empty())
        {
            int loExp           = 0;
            const double loFrac = deFrExp(x.lo(), &loExp);
            int hiExp           = 0;
            const double hiFrac = deFrExp(x.hi(), &hiExp);

            if (deSign(loFrac) != deSign(hiFrac))
            {
                exponent = Interval(-TCU_INFINITY, de::max(loExp, hiExp));
                ret      = Interval();
                if (deSign(loFrac) < 0)
                    ret |= Interval(-1.0 + DBL_EPSILON * 0.5, 0.0);
                if (deSign(hiFrac) > 0)
                    ret |= Interval(0.0, 1.0 - DBL_EPSILON * 0.5);
            }
            else
            {
                exponent = Interval(loExp, hiExp);
                if (loExp == hiExp)
                    ret = Interval(loFrac, hiFrac);
                else
                    ret = deSign(loFrac) * Interval(0.5, 1.0 - DBL_EPSILON * 0.5);
            }
        }

        return ret;
    }

    int getOutParamIndex(void) const
    {
        return 1;
    }
};

class LdExp : public PrimitiveFunc<Signature<float, float, int>>
{
public:
    string getName(void) const
    {
        return "ldexp";
    }

protected:
    Interval doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        Interval ret = call<Exp2>(ctx, iargs.b);
        // Khronos bug 11180 consensus: if exp2(exponent) cannot be represented,
        // the result is undefined.

        if (ret.contains(TCU_INFINITY) || ret.contains(-TCU_INFINITY))
            ret |= TCU_NAN;

        return call<Mul>(ctx, iargs.a, ret);
    }
};

template <int Rows, int Columns>
class Transpose : public PrimitiveFunc<Signature<Matrix<float, Rows, Columns>, Matrix<float, Columns, Rows>>>
{
public:
    typedef typename Transpose::IRet IRet;
    typedef typename Transpose::IArgs IArgs;

    string getName(void) const
    {
        return "transpose";
    }

protected:
    IRet doApply(const EvalContext &, const IArgs &iargs) const
    {
        IRet ret;

        for (int rowNdx = 0; rowNdx < Rows; ++rowNdx)
        {
            for (int colNdx = 0; colNdx < Columns; ++colNdx)
                ret(rowNdx, colNdx) = iargs.a(colNdx, rowNdx);
        }

        return ret;
    }
};

template <typename Ret, typename Arg0, typename Arg1>
class MulFunc : public PrimitiveFunc<Signature<Ret, Arg0, Arg1>>
{
public:
    string getName(void) const
    {
        return "mul";
    }

protected:
    void doPrint(ostream &os, const BaseArgExprs &args) const
    {
        os << "(" << *args[0] << " * " << *args[1] << ")";
    }
};

template <int LeftRows, int Middle, int RightCols>
class MatMul : public MulFunc<Matrix<float, LeftRows, RightCols>, Matrix<float, LeftRows, Middle>,
                              Matrix<float, Middle, RightCols>>
{
protected:
    typedef typename MatMul::IRet IRet;
    typedef typename MatMul::IArgs IArgs;
    typedef typename MatMul::IArg0 IArg0;
    typedef typename MatMul::IArg1 IArg1;

    IRet doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        const IArg0 &left  = iargs.a;
        const IArg1 &right = iargs.b;
        IRet ret;

        for (int row = 0; row < LeftRows; ++row)
        {
            for (int col = 0; col < RightCols; ++col)
            {
                Interval element(0.0);

                for (int ndx = 0; ndx < Middle; ++ndx)
                    element = call<Add>(ctx, element, call<Mul>(ctx, left[ndx][row], right[col][ndx]));

                ret[col][row] = element;
            }
        }

        return ret;
    }
};

template <int Rows, int Cols>
class VecMatMul : public MulFunc<Vector<float, Cols>, Vector<float, Rows>, Matrix<float, Rows, Cols>>
{
public:
    typedef typename VecMatMul::IRet IRet;
    typedef typename VecMatMul::IArgs IArgs;
    typedef typename VecMatMul::IArg0 IArg0;
    typedef typename VecMatMul::IArg1 IArg1;

protected:
    IRet doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        const IArg0 &left  = iargs.a;
        const IArg1 &right = iargs.b;
        IRet ret;

        for (int col = 0; col < Cols; ++col)
        {
            Interval element(0.0);

            for (int row = 0; row < Rows; ++row)
                element = call<Add>(ctx, element, call<Mul>(ctx, left[row], right[col][row]));

            ret[col] = element;
        }

        return ret;
    }
};

template <int Rows, int Cols>
class MatVecMul : public MulFunc<Vector<float, Rows>, Matrix<float, Rows, Cols>, Vector<float, Cols>>
{
public:
    typedef typename MatVecMul::IRet IRet;
    typedef typename MatVecMul::IArgs IArgs;
    typedef typename MatVecMul::IArg0 IArg0;
    typedef typename MatVecMul::IArg1 IArg1;

protected:
    IRet doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        const IArg0 &left  = iargs.a;
        const IArg1 &right = iargs.b;

        return call<VecMatMul<Cols, Rows>>(ctx, right, call<Transpose<Rows, Cols>>(ctx, left));
    }
};

template <int Rows, int Cols>
class OuterProduct
    : public PrimitiveFunc<Signature<Matrix<float, Rows, Cols>, Vector<float, Rows>, Vector<float, Cols>>>
{
public:
    typedef typename OuterProduct::IRet IRet;
    typedef typename OuterProduct::IArgs IArgs;

    string getName(void) const
    {
        return "outerProduct";
    }

protected:
    IRet doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        IRet ret;

        for (int row = 0; row < Rows; ++row)
        {
            for (int col = 0; col < Cols; ++col)
                ret[col][row] = call<Mul>(ctx, iargs.a[row], iargs.b[col]);
        }

        return ret;
    }
};

template <int Rows, int Cols>
ExprP<Matrix<float, Rows, Cols>> outerProduct(const ExprP<Vector<float, Rows>> &left,
                                              const ExprP<Vector<float, Cols>> &right)
{
    return app<OuterProduct<Rows, Cols>>(left, right);
}

template <int Size>
class DeterminantBase : public DerivedFunc<Signature<float, Matrix<float, Size, Size>>>
{
public:
    string getName(void) const
    {
        return "determinant";
    }
};

template <int Size>
class Determinant;

template <int Size>
ExprP<float> determinant(ExprP<Matrix<float, Size, Size>> mat)
{
    return app<Determinant<Size>>(mat);
}

template <>
class Determinant<2> : public DeterminantBase<2>
{
protected:
    ExprP<Ret> doExpand(ExpandContext &, const ArgExprs &args) const
    {
        ExprP<Mat2> mat = args.a;

        return mat[0][0] * mat[1][1] - mat[1][0] * mat[0][1];
    }
};

template <>
class Determinant<3> : public DeterminantBase<3>
{
protected:
    ExprP<Ret> doExpand(ExpandContext &, const ArgExprs &args) const
    {
        ExprP<Mat3> mat = args.a;

        return (mat[0][0] * (mat[1][1] * mat[2][2] - mat[1][2] * mat[2][1]) +
                mat[0][1] * (mat[1][2] * mat[2][0] - mat[1][0] * mat[2][2]) +
                mat[0][2] * (mat[1][0] * mat[2][1] - mat[1][1] * mat[2][0]));
    }
};

template <>
class Determinant<4> : public DeterminantBase<4>
{
protected:
    ExprP<Ret> doExpand(ExpandContext &ctx, const ArgExprs &args) const
    {
        ExprP<Mat4> mat = args.a;
        ExprP<Mat3> minors[4];

        for (int ndx = 0; ndx < 4; ++ndx)
        {
            ExprP<Vec4> minorColumns[3];
            ExprP<Vec3> columns[3];

            for (int col = 0; col < 3; ++col)
                minorColumns[col] = mat[col < ndx ? col : col + 1];

            for (int col = 0; col < 3; ++col)
                columns[col] = vec3(minorColumns[0][col + 1], minorColumns[1][col + 1], minorColumns[2][col + 1]);

            minors[ndx] = bindExpression("minor", ctx, mat3(columns[0], columns[1], columns[2]));
        }

        return (mat[0][0] * determinant(minors[0]) - mat[1][0] * determinant(minors[1]) +
                mat[2][0] * determinant(minors[2]) - mat[3][0] * determinant(minors[3]));
    }
};

template <int Size>
class Inverse;

template <int Size>
ExprP<Matrix<float, Size, Size>> inverse(ExprP<Matrix<float, Size, Size>> mat)
{
    return app<Inverse<Size>>(mat);
}

template <>
class Inverse<2> : public DerivedFunc<Signature<Mat2, Mat2>>
{
public:
    string getName(void) const
    {
        return "inverse";
    }

protected:
    ExprP<Ret> doExpand(ExpandContext &ctx, const ArgExprs &args) const
    {
        ExprP<Mat2> mat  = args.a;
        ExprP<float> det = bindExpression("det", ctx, determinant(mat));

        return mat2(vec2(mat[1][1] / det, -mat[0][1] / det), vec2(-mat[1][0] / det, mat[0][0] / det));
    }
};

template <>
class Inverse<3> : public DerivedFunc<Signature<Mat3, Mat3>>
{
public:
    string getName(void) const
    {
        return "inverse";
    }

protected:
    ExprP<Ret> doExpand(ExpandContext &ctx, const ArgExprs &args) const
    {
        ExprP<Mat3> mat = args.a;
        ExprP<Mat2> invA =
            bindExpression("invA", ctx, inverse(mat2(vec2(mat[0][0], mat[0][1]), vec2(mat[1][0], mat[1][1]))));

        ExprP<Vec2> matB  = bindExpression("matB", ctx, vec2(mat[2][0], mat[2][1]));
        ExprP<Vec2> matC  = bindExpression("matC", ctx, vec2(mat[0][2], mat[1][2]));
        ExprP<float> matD = bindExpression("matD", ctx, mat[2][2]);

        ExprP<float> schur = bindExpression("schur", ctx, constant(1.0f) / (matD - dot(matC * invA, matB)));

        ExprP<Vec2> t1     = invA * matB;
        ExprP<Vec2> t2     = t1 * schur;
        ExprP<Mat2> t3     = outerProduct(t2, matC);
        ExprP<Mat2> t4     = t3 * invA;
        ExprP<Mat2> t5     = invA + t4;
        ExprP<Mat2> blockA = bindExpression("blockA", ctx, t5);
        ExprP<Vec2> blockB = bindExpression("blockB", ctx, (invA * matB) * -schur);
        ExprP<Vec2> blockC = bindExpression("blockC", ctx, (matC * invA) * -schur);

        return mat3(vec3(blockA[0][0], blockA[0][1], blockC[0]), vec3(blockA[1][0], blockA[1][1], blockC[1]),
                    vec3(blockB[0], blockB[1], schur));
    }
};

template <>
class Inverse<4> : public DerivedFunc<Signature<Mat4, Mat4>>
{
public:
    string getName(void) const
    {
        return "inverse";
    }

protected:
    ExprP<Ret> doExpand(ExpandContext &ctx, const ArgExprs &args) const
    {
        ExprP<Mat4> mat = args.a;
        ExprP<Mat2> invA =
            bindExpression("invA", ctx, inverse(mat2(vec2(mat[0][0], mat[0][1]), vec2(mat[1][0], mat[1][1]))));
        ExprP<Mat2> matB   = bindExpression("matB", ctx, mat2(vec2(mat[2][0], mat[2][1]), vec2(mat[3][0], mat[3][1])));
        ExprP<Mat2> matC   = bindExpression("matC", ctx, mat2(vec2(mat[0][2], mat[0][3]), vec2(mat[1][2], mat[1][3])));
        ExprP<Mat2> matD   = bindExpression("matD", ctx, mat2(vec2(mat[2][2], mat[2][3]), vec2(mat[3][2], mat[3][3])));
        ExprP<Mat2> schur  = bindExpression("schur", ctx, inverse(matD + -(matC * invA * matB)));
        ExprP<Mat2> blockA = bindExpression("blockA", ctx, invA + (invA * matB * schur * matC * invA));
        ExprP<Mat2> blockB = bindExpression("blockB", ctx, (-invA) * matB * schur);
        ExprP<Mat2> blockC = bindExpression("blockC", ctx, (-schur) * matC * invA);

        return mat4(vec4(blockA[0][0], blockA[0][1], blockC[0][0], blockC[0][1]),
                    vec4(blockA[1][0], blockA[1][1], blockC[1][0], blockC[1][1]),
                    vec4(blockB[0][0], blockB[0][1], schur[0][0], schur[0][1]),
                    vec4(blockB[1][0], blockB[1][1], schur[1][0], schur[1][1]));
    }
};

class Fma : public DerivedFunc<Signature<float, float, float, float>>
{
public:
    string getName(void) const
    {
        return "fma";
    }

    string getRequiredExtension(const RenderContext &context) const
    {
        return (glu::contextSupports(context.getType(), glu::ApiType::core(4, 5))) ? "" : "GL_EXT_gpu_shader5";
    }

protected:
    ExprP<float> doExpand(ExpandContext &, const ArgExprs &x) const
    {
        return x.a * x.b + x.c;
    }
};

} // namespace Functions

using namespace Functions;

template <typename T>
ExprP<typename T::Element> ContainerExprPBase<T>::operator[](int i) const
{
    return Functions::getComponent(exprP<T>(*this), i);
}

ExprP<float> operator+(const ExprP<float> &arg0, const ExprP<float> &arg1)
{
    return app<Add>(arg0, arg1);
}

ExprP<float> operator-(const ExprP<float> &arg0, const ExprP<float> &arg1)
{
    return app<Sub>(arg0, arg1);
}

ExprP<float> operator-(const ExprP<float> &arg0)
{
    return app<Negate>(arg0);
}

ExprP<float> operator*(const ExprP<float> &arg0, const ExprP<float> &arg1)
{
    return app<Mul>(arg0, arg1);
}

ExprP<float> operator/(const ExprP<float> &arg0, const ExprP<float> &arg1)
{
    return app<Div>(arg0, arg1);
}

template <typename Sig_, int Size>
class GenFunc : public PrimitiveFunc<Signature<typename ContainerOf<typename Sig_::Ret, Size>::Container,
                                               typename ContainerOf<typename Sig_::Arg0, Size>::Container,
                                               typename ContainerOf<typename Sig_::Arg1, Size>::Container,
                                               typename ContainerOf<typename Sig_::Arg2, Size>::Container,
                                               typename ContainerOf<typename Sig_::Arg3, Size>::Container>>
{
public:
    typedef typename GenFunc::IArgs IArgs;
    typedef typename GenFunc::IRet IRet;

    GenFunc(const Func<Sig_> &scalarFunc) : m_func(scalarFunc)
    {
    }

    string getName(void) const
    {
        return m_func.getName();
    }

    int getOutParamIndex(void) const
    {
        return m_func.getOutParamIndex();
    }

    string getRequiredExtension(const RenderContext &context) const
    {
        return m_func.getRequiredExtension(context);
    }

protected:
    void doPrint(ostream &os, const BaseArgExprs &args) const
    {
        m_func.print(os, args);
    }

    IRet doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        IRet ret;

        for (int ndx = 0; ndx < Size; ++ndx)
        {
            ret[ndx] = m_func.apply(ctx, iargs.a[ndx], iargs.b[ndx], iargs.c[ndx], iargs.d[ndx]);
        }

        return ret;
    }

    void doGetUsedFuncs(FuncSet &dst) const
    {
        m_func.getUsedFuncs(dst);
    }

    const Func<Sig_> &m_func;
};

template <typename F, int Size>
class VectorizedFunc : public GenFunc<typename F::Sig, Size>
{
public:
    VectorizedFunc(void) : GenFunc<typename F::Sig, Size>(instance<F>())
    {
    }
};

template <typename Sig_, int Size>
class FixedGenFunc
    : public PrimitiveFunc<Signature<typename ContainerOf<typename Sig_::Ret, Size>::Container,
                                     typename ContainerOf<typename Sig_::Arg0, Size>::Container, typename Sig_::Arg1,
                                     typename ContainerOf<typename Sig_::Arg2, Size>::Container,
                                     typename ContainerOf<typename Sig_::Arg3, Size>::Container>>
{
public:
    typedef typename FixedGenFunc::IArgs IArgs;
    typedef typename FixedGenFunc::IRet IRet;

    string getName(void) const
    {
        return this->doGetScalarFunc().getName();
    }

protected:
    void doPrint(ostream &os, const BaseArgExprs &args) const
    {
        this->doGetScalarFunc().print(os, args);
    }

    IRet doApply(const EvalContext &ctx, const IArgs &iargs) const
    {
        IRet ret;
        const Func<Sig_> &func = this->doGetScalarFunc();

        for (int ndx = 0; ndx < Size; ++ndx)
            ret[ndx] = func.apply(ctx, iargs.a[ndx], iargs.b, iargs.c[ndx], iargs.d[ndx]);

        return ret;
    }

    virtual const Func<Sig_> &doGetScalarFunc(void) const = 0;
};

template <typename F, int Size>
class FixedVecFunc : public FixedGenFunc<typename F::Sig, Size>
{
protected:
    const Func<typename F::Sig> &doGetScalarFunc(void) const
    {
        return instance<F>();
    }
};

template <typename Sig>
struct GenFuncs
{
    GenFuncs(const Func<Sig> &func_, const GenFunc<Sig, 2> &func2_, const GenFunc<Sig, 3> &func3_,
             const GenFunc<Sig, 4> &func4_)
        : func(func_)
        , func2(func2_)
        , func3(func3_)
        , func4(func4_)
    {
    }

    const Func<Sig> &func;
    const GenFunc<Sig, 2> &func2;
    const GenFunc<Sig, 3> &func3;
    const GenFunc<Sig, 4> &func4;
};

template <typename F>
GenFuncs<typename F::Sig> makeVectorizedFuncs(void)
{
    return GenFuncs<typename F::Sig>(instance<F>(), instance<VectorizedFunc<F, 2>>(), instance<VectorizedFunc<F, 3>>(),
                                     instance<VectorizedFunc<F, 4>>());
}

template <int Size>
ExprP<Vector<float, Size>> operator*(const ExprP<Vector<float, Size>> &arg0, const ExprP<Vector<float, Size>> &arg1)
{
    return app<VectorizedFunc<Mul, Size>>(arg0, arg1);
}

template <int Size>
ExprP<Vector<float, Size>> operator*(const ExprP<Vector<float, Size>> &arg0, const ExprP<float> &arg1)
{
    return app<FixedVecFunc<Mul, Size>>(arg0, arg1);
}

template <int Size>
ExprP<Vector<float, Size>> operator/(const ExprP<Vector<float, Size>> &arg0, const ExprP<float> &arg1)
{
    return app<FixedVecFunc<Div, Size>>(arg0, arg1);
}

template <int Size>
ExprP<Vector<float, Size>> operator-(const ExprP<Vector<float, Size>> &arg0)
{
    return app<VectorizedFunc<Negate, Size>>(arg0);
}

template <int Size>
ExprP<Vector<float, Size>> operator-(const ExprP<Vector<float, Size>> &arg0, const ExprP<Vector<float, Size>> &arg1)
{
    return app<VectorizedFunc<Sub, Size>>(arg0, arg1);
}

template <int LeftRows, int Middle, int RightCols>
ExprP<Matrix<float, LeftRows, RightCols>> operator*(const ExprP<Matrix<float, LeftRows, Middle>> &left,
                                                    const ExprP<Matrix<float, Middle, RightCols>> &right)
{
    return app<MatMul<LeftRows, Middle, RightCols>>(left, right);
}

template <int Rows, int Cols>
ExprP<Vector<float, Rows>> operator*(const ExprP<Vector<float, Cols>> &left,
                                     const ExprP<Matrix<float, Rows, Cols>> &right)
{
    return app<VecMatMul<Rows, Cols>>(left, right);
}

template <int Rows, int Cols>
ExprP<Vector<float, Cols>> operator*(const ExprP<Matrix<float, Rows, Cols>> &left,
                                     const ExprP<Vector<float, Rows>> &right)
{
    return app<MatVecMul<Rows, Cols>>(left, right);
}

template <int Rows, int Cols>
ExprP<Matrix<float, Rows, Cols>> operator*(const ExprP<Matrix<float, Rows, Cols>> &left, const ExprP<float> &right)
{
    return app<ScalarMatFunc<Mul, Rows, Cols>>(left, right);
}

template <int Rows, int Cols>
ExprP<Matrix<float, Rows, Cols>> operator+(const ExprP<Matrix<float, Rows, Cols>> &left,
                                           const ExprP<Matrix<float, Rows, Cols>> &right)
{
    return app<CompMatFunc<Add, Rows, Cols>>(left, right);
}

template <int Rows, int Cols>
ExprP<Matrix<float, Rows, Cols>> operator-(const ExprP<Matrix<float, Rows, Cols>> &mat)
{
    return app<MatNeg<Rows, Cols>>(mat);
}

template <typename T>
class Sampling
{
public:
    virtual void genFixeds(const FloatFormat &, vector<T> &) const
    {
    }
    virtual T genRandom(const FloatFormat &, Precision, Random &) const
    {
        return T();
    }
    virtual double getWeight(void) const
    {
        return 0.0;
    }
};

template <>
class DefaultSampling<Void> : public Sampling<Void>
{
public:
    void genFixeds(const FloatFormat &, vector<Void> &dst) const
    {
        dst.push_back(Void());
    }
};

template <>
class DefaultSampling<bool> : public Sampling<bool>
{
public:
    void genFixeds(const FloatFormat &, vector<bool> &dst) const
    {
        dst.push_back(true);
        dst.push_back(false);
    }
};

template <>
class DefaultSampling<int> : public Sampling<int>
{
public:
    int genRandom(const FloatFormat &, Precision prec, Random &rnd) const
    {
        const int exp  = rnd.getInt(0, getNumBits(prec) - 2);
        const int sign = rnd.getBool() ? -1 : 1;

        return sign * rnd.getInt(0, (int32_t)1 << exp);
    }

    void genFixeds(const FloatFormat &, vector<int> &dst) const
    {
        dst.push_back(0);
        dst.push_back(-1);
        dst.push_back(1);
    }
    double getWeight(void) const
    {
        return 1.0;
    }

private:
    static inline int getNumBits(Precision prec)
    {
        switch (prec)
        {
        case glu::PRECISION_LOWP:
            return 8;
        case glu::PRECISION_MEDIUMP:
            return 16;
        case glu::PRECISION_HIGHP:
            return 32;
        default:
            DE_ASSERT(false);
            return 0;
        }
    }
};

template <>
class DefaultSampling<float> : public Sampling<float>
{
public:
    float genRandom(const FloatFormat &format, Precision prec, Random &rnd) const;
    void genFixeds(const FloatFormat &format, vector<float> &dst) const;
    double getWeight(void) const
    {
        return 1.0;
    }
};

//! Generate a random float from a reasonable general-purpose distribution.
float DefaultSampling<float>::genRandom(const FloatFormat &format, Precision, Random &rnd) const
{
    const int minExp         = format.getMinExp();
    const int maxExp         = format.getMaxExp();
    const bool haveSubnormal = format.hasSubnormal() != tcu::NO;

    // Choose exponent so that the cumulative distribution is cubic.
    // This makes the probability distribution quadratic, with the peak centered on zero.
    const double minRoot   = deCbrt(minExp - 0.5 - (haveSubnormal ? 1.0 : 0.0));
    const double maxRoot   = deCbrt(maxExp + 0.5);
    const int fractionBits = format.getFractionBits();
    const int exp          = int(deRoundEven(dePow(rnd.getDouble(minRoot, maxRoot), 3.0)));
    float base             = 0.0f; // integral power of two
    float quantum          = 0.0f; // smallest representable difference in the binade
    float significand      = 0.0f; // Significand.

    DE_ASSERT(fractionBits < std::numeric_limits<float>::digits);

    // Generate some occasional special numbers
    switch (rnd.getInt(0, 64))
    {
    case 0:
        return 0;
    case 1:
        return TCU_INFINITY;
    case 2:
        return -TCU_INFINITY;
    case 3:
        return TCU_NAN;
    default:
        break;
    }

    if (exp >= minExp)
    {
        // Normal number
        base    = deFloatLdExp(1.0f, exp);
        quantum = deFloatLdExp(1.0f, exp - fractionBits);
    }
    else
    {
        // Subnormal
        base    = 0.0f;
        quantum = deFloatLdExp(1.0f, minExp - fractionBits);
    }

    switch (rnd.getInt(0, 16))
    {
    case 0: // The highest number in this binade, significand is all bits one.
        significand = base - quantum;
        break;
    case 1: // Significand is one.
        significand = quantum;
        break;
    case 2: // Significand is zero.
        significand = 0.0;
        break;
    default: // Random (evenly distributed) significand.
    {
        uint64_t intFraction = rnd.getUint64() & ((1ull << fractionBits) - 1);
        significand          = float(intFraction) * quantum;
    }
    }

    // Produce positive numbers more often than negative.
    return (rnd.getInt(0, 3) == 0 ? -1.0f : 1.0f) * (base + significand);
}

//! Generate a standard set of floats that should always be tested.
void DefaultSampling<float>::genFixeds(const FloatFormat &format, vector<float> &dst) const
{
    const int minExp          = format.getMinExp();
    const int maxExp          = format.getMaxExp();
    const int fractionBits    = format.getFractionBits();
    const float minQuantum    = deFloatLdExp(1.0f, minExp - fractionBits);
    const float minNormalized = deFloatLdExp(1.0f, minExp);
    const float maxQuantum    = deFloatLdExp(1.0f, maxExp - fractionBits);

    // NaN
    dst.push_back(TCU_NAN);
    // Zero
    dst.push_back(0.0f);

    for (int sign = -1; sign <= 1; sign += 2)
    {
        // Smallest subnormal
        dst.push_back((float)sign * minQuantum);

        // Largest subnormal
        dst.push_back((float)sign * (minNormalized - minQuantum));

        // Smallest normalized
        dst.push_back((float)sign * minNormalized);

        // Next smallest normalized
        dst.push_back((float)sign * (minNormalized + minQuantum));

        dst.push_back((float)sign * 0.5f);
        dst.push_back((float)sign * 1.0f);
        dst.push_back((float)sign * 2.0f);

        // Largest number
        dst.push_back((float)sign * (deFloatLdExp(1.0f, maxExp) + (deFloatLdExp(1.0f, maxExp) - maxQuantum)));

        dst.push_back((float)sign * TCU_INFINITY);
    }
}

template <typename T, int Size>
class DefaultSampling<Vector<T, Size>> : public Sampling<Vector<T, Size>>
{
public:
    typedef Vector<T, Size> Value;

    Value genRandom(const FloatFormat &fmt, Precision prec, Random &rnd) const
    {
        Value ret;

        for (int ndx = 0; ndx < Size; ++ndx)
            ret[ndx] = instance<DefaultSampling<T>>().genRandom(fmt, prec, rnd);

        return ret;
    }

    void genFixeds(const FloatFormat &fmt, vector<Value> &dst) const
    {
        vector<T> scalars;

        instance<DefaultSampling<T>>().genFixeds(fmt, scalars);

        for (size_t scalarNdx = 0; scalarNdx < scalars.size(); ++scalarNdx)
            dst.push_back(Value(scalars[scalarNdx]));
    }

    double getWeight(void) const
    {
        return dePow(instance<DefaultSampling<T>>().getWeight(), Size);
    }
};

template <typename T, int Rows, int Columns>
class DefaultSampling<Matrix<T, Rows, Columns>> : public Sampling<Matrix<T, Rows, Columns>>
{
public:
    typedef Matrix<T, Rows, Columns> Value;

    Value genRandom(const FloatFormat &fmt, Precision prec, Random &rnd) const
    {
        Value ret;

        for (int rowNdx = 0; rowNdx < Rows; ++rowNdx)
            for (int colNdx = 0; colNdx < Columns; ++colNdx)
                ret(rowNdx, colNdx) = instance<DefaultSampling<T>>().genRandom(fmt, prec, rnd);

        return ret;
    }

    void genFixeds(const FloatFormat &fmt, vector<Value> &dst) const
    {
        vector<T> scalars;

        instance<DefaultSampling<T>>().genFixeds(fmt, scalars);

        for (size_t scalarNdx = 0; scalarNdx < scalars.size(); ++scalarNdx)
            dst.push_back(Value(scalars[scalarNdx]));

        if (Columns == Rows)
        {
            Value mat(0.0);
            T x       = T(1.0f);
            mat[0][0] = x;
            for (int ndx = 0; ndx < Columns; ++ndx)
            {
                mat[Columns - 1 - ndx][ndx] = x;
                x *= T(2.0f);
            }
            dst.push_back(mat);
        }
    }

    double getWeight(void) const
    {
        return dePow(instance<DefaultSampling<T>>().getWeight(), Rows * Columns);
    }
};

struct Context
{
    Context(const string &name_, TestContext &testContext_, RenderContext &renderContext_,
            const FloatFormat &floatFormat_, const FloatFormat &highpFormat_, Precision precision_,
            ShaderType shaderType_, size_t numRandoms_)
        : name(name_)
        , testContext(testContext_)
        , renderContext(renderContext_)
        , floatFormat(floatFormat_)
        , highpFormat(highpFormat_)
        , precision(precision_)
        , shaderType(shaderType_)
        , numRandoms(numRandoms_)
    {
    }

    string name;
    TestContext &testContext;
    RenderContext &renderContext;
    FloatFormat floatFormat;
    FloatFormat highpFormat;
    Precision precision;
    ShaderType shaderType;
    size_t numRandoms;
};

template <typename In0_ = Void, typename In1_ = Void, typename In2_ = Void, typename In3_ = Void>
struct InTypes
{
    typedef In0_ In0;
    typedef In1_ In1;
    typedef In2_ In2;
    typedef In3_ In3;
};

template <typename In>
int numInputs(void)
{
    return (!isTypeValid<typename In::In0>() ? 0 :
            !isTypeValid<typename In::In1>() ? 1 :
            !isTypeValid<typename In::In2>() ? 2 :
            !isTypeValid<typename In::In3>() ? 3 :
                                               4);
}

template <typename Out0_, typename Out1_ = Void>
struct OutTypes
{
    typedef Out0_ Out0;
    typedef Out1_ Out1;
};

template <typename Out>
int numOutputs(void)
{
    return (!isTypeValid<typename Out::Out0>() ? 0 : !isTypeValid<typename Out::Out1>() ? 1 : 2);
}

template <typename In>
struct Inputs
{
    vector<typename In::In0> in0;
    vector<typename In::In1> in1;
    vector<typename In::In2> in2;
    vector<typename In::In3> in3;
};

template <typename Out>
struct Outputs
{
    Outputs(size_t size) : out0(size), out1(size)
    {
    }

    vector<typename Out::Out0> out0;
    vector<typename Out::Out1> out1;
};

template <typename In, typename Out>
struct Variables
{
    VariableP<typename In::In0> in0;
    VariableP<typename In::In1> in1;
    VariableP<typename In::In2> in2;
    VariableP<typename In::In3> in3;
    VariableP<typename Out::Out0> out0;
    VariableP<typename Out::Out1> out1;
};

template <typename In>
struct Samplings
{
    Samplings(const Sampling<typename In::In0> &in0_, const Sampling<typename In::In1> &in1_,
              const Sampling<typename In::In2> &in2_, const Sampling<typename In::In3> &in3_)
        : in0(in0_)
        , in1(in1_)
        , in2(in2_)
        , in3(in3_)
    {
    }

    const Sampling<typename In::In0> &in0;
    const Sampling<typename In::In1> &in1;
    const Sampling<typename In::In2> &in2;
    const Sampling<typename In::In3> &in3;
};

template <typename In>
struct DefaultSamplings : Samplings<In>
{
    DefaultSamplings(void)
        : Samplings<In>(instance<DefaultSampling<typename In::In0>>(), instance<DefaultSampling<typename In::In1>>(),
                        instance<DefaultSampling<typename In::In2>>(), instance<DefaultSampling<typename In::In3>>())
    {
    }
};

class PrecisionCase : public TestCase
{
public:
    IterateResult iterate(void);

protected:
    PrecisionCase(const Context &context, const string &name, const string &extension = "")
        : TestCase(context.testContext, name.c_str(), name.c_str())
        , m_ctx(context)
        , m_status()
        , m_rnd(0xdeadbeefu + context.testContext.getCommandLine().getBaseSeed())
        , m_extension(extension)
    {
    }

    RenderContext &getRenderContext(void) const
    {
        return m_ctx.renderContext;
    }

    const FloatFormat &getFormat(void) const
    {
        return m_ctx.floatFormat;
    }

    TestLog &log(void) const
    {
        return m_testCtx.getLog();
    }

    virtual void runTest(void) = 0;

    template <typename In, typename Out>
    void testStatement(const Variables<In, Out> &variables, const Inputs<In> &inputs, const Statement &stmt);

    template <typename T>
    Symbol makeSymbol(const Variable<T> &variable)
    {
        return Symbol(variable.getName(), getVarTypeOf<T>(m_ctx.precision));
    }

    Context m_ctx;
    ResultCollector m_status;
    Random m_rnd;
    const string m_extension;
};

IterateResult PrecisionCase::iterate(void)
{
    runTest();
    m_status.setTestContextResult(m_testCtx);
    return STOP;
}

template <typename In, typename Out>
void PrecisionCase::testStatement(const Variables<In, Out> &variables, const Inputs<In> &inputs, const Statement &stmt)
{
    using namespace ShaderExecUtil;

    typedef typename In::In0 In0;
    typedef typename In::In1 In1;
    typedef typename In::In2 In2;
    typedef typename In::In3 In3;
    typedef typename Out::Out0 Out0;
    typedef typename Out::Out1 Out1;

    const FloatFormat &fmt = getFormat();
    const int inCount      = numInputs<In>();
    const int outCount     = numOutputs<Out>();
    const size_t numValues = (inCount > 0) ? inputs.in0.size() : 1;
    Outputs<Out> outputs(numValues);
    ShaderSpec spec;
    const FloatFormat highpFmt = m_ctx.highpFormat;
    const int maxMsgs          = 100;
    int numErrors              = 0;
    Environment env; // Hoisted out of the inner loop for optimization.

    switch (inCount)
    {
    case 4:
        DE_ASSERT(inputs.in3.size() == numValues);
    // Fallthrough
    case 3:
        DE_ASSERT(inputs.in2.size() == numValues);
    // Fallthrough
    case 2:
        DE_ASSERT(inputs.in1.size() == numValues);
    // Fallthrough
    case 1:
        DE_ASSERT(inputs.in0.size() == numValues);
    // Fallthrough
    default:
        break;
    }

    // Print out the statement and its definitions
    log() << TestLog::Message << "Statement: " << stmt << TestLog::EndMessage;
    {
        ostringstream oss;
        FuncSet funcs;

        stmt.getUsedFuncs(funcs);
        for (FuncSet::const_iterator it = funcs.begin(); it != funcs.end(); ++it)
        {
            (*it)->printDefinition(oss);
        }
        if (!funcs.empty())
            log() << TestLog::Message << "Reference definitions:\n" << oss.str() << TestLog::EndMessage;
    }

    // Initialize ShaderSpec from precision, variables and statement.
    {
        ostringstream os;
        os << "precision " << glu::getPrecisionName(m_ctx.precision) << " float;\n";
        spec.globalDeclarations = os.str();
    }

    spec.version = getContextTypeGLSLVersion(getRenderContext().getType());

    if (!m_extension.empty())
        spec.globalDeclarations = "#extension " + m_extension + " : require\n";

    spec.inputs.resize(inCount);

    switch (inCount)
    {
    case 4:
        spec.inputs[3] = makeSymbol(*variables.in3);
    // Fallthrough
    case 3:
        spec.inputs[2] = makeSymbol(*variables.in2);
    // Fallthrough
    case 2:
        spec.inputs[1] = makeSymbol(*variables.in1);
    // Fallthrough
    case 1:
        spec.inputs[0] = makeSymbol(*variables.in0);
    // Fallthrough
    default:
        break;
    }

    spec.outputs.resize(outCount);

    switch (outCount)
    {
    case 2:
        spec.outputs[1] = makeSymbol(*variables.out1); // Fallthrough
    case 1:
        spec.outputs[0] = makeSymbol(*variables.out0);
    default:
        break;
    }

    spec.source = de::toString(stmt);

    // Run the shader with inputs.
    {
        UniquePtr<ShaderExecutor> executor(createExecutor(getRenderContext(), m_ctx.shaderType, spec));
        const void *inputArr[] = {
            &inputs.in0.front(),
            &inputs.in1.front(),
            &inputs.in2.front(),
            &inputs.in3.front(),
        };
        void *outputArr[] = {
            &outputs.out0.front(),
            &outputs.out1.front(),
        };

        executor->log(log());
        if (!executor->isOk())
            TCU_FAIL("Shader compilation failed");

        executor->useProgram();
        executor->execute(int(numValues), inputArr, outputArr);
    }

    // Initialize environment with unused values so we don't need to bind in inner loop.
    {
        const typename Traits<In0>::IVal in0;
        const typename Traits<In1>::IVal in1;
        const typename Traits<In2>::IVal in2;
        const typename Traits<In3>::IVal in3;
        const typename Traits<Out0>::IVal reference0;
        const typename Traits<Out1>::IVal reference1;

        env.bind(*variables.in0, in0);
        env.bind(*variables.in1, in1);
        env.bind(*variables.in2, in2);
        env.bind(*variables.in3, in3);
        env.bind(*variables.out0, reference0);
        env.bind(*variables.out1, reference1);
    }

    // For each input tuple, compute output reference interval and compare
    // shader output to the reference.
    for (size_t valueNdx = 0; valueNdx < numValues; valueNdx++)
    {
        bool result = true;
        bool inExpectedRange;
        bool inWarningRange;
        const char *failStr = "Fail";
        typename Traits<Out0>::IVal reference0;
        typename Traits<Out1>::IVal reference1;

        if (valueNdx % (size_t)TOUCH_WATCHDOG_VALUE_FREQUENCY == 0)
            m_testCtx.touchWatchdog();

        env.lookup(*variables.in0) = convert<In0>(fmt, round(fmt, inputs.in0[valueNdx]));
        env.lookup(*variables.in1) = convert<In1>(fmt, round(fmt, inputs.in1[valueNdx]));
        env.lookup(*variables.in2) = convert<In2>(fmt, round(fmt, inputs.in2[valueNdx]));
        env.lookup(*variables.in3) = convert<In3>(fmt, round(fmt, inputs.in3[valueNdx]));

        {
            EvalContext ctx(fmt, m_ctx.precision, env);
            stmt.execute(ctx);
        }

        switch (outCount)
        {
        case 2:
            reference1      = convert<Out1>(highpFmt, env.lookup(*variables.out1));
            inExpectedRange = contains(reference1, outputs.out1[valueNdx]);
            inWarningRange  = containsWarning(reference1, outputs.out1[valueNdx]);
            if (!inExpectedRange && inWarningRange)
            {
                m_status.addResult(QP_TEST_RESULT_QUALITY_WARNING, "Shader output 1 has low-quality shader precision");
                failStr = "QualityWarning";
                result  = false;
            }
            else if (!inExpectedRange)
            {
                m_status.addResult(QP_TEST_RESULT_FAIL, "Shader output 1 is outside acceptable range");
                failStr = "Fail";
                result  = false;
            }
            // Fallthrough

        case 1:
            reference0      = convert<Out0>(highpFmt, env.lookup(*variables.out0));
            inExpectedRange = contains(reference0, outputs.out0[valueNdx]);
            inWarningRange  = containsWarning(reference0, outputs.out0[valueNdx]);
            if (!inExpectedRange && inWarningRange)
            {
                m_status.addResult(QP_TEST_RESULT_QUALITY_WARNING, "Shader output 0 has low-quality shader precision");
                failStr = "QualityWarning";
                result  = false;
            }
            else if (!inExpectedRange)
            {
                m_status.addResult(QP_TEST_RESULT_FAIL, "Shader output 0 is outside acceptable range");
                failStr = "Fail";
                result  = false;
            }

        default:
            break;
        }

        if (!result)
            ++numErrors;

        if ((!result && numErrors <= maxMsgs) || GLS_LOG_ALL_RESULTS)
        {
            MessageBuilder builder = log().message();

            builder << (result ? "Passed" : failStr) << " sample:\n";

            if (inCount > 0)
            {
                builder << "\t" << variables.in0->getName() << " = " << valueToString(highpFmt, inputs.in0[valueNdx])
                        << "\n";
            }

            if (inCount > 1)
            {
                builder << "\t" << variables.in1->getName() << " = " << valueToString(highpFmt, inputs.in1[valueNdx])
                        << "\n";
            }

            if (inCount > 2)
            {
                builder << "\t" << variables.in2->getName() << " = " << valueToString(highpFmt, inputs.in2[valueNdx])
                        << "\n";
            }

            if (inCount > 3)
            {
                builder << "\t" << variables.in3->getName() << " = " << valueToString(highpFmt, inputs.in3[valueNdx])
                        << "\n";
            }

            if (outCount > 0)
            {
                builder << "\t" << variables.out0->getName() << " = " << valueToString(highpFmt, outputs.out0[valueNdx])
                        << "\n"
                        << "\tExpected range: " << intervalToString<typename Out::Out0>(highpFmt, reference0) << "\n";
            }

            if (outCount > 1)
            {
                builder << "\t" << variables.out1->getName() << " = " << valueToString(highpFmt, outputs.out1[valueNdx])
                        << "\n"
                        << "\tExpected range: " << intervalToString<typename Out::Out1>(highpFmt, reference1) << "\n";
            }

            builder << TestLog::EndMessage;
        }
    }

    if (numErrors > maxMsgs)
    {
        log() << TestLog::Message << "(Skipped " << (numErrors - maxMsgs) << " messages.)" << TestLog::EndMessage;
    }

    if (numErrors == 0)
    {
        log() << TestLog::Message << "All " << numValues << " inputs passed." << TestLog::EndMessage;
    }
    else
    {
        log() << TestLog::Message << numErrors << "/" << numValues << " inputs failed or had quality warnings."
              << TestLog::EndMessage;
    }
}

template <typename T>
struct InputLess
{
    bool operator()(const T &val1, const T &val2) const
    {
        return val1 < val2;
    }
};

template <typename T>
bool inputLess(const T &val1, const T &val2)
{
    return InputLess<T>()(val1, val2);
}

template <>
struct InputLess<float>
{
    bool operator()(const float &val1, const float &val2) const
    {
        if (std::isnan(val1))
            return false;
        if (std::isnan(val2))
            return true;
        return val1 < val2;
    }
};

template <typename T, int Size>
struct InputLess<Vector<T, Size>>
{
    bool operator()(const Vector<T, Size> &vec1, const Vector<T, Size> &vec2) const
    {
        for (int ndx = 0; ndx < Size; ++ndx)
        {
            if (inputLess(vec1[ndx], vec2[ndx]))
                return true;
            if (inputLess(vec2[ndx], vec1[ndx]))
                return false;
        }

        return false;
    }
};

template <typename T, int Rows, int Cols>
struct InputLess<Matrix<T, Rows, Cols>>
{
    bool operator()(const Matrix<T, Rows, Cols> &mat1, const Matrix<T, Rows, Cols> &mat2) const
    {
        for (int col = 0; col < Cols; ++col)
        {
            if (inputLess(mat1[col], mat2[col]))
                return true;
            if (inputLess(mat2[col], mat1[col]))
                return false;
        }

        return false;
    }
};

template <typename In>
struct InTuple : public Tuple4<typename In::In0, typename In::In1, typename In::In2, typename In::In3>
{
    InTuple(const typename In::In0 &in0, const typename In::In1 &in1, const typename In::In2 &in2,
            const typename In::In3 &in3)
        : Tuple4<typename In::In0, typename In::In1, typename In::In2, typename In::In3>(in0, in1, in2, in3)
    {
    }
};

template <typename In>
struct InputLess<InTuple<In>>
{
    bool operator()(const InTuple<In> &in1, const InTuple<In> &in2) const
    {
        if (inputLess(in1.a, in2.a))
            return true;
        if (inputLess(in2.a, in1.a))
            return false;
        if (inputLess(in1.b, in2.b))
            return true;
        if (inputLess(in2.b, in1.b))
            return false;
        if (inputLess(in1.c, in2.c))
            return true;
        if (inputLess(in2.c, in1.c))
            return false;
        if (inputLess(in1.d, in2.d))
            return true;
        return false;
    }
};

template <typename In>
Inputs<In> generateInputs(const Samplings<In> &samplings, const FloatFormat &floatFormat, Precision intPrecision,
                          size_t numSamples, Random &rnd)
{
    Inputs<In> ret;
    Inputs<In> fixedInputs;
    set<InTuple<In>, InputLess<InTuple<In>>> seenInputs;

    samplings.in0.genFixeds(floatFormat, fixedInputs.in0);
    samplings.in1.genFixeds(floatFormat, fixedInputs.in1);
    samplings.in2.genFixeds(floatFormat, fixedInputs.in2);
    samplings.in3.genFixeds(floatFormat, fixedInputs.in3);

    for (size_t ndx0 = 0; ndx0 < fixedInputs.in0.size(); ++ndx0)
    {
        for (size_t ndx1 = 0; ndx1 < fixedInputs.in1.size(); ++ndx1)
        {
            for (size_t ndx2 = 0; ndx2 < fixedInputs.in2.size(); ++ndx2)
            {
                for (size_t ndx3 = 0; ndx3 < fixedInputs.in3.size(); ++ndx3)
                {
                    const InTuple<In> tuple(fixedInputs.in0[ndx0], fixedInputs.in1[ndx1], fixedInputs.in2[ndx2],
                                            fixedInputs.in3[ndx3]);

                    seenInputs.insert(tuple);
                    ret.in0.push_back(tuple.a);
                    ret.in1.push_back(tuple.b);
                    ret.in2.push_back(tuple.c);
                    ret.in3.push_back(tuple.d);
                }
            }
        }
    }

    for (size_t ndx = 0; ndx < numSamples; ++ndx)
    {
        const typename In::In0 in0 = samplings.in0.genRandom(floatFormat, intPrecision, rnd);
        const typename In::In1 in1 = samplings.in1.genRandom(floatFormat, intPrecision, rnd);
        const typename In::In2 in2 = samplings.in2.genRandom(floatFormat, intPrecision, rnd);
        const typename In::In3 in3 = samplings.in3.genRandom(floatFormat, intPrecision, rnd);
        const InTuple<In> tuple(in0, in1, in2, in3);

        if (de::contains(seenInputs, tuple))
            continue;

        seenInputs.insert(tuple);
        ret.in0.push_back(in0);
        ret.in1.push_back(in1);
        ret.in2.push_back(in2);
        ret.in3.push_back(in3);
    }

    return ret;
}

class FuncCaseBase : public PrecisionCase
{
public:
    IterateResult iterate(void);

protected:
    FuncCaseBase(const Context &context, const string &name, const FuncBase &func)
        : PrecisionCase(context, name, func.getRequiredExtension(context.renderContext))
    {
    }
};

IterateResult FuncCaseBase::iterate(void)
{
    MovePtr<ContextInfo> info(ContextInfo::create(getRenderContext()));

    if (!m_extension.empty() && !info->isExtensionSupported(m_extension.c_str()) &&
        !glu::contextSupports(getRenderContext().getType(), glu::ApiType::core(4, 5)))
        throw NotSupportedError("Unsupported extension: " + m_extension);

    runTest();

    m_status.setTestContextResult(m_testCtx);
    return STOP;
}

template <typename Sig>
class FuncCase : public FuncCaseBase
{
public:
    typedef Func<Sig> CaseFunc;
    typedef typename Sig::Ret Ret;
    typedef typename Sig::Arg0 Arg0;
    typedef typename Sig::Arg1 Arg1;
    typedef typename Sig::Arg2 Arg2;
    typedef typename Sig::Arg3 Arg3;
    typedef InTypes<Arg0, Arg1, Arg2, Arg3> In;
    typedef OutTypes<Ret> Out;

    FuncCase(const Context &context, const string &name, const CaseFunc &func)
        : FuncCaseBase(context, name, func)
        , m_func(func)
    {
    }

protected:
    void runTest(void);

    virtual const Samplings<In> &getSamplings(void)
    {
        return instance<DefaultSamplings<In>>();
    }

private:
    const CaseFunc &m_func;
};

template <typename Sig>
void FuncCase<Sig>::runTest(void)
{
    const Inputs<In> inputs(
        generateInputs(getSamplings(), m_ctx.floatFormat, m_ctx.precision, m_ctx.numRandoms, m_rnd));
    Variables<In, Out> variables;

    variables.out0 = variable<Ret>("out0");
    variables.out1 = variable<Void>("out1");
    variables.in0  = variable<Arg0>("in0");
    variables.in1  = variable<Arg1>("in1");
    variables.in2  = variable<Arg2>("in2");
    variables.in3  = variable<Arg3>("in3");

    {
        ExprP<Ret> expr = applyVar(m_func, variables.in0, variables.in1, variables.in2, variables.in3);
        StatementP stmt = variableAssignment(variables.out0, expr);

        this->testStatement(variables, inputs, *stmt);
    }
}

template <typename Sig>
class InOutFuncCase : public FuncCaseBase
{
public:
    typedef Func<Sig> CaseFunc;
    typedef typename Sig::Ret Ret;
    typedef typename Sig::Arg0 Arg0;
    typedef typename Sig::Arg1 Arg1;
    typedef typename Sig::Arg2 Arg2;
    typedef typename Sig::Arg3 Arg3;
    typedef InTypes<Arg0, Arg2, Arg3> In;
    typedef OutTypes<Ret, Arg1> Out;

    InOutFuncCase(const Context &context, const string &name, const CaseFunc &func)
        : FuncCaseBase(context, name, func)
        , m_func(func)
    {
    }

protected:
    void runTest(void);

    virtual const Samplings<In> &getSamplings(void)
    {
        return instance<DefaultSamplings<In>>();
    }

private:
    const CaseFunc &m_func;
};

template <typename Sig>
void InOutFuncCase<Sig>::runTest(void)
{
    const Inputs<In> inputs(
        generateInputs(getSamplings(), m_ctx.floatFormat, m_ctx.precision, m_ctx.numRandoms, m_rnd));
    Variables<In, Out> variables;

    variables.out0 = variable<Ret>("out0");
    variables.out1 = variable<Arg1>("out1");
    variables.in0  = variable<Arg0>("in0");
    variables.in1  = variable<Arg2>("in1");
    variables.in2  = variable<Arg3>("in2");
    variables.in3  = variable<Void>("in3");

    {
        ExprP<Ret> expr = applyVar(m_func, variables.in0, variables.out1, variables.in1, variables.in2);
        StatementP stmt = variableAssignment(variables.out0, expr);

        this->testStatement(variables, inputs, *stmt);
    }
}

template <typename Sig>
PrecisionCase *createFuncCase(const Context &context, const string &name, const Func<Sig> &func)
{
    switch (func.getOutParamIndex())
    {
    case -1:
        return new FuncCase<Sig>(context, name, func);
    case 1:
        return new InOutFuncCase<Sig>(context, name, func);
    default:
        DE_FATAL("Impossible");
    }
    return nullptr;
}

class CaseFactory
{
public:
    virtual ~CaseFactory(void)
    {
    }
    virtual MovePtr<TestNode> createCase(const Context &ctx) const = 0;
    virtual string getName(void) const                             = 0;
    virtual string getDesc(void) const                             = 0;
};

class FuncCaseFactory : public CaseFactory
{
public:
    virtual const FuncBase &getFunc(void) const = 0;

    string getName(void) const
    {
        return de::toLower(getFunc().getName());
    }

    string getDesc(void) const
    {
        return "Function '" + getFunc().getName() + "'";
    }
};

template <typename Sig>
class GenFuncCaseFactory : public CaseFactory
{
public:
    GenFuncCaseFactory(const GenFuncs<Sig> &funcs, const string &name) : m_funcs(funcs), m_name(de::toLower(name))
    {
    }

    MovePtr<TestNode> createCase(const Context &ctx) const
    {
        TestCaseGroup *group = new TestCaseGroup(ctx.testContext, ctx.name.c_str(), ctx.name.c_str());

        group->addChild(createFuncCase(ctx, "scalar", m_funcs.func));
        group->addChild(createFuncCase(ctx, "vec2", m_funcs.func2));
        group->addChild(createFuncCase(ctx, "vec3", m_funcs.func3));
        group->addChild(createFuncCase(ctx, "vec4", m_funcs.func4));

        return MovePtr<TestNode>(group);
    }

    string getName(void) const
    {
        return m_name;
    }

    string getDesc(void) const
    {
        return "Function '" + m_funcs.func.getName() + "'";
    }

private:
    const GenFuncs<Sig> m_funcs;
    string m_name;
};

template <template <int> class GenF>
class TemplateFuncCaseFactory : public FuncCaseFactory
{
public:
    MovePtr<TestNode> createCase(const Context &ctx) const
    {
        TestCaseGroup *group = new TestCaseGroup(ctx.testContext, ctx.name.c_str(), ctx.name.c_str());
        group->addChild(createFuncCase(ctx, "scalar", instance<GenF<1>>()));
        group->addChild(createFuncCase(ctx, "vec2", instance<GenF<2>>()));
        group->addChild(createFuncCase(ctx, "vec3", instance<GenF<3>>()));
        group->addChild(createFuncCase(ctx, "vec4", instance<GenF<4>>()));

        return MovePtr<TestNode>(group);
    }

    const FuncBase &getFunc(void) const
    {
        return instance<GenF<1>>();
    }
};

template <template <int> class GenF>
class SquareMatrixFuncCaseFactory : public FuncCaseFactory
{
public:
    MovePtr<TestNode> createCase(const Context &ctx) const
    {
        TestCaseGroup *group = new TestCaseGroup(ctx.testContext, ctx.name.c_str(), ctx.name.c_str());
        group->addChild(createFuncCase(ctx, "mat2", instance<GenF<2>>()));
#if 0
        // disabled until we get reasonable results
        group->addChild(createFuncCase(ctx, "mat3", instance<GenF<3> >()));
        group->addChild(createFuncCase(ctx, "mat4", instance<GenF<4> >()));
#endif

        return MovePtr<TestNode>(group);
    }

    const FuncBase &getFunc(void) const
    {
        return instance<GenF<2>>();
    }
};

template <template <int, int> class GenF>
class MatrixFuncCaseFactory : public FuncCaseFactory
{
public:
    MovePtr<TestNode> createCase(const Context &ctx) const
    {
        TestCaseGroup *const group = new TestCaseGroup(ctx.testContext, ctx.name.c_str(), ctx.name.c_str());

        this->addCase<2, 2>(ctx, group);
        this->addCase<3, 2>(ctx, group);
        this->addCase<4, 2>(ctx, group);
        this->addCase<2, 3>(ctx, group);
        this->addCase<3, 3>(ctx, group);
        this->addCase<4, 3>(ctx, group);
        this->addCase<2, 4>(ctx, group);
        this->addCase<3, 4>(ctx, group);
        this->addCase<4, 4>(ctx, group);

        return MovePtr<TestNode>(group);
    }

    const FuncBase &getFunc(void) const
    {
        return instance<GenF<2, 2>>();
    }

private:
    template <int Rows, int Cols>
    void addCase(const Context &ctx, TestCaseGroup *group) const
    {
        const char *const name = dataTypeNameOf<Matrix<float, Rows, Cols>>();

        group->addChild(createFuncCase(ctx, name, instance<GenF<Rows, Cols>>()));
    }
};

template <typename Sig>
class SimpleFuncCaseFactory : public CaseFactory
{
public:
    SimpleFuncCaseFactory(const Func<Sig> &func) : m_func(func)
    {
    }

    MovePtr<TestNode> createCase(const Context &ctx) const
    {
        return MovePtr<TestNode>(createFuncCase(ctx, ctx.name.c_str(), m_func));
    }

    string getName(void) const
    {
        return de::toLower(m_func.getName());
    }

    string getDesc(void) const
    {
        return "Function '" + getName() + "'";
    }

private:
    const Func<Sig> &m_func;
};

template <typename F>
SharedPtr<SimpleFuncCaseFactory<typename F::Sig>> createSimpleFuncCaseFactory(void)
{
    return SharedPtr<SimpleFuncCaseFactory<typename F::Sig>>(new SimpleFuncCaseFactory<typename F::Sig>(instance<F>()));
}

class BuiltinFuncs : public CaseFactories
{
public:
    const vector<const CaseFactory *> getFactories(void) const
    {
        vector<const CaseFactory *> ret;

        for (size_t ndx = 0; ndx < m_factories.size(); ++ndx)
            ret.push_back(m_factories[ndx].get());

        return ret;
    }

    void addFactory(SharedPtr<const CaseFactory> fact)
    {
        m_factories.push_back(fact);
    }

private:
    vector<SharedPtr<const CaseFactory>> m_factories;
};

template <typename F>
void addScalarFactory(BuiltinFuncs &funcs, string name = "")
{
    if (name.empty())
        name = instance<F>().getName();

    funcs.addFactory(
        SharedPtr<const CaseFactory>(new GenFuncCaseFactory<typename F::Sig>(makeVectorizedFuncs<F>(), name)));
}

MovePtr<const CaseFactories> createES3BuiltinCases(void)
{
    MovePtr<BuiltinFuncs> funcs(new BuiltinFuncs());

    addScalarFactory<Add>(*funcs);
    addScalarFactory<Sub>(*funcs);
    addScalarFactory<Mul>(*funcs);
    addScalarFactory<Div>(*funcs);

    addScalarFactory<Radians>(*funcs);
    addScalarFactory<Degrees>(*funcs);
    addScalarFactory<Sin>(*funcs);
    addScalarFactory<Cos>(*funcs);
    addScalarFactory<Tan>(*funcs);
    addScalarFactory<ASin>(*funcs);
    addScalarFactory<ACos>(*funcs);
    addScalarFactory<ATan2>(*funcs, "atan2");
    addScalarFactory<ATan>(*funcs);
    addScalarFactory<Sinh>(*funcs);
    addScalarFactory<Cosh>(*funcs);
    addScalarFactory<Tanh>(*funcs);
    addScalarFactory<ASinh>(*funcs);
    addScalarFactory<ACosh>(*funcs);
    addScalarFactory<ATanh>(*funcs);

    addScalarFactory<Pow>(*funcs);
    addScalarFactory<Exp>(*funcs);
    addScalarFactory<Log>(*funcs);
    addScalarFactory<Exp2>(*funcs);
    addScalarFactory<Log2>(*funcs);
    addScalarFactory<Sqrt>(*funcs);
    addScalarFactory<InverseSqrt>(*funcs);

    addScalarFactory<Abs>(*funcs);
    addScalarFactory<Sign>(*funcs);
    addScalarFactory<Floor>(*funcs);
    addScalarFactory<Trunc>(*funcs);
    addScalarFactory<Round>(*funcs);
    addScalarFactory<RoundEven>(*funcs);
    addScalarFactory<Ceil>(*funcs);
    addScalarFactory<Fract>(*funcs);
    addScalarFactory<Mod>(*funcs);
    funcs->addFactory(createSimpleFuncCaseFactory<Modf>());
    addScalarFactory<Min>(*funcs);
    addScalarFactory<Max>(*funcs);
    addScalarFactory<Clamp>(*funcs);
    addScalarFactory<Mix>(*funcs);
    addScalarFactory<Step>(*funcs);
    addScalarFactory<SmoothStep>(*funcs);

    funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Length>()));
    funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Distance>()));
    funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Dot>()));
    funcs->addFactory(createSimpleFuncCaseFactory<Cross>());
    funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Normalize>()));
    funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<FaceForward>()));
    funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Reflect>()));
    funcs->addFactory(SharedPtr<const CaseFactory>(new TemplateFuncCaseFactory<Refract>()));

    funcs->addFactory(SharedPtr<const CaseFactory>(new MatrixFuncCaseFactory<MatrixCompMult>()));
    funcs->addFactory(SharedPtr<const CaseFactory>(new MatrixFuncCaseFactory<OuterProduct>()));
    funcs->addFactory(SharedPtr<const CaseFactory>(new MatrixFuncCaseFactory<Transpose>()));
    funcs->addFactory(SharedPtr<const CaseFactory>(new SquareMatrixFuncCaseFactory<Determinant>()));
    funcs->addFactory(SharedPtr<const CaseFactory>(new SquareMatrixFuncCaseFactory<Inverse>()));

    return MovePtr<const CaseFactories>(funcs.release());
}

MovePtr<const CaseFactories> createES31BuiltinCases(void)
{
    MovePtr<BuiltinFuncs> funcs(new BuiltinFuncs());

    addScalarFactory<FrExp>(*funcs);
    addScalarFactory<LdExp>(*funcs);
    addScalarFactory<Fma>(*funcs);

    return MovePtr<const CaseFactories>(funcs.release());
}

struct PrecisionTestContext
{
    PrecisionTestContext(TestContext &testCtx_, RenderContext &renderCtx_, const FloatFormat &highp_,
                         const FloatFormat &mediump_, const FloatFormat &lowp_, const vector<ShaderType> &shaderTypes_,
                         int numRandoms_)
        : testCtx(testCtx_)
        , renderCtx(renderCtx_)
        , shaderTypes(shaderTypes_)
        , numRandoms(numRandoms_)
    {
        formats[glu::PRECISION_HIGHP]   = &highp_;
        formats[glu::PRECISION_MEDIUMP] = &mediump_;
        formats[glu::PRECISION_LOWP]    = &lowp_;
    }

    TestContext &testCtx;
    RenderContext &renderCtx;
    const FloatFormat *formats[glu::PRECISION_LAST];
    vector<ShaderType> shaderTypes;
    int numRandoms;
};

TestCaseGroup *createFuncGroup(const PrecisionTestContext &ctx, const CaseFactory &factory)
{
    TestCaseGroup *const group = new TestCaseGroup(ctx.testCtx, factory.getName().c_str(), factory.getDesc().c_str());

    for (int precNdx = 0; precNdx < glu::PRECISION_LAST; ++precNdx)
    {
        const Precision precision = Precision(precNdx);
        const string precName(glu::getPrecisionName(precision));
        const FloatFormat &fmt      = *de::getSizedArrayElement<glu::PRECISION_LAST>(ctx.formats, precNdx);
        const FloatFormat &highpFmt = *de::getSizedArrayElement<glu::PRECISION_LAST>(ctx.formats, glu::PRECISION_HIGHP);

        for (size_t shaderNdx = 0; shaderNdx < ctx.shaderTypes.size(); ++shaderNdx)
        {
            const ShaderType shaderType = ctx.shaderTypes[shaderNdx];
            const string shaderName(glu::getShaderTypeName(shaderType));
            const string name = precName + "_" + shaderName;
            const Context caseCtx(name, ctx.testCtx, ctx.renderCtx, fmt, highpFmt, precision, shaderType,
                                  ctx.numRandoms);

            group->addChild(factory.createCase(caseCtx).release());
        }
    }

    return group;
}

void addBuiltinPrecisionTests(TestContext &testCtx, RenderContext &renderCtx, const CaseFactories &cases,
                              const vector<ShaderType> &shaderTypes, TestCaseGroup &dstGroup)
{
    const int userRandoms = testCtx.getCommandLine().getTestIterationCount();
    const int defRandoms  = 16384;
    const int numRandoms  = userRandoms > 0 ? userRandoms : defRandoms;
    const FloatFormat highp(-126, 127, 23, true,
                            tcu::MAYBE,  // subnormals
                            tcu::YES,    // infinities
                            tcu::MAYBE); // NaN
    // \todo [2014-04-01 lauri] Check these once Khronos bug 11840 is resolved.
    const FloatFormat mediump(-13, 13, 9, false);
    // A fixed-point format is just a floating point format with a fixed
    // exponent and support for subnormals.
    const FloatFormat lowp(0, 0, 7, false, tcu::YES);
    const PrecisionTestContext ctx(testCtx, renderCtx, highp, mediump, lowp, shaderTypes, numRandoms);

    for (size_t ndx = 0; ndx < cases.getFactories().size(); ++ndx)
        dstGroup.addChild(createFuncGroup(ctx, *cases.getFactories()[ndx]));
}

} // namespace BuiltinPrecisionTests
} // namespace gls
} // namespace deqp
