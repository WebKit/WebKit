/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_XMPPENGINEIMPL_H_
#define WEBRTC_LIBJINGLE_XMPP_XMPPENGINEIMPL_H_

#include <memory>
#include <sstream>
#include <vector>

#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/libjingle/xmpp/xmppstanzaparser.h"

namespace buzz {

class XmppLoginTask;
class XmppEngine;
class XmppIqEntry;
class SaslHandler;
class SaslMechanism;

//! The XMPP connection engine.
//! This engine implements the client side of the 'core' XMPP protocol.
//! To use it, register an XmppOutputHandler to handle socket output
//! and pass socket input to HandleInput.  Then application code can
//! set up the connection with a user, password, and other settings,
//! and then call Connect() to initiate the connection.
//! An application can listen for events and receive stanzas by
//! registering an XmppStanzaHandler via AddStanzaHandler().
class XmppEngineImpl : public XmppEngine {
 public:
  XmppEngineImpl();
  virtual ~XmppEngineImpl();

  // SOCKET INPUT AND OUTPUT ------------------------------------------------

  //! Registers the handler for socket output
  virtual XmppReturnStatus SetOutputHandler(XmppOutputHandler *pxoh);

  //! Provides socket input to the engine
  virtual XmppReturnStatus HandleInput(const char* bytes, size_t len);

  //! Advises the engine that the socket has closed
  virtual XmppReturnStatus ConnectionClosed(int subcode);

  // SESSION SETUP ---------------------------------------------------------

  //! Indicates the (bare) JID for the user to use.
  virtual XmppReturnStatus SetUser(const Jid& jid);

  //! Get the login (bare) JID.
  virtual const Jid& GetUser();

  //! Indicates the autentication to use.  Takes ownership of the object.
  virtual XmppReturnStatus SetSaslHandler(SaslHandler* sasl_handler);

  //! Sets whether TLS will be used within the connection (default true).
  virtual XmppReturnStatus SetTls(TlsOptions use_tls);

  //! Sets an alternate domain from which we allows TLS certificates.
  //! This is for use in the case where a we want to allow a proxy to
  //! serve up its own certificate rather than one owned by the underlying
  //! domain.
  virtual XmppReturnStatus SetTlsServer(const std::string& proxy_hostname,
                                        const std::string& proxy_domain);

  //! Gets whether TLS will be used within the connection.
  virtual TlsOptions GetTls();

  //! Sets the request resource name, if any (optional).
  //! Note that the resource name may be overridden by the server; after
  //! binding, the actual resource name is available as part of FullJid().
  virtual XmppReturnStatus SetRequestedResource(const std::string& resource);

  //! Gets the request resource name.
  virtual const std::string& GetRequestedResource();

  //! Sets language
  virtual void SetLanguage(const std::string& lang) {
    lang_ = lang;
  }

  // SESSION MANAGEMENT ---------------------------------------------------

  //! Set callback for state changes.
  virtual XmppReturnStatus SetSessionHandler(XmppSessionHandler* handler);

  //! Initiates the XMPP connection.
  //! After supplying connection settings, call this once to initiate,
  //! (optionally) encrypt, authenticate, and bind the connection.
  virtual XmppReturnStatus Connect();

  //! The current engine state.
  virtual State GetState() { return state_; }

  //! Returns true if the connection is encrypted (under TLS)
  virtual bool IsEncrypted() { return encrypted_; }

  //! The error code.
  //! Consult this after XmppOutputHandler.OnClose().
  virtual Error GetError(int *subcode) {
     if (subcode) {
       *subcode = subcode_;
     }
     return error_code_;
  }

  //! The stream:error stanza, when the error is XmppEngine::ERROR_STREAM.
  //! Notice the stanza returned is owned by the XmppEngine and
  //! is deleted when the engine is destroyed.
  virtual const XmlElement* GetStreamError() { return stream_error_.get(); }

  //! Closes down the connection.
  //! Sends CloseConnection to output, and disconnects and registered
  //! session handlers.  After Disconnect completes, it is guaranteed
  //! that no further callbacks will be made.
  virtual XmppReturnStatus Disconnect();

  // APPLICATION USE -------------------------------------------------------

  //! Adds a listener for session events.
  //! Stanza delivery is chained to session handlers; the first to
  //! return 'true' is the last to get each stanza.
  virtual XmppReturnStatus AddStanzaHandler(XmppStanzaHandler* handler,
                                            XmppEngine::HandlerLevel level);

  //! Removes a listener for session events.
  virtual XmppReturnStatus RemoveStanzaHandler(XmppStanzaHandler* handler);

  //! Sends a stanza to the server.
  virtual XmppReturnStatus SendStanza(const XmlElement* stanza);

  //! Sends raw text to the server
  virtual XmppReturnStatus SendRaw(const std::string& text);

  //! Sends an iq to the server, and registers a callback for the result.
  //! Returns the cookie passed to the result handler.
  virtual XmppReturnStatus SendIq(const XmlElement* stanza,
                                  XmppIqHandler* iq_handler,
                                  XmppIqCookie* cookie);

  //! Unregisters an iq callback handler given its cookie.
  //! No callback will come to this handler after it's unregistered.
  virtual XmppReturnStatus RemoveIqHandler(XmppIqCookie cookie,
                                      XmppIqHandler** iq_handler);

  //! Forms and sends an error in response to the given stanza.
  //! Swaps to and from, sets type to "error", and adds error information
  //! based on the passed code.  Text is optional and may be STR_EMPTY.
  virtual XmppReturnStatus SendStanzaError(const XmlElement* pelOriginal,
                                           XmppStanzaError code,
                                           const std::string& text);

  //! The fullly bound JID.
  //! This JID is only valid after binding has succeeded.  If the value
  //! is JID_NULL, the binding has not succeeded.
  virtual const Jid& FullJid() { return bound_jid_; }

  //! The next unused iq id for this connection.
  //! Call this when building iq stanzas, to ensure that each iq
  //! gets its own unique id.
  virtual std::string NextId();

 private:
  friend class XmppLoginTask;
  friend class XmppIqEntry;

  void IncomingStanza(const XmlElement *stanza);
  void IncomingStart(const XmlElement *stanza);
  void IncomingEnd(bool isError);

  void InternalSendStart(const std::string& domainName);
  void InternalSendStanza(const XmlElement* stanza);
  std::string ChooseBestSaslMechanism(
      const std::vector<std::string>& mechanisms, bool encrypted);
  SaslMechanism* GetSaslMechanism(const std::string& name);
  void SignalBound(const Jid& fullJid);
  void SignalStreamError(const XmlElement* streamError);
  void SignalError(Error errorCode, int subCode);
  bool HasError();
  void DeleteIqCookies();
  bool HandleIqResponse(const XmlElement* element);
  void StartTls(const std::string& domain);
  void RaiseReset() { raised_reset_ = true; }

  class StanzaParseHandler : public XmppStanzaParseHandler {
   public:
    StanzaParseHandler(XmppEngineImpl* outer) : outer_(outer) {}
    virtual ~StanzaParseHandler() {}

    virtual void StartStream(const XmlElement* stream) {
      outer_->IncomingStart(stream);
    }
    virtual void Stanza(const XmlElement* stanza) {
      outer_->IncomingStanza(stanza);
    }
    virtual void EndStream() {
      outer_->IncomingEnd(false);
    }
    virtual void XmlError() {
      outer_->IncomingEnd(true);
    }

   private:
    XmppEngineImpl* const outer_;
  };

  class EnterExit {
   public:
    EnterExit(XmppEngineImpl* engine);
    ~EnterExit();
   private:
    XmppEngineImpl* engine_;
    State state_;
  };

  friend class StanzaParseHandler;
  friend class EnterExit;

  StanzaParseHandler stanza_parse_handler_;
  XmppStanzaParser stanza_parser_;

  // state
  int engine_entered_;
  Jid user_jid_;
  std::string password_;
  std::string requested_resource_;
  TlsOptions tls_option_;
  std::string tls_server_hostname_;
  std::string tls_server_domain_;
  std::unique_ptr<XmppLoginTask> login_task_;
  std::string lang_;

  int next_id_;
  Jid bound_jid_;
  State state_;
  bool encrypted_;
  Error error_code_;
  int subcode_;
  std::unique_ptr<XmlElement> stream_error_;
  bool raised_reset_;
  XmppOutputHandler* output_handler_;
  XmppSessionHandler* session_handler_;

  XmlnsStack xmlns_stack_;

  typedef std::vector<XmppStanzaHandler*> StanzaHandlerVector;
  std::unique_ptr<StanzaHandlerVector> stanza_handlers_[HL_COUNT];

  typedef std::vector<XmppIqEntry*> IqEntryVector;
  std::unique_ptr<IqEntryVector> iq_entries_;

  std::unique_ptr<SaslHandler> sasl_handler_;

  std::unique_ptr<std::stringstream> output_;
};

}  // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMPP_XMPPENGINEIMPL_H_
