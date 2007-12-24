/*
 *  Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef NodeInfo_h
#define NodeInfo_h

#include "nodes.h"
#include "Parser.h"

namespace KJS {

template <typename T> struct NodeInfo {
    T m_node;
    ParserRefCountedData<DeclarationStacks::VarStack>* m_varDeclarations;
    ParserRefCountedData<DeclarationStacks::FunctionStack>* m_funcDeclarations;
};

typedef NodeInfo<StatementNode*> StatementNodeInfo;
typedef NodeInfo<CaseBlockNode*> CaseBlockNodeInfo;
typedef NodeInfo<CaseClauseNode*> CaseClauseNodeInfo;
typedef NodeInfo<SourceElements*> SourceElementsInfo;
typedef NodeInfo<ClauseList> ClauseListInfo;
typedef NodeInfo<ExpressionNode*> VarDeclListInfo;
typedef NodeInfo<ConstDeclList> ConstDeclListInfo;

} // namespace KJS

#endif // NodeInfo_h
