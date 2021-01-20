//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_CONSTANT_NAMES_H_
#define COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_CONSTANT_NAMES_H_

#include "compiler/translator/TranslatorMetalDirect/Name.h"

namespace sh
{

namespace constant_names
{

constexpr Name kCoverageMaskEnabled("ANGLE_CoverageMaskEnabled", SymbolType::AngleInternal);
constexpr Name kRasterizationDiscardEnabled("ANGLE_RasterizationDiscard",
                                            SymbolType::AngleInternal);

}  // namespace constant_names

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_CONSTANT_NAMES_H_
