// Copyright 2018 The Chromium Authors. All rights reserved.
// Copyright (C) 2018 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#if ENABLE(WEB_AUTHN)

#include "PublicKeyCredentialType.h"

namespace fido {

enum class ProtocolVersion {
    kCtap,
    kU2f,
    kUnknown,
};

// Length of the U2F challenge/application parameter:
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#registration-request-message---u2f_register
constexpr size_t kU2fChallengeParamLength = 32;
constexpr size_t kU2fApplicationParamLength = 32;
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#registration-response-message-success
constexpr size_t kReservedLength = 1;
constexpr size_t kU2fKeyHandleLengthOffset = 66;
constexpr size_t kU2fKeyHandleOffset = 67;

// CTAP protocol device response code, as specified in
// https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html#error-responses
enum class CtapDeviceResponseCode : uint8_t {
    kSuccess = 0x00,
    kCtap1ErrInvalidCommand = 0x01,
    kCtap1ErrInvalidParameter = 0x02,
    kCtap1ErrInvalidLength = 0x03,
    kCtap1ErrInvalidSeq = 0x04,
    kCtap1ErrTimeout = 0x05,
    kCtap1ErrChannelBusy = 0x06,
    kCtap1ErrLockRequired = 0x0A,
    kCtap1ErrInvalidChannel = 0x0B,
    kCtap2ErrCBORParsing = 0x10,
    kCtap2ErrUnexpectedType = 0x11,
    kCtap2ErrInvalidCBOR = 0x12,
    kCtap2ErrInvalidCBORType = 0x13,
    kCtap2ErrMissingParameter = 0x14,
    kCtap2ErrLimitExceeded = 0x15,
    kCtap2ErrUnsupportedExtension = 0x16,
    kCtap2ErrTooManyElements = 0x17,
    kCtap2ErrExtensionNotSupported = 0x18,
    kCtap2ErrCredentialExcluded = 0x19,
    kCtap2ErrProcesssing = 0x21,
    kCtap2ErrInvalidCredential = 0x22,
    kCtap2ErrUserActionPending = 0x23,
    kCtap2ErrOperationPending = 0x24,
    kCtap2ErrNoOperations = 0x25,
    kCtap2ErrUnsupportedAlgorithms = 0x26,
    kCtap2ErrOperationDenied = 0x27,
    kCtap2ErrKeyStoreFull = 0x28,
    kCtap2ErrNotBusy = 0x29,
    kCtap2ErrNoOperationPending = 0x2A,
    kCtap2ErrUnsupportedOption = 0x2B,
    kCtap2ErrInvalidOption = 0x2C,
    kCtap2ErrKeepAliveCancel = 0x2D,
    kCtap2ErrNoCredentials = 0x2E,
    kCtap2ErrUserActionTimeout = 0x2F,
    kCtap2ErrNotAllowed = 0x30,
    kCtap2ErrPinInvalid = 0x31,
    kCtap2ErrPinBlocked = 0x32,
    kCtap2ErrPinAuthInvalid = 0x33,
    kCtap2ErrPinAuthBlocked = 0x34,
    kCtap2ErrPinNotSet = 0x35,
    kCtap2ErrPinRequired = 0x36,
    kCtap2ErrPinPolicyViolation = 0x37,
    kCtap2ErrPinTokenExpired = 0x38,
    kCtap2ErrRequestTooLarge = 0x39,
    kCtap2ErrOther = 0x7F,
    kCtap2ErrSpecLast = 0xDF,
    kCtap2ErrExtensionFirst = 0xE0,
    kCtap2ErrExtensionLast = 0xEF,
    kCtap2ErrVendorFirst = 0xF0,
    kCtap2ErrVendorLast = 0xFF
};

bool isCtapDeviceResponseCode(CtapDeviceResponseCode);

// Commands supported by CTAPHID device as specified in
// https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html#ctaphid-commands
enum class FidoHidDeviceCommand : uint8_t {
    kMsg = 0x03,
    kCbor = 0x10,
    kInit = 0x06,
    kPing = 0x01,
    kCancel = 0x11,
    kError = 0x3F,
    kKeepAlive = 0x3B,
    kWink = 0x08,
    kLock = 0x04,
};

bool isFidoHidDeviceCommand(FidoHidDeviceCommand);

// Parameters for fake U2F registration used to check for user presence.
const uint8_t kBogusAppParam[] = {
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41
};

const uint8_t kBogusChallenge[] = {
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42
};

// String key values for CTAP request optional parameters and
// AuthenticatorGetInfo response.
const char kResidentKeyMapKey[] = "rk";
const char kUserVerificationMapKey[] = "uv";
const char kUserPresenceMapKey[] = "up";
const char kClientPinMapKey[] = "clientPin";
const char kPlatformDeviceMapKey[] = "plat";
const char kEntityIdMapKey[] = "id";
const char kEntityNameMapKey[] = "name";
const char kDisplayNameMapKey[] = "displayName";
const char kIconUrlMapKey[] = "icon";
const char kCredentialTypeMapKey[] = "type";
const char kCredentialAlgorithmMapKey[] = "alg";
// Keys for storing credential descriptor information in CBOR map.
const char kCredentialIdKey[] = "id";
const char kCredentialTypeKey[] = "type";

// HID transport specific constants.
const size_t kHidPacketSize = 64;
const uint32_t kHidBroadcastChannel = 0xffffffff;
const size_t kHidInitPacketHeaderSize = 7;
const size_t kHidContinuationPacketHeader = 5;
const size_t kHidMaxPacketSize = 64;
const size_t kHidInitPacketDataSize = kHidMaxPacketSize - kHidInitPacketHeaderSize;
const size_t kHidContinuationPacketDataSize = kHidMaxPacketSize - kHidContinuationPacketHeader;
const size_t kHidInitResponseSize = 17;
const size_t kHidInitNonceLength = 8;

const uint8_t kHidMaxLockSeconds = 10;

// Messages are limited to an initiation packet and 128 continuation packets.
const size_t kHidMaxMessageSize = 7609;

// CTAP/U2F devices only provide a single report so specify a report ID of 0 here.
const uint8_t kHidReportId = 0x00;

// U2F APDU encoding constants, as specified in
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#authentication-messages

// P1 instructions.
constexpr uint8_t kP1EnforceUserPresenceAndSign = 0x03;
constexpr uint8_t kP1CheckOnly = 0x07;

constexpr size_t kMaxKeyHandleLength = 255;

// Authenticator API commands supported by CTAP devices, as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#authenticator-api
enum class CtapRequestCommand : uint8_t {
    kAuthenticatorMakeCredential = 0x01,
    kAuthenticatorGetAssertion = 0x02,
    kAuthenticatorGetNextAssertion = 0x08,
    kAuthenticatorGetInfo = 0x04,
    kAuthenticatorClientPin = 0x06,
    kAuthenticatorReset = 0x07,
};

// APDU instruction code for U2F request encoding.
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#command-and-parameter-values
enum class U2fApduInstruction : uint8_t {
    kRegister = 0x01,
    kSign = 0x02,
    kVersion = 0x03,
    kVendorFirst = 0x40,
    kVenderLast = 0xBF,
};

// String key values for attestation object as a response to MakeCredential
// request.
const char kFormatKey[] = "fmt";
const char kAttestationStatementKey[] = "attStmt";
const char kAuthDataKey[] = "authData";

// String representation of public key credential enum.
// https://w3c.github.io/webauthn/#credentialType
const char kPublicKey[] = "public-key";

const char* publicKeyCredentialTypeToString(WebCore::PublicKeyCredentialType);

// FIXME: Add url to the official spec once it's standardized.
const char kCtap2Version[] = "FIDO_2_0";
const char kU2fVersion[] = "U2F_V2";

// CTAPHID Usage Page and Usage
// https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html#hid-report-descriptor-and-device-discovery
const uint32_t kCtapHidUsagePage = 0xF1D0;
const uint32_t kCtapHidUsage = 0x01;

// CTAPNFC Applet selection command and responses
// https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#nfc-applet-selection
const uint8_t kCtapNfcAppletSelectionCommand[] = {
    0x00, 0xA4, 0x04, 0x00, // CLA, INS, P1, P2
    0x08, // L
    0xA0, 0x00, 0x00, 0x06, 0x47, // RID
    0x2F, 0x00, 0x01 // PIX
};

const uint8_t kCtapNfcAppletSelectionU2f[] = {
    0x55, 0x32, 0x46, 0x5F, 0x56, 0x32, // Version
    0x90, 0x00 // APDU response code
};

const uint8_t kCtapNfcAppletSelectionCtap[] = {
    0x46, 0x49, 0x44, 0x4f, 0x5f, 0x32, 0x5f, 0x30, // Version
    0x90, 0x00 // APDU response code
};

// https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#nfc-command-framing
const uint8_t kCtapNfcApduCla = 0x80;
const uint8_t kCtapNfcApduIns = 0x10;

} // namespace fido

#endif // ENABLE(WEB_AUTHN)
