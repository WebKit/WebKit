//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/BuiltInFunctionEmulator.h"

#include "compiler/SymbolTable.h"

namespace {

// we use macros here instead of function definitions to work around more GLSL
// compiler bugs, in particular on NVIDIA hardware on Mac OSX. Macros are
// problematic because if the argument has side-effects they will be repeatedly
// evaluated. This is unlikely to show up in real shaders, but is something to
// consider.
const char* kFunctionEmulationVertexSource[] = {
    "#error no emulation for atan(float, float)",
    "vec2 webgl_atan_emu(vec2 y, vec2 x) { return vec2(atan(y[0], x[0]), atan(y[1], x[1])); }",
    "vec3 webgl_atan_emu(vec3 y, vec3 x) { return vec3(atan(y[0], x[0]), atan(y[1], x[1]), atan(y[2], x[2])); }",
    "vec4 webgl_atan_emu(vec4 y, vec4 x) { return vec4(atan(y[0], x[0]), atan(y[1], x[1]), atan(y[2], x[2]), atan(y[3], x[3])); }",

    "#error no emulation for cos(float)",
    "#error no emulation for cos(vec2)",
    "#error no emulation for cos(vec3)",
    "#error no emulation for cos(vec4)",

    "#define webgl_distance_emu(x, y) ((x) >= (y) ? (x) - (y) : (y) - (x))",
    "#error no emulation for distance(vec2, vec2)",
    "#error no emulation for distance(vec3, vec3)",
    "#error no emulation for distance(vec4, vec4)",

    "#define webgl_dot_emu(x, y) ((x) * (y))",
    "#error no emulation for dot(vec2, vec2)",
    "#error no emulation for dot(vec3, vec3)",
    "#error no emulation for dot(vec4, vec4)",

    "#define webgl_length_emu(x) ((x) >= 0.0 ? (x) : -(x))",
    "#error no emulation for length(vec2)",
    "#error no emulation for length(vec3)",
    "#error no emulation for length(vec4)",

    "#error no emulation for mod(float, float)",
    "vec2 webgl_mod_emu(vec2 x, vec2 y) { return vec2(mod(x[0], y[0]), mod(x[1], y[1])); }",
    "vec3 webgl_mod_emu(vec3 x, vec3 y) { return vec3(mod(x[0], y[0]), mod(x[1], y[1]), mod(x[2], y[2])); }",
    "vec4 webgl_mod_emu(vec4 x, vec4 y) { return vec4(mod(x[0], y[0]), mod(x[1], y[1]), mod(x[2], y[2]), mod(x[3], y[3])); }",

    "#define webgl_normalize_emu(x) ((x) == 0.0 ? 0.0 : ((x) > 0.0 ? 1.0 : -1.0))",
    "#error no emulation for normalize(vec2)",
    "#error no emulation for normalize(vec3)",
    "#error no emulation for normalize(vec4)",

    "#define webgl_reflect_emu(I, N) ((I) - 2.0 * (N) * (I) * (N))",
    "#error no emulation for reflect(vec2, vec2)",
    "#error no emulation for reflect(vec3, vec3)",
    "#error no emulation for reflect(vec4, vec4)"
};

const char* kFunctionEmulationFragmentSource[] = {
    "#error no emulation for atan(float, float)",
    "#error no emulation for atan(vec2, vec2)",
    "#error no emulation for atan(vec3, vec3)",
    "#error no emulation for atan(vec4, vec4)",

    "webgl_emu_precision float webgl_cos_emu(webgl_emu_precision float a) { return cos(a); }",
    "webgl_emu_precision vec2 webgl_cos_emu(webgl_emu_precision vec2 a) { return cos(a); }",
    "webgl_emu_precision vec3 webgl_cos_emu(webgl_emu_precision vec3 a) { return cos(a); }",
    "webgl_emu_precision vec4 webgl_cos_emu(webgl_emu_precision vec4 a) { return cos(a); }",

    "#error no emulation for distance(float, float)",
    "#error no emulation for distance(vec2, vec2)",
    "#error no emulation for distance(vec3, vec3)",
    "#error no emulation for distance(vec4, vec4)",

    "#error no emulation for dot(float, float)",
    "#error no emulation for dot(vec2, vec2)",
    "#error no emulation for dot(vec3, vec3)",
    "#error no emulation for dot(vec4, vec4)",

    "#error no emulation for length(float)",
    "#error no emulation for length(vec2)",
    "#error no emulation for length(vec3)",
    "#error no emulation for length(vec4)",

    "#error no emulation for mod(float, float)",
    "#error no emulation for mod(vec2, vec2)",
    "#error no emulation for mod(vec3, vec3)",
    "#error no emulation for mod(vec4, vec4)",

    "#error no emulation for normalize(float)",
    "#error no emulation for normalize(vec2)",
    "#error no emulation for normalize(vec3)",
    "#error no emulation for normalize(vec4)",

    "#error no emulation for reflect(float, float)",
    "#error no emulation for reflect(vec2, vec2)",
    "#error no emulation for reflect(vec3, vec3)",
    "#error no emulation for reflect(vec4, vec4)"
};

const bool kFunctionEmulationVertexMask[] = {
#if defined(__APPLE__)
    // Work around ATI driver bugs in Mac.
    false, // TFunctionAtan1_1
    false, // TFunctionAtan2_2
    false, // TFunctionAtan3_3
    false, // TFunctionAtan4_4
    false, // TFunctionCos1
    false, // TFunctionCos2
    false, // TFunctionCos3
    false, // TFunctionCos4
    true,  // TFunctionDistance1_1
    false, // TFunctionDistance2_2
    false, // TFunctionDistance3_3
    false, // TFunctionDistance4_4
    true,  // TFunctionDot1_1
    false, // TFunctionDot2_2
    false, // TFunctionDot3_3
    false, // TFunctionDot4_4
    true,  // TFunctionLength1
    false, // TFunctionLength2
    false, // TFunctionLength3
    false, // TFunctionLength4
    false, // TFunctionMod1_1
    false, // TFunctionMod2_2
    false, // TFunctionMod3_3
    false, // TFunctionMod4_4
    true,  // TFunctionNormalize1
    false, // TFunctionNormalize2
    false, // TFunctionNormalize3
    false, // TFunctionNormalize4
    true,  // TFunctionReflect1_1
    false, // TFunctionReflect2_2
    false, // TFunctionReflect3_3
    false, // TFunctionReflect4_4
#else
    // Work around D3D driver bug in Win.
    false, // TFunctionAtan1_1
    true,  // TFunctionAtan2_2
    true,  // TFunctionAtan3_3
    true,  // TFunctionAtan4_4
    false, // TFunctionCos1
    false, // TFunctionCos2
    false, // TFunctionCos3
    false, // TFunctionCos4
    false, // TFunctionDistance1_1
    false, // TFunctionDistance2_2
    false, // TFunctionDistance3_3
    false, // TFunctionDistance4_4
    false, // TFunctionDot1_1
    false, // TFunctionDot2_2
    false, // TFunctionDot3_3
    false, // TFunctionDot4_4
    false, // TFunctionLength1
    false, // TFunctionLength2
    false, // TFunctionLength3
    false, // TFunctionLength4
    false, // TFunctionMod1_1
    true,  // TFunctionMod2_2
    true,  // TFunctionMod3_3
    true,  // TFunctionMod4_4
    false, // TFunctionNormalize1
    false, // TFunctionNormalize2
    false, // TFunctionNormalize3
    false, // TFunctionNormalize4
    false, // TFunctionReflect1_1
    false, // TFunctionReflect2_2
    false, // TFunctionReflect3_3
    false, // TFunctionReflect4_4
#endif
    false  // TFunctionUnknown
};

const bool kFunctionEmulationFragmentMask[] = {
    false, // TFunctionAtan1_1
    false, // TFunctionAtan2_2
    false, // TFunctionAtan3_3
    false, // TFunctionAtan4_4
#if defined(__APPLE__)
    // Work around a ATI driver bug in Mac that causes crashes.
    true,  // TFunctionCos1
    true,  // TFunctionCos2
    true,  // TFunctionCos3
    true,  // TFunctionCos4
#else
    false, // TFunctionCos1
    false, // TFunctionCos2
    false, // TFunctionCos3
    false, // TFunctionCos4
#endif
    false, // TFunctionDistance1_1
    false, // TFunctionDistance2_2
    false, // TFunctionDistance3_3
    false, // TFunctionDistance4_4
    false, // TFunctionDot1_1
    false, // TFunctionDot2_2
    false, // TFunctionDot3_3
    false, // TFunctionDot4_4
    false, // TFunctionLength1
    false, // TFunctionLength2
    false, // TFunctionLength3
    false, // TFunctionLength4
    false, // TFunctionMod1_1
    false, // TFunctionMod2_2
    false, // TFunctionMod3_3
    false, // TFunctionMod4_4
    false, // TFunctionNormalize1
    false, // TFunctionNormalize2
    false, // TFunctionNormalize3
    false, // TFunctionNormalize4
    false, // TFunctionReflect1_1
    false, // TFunctionReflect2_2
    false, // TFunctionReflect3_3
    false, // TFunctionReflect4_4
    false  // TFunctionUnknown
};

class BuiltInFunctionEmulationMarker : public TIntermTraverser {
public:
    BuiltInFunctionEmulationMarker(BuiltInFunctionEmulator& emulator)
        : mEmulator(emulator)
    {
    }

    virtual bool visitUnary(Visit visit, TIntermUnary* node)
    {
        if (visit == PreVisit) {
            bool needToEmulate = mEmulator.SetFunctionCalled(
                node->getOp(), node->getOperand()->getType());
            if (needToEmulate)
                node->setUseEmulatedFunction();
        }
        return true;
    }

    virtual bool visitAggregate(Visit visit, TIntermAggregate* node)
    {
        if (visit == PreVisit) {
            // Here we handle all the built-in functions instead of the ones we
            // currently identified as problematic.
            switch (node->getOp()) {
                case EOpLessThan:
                case EOpGreaterThan:
                case EOpLessThanEqual:
                case EOpGreaterThanEqual:
                case EOpVectorEqual:
                case EOpVectorNotEqual:
                case EOpMod:
                case EOpPow:
                case EOpAtan:
                case EOpMin:
                case EOpMax:
                case EOpClamp:
                case EOpMix:
                case EOpStep:
                case EOpSmoothStep:
                case EOpDistance:
                case EOpDot:
                case EOpCross:
                case EOpFaceForward:
                case EOpReflect:
                case EOpRefract:
                case EOpMul:
                    break;
                default:
                    return true;
            };
            const TIntermSequence& sequence = node->getSequence();
            // Right now we only handle built-in functions with two parameters.
            if (sequence.size() != 2)
                return true;
            TIntermTyped* param1 = sequence[0]->getAsTyped();
            TIntermTyped* param2 = sequence[1]->getAsTyped();
            if (!param1 || !param2)
                return true;
            bool needToEmulate = mEmulator.SetFunctionCalled(
                node->getOp(), param1->getType(), param2->getType());
            if (needToEmulate)
                node->setUseEmulatedFunction();
        }
        return true;
    }

private:
    BuiltInFunctionEmulator& mEmulator;
};

}  // anonymous namepsace

BuiltInFunctionEmulator::BuiltInFunctionEmulator(ShShaderType shaderType)
{
    if (shaderType == SH_FRAGMENT_SHADER) {
        mFunctionMask = kFunctionEmulationFragmentMask;
        mFunctionSource = kFunctionEmulationFragmentSource;
    } else {
        mFunctionMask = kFunctionEmulationVertexMask;
        mFunctionSource = kFunctionEmulationVertexSource;
    }
}

bool BuiltInFunctionEmulator::SetFunctionCalled(
    TOperator op, const TType& param)
{
    TBuiltInFunction function = IdentifyFunction(op, param);
    return SetFunctionCalled(function);
}

bool BuiltInFunctionEmulator::SetFunctionCalled(
    TOperator op, const TType& param1, const TType& param2)
{
    TBuiltInFunction function = IdentifyFunction(op, param1, param2);
    return SetFunctionCalled(function);
}

bool BuiltInFunctionEmulator::SetFunctionCalled(
    BuiltInFunctionEmulator::TBuiltInFunction function) {
    if (function == TFunctionUnknown || mFunctionMask[function] == false)
        return false;
    for (size_t i = 0; i < mFunctions.size(); ++i) {
        if (mFunctions[i] == function)
            return true;
    }
    mFunctions.push_back(function);
    return true;
}

void BuiltInFunctionEmulator::OutputEmulatedFunctionDefinition(
    TInfoSinkBase& out, bool withPrecision) const
{
    if (mFunctions.size() == 0)
        return;
    out << "// BEGIN: Generated code for built-in function emulation\n\n";
    if (withPrecision) {
        out << "#if defined(GL_FRAGMENT_PRECISION_HIGH)\n"
            << "#define webgl_emu_precision highp\n"
            << "#else\n"
            << "#define webgl_emu_precision mediump\n"
            << "#endif\n\n";
    } else {
        out << "#define webgl_emu_precision\n\n";
    }
    for (size_t i = 0; i < mFunctions.size(); ++i) {
        out << mFunctionSource[mFunctions[i]] << "\n\n";
    }
    out << "// END: Generated code for built-in function emulation\n\n";
}

BuiltInFunctionEmulator::TBuiltInFunction
BuiltInFunctionEmulator::IdentifyFunction(
    TOperator op, const TType& param)
{
    if (param.getNominalSize() > 4)
        return TFunctionUnknown;
    unsigned int function = TFunctionUnknown;
    switch (op) {
        case EOpCos:
            function = TFunctionCos1;
            break;
        case EOpLength:
            function = TFunctionLength1;
            break;
        case EOpNormalize:
            function = TFunctionNormalize1;
            break;
        default:
            break;
    }
    if (function == TFunctionUnknown)
        return TFunctionUnknown;
    if (param.isVector())
        function += param.getNominalSize() - 1;
    return static_cast<TBuiltInFunction>(function);
}

BuiltInFunctionEmulator::TBuiltInFunction
BuiltInFunctionEmulator::IdentifyFunction(
    TOperator op, const TType& param1, const TType& param2)
{
    // Right now for all the emulated functions with two parameters, the two
    // parameters have the same type.
    if (param1.isVector() != param2.isVector() ||
        param1.getNominalSize() != param2.getNominalSize() ||
        param1.getNominalSize() > 4)
        return TFunctionUnknown;

    unsigned int function = TFunctionUnknown;
    switch (op) {
        case EOpAtan:
            function = TFunctionAtan1_1;
            break;
        case EOpDistance:
            function = TFunctionDistance1_1;
            break;
        case EOpDot:
            function = TFunctionDot1_1;
            break;
        case EOpMod:
            function = TFunctionMod1_1;
            break;
        case EOpReflect:
            function = TFunctionReflect1_1;
            break;
        default:
            break;
    }
    if (function == TFunctionUnknown)
        return TFunctionUnknown;
    if (param1.isVector())
        function += param1.getNominalSize() - 1;
    return static_cast<TBuiltInFunction>(function);
}

void BuiltInFunctionEmulator::MarkBuiltInFunctionsForEmulation(
    TIntermNode* root)
{
    ASSERT(root);

    BuiltInFunctionEmulationMarker marker(*this);
    root->traverse(&marker);
}

void BuiltInFunctionEmulator::Cleanup()
{
    mFunctions.clear();
}

//static
TString BuiltInFunctionEmulator::GetEmulatedFunctionName(
    const TString& name)
{
    ASSERT(name[name.length() - 1] == '(');
    return "webgl_" + name.substr(0, name.length() - 1) + "_emu(";
}

