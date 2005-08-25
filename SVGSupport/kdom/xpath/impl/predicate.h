/*
 * predicate.h - Copyright 2005 Frerich Raabe <raabe@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef PREDICATE_H
#define PREDICATE_H

#include "expression.h"

class Number : public Expression
{
	public:
		Number( double value );

		bool isConstant() const;
		virtual QString dump() const;

	private:
		virtual Value doEvaluate() const;

		double m_value;
};

class String : public Expression
{
	public:
		String( const DomString &value );

		bool isConstant() const;
		virtual QString dump() const;

	private:
		virtual Value doEvaluate() const;

		DomString m_value;
};

class Negative : public Expression
{
	public:
		virtual QString dump() const;

	private:
		virtual Value doEvaluate() const;
};

class BinaryExprBase : public Expression
{
	public:
		virtual QString dump() const;

	protected:
		virtual QString opName() const = 0;
};

class NumericOp : public BinaryExprBase
{
	public:
		enum {
			OP_Add = 1,
			OP_Sub,
			OP_Mul,
			OP_Div,
			OP_Mod,
			OP_GT,
			OP_LT,
			OP_GE,
			OP_LE
		};

		NumericOp( int opCode, Expression* lhs, Expression* rhs );

	private:
		virtual QString opName() const;
		virtual Value doEvaluate() const;
		int opCode;
};

class EqTestOp : public BinaryExprBase
{
	public:
		enum {
			OP_EQ = 1,
			OP_NE
		};

		EqTestOp( int opCode, Expression* lhs, Expression* rhs );

	private:
		virtual QString opName() const;
		virtual Value doEvaluate() const;
		int opCode;
};

class LogicalOp : public BinaryExprBase
{
	public:
		enum {
			OP_And = 1,
			OP_Or
		};

		LogicalOp( int opCode, Expression* lhs, Expression* rhs );

		virtual bool isConstant() const;

	private:
		bool    shortCircuitOn() const;
		virtual QString opName() const;
		virtual Value doEvaluate() const;
		int opCode;
};

class Union : public BinaryExprBase
{
	private:
		virtual QString opName() const;
		virtual Value doEvaluate() const;
};

class Predicate
{
	public:
		Predicate( Expression *expr );
		~Predicate();

		bool evaluate() const;

		void optimize();
		QString dump() const;

	private:
		Predicate( const Predicate &rhs );
		Predicate &operator=( const Predicate &rhs );

		Expression *m_expr;
};

#endif // PREDICATE_H

