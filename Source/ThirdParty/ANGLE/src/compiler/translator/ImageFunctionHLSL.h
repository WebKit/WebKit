//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ImageFunctionHLSL: Class for writing implementations of ESSL image functions into HLSL output.
//

#ifndef COMPILER_TRANSLATOR_IMAGEFUNCTIONHLSL_H_
#define COMPILER_TRANSLATOR_IMAGEFUNCTIONHLSL_H_

#include <set>

#include "GLSLANG/ShaderLang.h"
#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/Common.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/Types.h"

namespace sh
{

class ImageFunctionHLSL final : angle::NonCopyable
{
  public:
    // Returns the name of the image function implementation to caller.
    // The name that's passed in is the name of the GLSL image function that it should implement.
    TString useImageFunction(const TString &name,
                             const TBasicType &type,
                             TLayoutImageInternalFormat imageInternalFormat,
                             bool readonly);

    void imageFunctionHeader(TInfoSinkBase &out);

  private:
    struct ImageFunction
    {
        // See ESSL 3.10.4 section 8.12 for reference about what the different methods below do.
        enum class Method
        {
            SIZE,
            LOAD,
            STORE
        };

        TString name() const;

        bool operator<(const ImageFunction &rhs) const;

        const char *getReturnType() const;

        TBasicType image;
        TLayoutImageInternalFormat imageInternalFormat;
        bool readonly;
        Method method;
    };

    static void OutputImageFunctionArgumentList(
        TInfoSinkBase &out,
        const ImageFunctionHLSL::ImageFunction &imageFunction);
    static void OutputImageSizeFunctionBody(TInfoSinkBase &out,
                                            const ImageFunctionHLSL::ImageFunction &imageFunction,
                                            const TString &imageReference);
    static void OutputImageLoadFunctionBody(TInfoSinkBase &out,
                                            const ImageFunctionHLSL::ImageFunction &imageFunction,
                                            const TString &imageReference);
    static void OutputImageStoreFunctionBody(TInfoSinkBase &out,
                                             const ImageFunctionHLSL::ImageFunction &imageFunction,
                                             const TString &imageReference);
    using ImageFunctionSet = std::set<ImageFunction>;
    ImageFunctionSet mUsesImage;
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_IMAGEFUNCTIONHLSL_H_
