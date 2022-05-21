//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLFunctionStitching.hpp
//
// Copyright 2020-2021 Apple Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#pragma once

#include "MTLDefines.hpp"
#include "MTLHeaderBridge.hpp"
#include "MTLPrivate.hpp"

#include <Foundation/Foundation.hpp>

#include "MTLFunctionStitching.hpp"

namespace MTL
{
class FunctionStitchingAttribute : public NS::Referencing<FunctionStitchingAttribute>
{
};

class FunctionStitchingAttributeAlwaysInline : public NS::Referencing<FunctionStitchingAttributeAlwaysInline, FunctionStitchingAttribute>
{
public:
    static class FunctionStitchingAttributeAlwaysInline* alloc();

    class FunctionStitchingAttributeAlwaysInline*        init();
};

class FunctionStitchingNode : public NS::Copying<FunctionStitchingNode>
{
};

class FunctionStitchingInputNode : public NS::Referencing<FunctionStitchingInputNode, FunctionStitchingNode>
{
public:
    static class FunctionStitchingInputNode* alloc();

    class FunctionStitchingInputNode*        init();

    NS::UInteger                             argumentIndex() const;
    void                                     setArgumentIndex(NS::UInteger argumentIndex);

    MTL::FunctionStitchingInputNode*         init(NS::UInteger argument);
};

class FunctionStitchingFunctionNode : public NS::Referencing<FunctionStitchingFunctionNode, FunctionStitchingNode>
{
public:
    static class FunctionStitchingFunctionNode* alloc();

    class FunctionStitchingFunctionNode*        init();

    NS::String*                                 name() const;
    void                                        setName(const NS::String* name);

    NS::Array*                                  arguments() const;
    void                                        setArguments(const NS::Array* arguments);

    NS::Array*                                  controlDependencies() const;
    void                                        setControlDependencies(const NS::Array* controlDependencies);

    MTL::FunctionStitchingFunctionNode*         init(const NS::String* name, const NS::Array* arguments, const NS::Array* controlDependencies);
};

class FunctionStitchingGraph : public NS::Copying<FunctionStitchingGraph>
{
public:
    static class FunctionStitchingGraph* alloc();

    class FunctionStitchingGraph*        init();

    NS::String*                          functionName() const;
    void                                 setFunctionName(const NS::String* functionName);

    NS::Array*                           nodes() const;
    void                                 setNodes(const NS::Array* nodes);

    class FunctionStitchingFunctionNode* outputNode() const;
    void                                 setOutputNode(const class FunctionStitchingFunctionNode* outputNode);

    NS::Array*                           attributes() const;
    void                                 setAttributes(const NS::Array* attributes);

    MTL::FunctionStitchingGraph*         init(const NS::String* functionName, const NS::Array* nodes, const class FunctionStitchingFunctionNode* outputNode, const NS::Array* attributes);
};

class StitchedLibraryDescriptor : public NS::Copying<StitchedLibraryDescriptor>
{
public:
    static class StitchedLibraryDescriptor* alloc();

    class StitchedLibraryDescriptor*        init();

    NS::Array*                              functionGraphs() const;
    void                                    setFunctionGraphs(const NS::Array* functionGraphs);

    NS::Array*                              functions() const;
    void                                    setFunctions(const NS::Array* functions);
};

}

// static method: alloc
_MTL_INLINE MTL::FunctionStitchingAttributeAlwaysInline* MTL::FunctionStitchingAttributeAlwaysInline::alloc()
{
    return NS::Object::alloc<MTL::FunctionStitchingAttributeAlwaysInline>(_MTL_PRIVATE_CLS(MTLFunctionStitchingAttributeAlwaysInline));
}

// method: init
_MTL_INLINE MTL::FunctionStitchingAttributeAlwaysInline* MTL::FunctionStitchingAttributeAlwaysInline::init()
{
    return NS::Object::init<MTL::FunctionStitchingAttributeAlwaysInline>();
}

// static method: alloc
_MTL_INLINE MTL::FunctionStitchingInputNode* MTL::FunctionStitchingInputNode::alloc()
{
    return NS::Object::alloc<MTL::FunctionStitchingInputNode>(_MTL_PRIVATE_CLS(MTLFunctionStitchingInputNode));
}

// method: init
_MTL_INLINE MTL::FunctionStitchingInputNode* MTL::FunctionStitchingInputNode::init()
{
    return NS::Object::init<MTL::FunctionStitchingInputNode>();
}

// property: argumentIndex
_MTL_INLINE NS::UInteger MTL::FunctionStitchingInputNode::argumentIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(argumentIndex));
}

_MTL_INLINE void MTL::FunctionStitchingInputNode::setArgumentIndex(NS::UInteger argumentIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setArgumentIndex_), argumentIndex);
}

// method: initWithArgumentIndex:
_MTL_INLINE MTL::FunctionStitchingInputNode* MTL::FunctionStitchingInputNode::init(NS::UInteger argument)
{
    return Object::sendMessage<MTL::FunctionStitchingInputNode*>(this, _MTL_PRIVATE_SEL(initWithArgumentIndex_), argument);
}

// static method: alloc
_MTL_INLINE MTL::FunctionStitchingFunctionNode* MTL::FunctionStitchingFunctionNode::alloc()
{
    return NS::Object::alloc<MTL::FunctionStitchingFunctionNode>(_MTL_PRIVATE_CLS(MTLFunctionStitchingFunctionNode));
}

// method: init
_MTL_INLINE MTL::FunctionStitchingFunctionNode* MTL::FunctionStitchingFunctionNode::init()
{
    return NS::Object::init<MTL::FunctionStitchingFunctionNode>();
}

// property: name
_MTL_INLINE NS::String* MTL::FunctionStitchingFunctionNode::name() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(name));
}

_MTL_INLINE void MTL::FunctionStitchingFunctionNode::setName(const NS::String* name)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setName_), name);
}

// property: arguments
_MTL_INLINE NS::Array* MTL::FunctionStitchingFunctionNode::arguments() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(arguments));
}

_MTL_INLINE void MTL::FunctionStitchingFunctionNode::setArguments(const NS::Array* arguments)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setArguments_), arguments);
}

// property: controlDependencies
_MTL_INLINE NS::Array* MTL::FunctionStitchingFunctionNode::controlDependencies() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(controlDependencies));
}

_MTL_INLINE void MTL::FunctionStitchingFunctionNode::setControlDependencies(const NS::Array* controlDependencies)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setControlDependencies_), controlDependencies);
}

// method: initWithName:arguments:controlDependencies:
_MTL_INLINE MTL::FunctionStitchingFunctionNode* MTL::FunctionStitchingFunctionNode::init(const NS::String* name, const NS::Array* arguments, const NS::Array* controlDependencies)
{
    return Object::sendMessage<MTL::FunctionStitchingFunctionNode*>(this, _MTL_PRIVATE_SEL(initWithName_arguments_controlDependencies_), name, arguments, controlDependencies);
}

// static method: alloc
_MTL_INLINE MTL::FunctionStitchingGraph* MTL::FunctionStitchingGraph::alloc()
{
    return NS::Object::alloc<MTL::FunctionStitchingGraph>(_MTL_PRIVATE_CLS(MTLFunctionStitchingGraph));
}

// method: init
_MTL_INLINE MTL::FunctionStitchingGraph* MTL::FunctionStitchingGraph::init()
{
    return NS::Object::init<MTL::FunctionStitchingGraph>();
}

// property: functionName
_MTL_INLINE NS::String* MTL::FunctionStitchingGraph::functionName() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(functionName));
}

_MTL_INLINE void MTL::FunctionStitchingGraph::setFunctionName(const NS::String* functionName)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFunctionName_), functionName);
}

// property: nodes
_MTL_INLINE NS::Array* MTL::FunctionStitchingGraph::nodes() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(nodes));
}

_MTL_INLINE void MTL::FunctionStitchingGraph::setNodes(const NS::Array* nodes)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setNodes_), nodes);
}

// property: outputNode
_MTL_INLINE MTL::FunctionStitchingFunctionNode* MTL::FunctionStitchingGraph::outputNode() const
{
    return Object::sendMessage<MTL::FunctionStitchingFunctionNode*>(this, _MTL_PRIVATE_SEL(outputNode));
}

_MTL_INLINE void MTL::FunctionStitchingGraph::setOutputNode(const MTL::FunctionStitchingFunctionNode* outputNode)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setOutputNode_), outputNode);
}

// property: attributes
_MTL_INLINE NS::Array* MTL::FunctionStitchingGraph::attributes() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(attributes));
}

_MTL_INLINE void MTL::FunctionStitchingGraph::setAttributes(const NS::Array* attributes)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setAttributes_), attributes);
}

// method: initWithFunctionName:nodes:outputNode:attributes:
_MTL_INLINE MTL::FunctionStitchingGraph* MTL::FunctionStitchingGraph::init(const NS::String* functionName, const NS::Array* nodes, const MTL::FunctionStitchingFunctionNode* outputNode, const NS::Array* attributes)
{
    return Object::sendMessage<MTL::FunctionStitchingGraph*>(this, _MTL_PRIVATE_SEL(initWithFunctionName_nodes_outputNode_attributes_), functionName, nodes, outputNode, attributes);
}

// static method: alloc
_MTL_INLINE MTL::StitchedLibraryDescriptor* MTL::StitchedLibraryDescriptor::alloc()
{
    return NS::Object::alloc<MTL::StitchedLibraryDescriptor>(_MTL_PRIVATE_CLS(MTLStitchedLibraryDescriptor));
}

// method: init
_MTL_INLINE MTL::StitchedLibraryDescriptor* MTL::StitchedLibraryDescriptor::init()
{
    return NS::Object::init<MTL::StitchedLibraryDescriptor>();
}

// property: functionGraphs
_MTL_INLINE NS::Array* MTL::StitchedLibraryDescriptor::functionGraphs() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(functionGraphs));
}

_MTL_INLINE void MTL::StitchedLibraryDescriptor::setFunctionGraphs(const NS::Array* functionGraphs)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFunctionGraphs_), functionGraphs);
}

// property: functions
_MTL_INLINE NS::Array* MTL::StitchedLibraryDescriptor::functions() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(functions));
}

_MTL_INLINE void MTL::StitchedLibraryDescriptor::setFunctions(const NS::Array* functions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFunctions_), functions);
}
