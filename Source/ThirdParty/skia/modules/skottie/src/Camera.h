/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkottieCamera_DEFINED
#define SkottieCamera_DEFINED

#include "include/core/SkM44.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkSize.h"
#include "modules/skottie/src/SkottieValue.h"
#include "modules/skottie/src/Transform.h"

namespace skjson {
class ObjectValue;
}
namespace sksg {
class Transform;
}

namespace skottie {
namespace internal {
class AnimationBuilder;

class CameraAdaper final : public TransformAdapter3D {
public:
    CameraAdaper(const skjson::ObjectValue& jlayer,
                 const skjson::ObjectValue& jtransform,
                 const AnimationBuilder& abuilder,
                 const SkSize& viewport_size);
    ~CameraAdaper() override;

    // Used in the absence of an explicit camera layer.
    static sk_sp<sksg::Transform> DefaultCameraTransform(const SkSize& viewport_size);

    SkM44 totalMatrix() const override;

private:
    enum class CameraType {
        kOneNode, // implicitly facing forward (decreasing z), does not auto-orient
        kTwoNode, // explicitly facing a POI (the anchor point), auto-orients
    };

    SkV3 poi(const SkV3& pos) const;

    const SkSize     fViewportSize;
    const CameraType fType;

    ScalarValue fZoom = 0;

    using INHERITED = TransformAdapter3D;
};

} // namespace internal
} // namespace skottie

#endif // SkottieCamera_DEFINED
