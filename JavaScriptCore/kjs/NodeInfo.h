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

    typedef unsigned int FeatureInfo;

    const FeatureInfo NoFeatures = 0;
    const FeatureInfo EvalFeature = 1 << 0;
    const FeatureInfo ClosureFeature = 1 << 1;
    const FeatureInfo AssignFeature = 1 << 2;

    template <typename T> struct NodeFeatureInfo {
        T m_node;
        FeatureInfo m_featureInfo;
    };
    
    typedef NodeFeatureInfo<FuncExprNode*> FuncExprNodeInfo;
    typedef NodeFeatureInfo<ExpressionNode*> ExpressionNodeInfo;
    typedef NodeFeatureInfo<ArgumentsNode*> ArgumentsNodeInfo;
    typedef NodeFeatureInfo<ConstDeclNode*> ConstDeclNodeInfo;
    typedef NodeFeatureInfo<PropertyNode*> PropertyNodeInfo;
    typedef NodeFeatureInfo<PropertyList> PropertyListInfo;
    typedef NodeFeatureInfo<ElementList> ElementListInfo;
    typedef NodeFeatureInfo<ArgumentList> ArgumentListInfo;
    
    template <typename T> struct NodeDeclarationInfo {
        T m_node;
        ParserRefCountedData<DeclarationStacks::VarStack>* m_varDeclarations;
        ParserRefCountedData<DeclarationStacks::FunctionStack>* m_funcDeclarations;
        FeatureInfo m_featureInfo;
    };
    
    typedef NodeDeclarationInfo<StatementNode*> StatementNodeInfo;
    typedef NodeDeclarationInfo<CaseBlockNode*> CaseBlockNodeInfo;
    typedef NodeDeclarationInfo<CaseClauseNode*> CaseClauseNodeInfo;
    typedef NodeDeclarationInfo<SourceElements*> SourceElementsInfo;
    typedef NodeDeclarationInfo<ClauseList> ClauseListInfo;
    typedef NodeDeclarationInfo<ExpressionNode*> VarDeclListInfo;
    typedef NodeDeclarationInfo<ConstDeclList> ConstDeclListInfo;

} // namespace KJS

#endif // NodeInfo_h
