//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_BUILTINFUNCTIONEMULATOR_H_
#define COMPILER_TRANSLATOR_BUILTINFUNCTIONEMULATOR_H_

#include "compiler/translator/InfoSink.h"
#include "compiler/translator/IntermNode.h"

//
// This class decides which built-in functions need to be replaced with the
// emulated ones.
// It can be used to work around driver bugs or implement functions that are
// not natively implemented on a specific platform.
//
class BuiltInFunctionEmulator
{
  public:
    BuiltInFunctionEmulator();

    void MarkBuiltInFunctionsForEmulation(TIntermNode* root);

    void Cleanup();

    // "name(" becomes "webgl_name_emu(".
    static TString GetEmulatedFunctionName(const TString& name);

    bool IsOutputEmpty() const;

    // Output function emulation definition. This should be before any other
    // shader source.
    void OutputEmulatedFunctions(TInfoSinkBase& out) const;

    // Add functions that need to be emulated.
    void addEmulatedFunction(TOperator op, const TType& param, const char* emulatedFunctionDefinition);
    void addEmulatedFunction(TOperator op, const TType& param1, const TType& param2, const char* emulatedFunctionDefinition);
    void addEmulatedFunction(TOperator op, const TType& param1, const TType& param2, const TType& param3, const char* emulatedFunctionDefinition);

  private:
    class BuiltInFunctionEmulationMarker;

    // Records that a function is called by the shader and might need to be
    // emulated. If the function is not in mEmulatedFunctions, this becomes a
    // no-op. Returns true if the function call needs to be replaced with an
    // emulated one.
    bool SetFunctionCalled(TOperator op, const TType& param);
    bool SetFunctionCalled(
        TOperator op, const TType& param1, const TType& param2);
    bool SetFunctionCalled(
        TOperator op, const TType& param1, const TType& param2, const TType& param3);

    class FunctionId {
      public:
        FunctionId(TOperator op, const TType& param);
        FunctionId(TOperator op, const TType& param1, const TType& param2);
        FunctionId(TOperator op, const TType& param1, const TType& param2, const TType& param3);

        bool operator==(const FunctionId& other) const;
        bool operator<(const FunctionId& other) const;
      private:
        TOperator mOp;
        TType mParam1;
        TType mParam2;
        TType mParam3;
    };

    bool SetFunctionCalled(const FunctionId& functionId);

    // Map from function id to emulated function definition
    std::map<FunctionId, std::string> mEmulatedFunctions;

    // Called function ids
    std::vector<FunctionId> mFunctions;
};

#endif  // COMPILER_TRANSLATOR_BUILTINFUNCTIONEMULATOR_H_
