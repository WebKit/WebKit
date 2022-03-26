//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Header for the shared ShaderInterfaceVariableInfoMap class, used by both the
// Direct-to-Metal and Metal-SPIRV backends

#ifndef LIBANGLE_RENDERER_SHADERINTERFACEVARIABLEINFOMAP_H_
#define LIBANGLE_RENDERER_SHADERINTERFACEVARIABLEINFOMAP_H_

#include <functional>

#include <stdio.h>
#include "libANGLE/renderer/ProgramImpl.h"
#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/renderer_utils.h"
namespace rx
{

// TODO: http://anglebug.com/4524: Need a different hash key than a string, since that's slow to
// calculate.
class ShaderInterfaceVariableInfoMap final : angle::NonCopyable
{
  public:
    ShaderInterfaceVariableInfoMap();
    ~ShaderInterfaceVariableInfoMap();

    void clear();
    bool contains(gl::ShaderType shaderType, const std::string &variableName) const;
    const ShaderInterfaceVariableInfo &get(gl::ShaderType shaderType,
                                           const std::string &variableName) const;
    ShaderInterfaceVariableInfo &get(gl::ShaderType shaderType, const std::string &variableName);
    ShaderInterfaceVariableInfo &add(gl::ShaderType shaderType, const std::string &variableName);
    void markAsDuplicate(gl::ShaderType shaderType, const std::string &variableName);
    ShaderInterfaceVariableInfo &addOrGet(gl::ShaderType shaderType,
                                          const std::string &variableName);
    size_t variableCount(gl::ShaderType shaderType) const { return mData[shaderType].size(); }

    using VariableNameToInfoMap = angle::HashMap<std::string, ShaderInterfaceVariableInfo>;

    class Iterator final
    {
      public:
        Iterator(VariableNameToInfoMap::const_iterator beginIt,
                 VariableNameToInfoMap::const_iterator endIt)
            : mBeginIt(beginIt), mEndIt(endIt)
        {}
        VariableNameToInfoMap::const_iterator begin() { return mBeginIt; }
        VariableNameToInfoMap::const_iterator end() { return mEndIt; }

      private:
        VariableNameToInfoMap::const_iterator mBeginIt;
        VariableNameToInfoMap::const_iterator mEndIt;
    };

    Iterator getIterator(gl::ShaderType shaderType) const;

  private:
    gl::ShaderMap<VariableNameToInfoMap> mData;
};

}  // namespace rx
#endif  // LIBANGLE_RENDERER_SHADERINTERFACEVARIABLEINFOMAP_H_
