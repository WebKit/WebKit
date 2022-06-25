//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLLibrary.hpp
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

#include "MTLArgument.hpp"
#include "MTLFunctionDescriptor.hpp"
#include "MTLLibrary.hpp"
#include <functional>

namespace MTL
{
_MTL_ENUM(NS::UInteger, PatchType) {
    PatchTypeNone = 0,
    PatchTypeTriangle = 1,
    PatchTypeQuad = 2,
};

class VertexAttribute : public NS::Referencing<VertexAttribute>
{
public:
    static class VertexAttribute* alloc();

    class VertexAttribute*        init();

    NS::String*                   name() const;

    NS::UInteger                  attributeIndex() const;

    MTL::DataType                 attributeType() const;

    bool                          active() const;

    bool                          patchData() const;

    bool                          patchControlPointData() const;
};

class Attribute : public NS::Referencing<Attribute>
{
public:
    static class Attribute* alloc();

    class Attribute*        init();

    NS::String*             name() const;

    NS::UInteger            attributeIndex() const;

    MTL::DataType           attributeType() const;

    bool                    active() const;

    bool                    patchData() const;

    bool                    patchControlPointData() const;
};

_MTL_ENUM(NS::UInteger, FunctionType) {
    FunctionTypeVertex = 1,
    FunctionTypeFragment = 2,
    FunctionTypeKernel = 3,
    FunctionTypeVisible = 5,
    FunctionTypeIntersection = 6,
};

class FunctionConstant : public NS::Referencing<FunctionConstant>
{
public:
    static class FunctionConstant* alloc();

    class FunctionConstant*        init();

    NS::String*                    name() const;

    MTL::DataType                  type() const;

    NS::UInteger                   index() const;

    bool                           required() const;
};

using AutoreleasedArgument = class Argument*;

class Function : public NS::Referencing<Function>
{
public:
    NS::String*            label() const;
    void                   setLabel(const NS::String* label);

    class Device*          device() const;

    MTL::FunctionType      functionType() const;

    MTL::PatchType         patchType() const;

    NS::Integer            patchControlPointCount() const;

    NS::Array*             vertexAttributes() const;

    NS::Array*             stageInputAttributes() const;

    NS::String*            name() const;

    NS::Dictionary*        functionConstantsDictionary() const;

    class ArgumentEncoder* newArgumentEncoder(NS::UInteger bufferIndex);

    class ArgumentEncoder* newArgumentEncoder(NS::UInteger bufferIndex, const MTL::AutoreleasedArgument* reflection);

    MTL::FunctionOptions   options() const;
};

_MTL_ENUM(NS::UInteger, LanguageVersion) {
    LanguageVersion1_0 = 65536,
    LanguageVersion1_1 = 65537,
    LanguageVersion1_2 = 65538,
    LanguageVersion2_0 = 131072,
    LanguageVersion2_1 = 131073,
    LanguageVersion2_2 = 131074,
    LanguageVersion2_3 = 131075,
    LanguageVersion2_4 = 131076,
};

_MTL_ENUM(NS::Integer, LibraryType) {
    LibraryTypeExecutable = 0,
    LibraryTypeDynamic = 1,
};

class CompileOptions : public NS::Copying<CompileOptions>
{
public:
    static class CompileOptions* alloc();

    class CompileOptions*        init();

    NS::Dictionary*              preprocessorMacros() const;
    void                         setPreprocessorMacros(const NS::Dictionary* preprocessorMacros);

    bool                         fastMathEnabled() const;
    void                         setFastMathEnabled(bool fastMathEnabled);

    MTL::LanguageVersion         languageVersion() const;
    void                         setLanguageVersion(MTL::LanguageVersion languageVersion);

    MTL::LibraryType             libraryType() const;
    void                         setLibraryType(MTL::LibraryType libraryType);

    NS::String*                  installName() const;
    void                         setInstallName(const NS::String* installName);

    NS::Array*                   libraries() const;
    void                         setLibraries(const NS::Array* libraries);

    bool                         preserveInvariance() const;
    void                         setPreserveInvariance(bool preserveInvariance);
};

_MTL_ENUM(NS::UInteger, LibraryError) {
    LibraryErrorUnsupported = 1,
    LibraryErrorCompileFailure = 3,
    LibraryErrorCompileWarning = 4,
    LibraryErrorFunctionNotFound = 5,
    LibraryErrorFileNotFound = 6,
};

class Library : public NS::Referencing<Library>
{
public:
    void             newFunction(const NS::String* pFunctionName, const class FunctionConstantValues* pConstantValues, const std::function<void(Function* pFunction, NS::Error* pError)>& completionHandler);

    void             newFunction(const class FunctionDescriptor* pDescriptor, const std::function<void(Function* pFunction, NS::Error* pError)>& completionHandler);

    void             newIntersectionFunction(const class IntersectionFunctionDescriptor* pDescriptor, const std::function<void(Function* pFunction, NS::Error* pError)>& completionHandler);

    NS::String*      label() const;
    void             setLabel(const NS::String* label);

    class Device*    device() const;

    class Function*  newFunction(const NS::String* functionName);

    class Function*  newFunction(const NS::String* name, const class FunctionConstantValues* constantValues, NS::Error** error);

    void             newFunction(const NS::String* name, const class FunctionConstantValues* constantValues, void (^completionHandler)(MTL::Function*, NS::Error*));

    void             newFunction(const class FunctionDescriptor* descriptor, void (^completionHandler)(MTL::Function*, NS::Error*));

    class Function*  newFunction(const class FunctionDescriptor* descriptor, NS::Error** error);

    void             newIntersectionFunction(const class IntersectionFunctionDescriptor* descriptor, void (^completionHandler)(MTL::Function*, NS::Error*));

    class Function*  newIntersectionFunction(const class IntersectionFunctionDescriptor* descriptor, NS::Error** error);

    NS::Array*       functionNames() const;

    MTL::LibraryType type() const;

    NS::String*      installName() const;
};

}

// static method: alloc
_MTL_INLINE MTL::VertexAttribute* MTL::VertexAttribute::alloc()
{
    return NS::Object::alloc<MTL::VertexAttribute>(_MTL_PRIVATE_CLS(MTLVertexAttribute));
}

// method: init
_MTL_INLINE MTL::VertexAttribute* MTL::VertexAttribute::init()
{
    return NS::Object::init<MTL::VertexAttribute>();
}

// property: name
_MTL_INLINE NS::String* MTL::VertexAttribute::name() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(name));
}

// property: attributeIndex
_MTL_INLINE NS::UInteger MTL::VertexAttribute::attributeIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(attributeIndex));
}

// property: attributeType
_MTL_INLINE MTL::DataType MTL::VertexAttribute::attributeType() const
{
    return Object::sendMessage<MTL::DataType>(this, _MTL_PRIVATE_SEL(attributeType));
}

// property: active
_MTL_INLINE bool MTL::VertexAttribute::active() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isActive));
}

// property: patchData
_MTL_INLINE bool MTL::VertexAttribute::patchData() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isPatchData));
}

// property: patchControlPointData
_MTL_INLINE bool MTL::VertexAttribute::patchControlPointData() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isPatchControlPointData));
}

// static method: alloc
_MTL_INLINE MTL::Attribute* MTL::Attribute::alloc()
{
    return NS::Object::alloc<MTL::Attribute>(_MTL_PRIVATE_CLS(MTLAttribute));
}

// method: init
_MTL_INLINE MTL::Attribute* MTL::Attribute::init()
{
    return NS::Object::init<MTL::Attribute>();
}

// property: name
_MTL_INLINE NS::String* MTL::Attribute::name() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(name));
}

// property: attributeIndex
_MTL_INLINE NS::UInteger MTL::Attribute::attributeIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(attributeIndex));
}

// property: attributeType
_MTL_INLINE MTL::DataType MTL::Attribute::attributeType() const
{
    return Object::sendMessage<MTL::DataType>(this, _MTL_PRIVATE_SEL(attributeType));
}

// property: active
_MTL_INLINE bool MTL::Attribute::active() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isActive));
}

// property: patchData
_MTL_INLINE bool MTL::Attribute::patchData() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isPatchData));
}

// property: patchControlPointData
_MTL_INLINE bool MTL::Attribute::patchControlPointData() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isPatchControlPointData));
}

// static method: alloc
_MTL_INLINE MTL::FunctionConstant* MTL::FunctionConstant::alloc()
{
    return NS::Object::alloc<MTL::FunctionConstant>(_MTL_PRIVATE_CLS(MTLFunctionConstant));
}

// method: init
_MTL_INLINE MTL::FunctionConstant* MTL::FunctionConstant::init()
{
    return NS::Object::init<MTL::FunctionConstant>();
}

// property: name
_MTL_INLINE NS::String* MTL::FunctionConstant::name() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(name));
}

// property: type
_MTL_INLINE MTL::DataType MTL::FunctionConstant::type() const
{
    return Object::sendMessage<MTL::DataType>(this, _MTL_PRIVATE_SEL(type));
}

// property: index
_MTL_INLINE NS::UInteger MTL::FunctionConstant::index() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(index));
}

// property: required
_MTL_INLINE bool MTL::FunctionConstant::required() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(required));
}

// property: label
_MTL_INLINE NS::String* MTL::Function::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::Function::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// property: device
_MTL_INLINE MTL::Device* MTL::Function::device() const
{
    return Object::sendMessage<MTL::Device*>(this, _MTL_PRIVATE_SEL(device));
}

// property: functionType
_MTL_INLINE MTL::FunctionType MTL::Function::functionType() const
{
    return Object::sendMessage<MTL::FunctionType>(this, _MTL_PRIVATE_SEL(functionType));
}

// property: patchType
_MTL_INLINE MTL::PatchType MTL::Function::patchType() const
{
    return Object::sendMessage<MTL::PatchType>(this, _MTL_PRIVATE_SEL(patchType));
}

// property: patchControlPointCount
_MTL_INLINE NS::Integer MTL::Function::patchControlPointCount() const
{
    return Object::sendMessage<NS::Integer>(this, _MTL_PRIVATE_SEL(patchControlPointCount));
}

// property: vertexAttributes
_MTL_INLINE NS::Array* MTL::Function::vertexAttributes() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(vertexAttributes));
}

// property: stageInputAttributes
_MTL_INLINE NS::Array* MTL::Function::stageInputAttributes() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(stageInputAttributes));
}

// property: name
_MTL_INLINE NS::String* MTL::Function::name() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(name));
}

// property: functionConstantsDictionary
_MTL_INLINE NS::Dictionary* MTL::Function::functionConstantsDictionary() const
{
    return Object::sendMessage<NS::Dictionary*>(this, _MTL_PRIVATE_SEL(functionConstantsDictionary));
}

// method: newArgumentEncoderWithBufferIndex:
_MTL_INLINE MTL::ArgumentEncoder* MTL::Function::newArgumentEncoder(NS::UInteger bufferIndex)
{
    return Object::sendMessage<MTL::ArgumentEncoder*>(this, _MTL_PRIVATE_SEL(newArgumentEncoderWithBufferIndex_), bufferIndex);
}

// method: newArgumentEncoderWithBufferIndex:reflection:
_MTL_INLINE MTL::ArgumentEncoder* MTL::Function::newArgumentEncoder(NS::UInteger bufferIndex, const MTL::AutoreleasedArgument* reflection)
{
    return Object::sendMessage<MTL::ArgumentEncoder*>(this, _MTL_PRIVATE_SEL(newArgumentEncoderWithBufferIndex_reflection_), bufferIndex, reflection);
}

// property: options
_MTL_INLINE MTL::FunctionOptions MTL::Function::options() const
{
    return Object::sendMessage<MTL::FunctionOptions>(this, _MTL_PRIVATE_SEL(options));
}

// static method: alloc
_MTL_INLINE MTL::CompileOptions* MTL::CompileOptions::alloc()
{
    return NS::Object::alloc<MTL::CompileOptions>(_MTL_PRIVATE_CLS(MTLCompileOptions));
}

// method: init
_MTL_INLINE MTL::CompileOptions* MTL::CompileOptions::init()
{
    return NS::Object::init<MTL::CompileOptions>();
}

// property: preprocessorMacros
_MTL_INLINE NS::Dictionary* MTL::CompileOptions::preprocessorMacros() const
{
    return Object::sendMessage<NS::Dictionary*>(this, _MTL_PRIVATE_SEL(preprocessorMacros));
}

_MTL_INLINE void MTL::CompileOptions::setPreprocessorMacros(const NS::Dictionary* preprocessorMacros)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setPreprocessorMacros_), preprocessorMacros);
}

// property: fastMathEnabled
_MTL_INLINE bool MTL::CompileOptions::fastMathEnabled() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(fastMathEnabled));
}

_MTL_INLINE void MTL::CompileOptions::setFastMathEnabled(bool fastMathEnabled)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFastMathEnabled_), fastMathEnabled);
}

// property: languageVersion
_MTL_INLINE MTL::LanguageVersion MTL::CompileOptions::languageVersion() const
{
    return Object::sendMessage<MTL::LanguageVersion>(this, _MTL_PRIVATE_SEL(languageVersion));
}

_MTL_INLINE void MTL::CompileOptions::setLanguageVersion(MTL::LanguageVersion languageVersion)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLanguageVersion_), languageVersion);
}

// property: libraryType
_MTL_INLINE MTL::LibraryType MTL::CompileOptions::libraryType() const
{
    return Object::sendMessage<MTL::LibraryType>(this, _MTL_PRIVATE_SEL(libraryType));
}

_MTL_INLINE void MTL::CompileOptions::setLibraryType(MTL::LibraryType libraryType)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLibraryType_), libraryType);
}

// property: installName
_MTL_INLINE NS::String* MTL::CompileOptions::installName() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(installName));
}

_MTL_INLINE void MTL::CompileOptions::setInstallName(const NS::String* installName)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setInstallName_), installName);
}

// property: libraries
_MTL_INLINE NS::Array* MTL::CompileOptions::libraries() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(libraries));
}

_MTL_INLINE void MTL::CompileOptions::setLibraries(const NS::Array* libraries)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLibraries_), libraries);
}

// property: preserveInvariance
_MTL_INLINE bool MTL::CompileOptions::preserveInvariance() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(preserveInvariance));
}

_MTL_INLINE void MTL::CompileOptions::setPreserveInvariance(bool preserveInvariance)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setPreserveInvariance_), preserveInvariance);
}

_MTL_INLINE void MTL::Library::newFunction(const NS::String* pFunctionName, const FunctionConstantValues* pConstantValues, const std::function<void(Function* pFunction, NS::Error* pError)>& completionHandler)
{
    __block std::function<void(Function * pFunction, NS::Error * pError)> blockCompletionHandler = completionHandler;

    newFunction(pFunctionName, pConstantValues, ^(Function* pFunction, NS::Error* pError) { blockCompletionHandler(pFunction, pError); });
}

_MTL_INLINE void MTL::Library::newFunction(const FunctionDescriptor* pDescriptor, const std::function<void(Function* pFunction, NS::Error* pError)>& completionHandler)
{
    __block std::function<void(Function * pFunction, NS::Error * pError)> blockCompletionHandler = completionHandler;

    newFunction(pDescriptor, ^(Function* pFunction, NS::Error* pError) { blockCompletionHandler(pFunction, pError); });
}

_MTL_INLINE void MTL::Library::newIntersectionFunction(const IntersectionFunctionDescriptor* pDescriptor, const std::function<void(Function* pFunction, NS::Error* pError)>& completionHandler)
{
    __block std::function<void(Function * pFunction, NS::Error * pError)> blockCompletionHandler = completionHandler;

    newIntersectionFunction(pDescriptor, ^(Function* pFunction, NS::Error* pError) { blockCompletionHandler(pFunction, pError); });
}

// property: label
_MTL_INLINE NS::String* MTL::Library::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::Library::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// property: device
_MTL_INLINE MTL::Device* MTL::Library::device() const
{
    return Object::sendMessage<MTL::Device*>(this, _MTL_PRIVATE_SEL(device));
}

// method: newFunctionWithName:
_MTL_INLINE MTL::Function* MTL::Library::newFunction(const NS::String* functionName)
{
    return Object::sendMessage<MTL::Function*>(this, _MTL_PRIVATE_SEL(newFunctionWithName_), functionName);
}

// method: newFunctionWithName:constantValues:error:
_MTL_INLINE MTL::Function* MTL::Library::newFunction(const NS::String* name, const MTL::FunctionConstantValues* constantValues, NS::Error** error)
{
    return Object::sendMessage<MTL::Function*>(this, _MTL_PRIVATE_SEL(newFunctionWithName_constantValues_error_), name, constantValues, error);
}

// method: newFunctionWithName:constantValues:completionHandler:
_MTL_INLINE void MTL::Library::newFunction(const NS::String* name, const MTL::FunctionConstantValues* constantValues, void (^completionHandler)(MTL::Function*, NS::Error*))
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(newFunctionWithName_constantValues_completionHandler_), name, constantValues, completionHandler);
}

// method: newFunctionWithDescriptor:completionHandler:
_MTL_INLINE void MTL::Library::newFunction(const MTL::FunctionDescriptor* descriptor, void (^completionHandler)(MTL::Function*, NS::Error*))
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(newFunctionWithDescriptor_completionHandler_), descriptor, completionHandler);
}

// method: newFunctionWithDescriptor:error:
_MTL_INLINE MTL::Function* MTL::Library::newFunction(const MTL::FunctionDescriptor* descriptor, NS::Error** error)
{
    return Object::sendMessage<MTL::Function*>(this, _MTL_PRIVATE_SEL(newFunctionWithDescriptor_error_), descriptor, error);
}

// method: newIntersectionFunctionWithDescriptor:completionHandler:
_MTL_INLINE void MTL::Library::newIntersectionFunction(const MTL::IntersectionFunctionDescriptor* descriptor, void (^completionHandler)(MTL::Function*, NS::Error*))
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(newIntersectionFunctionWithDescriptor_completionHandler_), descriptor, completionHandler);
}

// method: newIntersectionFunctionWithDescriptor:error:
_MTL_INLINE MTL::Function* MTL::Library::newIntersectionFunction(const MTL::IntersectionFunctionDescriptor* descriptor, NS::Error** error)
{
    return Object::sendMessage<MTL::Function*>(this, _MTL_PRIVATE_SEL(newIntersectionFunctionWithDescriptor_error_), descriptor, error);
}

// property: functionNames
_MTL_INLINE NS::Array* MTL::Library::functionNames() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(functionNames));
}

// property: type
_MTL_INLINE MTL::LibraryType MTL::Library::type() const
{
    return Object::sendMessage<MTL::LibraryType>(this, _MTL_PRIVATE_SEL(type));
}

// property: installName
_MTL_INLINE NS::String* MTL::Library::installName() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(installName));
}
