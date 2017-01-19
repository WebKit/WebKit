/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/constants.h"

#include <string>

#include "webrtc/libjingle/xmllite/qname.h"
#include "webrtc/libjingle/xmllite/xmlconstants.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/jid.h"

namespace buzz {

// TODO: Remove static objects of complex types, particularly
// Jid and QName.

const char NS_CLIENT[] = "jabber:client";
const char NS_SERVER[] = "jabber:server";
const char NS_STREAM[] = "http://etherx.jabber.org/streams";
const char NS_XSTREAM[] = "urn:ietf:params:xml:ns:xmpp-streams";
const char NS_TLS[] = "urn:ietf:params:xml:ns:xmpp-tls";
const char NS_SASL[] = "urn:ietf:params:xml:ns:xmpp-sasl";
const char NS_BIND[] = "urn:ietf:params:xml:ns:xmpp-bind";
const char NS_DIALBACK[] = "jabber:server:dialback";
const char NS_SESSION[] = "urn:ietf:params:xml:ns:xmpp-session";
const char NS_STANZA[] = "urn:ietf:params:xml:ns:xmpp-stanzas";
const char NS_PRIVACY[] = "jabber:iq:privacy";
const char NS_ROSTER[] = "jabber:iq:roster";
const char NS_VCARD[] = "vcard-temp";
const char NS_AVATAR_HASH[] = "google:avatar";
const char NS_VCARD_UPDATE[] = "vcard-temp:x:update";
const char STR_CLIENT[] = "client";
const char STR_SERVER[] = "server";
const char STR_STREAM[] = "stream";

const char STR_GET[] = "get";
const char STR_SET[] = "set";
const char STR_RESULT[] = "result";
const char STR_ERROR[] = "error";

const char STR_FORM[] = "form";
const char STR_SUBMIT[] = "submit";
const char STR_TEXT_SINGLE[] = "text-single";
const char STR_LIST_SINGLE[] = "list-single";
const char STR_LIST_MULTI[] = "list-multi";
const char STR_HIDDEN[] = "hidden";
const char STR_FORM_TYPE[] = "FORM_TYPE";

const char STR_FROM[] = "from";
const char STR_TO[] = "to";
const char STR_BOTH[] = "both";
const char STR_REMOVE[] = "remove";
const char STR_TRUE[] = "true";

const char STR_TYPE[] = "type";
const char STR_NAME[] = "name";
const char STR_ID[] = "id";
const char STR_JID[] = "jid";
const char STR_SUBSCRIPTION[] = "subscription";
const char STR_ASK[] = "ask";
const char STR_X[] = "x";
const char STR_GOOGLE_COM[] = "google.com";
const char STR_GMAIL_COM[] = "gmail.com";
const char STR_GOOGLEMAIL_COM[] = "googlemail.com";
const char STR_DEFAULT_DOMAIN[] = "default.talk.google.com";
const char STR_TALK_GOOGLE_COM[] = "talk.google.com";
const char STR_TALKX_L_GOOGLE_COM[] = "talkx.l.google.com";
const char STR_XMPP_GOOGLE_COM[] = "xmpp.google.com";
const char STR_XMPPX_L_GOOGLE_COM[] = "xmppx.l.google.com";

#ifdef FEATURE_ENABLE_VOICEMAIL
const char STR_VOICEMAIL[] = "voicemail";
const char STR_OUTGOINGVOICEMAIL[] = "outgoingvoicemail";
#endif

const char STR_UNAVAILABLE[] = "unavailable";

const char NS_PING[] = "urn:xmpp:ping";
const StaticQName QN_PING = { NS_PING, "ping" };

const char NS_MUC_UNIQUE[] = "http://jabber.org/protocol/muc#unique";
const StaticQName QN_MUC_UNIQUE_QUERY = { NS_MUC_UNIQUE, "unique" };
const StaticQName QN_HANGOUT_ID = { STR_EMPTY, "hangout-id" };

const char STR_GOOGLE_MUC_LOOKUP_JID[] = "lookup.groupchat.google.com";

const char STR_MUC_ROOMCONFIG_ROOMNAME[] = "muc#roomconfig_roomname";
const char STR_MUC_ROOMCONFIG_FEATURES[] = "muc#roomconfig_features";
const char STR_MUC_ROOM_FEATURE_ENTERPRISE[] = "muc_enterprise";
const char STR_MUC_ROOMCONFIG[] = "http://jabber.org/protocol/muc#roomconfig";
const char STR_MUC_ROOM_FEATURE_HANGOUT[] = "muc_es";
const char STR_MUC_ROOM_FEATURE_HANGOUT_LITE[] = "muc_lite";
const char STR_MUC_ROOM_FEATURE_BROADCAST[] = "broadcast";
const char STR_MUC_ROOM_FEATURE_MULTI_USER_VC[] = "muc_muvc";
const char STR_MUC_ROOM_FEATURE_RECORDABLE[] = "recordable";
const char STR_MUC_ROOM_FEATURE_CUSTOM_RECORDING[] = "custom_recording";
const char STR_MUC_ROOM_OWNER_PROFILE_ID[] = "muc#roominfo_owner_profile_id";
const char STR_MUC_ROOM_FEATURE_ABUSE_RECORDABLE[] = "abuse_recordable";

const char STR_ID_TYPE_CONVERSATION[] = "conversation";
const char NS_GOOGLE_MUC_HANGOUT[] = "google:muc#hangout";
const StaticQName QN_GOOGLE_MUC_HANGOUT_INVITE =
    { NS_GOOGLE_MUC_HANGOUT, "invite" };
const StaticQName QN_GOOGLE_MUC_HANGOUT_INVITE_TYPE =
    { NS_GOOGLE_MUC_HANGOUT, "invite-type" };
const StaticQName QN_ATTR_CREATE_ACTIVITY =
    { STR_EMPTY, "create-activity" };
const StaticQName QN_GOOGLE_MUC_HANGOUT_PUBLIC =
    { NS_GOOGLE_MUC_HANGOUT, "public" };
const StaticQName QN_GOOGLE_MUC_HANGOUT_INVITEE =
    { NS_GOOGLE_MUC_HANGOUT, "invitee" };
const StaticQName QN_GOOGLE_MUC_HANGOUT_NOTIFICATION_STATUS =
    { NS_GOOGLE_MUC_HANGOUT, "notification-status" };
const StaticQName QN_GOOGLE_MUC_HANGOUT_NOTIFICATION_TYPE = {
    NS_GOOGLE_MUC_HANGOUT, "notification-type" };
const StaticQName QN_GOOGLE_MUC_HANGOUT_HANGOUT_START_CONTEXT = {
    NS_GOOGLE_MUC_HANGOUT, "hangout-start-context" };
const StaticQName QN_GOOGLE_MUC_HANGOUT_CONVERSATION_ID = {
    NS_GOOGLE_MUC_HANGOUT, "conversation-id" };

const StaticQName QN_STREAM_STREAM = { NS_STREAM, STR_STREAM };
const StaticQName QN_STREAM_FEATURES = { NS_STREAM, "features" };
const StaticQName QN_STREAM_ERROR = { NS_STREAM, "error" };

const StaticQName QN_XSTREAM_BAD_FORMAT = { NS_XSTREAM, "bad-format" };
const StaticQName QN_XSTREAM_BAD_NAMESPACE_PREFIX =
    { NS_XSTREAM, "bad-namespace-prefix" };
const StaticQName QN_XSTREAM_CONFLICT = { NS_XSTREAM, "conflict" };
const StaticQName QN_XSTREAM_CONNECTION_TIMEOUT =
    { NS_XSTREAM, "connection-timeout" };
const StaticQName QN_XSTREAM_HOST_GONE = { NS_XSTREAM, "host-gone" };
const StaticQName QN_XSTREAM_HOST_UNKNOWN = { NS_XSTREAM, "host-unknown" };
const StaticQName QN_XSTREAM_IMPROPER_ADDRESSIING =
     { NS_XSTREAM, "improper-addressing" };
const StaticQName QN_XSTREAM_INTERNAL_SERVER_ERROR =
    { NS_XSTREAM, "internal-server-error" };
const StaticQName QN_XSTREAM_INVALID_FROM = { NS_XSTREAM, "invalid-from" };
const StaticQName QN_XSTREAM_INVALID_ID = { NS_XSTREAM, "invalid-id" };
const StaticQName QN_XSTREAM_INVALID_NAMESPACE =
    { NS_XSTREAM, "invalid-namespace" };
const StaticQName QN_XSTREAM_INVALID_XML = { NS_XSTREAM, "invalid-xml" };
const StaticQName QN_XSTREAM_NOT_AUTHORIZED = { NS_XSTREAM, "not-authorized" };
const StaticQName QN_XSTREAM_POLICY_VIOLATION =
    { NS_XSTREAM, "policy-violation" };
const StaticQName QN_XSTREAM_REMOTE_CONNECTION_FAILED =
    { NS_XSTREAM, "remote-connection-failed" };
const StaticQName QN_XSTREAM_RESOURCE_CONSTRAINT =
    { NS_XSTREAM, "resource-constraint" };
const StaticQName QN_XSTREAM_RESTRICTED_XML = { NS_XSTREAM, "restricted-xml" };
const StaticQName QN_XSTREAM_SEE_OTHER_HOST = { NS_XSTREAM, "see-other-host" };
const StaticQName QN_XSTREAM_SYSTEM_SHUTDOWN =
    { NS_XSTREAM, "system-shutdown" };
const StaticQName QN_XSTREAM_UNDEFINED_CONDITION =
    { NS_XSTREAM, "undefined-condition" };
const StaticQName QN_XSTREAM_UNSUPPORTED_ENCODING =
    { NS_XSTREAM, "unsupported-encoding" };
const StaticQName QN_XSTREAM_UNSUPPORTED_STANZA_TYPE =
    { NS_XSTREAM, "unsupported-stanza-type" };
const StaticQName QN_XSTREAM_UNSUPPORTED_VERSION =
    { NS_XSTREAM, "unsupported-version" };
const StaticQName QN_XSTREAM_XML_NOT_WELL_FORMED =
    { NS_XSTREAM, "xml-not-well-formed" };
const StaticQName QN_XSTREAM_TEXT = { NS_XSTREAM, "text" };

const StaticQName QN_TLS_STARTTLS = { NS_TLS, "starttls" };
const StaticQName QN_TLS_REQUIRED = { NS_TLS, "required" };
const StaticQName QN_TLS_PROCEED = { NS_TLS, "proceed" };
const StaticQName QN_TLS_FAILURE = { NS_TLS, "failure" };

const StaticQName QN_SASL_MECHANISMS = { NS_SASL, "mechanisms" };
const StaticQName QN_SASL_MECHANISM = { NS_SASL, "mechanism" };
const StaticQName QN_SASL_AUTH = { NS_SASL, "auth" };
const StaticQName QN_SASL_CHALLENGE = { NS_SASL, "challenge" };
const StaticQName QN_SASL_RESPONSE = { NS_SASL, "response" };
const StaticQName QN_SASL_ABORT = { NS_SASL, "abort" };
const StaticQName QN_SASL_SUCCESS = { NS_SASL, "success" };
const StaticQName QN_SASL_FAILURE = { NS_SASL, "failure" };
const StaticQName QN_SASL_ABORTED = { NS_SASL, "aborted" };
const StaticQName QN_SASL_INCORRECT_ENCODING =
    { NS_SASL, "incorrect-encoding" };
const StaticQName QN_SASL_INVALID_AUTHZID = { NS_SASL, "invalid-authzid" };
const StaticQName QN_SASL_INVALID_MECHANISM = { NS_SASL, "invalid-mechanism" };
const StaticQName QN_SASL_MECHANISM_TOO_WEAK =
    { NS_SASL, "mechanism-too-weak" };
const StaticQName QN_SASL_NOT_AUTHORIZED = { NS_SASL, "not-authorized" };
const StaticQName QN_SASL_TEMPORARY_AUTH_FAILURE =
    { NS_SASL, "temporary-auth-failure" };

// These are non-standard.
const char NS_GOOGLE_AUTH_PROTOCOL[] =
    "http://www.google.com/talk/protocol/auth";
const StaticQName QN_GOOGLE_AUTH_CLIENT_USES_FULL_BIND_RESULT =
    { NS_GOOGLE_AUTH_PROTOCOL, "client-uses-full-bind-result" };
const StaticQName QN_GOOGLE_ALLOW_NON_GOOGLE_ID_XMPP_LOGIN =
    { NS_GOOGLE_AUTH_PROTOCOL, "allow-non-google-login" };
const StaticQName QN_GOOGLE_AUTH_SERVICE =
    { NS_GOOGLE_AUTH_PROTOCOL, "service" };

const StaticQName QN_DIALBACK_RESULT = { NS_DIALBACK, "result" };
const StaticQName QN_DIALBACK_VERIFY = { NS_DIALBACK, "verify" };

const StaticQName QN_STANZA_BAD_REQUEST = { NS_STANZA, "bad-request" };
const StaticQName QN_STANZA_CONFLICT = { NS_STANZA, "conflict" };
const StaticQName QN_STANZA_FEATURE_NOT_IMPLEMENTED =
    { NS_STANZA, "feature-not-implemented" };
const StaticQName QN_STANZA_FORBIDDEN = { NS_STANZA, "forbidden" };
const StaticQName QN_STANZA_GONE = { NS_STANZA, "gone" };
const StaticQName QN_STANZA_INTERNAL_SERVER_ERROR =
    { NS_STANZA, "internal-server-error" };
const StaticQName QN_STANZA_ITEM_NOT_FOUND = { NS_STANZA, "item-not-found" };
const StaticQName QN_STANZA_JID_MALFORMED = { NS_STANZA, "jid-malformed" };
const StaticQName QN_STANZA_NOT_ACCEPTABLE = { NS_STANZA, "not-acceptable" };
const StaticQName QN_STANZA_NOT_ALLOWED = { NS_STANZA, "not-allowed" };
const StaticQName QN_STANZA_PAYMENT_REQUIRED =
    { NS_STANZA, "payment-required" };
const StaticQName QN_STANZA_RECIPIENT_UNAVAILABLE =
    { NS_STANZA, "recipient-unavailable" };
const StaticQName QN_STANZA_REDIRECT = { NS_STANZA, "redirect" };
const StaticQName QN_STANZA_REGISTRATION_REQUIRED =
    { NS_STANZA, "registration-required" };
const StaticQName QN_STANZA_REMOTE_SERVER_NOT_FOUND =
    { NS_STANZA, "remote-server-not-found" };
const StaticQName QN_STANZA_REMOTE_SERVER_TIMEOUT =
    { NS_STANZA, "remote-server-timeout" };
const StaticQName QN_STANZA_RESOURCE_CONSTRAINT =
    { NS_STANZA, "resource-constraint" };
const StaticQName QN_STANZA_SERVICE_UNAVAILABLE =
    { NS_STANZA, "service-unavailable" };
const StaticQName QN_STANZA_SUBSCRIPTION_REQUIRED =
    { NS_STANZA, "subscription-required" };
const StaticQName QN_STANZA_UNDEFINED_CONDITION =
    { NS_STANZA, "undefined-condition" };
const StaticQName QN_STANZA_UNEXPECTED_REQUEST =
    { NS_STANZA, "unexpected-request" };
const StaticQName QN_STANZA_TEXT = { NS_STANZA, "text" };

const StaticQName QN_BIND_BIND = { NS_BIND, "bind" };
const StaticQName QN_BIND_RESOURCE = { NS_BIND, "resource" };
const StaticQName QN_BIND_JID = { NS_BIND, "jid" };

const StaticQName QN_MESSAGE = { NS_CLIENT, "message" };
const StaticQName QN_BODY = { NS_CLIENT, "body" };
const StaticQName QN_SUBJECT = { NS_CLIENT, "subject" };
const StaticQName QN_THREAD = { NS_CLIENT, "thread" };
const StaticQName QN_PRESENCE = { NS_CLIENT, "presence" };
const StaticQName QN_SHOW = { NS_CLIENT, "show" };
const StaticQName QN_STATUS = { NS_CLIENT, "status" };
const StaticQName QN_LANG = { NS_CLIENT, "lang" };
const StaticQName QN_PRIORITY = { NS_CLIENT, "priority" };
const StaticQName QN_IQ = { NS_CLIENT, "iq" };
const StaticQName QN_ERROR = { NS_CLIENT, "error" };

const StaticQName QN_SERVER_MESSAGE = { NS_SERVER, "message" };
const StaticQName QN_SERVER_BODY = { NS_SERVER, "body" };
const StaticQName QN_SERVER_SUBJECT = { NS_SERVER, "subject" };
const StaticQName QN_SERVER_THREAD = { NS_SERVER, "thread" };
const StaticQName QN_SERVER_PRESENCE = { NS_SERVER, "presence" };
const StaticQName QN_SERVER_SHOW = { NS_SERVER, "show" };
const StaticQName QN_SERVER_STATUS = { NS_SERVER, "status" };
const StaticQName QN_SERVER_LANG = { NS_SERVER, "lang" };
const StaticQName QN_SERVER_PRIORITY = { NS_SERVER, "priority" };
const StaticQName QN_SERVER_IQ = { NS_SERVER, "iq" };
const StaticQName QN_SERVER_ERROR = { NS_SERVER, "error" };

const StaticQName QN_SESSION_SESSION = { NS_SESSION, "session" };

const StaticQName QN_PRIVACY_QUERY = { NS_PRIVACY, "query" };
const StaticQName QN_PRIVACY_ACTIVE = { NS_PRIVACY, "active" };
const StaticQName QN_PRIVACY_DEFAULT = { NS_PRIVACY, "default" };
const StaticQName QN_PRIVACY_LIST = { NS_PRIVACY, "list" };
const StaticQName QN_PRIVACY_ITEM = { NS_PRIVACY, "item" };
const StaticQName QN_PRIVACY_IQ = { NS_PRIVACY, "iq" };
const StaticQName QN_PRIVACY_MESSAGE = { NS_PRIVACY, "message" };
const StaticQName QN_PRIVACY_PRESENCE_IN = { NS_PRIVACY, "presence-in" };
const StaticQName QN_PRIVACY_PRESENCE_OUT = { NS_PRIVACY, "presence-out" };

const StaticQName QN_ROSTER_QUERY = { NS_ROSTER, "query" };
const StaticQName QN_ROSTER_ITEM = { NS_ROSTER, "item" };
const StaticQName QN_ROSTER_GROUP = { NS_ROSTER, "group" };

const StaticQName QN_VCARD = { NS_VCARD, "vCard" };
const StaticQName QN_VCARD_FN = { NS_VCARD, "FN" };
const StaticQName QN_VCARD_PHOTO = { NS_VCARD, "PHOTO" };
const StaticQName QN_VCARD_PHOTO_BINVAL = { NS_VCARD, "BINVAL" };
const StaticQName QN_VCARD_AVATAR_HASH = { NS_AVATAR_HASH, "hash" };
const StaticQName QN_VCARD_AVATAR_HASH_MODIFIED =
    { NS_AVATAR_HASH, "modified" };

const StaticQName QN_NAME = { STR_EMPTY, "name" };
const StaticQName QN_AFFILIATION = { STR_EMPTY, "affiliation" };
const StaticQName QN_ROLE = { STR_EMPTY, "role" };

#if defined(FEATURE_ENABLE_PSTN)
const StaticQName QN_VCARD_TEL = { NS_VCARD, "TEL" };
const StaticQName QN_VCARD_VOICE = { NS_VCARD, "VOICE" };
const StaticQName QN_VCARD_HOME = { NS_VCARD, "HOME" };
const StaticQName QN_VCARD_WORK = { NS_VCARD, "WORK" };
const StaticQName QN_VCARD_CELL = { NS_VCARD, "CELL" };
const StaticQName QN_VCARD_NUMBER = { NS_VCARD, "NUMBER" };
#endif

const StaticQName QN_XML_LANG = { NS_XML, "lang" };

const StaticQName QN_ENCODING = { STR_EMPTY, STR_ENCODING };
const StaticQName QN_VERSION = { STR_EMPTY, STR_VERSION };
const StaticQName QN_TO = { STR_EMPTY, "to" };
const StaticQName QN_FROM = { STR_EMPTY, "from" };
const StaticQName QN_TYPE = { STR_EMPTY, "type" };
const StaticQName QN_ID = { STR_EMPTY, "id" };
const StaticQName QN_CODE = { STR_EMPTY, "code" };

const StaticQName QN_VALUE = { STR_EMPTY, "value" };
const StaticQName QN_ACTION = { STR_EMPTY, "action" };
const StaticQName QN_ORDER = { STR_EMPTY, "order" };
const StaticQName QN_MECHANISM = { STR_EMPTY, "mechanism" };
const StaticQName QN_ASK = { STR_EMPTY, "ask" };
const StaticQName QN_JID = { STR_EMPTY, "jid" };
const StaticQName QN_NICK = { STR_EMPTY, "nick" };
const StaticQName QN_SUBSCRIPTION = { STR_EMPTY, "subscription" };
const StaticQName QN_TITLE1 = { STR_EMPTY, "title1" };
const StaticQName QN_TITLE2 = { STR_EMPTY, "title2" };

const StaticQName QN_XMLNS_CLIENT = { NS_XMLNS, STR_CLIENT };
const StaticQName QN_XMLNS_SERVER = { NS_XMLNS, STR_SERVER };
const StaticQName QN_XMLNS_STREAM = { NS_XMLNS, STR_STREAM };


// Presence
const char STR_SHOW_AWAY[] = "away";
const char STR_SHOW_CHAT[] = "chat";
const char STR_SHOW_DND[] = "dnd";
const char STR_SHOW_XA[] = "xa";
const char STR_SHOW_OFFLINE[] = "offline";

const char NS_GOOGLE_PSTN_CONFERENCE[] = "http://www.google.com/pstn-conference";
const StaticQName QN_GOOGLE_PSTN_CONFERENCE_STATUS = { NS_GOOGLE_PSTN_CONFERENCE, "status" };
const StaticQName QN_ATTR_STATUS = { STR_EMPTY, "status" };

// Presence connection status
const char STR_PSTN_CONFERENCE_STATUS_CONNECTING[] = "connecting";
const char STR_PSTN_CONFERENCE_STATUS_JOINING[] = "joining";
const char STR_PSTN_CONFERENCE_STATUS_CONNECTED[] = "connected";
const char STR_PSTN_CONFERENCE_STATUS_HANGUP[] = "hangup";

// Subscription
const char STR_SUBSCRIBE[] = "subscribe";
const char STR_SUBSCRIBED[] = "subscribed";
const char STR_UNSUBSCRIBE[] = "unsubscribe";
const char STR_UNSUBSCRIBED[] = "unsubscribed";

// Google Invite
const char NS_GOOGLE_SUBSCRIBE[] = "google:subscribe";
const StaticQName QN_INVITATION = { NS_GOOGLE_SUBSCRIBE, "invitation" };
const StaticQName QN_INVITE_NAME = { NS_GOOGLE_SUBSCRIBE, "name" };
const StaticQName QN_INVITE_SUBJECT = { NS_GOOGLE_SUBSCRIBE, "subject" };
const StaticQName QN_INVITE_MESSAGE = { NS_GOOGLE_SUBSCRIBE, "body" };

// Kick
const char NS_GOOGLE_MUC_ADMIN[] = "google:muc#admin";
const StaticQName QN_GOOGLE_MUC_ADMIN_QUERY = { NS_GOOGLE_MUC_ADMIN, "query" };
const StaticQName QN_GOOGLE_MUC_ADMIN_QUERY_ITEM =
    { NS_GOOGLE_MUC_ADMIN, "item" };
const StaticQName QN_GOOGLE_MUC_ADMIN_QUERY_ITEM_REASON =
    { NS_GOOGLE_MUC_ADMIN, "reason" };

// PubSub: http://xmpp.org/extensions/xep-0060.html
const char NS_PUBSUB[] = "http://jabber.org/protocol/pubsub";
const StaticQName QN_PUBSUB = { NS_PUBSUB, "pubsub" };
const StaticQName QN_PUBSUB_ITEMS = { NS_PUBSUB, "items" };
const StaticQName QN_PUBSUB_ITEM = { NS_PUBSUB, "item" };
const StaticQName QN_PUBSUB_PUBLISH = { NS_PUBSUB, "publish" };
const StaticQName QN_PUBSUB_RETRACT = { NS_PUBSUB, "retract" };
const StaticQName QN_ATTR_PUBLISHER = { STR_EMPTY, "publisher" };

const char NS_PUBSUB_EVENT[] = "http://jabber.org/protocol/pubsub#event";
const StaticQName QN_NODE = { STR_EMPTY, "node" };
const StaticQName QN_PUBSUB_EVENT = { NS_PUBSUB_EVENT, "event" };
const StaticQName QN_PUBSUB_EVENT_ITEMS = { NS_PUBSUB_EVENT, "items" };
const StaticQName QN_PUBSUB_EVENT_ITEM = { NS_PUBSUB_EVENT, "item" };
const StaticQName QN_PUBSUB_EVENT_RETRACT = { NS_PUBSUB_EVENT, "retract" };
const StaticQName QN_NOTIFY = { STR_EMPTY, "notify" };

const char NS_PRESENTER[] = "google:presenter";
const StaticQName QN_PRESENTER_PRESENTER = { NS_PRESENTER, "presenter" };
const StaticQName QN_PRESENTER_PRESENTATION_ITEM =
    { NS_PRESENTER, "presentation-item" };
const StaticQName QN_PRESENTER_PRESENTATION_TYPE =
    { NS_PRESENTER, "presentation-type" };
const StaticQName QN_PRESENTER_PRESENTATION_ID =
    { NS_PRESENTER, "presentation-id" };

// JEP 0030
const StaticQName QN_CATEGORY = { STR_EMPTY, "category" };
const StaticQName QN_VAR = { STR_EMPTY, "var" };
const char NS_DISCO_INFO[] = "http://jabber.org/protocol/disco#info";
const char NS_DISCO_ITEMS[] = "http://jabber.org/protocol/disco#items";
const StaticQName QN_DISCO_INFO_QUERY = { NS_DISCO_INFO, "query" };
const StaticQName QN_DISCO_IDENTITY = { NS_DISCO_INFO, "identity" };
const StaticQName QN_DISCO_FEATURE = { NS_DISCO_INFO, "feature" };

const StaticQName QN_DISCO_ITEMS_QUERY = { NS_DISCO_ITEMS, "query" };
const StaticQName QN_DISCO_ITEM = { NS_DISCO_ITEMS, "item" };

// JEP 0020
const char NS_FEATURE[] = "http://jabber.org/protocol/feature-neg";
const StaticQName QN_FEATURE_FEATURE = { NS_FEATURE, "feature" };

// JEP 0004
const char NS_XDATA[] = "jabber:x:data";
const StaticQName QN_XDATA_X = { NS_XDATA, "x" };
const StaticQName QN_XDATA_INSTRUCTIONS = { NS_XDATA, "instructions" };
const StaticQName QN_XDATA_TITLE = { NS_XDATA, "title" };
const StaticQName QN_XDATA_FIELD = { NS_XDATA, "field" };
const StaticQName QN_XDATA_REPORTED = { NS_XDATA, "reported" };
const StaticQName QN_XDATA_ITEM = { NS_XDATA, "item" };
const StaticQName QN_XDATA_DESC = { NS_XDATA, "desc" };
const StaticQName QN_XDATA_REQUIRED = { NS_XDATA, "required" };
const StaticQName QN_XDATA_VALUE = { NS_XDATA, "value" };
const StaticQName QN_XDATA_OPTION = { NS_XDATA, "option" };

// JEP 0045
const char NS_MUC[] = "http://jabber.org/protocol/muc";
const StaticQName QN_MUC_X = { NS_MUC, "x" };
const StaticQName QN_MUC_ITEM = { NS_MUC, "item" };
const StaticQName QN_MUC_AFFILIATION = { NS_MUC, "affiliation" };
const StaticQName QN_MUC_ROLE = { NS_MUC, "role" };
const char STR_AFFILIATION_NONE[] = "none";
const char STR_ROLE_PARTICIPANT[] = "participant";

const char NS_GOOGLE_SESSION[] = "http://www.google.com/session";
const StaticQName QN_GOOGLE_CIRCLE_ID = { STR_EMPTY, "google-circle-id" };
const StaticQName QN_GOOGLE_USER_ID = { STR_EMPTY, "google-user-id" };
const StaticQName QN_GOOGLE_SESSION_BLOCKED = { NS_GOOGLE_SESSION, "blocked" };
const StaticQName QN_GOOGLE_SESSION_BLOCKING =
    { NS_GOOGLE_SESSION, "blocking" };

const char NS_MUC_OWNER[] = "http://jabber.org/protocol/muc#owner";
const StaticQName QN_MUC_OWNER_QUERY = { NS_MUC_OWNER, "query" };

const char NS_MUC_USER[] = "http://jabber.org/protocol/muc#user";
const StaticQName QN_MUC_USER_CONTINUE = { NS_MUC_USER, "continue" };
const StaticQName QN_MUC_USER_X = { NS_MUC_USER, "x" };
const StaticQName QN_MUC_USER_ITEM = { NS_MUC_USER, "item" };
const StaticQName QN_MUC_USER_STATUS = { NS_MUC_USER, "status" };
const StaticQName QN_MUC_USER_REASON = { NS_MUC_USER, "reason" };
const StaticQName QN_MUC_USER_ABUSE_VIOLATION = { NS_MUC_USER, "abuse-violation" };

// JEP 0055 - Jabber Search
const char NS_SEARCH[] = "jabber:iq:search";
const StaticQName QN_SEARCH_QUERY = { NS_SEARCH, "query" };
const StaticQName QN_SEARCH_ITEM = { NS_SEARCH, "item" };
const StaticQName QN_SEARCH_ROOM_NAME = { NS_SEARCH, "room-name" };
const StaticQName QN_SEARCH_ROOM_DOMAIN = { NS_SEARCH, "room-domain" };
const StaticQName QN_SEARCH_ROOM_JID = { NS_SEARCH, "room-jid" };
const StaticQName QN_SEARCH_HANGOUT_ID = { NS_SEARCH, "hangout-id" };
const StaticQName QN_SEARCH_EXTERNAL_ID = { NS_SEARCH, "external-id" };

// JEP 0115
const char NS_CAPS[] = "http://jabber.org/protocol/caps";
const StaticQName QN_CAPS_C = { NS_CAPS, "c" };
const StaticQName QN_VER = { STR_EMPTY, "ver" };
const StaticQName QN_EXT = { STR_EMPTY, "ext" };

// JEP 0153
const char kNSVCard[] = "vcard-temp:x:update";
const StaticQName kQnVCardX = { kNSVCard, "x" };
const StaticQName kQnVCardPhoto = { kNSVCard, "photo" };

// JEP 0172 User Nickname
const char NS_NICKNAME[] = "http://jabber.org/protocol/nick";
const StaticQName QN_NICKNAME = { NS_NICKNAME, "nick" };

// JEP 0085 chat state
const char NS_CHATSTATE[] = "http://jabber.org/protocol/chatstates";
const StaticQName QN_CS_ACTIVE = { NS_CHATSTATE, "active" };
const StaticQName QN_CS_COMPOSING = { NS_CHATSTATE, "composing" };
const StaticQName QN_CS_PAUSED = { NS_CHATSTATE, "paused" };
const StaticQName QN_CS_INACTIVE = { NS_CHATSTATE, "inactive" };
const StaticQName QN_CS_GONE = { NS_CHATSTATE, "gone" };

// JEP 0091 Delayed Delivery
const char kNSDelay[] = "jabber:x:delay";
const StaticQName kQnDelayX = { kNSDelay, "x" };
const StaticQName kQnStamp = { STR_EMPTY, "stamp" };

// Google time stamping (higher resolution)
const char kNSTimestamp[] = "google:timestamp";
const StaticQName kQnTime = { kNSTimestamp, "time" };
const StaticQName kQnMilliseconds = { STR_EMPTY, "ms" };

// Jingle Info
const char NS_JINGLE_INFO[] = "google:jingleinfo";
const StaticQName QN_JINGLE_INFO_QUERY = { NS_JINGLE_INFO, "query" };
const StaticQName QN_JINGLE_INFO_STUN = { NS_JINGLE_INFO, "stun" };
const StaticQName QN_JINGLE_INFO_RELAY = { NS_JINGLE_INFO, "relay" };
const StaticQName QN_JINGLE_INFO_SERVER = { NS_JINGLE_INFO, "server" };
const StaticQName QN_JINGLE_INFO_TOKEN = { NS_JINGLE_INFO, "token" };
const StaticQName QN_JINGLE_INFO_HOST = { STR_EMPTY, "host" };
const StaticQName QN_JINGLE_INFO_TCP = { STR_EMPTY, "tcp" };
const StaticQName QN_JINGLE_INFO_UDP = { STR_EMPTY, "udp" };
const StaticQName QN_JINGLE_INFO_TCPSSL = { STR_EMPTY, "tcpssl" };

// Call Performance Logging
const char NS_GOOGLE_CALLPERF_STATS[] = "google:call-perf-stats";
const StaticQName QN_CALLPERF_STATS =
    { NS_GOOGLE_CALLPERF_STATS, "callPerfStats" };
const StaticQName QN_CALLPERF_SESSIONID = { STR_EMPTY, "sessionId" };
const StaticQName QN_CALLPERF_LOCALUSER = { STR_EMPTY, "localUser" };
const StaticQName QN_CALLPERF_REMOTEUSER = { STR_EMPTY, "remoteUser" };
const StaticQName QN_CALLPERF_STARTTIME = { STR_EMPTY, "startTime" };
const StaticQName QN_CALLPERF_CALL_LENGTH = { STR_EMPTY, "callLength" };
const StaticQName QN_CALLPERF_CALL_ACCEPTED = { STR_EMPTY, "callAccepted" };
const StaticQName QN_CALLPERF_CALL_ERROR_CODE = { STR_EMPTY, "callErrorCode" };
const StaticQName QN_CALLPERF_TERMINATE_CODE = { STR_EMPTY, "terminateCode" };
const StaticQName QN_CALLPERF_DATAPOINT =
    { NS_GOOGLE_CALLPERF_STATS, "dataPoint" };
const StaticQName QN_CALLPERF_DATAPOINT_TIME = { STR_EMPTY, "timeStamp" };
const StaticQName QN_CALLPERF_DATAPOINT_FRACTION_LOST =
    { STR_EMPTY, "fraction_lost" };
const StaticQName QN_CALLPERF_DATAPOINT_CUM_LOST = { STR_EMPTY, "cum_lost" };
const StaticQName QN_CALLPERF_DATAPOINT_EXT_MAX = { STR_EMPTY, "ext_max" };
const StaticQName QN_CALLPERF_DATAPOINT_JITTER = { STR_EMPTY, "jitter" };
const StaticQName QN_CALLPERF_DATAPOINT_RTT = { STR_EMPTY, "RTT" };
const StaticQName QN_CALLPERF_DATAPOINT_BYTES_R =
    { STR_EMPTY, "bytesReceived" };
const StaticQName QN_CALLPERF_DATAPOINT_PACKETS_R =
    { STR_EMPTY, "packetsReceived" };
const StaticQName QN_CALLPERF_DATAPOINT_BYTES_S = { STR_EMPTY, "bytesSent" };
const StaticQName QN_CALLPERF_DATAPOINT_PACKETS_S =
    { STR_EMPTY, "packetsSent" };
const StaticQName QN_CALLPERF_DATAPOINT_PROCESS_CPU =
    { STR_EMPTY, "processCpu" };
const StaticQName QN_CALLPERF_DATAPOINT_SYSTEM_CPU = { STR_EMPTY, "systemCpu" };
const StaticQName QN_CALLPERF_DATAPOINT_CPUS = { STR_EMPTY, "cpus" };
const StaticQName QN_CALLPERF_CONNECTION =
    { NS_GOOGLE_CALLPERF_STATS, "connection" };
const StaticQName QN_CALLPERF_CONNECTION_LOCAL_ADDRESS =
    { STR_EMPTY, "localAddress" };
const StaticQName QN_CALLPERF_CONNECTION_REMOTE_ADDRESS =
    { STR_EMPTY, "remoteAddress" };
const StaticQName QN_CALLPERF_CONNECTION_FLAGS = { STR_EMPTY, "flags" };
const StaticQName QN_CALLPERF_CONNECTION_RTT = { STR_EMPTY, "rtt" };
const StaticQName QN_CALLPERF_CONNECTION_TOTAL_BYTES_S =
    { STR_EMPTY, "totalBytesSent" };
const StaticQName QN_CALLPERF_CONNECTION_BYTES_SECOND_S =
    { STR_EMPTY, "bytesSecondSent" };
const StaticQName QN_CALLPERF_CONNECTION_TOTAL_BYTES_R =
    { STR_EMPTY, "totalBytesRecv" };
const StaticQName QN_CALLPERF_CONNECTION_BYTES_SECOND_R =
    { STR_EMPTY, "bytesSecondRecv" };
const StaticQName QN_CALLPERF_CANDIDATE =
    { NS_GOOGLE_CALLPERF_STATS, "candidate" };
const StaticQName QN_CALLPERF_CANDIDATE_ENDPOINT = { STR_EMPTY, "endpoint" };
const StaticQName QN_CALLPERF_CANDIDATE_PROTOCOL = { STR_EMPTY, "protocol" };
const StaticQName QN_CALLPERF_CANDIDATE_ADDRESS = { STR_EMPTY, "address" };
const StaticQName QN_CALLPERF_MEDIA = { NS_GOOGLE_CALLPERF_STATS, "media" };
const StaticQName QN_CALLPERF_MEDIA_DIRECTION = { STR_EMPTY, "direction" };
const StaticQName QN_CALLPERF_MEDIA_SSRC = { STR_EMPTY, "SSRC" };
const StaticQName QN_CALLPERF_MEDIA_ENERGY = { STR_EMPTY, "energy" };
const StaticQName QN_CALLPERF_MEDIA_FIR = { STR_EMPTY, "fir" };
const StaticQName QN_CALLPERF_MEDIA_NACK = { STR_EMPTY, "nack" };
const StaticQName QN_CALLPERF_MEDIA_FPS = { STR_EMPTY, "fps" };
const StaticQName QN_CALLPERF_MEDIA_FPS_NETWORK = { STR_EMPTY, "fpsNetwork" };
const StaticQName QN_CALLPERF_MEDIA_FPS_DECODED = { STR_EMPTY, "fpsDecoded" };
const StaticQName QN_CALLPERF_MEDIA_JITTER_BUFFER_SIZE =
    { STR_EMPTY, "jitterBufferSize" };
const StaticQName QN_CALLPERF_MEDIA_PREFERRED_JITTER_BUFFER_SIZE =
    { STR_EMPTY, "preferredJitterBufferSize" };
const StaticQName QN_CALLPERF_MEDIA_TOTAL_PLAYOUT_DELAY =
    { STR_EMPTY, "totalPlayoutDelay" };

// Muc invites.
const StaticQName QN_MUC_USER_INVITE = { NS_MUC_USER, "invite" };

// Multiway audio/video.
const char NS_GOOGLE_MUC_USER[] = "google:muc#user";
const StaticQName QN_GOOGLE_MUC_USER_AVAILABLE_MEDIA =
    { NS_GOOGLE_MUC_USER, "available-media" };
const StaticQName QN_GOOGLE_MUC_USER_ENTRY = { NS_GOOGLE_MUC_USER, "entry" };
const StaticQName QN_GOOGLE_MUC_USER_MEDIA = { NS_GOOGLE_MUC_USER, "media" };
const StaticQName QN_GOOGLE_MUC_USER_TYPE = { NS_GOOGLE_MUC_USER, "type" };
const StaticQName QN_GOOGLE_MUC_USER_SRC_ID = { NS_GOOGLE_MUC_USER, "src-id" };
const StaticQName QN_GOOGLE_MUC_USER_STATUS = { NS_GOOGLE_MUC_USER, "status" };
const StaticQName QN_CLIENT_VERSION = { NS_GOOGLE_MUC_USER, "client-version" };
const StaticQName QN_LOCALE = { NS_GOOGLE_MUC_USER, "locale" };
const StaticQName QN_LABEL = { STR_EMPTY, "label" };

const char NS_GOOGLE_MUC_MEDIA[] = "google:muc#media";
const StaticQName QN_GOOGLE_MUC_AUDIO_MUTE =
    { NS_GOOGLE_MUC_MEDIA, "audio-mute" };
const StaticQName QN_GOOGLE_MUC_VIDEO_MUTE =
    { NS_GOOGLE_MUC_MEDIA, "video-mute" };
const StaticQName QN_GOOGLE_MUC_VIDEO_PAUSE =
    { NS_GOOGLE_MUC_MEDIA, "video-pause" };
const StaticQName QN_GOOGLE_MUC_RECORDING =
    { NS_GOOGLE_MUC_MEDIA, "recording" };
const StaticQName QN_GOOGLE_MUC_MEDIA_BLOCK = { NS_GOOGLE_MUC_MEDIA, "block" };
const StaticQName QN_STATE_ATTR = { STR_EMPTY, "state" };

const char AUTH_MECHANISM_GOOGLE_COOKIE[] = "X-GOOGLE-COOKIE";
const char AUTH_MECHANISM_GOOGLE_TOKEN[] = "X-GOOGLE-TOKEN";
const char AUTH_MECHANISM_OAUTH2[] = "X-OAUTH2";
const char AUTH_MECHANISM_PLAIN[] = "PLAIN";

}  // namespace buzz
