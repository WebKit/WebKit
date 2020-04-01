/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_RESOURCE_H_
#define CALL_ADAPTATION_RESOURCE_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"

namespace webrtc {

class Resource;

enum class ResourceUsageState {
  // Action is needed to minimze the load on this resource.
  kOveruse,
  // No action needed for this resource, increasing the load on this resource
  // is not allowed.
  kStable,
  // Increasing the load on this resource is allowed.
  kUnderuse,
};

enum class ResourceListenerResponse {
  kNothing,
  // This response is only applicable to QualityScaler-based resources.
  // It tells the QualityScaler to increase its QP measurement frequency.
  //
  // This is modelled after AdaptationObserverInterface::AdaptDown()'s return
  // value. The method comment says "Returns false if a downgrade was requested
  // but the request did not result in a new limiting resolution or fps."
  // However the actual implementation seems to be: Return false if
  // !has_input_video_ or if we use balanced degradation preference and we DID
  // adapt frame rate but the difference between input frame rate and balanced
  // settings' min fps is less than the balanced settings' min fps diff - in all
  // other cases, return true whether or not adaptation happened.
  //
  // For QualityScaler-based resources, kQualityScalerShouldIncreaseFrequency
  // maps to "return false" and kNothing maps to "return true".
  //
  // TODO(https://crbug.com/webrtc/11222): Remove this enum. Resource
  // measurements and adaptation decisions need to be separated in order to
  // support injectable adaptation modules, multi-stream aware adaptation and
  // decision-making logic based on multiple resources.
  kQualityScalerShouldIncreaseFrequency,
};

class ResourceListener {
 public:
  virtual ~ResourceListener();

  // Informs the listener of a new measurement of resource usage. This means
  // that |resource.usage_state()| is now up-to-date.
  //
  // The listener may influence the resource that signaled the measurement
  // according to the returned ResourceListenerResponse enum.
  virtual ResourceListenerResponse OnResourceUsageStateMeasured(
      const Resource& resource) = 0;
};

// A Resource is something which can be measured as "overused", "stable" or
// "underused". When the resource usage changes, listeners of the resource are
// informed.
//
// Implementations of this interface are responsible for performing resource
// usage measurements and invoking OnResourceUsageStateMeasured().
class Resource {
 public:
  // By default, usage_state() is kStable until a measurement is made.
  Resource();
  virtual ~Resource();

  void RegisterListener(ResourceListener* listener);
  void UnregisterListener(ResourceListener* listener);

  ResourceUsageState usage_state() const;

  virtual std::string name() const = 0;

 protected:
  // Updates the usage state and informs all registered listeners.
  // Returns the result of the last listener's OnResourceUsageStateMeasured()
  // call that was not kNothing, else kNothing.
  ResourceListenerResponse OnResourceUsageStateMeasured(
      ResourceUsageState usage_state);

 private:
  ResourceUsageState usage_state_;
  std::vector<ResourceListener*> listeners_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_H_
