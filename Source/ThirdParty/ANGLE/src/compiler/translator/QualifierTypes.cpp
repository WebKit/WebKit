//
// Copyright (c) 2002-2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/QualifierTypes.h"

#include "compiler/translator/Diagnostics.h"

#include <algorithm>

namespace sh
{
TLayoutQualifier JoinLayoutQualifiers(TLayoutQualifier leftQualifier,
                                      TLayoutQualifier rightQualifier,
                                      const TSourceLoc &rightQualifierLocation,
                                      TDiagnostics *diagnostics)
{
    TLayoutQualifier joinedQualifier = leftQualifier;

    if (rightQualifier.location != -1)
    {
        joinedQualifier.location = rightQualifier.location;
        ++joinedQualifier.locationsSpecified;
    }
    if (rightQualifier.matrixPacking != EmpUnspecified)
    {
        joinedQualifier.matrixPacking = rightQualifier.matrixPacking;
    }
    if (rightQualifier.blockStorage != EbsUnspecified)
    {
        joinedQualifier.blockStorage = rightQualifier.blockStorage;
    }

    for (size_t i = 0u; i < rightQualifier.localSize.size(); ++i)
    {
        if (rightQualifier.localSize[i] != -1)
        {
            if (joinedQualifier.localSize[i] != -1 &&
                joinedQualifier.localSize[i] != rightQualifier.localSize[i])
            {
                diagnostics->error(rightQualifierLocation,
                                   "Cannot have multiple different work group size specifiers",
                                   getWorkGroupSizeString(i), "");
            }
            joinedQualifier.localSize[i] = rightQualifier.localSize[i];
        }
    }

    return joinedQualifier;
}
}  // namespace sh

namespace
{

// GLSL ES 3.10 does not impose a strict order on type qualifiers and allows multiple layout
// declarations.
// GLSL ES 3.10 Revision 4, 4.10 Order of Qualification
bool AreTypeQualifierChecksRelaxed(int shaderVersion)
{
    return shaderVersion >= 310;
}

#if defined(ANGLE_ENABLE_ASSERTS)
bool IsScopeQualifier(TQualifier qualifier)
{
    return qualifier == EvqGlobal || qualifier == EvqTemporary;
}

bool IsScopeQualifierWrapper(const TQualifierWrapperBase *qualifier)
{
    if (qualifier->getType() != QtStorage)
        return false;
    const TStorageQualifierWrapper *storageQualifier =
        static_cast<const TStorageQualifierWrapper *>(qualifier);
    TQualifier q = storageQualifier->getQualifier();
    return IsScopeQualifier(q);
}

// Returns true if the invariant for the qualifier sequence holds
bool IsInvariantCorrect(const TTypeQualifierBuilder::QualifierSequence &qualifiers)
{
    // We should have at least one qualifier.
    // The first qualifier always tells the scope.
    return qualifiers.size() >= 1 && IsScopeQualifierWrapper(qualifiers[0]);
}
#endif

// Returns true if there are qualifiers which have been specified multiple times
// If areQualifierChecksRelaxed is set to true, then layout qualifier repetition is allowed.
bool HasRepeatingQualifiers(const TTypeQualifierBuilder::QualifierSequence &qualifiers,
                            bool areQualifierChecksRelaxed,
                            std::string *errorMessage)
{
    bool invariantFound     = false;
    bool precisionFound     = false;
    bool layoutFound        = false;
    bool interpolationFound = false;

    unsigned int locationsSpecified = 0;
    bool isOut                      = false;

    // The iteration starts from one since the first qualifier only reveals the scope of the
    // expression. It is inserted first whenever the sequence gets created.
    for (size_t i = 1; i < qualifiers.size(); ++i)
    {
        switch (qualifiers[i]->getType())
        {
            case QtInvariant:
            {
                if (invariantFound)
                {
                    *errorMessage = "The invariant qualifier specified multiple times.";
                    return true;
                }
                invariantFound = true;
                break;
            }
            case QtPrecision:
            {
                if (precisionFound)
                {
                    *errorMessage = "The precision qualifier specified multiple times.";
                    return true;
                }
                precisionFound = true;
                break;
            }
            case QtLayout:
            {
                if (layoutFound && !areQualifierChecksRelaxed)
                {
                    *errorMessage = "The layout qualifier specified multiple times.";
                    return true;
                }
                if (invariantFound && !areQualifierChecksRelaxed)
                {
                    // This combination is not correct according to the syntax specified in the
                    // formal grammar in the ESSL 3.00 spec. In ESSL 3.10 the grammar does not have
                    // a similar restriction.
                    *errorMessage =
                        "The layout qualifier and invariant qualifier cannot coexist in the same "
                        "declaration according to the grammar.";
                    return true;
                }
                layoutFound = true;
                const TLayoutQualifier &currentQualifier =
                    static_cast<const TLayoutQualifierWrapper *>(qualifiers[i])->getQualifier();
                locationsSpecified += currentQualifier.locationsSpecified;
                break;
            }
            case QtInterpolation:
            {
                // 'centroid' is treated as a storage qualifier
                // 'flat centroid' will be squashed to 'flat'
                // 'smooth centroid' will be squashed to 'centroid'
                if (interpolationFound)
                {
                    *errorMessage = "The interpolation qualifier specified multiple times.";
                    return true;
                }
                interpolationFound = true;
                break;
            }
            case QtStorage:
            {
                // Go over all of the storage qualifiers up until the current one and check for
                // repetitions.
                TQualifier currentQualifier =
                    static_cast<const TStorageQualifierWrapper *>(qualifiers[i])->getQualifier();
                if (currentQualifier == EvqVertexOut || currentQualifier == EvqFragmentOut)
                {
                    isOut = true;
                }
                for (size_t j = 1; j < i; ++j)
                {
                    if (qualifiers[j]->getType() == QtStorage)
                    {
                        const TStorageQualifierWrapper *previousQualifierWrapper =
                            static_cast<const TStorageQualifierWrapper *>(qualifiers[j]);
                        TQualifier previousQualifier = previousQualifierWrapper->getQualifier();
                        if (currentQualifier == previousQualifier)
                        {
                            *errorMessage = previousQualifierWrapper->getQualifierString().c_str();
                            *errorMessage += " specified multiple times";
                            return true;
                        }
                    }
                }
                break;
            }
            default:
                UNREACHABLE();
        }
    }

    if (locationsSpecified > 1 && isOut)
    {
        // GLSL ES 3.00.6 section 4.3.8.2 Output Layout Qualifiers
        // GLSL ES 3.10 section 4.4.2 Output Layout Qualifiers
        // "The qualifier may appear at most once within a declaration."
        *errorMessage = "Output layout location specified multiple times.";
        return true;
    }

    return false;
}

// GLSL ES 3.00_6, 4.7 Order of Qualification
// The correct order of qualifiers is:
// invariant-qualifier interpolation-qualifier storage-qualifier precision-qualifier
// layout-qualifier has to be before storage-qualifier.
bool AreQualifiersInOrder(const TTypeQualifierBuilder::QualifierSequence &qualifiers,
                          std::string *errorMessage)
{
    bool foundInterpolation = false;
    bool foundStorage       = false;
    bool foundPrecision     = false;
    for (size_t i = 1; i < qualifiers.size(); ++i)
    {
        switch (qualifiers[i]->getType())
        {
            case QtInvariant:
                if (foundInterpolation || foundStorage || foundPrecision)
                {
                    *errorMessage = "The invariant qualifier has to be first in the expression.";
                    return false;
                }
                break;
            case QtInterpolation:
                if (foundStorage)
                {
                    *errorMessage = "Storage qualifiers have to be after interpolation qualifiers.";
                    return false;
                }
                else if (foundPrecision)
                {
                    *errorMessage =
                        "Precision qualifiers have to be after interpolation qualifiers.";
                    return false;
                }
                foundInterpolation = true;
                break;
            case QtLayout:
                if (foundStorage)
                {
                    *errorMessage = "Storage qualifiers have to be after layout qualifiers.";
                    return false;
                }
                else if (foundPrecision)
                {
                    *errorMessage = "Precision qualifiers have to be after layout qualifiers.";
                    return false;
                }
                break;
            case QtStorage:
                if (foundPrecision)
                {
                    *errorMessage = "Precision qualifiers have to be after storage qualifiers.";
                    return false;
                }
                foundStorage = true;
                break;
            case QtPrecision:
                foundPrecision = true;
                break;
            default:
                UNREACHABLE();
        }
    }
    return true;
}

struct QualifierComparator
{
    bool operator()(const TQualifierWrapperBase *q1, const TQualifierWrapperBase *q2)
    {
        return q1->getRank() < q2->getRank();
    }
};

void SortSequence(TTypeQualifierBuilder::QualifierSequence &qualifiers)
{
    // We need a stable sorting algorithm since the order of layout-qualifier declarations matter.
    // The sorting starts from index 1, instead of 0, since the element at index 0 tells the scope
    // and we always want it to be first.
    std::stable_sort(qualifiers.begin() + 1, qualifiers.end(), QualifierComparator());
}

// Handles the joining of storage qualifiers for variables.
bool JoinVariableStorageQualifier(TQualifier *joinedQualifier, TQualifier storageQualifier)
{
    switch (*joinedQualifier)
    {
        case EvqGlobal:
            *joinedQualifier = storageQualifier;
            break;
        case EvqTemporary:
        {
            switch (storageQualifier)
            {
                case EvqConst:
                    *joinedQualifier = storageQualifier;
                    break;
                default:
                    return false;
            }
            break;
        }
        case EvqSmooth:
        {
            switch (storageQualifier)
            {
                case EvqCentroid:
                    *joinedQualifier = EvqCentroid;
                    break;
                case EvqVertexOut:
                    *joinedQualifier = EvqSmoothOut;
                    break;
                case EvqFragmentIn:
                    *joinedQualifier = EvqSmoothIn;
                    break;
                default:
                    return false;
            }
            break;
        }
        case EvqFlat:
        {
            switch (storageQualifier)
            {
                case EvqCentroid:
                    *joinedQualifier = EvqFlat;
                    break;
                case EvqVertexOut:
                    *joinedQualifier = EvqFlatOut;
                    break;
                case EvqFragmentIn:
                    *joinedQualifier = EvqFlatIn;
                    break;
                default:
                    return false;
            }
            break;
        }
        case EvqCentroid:
        {
            switch (storageQualifier)
            {
                case EvqVertexOut:
                    *joinedQualifier = EvqCentroidOut;
                    break;
                case EvqFragmentIn:
                    *joinedQualifier = EvqCentroidIn;
                    break;
                default:
                    return false;
            }
            break;
        }
        default:
            return false;
    }
    return true;
}

// Handles the joining of storage qualifiers for a parameter in a function.
bool JoinParameterStorageQualifier(TQualifier *joinedQualifier, TQualifier storageQualifier)
{
    switch (*joinedQualifier)
    {
        case EvqTemporary:
            *joinedQualifier = storageQualifier;
            break;
        case EvqConst:
        {
            switch (storageQualifier)
            {
                case EvqIn:
                    *joinedQualifier = EvqConstReadOnly;
                    break;
                default:
                    return false;
            }
            break;
        }
        default:
            return false;
    }
    return true;
}

TTypeQualifier GetVariableTypeQualifierFromSortedSequence(
    const TTypeQualifierBuilder::QualifierSequence &sortedSequence,
    TDiagnostics *diagnostics)
{
    TTypeQualifier typeQualifier(
        static_cast<const TStorageQualifierWrapper *>(sortedSequence[0])->getQualifier(),
        sortedSequence[0]->getLine());
    for (size_t i = 1; i < sortedSequence.size(); ++i)
    {
        const TQualifierWrapperBase *qualifier = sortedSequence[i];
        bool isQualifierValid                  = false;
        switch (qualifier->getType())
        {
            case QtInvariant:
                isQualifierValid        = true;
                typeQualifier.invariant = true;
                break;
            case QtInterpolation:
            {
                switch (typeQualifier.qualifier)
                {
                    case EvqGlobal:
                        isQualifierValid = true;
                        typeQualifier.qualifier =
                            static_cast<const TInterpolationQualifierWrapper *>(qualifier)
                                ->getQualifier();
                        break;
                    default:
                        isQualifierValid = false;
                }
                break;
            }
            case QtLayout:
            {
                const TLayoutQualifierWrapper *layoutQualifierWrapper =
                    static_cast<const TLayoutQualifierWrapper *>(qualifier);
                isQualifierValid              = true;
                typeQualifier.layoutQualifier = sh::JoinLayoutQualifiers(
                    typeQualifier.layoutQualifier, layoutQualifierWrapper->getQualifier(),
                    layoutQualifierWrapper->getLine(), diagnostics);
                break;
            }
            case QtStorage:
                isQualifierValid = JoinVariableStorageQualifier(
                    &typeQualifier.qualifier,
                    static_cast<const TStorageQualifierWrapper *>(qualifier)->getQualifier());
                break;
            case QtPrecision:
                isQualifierValid = true;
                typeQualifier.precision =
                    static_cast<const TPrecisionQualifierWrapper *>(qualifier)->getQualifier();
                ASSERT(typeQualifier.precision != EbpUndefined);
                break;
            default:
                UNREACHABLE();
        }
        if (!isQualifierValid)
        {
            const TString &qualifierString = qualifier->getQualifierString();
            diagnostics->error(qualifier->getLine(), "invalid qualifier combination",
                               qualifierString.c_str(), "");
            break;
        }
    }
    return typeQualifier;
}

TTypeQualifier GetParameterTypeQualifierFromSortedSequence(
    const TTypeQualifierBuilder::QualifierSequence &sortedSequence,
    TDiagnostics *diagnostics)
{
    TTypeQualifier typeQualifier(EvqTemporary, sortedSequence[0]->getLine());
    for (size_t i = 1; i < sortedSequence.size(); ++i)
    {
        const TQualifierWrapperBase *qualifier = sortedSequence[i];
        bool isQualifierValid                  = false;
        switch (qualifier->getType())
        {
            case QtInvariant:
            case QtInterpolation:
            case QtLayout:
                break;
            case QtStorage:
                isQualifierValid = JoinParameterStorageQualifier(
                    &typeQualifier.qualifier,
                    static_cast<const TStorageQualifierWrapper *>(qualifier)->getQualifier());
                break;
            case QtPrecision:
                isQualifierValid = true;
                typeQualifier.precision =
                    static_cast<const TPrecisionQualifierWrapper *>(qualifier)->getQualifier();
                ASSERT(typeQualifier.precision != EbpUndefined);
                break;
            default:
                UNREACHABLE();
        }
        if (!isQualifierValid)
        {
            const TString &qualifierString = qualifier->getQualifierString();
            diagnostics->error(qualifier->getLine(), "invalid parameter qualifier",
                               qualifierString.c_str(), "");
            break;
        }
    }

    switch (typeQualifier.qualifier)
    {
        case EvqIn:
        case EvqConstReadOnly:  // const in
        case EvqOut:
        case EvqInOut:
            break;
        case EvqConst:
            typeQualifier.qualifier = EvqConstReadOnly;
            break;
        case EvqTemporary:
            // no qualifier has been specified, set it to EvqIn which is the default
            typeQualifier.qualifier = EvqIn;
            break;
        default:
            diagnostics->error(sortedSequence[0]->getLine(), "Invalid parameter qualifier ",
                               getQualifierString(typeQualifier.qualifier), "");
    }
    return typeQualifier;
}
}  // namespace

unsigned int TInvariantQualifierWrapper::getRank() const
{
    return 0u;
}

unsigned int TInterpolationQualifierWrapper::getRank() const
{
    return 1u;
}

unsigned int TLayoutQualifierWrapper::getRank() const
{
    return 2u;
}

unsigned int TStorageQualifierWrapper::getRank() const
{
    // Force the 'centroid' auxilary storage qualifier to be always first among all storage
    // qualifiers.
    if (mStorageQualifier == EvqCentroid)
    {
        return 3u;
    }
    else
    {
        return 4u;
    }
}

unsigned int TPrecisionQualifierWrapper::getRank() const
{
    return 5u;
}

TTypeQualifier::TTypeQualifier(TQualifier scope, const TSourceLoc &loc)
    : layoutQualifier(TLayoutQualifier::create()),
      precision(EbpUndefined),
      qualifier(scope),
      invariant(false),
      line(loc)
{
    ASSERT(IsScopeQualifier(qualifier));
}

TTypeQualifierBuilder::TTypeQualifierBuilder(const TStorageQualifierWrapper *scope,
                                             int shaderVersion)
    : mShaderVersion(shaderVersion)
{
    ASSERT(IsScopeQualifier(scope->getQualifier()));
    mQualifiers.push_back(scope);
}

void TTypeQualifierBuilder::appendQualifier(const TQualifierWrapperBase *qualifier)
{
    mQualifiers.push_back(qualifier);
}

bool TTypeQualifierBuilder::checkSequenceIsValid(TDiagnostics *diagnostics) const
{
    bool areQualifierChecksRelaxed = AreTypeQualifierChecksRelaxed(mShaderVersion);
    std::string errorMessage;
    if (HasRepeatingQualifiers(mQualifiers, areQualifierChecksRelaxed, &errorMessage))
    {
        diagnostics->error(mQualifiers[0]->getLine(), "qualifier sequence", errorMessage.c_str(),
                           "");
        return false;
    }

    if (!areQualifierChecksRelaxed && !AreQualifiersInOrder(mQualifiers, &errorMessage))
    {
        diagnostics->error(mQualifiers[0]->getLine(), "qualifier sequence", errorMessage.c_str(),
                           "");
        return false;
    }

    return true;
}

TTypeQualifier TTypeQualifierBuilder::getParameterTypeQualifier(TDiagnostics *diagnostics) const
{
    ASSERT(IsInvariantCorrect(mQualifiers));
    ASSERT(static_cast<const TStorageQualifierWrapper *>(mQualifiers[0])->getQualifier() ==
           EvqTemporary);

    if (!checkSequenceIsValid(diagnostics))
    {
        return TTypeQualifier(EvqTemporary, mQualifiers[0]->getLine());
    }

    // If the qualifier checks are relaxed, then it is easier to sort the qualifiers so
    // that the order imposed by the GLSL ES 3.00 spec is kept. Then we can use the same code to
    // combine the qualifiers.
    if (AreTypeQualifierChecksRelaxed(mShaderVersion))
    {
        // Copy the qualifier sequence so that we can sort them.
        QualifierSequence sortedQualifierSequence = mQualifiers;
        SortSequence(sortedQualifierSequence);
        return GetParameterTypeQualifierFromSortedSequence(sortedQualifierSequence, diagnostics);
    }
    return GetParameterTypeQualifierFromSortedSequence(mQualifiers, diagnostics);
}

TTypeQualifier TTypeQualifierBuilder::getVariableTypeQualifier(TDiagnostics *diagnostics) const
{
    ASSERT(IsInvariantCorrect(mQualifiers));

    if (!checkSequenceIsValid(diagnostics))
    {
        return TTypeQualifier(
            static_cast<const TStorageQualifierWrapper *>(mQualifiers[0])->getQualifier(),
            mQualifiers[0]->getLine());
    }

    // If the qualifier checks are relaxed, then it is easier to sort the qualifiers so
    // that the order imposed by the GLSL ES 3.00 spec is kept. Then we can use the same code to
    // combine the qualifiers.
    if (AreTypeQualifierChecksRelaxed(mShaderVersion))
    {
        // Copy the qualifier sequence so that we can sort them.
        QualifierSequence sortedQualifierSequence = mQualifiers;
        SortSequence(sortedQualifierSequence);
        return GetVariableTypeQualifierFromSortedSequence(sortedQualifierSequence, diagnostics);
    }
    return GetVariableTypeQualifierFromSortedSequence(mQualifiers, diagnostics);
}
