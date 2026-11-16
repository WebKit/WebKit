//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// blocklayout.h:
//   Methods and classes related to uniform layout and packing in GLSL and HLSL.
//

#ifndef COMMON_BLOCKLAYOUTHLSL_H_
#define COMMON_BLOCKLAYOUTHLSL_H_

#include <cstddef>
#include <vector>

#include <GLSLANG/ShaderLang.h>
#include "angle_gl.h"
#include "compiler/translator/blocklayout.h"

namespace sh
{
// Block layout packed according to the default D3D10+ register packing rules
// See http://msdn.microsoft.com/en-us/library/windows/desktop/bb509632(v=vs.85).aspx
// This applies to D3D10+ constant blocks and all attributes/varyings.

class HLSLBlockEncoder : public BlockLayoutEncoder
{
  public:
    explicit HLSLBlockEncoder(bool transposeMatrices);

    void enterAggregateType(const ShaderVariable &structVar) override;
    void exitAggregateType(const ShaderVariable &structVar) override;
    void skipRegisters(unsigned int numRegisters);

  protected:
    void getBlockLayoutInfo(GLenum type,
                            size_t bytesPerComponent,
                            const std::vector<unsigned int> &arraySizes,
                            bool isRowMajorMatrix,
                            int *arrayStrideOut,
                            int *matrixStrideOut) override;
    void advanceOffset(GLenum type,
                       size_t bytesPerComponent,
                       const std::vector<unsigned int> &arraySizes,
                       bool isRowMajorMatrix,
                       int arrayStride,
                       int matrixStride) override;

    bool mTransposeMatrices;
};

// This method returns the number of used registers for a ShaderVariable. It is dependent on the
// HLSLBlockEncoder class to count the number of used registers in a struct (which are individually
// packed according to the same rules).
unsigned int HLSLVariableRegisterCount(const ShaderVariable &variable);
}  // namespace sh

#endif  // COMMON_BLOCKLAYOUTHLSL_H_
