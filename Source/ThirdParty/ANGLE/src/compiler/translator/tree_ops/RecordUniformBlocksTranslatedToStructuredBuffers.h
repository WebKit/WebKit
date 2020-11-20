//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RecordUniformBlocksTranslatedToStructuredBuffers.h:
// Collect all uniform blocks which will been translated to StructuredBuffers on Direct3D
// backend.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_RECORDUNIFORMBLOCKSTRANSLATEDTOSTRUCTUREDBUFFERS_H_
#define COMPILER_TRANSLATOR_TREEOPS_RECORDUNIFORMBLOCKSTRANSLATEDTOSTRUCTUREDBUFFERS_H_

#include "compiler/translator/IntermNode.h"

namespace sh
{
class TIntermNode;

ANGLE_NO_DISCARD bool RecordUniformBlocksTranslatedToStructuredBuffers(
    TIntermNode *root,
    std::map<int, const TInterfaceBlock *> &uniformBlockTranslatedToStructuredBuffer);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_RECORDACCESSUNIFORMBLOCKENTIREARRAYMEMBER_H_
