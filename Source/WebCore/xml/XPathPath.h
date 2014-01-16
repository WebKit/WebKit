/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009, 2013 Apple Inc. All rights reserved.
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

#ifndef XPathPath_h
#define XPathPath_h

#include "XPathExpressionNode.h"

namespace WebCore {

    namespace XPath {

        class Step;

        class Filter final : public Expression {
        public:
            Filter(std::unique_ptr<Expression>, Vector<std::unique_ptr<Expression>> predicates);

        private:
            virtual Value evaluate() const override;
            virtual Value::Type resultType() const override { return Value::NodeSetValue; }

            std::unique_ptr<Expression> m_expression;
            Vector<std::unique_ptr<Expression>> m_predicates;
        };

        class LocationPath final : public Expression {
        public:
            LocationPath();

            void setAbsolute() { m_isAbsolute = true; setIsContextNodeSensitive(false); }

            void evaluate(NodeSet& nodes) const; // nodes is an input/output parameter

            void appendStep(std::unique_ptr<Step>);
            void prependStep(std::unique_ptr<Step>);

        private:
            virtual Value evaluate() const override;
            virtual Value::Type resultType() const override { return Value::NodeSetValue; }

            Vector<std::unique_ptr<Step>> m_steps;
            bool m_isAbsolute;
        };

        class Path final : public Expression {
        public:
            Path(std::unique_ptr<Expression> filter, std::unique_ptr<LocationPath>);

        private:
            virtual Value evaluate() const override;
            virtual Value::Type resultType() const override { return Value::NodeSetValue; }

            std::unique_ptr<Expression> m_filter;
            std::unique_ptr<LocationPath> m_path;
        };

    }
}

#endif // XPath_Path_H
