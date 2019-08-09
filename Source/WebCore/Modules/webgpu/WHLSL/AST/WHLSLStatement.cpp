/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WHLSLStatement.h"

#if ENABLE(WEBGPU)

#include "WHLSLAST.h"

namespace WebCore {

namespace WHLSL {

namespace AST {

void Statement::destroy(Statement& statement)
{
    switch (statement.kind()) {
    case Kind::Block:
        delete &downcast<Block>(statement);
        break;
    case Kind::Break:
        delete &downcast<Break>(statement);
        break;
    case Kind::Continue:
        delete &downcast<Continue>(statement);
        break;
    case Kind::DoWhileLoop:
        delete &downcast<DoWhileLoop>(statement);
        break;
    case Kind::EffectfulExpression:
        delete &downcast<EffectfulExpressionStatement>(statement);
        break;
    case Kind::Fallthrough:
        delete &downcast<Fallthrough>(statement);
        break;
    case Kind::ForLoop:
        delete &downcast<ForLoop>(statement);
        break;
    case Kind::If:
        delete &downcast<IfStatement>(statement);
        break;
    case Kind::Return:
        delete &downcast<Return>(statement);
        break;
    case Kind::StatementList:
        delete &downcast<StatementList>(statement);
        break;
    case Kind::SwitchCase:
        delete &downcast<SwitchCase>(statement);
        break;
    case Kind::Switch:
        delete &downcast<SwitchStatement>(statement);
        break;
    case Kind::VariableDeclarations:
        delete &downcast<VariableDeclarationsStatement>(statement);
        break;
    case Kind::WhileLoop:
        delete &downcast<WhileLoop>(statement);
        break;
    }
}

void Statement::destruct(Statement& statement)
{
    switch (statement.kind()) {
    case Kind::Block:
        downcast<Block>(statement).~Block();
        break;
    case Kind::Break:
        downcast<Break>(statement).~Break();
        break;
    case Kind::Continue:
        downcast<Continue>(statement).~Continue();
        break;
    case Kind::DoWhileLoop:
        downcast<DoWhileLoop>(statement).~DoWhileLoop();
        break;
    case Kind::EffectfulExpression:
        downcast<EffectfulExpressionStatement>(statement).~EffectfulExpressionStatement();
        break;
    case Kind::Fallthrough:
        downcast<Fallthrough>(statement).~Fallthrough();
        break;
    case Kind::ForLoop:
        downcast<ForLoop>(statement).~ForLoop();
        break;
    case Kind::If:
        downcast<IfStatement>(statement).~IfStatement();
        break;
    case Kind::Return:
        downcast<Return>(statement).~Return();
        break;
    case Kind::StatementList:
        downcast<StatementList>(statement).~StatementList();
        break;
    case Kind::SwitchCase:
        downcast<SwitchCase>(statement).~SwitchCase();
        break;
    case Kind::Switch:
        downcast<SwitchStatement>(statement).~SwitchStatement();
        break;
    case Kind::VariableDeclarations:
        downcast<VariableDeclarationsStatement>(statement).~VariableDeclarationsStatement();
        break;
    case Kind::WhileLoop:
        downcast<WhileLoop>(statement).~WhileLoop();
        break;
    }
}

} // namespace AST

} // namespace WHLSL

} // namespace WebCore

#endif
