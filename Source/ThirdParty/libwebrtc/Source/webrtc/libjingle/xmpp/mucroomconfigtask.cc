/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include <vector>

#include "webrtc/libjingle/xmpp/mucroomconfigtask.h"

#include "webrtc/libjingle/xmpp/constants.h"

namespace buzz {

MucRoomConfigTask::MucRoomConfigTask(
    XmppTaskParentInterface* parent,
    const Jid& room_jid,
    const std::string& room_name,
    const std::vector<std::string>& room_features)
    : IqTask(parent, STR_SET, room_jid,
             MakeRequest(room_name, room_features)),
      room_jid_(room_jid) {
}

XmlElement* MucRoomConfigTask::MakeRequest(
    const std::string& room_name,
    const std::vector<std::string>& room_features) {
  buzz::XmlElement* owner_query = new
      buzz::XmlElement(buzz::QN_MUC_OWNER_QUERY, true);

  buzz::XmlElement* x_form = new buzz::XmlElement(buzz::QN_XDATA_X, true);
  x_form->SetAttr(buzz::QN_TYPE, buzz::STR_FORM);

  buzz::XmlElement* roomname_field =
      new buzz::XmlElement(buzz::QN_XDATA_FIELD, false);
  roomname_field->SetAttr(buzz::QN_VAR, buzz::STR_MUC_ROOMCONFIG_ROOMNAME);
  roomname_field->SetAttr(buzz::QN_TYPE, buzz::STR_TEXT_SINGLE);

  buzz::XmlElement* roomname_value =
      new buzz::XmlElement(buzz::QN_XDATA_VALUE, false);
  roomname_value->SetBodyText(room_name);

  roomname_field->AddElement(roomname_value);
  x_form->AddElement(roomname_field);

  buzz::XmlElement* features_field =
      new buzz::XmlElement(buzz::QN_XDATA_FIELD, false);
  features_field->SetAttr(buzz::QN_VAR, buzz::STR_MUC_ROOMCONFIG_FEATURES);
  features_field->SetAttr(buzz::QN_TYPE, buzz::STR_LIST_MULTI);

  for (std::vector<std::string>::const_iterator feature = room_features.begin();
       feature != room_features.end(); ++feature) {
    buzz::XmlElement* features_value =
        new buzz::XmlElement(buzz::QN_XDATA_VALUE, false);
    features_value->SetBodyText(*feature);
    features_field->AddElement(features_value);
  }

  x_form->AddElement(features_field);
  owner_query->AddElement(x_form);
  return owner_query;
}

void MucRoomConfigTask::HandleResult(const XmlElement* element) {
  SignalResult(this);
}

}  // namespace buzz
