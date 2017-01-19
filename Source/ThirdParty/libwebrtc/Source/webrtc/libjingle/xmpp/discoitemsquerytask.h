/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Fires a disco items query, such as the following example:
//
//      <iq type='get'
//          from='foo@gmail.com/asdf'
//          to='bar@google.com'
//          id='1234'>
//          <query xmlns=' http://jabber.org/protocol/disco#items'
//                 node='blah '/>
//      </iq>
//
// Sample response:
//
//      <iq type='result'
//          from=' hendriks@google.com'
//          to='rsturgell@google.com/asdf'
//          id='1234'>
//          <query xmlns=' http://jabber.org/protocol/disco#items '
//                 node='blah'>
//                 <item something='somethingelse'/>
//          </query>
//      </iq>


#ifndef WEBRTC_LIBJINGLE_XMPP_DISCOITEMSQUERYTASK_H_
#define WEBRTC_LIBJINGLE_XMPP_DISCOITEMSQUERYTASK_H_

#include <string>
#include <vector>

#include "webrtc/libjingle/xmpp/iqtask.h"

namespace buzz {

struct DiscoItem {
  std::string jid;
  std::string node;
  std::string name;
};

class DiscoItemsQueryTask : public IqTask {
 public:
  DiscoItemsQueryTask(XmppTaskParentInterface* parent,
                      const Jid& to, const std::string& node);

  sigslot::signal1<std::vector<DiscoItem> > SignalResult;

 private:
  static XmlElement* MakeRequest(const std::string& node);
  virtual void HandleResult(const XmlElement* result);
  static bool ParseItem(const XmlElement* element, DiscoItem* item);
};

}  // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMPP_DISCOITEMSQUERYTASK_H_
