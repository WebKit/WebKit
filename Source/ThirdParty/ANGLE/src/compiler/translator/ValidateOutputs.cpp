//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ValidateOutputs validates fragment shader outputs. It checks for conflicting locations,
// out-of-range locations, that locations are specified when using multiple outputs, and YUV output
// validity.

#include "compiler/translator/ValidateOutputs.h"

#include <set>

#include "compiler/translator/InfoSink.h"
#include "compiler/translator/IntermTraverse.h"
#include "compiler/translator/ParseContext.h"

namespace sh
{

namespace
{
void error(const TIntermSymbol &symbol, const char *reason, TDiagnostics *diagnostics)
{
    diagnostics->error(symbol.getLine(), reason, symbol.getSymbol().c_str());
}

class ValidateOutputsTraverser : public TIntermTraverser
{
  public:
    ValidateOutputsTraverser(const TExtensionBehavior &extBehavior, int maxDrawBuffers);

    void validate(TDiagnostics *diagnostics) const;

    void visitSymbol(TIntermSymbol *) override;

  private:
    int mMaxDrawBuffers;
    bool mAllowUnspecifiedOutputLocationResolution;
    bool mUsesFragDepth;

    typedef std::vector<TIntermSymbol *> OutputVector;
    OutputVector mOutputs;
    OutputVector mUnspecifiedLocationOutputs;
    OutputVector mYuvOutputs;
    std::set<std::string> mVisitedSymbols;
};

ValidateOutputsTraverser::ValidateOutputsTraverser(const TExtensionBehavior &extBehavior,
                                                   int maxDrawBuffers)
    : TIntermTraverser(true, false, false),
      mMaxDrawBuffers(maxDrawBuffers),
      mAllowUnspecifiedOutputLocationResolution(
          IsExtensionEnabled(extBehavior, TExtension::EXT_blend_func_extended)),
      mUsesFragDepth(false)
{
}

void ValidateOutputsTraverser::visitSymbol(TIntermSymbol *symbol)
{
    TString name         = symbol->getSymbol();
    TQualifier qualifier = symbol->getQualifier();

    if (mVisitedSymbols.count(name.c_str()) == 1)
        return;

    mVisitedSymbols.insert(name.c_str());

    if (qualifier == EvqFragmentOut)
    {
        if (symbol->getType().getLayoutQualifier().location != -1)
        {
            mOutputs.push_back(symbol);
        }
        else if (symbol->getType().getLayoutQualifier().yuv == true)
        {
            mYuvOutputs.push_back(symbol);
        }
        else
        {
            mUnspecifiedLocationOutputs.push_back(symbol);
        }
    }
    else if (qualifier == EvqFragDepth || qualifier == EvqFragDepthEXT)
    {
        mUsesFragDepth = true;
    }
}

void ValidateOutputsTraverser::validate(TDiagnostics *diagnostics) const
{
    ASSERT(diagnostics);
    OutputVector validOutputs(mMaxDrawBuffers);

    for (const auto &symbol : mOutputs)
    {
        const TType &type         = symbol->getType();
        ASSERT(!type.isArrayOfArrays());  // Disallowed in GLSL ES 3.10 section 4.3.6.
        const size_t elementCount =
            static_cast<size_t>(type.isArray() ? type.getOutermostArraySize() : 1u);
        const size_t location     = static_cast<size_t>(type.getLayoutQualifier().location);

        ASSERT(type.getLayoutQualifier().location != -1);

        if (location + elementCount <= validOutputs.size())
        {
            for (size_t elementIndex = 0; elementIndex < elementCount; elementIndex++)
            {
                const size_t offsetLocation = location + elementIndex;
                if (validOutputs[offsetLocation])
                {
                    std::stringstream strstr;
                    strstr << "conflicting output locations with previously defined output '"
                           << validOutputs[offsetLocation]->getSymbol() << "'";
                    error(*symbol, strstr.str().c_str(), diagnostics);
                }
                else
                {
                    validOutputs[offsetLocation] = symbol;
                }
            }
        }
        else
        {
            if (elementCount > 0)
            {
                error(*symbol,
                      elementCount > 1 ? "output array locations would exceed MAX_DRAW_BUFFERS"
                                       : "output location must be < MAX_DRAW_BUFFERS",
                      diagnostics);
            }
        }
    }

    if (!mAllowUnspecifiedOutputLocationResolution &&
        ((!mOutputs.empty() && !mUnspecifiedLocationOutputs.empty()) ||
         mUnspecifiedLocationOutputs.size() > 1))
    {
        for (const auto &symbol : mUnspecifiedLocationOutputs)
        {
            error(*symbol,
                  "must explicitly specify all locations when using multiple fragment outputs",
                  diagnostics);
        }
    }

    if (!mYuvOutputs.empty() && (mYuvOutputs.size() > 1 || mUsesFragDepth || !mOutputs.empty() ||
                                 !mUnspecifiedLocationOutputs.empty()))
    {
        for (const auto &symbol : mYuvOutputs)
        {
            error(*symbol,
                  "not allowed to specify yuv qualifier when using depth or multiple color "
                  "fragment outputs",
                  diagnostics);
        }
    }
}

}  // anonymous namespace

bool ValidateOutputs(TIntermBlock *root,
                     const TExtensionBehavior &extBehavior,
                     int maxDrawBuffers,
                     TDiagnostics *diagnostics)
{
    ValidateOutputsTraverser validateOutputs(extBehavior, maxDrawBuffers);
    root->traverse(&validateOutputs);
    int numErrorsBefore = diagnostics->numErrors();
    validateOutputs.validate(diagnostics);
    return (diagnostics->numErrors() == numErrorsBefore);
}

}  // namespace sh
