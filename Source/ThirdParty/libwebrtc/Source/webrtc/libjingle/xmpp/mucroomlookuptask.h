/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_MUCROOMLOOKUPTASK_H_
#define WEBRTC_LIBJINGLE_XMPP_MUCROOMLOOKUPTASK_H_

#include <string>
#include "webrtc/libjingle/xmpp/iqtask.h"

namespace buzz {

struct MucRoomInfo {
  Jid jid;
  std::string name;
  std::string domain;
  std::string hangout_id;

  std::string full_name() const {
    return name + "@" + domain;
  }
};

class MucRoomLookupTask : public IqTask {
 public:
  enum IdType {
    ID_TYPE_CONVERSATION,
    ID_TYPE_HANGOUT
  };

  static MucRoomLookupTask*
      CreateLookupTaskForRoomName(XmppTaskParentInterface* parent,
                                  const Jid& lookup_server_jid,
                                  const std::string& room_name,
                                  const std::string& room_domain);
  static MucRoomLookupTask*
      CreateLookupTaskForRoomJid(XmppTaskParentInterface* parent,
                                 const Jid& lookup_server_jid,
                                 const Jid& room_jid);
  static MucRoomLookupTask*
      CreateLookupTaskForHangoutId(XmppTaskParentInterface* parent,
                                   const Jid& lookup_server_jid,
                                   const std::string& hangout_id);
  static MucRoomLookupTask*
      CreateLookupTaskForExternalId(XmppTaskParentInterface* parent,
                                    const Jid& lookup_server_jid,
                                    const std::string& external_id,
                                    const std::string& type);

  sigslot::signal2<MucRoomLookupTask*,
                   const MucRoomInfo&> SignalResult;

 protected:
  virtual void HandleResult(const XmlElement* element);

 private:
  MucRoomLookupTask(XmppTaskParentInterface* parent,
                    const Jid& lookup_server_jid,
                    XmlElement* query);
  static XmlElement* MakeNameQuery(const std::string& room_name,
                                   const std::string& room_domain);
  static XmlElement* MakeJidQuery(const Jid& room_jid);
  static XmlElement* MakeHangoutIdQuery(const std::string& hangout_id);
  static XmlElement* MakeExternalIdQuery(const std::string& external_id,
                                         const std::string& type);
};

}  // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMPP_MUCROOMLOOKUPTASK_H_
