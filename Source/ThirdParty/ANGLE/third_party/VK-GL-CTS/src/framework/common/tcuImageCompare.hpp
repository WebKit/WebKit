#ifndef _TCUIMAGECOMPARE_HPP
#define _TCUIMAGECOMPARE_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Image comparison utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuVectorType.hpp"

namespace tcu
{

class RGBA;
class Surface;
class ConstPixelBufferAccess;
class TestLog;

enum CompareLogMode
{
    COMPARE_LOG_EVERYTHING = 0, //!< Always log both reference, result and error mask.
    COMPARE_LOG_RESULT,         //!< If comparison passes, log result only. If fails, everything is written to log.
    COMPARE_LOG_ON_ERROR,       //!< If comparison passes, nothing is logged. If fails, everything is written to log.

    COMPARE_LOG_LAST
};

// Utilities for comparing and logging.
bool pixelThresholdCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc, const Surface &reference,
                           const Surface &result, const RGBA &threshold, CompareLogMode logMode);
bool fuzzyCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc, const Surface &reference,
                  const Surface &result, float threshold, CompareLogMode logMode);
bool fuzzyCompareMaxError(TestLog &log, const char *imageSetName, const char *imageSetDesc, const Surface &reference,
                          const Surface &result, float threshold, CompareLogMode logMode);
int measurePixelDiffAccuracy(TestLog &log, const char *imageSetName, const char *imageSetDesc, const Surface &reference,
                             const Surface &result, int bestScoreDiff, int worstScoreDiff, CompareLogMode logMode);

bool fuzzyCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                  const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result, float threshold,
                  CompareLogMode logMode);
bool bitwiseCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                    const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                    CompareLogMode logMode);
bool fuzzyCompareMaxError(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                          const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                          float threshold, CompareLogMode logMode);
bool floatUlpThresholdCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                              const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                              const UVec4 &threshold, CompareLogMode logMode);
bool floatThresholdCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                           const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                           const Vec4 &threshold, CompareLogMode logMode);
bool floatThresholdCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                           const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                           const Vec4 &ignorekey, const Vec4 &threshold, CompareLogMode logMode);
bool floatThresholdCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc, const Vec4 &reference,
                           const ConstPixelBufferAccess &result, const Vec4 &threshold, CompareLogMode logMode);
bool intThresholdCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                         const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                         const UVec4 &threshold, CompareLogMode logMode, bool use64Bits = false);
bool intThresholdPositionDeviationCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                                          const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                                          const UVec4 &threshold, const tcu::IVec3 &maxPositionDeviation,
                                          bool acceptOutOfBoundsAsAnyValue, CompareLogMode logMode);
bool intThresholdPositionDeviationErrorThresholdCompare(
    TestLog &log, const char *imageSetName, const char *imageSetDesc, const ConstPixelBufferAccess &reference,
    const ConstPixelBufferAccess &result, const UVec4 &threshold, const tcu::IVec3 &maxPositionDeviation,
    bool acceptOutOfBoundsAsAnyValue, int maxAllowedFailingPixels, CompareLogMode logMode);
bool dsThresholdCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                        const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                        const float threshold, CompareLogMode logMode);
int measurePixelDiffAccuracy(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                             const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                             int bestScoreDiff, int worstScoreDiff, CompareLogMode logMode);
bool bilinearCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                     const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                     const RGBA threshold, CompareLogMode logMode);

} // namespace tcu

#endif // _TCUIMAGECOMPARE_HPP
