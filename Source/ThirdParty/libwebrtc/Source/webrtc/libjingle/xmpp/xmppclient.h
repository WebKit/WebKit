/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_XMPPCLIENT_H_
#define WEBRTC_LIBJINGLE_XMPP_XMPPCLIENT_H_

#include <memory>
#include <string>

#include "webrtc/libjingle/xmpp/asyncsocket.h"
#include "webrtc/libjingle/xmpp/xmppclientsettings.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/libjingle/xmpp/xmpptask.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/task.h"

namespace buzz {

class PreXmppAuth;
class CaptchaChallenge;

// Just some non-colliding number.  Could have picked "1".
#define XMPP_CLIENT_TASK_CODE 0x366c1e47

/////////////////////////////////////////////////////////////////////
//
// XMPPCLIENT
//
/////////////////////////////////////////////////////////////////////
//
// See Task first.  XmppClient is a parent task for XmppTasks.
//
// XmppClient is a task which is designed to be the parent task for
// all tasks that depend on a single Xmpp connection.  If you want to,
// for example, listen for subscription requests forever, then your
// listener should be a task that is a child of the XmppClient that owns
// the connection you are using.  XmppClient has all the utility methods
// that basically drill through to XmppEngine.
//
// XmppClient is just a wrapper for XmppEngine, and if I were writing it
// all over again, I would make XmppClient == XmppEngine.  Why?
// XmppEngine needs tasks too, for example it has an XmppLoginTask which
// should just be the same kind of Task instead of an XmppEngine specific
// thing.  It would help do certain things like GAIA auth cleaner.
//
/////////////////////////////////////////////////////////////////////

class XmppClient : public XmppTaskParentInterface,
                   public XmppClientInterface,
                   public sigslot::has_slots<>
{
public:
  explicit XmppClient(rtc::TaskParent * parent);
  virtual ~XmppClient();

  XmppReturnStatus Connect(const XmppClientSettings & settings,
                           const std::string & lang,
                           AsyncSocket * socket,
                           PreXmppAuth * preauth);

  virtual int ProcessStart();
  virtual int ProcessResponse();
  XmppReturnStatus Disconnect();

  sigslot::signal1<XmppEngine::State> SignalStateChange;
  XmppEngine::Error GetError(int *subcode);

  // When there is a <stream:error> stanza, return the stanza
  // so that they can be handled.
  const XmlElement *GetStreamError();

  // When there is an authentication error, we may have captcha info
  // that the user can use to unlock their account
  CaptchaChallenge GetCaptchaChallenge();

  // When authentication is successful, this returns the service token
  // (if we used GAIA authentication)
  std::string GetAuthMechanism();
  std::string GetAuthToken();

  XmppReturnStatus SendRaw(const std::string & text);

  XmppEngine* engine();

  sigslot::signal2<const char *, int> SignalLogInput;
  sigslot::signal2<const char *, int> SignalLogOutput;

  // As XmppTaskParentIntreface
  virtual XmppClientInterface* GetClient() { return this; }

  // As XmppClientInterface
  virtual XmppEngine::State GetState() const;
  virtual const Jid& jid() const;
  virtual std::string NextId();
  virtual XmppReturnStatus SendStanza(const XmlElement *stanza);
  virtual XmppReturnStatus SendStanzaError(const XmlElement * pelOriginal,
                                           XmppStanzaError code,
                                           const std::string & text);
  virtual void AddXmppTask(XmppTask *, XmppEngine::HandlerLevel);
  virtual void RemoveXmppTask(XmppTask *);

 private:
  friend class XmppTask;

  void OnAuthDone();

  // Internal state management
  enum {
    STATE_PRE_XMPP_LOGIN = STATE_NEXT,
    STATE_START_XMPP_LOGIN = STATE_NEXT + 1,
  };
  int Process(int state) {
    switch (state) {
      case STATE_PRE_XMPP_LOGIN: return ProcessTokenLogin();
      case STATE_START_XMPP_LOGIN: return ProcessStartXmppLogin();
      default: return Task::Process(state);
    }
  }

  std::string GetStateName(int state) const {
    switch (state) {
      case STATE_PRE_XMPP_LOGIN:      return "PRE_XMPP_LOGIN";
      case STATE_START_XMPP_LOGIN:  return "START_XMPP_LOGIN";
      default: return Task::GetStateName(state);
    }
  }

  int ProcessTokenLogin();
  int ProcessStartXmppLogin();
  void EnsureClosed();

  class Private;
  friend class Private;
  std::unique_ptr<Private> d_;

  bool delivering_signal_;
  bool valid_;
};

}

#endif  // WEBRTC_LIBJINGLE_XMPP_XMPPCLIENT_H_
