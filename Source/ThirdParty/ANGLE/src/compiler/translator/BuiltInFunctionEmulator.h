//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_BUILTINFUNCTIONEMULATOR_H_
#define COMPILER_TRANSLATOR_BUILTINFUNCTIONEMULATOR_H_

#include "compiler/translator/InfoSink.h"
#include "compiler/translator/IntermNode.h"

namespace sh
{

//
// This class decides which built-in functions need to be replaced with the emulated ones. It can be
// used to work around driver bugs or implement functions that are not natively implemented on a
// specific platform.
//
class BuiltInFunctionEmulator
{
  public:
    BuiltInFunctionEmulator();

    void markBuiltInFunctionsForEmulation(TIntermNode *root);

    void cleanup();

    // "name" gets written as "webgl_name_emu".
    static void WriteEmulatedFunctionName(TInfoSinkBase &out, const char *name);

    bool isOutputEmpty() const;

    // Output function emulation definition. This should be before any other shader source.
    void outputEmulatedFunctions(TInfoSinkBase &out) const;

    class FunctionId
    {
      public:
        FunctionId();
        FunctionId(TOperator op, const TType *param);
        FunctionId(TOperator op, const TType *param1, const TType *param2);
        FunctionId(TOperator op, const TType *param1, const TType *param2, const TType *param3);
        FunctionId(TOperator op,
                   const TType *param1,
                   const TType *param2,
                   const TType *param3,
                   const TType *param4);

        FunctionId(const FunctionId &) = default;
        FunctionId &operator=(const FunctionId &) = default;

        bool operator==(const FunctionId &other) const;
        bool operator<(const FunctionId &other) const;

        FunctionId getCopy() const;

      private:
        TOperator mOp;

        // The memory that these TType objects use is freed by PoolAllocator. The
        // BuiltInFunctionEmulator's lifetime can extend until after the memory pool is freed, but
        // that's not an issue since this class never destructs these objects.
        const TType *mParam1;
        const TType *mParam2;
        const TType *mParam3;
        const TType *mParam4;
    };

    // Add functions that need to be emulated.
    FunctionId addEmulatedFunction(TOperator op,
                                   const TType *param,
                                   const char *emulatedFunctionDefinition);
    FunctionId addEmulatedFunction(TOperator op,
                                   const TType *param1,
                                   const TType *param2,
                                   const char *emulatedFunctionDefinition);
    FunctionId addEmulatedFunction(TOperator op,
                                   const TType *param1,
                                   const TType *param2,
                                   const TType *param3,
                                   const char *emulatedFunctionDefinition);
    FunctionId addEmulatedFunction(TOperator op,
                                   const TType *param1,
                                   const TType *param2,
                                   const TType *param3,
                                   const TType *param4,
                                   const char *emulatedFunctionDefinition);

    FunctionId addEmulatedFunctionWithDependency(FunctionId dependency,
                                                 TOperator op,
                                                 const TType *param1,
                                                 const TType *param2,
                                                 const char *emulatedFunctionDefinition);
    FunctionId addEmulatedFunctionWithDependency(FunctionId dependency,
                                                 TOperator op,
                                                 const TType *param1,
                                                 const TType *param2,
                                                 const TType *param3,
                                                 const TType *param4,
                                                 const char *emulatedFunctionDefinition);

  private:
    class BuiltInFunctionEmulationMarker;

    // Records that a function is called by the shader and might need to be emulated. If the
    // function is not in mEmulatedFunctions, this becomes a no-op. Returns true if the function
    // call needs to be replaced with an emulated one.
    bool setFunctionCalled(TOperator op, const TType &param);
    bool setFunctionCalled(TOperator op, const TType &param1, const TType &param2);
    bool setFunctionCalled(TOperator op,
                           const TType &param1,
                           const TType &param2,
                           const TType &param3);
    bool setFunctionCalled(TOperator op,
                           const TType &param1,
                           const TType &param2,
                           const TType &param3,
                           const TType &param4);

    bool setFunctionCalled(const FunctionId &functionId);

    // Map from function id to emulated function definition
    std::map<FunctionId, std::string> mEmulatedFunctions;

    // Map from dependent functions to their dependencies. This structure allows each function to
    // have at most one dependency.
    std::map<FunctionId, FunctionId> mFunctionDependencies;

    // Called function ids
    std::vector<FunctionId> mFunctions;
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_BUILTINFUNCTIONEMULATOR_H_
