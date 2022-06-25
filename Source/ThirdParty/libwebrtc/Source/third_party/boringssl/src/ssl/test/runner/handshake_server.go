// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package runner

import (
	"bytes"
	"crypto"
	"crypto/ecdsa"
	"crypto/ed25519"
	"crypto/elliptic"
	"crypto/rsa"
	"crypto/subtle"
	"crypto/x509"
	"errors"
	"fmt"
	"io"
	"math/big"
	"time"
)

// serverHandshakeState contains details of a server handshake in progress.
// It's discarded once the handshake has completed.
type serverHandshakeState struct {
	c               *Conn
	clientHello     *clientHelloMsg
	hello           *serverHelloMsg
	suite           *cipherSuite
	ellipticOk      bool
	ecdsaOk         bool
	sessionState    *sessionState
	finishedHash    finishedHash
	masterSecret    []byte
	certsFromClient [][]byte
	cert            *Certificate
	finishedBytes   []byte
}

// serverHandshake performs a TLS handshake as a server.
func (c *Conn) serverHandshake() error {
	config := c.config

	// If this is the first server handshake, we generate a random key to
	// encrypt the tickets with.
	config.serverInitOnce.Do(config.serverInit)

	c.sendHandshakeSeq = 0
	c.recvHandshakeSeq = 0

	hs := serverHandshakeState{
		c: c,
	}
	if err := hs.readClientHello(); err != nil {
		return err
	}

	if c.vers >= VersionTLS13 {
		if err := hs.doTLS13Handshake(); err != nil {
			return err
		}
	} else {
		isResume, err := hs.processClientHello()
		if err != nil {
			return err
		}

		// We only implement enough of SSL 3.0 to test that the client doesn't:
		// if negotiated (possibly with the NegotiateVersion bug), we send a
		// ServerHello and look for the resulting client protocol_version alert.
		if c.vers == VersionSSL30 {
			c.writeRecord(recordTypeHandshake, hs.hello.marshal())
			c.flushHandshake()
			if _, err := c.readHandshake(); err != nil {
				return err
			}
			return errors.New("tls: client did not reject an SSL 3.0 ServerHello")
		}

		// For an overview of TLS handshaking, see https://tools.ietf.org/html/rfc5246#section-7.3
		if isResume {
			// The client has included a session ticket and so we do an abbreviated handshake.
			if err := hs.doResumeHandshake(); err != nil {
				return err
			}
			if err := hs.establishKeys(); err != nil {
				return err
			}
			if c.config.Bugs.RenewTicketOnResume {
				if err := hs.sendSessionTicket(); err != nil {
					return err
				}
			}
			if err := hs.sendFinished(c.firstFinished[:], isResume); err != nil {
				return err
			}
			// Most retransmits are triggered by a timeout, but the final
			// leg of the handshake is retransmited upon re-receiving a
			// Finished.
			if err := c.simulatePacketLoss(func() {
				c.sendHandshakeSeq--
				c.writeRecord(recordTypeHandshake, hs.finishedBytes)
				c.flushHandshake()
			}); err != nil {
				return err
			}
			if err := hs.readFinished(nil, isResume); err != nil {
				return err
			}
			c.didResume = true
		} else {
			// The client didn't include a session ticket, or it wasn't
			// valid so we do a full handshake.
			if err := hs.doFullHandshake(); err != nil {
				return err
			}
			if err := hs.establishKeys(); err != nil {
				return err
			}
			if err := hs.readFinished(c.firstFinished[:], isResume); err != nil {
				return err
			}
			if c.config.Bugs.AlertBeforeFalseStartTest != 0 {
				c.sendAlert(c.config.Bugs.AlertBeforeFalseStartTest)
			}
			if c.config.Bugs.ExpectFalseStart {
				if err := c.readRecord(recordTypeApplicationData); err != nil {
					return fmt.Errorf("tls: peer did not false start: %s", err)
				}
			}
			if err := hs.sendSessionTicket(); err != nil {
				return err
			}
			if err := hs.sendFinished(nil, isResume); err != nil {
				return err
			}
		}

		c.exporterSecret = hs.masterSecret
	}
	c.handshakeComplete = true
	copy(c.clientRandom[:], hs.clientHello.random)
	copy(c.serverRandom[:], hs.hello.random)

	return nil
}

// readClientHello reads a ClientHello message from the client and determines
// the protocol version.
func (hs *serverHandshakeState) readClientHello() error {
	config := hs.c.config
	c := hs.c

	if err := c.simulatePacketLoss(nil); err != nil {
		return err
	}
	msg, err := c.readHandshake()
	if err != nil {
		return err
	}
	var ok bool
	hs.clientHello, ok = msg.(*clientHelloMsg)
	if !ok {
		c.sendAlert(alertUnexpectedMessage)
		return unexpectedMessageError(hs.clientHello, msg)
	}
	if size := config.Bugs.RequireClientHelloSize; size != 0 && len(hs.clientHello.raw) != size {
		return fmt.Errorf("tls: ClientHello record size is %d, but expected %d", len(hs.clientHello.raw), size)
	}

	if c.isDTLS && !config.Bugs.SkipHelloVerifyRequest {
		// Per RFC 6347, the version field in HelloVerifyRequest SHOULD
		// be always DTLS 1.0
		helloVerifyRequest := &helloVerifyRequestMsg{
			vers:   VersionDTLS10,
			cookie: make([]byte, 32),
		}
		if _, err := io.ReadFull(c.config.rand(), helloVerifyRequest.cookie); err != nil {
			c.sendAlert(alertInternalError)
			return errors.New("dtls: short read from Rand: " + err.Error())
		}
		c.writeRecord(recordTypeHandshake, helloVerifyRequest.marshal())
		c.flushHandshake()

		if err := c.simulatePacketLoss(nil); err != nil {
			return err
		}
		msg, err := c.readHandshake()
		if err != nil {
			return err
		}
		newClientHello, ok := msg.(*clientHelloMsg)
		if !ok {
			c.sendAlert(alertUnexpectedMessage)
			return unexpectedMessageError(hs.clientHello, msg)
		}
		if !bytes.Equal(newClientHello.cookie, helloVerifyRequest.cookie) {
			return errors.New("dtls: invalid cookie")
		}
		if err := checkClientHellosEqual(hs.clientHello.raw, newClientHello.raw, c.isDTLS, nil); err != nil {
			return err
		}
		hs.clientHello = newClientHello
	}

	if config.Bugs.RequireSameRenegoClientVersion && c.clientVersion != 0 {
		if c.clientVersion != hs.clientHello.vers {
			return fmt.Errorf("tls: client offered different version on renego")
		}
	}

	if config.Bugs.FailIfCECPQ2Offered {
		for _, offeredCurve := range hs.clientHello.supportedCurves {
			if isPqGroup(offeredCurve) {
				return errors.New("tls: CECPQ2 was offered")
			}
		}
	}

	if expected := config.Bugs.ExpectedKeyShares; expected != nil {
		if len(expected) != len(hs.clientHello.keyShares) {
			return fmt.Errorf("tls: expected %d key shares, but found %d", len(expected), len(hs.clientHello.keyShares))
		}

		for i, group := range expected {
			if found := hs.clientHello.keyShares[i].group; found != group {
				return fmt.Errorf("tls: key share #%d is for group %d, not %d", i, found, group)
			}
		}
	}

	c.clientVersion = hs.clientHello.vers

	// Use the versions extension if supplied, otherwise use the legacy ClientHello version.
	if len(hs.clientHello.supportedVersions) == 0 {
		if c.isDTLS {
			if hs.clientHello.vers <= VersionDTLS12 {
				hs.clientHello.supportedVersions = append(hs.clientHello.supportedVersions, VersionDTLS12)
			}
			if hs.clientHello.vers <= VersionDTLS10 {
				hs.clientHello.supportedVersions = append(hs.clientHello.supportedVersions, VersionDTLS10)
			}
		} else {
			if hs.clientHello.vers >= VersionTLS12 {
				hs.clientHello.supportedVersions = append(hs.clientHello.supportedVersions, VersionTLS12)
			}
			if hs.clientHello.vers >= VersionTLS11 {
				hs.clientHello.supportedVersions = append(hs.clientHello.supportedVersions, VersionTLS11)
			}
			if hs.clientHello.vers >= VersionTLS10 {
				hs.clientHello.supportedVersions = append(hs.clientHello.supportedVersions, VersionTLS10)
			}
			if hs.clientHello.vers >= VersionSSL30 {
				hs.clientHello.supportedVersions = append(hs.clientHello.supportedVersions, VersionSSL30)
			}
		}
	} else if config.Bugs.ExpectGREASE && !containsGREASE(hs.clientHello.supportedVersions) {
		return errors.New("tls: no GREASE version value found")
	}

	if !c.haveVers {
		if config.Bugs.NegotiateVersion != 0 {
			c.wireVersion = config.Bugs.NegotiateVersion
		} else {
			var found bool
			for _, vers := range hs.clientHello.supportedVersions {
				if _, ok := config.isSupportedVersion(vers, c.isDTLS); ok {
					c.wireVersion = vers
					found = true
					break
				}
			}
			if !found {
				c.sendAlert(alertProtocolVersion)
				return errors.New("tls: client did not offer any supported protocol versions")
			}
		}
	} else if config.Bugs.NegotiateVersionOnRenego != 0 {
		c.wireVersion = config.Bugs.NegotiateVersionOnRenego
	}

	c.vers, ok = wireToVersion(c.wireVersion, c.isDTLS)
	if !ok {
		panic("Could not map wire version")
	}
	c.haveVers = true

	clientProtocol, ok := wireToVersion(c.clientVersion, c.isDTLS)

	// Reject < 1.2 ClientHellos with signature_algorithms.
	if ok && clientProtocol < VersionTLS12 && len(hs.clientHello.signatureAlgorithms) > 0 {
		return fmt.Errorf("tls: client included signature_algorithms before TLS 1.2")
	}

	// Check the client cipher list is consistent with the version.
	if ok && clientProtocol < VersionTLS12 {
		for _, id := range hs.clientHello.cipherSuites {
			if isTLS12Cipher(id) {
				return fmt.Errorf("tls: client offered TLS 1.2 cipher before TLS 1.2")
			}
		}
	}

	if config.Bugs.MockQUICTransport != nil && len(hs.clientHello.sessionID) > 0 {
		return fmt.Errorf("tls: QUIC client did not disable compatibility mode")
	}
	if config.Bugs.ExpectNoTLS12Session {
		if len(hs.clientHello.sessionID) > 0 && c.vers >= VersionTLS13 {
			return fmt.Errorf("tls: client offered an unexpected session ID")
		}
		if len(hs.clientHello.sessionTicket) > 0 {
			return fmt.Errorf("tls: client offered an unexpected session ticket")
		}
	}

	if config.Bugs.ExpectNoTLS13PSK && len(hs.clientHello.pskIdentities) > 0 {
		return fmt.Errorf("tls: client offered unexpected PSK identities")
	}

	var scsvFound bool
	for _, cipherSuite := range hs.clientHello.cipherSuites {
		if cipherSuite == fallbackSCSV {
			scsvFound = true
			break
		}
	}

	if !scsvFound && config.Bugs.FailIfNotFallbackSCSV {
		return errors.New("tls: no fallback SCSV found when expected")
	} else if scsvFound && !config.Bugs.FailIfNotFallbackSCSV {
		return errors.New("tls: fallback SCSV found when not expected")
	}

	if config.Bugs.ExpectGREASE && !containsGREASE(hs.clientHello.cipherSuites) {
		return errors.New("tls: no GREASE cipher suite value found")
	}

	var greaseFound bool
	for _, curve := range hs.clientHello.supportedCurves {
		if isGREASEValue(uint16(curve)) {
			greaseFound = true
			break
		}
	}

	if !greaseFound && config.Bugs.ExpectGREASE {
		return errors.New("tls: no GREASE curve value found")
	}

	if len(hs.clientHello.keyShares) > 0 {
		greaseFound = false
		for _, keyShare := range hs.clientHello.keyShares {
			if isGREASEValue(uint16(keyShare.group)) {
				greaseFound = true
				break
			}
		}

		if !greaseFound && config.Bugs.ExpectGREASE {
			return errors.New("tls: no GREASE curve value found")
		}
	}

	if len(hs.clientHello.sessionID) == 0 && c.config.Bugs.ExpectClientHelloSessionID {
		return errors.New("tls: expected non-empty session ID from client")
	}

	applyBugsToClientHello(hs.clientHello, config)

	return nil
}

func applyBugsToClientHello(clientHello *clientHelloMsg, config *Config) {
	if config.Bugs.IgnorePeerSignatureAlgorithmPreferences {
		clientHello.signatureAlgorithms = config.signSignatureAlgorithms()
	}
	if config.Bugs.IgnorePeerCurvePreferences {
		clientHello.supportedCurves = config.curvePreferences()
	}
	if config.Bugs.IgnorePeerCipherPreferences {
		clientHello.cipherSuites = config.cipherSuites()
	}
}

func (hs *serverHandshakeState) doTLS13Handshake() error {
	c := hs.c
	config := c.config

	// We've read the ClientHello, so the next record must be preceded with ChangeCipherSpec.
	c.expectTLS13ChangeCipherSpec = true

	hs.hello = &serverHelloMsg{
		isDTLS:                c.isDTLS,
		vers:                  c.wireVersion,
		sessionID:             hs.clientHello.sessionID,
		compressionMethod:     config.Bugs.SendCompressionMethod,
		versOverride:          config.Bugs.SendServerHelloVersion,
		supportedVersOverride: config.Bugs.SendServerSupportedVersionExtension,
		omitSupportedVers:     config.Bugs.OmitServerSupportedVersionExtension,
		customExtension:       config.Bugs.CustomUnencryptedExtension,
		unencryptedALPN:       config.Bugs.SendUnencryptedALPN,
	}

	hs.hello.random = make([]byte, 32)
	if _, err := io.ReadFull(config.rand(), hs.hello.random); err != nil {
		c.sendAlert(alertInternalError)
		return err
	}

	// TLS 1.3 forbids clients from advertising any non-null compression.
	if len(hs.clientHello.compressionMethods) != 1 || hs.clientHello.compressionMethods[0] != compressionNone {
		return errors.New("tls: client sent compression method other than null for TLS 1.3")
	}

	// Prepare an EncryptedExtensions message, but do not send it yet.
	encryptedExtensions := new(encryptedExtensionsMsg)
	encryptedExtensions.empty = config.Bugs.EmptyEncryptedExtensions
	if err := hs.processClientExtensions(&encryptedExtensions.extensions); err != nil {
		return err
	}

	if config.Bugs.ExpectClientECH && hs.clientHello.clientECH == nil {
		return errors.New("tls: expected client to send ClientECH")
	}

	if hs.clientHello.clientECH != nil && len(config.Bugs.SendECHRetryConfigs) > 0 {
		encryptedExtensions.extensions.echRetryConfigs = config.Bugs.SendECHRetryConfigs
	}

	// Select the cipher suite.
	var preferenceList, supportedList []uint16
	if config.PreferServerCipherSuites {
		preferenceList = config.cipherSuites()
		supportedList = hs.clientHello.cipherSuites
	} else {
		preferenceList = hs.clientHello.cipherSuites
		supportedList = config.cipherSuites()
	}

	for _, id := range preferenceList {
		if hs.suite = c.tryCipherSuite(id, supportedList, c.vers, true, true); hs.suite != nil {
			break
		}
	}

	if hs.suite == nil {
		c.sendAlert(alertHandshakeFailure)
		return errors.New("tls: no cipher suite supported by both client and server")
	}

	hs.hello.cipherSuite = hs.suite.id
	if c.config.Bugs.SendCipherSuite != 0 {
		hs.hello.cipherSuite = c.config.Bugs.SendCipherSuite
	}

	hs.finishedHash = newFinishedHash(c.wireVersion, c.isDTLS, hs.suite)
	hs.finishedHash.discardHandshakeBuffer()
	hs.writeClientHash(hs.clientHello.marshal())

	supportedCurve := false
	var selectedCurve CurveID
	preferredCurves := config.curvePreferences()
Curves:
	for _, curve := range hs.clientHello.supportedCurves {
		for _, supported := range preferredCurves {
			if supported == curve {
				supportedCurve = true
				selectedCurve = curve
				break Curves
			}
		}
	}

	if !supportedCurve {
		c.sendAlert(alertHandshakeFailure)
		return errors.New("tls: no curve supported by both client and server")
	}

	pskIdentities := hs.clientHello.pskIdentities
	pskKEModes := hs.clientHello.pskKEModes

	var replacedPSKIdentities bool
	if len(pskIdentities) == 0 && len(hs.clientHello.sessionTicket) > 0 && c.config.Bugs.AcceptAnySession {
		// Pick up the ticket from the TLS 1.2 extension, to test the
		// client does not get in a mixed up state.
		psk := pskIdentity{
			ticket: hs.clientHello.sessionTicket,
		}
		pskIdentities = []pskIdentity{psk}
		pskKEModes = []byte{pskDHEKEMode}
		replacedPSKIdentities = true
	}

	var pskIndex int
	foundKEMode := bytes.IndexByte(pskKEModes, pskDHEKEMode) >= 0
	if foundKEMode && !config.SessionTicketsDisabled {
		for i, pskIdentity := range pskIdentities {
			// TODO(svaldez): Check the obfuscatedTicketAge before accepting 0-RTT.
			sessionState, ok := c.decryptTicket(pskIdentity.ticket)
			if !ok {
				continue
			}

			if !config.Bugs.AcceptAnySession {
				if sessionState.vers != c.vers {
					continue
				}
				if sessionState.ticketExpiration.Before(c.config.time()) {
					continue
				}
				sessionCipher := cipherSuiteFromID(sessionState.cipherSuite)
				if sessionCipher == nil || sessionCipher.hash() != hs.suite.hash() {
					continue
				}
			}

			clientTicketAge := time.Duration(uint32(pskIdentity.obfuscatedTicketAge-sessionState.ticketAgeAdd)) * time.Millisecond
			if config.Bugs.ExpectTicketAge != 0 && clientTicketAge != config.Bugs.ExpectTicketAge {
				c.sendAlert(alertHandshakeFailure)
				return errors.New("tls: invalid ticket age")
			}

			if !replacedPSKIdentities {
				binderToVerify := hs.clientHello.pskBinders[i]
				if err := verifyPSKBinder(c.wireVersion, hs.clientHello, sessionState, binderToVerify, []byte{}, []byte{}); err != nil {
					return err
				}
			}

			hs.sessionState = sessionState
			hs.hello.hasPSKIdentity = true
			hs.hello.pskIdentity = uint16(i)
			pskIndex = i
			if config.Bugs.SelectPSKIdentityOnResume != 0 {
				hs.hello.pskIdentity = config.Bugs.SelectPSKIdentityOnResume
			}
			c.didResume = true
			break
		}
	}

	if config.Bugs.AlwaysSelectPSKIdentity {
		hs.hello.hasPSKIdentity = true
		hs.hello.pskIdentity = 0
	}

	// Resolve PSK and compute the early secret.
	if hs.sessionState != nil {
		hs.finishedHash.addEntropy(hs.sessionState.secret)
	} else {
		hs.finishedHash.addEntropy(hs.finishedHash.zeroSecret())
	}

	hs.hello.hasKeyShare = true
	if hs.sessionState != nil && config.Bugs.NegotiatePSKResumption {
		hs.hello.hasKeyShare = false
	}
	if config.Bugs.MissingKeyShare {
		hs.hello.hasKeyShare = false
	}

	firstHelloRetryRequest := true

ResendHelloRetryRequest:
	var sendHelloRetryRequest bool
	cipherSuite := hs.suite.id
	if config.Bugs.SendHelloRetryRequestCipherSuite != 0 {
		cipherSuite = config.Bugs.SendHelloRetryRequestCipherSuite
	}
	helloRetryRequest := &helloRetryRequestMsg{
		vers:                c.wireVersion,
		sessionID:           hs.clientHello.sessionID,
		cipherSuite:         cipherSuite,
		compressionMethod:   config.Bugs.SendCompressionMethod,
		duplicateExtensions: config.Bugs.DuplicateHelloRetryRequestExtensions,
	}

	if config.Bugs.AlwaysSendHelloRetryRequest {
		sendHelloRetryRequest = true
	}

	if config.Bugs.SendHelloRetryRequestCookie != nil {
		sendHelloRetryRequest = true
		helloRetryRequest.cookie = config.Bugs.SendHelloRetryRequestCookie
	}

	if len(config.Bugs.CustomHelloRetryRequestExtension) > 0 {
		sendHelloRetryRequest = true
		helloRetryRequest.customExtension = config.Bugs.CustomHelloRetryRequestExtension
	}

	var selectedKeyShare *keyShareEntry
	if hs.hello.hasKeyShare {
		// Look for the key share corresponding to our selected curve.
		for i := range hs.clientHello.keyShares {
			if hs.clientHello.keyShares[i].group == selectedCurve {
				selectedKeyShare = &hs.clientHello.keyShares[i]
				break
			}
		}

		if config.Bugs.ExpectMissingKeyShare && selectedKeyShare != nil {
			return errors.New("tls: expected missing key share")
		}

		if selectedKeyShare == nil {
			helloRetryRequest.hasSelectedGroup = true
			helloRetryRequest.selectedGroup = selectedCurve
			sendHelloRetryRequest = true
		}
	}

	if config.Bugs.SendHelloRetryRequestCurve != 0 {
		helloRetryRequest.hasSelectedGroup = true
		helloRetryRequest.selectedGroup = config.Bugs.SendHelloRetryRequestCurve
		sendHelloRetryRequest = true
	}

	if config.Bugs.SkipHelloRetryRequest {
		sendHelloRetryRequest = false
	}

	if sendHelloRetryRequest {
		hs.finishedHash.UpdateForHelloRetryRequest()
		hs.writeServerHash(helloRetryRequest.marshal())
		if c.config.Bugs.PartialServerHelloWithHelloRetryRequest {
			data := helloRetryRequest.marshal()
			c.writeRecord(recordTypeHandshake, append(data[:len(data):len(data)], typeServerHello))
		} else {
			c.writeRecord(recordTypeHandshake, helloRetryRequest.marshal())
		}
		c.flushHandshake()

		if !c.config.Bugs.SkipChangeCipherSpec {
			c.writeRecord(recordTypeChangeCipherSpec, []byte{1})
		}

		if hs.clientHello.hasEarlyData {
			c.setSkipEarlyData()
		}

		// Read new ClientHello.
		newMsg, err := c.readHandshake()
		if err != nil {
			return err
		}
		newClientHello, ok := newMsg.(*clientHelloMsg)
		if !ok {
			c.sendAlert(alertUnexpectedMessage)
			return unexpectedMessageError(newClientHello, newMsg)
		}
		hs.writeClientHash(newClientHello.marshal())

		if config.Bugs.ExpectNoTLS13PSKAfterHRR && len(newClientHello.pskIdentities) > 0 {
			return fmt.Errorf("tls: client offered unexpected PSK identities after HelloRetryRequest")
		}

		applyBugsToClientHello(newClientHello, config)

		// Check that the new ClientHello matches the old ClientHello,
		// except for relevant modifications. See RFC 8446, section 4.1.2.
		ignoreExtensions := []uint16{extensionPadding}

		if helloRetryRequest.hasSelectedGroup {
			newKeyShares := newClientHello.keyShares
			if len(newKeyShares) != 1 || newKeyShares[0].group != helloRetryRequest.selectedGroup {
				return errors.New("tls: KeyShare from HelloRetryRequest not in new ClientHello")
			}
			selectedKeyShare = &newKeyShares[0]
			ignoreExtensions = append(ignoreExtensions, extensionKeyShare)
		}

		if len(helloRetryRequest.cookie) > 0 {
			if !bytes.Equal(newClientHello.tls13Cookie, helloRetryRequest.cookie) {
				return errors.New("tls: cookie from HelloRetryRequest not present in new ClientHello")
			}
			ignoreExtensions = append(ignoreExtensions, extensionCookie)
		}

		// The second ClientHello refreshes binders, and may drop PSK identities
		// that are no longer consistent with the cipher suite.
		oldPSKIdentities := hs.clientHello.pskIdentities
		for _, identity := range newClientHello.pskIdentities {
			// Skip to the matching PSK identity in oldPSKIdentities.
			for len(oldPSKIdentities) > 0 && !bytes.Equal(oldPSKIdentities[0].ticket, identity.ticket) {
				oldPSKIdentities = oldPSKIdentities[1:]
			}
			// The identity now either matches, or oldPSKIdentities is empty.
			if len(oldPSKIdentities) == 0 {
				return errors.New("tls: unexpected PSK identity in second ClientHello")
			}
			oldPSKIdentities = oldPSKIdentities[1:]
		}
		ignoreExtensions = append(ignoreExtensions, extensionPreSharedKey)

		// Update the index for the identity we resumed. The client may have
		// dropped some entries.
		if hs.sessionState != nil {
			var found bool
			ticket := hs.clientHello.pskIdentities[pskIndex].ticket
			for i, identity := range newClientHello.pskIdentities {
				if bytes.Equal(identity.ticket, ticket) {
					found = true
					pskIndex = i
					break
				}
			}
			if found {
				binderToVerify := newClientHello.pskBinders[pskIndex]
				if err := verifyPSKBinder(c.wireVersion, newClientHello, hs.sessionState, binderToVerify, hs.clientHello.marshal(), helloRetryRequest.marshal()); err != nil {
					return err
				}
			} else if !config.Bugs.AcceptAnySession {
				// If AcceptAnySession is set, the client may have already noticed
				// the selected session is incompatible with the HelloRetryRequest
				// and correctly dropped the PSK identity. We may also have
				// attempted to resume a session from the TLS 1.2 extension.
				return errors.New("tls: second ClientHello is missing selected session")
			}
		}

		// The second ClientHello must stop offering early data.
		if newClientHello.hasEarlyData {
			return errors.New("tls: EarlyData sent in new ClientHello")
		}
		ignoreExtensions = append(ignoreExtensions, extensionEarlyData)

		if err := checkClientHellosEqual(hs.clientHello.raw, newClientHello.raw, c.isDTLS, ignoreExtensions); err != nil {
			return err
		}

		if firstHelloRetryRequest && config.Bugs.SecondHelloRetryRequest {
			firstHelloRetryRequest = false
			goto ResendHelloRetryRequest
		}
	}

	// Decide whether or not to accept early data.
	if !sendHelloRetryRequest && hs.clientHello.hasEarlyData {
		if !config.Bugs.AlwaysRejectEarlyData && hs.sessionState != nil {
			if hs.sessionState.cipherSuite == hs.suite.id &&
				c.clientProtocol == string(hs.sessionState.earlyALPN) &&
				c.hasApplicationSettings == hs.sessionState.hasApplicationSettings &&
				bytes.Equal(c.localApplicationSettings, hs.sessionState.localApplicationSettings) {
				encryptedExtensions.extensions.hasEarlyData = true
			}
			if config.Bugs.AlwaysAcceptEarlyData {
				encryptedExtensions.extensions.hasEarlyData = true
			}
		}
		if encryptedExtensions.extensions.hasEarlyData {
			earlyTrafficSecret := hs.finishedHash.deriveSecret(earlyTrafficLabel)
			c.earlyExporterSecret = hs.finishedHash.deriveSecret(earlyExporterLabel)

			// Applications are implicit with early data.
			if !config.Bugs.SendApplicationSettingsWithEarlyData {
				encryptedExtensions.extensions.hasApplicationSettings = false
				encryptedExtensions.extensions.applicationSettings = nil
			}

			sessionCipher := cipherSuiteFromID(hs.sessionState.cipherSuite)
			if err := c.useInTrafficSecret(encryptionEarlyData, c.wireVersion, sessionCipher, earlyTrafficSecret); err != nil {
				return err
			}

			for _, expectedMsg := range config.Bugs.ExpectEarlyData {
				if err := c.readRecord(recordTypeApplicationData); err != nil {
					return err
				}
				msg := c.input.data[c.input.off:]
				if !bytes.Equal(msg, expectedMsg) {
					return fmt.Errorf("tls: got early data record %x, wanted %x", msg, expectedMsg)
				}
				c.in.freeBlock(c.input)
				c.input = nil
			}
		} else {
			c.setSkipEarlyData()
		}
	}

	if config.Bugs.SendEarlyDataExtension {
		encryptedExtensions.extensions.hasEarlyData = true
	}

	// Resolve ECDHE and compute the handshake secret.
	if hs.hello.hasKeyShare {
		// Once a curve has been selected and a key share identified,
		// the server needs to generate a public value and send it in
		// the ServerHello.
		curve, ok := curveForCurveID(selectedCurve, config)
		if !ok {
			panic("tls: server failed to look up curve ID")
		}
		c.curveID = selectedCurve

		var peerKey []byte
		if config.Bugs.SkipHelloRetryRequest {
			// If skipping HelloRetryRequest, use a random key to
			// avoid crashing.
			curve2, _ := curveForCurveID(selectedCurve, config)
			var err error
			peerKey, err = curve2.offer(config.rand())
			if err != nil {
				return err
			}
		} else {
			peerKey = selectedKeyShare.keyExchange
		}

		publicKey, ecdheSecret, err := curve.accept(config.rand(), peerKey)
		if err != nil {
			c.sendAlert(alertHandshakeFailure)
			return err
		}
		hs.finishedHash.nextSecret()
		hs.finishedHash.addEntropy(ecdheSecret)
		hs.hello.hasKeyShare = true

		curveID := selectedCurve
		if c.config.Bugs.SendCurve != 0 {
			curveID = config.Bugs.SendCurve
		}
		if c.config.Bugs.InvalidECDHPoint {
			publicKey[0] ^= 0xff
		}

		hs.hello.keyShare = keyShareEntry{
			group:       curveID,
			keyExchange: publicKey,
		}

		if config.Bugs.EncryptedExtensionsWithKeyShare {
			encryptedExtensions.extensions.hasKeyShare = true
			encryptedExtensions.extensions.keyShare = keyShareEntry{
				group:       curveID,
				keyExchange: publicKey,
			}
		}
	} else {
		hs.finishedHash.nextSecret()
		hs.finishedHash.addEntropy(hs.finishedHash.zeroSecret())
	}

	// Send unencrypted ServerHello.
	helloBytes := hs.hello.marshal()
	hs.writeServerHash(helloBytes)
	if config.Bugs.PartialServerHelloWithHelloRetryRequest {
		// The first byte has already been written.
		helloBytes = helloBytes[1:]
	}
	if config.Bugs.PartialEncryptedExtensionsWithServerHello {
		c.writeRecord(recordTypeHandshake, append(helloBytes[:len(helloBytes):len(helloBytes)], typeEncryptedExtensions))
	} else {
		c.writeRecord(recordTypeHandshake, helloBytes)
	}
	c.flushHandshake()

	if !c.config.Bugs.SkipChangeCipherSpec && !sendHelloRetryRequest {
		c.writeRecord(recordTypeChangeCipherSpec, []byte{1})
	}

	for i := 0; i < c.config.Bugs.SendExtraChangeCipherSpec; i++ {
		c.writeRecord(recordTypeChangeCipherSpec, []byte{1})
	}

	// Switch to handshake traffic keys.
	serverHandshakeTrafficSecret := hs.finishedHash.deriveSecret(serverHandshakeTrafficLabel)
	c.useOutTrafficSecret(encryptionHandshake, c.wireVersion, hs.suite, serverHandshakeTrafficSecret)
	// Derive handshake traffic read key, but don't switch yet.
	clientHandshakeTrafficSecret := hs.finishedHash.deriveSecret(clientHandshakeTrafficLabel)

	// Send EncryptedExtensions.
	hs.writeServerHash(encryptedExtensions.marshal())
	if config.Bugs.PartialEncryptedExtensionsWithServerHello {
		// The first byte has already been sent.
		c.writeRecord(recordTypeHandshake, encryptedExtensions.marshal()[1:])
	} else {
		c.writeRecord(recordTypeHandshake, encryptedExtensions.marshal())
	}

	if hs.sessionState == nil {
		if config.ClientAuth >= RequestClientCert {
			// Request a client certificate
			certReq := &certificateRequestMsg{
				vers:                  c.wireVersion,
				hasSignatureAlgorithm: !config.Bugs.OmitCertificateRequestAlgorithms,
				hasRequestContext:     true,
				requestContext:        config.Bugs.SendRequestContext,
				customExtension:       config.Bugs.SendCustomCertificateRequest,
			}
			if !config.Bugs.NoSignatureAlgorithms {
				certReq.signatureAlgorithms = config.verifySignatureAlgorithms()
			}

			// An empty list of certificateAuthorities signals to
			// the client that it may send any certificate in response
			// to our request. When we know the CAs we trust, then
			// we can send them down, so that the client can choose
			// an appropriate certificate to give to us.
			if config.ClientCAs != nil {
				certReq.certificateAuthorities = config.ClientCAs.Subjects()
			}
			hs.writeServerHash(certReq.marshal())
			c.writeRecord(recordTypeHandshake, certReq.marshal())
		}

		certMsg := &certificateMsg{
			hasRequestContext: true,
		}
		if !config.Bugs.EmptyCertificateList {
			for i, certData := range hs.cert.Certificate {
				cert := certificateEntry{
					data: certData,
				}
				if i == 0 {
					if hs.clientHello.ocspStapling && !c.config.Bugs.NoOCSPStapling {
						cert.ocspResponse = hs.cert.OCSPStaple
					}
					if hs.clientHello.sctListSupported && !c.config.Bugs.NoSignedCertificateTimestamps {
						cert.sctList = hs.cert.SignedCertificateTimestampList
					}
					cert.duplicateExtensions = config.Bugs.SendDuplicateCertExtensions
					cert.extraExtension = config.Bugs.SendExtensionOnCertificate
				} else {
					if config.Bugs.SendOCSPOnIntermediates != nil {
						cert.ocspResponse = config.Bugs.SendOCSPOnIntermediates
					}
					if config.Bugs.SendSCTOnIntermediates != nil {
						cert.sctList = config.Bugs.SendSCTOnIntermediates
					}
				}
				certMsg.certificates = append(certMsg.certificates, cert)
			}
		}
		certMsgBytes := certMsg.marshal()
		sentCompressedCertMsg := false

	FindCertCompressionAlg:
		for candidate, alg := range c.config.CertCompressionAlgs {
			for _, id := range hs.clientHello.compressedCertAlgs {
				if id == candidate {
					if expected := config.Bugs.ExpectedCompressedCert; expected != 0 && expected != id {
						return fmt.Errorf("expected to send compressed cert with alg %d, but picked %d", expected, id)
					}

					if override := config.Bugs.SendCertCompressionAlgID; override != 0 {
						id = override
					}

					uncompressed := certMsgBytes[4:]
					uncompressedLen := uint32(len(uncompressed))
					if override := config.Bugs.SendCertUncompressedLength; override != 0 {
						uncompressedLen = override
					}

					compressedCertMsgBytes := (&compressedCertificateMsg{
						algID:              id,
						uncompressedLength: uncompressedLen,
						compressed:         alg.Compress(uncompressed),
					}).marshal()

					hs.writeServerHash(compressedCertMsgBytes)
					c.writeRecord(recordTypeHandshake, compressedCertMsgBytes)
					sentCompressedCertMsg = true
					break FindCertCompressionAlg
				}
			}
		}

		if !sentCompressedCertMsg {
			if config.Bugs.ExpectedCompressedCert != 0 {
				return errors.New("unexpectedly sent uncompressed certificate")
			}
			hs.writeServerHash(certMsgBytes)
			c.writeRecord(recordTypeHandshake, certMsgBytes)
		}

		certVerify := &certificateVerifyMsg{
			hasSignatureAlgorithm: true,
		}

		// Determine the hash to sign.
		privKey := hs.cert.PrivateKey

		var err error
		certVerify.signatureAlgorithm, err = selectSignatureAlgorithm(c.vers, privKey, config, hs.clientHello.signatureAlgorithms)
		if err != nil {
			c.sendAlert(alertInternalError)
			return err
		}

		input := hs.finishedHash.certificateVerifyInput(serverCertificateVerifyContextTLS13)
		certVerify.signature, err = signMessage(c.vers, privKey, c.config, certVerify.signatureAlgorithm, input)
		if err != nil {
			c.sendAlert(alertInternalError)
			return err
		}

		if config.Bugs.SendSignatureAlgorithm != 0 {
			certVerify.signatureAlgorithm = config.Bugs.SendSignatureAlgorithm
		}

		if !config.Bugs.SkipCertificateVerify {
			hs.writeServerHash(certVerify.marshal())
			c.writeRecord(recordTypeHandshake, certVerify.marshal())
		}
	} else if hs.sessionState != nil {
		// Pick up certificates from the session instead.
		if len(hs.sessionState.certificates) > 0 {
			if _, err := hs.processCertsFromClient(hs.sessionState.certificates); err != nil {
				return err
			}
		}
	}

	finished := new(finishedMsg)
	finished.verifyData = hs.finishedHash.serverSum(serverHandshakeTrafficSecret)
	if config.Bugs.BadFinished {
		finished.verifyData[0]++
	}
	hs.writeServerHash(finished.marshal())
	c.writeRecord(recordTypeHandshake, finished.marshal())
	if c.config.Bugs.SendExtraFinished {
		c.writeRecord(recordTypeHandshake, finished.marshal())
	}
	c.flushHandshake()

	if encryptedExtensions.extensions.hasEarlyData && !c.shouldSkipEarlyData() {
		for _, expectedMsg := range config.Bugs.ExpectLateEarlyData {
			if err := c.readRecord(recordTypeApplicationData); err != nil {
				return err
			}
			if !bytes.Equal(c.input.data[c.input.off:], expectedMsg) {
				return errors.New("ExpectLateEarlyData: did not get expected message")
			}
			c.in.freeBlock(c.input)
			c.input = nil
		}
	}

	// The various secrets do not incorporate the client's final leg, so
	// derive them now before updating the handshake context.
	hs.finishedHash.nextSecret()
	hs.finishedHash.addEntropy(hs.finishedHash.zeroSecret())

	clientTrafficSecret := hs.finishedHash.deriveSecret(clientApplicationTrafficLabel)
	serverTrafficSecret := hs.finishedHash.deriveSecret(serverApplicationTrafficLabel)
	c.exporterSecret = hs.finishedHash.deriveSecret(exporterLabel)

	// Switch to application data keys on write. In particular, any alerts
	// from the client certificate are sent over these keys.
	c.useOutTrafficSecret(encryptionApplication, c.wireVersion, hs.suite, serverTrafficSecret)

	// Send 0.5-RTT messages.
	for _, halfRTTMsg := range config.Bugs.SendHalfRTTData {
		if _, err := c.writeRecord(recordTypeApplicationData, halfRTTMsg); err != nil {
			return err
		}
	}

	// Read end_of_early_data.
	if encryptedExtensions.extensions.hasEarlyData && config.Bugs.MockQUICTransport == nil {
		msg, err := c.readHandshake()
		if err != nil {
			return err
		}

		endOfEarlyData, ok := msg.(*endOfEarlyDataMsg)
		if !ok {
			c.sendAlert(alertUnexpectedMessage)
			return unexpectedMessageError(endOfEarlyData, msg)
		}
		hs.writeClientHash(endOfEarlyData.marshal())
	}

	// Switch input stream to handshake traffic keys.
	if err := c.useInTrafficSecret(encryptionHandshake, c.wireVersion, hs.suite, clientHandshakeTrafficSecret); err != nil {
		return err
	}

	// If we sent an ALPS extension, the client must respond with one.
	if encryptedExtensions.extensions.hasApplicationSettings {
		msg, err := c.readHandshake()
		if err != nil {
			return err
		}
		clientEncryptedExtensions, ok := msg.(*clientEncryptedExtensionsMsg)
		if !ok {
			c.sendAlert(alertUnexpectedMessage)
			return unexpectedMessageError(clientEncryptedExtensions, msg)
		}
		hs.writeClientHash(clientEncryptedExtensions.marshal())

		if !clientEncryptedExtensions.hasApplicationSettings {
			c.sendAlert(alertMissingExtension)
			return errors.New("tls: client didn't provide application settings")
		}
		c.peerApplicationSettings = clientEncryptedExtensions.applicationSettings
	} else if encryptedExtensions.extensions.hasEarlyData {
		// 0-RTT sessions carry application settings over.
		c.peerApplicationSettings = hs.sessionState.peerApplicationSettings
	}

	// If we requested a client certificate, then the client must send a
	// certificate message, even if it's empty.
	if config.ClientAuth >= RequestClientCert {
		msg, err := c.readHandshake()
		if err != nil {
			return err
		}

		certMsg, ok := msg.(*certificateMsg)
		if !ok {
			c.sendAlert(alertUnexpectedMessage)
			return unexpectedMessageError(certMsg, msg)
		}
		hs.writeClientHash(certMsg.marshal())

		if len(certMsg.certificates) == 0 {
			// The client didn't actually send a certificate
			switch config.ClientAuth {
			case RequireAnyClientCert, RequireAndVerifyClientCert:
				c.sendAlert(alertCertificateRequired)
				return errors.New("tls: client didn't provide a certificate")
			}
		}

		var certs [][]byte
		for _, cert := range certMsg.certificates {
			certs = append(certs, cert.data)
			// OCSP responses and SCT lists are not negotiated in
			// client certificates.
			if cert.ocspResponse != nil || cert.sctList != nil {
				c.sendAlert(alertUnsupportedExtension)
				return errors.New("tls: unexpected extensions in the client certificate")
			}
		}
		pub, err := hs.processCertsFromClient(certs)
		if err != nil {
			return err
		}

		if len(c.peerCertificates) > 0 {
			msg, err = c.readHandshake()
			if err != nil {
				return err
			}

			certVerify, ok := msg.(*certificateVerifyMsg)
			if !ok {
				c.sendAlert(alertUnexpectedMessage)
				return unexpectedMessageError(certVerify, msg)
			}

			c.peerSignatureAlgorithm = certVerify.signatureAlgorithm
			input := hs.finishedHash.certificateVerifyInput(clientCertificateVerifyContextTLS13)
			if err := verifyMessage(c.vers, pub, config, certVerify.signatureAlgorithm, input, certVerify.signature); err != nil {
				c.sendAlert(alertBadCertificate)
				return err
			}
			hs.writeClientHash(certVerify.marshal())
		}
	}

	if encryptedExtensions.extensions.channelIDRequested {
		msg, err := c.readHandshake()
		if err != nil {
			return err
		}
		channelIDMsg, ok := msg.(*channelIDMsg)
		if !ok {
			c.sendAlert(alertUnexpectedMessage)
			return unexpectedMessageError(channelIDMsg, msg)
		}
		channelIDHash := crypto.SHA256.New()
		channelIDHash.Write(hs.finishedHash.certificateVerifyInput(channelIDContextTLS13))
		channelID, err := verifyChannelIDMessage(channelIDMsg, channelIDHash.Sum(nil))
		if err != nil {
			return err
		}
		c.channelID = channelID

		hs.writeClientHash(channelIDMsg.marshal())
	}

	// Read the client Finished message.
	msg, err := c.readHandshake()
	if err != nil {
		return err
	}
	clientFinished, ok := msg.(*finishedMsg)
	if !ok {
		c.sendAlert(alertUnexpectedMessage)
		return unexpectedMessageError(clientFinished, msg)
	}

	verify := hs.finishedHash.clientSum(clientHandshakeTrafficSecret)
	if len(verify) != len(clientFinished.verifyData) ||
		subtle.ConstantTimeCompare(verify, clientFinished.verifyData) != 1 {
		c.sendAlert(alertHandshakeFailure)
		return errors.New("tls: client's Finished message was incorrect")
	}
	hs.writeClientHash(clientFinished.marshal())

	// Switch to application data keys on read.
	if err := c.useInTrafficSecret(encryptionApplication, c.wireVersion, hs.suite, clientTrafficSecret); err != nil {
		return err
	}

	c.cipherSuite = hs.suite
	c.resumptionSecret = hs.finishedHash.deriveSecret(resumptionLabel)

	// TODO(davidben): Allow configuring the number of tickets sent for
	// testing.
	if !c.config.SessionTicketsDisabled && foundKEMode {
		ticketCount := 2
		for i := 0; i < ticketCount; i++ {
			c.SendNewSessionTicket([]byte{byte(i)})
		}
	}
	return nil
}

// processClientHello processes the ClientHello message from the client and
// decides whether we will perform session resumption.
func (hs *serverHandshakeState) processClientHello() (isResume bool, err error) {
	config := hs.c.config
	c := hs.c

	hs.hello = &serverHelloMsg{
		isDTLS:            c.isDTLS,
		vers:              c.wireVersion,
		versOverride:      config.Bugs.SendServerHelloVersion,
		compressionMethod: config.Bugs.SendCompressionMethod,
		extensions: serverExtensions{
			supportedVersion: config.Bugs.SendServerSupportedVersionExtension,
		},
		omitExtensions:  config.Bugs.OmitExtensions,
		emptyExtensions: config.Bugs.EmptyExtensions,
	}

	hs.hello.random = make([]byte, 32)
	_, err = io.ReadFull(config.rand(), hs.hello.random)
	if err != nil {
		c.sendAlert(alertInternalError)
		return false, err
	}

	_, supportsTLS13 := c.config.isSupportedVersion(VersionTLS13, false)

	// Signal downgrades in the server random, per RFC 8446, section 4.1.3.
	if supportsTLS13 || config.Bugs.SendTLS13DowngradeRandom {
		if c.vers <= VersionTLS12 && config.maxVersion(c.isDTLS) >= VersionTLS13 {
			copy(hs.hello.random[len(hs.hello.random)-8:], downgradeTLS13)
		}
		if c.vers <= VersionTLS11 && config.maxVersion(c.isDTLS) == VersionTLS12 {
			copy(hs.hello.random[len(hs.hello.random)-8:], downgradeTLS12)
		}
	}
	if config.Bugs.SendJDK11DowngradeRandom {
		copy(hs.hello.random[len(hs.hello.random)-8:], downgradeJDK11)
	}

	foundCompression := false
	// We only support null compression, so check that the client offered it.
	for _, compression := range hs.clientHello.compressionMethods {
		if compression == compressionNone {
			foundCompression = true
			break
		}
	}

	if !foundCompression {
		c.sendAlert(alertHandshakeFailure)
		return false, errors.New("tls: client does not support uncompressed connections")
	}

	if err := hs.processClientExtensions(&hs.hello.extensions); err != nil {
		return false, err
	}

	supportedCurve := false
	preferredCurves := config.curvePreferences()
Curves:
	for _, curve := range hs.clientHello.supportedCurves {
		if isPqGroup(curve) && c.vers < VersionTLS13 {
			// CECPQ2 is TLS 1.3-only.
			continue
		}

		for _, supported := range preferredCurves {
			if supported == curve {
				supportedCurve = true
				break Curves
			}
		}
	}

	supportedPointFormat := false
	for _, pointFormat := range hs.clientHello.supportedPoints {
		if pointFormat == pointFormatUncompressed {
			supportedPointFormat = true
			break
		}
	}
	hs.ellipticOk = supportedCurve && supportedPointFormat

	_, hs.ecdsaOk = hs.cert.PrivateKey.(*ecdsa.PrivateKey)
	// Ed25519 also uses ECDSA certificates.
	_, ed25519Ok := hs.cert.PrivateKey.(ed25519.PrivateKey)
	hs.ecdsaOk = hs.ecdsaOk || ed25519Ok

	// For test purposes, check that the peer never offers a session when
	// renegotiating.
	if c.cipherSuite != nil && len(hs.clientHello.sessionID) > 0 && c.config.Bugs.FailIfResumeOnRenego {
		return false, errors.New("tls: offered resumption on renegotiation")
	}

	if c.config.Bugs.FailIfSessionOffered && (len(hs.clientHello.sessionTicket) > 0 || len(hs.clientHello.sessionID) > 0) {
		return false, errors.New("tls: client offered a session ticket or ID")
	}

	if hs.checkForResumption() {
		return true, nil
	}

	var preferenceList, supportedList []uint16
	if c.config.PreferServerCipherSuites {
		preferenceList = c.config.cipherSuites()
		supportedList = hs.clientHello.cipherSuites
	} else {
		preferenceList = hs.clientHello.cipherSuites
		supportedList = c.config.cipherSuites()
	}

	for _, id := range preferenceList {
		if hs.suite = c.tryCipherSuite(id, supportedList, c.vers, hs.ellipticOk, hs.ecdsaOk); hs.suite != nil {
			break
		}
	}

	if hs.suite == nil {
		c.sendAlert(alertHandshakeFailure)
		return false, errors.New("tls: no cipher suite supported by both client and server")
	}

	return false, nil
}

// processClientExtensions processes all ClientHello extensions not directly
// related to cipher suite negotiation and writes responses in serverExtensions.
func (hs *serverHandshakeState) processClientExtensions(serverExtensions *serverExtensions) error {
	config := hs.c.config
	c := hs.c

	if c.vers < VersionTLS13 || config.Bugs.NegotiateRenegotiationInfoAtAllVersions {
		if !bytes.Equal(c.clientVerify, hs.clientHello.secureRenegotiation) {
			c.sendAlert(alertHandshakeFailure)
			return errors.New("tls: renegotiation mismatch")
		}

		if len(c.clientVerify) > 0 && !c.config.Bugs.EmptyRenegotiationInfo {
			serverExtensions.secureRenegotiation = append(serverExtensions.secureRenegotiation, c.clientVerify...)
			serverExtensions.secureRenegotiation = append(serverExtensions.secureRenegotiation, c.serverVerify...)
			if c.config.Bugs.BadRenegotiationInfo {
				serverExtensions.secureRenegotiation[0] ^= 0x80
			}
			if c.config.Bugs.BadRenegotiationInfoEnd {
				serverExtensions.secureRenegotiation[len(serverExtensions.secureRenegotiation)-1] ^= 0x80
			}
		} else {
			serverExtensions.secureRenegotiation = hs.clientHello.secureRenegotiation
		}

		if c.noRenegotiationInfo() {
			serverExtensions.secureRenegotiation = nil
		}
	}

	serverExtensions.duplicateExtension = c.config.Bugs.DuplicateExtension

	if len(hs.clientHello.serverName) > 0 {
		c.serverName = hs.clientHello.serverName
	}
	if len(config.Certificates) == 0 {
		c.sendAlert(alertInternalError)
		return errors.New("tls: no certificates configured")
	}
	hs.cert = &config.Certificates[0]
	if len(hs.clientHello.serverName) > 0 {
		hs.cert = config.getCertificateForName(hs.clientHello.serverName)
	}
	if expected := c.config.Bugs.ExpectServerName; expected != "" && expected != hs.clientHello.serverName {
		return errors.New("tls: unexpected server name")
	}

	if cert := config.Bugs.RenegotiationCertificate; c.cipherSuite != nil && cert != nil {
		hs.cert = cert
	}

	if len(hs.clientHello.alpnProtocols) > 0 {
		// We will never offer ALPN as a client on renegotiation
		// handshakes.
		if len(c.clientVerify) > 0 {
			return errors.New("tls: offered ALPN on renegotiation")
		}
		if proto := c.config.Bugs.ALPNProtocol; proto != nil {
			serverExtensions.alpnProtocol = *proto
			serverExtensions.alpnProtocolEmpty = len(*proto) == 0
			c.clientProtocol = *proto
			c.usedALPN = true
		} else if selectedProto, fallback := mutualProtocol(hs.clientHello.alpnProtocols, c.config.NextProtos); !fallback {
			serverExtensions.alpnProtocol = selectedProto
			c.clientProtocol = selectedProto
			c.usedALPN = true
		}

		var alpsAllowed bool
		if c.vers >= VersionTLS13 {
			for _, proto := range hs.clientHello.alpsProtocols {
				if proto == c.clientProtocol {
					alpsAllowed = true
					break
				}
			}
		}
		if c.config.Bugs.AlwaysNegotiateApplicationSettings {
			alpsAllowed = true
		}
		if settings, ok := c.config.ApplicationSettings[c.clientProtocol]; ok && alpsAllowed {
			c.hasApplicationSettings = true
			c.localApplicationSettings = settings
			// Note these fields may later be cleared we accept 0-RTT.
			serverExtensions.hasApplicationSettings = true
			serverExtensions.applicationSettings = settings
		}
	}

	if len(c.config.Bugs.SendALPN) > 0 {
		serverExtensions.alpnProtocol = c.config.Bugs.SendALPN
	}

	if c.vers < VersionTLS13 || config.Bugs.NegotiateNPNAtAllVersions {
		if len(hs.clientHello.alpnProtocols) == 0 || c.config.Bugs.NegotiateALPNAndNPN {
			// Although sending an empty NPN extension is reasonable, Firefox has
			// had a bug around this. Best to send nothing at all if
			// config.NextProtos is empty. See
			// https://code.google.com/p/go/issues/detail?id=5445.
			if hs.clientHello.nextProtoNeg && len(config.NextProtos) > 0 {
				serverExtensions.nextProtoNeg = true
				serverExtensions.nextProtos = config.NextProtos
				serverExtensions.npnAfterAlpn = config.Bugs.SwapNPNAndALPN
			}
		}
	}

	if len(hs.clientHello.quicTransportParams) > 0 {
		c.quicTransportParams = hs.clientHello.quicTransportParams
	}
	if c.config.QUICTransportParamsUseLegacyCodepoint.IncludeStandard() {
		serverExtensions.quicTransportParams = c.config.QUICTransportParams
	}

	if len(hs.clientHello.quicTransportParamsLegacy) > 0 {
		c.quicTransportParamsLegacy = hs.clientHello.quicTransportParamsLegacy
	}
	if c.config.QUICTransportParamsUseLegacyCodepoint.IncludeLegacy() {
		serverExtensions.quicTransportParamsLegacy = c.config.QUICTransportParams
	}

	if c.vers < VersionTLS13 || config.Bugs.NegotiateEMSAtAllVersions {
		disableEMS := config.Bugs.NoExtendedMasterSecret
		if c.cipherSuite != nil {
			disableEMS = config.Bugs.NoExtendedMasterSecretOnRenegotiation
		}
		serverExtensions.extendedMasterSecret = hs.clientHello.extendedMasterSecret && !disableEMS
	}

	if hs.clientHello.channelIDSupported && config.RequestChannelID {
		serverExtensions.channelIDRequested = true
	}

	if config.TokenBindingParams != nil {
		if !bytes.Equal(config.ExpectTokenBindingParams, hs.clientHello.tokenBindingParams) {
			return errors.New("client did not send expected token binding params")
		}

		// For testing, blindly send whatever is set in config, even if
		// it is invalid.
		serverExtensions.tokenBindingParams = config.TokenBindingParams
		serverExtensions.tokenBindingVersion = config.TokenBindingVersion
	}

	if c.vers < VersionTLS13 && len(hs.clientHello.tokenBindingParams) > 0 && (!hs.clientHello.extendedMasterSecret || hs.clientHello.secureRenegotiation == nil) {
		return errors.New("client sent Token Binding without EMS and/or RI")
	}

	if hs.clientHello.srtpProtectionProfiles != nil {
	SRTPLoop:
		for _, p1 := range c.config.SRTPProtectionProfiles {
			for _, p2 := range hs.clientHello.srtpProtectionProfiles {
				if p1 == p2 {
					serverExtensions.srtpProtectionProfile = p1
					c.srtpProtectionProfile = p1
					break SRTPLoop
				}
			}
		}
	}

	if c.config.Bugs.SendSRTPProtectionProfile != 0 {
		serverExtensions.srtpProtectionProfile = c.config.Bugs.SendSRTPProtectionProfile
	}

	if expected := c.config.Bugs.ExpectedCustomExtension; expected != nil {
		if hs.clientHello.customExtension != *expected {
			return fmt.Errorf("tls: bad custom extension contents %q", hs.clientHello.customExtension)
		}
	}
	serverExtensions.customExtension = config.Bugs.CustomExtension

	if c.config.Bugs.AdvertiseTicketExtension {
		serverExtensions.ticketSupported = true
	}

	if c.config.Bugs.SendSupportedPointFormats != nil {
		serverExtensions.supportedPoints = c.config.Bugs.SendSupportedPointFormats
	}

	if c.config.Bugs.SendServerSupportedCurves {
		serverExtensions.supportedCurves = c.config.curvePreferences()
	}

	if !hs.clientHello.hasGREASEExtension && config.Bugs.ExpectGREASE {
		return errors.New("tls: no GREASE extension found")
	}

	serverExtensions.serverNameAck = c.config.Bugs.SendServerNameAck

	return nil
}

// checkForResumption returns true if we should perform resumption on this connection.
func (hs *serverHandshakeState) checkForResumption() bool {
	c := hs.c

	ticket := hs.clientHello.sessionTicket
	if len(ticket) == 0 && len(hs.clientHello.pskIdentities) > 0 && c.config.Bugs.AcceptAnySession {
		ticket = hs.clientHello.pskIdentities[0].ticket
	}
	if len(ticket) > 0 {
		if c.config.SessionTicketsDisabled {
			return false
		}

		var ok bool
		if hs.sessionState, ok = c.decryptTicket(ticket); !ok {
			return false
		}
	} else {
		if c.config.ServerSessionCache == nil {
			return false
		}

		var ok bool
		sessionID := string(hs.clientHello.sessionID)
		if hs.sessionState, ok = c.config.ServerSessionCache.Get(sessionID); !ok {
			return false
		}
	}

	if c.config.Bugs.AcceptAnySession {
		// Replace the cipher suite with one known to work, to test
		// cross-version resumption attempts.
		hs.sessionState.cipherSuite = TLS_RSA_WITH_AES_128_CBC_SHA
	} else {
		// Never resume a session for a different SSL version.
		if c.vers != hs.sessionState.vers {
			return false
		}

		cipherSuiteOk := false
		// Check that the client is still offering the ciphersuite in the session.
		for _, id := range hs.clientHello.cipherSuites {
			if id == hs.sessionState.cipherSuite {
				cipherSuiteOk = true
				break
			}
		}
		if !cipherSuiteOk {
			return false
		}
	}

	// Check that we also support the ciphersuite from the session.
	hs.suite = c.tryCipherSuite(hs.sessionState.cipherSuite, c.config.cipherSuites(), c.vers, hs.ellipticOk, hs.ecdsaOk)

	if hs.suite == nil {
		return false
	}

	sessionHasClientCerts := len(hs.sessionState.certificates) != 0
	needClientCerts := c.config.ClientAuth == RequireAnyClientCert || c.config.ClientAuth == RequireAndVerifyClientCert
	if needClientCerts && !sessionHasClientCerts {
		return false
	}
	if sessionHasClientCerts && c.config.ClientAuth == NoClientCert {
		return false
	}

	return true
}

func (hs *serverHandshakeState) doResumeHandshake() error {
	c := hs.c

	hs.hello.cipherSuite = hs.suite.id
	if c.config.Bugs.SendCipherSuite != 0 {
		hs.hello.cipherSuite = c.config.Bugs.SendCipherSuite
	}
	// We echo the client's session ID in the ServerHello to let it know
	// that we're doing a resumption.
	hs.hello.sessionID = hs.clientHello.sessionID
	hs.hello.extensions.ticketSupported = c.config.Bugs.RenewTicketOnResume

	if c.config.Bugs.SendSCTListOnResume != nil {
		hs.hello.extensions.sctList = c.config.Bugs.SendSCTListOnResume
	}

	if c.config.Bugs.SendOCSPResponseOnResume != nil {
		// There is no way, syntactically, to send an OCSP response on a
		// resumption handshake.
		hs.hello.extensions.ocspStapling = true
	}

	hs.finishedHash = newFinishedHash(c.wireVersion, c.isDTLS, hs.suite)
	hs.finishedHash.discardHandshakeBuffer()
	hs.writeClientHash(hs.clientHello.marshal())
	hs.writeServerHash(hs.hello.marshal())

	c.writeRecord(recordTypeHandshake, hs.hello.marshal())

	if len(hs.sessionState.certificates) > 0 {
		if _, err := hs.processCertsFromClient(hs.sessionState.certificates); err != nil {
			return err
		}
	}

	hs.masterSecret = hs.sessionState.secret
	c.extendedMasterSecret = hs.sessionState.extendedMasterSecret

	return nil
}

func (hs *serverHandshakeState) doFullHandshake() error {
	config := hs.c.config
	c := hs.c

	isPSK := hs.suite.flags&suitePSK != 0
	if !isPSK && hs.clientHello.ocspStapling && len(hs.cert.OCSPStaple) > 0 && !c.config.Bugs.NoOCSPStapling {
		hs.hello.extensions.ocspStapling = true
	}

	if hs.clientHello.sctListSupported && len(hs.cert.SignedCertificateTimestampList) > 0 && !c.config.Bugs.NoSignedCertificateTimestamps {
		hs.hello.extensions.sctList = hs.cert.SignedCertificateTimestampList
	}

	if len(c.clientVerify) > 0 && config.Bugs.SendSCTListOnRenegotiation != nil {
		hs.hello.extensions.sctList = config.Bugs.SendSCTListOnRenegotiation
	}

	hs.hello.extensions.ticketSupported = hs.clientHello.ticketSupported && !config.SessionTicketsDisabled
	hs.hello.cipherSuite = hs.suite.id
	if config.Bugs.SendCipherSuite != 0 {
		hs.hello.cipherSuite = config.Bugs.SendCipherSuite
	}
	c.extendedMasterSecret = hs.hello.extensions.extendedMasterSecret

	// Generate a session ID if we're to save the session.
	if !hs.hello.extensions.ticketSupported && config.ServerSessionCache != nil {
		hs.hello.sessionID = make([]byte, 32)
		if _, err := io.ReadFull(config.rand(), hs.hello.sessionID); err != nil {
			c.sendAlert(alertInternalError)
			return errors.New("tls: short read from Rand: " + err.Error())
		}
	}
	if config.Bugs.EchoSessionIDInFullHandshake {
		hs.hello.sessionID = hs.clientHello.sessionID
	}

	hs.finishedHash = newFinishedHash(c.wireVersion, c.isDTLS, hs.suite)
	hs.writeClientHash(hs.clientHello.marshal())
	hs.writeServerHash(hs.hello.marshal())

	if config.Bugs.SendSNIWarningAlert {
		c.SendAlert(alertLevelWarning, alertUnrecognizedName)
	}

	c.writeRecord(recordTypeHandshake, hs.hello.marshal())

	if !isPSK {
		certMsg := new(certificateMsg)
		if !config.Bugs.EmptyCertificateList {
			for _, certData := range hs.cert.Certificate {
				certMsg.certificates = append(certMsg.certificates, certificateEntry{
					data: certData,
				})
			}
		}
		if !config.Bugs.UnauthenticatedECDH {
			certMsgBytes := certMsg.marshal()
			hs.writeServerHash(certMsgBytes)
			c.writeRecord(recordTypeHandshake, certMsgBytes)
		}
	}

	if hs.hello.extensions.ocspStapling && !c.config.Bugs.SkipCertificateStatus {
		certStatus := new(certificateStatusMsg)
		certStatus.statusType = statusTypeOCSP
		certStatus.response = hs.cert.OCSPStaple
		if len(c.clientVerify) > 0 && config.Bugs.SendOCSPResponseOnRenegotiation != nil {
			certStatus.response = config.Bugs.SendOCSPResponseOnRenegotiation
		}
		hs.writeServerHash(certStatus.marshal())
		c.writeRecord(recordTypeHandshake, certStatus.marshal())
	}

	keyAgreement := hs.suite.ka(c.vers)
	skx, err := keyAgreement.generateServerKeyExchange(config, hs.cert, hs.clientHello, hs.hello, c.vers)
	if err != nil {
		c.sendAlert(alertHandshakeFailure)
		return err
	}
	if ecdhe, ok := keyAgreement.(*ecdheKeyAgreement); ok {
		c.curveID = ecdhe.curveID
	}
	if skx != nil && !config.Bugs.SkipServerKeyExchange {
		hs.writeServerHash(skx.marshal())
		c.writeRecord(recordTypeHandshake, skx.marshal())
	}

	if config.ClientAuth >= RequestClientCert {
		// Request a client certificate
		certReq := &certificateRequestMsg{
			vers:             c.wireVersion,
			certificateTypes: config.ClientCertificateTypes,
		}
		if certReq.certificateTypes == nil {
			certReq.certificateTypes = []byte{
				byte(CertTypeRSASign),
				byte(CertTypeECDSASign),
			}
		}
		if c.vers >= VersionTLS12 {
			certReq.hasSignatureAlgorithm = true
			if !config.Bugs.NoSignatureAlgorithms {
				certReq.signatureAlgorithms = config.verifySignatureAlgorithms()
			}
		}

		// An empty list of certificateAuthorities signals to
		// the client that it may send any certificate in response
		// to our request. When we know the CAs we trust, then
		// we can send them down, so that the client can choose
		// an appropriate certificate to give to us.
		if config.ClientCAs != nil {
			certReq.certificateAuthorities = config.ClientCAs.Subjects()
		}
		hs.writeServerHash(certReq.marshal())
		c.writeRecord(recordTypeHandshake, certReq.marshal())
	}

	helloDone := new(serverHelloDoneMsg)
	helloDoneBytes := helloDone.marshal()
	hs.writeServerHash(helloDoneBytes)
	var toAppend byte
	if config.Bugs.PartialNewSessionTicketWithServerHelloDone {
		toAppend = typeNewSessionTicket
	} else if config.Bugs.PartialFinishedWithServerHelloDone {
		toAppend = typeFinished
	}
	if toAppend != 0 {
		c.writeRecord(recordTypeHandshake, append(helloDoneBytes[:len(helloDoneBytes):len(helloDoneBytes)], toAppend))
	} else {
		c.writeRecord(recordTypeHandshake, helloDoneBytes)
	}
	c.flushHandshake()

	var pub crypto.PublicKey // public key for client auth, if any

	if err := c.simulatePacketLoss(nil); err != nil {
		return err
	}
	msg, err := c.readHandshake()
	if err != nil {
		return err
	}

	// If we requested a client certificate, then the client must send a
	// certificate message, even if it's empty.
	if config.ClientAuth >= RequestClientCert {
		certMsg, ok := msg.(*certificateMsg)
		if !ok {
			c.sendAlert(alertUnexpectedMessage)
			return unexpectedMessageError(certMsg, msg)
		}
		hs.writeClientHash(certMsg.marshal())

		if len(certMsg.certificates) == 0 {
			// The client didn't actually send a certificate
			switch config.ClientAuth {
			case RequireAnyClientCert, RequireAndVerifyClientCert:
				c.sendAlert(alertBadCertificate)
				return errors.New("tls: client didn't provide a certificate")
			}
		}

		var certificates [][]byte
		for _, cert := range certMsg.certificates {
			certificates = append(certificates, cert.data)
		}

		pub, err = hs.processCertsFromClient(certificates)
		if err != nil {
			return err
		}

		msg, err = c.readHandshake()
		if err != nil {
			return err
		}
	}

	// Get client key exchange
	ckx, ok := msg.(*clientKeyExchangeMsg)
	if !ok {
		c.sendAlert(alertUnexpectedMessage)
		return unexpectedMessageError(ckx, msg)
	}
	hs.writeClientHash(ckx.marshal())

	preMasterSecret, err := keyAgreement.processClientKeyExchange(config, hs.cert, ckx, c.vers)
	if err != nil {
		c.sendAlert(alertHandshakeFailure)
		return err
	}
	if c.extendedMasterSecret {
		hs.masterSecret = extendedMasterFromPreMasterSecret(c.vers, hs.suite, preMasterSecret, hs.finishedHash)
	} else {
		if c.config.Bugs.RequireExtendedMasterSecret {
			return errors.New("tls: extended master secret required but not supported by peer")
		}
		hs.masterSecret = masterFromPreMasterSecret(c.vers, hs.suite, preMasterSecret, hs.clientHello.random, hs.hello.random)
	}

	// If we received a client cert in response to our certificate request message,
	// the client will send us a certificateVerifyMsg immediately after the
	// clientKeyExchangeMsg.  This message is a digest of all preceding
	// handshake-layer messages that is signed using the private key corresponding
	// to the client's certificate. This allows us to verify that the client is in
	// possession of the private key of the certificate.
	if len(c.peerCertificates) > 0 {
		msg, err = c.readHandshake()
		if err != nil {
			return err
		}
		certVerify, ok := msg.(*certificateVerifyMsg)
		if !ok {
			c.sendAlert(alertUnexpectedMessage)
			return unexpectedMessageError(certVerify, msg)
		}

		// Determine the signature type.
		var sigAlg signatureAlgorithm
		if certVerify.hasSignatureAlgorithm {
			sigAlg = certVerify.signatureAlgorithm
			c.peerSignatureAlgorithm = sigAlg
		}

		if err := verifyMessage(c.vers, pub, c.config, sigAlg, hs.finishedHash.buffer, certVerify.signature); err != nil {
			c.sendAlert(alertBadCertificate)
			return errors.New("could not validate signature of connection nonces: " + err.Error())
		}

		hs.writeClientHash(certVerify.marshal())
	}

	hs.finishedHash.discardHandshakeBuffer()

	return nil
}

func (hs *serverHandshakeState) establishKeys() error {
	c := hs.c

	clientMAC, serverMAC, clientKey, serverKey, clientIV, serverIV :=
		keysFromMasterSecret(c.vers, hs.suite, hs.masterSecret, hs.clientHello.random, hs.hello.random, hs.suite.macLen, hs.suite.keyLen, hs.suite.ivLen(c.vers))

	var clientCipher, serverCipher interface{}
	var clientHash, serverHash macFunction

	if hs.suite.aead == nil {
		clientCipher = hs.suite.cipher(clientKey, clientIV, true /* for reading */)
		clientHash = hs.suite.mac(c.vers, clientMAC)
		serverCipher = hs.suite.cipher(serverKey, serverIV, false /* not for reading */)
		serverHash = hs.suite.mac(c.vers, serverMAC)
	} else {
		clientCipher = hs.suite.aead(c.vers, clientKey, clientIV)
		serverCipher = hs.suite.aead(c.vers, serverKey, serverIV)
	}

	c.in.prepareCipherSpec(c.wireVersion, clientCipher, clientHash)
	c.out.prepareCipherSpec(c.wireVersion, serverCipher, serverHash)

	return nil
}

func (hs *serverHandshakeState) readFinished(out []byte, isResume bool) error {
	c := hs.c

	c.readRecord(recordTypeChangeCipherSpec)
	if err := c.in.error(); err != nil {
		return err
	}

	if hs.hello.extensions.nextProtoNeg {
		msg, err := c.readHandshake()
		if err != nil {
			return err
		}
		nextProto, ok := msg.(*nextProtoMsg)
		if !ok {
			c.sendAlert(alertUnexpectedMessage)
			return unexpectedMessageError(nextProto, msg)
		}
		hs.writeClientHash(nextProto.marshal())
		c.clientProtocol = nextProto.proto
	}

	if hs.hello.extensions.channelIDRequested {
		msg, err := c.readHandshake()
		if err != nil {
			return err
		}
		channelIDMsg, ok := msg.(*channelIDMsg)
		if !ok {
			c.sendAlert(alertUnexpectedMessage)
			return unexpectedMessageError(channelIDMsg, msg)
		}
		var resumeHash []byte
		if isResume {
			resumeHash = hs.sessionState.handshakeHash
		}
		channelID, err := verifyChannelIDMessage(channelIDMsg, hs.finishedHash.hashForChannelID(resumeHash))
		if err != nil {
			return err
		}
		c.channelID = channelID

		hs.writeClientHash(channelIDMsg.marshal())
	}

	msg, err := c.readHandshake()
	if err != nil {
		return err
	}
	clientFinished, ok := msg.(*finishedMsg)
	if !ok {
		c.sendAlert(alertUnexpectedMessage)
		return unexpectedMessageError(clientFinished, msg)
	}

	verify := hs.finishedHash.clientSum(hs.masterSecret)
	if len(verify) != len(clientFinished.verifyData) ||
		subtle.ConstantTimeCompare(verify, clientFinished.verifyData) != 1 {
		c.sendAlert(alertHandshakeFailure)
		return errors.New("tls: client's Finished message is incorrect")
	}
	c.clientVerify = append(c.clientVerify[:0], clientFinished.verifyData...)
	copy(out, clientFinished.verifyData)

	hs.writeClientHash(clientFinished.marshal())
	return nil
}

func (hs *serverHandshakeState) sendSessionTicket() error {
	c := hs.c
	state := sessionState{
		vers:          c.vers,
		cipherSuite:   hs.suite.id,
		secret:        hs.masterSecret,
		certificates:  hs.certsFromClient,
		handshakeHash: hs.finishedHash.Sum(),
	}

	if !hs.hello.extensions.ticketSupported || hs.c.config.Bugs.SkipNewSessionTicket {
		if c.config.ServerSessionCache != nil && len(hs.hello.sessionID) != 0 {
			c.config.ServerSessionCache.Put(string(hs.hello.sessionID), &state)
		}
		return nil
	}

	m := new(newSessionTicketMsg)
	m.vers = c.wireVersion
	m.isDTLS = c.isDTLS
	if c.config.Bugs.SendTicketLifetime != 0 {
		m.ticketLifetime = uint32(c.config.Bugs.SendTicketLifetime / time.Second)
	}

	if !c.config.Bugs.SendEmptySessionTicket {
		var err error
		m.ticket, err = c.encryptTicket(&state)
		if err != nil {
			return err
		}
	}

	hs.writeServerHash(m.marshal())
	if c.config.Bugs.PartialNewSessionTicketWithServerHelloDone {
		// The first byte was already sent.
		c.writeRecord(recordTypeHandshake, m.marshal()[1:])
	} else {
		c.writeRecord(recordTypeHandshake, m.marshal())
	}

	return nil
}

func (hs *serverHandshakeState) sendFinished(out []byte, isResume bool) error {
	c := hs.c

	finished := new(finishedMsg)
	finished.verifyData = hs.finishedHash.serverSum(hs.masterSecret)
	copy(out, finished.verifyData)
	if c.config.Bugs.BadFinished {
		finished.verifyData[0]++
	}
	c.serverVerify = append(c.serverVerify[:0], finished.verifyData...)
	hs.finishedBytes = finished.marshal()
	hs.writeServerHash(hs.finishedBytes)
	postCCSBytes := hs.finishedBytes
	if c.config.Bugs.PartialFinishedWithServerHelloDone {
		// The first byte has already been sent.
		postCCSBytes = postCCSBytes[1:]
	}

	if c.config.Bugs.FragmentAcrossChangeCipherSpec {
		c.writeRecord(recordTypeHandshake, postCCSBytes[:5])
		postCCSBytes = postCCSBytes[5:]
	} else if c.config.Bugs.SendUnencryptedFinished {
		c.writeRecord(recordTypeHandshake, postCCSBytes)
		postCCSBytes = nil
	}

	if !c.config.Bugs.SkipChangeCipherSpec {
		ccs := []byte{1}
		if c.config.Bugs.BadChangeCipherSpec != nil {
			ccs = c.config.Bugs.BadChangeCipherSpec
		}
		c.writeRecord(recordTypeChangeCipherSpec, ccs)
	}

	if c.config.Bugs.AppDataAfterChangeCipherSpec != nil {
		c.writeRecord(recordTypeApplicationData, c.config.Bugs.AppDataAfterChangeCipherSpec)
	}
	if c.config.Bugs.AlertAfterChangeCipherSpec != 0 {
		c.sendAlert(c.config.Bugs.AlertAfterChangeCipherSpec)
		return errors.New("tls: simulating post-CCS alert")
	}

	if !c.config.Bugs.SkipFinished && len(postCCSBytes) > 0 {
		c.writeRecord(recordTypeHandshake, postCCSBytes)
		if c.config.Bugs.SendExtraFinished {
			c.writeRecord(recordTypeHandshake, finished.marshal())
		}
	}

	if isResume || (!c.config.Bugs.PackHelloRequestWithFinished && !c.config.Bugs.PackAppDataWithHandshake) {
		// Defer flushing until Renegotiate() or Write().
		c.flushHandshake()
	}

	c.cipherSuite = hs.suite

	return nil
}

// processCertsFromClient takes a chain of client certificates either from a
// Certificates message or from a sessionState and verifies them. It returns
// the public key of the leaf certificate.
func (hs *serverHandshakeState) processCertsFromClient(certificates [][]byte) (crypto.PublicKey, error) {
	c := hs.c

	hs.certsFromClient = certificates
	certs := make([]*x509.Certificate, len(certificates))
	var err error
	for i, asn1Data := range certificates {
		if certs[i], err = x509.ParseCertificate(asn1Data); err != nil {
			c.sendAlert(alertBadCertificate)
			return nil, errors.New("tls: failed to parse client certificate: " + err.Error())
		}
	}

	if c.config.ClientAuth >= VerifyClientCertIfGiven && len(certs) > 0 {
		opts := x509.VerifyOptions{
			Roots:         c.config.ClientCAs,
			CurrentTime:   c.config.time(),
			Intermediates: x509.NewCertPool(),
			KeyUsages:     []x509.ExtKeyUsage{x509.ExtKeyUsageClientAuth},
		}

		for _, cert := range certs[1:] {
			opts.Intermediates.AddCert(cert)
		}

		chains, err := certs[0].Verify(opts)
		if err != nil {
			c.sendAlert(alertBadCertificate)
			return nil, errors.New("tls: failed to verify client's certificate: " + err.Error())
		}

		ok := false
		for _, ku := range certs[0].ExtKeyUsage {
			if ku == x509.ExtKeyUsageClientAuth {
				ok = true
				break
			}
		}
		if !ok {
			c.sendAlert(alertHandshakeFailure)
			return nil, errors.New("tls: client's certificate's extended key usage doesn't permit it to be used for client authentication")
		}

		c.verifiedChains = chains
	}

	if len(certs) > 0 {
		pub := certs[0].PublicKey
		switch pub.(type) {
		case *ecdsa.PublicKey, *rsa.PublicKey, ed25519.PublicKey:
			break
		default:
			c.sendAlert(alertUnsupportedCertificate)
			return nil, fmt.Errorf("tls: client's certificate contains an unsupported public key of type %T", pub)
		}
		c.peerCertificates = certs
		return pub, nil
	}

	return nil, nil
}

func verifyChannelIDMessage(channelIDMsg *channelIDMsg, channelIDHash []byte) (*ecdsa.PublicKey, error) {
	x := new(big.Int).SetBytes(channelIDMsg.channelID[0:32])
	y := new(big.Int).SetBytes(channelIDMsg.channelID[32:64])
	r := new(big.Int).SetBytes(channelIDMsg.channelID[64:96])
	s := new(big.Int).SetBytes(channelIDMsg.channelID[96:128])
	if !elliptic.P256().IsOnCurve(x, y) {
		return nil, errors.New("tls: invalid channel ID public key")
	}
	channelID := &ecdsa.PublicKey{Curve: elliptic.P256(), X: x, Y: y}
	if !ecdsa.Verify(channelID, channelIDHash, r, s) {
		return nil, errors.New("tls: invalid channel ID signature")
	}
	return channelID, nil
}

func (hs *serverHandshakeState) writeServerHash(msg []byte) {
	// writeServerHash is called before writeRecord.
	hs.finishedHash.WriteHandshake(msg, hs.c.sendHandshakeSeq)
}

func (hs *serverHandshakeState) writeClientHash(msg []byte) {
	// writeClientHash is called after readHandshake.
	hs.finishedHash.WriteHandshake(msg, hs.c.recvHandshakeSeq-1)
}

// tryCipherSuite returns a cipherSuite with the given id if that cipher suite
// is acceptable to use.
func (c *Conn) tryCipherSuite(id uint16, supportedCipherSuites []uint16, version uint16, ellipticOk, ecdsaOk bool) *cipherSuite {
	for _, supported := range supportedCipherSuites {
		if id == supported {
			var candidate *cipherSuite

			for _, s := range cipherSuites {
				if s.id == id {
					candidate = s
					break
				}
			}
			if candidate == nil {
				continue
			}

			// Don't select a ciphersuite which we can't
			// support for this client.
			if version >= VersionTLS13 || candidate.flags&suiteTLS13 != 0 {
				if version < VersionTLS13 || candidate.flags&suiteTLS13 == 0 {
					continue
				}
				return candidate
			}
			if (candidate.flags&suiteECDHE != 0) && !ellipticOk {
				continue
			}
			if (candidate.flags&suiteECDSA != 0) != ecdsaOk {
				continue
			}
			if version < VersionTLS12 && candidate.flags&suiteTLS12 != 0 {
				continue
			}
			return candidate
		}
	}

	return nil
}

func isTLS12Cipher(id uint16) bool {
	for _, cipher := range cipherSuites {
		if cipher.id != id {
			continue
		}
		return cipher.flags&suiteTLS12 != 0
	}
	// Unknown cipher.
	return false
}

func isGREASEValue(val uint16) bool {
	return val&0x0f0f == 0x0a0a && val&0xff == val>>8
}

func verifyPSKBinder(version uint16, clientHello *clientHelloMsg, sessionState *sessionState, binderToVerify, firstClientHello, helloRetryRequest []byte) error {
	binderLen := 2
	for _, binder := range clientHello.pskBinders {
		binderLen += 1 + len(binder)
	}

	truncatedHello := clientHello.marshal()
	truncatedHello = truncatedHello[:len(truncatedHello)-binderLen]
	pskCipherSuite := cipherSuiteFromID(sessionState.cipherSuite)
	if pskCipherSuite == nil {
		return errors.New("tls: Unknown cipher suite for PSK in session")
	}

	binder := computePSKBinder(sessionState.secret, version, resumptionPSKBinderLabel, pskCipherSuite, firstClientHello, helloRetryRequest, truncatedHello)
	if !bytes.Equal(binder, binderToVerify) {
		return errors.New("tls: PSK binder does not verify")
	}

	return nil
}

// checkClientHellosEqual checks whether a and b are equal ClientHello
// messages. If isDTLS is true, the ClientHellos are parsed as DTLS and any
// differences in the cookie field are ignored. Extensions listed in
// ignoreExtensions may change or be removed between the two ClientHellos.
func checkClientHellosEqual(a, b []byte, isDTLS bool, ignoreExtensions []uint16) error {
	ignoreExtensionsSet := make(map[uint16]struct{})
	for _, ext := range ignoreExtensions {
		ignoreExtensionsSet[ext] = struct{}{}
	}

	// Skip the handshake message header.
	aReader := byteReader(a[4:])
	bReader := byteReader(b[4:])

	var aVers, bVers uint16
	var aRandom, bRandom []byte
	var aSessionID, bSessionID []byte
	if !aReader.readU16(&aVers) ||
		!bReader.readU16(&bVers) ||
		!aReader.readBytes(&aRandom, 32) ||
		!bReader.readBytes(&bRandom, 32) ||
		!aReader.readU8LengthPrefixedBytes(&aSessionID) ||
		!bReader.readU8LengthPrefixedBytes(&bSessionID) {
		return errors.New("tls: could not parse ClientHello")
	}

	if aVers != bVers {
		return errors.New("tls: second ClientHello version did not match")
	}
	if !bytes.Equal(aRandom, bRandom) {
		return errors.New("tls: second ClientHello random did not match")
	}
	if !bytes.Equal(aSessionID, bSessionID) {
		return errors.New("tls: second ClientHello session ID did not match")
	}

	if isDTLS {
		// DTLS 1.2 checks two ClientHellos match after a HelloVerifyRequest,
		// where we expect the cookies to change. DTLS 1.3 forbids the legacy
		// cookie altogether. If we implement DTLS 1.3, we'll need to ensure
		// that parsing logic above this function rejects this cookie.
		var aCookie, bCookie []byte
		if !aReader.readU8LengthPrefixedBytes(&aCookie) ||
			!bReader.readU8LengthPrefixedBytes(&bCookie) {
			return errors.New("tls: could not parse ClientHello")
		}
	}

	var aCipherSuites, bCipherSuites, aCompressionMethods, bCompressionMethods []byte
	if !aReader.readU16LengthPrefixedBytes(&aCipherSuites) ||
		!bReader.readU16LengthPrefixedBytes(&bCipherSuites) ||
		!aReader.readU8LengthPrefixedBytes(&aCompressionMethods) ||
		!bReader.readU8LengthPrefixedBytes(&bCompressionMethods) {
		return errors.New("tls: could not parse ClientHello")
	}
	if !bytes.Equal(aCipherSuites, bCipherSuites) {
		return errors.New("tls: second ClientHello cipher suites did not match")
	}
	if !bytes.Equal(aCompressionMethods, bCompressionMethods) {
		return errors.New("tls: second ClientHello compression methods did not match")
	}

	if len(aReader) == 0 && len(bReader) == 0 {
		// Both ClientHellos omit the extensions block.
		return nil
	}

	var aExtensions, bExtensions byteReader
	if !aReader.readU16LengthPrefixed(&aExtensions) ||
		!bReader.readU16LengthPrefixed(&bExtensions) ||
		len(aReader) != 0 ||
		len(bReader) != 0 {
		return errors.New("tls: could not parse ClientHello")
	}

	for len(aExtensions) != 0 {
		var aID uint16
		var aBody []byte
		if !aExtensions.readU16(&aID) ||
			!aExtensions.readU16LengthPrefixedBytes(&aBody) {
			return errors.New("tls: could not parse ClientHello")
		}
		if _, ok := ignoreExtensionsSet[aID]; ok {
			continue
		}

		for {
			if len(bExtensions) == 0 {
				return fmt.Errorf("tls: second ClientHello missing extension %d", aID)
			}
			var bID uint16
			var bBody []byte
			if !bExtensions.readU16(&bID) ||
				!bExtensions.readU16LengthPrefixedBytes(&bBody) {
				return errors.New("tls: could not parse ClientHello")
			}
			if _, ok := ignoreExtensionsSet[bID]; ok {
				continue
			}
			if aID != bID {
				return fmt.Errorf("tls: unexpected extension %d in second ClientHello (wanted %d)", bID, aID)
			}
			if !bytes.Equal(aBody, bBody) {
				return fmt.Errorf("tls: extension %d in second ClientHello unexpectedly changed", aID)
			}
			break
		}
	}

	// Any remaining extensions in the second ClientHello must be in the
	// ignored set.
	for len(bExtensions) != 0 {
		var id uint16
		var body []byte
		if !bExtensions.readU16(&id) ||
			!bExtensions.readU16LengthPrefixedBytes(&body) {
			return errors.New("tls: could not parse ClientHello")
		}
		if _, ok := ignoreExtensionsSet[id]; !ok {
			return fmt.Errorf("tls: unexpected extension %d in second ClientHello", id)
		}
	}

	return nil
}
