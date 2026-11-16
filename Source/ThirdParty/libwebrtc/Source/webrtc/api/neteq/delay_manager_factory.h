/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_NETEQ_DELAY_MANAGER_FACTORY_H_
#define API_NETEQ_DELAY_MANAGER_FACTORY_H_

#include <memory>

#include "api/field_trials_view.h"
#include "api/neteq/delay_manager_interface.h"
#include "api/neteq/tick_timer.h"
namespace webrtc {

// Creates DelayManagerInterface instances.
class DelayManagerFactory {
 public:
  virtual ~DelayManagerFactory() = default;

  // Creates a new DelayManager object.
  virtual std::unique_ptr<DelayManagerInterface> Create(
      const FieldTrialsView& field_trials,
      const TickTimer* tick_timer) const = 0;
};

}  // namespace webrtc

#endif  // API_NETEQ_DELAY_MANAGER_FACTORY_H_
