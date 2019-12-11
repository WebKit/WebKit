/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/relayserver.h"

#ifdef WEBRTC_POSIX
#include <errno.h>
#endif  // WEBRTC_POSIX

#include <algorithm>
#include <utility>

#include "rtc_base/asynctcpsocket.h"
#include "rtc_base/checks.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/socketadapters.h"

namespace cricket {

// By default, we require a ping every 90 seconds.
const int MAX_LIFETIME = 15 * 60 * 1000;

// The number of bytes in each of the usernames we use.
const uint32_t USERNAME_LENGTH = 16;

// Calls SendTo on the given socket and logs any bad results.
void Send(rtc::AsyncPacketSocket* socket,
          const char* bytes,
          size_t size,
          const rtc::SocketAddress& addr) {
  rtc::PacketOptions options;
  int result = socket->SendTo(bytes, size, addr, options);
  if (result < static_cast<int>(size)) {
    RTC_LOG(LS_ERROR) << "SendTo wrote only " << result << " of " << size
                      << " bytes";
  } else if (result < 0) {
    RTC_LOG_ERR(LS_ERROR) << "SendTo";
  }
}

// Sends the given STUN message on the given socket.
void SendStun(const StunMessage& msg,
              rtc::AsyncPacketSocket* socket,
              const rtc::SocketAddress& addr) {
  rtc::ByteBufferWriter buf;
  msg.Write(&buf);
  Send(socket, buf.Data(), buf.Length(), addr);
}

// Constructs a STUN error response and sends it on the given socket.
void SendStunError(const StunMessage& msg,
                   rtc::AsyncPacketSocket* socket,
                   const rtc::SocketAddress& remote_addr,
                   int error_code,
                   const char* error_desc,
                   const std::string& magic_cookie) {
  RelayMessage err_msg;
  err_msg.SetType(GetStunErrorResponseType(msg.type()));
  err_msg.SetTransactionID(msg.transaction_id());

  auto magic_cookie_attr =
      StunAttribute::CreateByteString(cricket::STUN_ATTR_MAGIC_COOKIE);
  if (magic_cookie.size() == 0) {
    magic_cookie_attr->CopyBytes(cricket::TURN_MAGIC_COOKIE_VALUE,
                                 sizeof(cricket::TURN_MAGIC_COOKIE_VALUE));
  } else {
    magic_cookie_attr->CopyBytes(magic_cookie.c_str(), magic_cookie.size());
  }
  err_msg.AddAttribute(std::move(magic_cookie_attr));

  auto err_code = StunAttribute::CreateErrorCode();
  err_code->SetClass(error_code / 100);
  err_code->SetNumber(error_code % 100);
  err_code->SetReason(error_desc);
  err_msg.AddAttribute(std::move(err_code));

  SendStun(err_msg, socket, remote_addr);
}

RelayServer::RelayServer(rtc::Thread* thread)
    : thread_(thread), random_(rtc::SystemTimeNanos()), log_bindings_(true) {}

RelayServer::~RelayServer() {
  // Deleting the binding will cause it to be removed from the map.
  while (!bindings_.empty())
    delete bindings_.begin()->second;
  for (size_t i = 0; i < internal_sockets_.size(); ++i)
    delete internal_sockets_[i];
  for (size_t i = 0; i < external_sockets_.size(); ++i)
    delete external_sockets_[i];
  for (size_t i = 0; i < removed_sockets_.size(); ++i)
    delete removed_sockets_[i];
  while (!server_sockets_.empty()) {
    rtc::AsyncSocket* socket = server_sockets_.begin()->first;
    server_sockets_.erase(server_sockets_.begin()->first);
    delete socket;
  }
}

void RelayServer::AddInternalSocket(rtc::AsyncPacketSocket* socket) {
  RTC_DCHECK(internal_sockets_.end() == std::find(internal_sockets_.begin(),
                                                  internal_sockets_.end(),
                                                  socket));
  internal_sockets_.push_back(socket);
  socket->SignalReadPacket.connect(this, &RelayServer::OnInternalPacket);
}

void RelayServer::RemoveInternalSocket(rtc::AsyncPacketSocket* socket) {
  auto iter =
      std::find(internal_sockets_.begin(), internal_sockets_.end(), socket);
  RTC_DCHECK(iter != internal_sockets_.end());
  internal_sockets_.erase(iter);
  removed_sockets_.push_back(socket);
  socket->SignalReadPacket.disconnect(this);
}

void RelayServer::AddExternalSocket(rtc::AsyncPacketSocket* socket) {
  RTC_DCHECK(external_sockets_.end() == std::find(external_sockets_.begin(),
                                                  external_sockets_.end(),
                                                  socket));
  external_sockets_.push_back(socket);
  socket->SignalReadPacket.connect(this, &RelayServer::OnExternalPacket);
}

void RelayServer::RemoveExternalSocket(rtc::AsyncPacketSocket* socket) {
  auto iter =
      std::find(external_sockets_.begin(), external_sockets_.end(), socket);
  RTC_DCHECK(iter != external_sockets_.end());
  external_sockets_.erase(iter);
  removed_sockets_.push_back(socket);
  socket->SignalReadPacket.disconnect(this);
}

void RelayServer::AddInternalServerSocket(rtc::AsyncSocket* socket,
                                          cricket::ProtocolType proto) {
  RTC_DCHECK(server_sockets_.end() == server_sockets_.find(socket));
  server_sockets_[socket] = proto;
  socket->SignalReadEvent.connect(this, &RelayServer::OnReadEvent);
}

void RelayServer::RemoveInternalServerSocket(rtc::AsyncSocket* socket) {
  auto iter = server_sockets_.find(socket);
  RTC_DCHECK(iter != server_sockets_.end());
  server_sockets_.erase(iter);
  socket->SignalReadEvent.disconnect(this);
}

int RelayServer::GetConnectionCount() const {
  return static_cast<int>(connections_.size());
}

rtc::SocketAddressPair RelayServer::GetConnection(int connection) const {
  int i = 0;
  for (const auto& entry : connections_) {
    if (i == connection) {
      return entry.second->addr_pair();
    }
    ++i;
  }
  return rtc::SocketAddressPair();
}

bool RelayServer::HasConnection(const rtc::SocketAddress& address) const {
  for (const auto& entry : connections_) {
    if (entry.second->addr_pair().destination() == address) {
      return true;
    }
  }
  return false;
}

void RelayServer::OnReadEvent(rtc::AsyncSocket* socket) {
  RTC_DCHECK(server_sockets_.find(socket) != server_sockets_.end());
  AcceptConnection(socket);
}

void RelayServer::OnInternalPacket(rtc::AsyncPacketSocket* socket,
                                   const char* bytes,
                                   size_t size,
                                   const rtc::SocketAddress& remote_addr,
                                   const int64_t& /* packet_time_us */) {
  // Get the address of the connection we just received on.
  rtc::SocketAddressPair ap(remote_addr, socket->GetLocalAddress());
  RTC_DCHECK(!ap.destination().IsNil());

  // If this did not come from an existing connection, it should be a STUN
  // allocate request.
  auto piter = connections_.find(ap);
  if (piter == connections_.end()) {
    HandleStunAllocate(bytes, size, ap, socket);
    return;
  }

  RelayServerConnection* int_conn = piter->second;

  // Handle STUN requests to the server itself.
  if (int_conn->binding()->HasMagicCookie(bytes, size)) {
    HandleStun(int_conn, bytes, size);
    return;
  }

  // Otherwise, this is a non-wrapped packet that we are to forward.  Make sure
  // that this connection has been locked.  (Otherwise, we would not know what
  // address to forward to.)
  if (!int_conn->locked()) {
    RTC_LOG(LS_WARNING) << "Dropping packet: connection not locked";
    return;
  }

  // Forward this to the destination address into the connection.
  RelayServerConnection* ext_conn = int_conn->binding()->GetExternalConnection(
      int_conn->default_destination());
  if (ext_conn && ext_conn->locked()) {
    // TODO(?): Check the HMAC.
    ext_conn->Send(bytes, size);
  } else {
    // This happens very often and is not an error.
    RTC_LOG(LS_INFO) << "Dropping packet: no external connection";
  }
}

void RelayServer::OnExternalPacket(rtc::AsyncPacketSocket* socket,
                                   const char* bytes,
                                   size_t size,
                                   const rtc::SocketAddress& remote_addr,
                                   const int64_t& /* packet_time_us */) {
  // Get the address of the connection we just received on.
  rtc::SocketAddressPair ap(remote_addr, socket->GetLocalAddress());
  RTC_DCHECK(!ap.destination().IsNil());

  // If this connection already exists, then forward the traffic.
  auto piter = connections_.find(ap);
  if (piter != connections_.end()) {
    // TODO(?): Check the HMAC.
    RelayServerConnection* ext_conn = piter->second;
    RelayServerConnection* int_conn =
        ext_conn->binding()->GetInternalConnection(
            ext_conn->addr_pair().source());
    RTC_DCHECK(int_conn != NULL);
    int_conn->Send(bytes, size, ext_conn->addr_pair().source());
    ext_conn->Lock();  // allow outgoing packets
    return;
  }

  // The first packet should always be a STUN / TURN packet.  If it isn't, then
  // we should just ignore this packet.
  RelayMessage msg;
  rtc::ByteBufferReader buf(bytes, size);
  if (!msg.Read(&buf)) {
    RTC_LOG(LS_WARNING) << "Dropping packet: first packet not STUN";
    return;
  }

  // The initial packet should have a username (which identifies the binding).
  const StunByteStringAttribute* username_attr =
      msg.GetByteString(STUN_ATTR_USERNAME);
  if (!username_attr) {
    RTC_LOG(LS_WARNING) << "Dropping packet: no username";
    return;
  }

  uint32_t length =
      std::min(static_cast<uint32_t>(username_attr->length()), USERNAME_LENGTH);
  std::string username(username_attr->bytes(), length);
  // TODO(?): Check the HMAC.

  // The binding should already be present.
  auto biter = bindings_.find(username);
  if (biter == bindings_.end()) {
    RTC_LOG(LS_WARNING) << "Dropping packet: no binding with username";
    return;
  }

  // Add this authenticted connection to the binding.
  RelayServerConnection* ext_conn =
      new RelayServerConnection(biter->second, ap, socket);
  ext_conn->binding()->AddExternalConnection(ext_conn);
  AddConnection(ext_conn);

  // We always know where external packets should be forwarded, so we can lock
  // them from the beginning.
  ext_conn->Lock();

  // Send this message on the appropriate internal connection.
  RelayServerConnection* int_conn = ext_conn->binding()->GetInternalConnection(
      ext_conn->addr_pair().source());
  RTC_DCHECK(int_conn != NULL);
  int_conn->Send(bytes, size, ext_conn->addr_pair().source());
}

bool RelayServer::HandleStun(const char* bytes,
                             size_t size,
                             const rtc::SocketAddress& remote_addr,
                             rtc::AsyncPacketSocket* socket,
                             std::string* username,
                             StunMessage* msg) {
  // Parse this into a stun message. Eat the message if this fails.
  rtc::ByteBufferReader buf(bytes, size);
  if (!msg->Read(&buf)) {
    return false;
  }

  // The initial packet should have a username (which identifies the binding).
  const StunByteStringAttribute* username_attr =
      msg->GetByteString(STUN_ATTR_USERNAME);
  if (!username_attr) {
    SendStunError(*msg, socket, remote_addr, 432, "Missing Username", "");
    return false;
  }

  // Record the username if requested.
  if (username)
    username->append(username_attr->bytes(), username_attr->length());

  // TODO(?): Check for unknown attributes (<= 0x7fff)

  return true;
}

void RelayServer::HandleStunAllocate(const char* bytes,
                                     size_t size,
                                     const rtc::SocketAddressPair& ap,
                                     rtc::AsyncPacketSocket* socket) {
  // Make sure this is a valid STUN request.
  RelayMessage request;
  std::string username;
  if (!HandleStun(bytes, size, ap.source(), socket, &username, &request))
    return;

  // Make sure this is a an allocate request.
  if (request.type() != STUN_ALLOCATE_REQUEST) {
    SendStunError(request, socket, ap.source(), 600, "Operation Not Supported",
                  "");
    return;
  }

  // TODO(?): Check the HMAC.

  // Find or create the binding for this username.

  RelayServerBinding* binding;

  auto biter = bindings_.find(username);
  if (biter != bindings_.end()) {
    binding = biter->second;
  } else {
    // NOTE: In the future, bindings will be created by the bot only.  This
    //       else-branch will then disappear.

    // Compute the appropriate lifetime for this binding.
    int lifetime = MAX_LIFETIME;
    const StunUInt32Attribute* lifetime_attr =
        request.GetUInt32(STUN_ATTR_LIFETIME);
    if (lifetime_attr)
      lifetime =
          std::min(lifetime, static_cast<int>(lifetime_attr->value() * 1000));

    binding = new RelayServerBinding(this, username, "0", lifetime);
    binding->SignalTimeout.connect(this, &RelayServer::OnTimeout);
    bindings_[username] = binding;

    if (log_bindings_) {
      RTC_LOG(LS_INFO) << "Added new binding " << username << ", "
                       << bindings_.size() << " total";
    }
  }

  // Add this connection to the binding.  It starts out unlocked.
  RelayServerConnection* int_conn =
      new RelayServerConnection(binding, ap, socket);
  binding->AddInternalConnection(int_conn);
  AddConnection(int_conn);

  // Now that we have a connection, this other method takes over.
  HandleStunAllocate(int_conn, request);
}

void RelayServer::HandleStun(RelayServerConnection* int_conn,
                             const char* bytes,
                             size_t size) {
  // Make sure this is a valid STUN request.
  RelayMessage request;
  std::string username;
  if (!HandleStun(bytes, size, int_conn->addr_pair().source(),
                  int_conn->socket(), &username, &request))
    return;

  // Make sure the username is the one were were expecting.
  if (username != int_conn->binding()->username()) {
    int_conn->SendStunError(request, 430, "Stale Credentials");
    return;
  }

  // TODO(?): Check the HMAC.

  // Send this request to the appropriate handler.
  if (request.type() == STUN_SEND_REQUEST)
    HandleStunSend(int_conn, request);
  else if (request.type() == STUN_ALLOCATE_REQUEST)
    HandleStunAllocate(int_conn, request);
  else
    int_conn->SendStunError(request, 600, "Operation Not Supported");
}

void RelayServer::HandleStunAllocate(RelayServerConnection* int_conn,
                                     const StunMessage& request) {
  // Create a response message that includes an address with which external
  // clients can communicate.

  RelayMessage response;
  response.SetType(STUN_ALLOCATE_RESPONSE);
  response.SetTransactionID(request.transaction_id());

  auto magic_cookie_attr =
      StunAttribute::CreateByteString(cricket::STUN_ATTR_MAGIC_COOKIE);
  magic_cookie_attr->CopyBytes(int_conn->binding()->magic_cookie().c_str(),
                               int_conn->binding()->magic_cookie().size());
  response.AddAttribute(std::move(magic_cookie_attr));

  RTC_DCHECK_GT(external_sockets_.size(), 0);
  size_t index =
      random_.Rand(rtc::dchecked_cast<uint32_t>(external_sockets_.size() - 1));
  rtc::SocketAddress ext_addr = external_sockets_[index]->GetLocalAddress();

  auto addr_attr = StunAttribute::CreateAddress(STUN_ATTR_MAPPED_ADDRESS);
  addr_attr->SetIP(ext_addr.ipaddr());
  addr_attr->SetPort(ext_addr.port());
  response.AddAttribute(std::move(addr_attr));

  auto res_lifetime_attr = StunAttribute::CreateUInt32(STUN_ATTR_LIFETIME);
  res_lifetime_attr->SetValue(int_conn->binding()->lifetime() / 1000);
  response.AddAttribute(std::move(res_lifetime_attr));

  // TODO(?): Support transport-prefs (preallocate RTCP port).
  // TODO(?): Support bandwidth restrictions.
  // TODO(?): Add message integrity check.

  // Send a response to the caller.
  int_conn->SendStun(response);
}

void RelayServer::HandleStunSend(RelayServerConnection* int_conn,
                                 const StunMessage& request) {
  const StunAddressAttribute* addr_attr =
      request.GetAddress(STUN_ATTR_DESTINATION_ADDRESS);
  if (!addr_attr) {
    int_conn->SendStunError(request, 400, "Bad Request");
    return;
  }

  const StunByteStringAttribute* data_attr =
      request.GetByteString(STUN_ATTR_DATA);
  if (!data_attr) {
    int_conn->SendStunError(request, 400, "Bad Request");
    return;
  }

  rtc::SocketAddress ext_addr(addr_attr->ipaddr(), addr_attr->port());
  RelayServerConnection* ext_conn =
      int_conn->binding()->GetExternalConnection(ext_addr);
  if (!ext_conn) {
    // Create a new connection to establish the relationship with this binding.
    RTC_DCHECK(external_sockets_.size() == 1);
    rtc::AsyncPacketSocket* socket = external_sockets_[0];
    rtc::SocketAddressPair ap(ext_addr, socket->GetLocalAddress());
    ext_conn = new RelayServerConnection(int_conn->binding(), ap, socket);
    ext_conn->binding()->AddExternalConnection(ext_conn);
    AddConnection(ext_conn);
  }

  // If this connection has pinged us, then allow outgoing traffic.
  if (ext_conn->locked())
    ext_conn->Send(data_attr->bytes(), data_attr->length());

  const StunUInt32Attribute* options_attr =
      request.GetUInt32(STUN_ATTR_OPTIONS);
  if (options_attr && (options_attr->value() & 0x01)) {
    int_conn->set_default_destination(ext_addr);
    int_conn->Lock();

    RelayMessage response;
    response.SetType(STUN_SEND_RESPONSE);
    response.SetTransactionID(request.transaction_id());

    auto magic_cookie_attr =
        StunAttribute::CreateByteString(cricket::STUN_ATTR_MAGIC_COOKIE);
    magic_cookie_attr->CopyBytes(int_conn->binding()->magic_cookie().c_str(),
                                 int_conn->binding()->magic_cookie().size());
    response.AddAttribute(std::move(magic_cookie_attr));

    auto options2_attr =
        StunAttribute::CreateUInt32(cricket::STUN_ATTR_OPTIONS);
    options2_attr->SetValue(0x01);
    response.AddAttribute(std::move(options2_attr));

    int_conn->SendStun(response);
  }
}

void RelayServer::AddConnection(RelayServerConnection* conn) {
  RTC_DCHECK(connections_.find(conn->addr_pair()) == connections_.end());
  connections_[conn->addr_pair()] = conn;
}

void RelayServer::RemoveConnection(RelayServerConnection* conn) {
  auto iter = connections_.find(conn->addr_pair());
  RTC_DCHECK(iter != connections_.end());
  connections_.erase(iter);
}

void RelayServer::RemoveBinding(RelayServerBinding* binding) {
  auto iter = bindings_.find(binding->username());
  RTC_DCHECK(iter != bindings_.end());
  bindings_.erase(iter);

  if (log_bindings_) {
    RTC_LOG(LS_INFO) << "Removed binding " << binding->username() << ", "
                     << bindings_.size() << " remaining";
  }
}

void RelayServer::OnMessage(rtc::Message* pmsg) {
  static const uint32_t kMessageAcceptConnection = 1;
  RTC_DCHECK(pmsg->message_id == kMessageAcceptConnection);

  rtc::MessageData* data = pmsg->pdata;
  rtc::AsyncSocket* socket =
      static_cast<rtc::TypedMessageData<rtc::AsyncSocket*>*>(data)->data();
  AcceptConnection(socket);
  delete data;
}

void RelayServer::OnTimeout(RelayServerBinding* binding) {
  // This call will result in all of the necessary clean-up. We can't call
  // delete here, because you can't delete an object that is signaling you.
  thread_->Dispose(binding);
}

void RelayServer::AcceptConnection(rtc::AsyncSocket* server_socket) {
  // Check if someone is trying to connect to us.
  rtc::SocketAddress accept_addr;
  rtc::AsyncSocket* accepted_socket = server_socket->Accept(&accept_addr);
  if (accepted_socket != NULL) {
    // We had someone trying to connect, now check which protocol to
    // use and create a packet socket.
    RTC_DCHECK(server_sockets_[server_socket] == cricket::PROTO_TCP ||
               server_sockets_[server_socket] == cricket::PROTO_SSLTCP);
    if (server_sockets_[server_socket] == cricket::PROTO_SSLTCP) {
      accepted_socket = new rtc::AsyncSSLServerSocket(accepted_socket);
    }
    rtc::AsyncTCPSocket* tcp_socket =
        new rtc::AsyncTCPSocket(accepted_socket, false);

    // Finally add the socket so it can start communicating with the client.
    AddInternalSocket(tcp_socket);
  }
}

RelayServerConnection::RelayServerConnection(
    RelayServerBinding* binding,
    const rtc::SocketAddressPair& addrs,
    rtc::AsyncPacketSocket* socket)
    : binding_(binding), addr_pair_(addrs), socket_(socket), locked_(false) {
  // The creation of a new connection constitutes a use of the binding.
  binding_->NoteUsed();
}

RelayServerConnection::~RelayServerConnection() {
  // Remove this connection from the server's map (if it exists there).
  binding_->server()->RemoveConnection(this);
}

void RelayServerConnection::Send(const char* data, size_t size) {
  // Note that the binding has been used again.
  binding_->NoteUsed();

  cricket::Send(socket_, data, size, addr_pair_.source());
}

void RelayServerConnection::Send(const char* data,
                                 size_t size,
                                 const rtc::SocketAddress& from_addr) {
  // If the from address is known to the client, we don't need to send it.
  if (locked() && (from_addr == default_dest_)) {
    Send(data, size);
    return;
  }

  // Wrap the given data in a data-indication packet.

  RelayMessage msg;
  msg.SetType(STUN_DATA_INDICATION);

  auto magic_cookie_attr =
      StunAttribute::CreateByteString(cricket::STUN_ATTR_MAGIC_COOKIE);
  magic_cookie_attr->CopyBytes(binding_->magic_cookie().c_str(),
                               binding_->magic_cookie().size());
  msg.AddAttribute(std::move(magic_cookie_attr));

  auto addr_attr = StunAttribute::CreateAddress(STUN_ATTR_SOURCE_ADDRESS2);
  addr_attr->SetIP(from_addr.ipaddr());
  addr_attr->SetPort(from_addr.port());
  msg.AddAttribute(std::move(addr_attr));

  auto data_attr = StunAttribute::CreateByteString(STUN_ATTR_DATA);
  RTC_DCHECK(size <= 65536);
  data_attr->CopyBytes(data, uint16_t(size));
  msg.AddAttribute(std::move(data_attr));

  SendStun(msg);
}

void RelayServerConnection::SendStun(const StunMessage& msg) {
  // Note that the binding has been used again.
  binding_->NoteUsed();

  cricket::SendStun(msg, socket_, addr_pair_.source());
}

void RelayServerConnection::SendStunError(const StunMessage& request,
                                          int error_code,
                                          const char* error_desc) {
  // An error does not indicate use.  If no legitimate use off the binding
  // occurs, we want it to be cleaned up even if errors are still occuring.

  cricket::SendStunError(request, socket_, addr_pair_.source(), error_code,
                         error_desc, binding_->magic_cookie());
}

void RelayServerConnection::Lock() {
  locked_ = true;
}

void RelayServerConnection::Unlock() {
  locked_ = false;
}

// IDs used for posted messages:
const uint32_t MSG_LIFETIME_TIMER = 1;

RelayServerBinding::RelayServerBinding(RelayServer* server,
                                       const std::string& username,
                                       const std::string& password,
                                       int lifetime)
    : server_(server),
      username_(username),
      password_(password),
      lifetime_(lifetime) {
  // For now, every connection uses the standard magic cookie value.
  magic_cookie_.append(reinterpret_cast<const char*>(TURN_MAGIC_COOKIE_VALUE),
                       sizeof(TURN_MAGIC_COOKIE_VALUE));

  // Initialize the last-used time to now.
  NoteUsed();

  // Set the first timeout check.
  server_->thread()->PostDelayed(RTC_FROM_HERE, lifetime_, this,
                                 MSG_LIFETIME_TIMER);
}

RelayServerBinding::~RelayServerBinding() {
  // Clear the outstanding timeout check.
  server_->thread()->Clear(this);

  // Clean up all of the connections.
  for (size_t i = 0; i < internal_connections_.size(); ++i)
    delete internal_connections_[i];
  for (size_t i = 0; i < external_connections_.size(); ++i)
    delete external_connections_[i];

  // Remove this binding from the server's map.
  server_->RemoveBinding(this);
}

void RelayServerBinding::AddInternalConnection(RelayServerConnection* conn) {
  internal_connections_.push_back(conn);
}

void RelayServerBinding::AddExternalConnection(RelayServerConnection* conn) {
  external_connections_.push_back(conn);
}

void RelayServerBinding::NoteUsed() {
  last_used_ = rtc::TimeMillis();
}

bool RelayServerBinding::HasMagicCookie(const char* bytes, size_t size) const {
  if (size < 24 + magic_cookie_.size()) {
    return false;
  } else {
    return memcmp(bytes + 24, magic_cookie_.c_str(), magic_cookie_.size()) == 0;
  }
}

RelayServerConnection* RelayServerBinding::GetInternalConnection(
    const rtc::SocketAddress& ext_addr) {
  // Look for an internal connection that is locked to this address.
  for (size_t i = 0; i < internal_connections_.size(); ++i) {
    if (internal_connections_[i]->locked() &&
        (ext_addr == internal_connections_[i]->default_destination()))
      return internal_connections_[i];
  }

  // If one was not found, we send to the first connection.
  RTC_DCHECK(internal_connections_.size() > 0);
  return internal_connections_[0];
}

RelayServerConnection* RelayServerBinding::GetExternalConnection(
    const rtc::SocketAddress& ext_addr) {
  for (size_t i = 0; i < external_connections_.size(); ++i) {
    if (ext_addr == external_connections_[i]->addr_pair().source())
      return external_connections_[i];
  }
  return 0;
}

void RelayServerBinding::OnMessage(rtc::Message* pmsg) {
  if (pmsg->message_id == MSG_LIFETIME_TIMER) {
    RTC_DCHECK(!pmsg->pdata);

    // If the lifetime timeout has been exceeded, then send a signal.
    // Otherwise, just keep waiting.
    if (rtc::TimeMillis() >= last_used_ + lifetime_) {
      RTC_LOG(LS_INFO) << "Expiring binding " << username_;
      SignalTimeout(this);
    } else {
      server_->thread()->PostDelayed(RTC_FROM_HERE, lifetime_, this,
                                     MSG_LIFETIME_TIMER);
    }

  } else {
    RTC_NOTREACHED();
  }
}

}  // namespace cricket
