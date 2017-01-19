/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_CONSTANTS_H_
#define WEBRTC_LIBJINGLE_XMPP_CONSTANTS_H_

#include <string>
#include "webrtc/libjingle/xmllite/qname.h"
#include "webrtc/libjingle/xmpp/jid.h"

namespace buzz {

extern const char NS_CLIENT[];
extern const char NS_SERVER[];
extern const char NS_STREAM[];
extern const char NS_XSTREAM[];
extern const char NS_TLS[];
extern const char NS_SASL[];
extern const char NS_BIND[];
extern const char NS_DIALBACK[];
extern const char NS_SESSION[];
extern const char NS_STANZA[];
extern const char NS_PRIVACY[];
extern const char NS_ROSTER[];
extern const char NS_VCARD[];
extern const char NS_AVATAR_HASH[];
extern const char NS_VCARD_UPDATE[];
extern const char STR_CLIENT[];
extern const char STR_SERVER[];
extern const char STR_STREAM[];

extern const char STR_GET[];
extern const char STR_SET[];
extern const char STR_RESULT[];
extern const char STR_ERROR[];

extern const char STR_FORM[];
extern const char STR_SUBMIT[];
extern const char STR_TEXT_SINGLE[];
extern const char STR_LIST_SINGLE[];
extern const char STR_LIST_MULTI[];
extern const char STR_HIDDEN[];
extern const char STR_FORM_TYPE[];

extern const char STR_FROM[];
extern const char STR_TO[];
extern const char STR_BOTH[];
extern const char STR_REMOVE[];
extern const char STR_TRUE[];

extern const char STR_TYPE[];
extern const char STR_NAME[];
extern const char STR_ID[];
extern const char STR_JID[];
extern const char STR_SUBSCRIPTION[];
extern const char STR_ASK[];
extern const char STR_X[];
extern const char STR_GOOGLE_COM[];
extern const char STR_GMAIL_COM[];
extern const char STR_GOOGLEMAIL_COM[];
extern const char STR_DEFAULT_DOMAIN[];
extern const char STR_TALK_GOOGLE_COM[];
extern const char STR_TALKX_L_GOOGLE_COM[];
extern const char STR_XMPP_GOOGLE_COM[];
extern const char STR_XMPPX_L_GOOGLE_COM[];

#ifdef FEATURE_ENABLE_VOICEMAIL
extern const char STR_VOICEMAIL[];
extern const char STR_OUTGOINGVOICEMAIL[];
#endif

extern const char STR_UNAVAILABLE[];

extern const char NS_PING[];
extern const StaticQName QN_PING;

extern const char NS_MUC_UNIQUE[];
extern const StaticQName QN_MUC_UNIQUE_QUERY;
extern const StaticQName QN_HANGOUT_ID;

extern const char STR_GOOGLE_MUC_LOOKUP_JID[];
extern const char STR_MUC_ROOMCONFIG_ROOMNAME[];
extern const char STR_MUC_ROOMCONFIG_FEATURES[];
extern const char STR_MUC_ROOM_FEATURE_ENTERPRISE[];
extern const char STR_MUC_ROOMCONFIG[];
extern const char STR_MUC_ROOM_FEATURE_HANGOUT[];
extern const char STR_MUC_ROOM_FEATURE_HANGOUT_LITE[];
extern const char STR_MUC_ROOM_FEATURE_BROADCAST[];
extern const char STR_MUC_ROOM_FEATURE_MULTI_USER_VC[];
extern const char STR_MUC_ROOM_FEATURE_RECORDABLE[];
extern const char STR_MUC_ROOM_FEATURE_CUSTOM_RECORDING[];
extern const char STR_MUC_ROOM_OWNER_PROFILE_ID[];
extern const char STR_MUC_ROOM_FEATURE_ABUSE_RECORDABLE[];

extern const char STR_ID_TYPE_CONVERSATION[];
extern const char NS_GOOGLE_MUC_HANGOUT[];
extern const StaticQName QN_GOOGLE_MUC_HANGOUT_INVITE;
extern const StaticQName QN_GOOGLE_MUC_HANGOUT_INVITE_TYPE;
extern const StaticQName QN_ATTR_CREATE_ACTIVITY;
extern const StaticQName QN_GOOGLE_MUC_HANGOUT_PUBLIC;
extern const StaticQName QN_GOOGLE_MUC_HANGOUT_INVITEE;
extern const StaticQName QN_GOOGLE_MUC_HANGOUT_NOTIFICATION_STATUS;
extern const StaticQName QN_GOOGLE_MUC_HANGOUT_NOTIFICATION_TYPE;
extern const StaticQName QN_GOOGLE_MUC_HANGOUT_HANGOUT_START_CONTEXT;
extern const StaticQName QN_GOOGLE_MUC_HANGOUT_CONVERSATION_ID;

extern const StaticQName QN_STREAM_STREAM;
extern const StaticQName QN_STREAM_FEATURES;
extern const StaticQName QN_STREAM_ERROR;

extern const StaticQName QN_XSTREAM_BAD_FORMAT;
extern const StaticQName QN_XSTREAM_BAD_NAMESPACE_PREFIX;
extern const StaticQName QN_XSTREAM_CONFLICT;
extern const StaticQName QN_XSTREAM_CONNECTION_TIMEOUT;
extern const StaticQName QN_XSTREAM_HOST_GONE;
extern const StaticQName QN_XSTREAM_HOST_UNKNOWN;
extern const StaticQName QN_XSTREAM_IMPROPER_ADDRESSIING;
extern const StaticQName QN_XSTREAM_INTERNAL_SERVER_ERROR;
extern const StaticQName QN_XSTREAM_INVALID_FROM;
extern const StaticQName QN_XSTREAM_INVALID_ID;
extern const StaticQName QN_XSTREAM_INVALID_NAMESPACE;
extern const StaticQName QN_XSTREAM_INVALID_XML;
extern const StaticQName QN_XSTREAM_NOT_AUTHORIZED;
extern const StaticQName QN_XSTREAM_POLICY_VIOLATION;
extern const StaticQName QN_XSTREAM_REMOTE_CONNECTION_FAILED;
extern const StaticQName QN_XSTREAM_RESOURCE_CONSTRAINT;
extern const StaticQName QN_XSTREAM_RESTRICTED_XML;
extern const StaticQName QN_XSTREAM_SEE_OTHER_HOST;
extern const StaticQName QN_XSTREAM_SYSTEM_SHUTDOWN;
extern const StaticQName QN_XSTREAM_UNDEFINED_CONDITION;
extern const StaticQName QN_XSTREAM_UNSUPPORTED_ENCODING;
extern const StaticQName QN_XSTREAM_UNSUPPORTED_STANZA_TYPE;
extern const StaticQName QN_XSTREAM_UNSUPPORTED_VERSION;
extern const StaticQName QN_XSTREAM_XML_NOT_WELL_FORMED;
extern const StaticQName QN_XSTREAM_TEXT;

extern const StaticQName QN_TLS_STARTTLS;
extern const StaticQName QN_TLS_REQUIRED;
extern const StaticQName QN_TLS_PROCEED;
extern const StaticQName QN_TLS_FAILURE;

extern const StaticQName QN_SASL_MECHANISMS;
extern const StaticQName QN_SASL_MECHANISM;
extern const StaticQName QN_SASL_AUTH;
extern const StaticQName QN_SASL_CHALLENGE;
extern const StaticQName QN_SASL_RESPONSE;
extern const StaticQName QN_SASL_ABORT;
extern const StaticQName QN_SASL_SUCCESS;
extern const StaticQName QN_SASL_FAILURE;
extern const StaticQName QN_SASL_ABORTED;
extern const StaticQName QN_SASL_INCORRECT_ENCODING;
extern const StaticQName QN_SASL_INVALID_AUTHZID;
extern const StaticQName QN_SASL_INVALID_MECHANISM;
extern const StaticQName QN_SASL_MECHANISM_TOO_WEAK;
extern const StaticQName QN_SASL_NOT_AUTHORIZED;
extern const StaticQName QN_SASL_TEMPORARY_AUTH_FAILURE;

// These are non-standard.
extern const char NS_GOOGLE_AUTH[];
extern const char NS_GOOGLE_AUTH_PROTOCOL[];
extern const StaticQName QN_GOOGLE_AUTH_CLIENT_USES_FULL_BIND_RESULT;
extern const StaticQName QN_GOOGLE_ALLOW_NON_GOOGLE_ID_XMPP_LOGIN;
extern const StaticQName QN_GOOGLE_AUTH_SERVICE;

extern const StaticQName QN_DIALBACK_RESULT;
extern const StaticQName QN_DIALBACK_VERIFY;

extern const StaticQName QN_STANZA_BAD_REQUEST;
extern const StaticQName QN_STANZA_CONFLICT;
extern const StaticQName QN_STANZA_FEATURE_NOT_IMPLEMENTED;
extern const StaticQName QN_STANZA_FORBIDDEN;
extern const StaticQName QN_STANZA_GONE;
extern const StaticQName QN_STANZA_INTERNAL_SERVER_ERROR;
extern const StaticQName QN_STANZA_ITEM_NOT_FOUND;
extern const StaticQName QN_STANZA_JID_MALFORMED;
extern const StaticQName QN_STANZA_NOT_ACCEPTABLE;
extern const StaticQName QN_STANZA_NOT_ALLOWED;
extern const StaticQName QN_STANZA_PAYMENT_REQUIRED;
extern const StaticQName QN_STANZA_RECIPIENT_UNAVAILABLE;
extern const StaticQName QN_STANZA_REDIRECT;
extern const StaticQName QN_STANZA_REGISTRATION_REQUIRED;
extern const StaticQName QN_STANZA_REMOTE_SERVER_NOT_FOUND;
extern const StaticQName QN_STANZA_REMOTE_SERVER_TIMEOUT;
extern const StaticQName QN_STANZA_RESOURCE_CONSTRAINT;
extern const StaticQName QN_STANZA_SERVICE_UNAVAILABLE;
extern const StaticQName QN_STANZA_SUBSCRIPTION_REQUIRED;
extern const StaticQName QN_STANZA_UNDEFINED_CONDITION;
extern const StaticQName QN_STANZA_UNEXPECTED_REQUEST;
extern const StaticQName QN_STANZA_TEXT;

extern const StaticQName QN_BIND_BIND;
extern const StaticQName QN_BIND_RESOURCE;
extern const StaticQName QN_BIND_JID;

extern const StaticQName QN_MESSAGE;
extern const StaticQName QN_BODY;
extern const StaticQName QN_SUBJECT;
extern const StaticQName QN_THREAD;
extern const StaticQName QN_PRESENCE;
extern const StaticQName QN_SHOW;
extern const StaticQName QN_STATUS;
extern const StaticQName QN_LANG;
extern const StaticQName QN_PRIORITY;
extern const StaticQName QN_IQ;
extern const StaticQName QN_ERROR;

extern const StaticQName QN_SERVER_MESSAGE;
extern const StaticQName QN_SERVER_BODY;
extern const StaticQName QN_SERVER_SUBJECT;
extern const StaticQName QN_SERVER_THREAD;
extern const StaticQName QN_SERVER_PRESENCE;
extern const StaticQName QN_SERVER_SHOW;
extern const StaticQName QN_SERVER_STATUS;
extern const StaticQName QN_SERVER_LANG;
extern const StaticQName QN_SERVER_PRIORITY;
extern const StaticQName QN_SERVER_IQ;
extern const StaticQName QN_SERVER_ERROR;

extern const StaticQName QN_SESSION_SESSION;

extern const StaticQName QN_PRIVACY_QUERY;
extern const StaticQName QN_PRIVACY_ACTIVE;
extern const StaticQName QN_PRIVACY_DEFAULT;
extern const StaticQName QN_PRIVACY_LIST;
extern const StaticQName QN_PRIVACY_ITEM;
extern const StaticQName QN_PRIVACY_IQ;
extern const StaticQName QN_PRIVACY_MESSAGE;
extern const StaticQName QN_PRIVACY_PRESENCE_IN;
extern const StaticQName QN_PRIVACY_PRESENCE_OUT;

extern const StaticQName QN_ROSTER_QUERY;
extern const StaticQName QN_ROSTER_ITEM;
extern const StaticQName QN_ROSTER_GROUP;

extern const StaticQName QN_VCARD;
extern const StaticQName QN_VCARD_FN;
extern const StaticQName QN_VCARD_PHOTO;
extern const StaticQName QN_VCARD_PHOTO_BINVAL;
extern const StaticQName QN_VCARD_AVATAR_HASH;
extern const StaticQName QN_VCARD_AVATAR_HASH_MODIFIED;

#if defined(FEATURE_ENABLE_PSTN)
extern const StaticQName QN_VCARD_TEL;
extern const StaticQName QN_VCARD_VOICE;
extern const StaticQName QN_VCARD_HOME;
extern const StaticQName QN_VCARD_WORK;
extern const StaticQName QN_VCARD_CELL;
extern const StaticQName QN_VCARD_NUMBER;
#endif

#if defined(FEATURE_ENABLE_RICHPROFILES)
extern const StaticQName QN_USER_PROFILE_QUERY;
extern const StaticQName QN_USER_PROFILE_URL;

extern const StaticQName QN_ATOM_FEED;
extern const StaticQName QN_ATOM_ENTRY;
extern const StaticQName QN_ATOM_TITLE;
extern const StaticQName QN_ATOM_ID;
extern const StaticQName QN_ATOM_MODIFIED;
extern const StaticQName QN_ATOM_IMAGE;
extern const StaticQName QN_ATOM_LINK;
extern const StaticQName QN_ATOM_HREF;
#endif

extern const StaticQName QN_XML_LANG;

extern const StaticQName QN_ENCODING;
extern const StaticQName QN_VERSION;
extern const StaticQName QN_TO;
extern const StaticQName QN_FROM;
extern const StaticQName QN_TYPE;
extern const StaticQName QN_ID;
extern const StaticQName QN_CODE;
extern const StaticQName QN_NAME;
extern const StaticQName QN_VALUE;
extern const StaticQName QN_ACTION;
extern const StaticQName QN_ORDER;
extern const StaticQName QN_MECHANISM;
extern const StaticQName QN_ASK;
extern const StaticQName QN_JID;
extern const StaticQName QN_NICK;
extern const StaticQName QN_SUBSCRIPTION;
extern const StaticQName QN_TITLE1;
extern const StaticQName QN_TITLE2;
extern const StaticQName QN_AFFILIATION;
extern const StaticQName QN_ROLE;
extern const StaticQName QN_TIME;

extern const StaticQName QN_XMLNS_CLIENT;
extern const StaticQName QN_XMLNS_SERVER;
extern const StaticQName QN_XMLNS_STREAM;

// Presence
extern const char STR_SHOW_AWAY[];
extern const char STR_SHOW_CHAT[];
extern const char STR_SHOW_DND[];
extern const char STR_SHOW_XA[];
extern const char STR_SHOW_OFFLINE[];

extern const char NS_GOOGLE_PSTN_CONFERENCE[];
extern const StaticQName QN_GOOGLE_PSTN_CONFERENCE_STATUS;
extern const StaticQName QN_ATTR_STATUS;

// Presence connection status
extern const char STR_PSTN_CONFERENCE_STATUS_CONNECTING[];
extern const char STR_PSTN_CONFERENCE_STATUS_JOINING[];
extern const char STR_PSTN_CONFERENCE_STATUS_CONNECTED[];
extern const char STR_PSTN_CONFERENCE_STATUS_HANGUP[];

// Subscription
extern const char STR_SUBSCRIBE[];
extern const char STR_SUBSCRIBED[];
extern const char STR_UNSUBSCRIBE[];
extern const char STR_UNSUBSCRIBED[];

// Google Invite
extern const char NS_GOOGLE_SUBSCRIBE[];
extern const StaticQName QN_INVITATION;
extern const StaticQName QN_INVITE_NAME;
extern const StaticQName QN_INVITE_SUBJECT;
extern const StaticQName QN_INVITE_MESSAGE;

// Kick
extern const char NS_GOOGLE_MUC_ADMIN[];
extern const StaticQName QN_GOOGLE_MUC_ADMIN_QUERY;
extern const StaticQName QN_GOOGLE_MUC_ADMIN_QUERY_ITEM;
extern const StaticQName QN_GOOGLE_MUC_ADMIN_QUERY_ITEM_REASON;

// PubSub: http://xmpp.org/extensions/xep-0060.html
extern const char NS_PUBSUB[];
extern const StaticQName QN_PUBSUB;
extern const StaticQName QN_PUBSUB_ITEMS;
extern const StaticQName QN_PUBSUB_ITEM;
extern const StaticQName QN_PUBSUB_PUBLISH;
extern const StaticQName QN_PUBSUB_RETRACT;
extern const StaticQName QN_ATTR_PUBLISHER;

extern const char NS_PUBSUB_EVENT[];
extern const StaticQName QN_NODE;
extern const StaticQName QN_PUBSUB_EVENT;
extern const StaticQName QN_PUBSUB_EVENT_ITEMS;
extern const StaticQName QN_PUBSUB_EVENT_ITEM;
extern const StaticQName QN_PUBSUB_EVENT_RETRACT;
extern const StaticQName QN_NOTIFY;

extern const char NS_PRESENTER[];
extern const StaticQName QN_PRESENTER_PRESENTER;
extern const StaticQName QN_PRESENTER_PRESENTATION_ITEM;
extern const StaticQName QN_PRESENTER_PRESENTATION_TYPE;
extern const StaticQName QN_PRESENTER_PRESENTATION_ID;

// JEP 0030
extern const StaticQName QN_CATEGORY;
extern const StaticQName QN_VAR;
extern const char NS_DISCO_INFO[];
extern const char NS_DISCO_ITEMS[];

extern const StaticQName QN_DISCO_INFO_QUERY;
extern const StaticQName QN_DISCO_IDENTITY;
extern const StaticQName QN_DISCO_FEATURE;

extern const StaticQName QN_DISCO_ITEMS_QUERY;
extern const StaticQName QN_DISCO_ITEM;

// JEP 0020
extern const char NS_FEATURE[];
extern const StaticQName QN_FEATURE_FEATURE;

// JEP 0004
extern const char NS_XDATA[];
extern const StaticQName QN_XDATA_X;
extern const StaticQName QN_XDATA_INSTRUCTIONS;
extern const StaticQName QN_XDATA_TITLE;
extern const StaticQName QN_XDATA_FIELD;
extern const StaticQName QN_XDATA_REPORTED;
extern const StaticQName QN_XDATA_ITEM;
extern const StaticQName QN_XDATA_DESC;
extern const StaticQName QN_XDATA_REQUIRED;
extern const StaticQName QN_XDATA_VALUE;
extern const StaticQName QN_XDATA_OPTION;

// JEP 0045
extern const char NS_MUC[];
extern const StaticQName QN_MUC_X;
extern const StaticQName QN_MUC_ITEM;
extern const StaticQName QN_MUC_AFFILIATION;
extern const StaticQName QN_MUC_ROLE;
extern const StaticQName QN_CLIENT_VERSION;
extern const StaticQName QN_LOCALE;
extern const char STR_AFFILIATION_NONE[];
extern const char STR_ROLE_PARTICIPANT[];

extern const char NS_GOOGLE_SESSION[];
extern const StaticQName QN_GOOGLE_USER_ID;
extern const StaticQName QN_GOOGLE_CIRCLE_ID;
extern const StaticQName QN_GOOGLE_SESSION_BLOCKED;
extern const StaticQName QN_GOOGLE_SESSION_BLOCKING;

extern const char NS_MUC_OWNER[];
extern const StaticQName QN_MUC_OWNER_QUERY;

extern const char NS_MUC_USER[];
extern const StaticQName QN_MUC_USER_CONTINUE;
extern const StaticQName QN_MUC_USER_X;
extern const StaticQName QN_MUC_USER_ITEM;
extern const StaticQName QN_MUC_USER_STATUS;
extern const StaticQName QN_MUC_USER_REASON;
extern const StaticQName QN_MUC_USER_ABUSE_VIOLATION;

// JEP 0055 - Jabber Search
extern const char NS_SEARCH[];
extern const StaticQName QN_SEARCH_QUERY;
extern const StaticQName QN_SEARCH_ITEM;
extern const StaticQName QN_SEARCH_ROOM_NAME;
extern const StaticQName QN_SEARCH_ROOM_JID;
extern const StaticQName QN_SEARCH_ROOM_DOMAIN;
extern const StaticQName QN_SEARCH_HANGOUT_ID;
extern const StaticQName QN_SEARCH_EXTERNAL_ID;

// JEP 0115
extern const char NS_CAPS[];
extern const StaticQName QN_CAPS_C;
extern const StaticQName QN_VER;
extern const StaticQName QN_EXT;


// Avatar - JEP 0153
extern const char kNSVCard[];
extern const StaticQName kQnVCardX;
extern const StaticQName kQnVCardPhoto;

// JEP 0172 User Nickname
extern const char NS_NICKNAME[];
extern const StaticQName QN_NICKNAME;

// JEP 0085 chat state
extern const char NS_CHATSTATE[];
extern const StaticQName QN_CS_ACTIVE;
extern const StaticQName QN_CS_COMPOSING;
extern const StaticQName QN_CS_PAUSED;
extern const StaticQName QN_CS_INACTIVE;
extern const StaticQName QN_CS_GONE;

// JEP 0091 Delayed Delivery
extern const char kNSDelay[];
extern const StaticQName kQnDelayX;
extern const StaticQName kQnStamp;

// Google time stamping (higher resolution)
extern const char kNSTimestamp[];
extern const StaticQName kQnTime;
extern const StaticQName kQnMilliseconds;

extern const char NS_JINGLE_INFO[];
extern const StaticQName QN_JINGLE_INFO_QUERY;
extern const StaticQName QN_JINGLE_INFO_STUN;
extern const StaticQName QN_JINGLE_INFO_RELAY;
extern const StaticQName QN_JINGLE_INFO_SERVER;
extern const StaticQName QN_JINGLE_INFO_TOKEN;
extern const StaticQName QN_JINGLE_INFO_HOST;
extern const StaticQName QN_JINGLE_INFO_TCP;
extern const StaticQName QN_JINGLE_INFO_UDP;
extern const StaticQName QN_JINGLE_INFO_TCPSSL;

extern const char NS_GOOGLE_CALLPERF_STATS[];
extern const StaticQName QN_CALLPERF_STATS;
extern const StaticQName QN_CALLPERF_SESSIONID;
extern const StaticQName QN_CALLPERF_LOCALUSER;
extern const StaticQName QN_CALLPERF_REMOTEUSER;
extern const StaticQName QN_CALLPERF_STARTTIME;
extern const StaticQName QN_CALLPERF_CALL_LENGTH;
extern const StaticQName QN_CALLPERF_CALL_ACCEPTED;
extern const StaticQName QN_CALLPERF_CALL_ERROR_CODE;
extern const StaticQName QN_CALLPERF_TERMINATE_CODE;
extern const StaticQName QN_CALLPERF_DATAPOINT;
extern const StaticQName QN_CALLPERF_DATAPOINT_TIME;
extern const StaticQName QN_CALLPERF_DATAPOINT_FRACTION_LOST;
extern const StaticQName QN_CALLPERF_DATAPOINT_CUM_LOST;
extern const StaticQName QN_CALLPERF_DATAPOINT_EXT_MAX;
extern const StaticQName QN_CALLPERF_DATAPOINT_JITTER;
extern const StaticQName QN_CALLPERF_DATAPOINT_RTT;
extern const StaticQName QN_CALLPERF_DATAPOINT_BYTES_R;
extern const StaticQName QN_CALLPERF_DATAPOINT_PACKETS_R;
extern const StaticQName QN_CALLPERF_DATAPOINT_BYTES_S;
extern const StaticQName QN_CALLPERF_DATAPOINT_PACKETS_S;
extern const StaticQName QN_CALLPERF_DATAPOINT_PROCESS_CPU;
extern const StaticQName QN_CALLPERF_DATAPOINT_SYSTEM_CPU;
extern const StaticQName QN_CALLPERF_DATAPOINT_CPUS;
extern const StaticQName QN_CALLPERF_CONNECTION;
extern const StaticQName QN_CALLPERF_CONNECTION_LOCAL_ADDRESS;
extern const StaticQName QN_CALLPERF_CONNECTION_REMOTE_ADDRESS;
extern const StaticQName QN_CALLPERF_CONNECTION_FLAGS;
extern const StaticQName QN_CALLPERF_CONNECTION_RTT;
extern const StaticQName QN_CALLPERF_CONNECTION_TOTAL_BYTES_S;
extern const StaticQName QN_CALLPERF_CONNECTION_BYTES_SECOND_S;
extern const StaticQName QN_CALLPERF_CONNECTION_TOTAL_BYTES_R;
extern const StaticQName QN_CALLPERF_CONNECTION_BYTES_SECOND_R;
extern const StaticQName QN_CALLPERF_CANDIDATE;
extern const StaticQName QN_CALLPERF_CANDIDATE_ENDPOINT;
extern const StaticQName QN_CALLPERF_CANDIDATE_PROTOCOL;
extern const StaticQName QN_CALLPERF_CANDIDATE_ADDRESS;
extern const StaticQName QN_CALLPERF_MEDIA;
extern const StaticQName QN_CALLPERF_MEDIA_DIRECTION;
extern const StaticQName QN_CALLPERF_MEDIA_SSRC;
extern const StaticQName QN_CALLPERF_MEDIA_ENERGY;
extern const StaticQName QN_CALLPERF_MEDIA_FIR;
extern const StaticQName QN_CALLPERF_MEDIA_NACK;
extern const StaticQName QN_CALLPERF_MEDIA_FPS;
extern const StaticQName QN_CALLPERF_MEDIA_FPS_NETWORK;
extern const StaticQName QN_CALLPERF_MEDIA_FPS_DECODED;
extern const StaticQName QN_CALLPERF_MEDIA_JITTER_BUFFER_SIZE;
extern const StaticQName QN_CALLPERF_MEDIA_PREFERRED_JITTER_BUFFER_SIZE;
extern const StaticQName QN_CALLPERF_MEDIA_TOTAL_PLAYOUT_DELAY;

// Muc invites.
extern const StaticQName QN_MUC_USER_INVITE;

// Multiway audio/video.
extern const char NS_GOOGLE_MUC_USER[];
extern const StaticQName QN_GOOGLE_MUC_USER_AVAILABLE_MEDIA;
extern const StaticQName QN_GOOGLE_MUC_USER_ENTRY;
extern const StaticQName QN_GOOGLE_MUC_USER_MEDIA;
extern const StaticQName QN_GOOGLE_MUC_USER_TYPE;
extern const StaticQName QN_GOOGLE_MUC_USER_SRC_ID;
extern const StaticQName QN_GOOGLE_MUC_USER_STATUS;
extern const StaticQName QN_LABEL;

extern const char NS_GOOGLE_MUC_MEDIA[];
extern const StaticQName QN_GOOGLE_MUC_AUDIO_MUTE;
extern const StaticQName QN_GOOGLE_MUC_VIDEO_MUTE;
extern const StaticQName QN_GOOGLE_MUC_VIDEO_PAUSE;
extern const StaticQName QN_GOOGLE_MUC_RECORDING;
extern const StaticQName QN_GOOGLE_MUC_MEDIA_BLOCK;
extern const StaticQName QN_STATE_ATTR;


extern const char AUTH_MECHANISM_GOOGLE_COOKIE[];
extern const char AUTH_MECHANISM_GOOGLE_TOKEN[];
extern const char AUTH_MECHANISM_OAUTH2[];
extern const char AUTH_MECHANISM_PLAIN[];

}  // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMPP_CONSTANTS_H_
