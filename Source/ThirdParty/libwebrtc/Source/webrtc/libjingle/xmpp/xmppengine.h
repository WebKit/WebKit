/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_XMPPENGINE_H_
#define WEBRTC_LIBJINGLE_XMPP_XMPPENGINE_H_

// also part of the API
#include "webrtc/libjingle/xmllite/qname.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/jid.h"


namespace buzz {

class XmppEngine;
class SaslHandler;
typedef void * XmppIqCookie;

//! XMPP stanza error codes.
//! Used in XmppEngine.SendStanzaError().
enum XmppStanzaError {
  XSE_BAD_REQUEST,
  XSE_CONFLICT,
  XSE_FEATURE_NOT_IMPLEMENTED,
  XSE_FORBIDDEN,
  XSE_GONE,
  XSE_INTERNAL_SERVER_ERROR,
  XSE_ITEM_NOT_FOUND,
  XSE_JID_MALFORMED,
  XSE_NOT_ACCEPTABLE,
  XSE_NOT_ALLOWED,
  XSE_PAYMENT_REQUIRED,
  XSE_RECIPIENT_UNAVAILABLE,
  XSE_REDIRECT,
  XSE_REGISTRATION_REQUIRED,
  XSE_SERVER_NOT_FOUND,
  XSE_SERVER_TIMEOUT,
  XSE_RESOURCE_CONSTRAINT,
  XSE_SERVICE_UNAVAILABLE,
  XSE_SUBSCRIPTION_REQUIRED,
  XSE_UNDEFINED_CONDITION,
  XSE_UNEXPECTED_REQUEST,
};

// XmppReturnStatus
//    This is used by API functions to synchronously return status.
enum XmppReturnStatus {
  XMPP_RETURN_OK,
  XMPP_RETURN_BADARGUMENT,
  XMPP_RETURN_BADSTATE,
  XMPP_RETURN_PENDING,
  XMPP_RETURN_UNEXPECTED,
  XMPP_RETURN_NOTYETIMPLEMENTED,
};

// TlsOptions
//    This is used by API to identify TLS setting.
enum TlsOptions {
  TLS_DISABLED,
  TLS_ENABLED,
  TLS_REQUIRED
};

//! Callback for socket output for an XmppEngine connection.
//! Register via XmppEngine.SetOutputHandler.  An XmppEngine
//! can call back to this handler while it is processing
//! Connect, SendStanza, SendIq, Disconnect, or HandleInput.
class XmppOutputHandler {
public:
  virtual ~XmppOutputHandler() {}

  //! Deliver the specified bytes to the XMPP socket.
  virtual void WriteOutput(const char * bytes, size_t len) = 0;

  //! Initiate TLS encryption on the socket.
  //! The implementation must verify that the SSL
  //! certificate matches the given domainname.
  virtual void StartTls(const std::string & domainname) = 0;

  //! Called when engine wants the connecton closed.
  virtual void CloseConnection() = 0;
};

//! Callback to deliver engine state change notifications
//! to the object managing the engine.
class XmppSessionHandler {
public:
  virtual ~XmppSessionHandler() {}
  //! Called when engine changes state. Argument is new state.
  virtual void OnStateChange(int state) = 0;
};

//! Callback to deliver stanzas to an Xmpp application module.
//! Register via XmppEngine.SetDefaultSessionHandler or via
//! XmppEngine.AddSessionHAndler.  
class XmppStanzaHandler {
public:
  virtual ~XmppStanzaHandler() {}
  //! Process the given stanza.
  //! The handler must return true if it has handled the stanza.
  //! A false return value causes the stanza to be passed on to
  //! the next registered handler.
  virtual bool HandleStanza(const XmlElement * stanza) = 0;
};

//! Callback to deliver iq responses (results and errors).
//! Register while sending an iq via XmppEngine.SendIq.
//! Iq responses are routed to matching XmppIqHandlers in preference
//! to sending to any registered SessionHandlers.
class XmppIqHandler {
public:
  virtual ~XmppIqHandler() {}
  //! Called to handle the iq response.
  //! The response may be either a result or an error, and will have
  //! an 'id' that matches the request and a 'from' that matches the
  //! 'to' of the request.  Called no more than once; once this is
  //! called, the handler is automatically unregistered.
  virtual void IqResponse(XmppIqCookie cookie, const XmlElement * pelStanza) = 0;
};

//! The XMPP connection engine.
//! This engine implements the client side of the 'core' XMPP protocol.
//! To use it, register an XmppOutputHandler to handle socket output
//! and pass socket input to HandleInput.  Then application code can
//! set up the connection with a user, password, and other settings,
//! and then call Connect() to initiate the connection.
//! An application can listen for events and receive stanzas by
//! registering an XmppStanzaHandler via AddStanzaHandler().
class XmppEngine {
public:
  static XmppEngine * Create();
  virtual ~XmppEngine() {}

  //! Error codes. See GetError().
  enum Error {
    ERROR_NONE = 0,         //!< No error
    ERROR_XML,              //!< Malformed XML or encoding error
    ERROR_STREAM,           //!< XMPP stream error - see GetStreamError()
    ERROR_VERSION,          //!< XMPP version error
    ERROR_UNAUTHORIZED,     //!< User is not authorized (rejected credentials)
    ERROR_TLS,              //!< TLS could not be negotiated
    ERROR_AUTH,             //!< Authentication could not be negotiated
    ERROR_BIND,             //!< Resource or session binding could not be negotiated
    ERROR_CONNECTION_CLOSED,//!< Connection closed by output handler.
    ERROR_DOCUMENT_CLOSED,  //!< Closed by </stream:stream>
    ERROR_SOCKET,           //!< Socket error
    ERROR_NETWORK_TIMEOUT,  //!< Some sort of timeout (eg., we never got the roster)
    ERROR_MISSING_USERNAME  //!< User has a Google Account but no nickname
  };

  //! States.  See GetState().
  enum State {
    STATE_NONE = 0,        //!< Nonexistent state
    STATE_START,           //!< Initial state.
    STATE_OPENING,         //!< Exchanging stream headers, authenticating and so on.
    STATE_OPEN,            //!< Authenticated and bound.
    STATE_CLOSED,          //!< Session closed, possibly due to error.
  };

  // SOCKET INPUT AND OUTPUT ------------------------------------------------

  //! Registers the handler for socket output
  virtual XmppReturnStatus SetOutputHandler(XmppOutputHandler *pxoh) = 0;

  //! Provides socket input to the engine
  virtual XmppReturnStatus HandleInput(const char * bytes, size_t len) = 0;

  //! Advises the engine that the socket has closed
  virtual XmppReturnStatus ConnectionClosed(int subcode) = 0;

  // SESSION SETUP ---------------------------------------------------------

  //! Indicates the (bare) JID for the user to use.
  virtual XmppReturnStatus SetUser(const Jid & jid)= 0;

  //! Get the login (bare) JID.
  virtual const Jid & GetUser() = 0;

  //! Provides different methods for credentials for login.
  //! Takes ownership of this object; deletes when login is done
  virtual XmppReturnStatus SetSaslHandler(SaslHandler * h) = 0;

  //! Sets whether TLS will be used within the connection (default true).
  virtual XmppReturnStatus SetTls(TlsOptions useTls) = 0;

  //! Sets an alternate domain from which we allows TLS certificates.
  //! This is for use in the case where a we want to allow a proxy to
  //! serve up its own certificate rather than one owned by the underlying
  //! domain.
  virtual XmppReturnStatus SetTlsServer(const std::string & proxy_hostname, 
                                        const std::string & proxy_domain) = 0;

  //! Gets whether TLS will be used within the connection.
  virtual TlsOptions GetTls() = 0;

  //! Sets the request resource name, if any (optional).
  //! Note that the resource name may be overridden by the server; after
  //! binding, the actual resource name is available as part of FullJid().
  virtual XmppReturnStatus SetRequestedResource(const std::string& resource) = 0;

  //! Gets the request resource name.
  virtual const std::string & GetRequestedResource() = 0;

  //! Sets language
  virtual void SetLanguage(const std::string & lang) = 0;

  // SESSION MANAGEMENT ---------------------------------------------------

  //! Set callback for state changes.
  virtual XmppReturnStatus SetSessionHandler(XmppSessionHandler* handler) = 0;

  //! Initiates the XMPP connection.
  //! After supplying connection settings, call this once to initiate,
  //! (optionally) encrypt, authenticate, and bind the connection.
  virtual XmppReturnStatus Connect() = 0;

  //! The current engine state.
  virtual State GetState() = 0;

  //! Returns true if the connection is encrypted (under TLS)
  virtual bool IsEncrypted() = 0;

  //! The error code.
  //! Consult this after XmppOutputHandler.OnClose().
  virtual Error GetError(int *subcode) = 0;

  //! The stream:error stanza, when the error is XmppEngine::ERROR_STREAM.
  //! Notice the stanza returned is owned by the XmppEngine and
  //! is deleted when the engine is destroyed.
  virtual const XmlElement * GetStreamError() = 0;

  //! Closes down the connection.
  //! Sends CloseConnection to output, and disconnects and registered
  //! session handlers.  After Disconnect completes, it is guaranteed
  //! that no further callbacks will be made.
  virtual XmppReturnStatus Disconnect() = 0;

  // APPLICATION USE -------------------------------------------------------

  enum HandlerLevel {
    HL_NONE = 0,
    HL_PEEK,   //!< Sees messages before all other processing; cannot abort
    HL_SINGLE, //!< Watches for a single message, e.g., by id and sender
    HL_SENDER, //!< Watches for a type of message from a specific sender
    HL_TYPE,   //!< Watches a type of message, e.g., all groupchat msgs
    HL_ALL,    //!< Watches all messages - gets last shot
    HL_COUNT,  //!< Count of handler levels
  };

  //! Adds a listener for session events.
  //! Stanza delivery is chained to session handlers; the first to
  //! return 'true' is the last to get each stanza.
  virtual XmppReturnStatus AddStanzaHandler(XmppStanzaHandler* handler, HandlerLevel level = HL_PEEK) = 0;

  //! Removes a listener for session events.
  virtual XmppReturnStatus RemoveStanzaHandler(XmppStanzaHandler* handler) = 0;

  //! Sends a stanza to the server.
  virtual XmppReturnStatus SendStanza(const XmlElement * pelStanza) = 0;

  //! Sends raw text to the server
  virtual XmppReturnStatus SendRaw(const std::string & text) = 0;

  //! Sends an iq to the server, and registers a callback for the result.
  //! Returns the cookie passed to the result handler.
  virtual XmppReturnStatus SendIq(const XmlElement* pelStanza,
                                  XmppIqHandler* iq_handler,
                                  XmppIqCookie* cookie) = 0;

  //! Unregisters an iq callback handler given its cookie.
  //! No callback will come to this handler after it's unregistered.
  virtual XmppReturnStatus RemoveIqHandler(XmppIqCookie cookie,
                                      XmppIqHandler** iq_handler) = 0;


  //! Forms and sends an error in response to the given stanza.
  //! Swaps to and from, sets type to "error", and adds error information
  //! based on the passed code.  Text is optional and may be STR_EMPTY.
  virtual XmppReturnStatus SendStanzaError(const XmlElement * pelOriginal,
                                           XmppStanzaError code,
                                           const std::string & text) = 0;

  //! The fullly bound JID.
  //! This JID is only valid after binding has succeeded.  If the value
  //! is JID_NULL, the binding has not succeeded.
  virtual const Jid & FullJid() = 0;

  //! The next unused iq id for this connection.
  //! Call this when building iq stanzas, to ensure that each iq
  //! gets its own unique id.
  virtual std::string NextId() = 0;

};

}


// Move these to a better location

#define XMPP_FAILED(x)                      \
  ( (x) == buzz::XMPP_RETURN_OK ? false : true)   \


#define XMPP_SUCCEEDED(x)                   \
  ( (x) == buzz::XMPP_RETURN_OK ? true : false)   \

#define IFR(x)                        \
  do {                                \
    xmpp_status = (x);                \
    if (XMPP_FAILED(xmpp_status)) {   \
      return xmpp_status;             \
    }                                 \
  } while (false)                     \


#define IFC(x)                        \
  do {                                \
    xmpp_status = (x);                \
    if (XMPP_FAILED(xmpp_status)) {   \
      goto Cleanup;                   \
    }                                 \
  } while (false)                     \


#endif  // WEBRTC_LIBJINGLE_XMPP_XMPPENGINE_H_
