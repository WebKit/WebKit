/*
 * functions.h - Copyright 2005 Frerich Raabe <raabe@kde.org>
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
#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "expression.h"

#include <qdict.h>
#include <qvaluelist.h>

class Function : public Expression
{
    public:
        void setArguments( const QValueList<Expression *> &args );
        void setName( const QString &name );

        virtual QString dump() const;

    protected:
        Expression *arg( int pos );
        const Expression *arg( int pos ) const;
        unsigned int argCount() const;
        QString name() const;

    private:
        QString m_name;
};

class FunctionLibrary
{
    friend class FunctionLibraryDeleter;
    public:
        static FunctionLibrary &self();

        Function *getFunction( const char *name,
                               const QValueList<Expression *> &args = QValueList<Expression *>() ) const;

    private:
        struct FunctionRec;

        FunctionLibrary();
        FunctionLibrary( const FunctionLibrary &rhs );
        FunctionLibrary &operator=( const FunctionLibrary &rhs );

        static FunctionLibrary *s_instance;
        QDict<FunctionRec> m_functionDict;
};

#endif // FUNCTIONS_H

