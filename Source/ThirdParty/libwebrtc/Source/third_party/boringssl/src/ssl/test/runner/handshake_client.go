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
	"net"
	"time"

	"boringssl.googlesource.com/boringssl/ssl/test/runner/hpke"
)

type clientHandshakeState struct {
	c                 *Conn
	serverHello       *serverHelloMsg
	hello             *clientHelloMsg
	innerHello        *clientHelloMsg
	echHPKEContext    *hpke.Context
	suite             *cipherSuite
	finishedHash      finishedHash
	innerFinishedHash finishedHash
	keyShares         map[CurveID]ecdhCurve
	masterSecret      []byte
	session           *ClientSessionState
	finishedBytes     []byte
	peerPublicKey     crypto.PublicKey
}

func mapClientHelloVersion(vers uint16, isDTLS bool) uint16 {
	if !isDTLS {
		return vers
	}

	switch vers {
	case VersionTLS12:
		return VersionDTLS12
	case VersionTLS10:
		return VersionDTLS10
	}

	panic("Unknown ClientHello version.")
}

// replaceClientHello returns a new clientHelloMsg which serializes to |in|, but
// with key shares copied from |hello|. This allows sending an exact
// externally-specified ClientHello in tests. However, we use |hello|'s key
// shares. This ensures we have the private keys to complete the handshake. Note
// this function does not update internal handshake state, so the test must be
// configured compatibly with |in|.
func replaceClientHello(hello *clientHelloMsg, in []byte) (*clientHelloMsg, error) {
	copied := append([]byte{}, in...)
	newHello := new(clientHelloMsg)
	if !newHello.unmarshal(copied) {
		return nil, errors.New("tls: invalid ClientHello")
	}

	// Replace |newHellos|'s key shares with those of |hello|. For simplicity,
	// we require their lengths match, which is satisfied by matching the
	// DefaultCurves setting to the selection in the replacement ClientHello.
	bb := newByteBuilder()
	hello.marshalKeyShares(bb)
	keyShares := bb.finish()
	if len(keyShares) != len(newHello.keySharesRaw) {
		return nil, errors.New("tls: ClientHello key share length is inconsistent with DefaultCurves setting")
	}
	// |newHello.keySharesRaw| aliases |copied|.
	copy(newHello.keySharesRaw, keyShares)
	newHello.keyShares = hello.keyShares

	return newHello, nil
}

func (c *Conn) clientHandshake() error {
	if c.config == nil {
		c.config = defaultConfig()
	}

	if len(c.config.ServerName) == 0 && !c.config.InsecureSkipVerify {
		return errors.New("tls: either ServerName or InsecureSkipVerify must be specified in the tls.Config")
	}

	c.sendHandshakeSeq = 0
	c.recvHandshakeSeq = 0

	hs := &clientHandshakeState{
		c:         c,
		keyShares: make(map[CurveID]ecdhCurve),
	}

	// Pick a session to resume.
	var session *ClientSessionState
	var cacheKey string
	sessionCache := c.config.ClientSessionCache
	if sessionCache != nil {
		// Try to resume a previously negotiated TLS session, if
		// available.
		cacheKey = clientSessionCacheKey(c.conn.RemoteAddr(), c.config)
		// TODO(nharper): Support storing more than one session
		// ticket for TLS 1.3.
		candidateSession, ok := sessionCache.Get(cacheKey)
		if ok {
			ticketOk := !c.config.SessionTicketsDisabled || candidateSession.sessionTicket == nil

			// Check that the ciphersuite/version used for the
			// previous session are still valid.
			cipherSuiteOk := false
			if candidateSession.vers <= VersionTLS12 {
				for _, id := range c.config.cipherSuites() {
					if id == candidateSession.cipherSuite.id {
						cipherSuiteOk = true
						break
					}
				}
			} else {
				// TLS 1.3 allows the cipher to change on
				// resumption.
				cipherSuiteOk = true
			}

			_, versOk := c.config.isSupportedVersion(candidateSession.wireVersion, c.isDTLS)
			if ticketOk && versOk && cipherSuiteOk {
				session = candidateSession
				hs.session = session
			}
		}
	}

	// Set up ECH parameters.
	var err error
	var earlyHello *clientHelloMsg
	if c.config.ClientECHConfig != nil {
		if c.config.ClientECHConfig.KEM != hpke.X25519WithHKDFSHA256 {
			return errors.New("tls: unsupported KEM type in ECHConfig")
		}

		echCipherSuite, ok := chooseECHCipherSuite(c.config.ClientECHConfig, c.config)
		if !ok {
			return errors.New("tls: did not find compatible cipher suite in ECHConfig")
		}

		info := []byte("tls ech\x00")
		info = append(info, MarshalECHConfig(c.config.ClientECHConfig)...)

		var echEnc []byte
		hs.echHPKEContext, echEnc, err = hpke.SetupBaseSenderX25519(echCipherSuite.KDF, echCipherSuite.AEAD, c.config.ClientECHConfig.PublicKey, info, nil)
		if err != nil {
			return errors.New("tls: ech: failed to set up client's HPKE sender context")
		}

		hs.innerHello, err = hs.createClientHello(nil, nil)
		if err != nil {
			return err
		}
		hs.hello, err = hs.createClientHello(hs.innerHello, echEnc)
		if err != nil {
			return err
		}
		earlyHello = hs.innerHello
	} else {
		hs.hello, err = hs.createClientHello(nil, nil)
		if err != nil {
			return err
		}
		earlyHello = hs.hello
	}

	if len(earlyHello.pskIdentities) == 0 || c.config.Bugs.SendEarlyData == nil {
		earlyHello = nil
	}

	if c.config.Bugs.SendV2ClientHello {
		hs.hello.isV2ClientHello = true

		// The V2ClientHello "challenge" field is variable-length and is
		// left-padded or truncated to become the SSL3/TLS random.
		challengeLength := c.config.Bugs.V2ClientHelloChallengeLength
		if challengeLength == 0 {
			challengeLength = len(hs.hello.random)
		}
		if challengeLength <= len(hs.hello.random) {
			skip := len(hs.hello.random) - challengeLength
			for i := 0; i < skip; i++ {
				hs.hello.random[i] = 0
			}
			hs.hello.v2Challenge = hs.hello.random[skip:]
		} else {
			hs.hello.v2Challenge = make([]byte, challengeLength)
			copy(hs.hello.v2Challenge, hs.hello.random)
			if _, err := io.ReadFull(c.config.rand(), hs.hello.v2Challenge[len(hs.hello.random):]); err != nil {
				c.sendAlert(alertInternalError)
				return fmt.Errorf("tls: short read from Rand: %s", err)
			}
		}

		c.writeV2Record(hs.hello.marshal())
	} else {
		helloBytes := hs.hello.marshal()
		var appendToHello byte
		if c.config.Bugs.PartialClientFinishedWithClientHello {
			appendToHello = typeFinished
		} else if c.config.Bugs.PartialEndOfEarlyDataWithClientHello {
			appendToHello = typeEndOfEarlyData
		} else if c.config.Bugs.PartialSecondClientHelloAfterFirst {
			appendToHello = typeClientHello
		} else if c.config.Bugs.PartialClientKeyExchangeWithClientHello {
			appendToHello = typeClientKeyExchange
		}
		if appendToHello != 0 {
			c.writeRecord(recordTypeHandshake, append(helloBytes[:len(helloBytes):len(helloBytes)], appendToHello))
		} else {
			c.writeRecord(recordTypeHandshake, helloBytes)
		}
	}
	c.flushHandshake()

	if err := c.simulatePacketLoss(nil); err != nil {
		return err
	}
	if c.config.Bugs.SendEarlyAlert {
		c.sendAlert(alertHandshakeFailure)
	}
	if c.config.Bugs.SendFakeEarlyDataLength > 0 {
		c.sendFakeEarlyData(c.config.Bugs.SendFakeEarlyDataLength)
	}

	// Derive early write keys and set Conn state to allow early writes.
	if earlyHello != nil {
		finishedHash := newFinishedHash(session.wireVersion, c.isDTLS, session.cipherSuite)
		finishedHash.addEntropy(session.secret)
		finishedHash.Write(earlyHello.marshal())

		if !c.config.Bugs.SkipChangeCipherSpec {
			c.wireVersion = session.wireVersion
			c.vers = VersionTLS13
			c.writeRecord(recordTypeChangeCipherSpec, []byte{1})
			c.wireVersion = 0
			c.vers = 0
		}

		earlyTrafficSecret := finishedHash.deriveSecret(earlyTrafficLabel)
		c.earlyExporterSecret = finishedHash.deriveSecret(earlyExporterLabel)

		c.useOutTrafficSecret(encryptionEarlyData, session.wireVersion, session.cipherSuite, earlyTrafficSecret)
		for _, earlyData := range c.config.Bugs.SendEarlyData {
			if _, err := c.writeRecord(recordTypeApplicationData, earlyData); err != nil {
				return err
			}
		}
	}

	msg, err := c.readHandshake()
	if err != nil {
		return err
	}

	if c.isDTLS {
		helloVerifyRequest, ok := msg.(*helloVerifyRequestMsg)
		if ok {
			if helloVerifyRequest.vers != VersionDTLS10 {
				// Per RFC 6347, the version field in
				// HelloVerifyRequest SHOULD be always DTLS
				// 1.0. Enforce this for testing purposes.
				return errors.New("dtls: bad HelloVerifyRequest version")
			}

			hs.hello.raw = nil
			hs.hello.cookie = helloVerifyRequest.cookie
			c.writeRecord(recordTypeHandshake, hs.hello.marshal())
			c.flushHandshake()

			if err := c.simulatePacketLoss(nil); err != nil {
				return err
			}
			msg, err = c.readHandshake()
			if err != nil {
				return err
			}
		}
	}

	// The first message is either ServerHello or HelloRetryRequest, either of
	// which determines the version and cipher suite.
	var serverWireVersion, suiteID uint16
	switch m := msg.(type) {
	case *helloRetryRequestMsg:
		serverWireVersion = m.vers
		suiteID = m.cipherSuite
	case *serverHelloMsg:
		serverWireVersion = m.vers
		suiteID = m.cipherSuite
	default:
		c.sendAlert(alertUnexpectedMessage)
		return fmt.Errorf("tls: received unexpected message of type %T when waiting for HelloRetryRequest or ServerHello", msg)
	}

	serverVersion, ok := c.config.isSupportedVersion(serverWireVersion, c.isDTLS)
	if !ok {
		c.sendAlert(alertProtocolVersion)
		return fmt.Errorf("tls: server selected unsupported protocol version %x", c.vers)
	}
	c.wireVersion = serverWireVersion
	c.vers = serverVersion
	c.haveVers = true

	// We only implement enough of SSL 3.0 to test that the server doesn't:
	// we can send a ClientHello and attempt to read a ServerHello. The server
	// should respond with a protocol_version alert and not get this far.
	if c.vers == VersionSSL30 {
		return errors.New("tls: server selected SSL 3.0")
	}

	cipherSuites := hs.hello.cipherSuites
	if hs.innerHello != nil && c.config.Bugs.MinimalClientHelloOuter {
		// hs.hello has a placeholder list of ciphers if testing with
		// MinimalClientHelloOuter, so we use hs.innerHello instead. (We do not
		// attempt to support actual different cipher suite preferences between
		// the two.)
		cipherSuites = hs.innerHello.cipherSuites
	}
	hs.suite = mutualCipherSuite(cipherSuites, suiteID)
	if hs.suite == nil {
		c.sendAlert(alertHandshakeFailure)
		return fmt.Errorf("tls: server selected an unsupported cipher suite")
	}

	hs.finishedHash = newFinishedHash(c.wireVersion, c.isDTLS, hs.suite)
	hs.finishedHash.WriteHandshake(hs.hello.marshal(), hs.c.sendHandshakeSeq-1)
	if hs.innerHello != nil {
		hs.innerFinishedHash = newFinishedHash(c.wireVersion, c.isDTLS, hs.suite)
		hs.innerFinishedHash.WriteHandshake(hs.innerHello.marshal(), hs.c.sendHandshakeSeq-1)
	}

	if c.vers >= VersionTLS13 {
		if err := hs.doTLS13Handshake(msg); err != nil {
			return err
		}
	} else {
		hs.serverHello, ok = msg.(*serverHelloMsg)
		if !ok {
			c.sendAlert(alertUnexpectedMessage)
			return unexpectedMessageError(hs.serverHello, msg)
		}

		hs.writeServerHash(hs.serverHello.marshal())
		if c.config.Bugs.EarlyChangeCipherSpec > 0 {
			hs.establishKeys()
			c.writeRecord(recordTypeChangeCipherSpec, []byte{1})
		}

		if hs.serverHello.compressionMethod != compressionNone {
			c.sendAlert(alertUnexpectedMessage)
			return errors.New("tls: server selected unsupported compression format")
		}

		err = hs.processServerExtensions(&hs.serverHello.extensions)
		if err != nil {
			return err
		}

		isResume, err := hs.processServerHello()
		if err != nil {
			return err
		}

		if isResume {
			if c.config.Bugs.EarlyChangeCipherSpec == 0 {
				if err := hs.establishKeys(); err != nil {
					return err
				}
			}
			if err := hs.readSessionTicket(); err != nil {
				return err
			}
			if err := hs.readFinished(c.firstFinished[:]); err != nil {
				return err
			}
			if err := hs.sendFinished(nil, isResume); err != nil {
				return err
			}
		} else {
			if err := hs.doFullHandshake(); err != nil {
				return err
			}
			if err := hs.establishKeys(); err != nil {
				return err
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
			if err := hs.readSessionTicket(); err != nil {
				return err
			}
			if err := hs.readFinished(nil); err != nil {
				return err
			}
		}

		if sessionCache != nil && hs.session != nil && session != hs.session {
			if c.config.Bugs.RequireSessionTickets && len(hs.session.sessionTicket) == 0 {
				return errors.New("tls: new session used session IDs instead of tickets")
			}
			if c.config.Bugs.RequireSessionIDs && len(hs.session.sessionID) == 0 {
				return errors.New("tls: new session used session tickets instead of IDs")
			}
			sessionCache.Put(cacheKey, hs.session)
		}

		c.didResume = isResume
		c.exporterSecret = hs.masterSecret
	}

	c.handshakeComplete = true
	c.cipherSuite = hs.suite
	copy(c.clientRandom[:], hs.hello.random)
	copy(c.serverRandom[:], hs.serverHello.random)

	return nil
}

func chooseECHCipherSuite(echConfig *ECHConfig, config *Config) (HPKECipherSuite, bool) {
	if echConfig.KEM != hpke.X25519WithHKDFSHA256 {
		return HPKECipherSuite{}, false
	}

	for _, wantSuite := range config.echCipherSuitePreferences() {
		if config.Bugs.IgnoreECHConfigCipherPreferences {
			return wantSuite, true
		}
		for _, cipherSuite := range echConfig.CipherSuites {
			if cipherSuite == wantSuite {
				return cipherSuite, true
			}
		}
	}
	return HPKECipherSuite{}, false
}

// createClientHello creates a new ClientHello message. If |innerHello| is not
// nil, this is a ClientHelloOuter that should contain an encrypted |innerHello|
// with |echEnc| as the encapsulated public key. Otherwise, the ClientHello
// should reflect the connection's true preferences.
func (hs *clientHandshakeState) createClientHello(innerHello *clientHelloMsg, echEnc []byte) (*clientHelloMsg, error) {
	c := hs.c
	nextProtosLength := 0
	for _, proto := range c.config.NextProtos {
		if l := len(proto); l > 255 {
			return nil, errors.New("tls: invalid NextProtos value")
		} else {
			nextProtosLength += 1 + l
		}
	}
	if nextProtosLength > 0xffff {
		return nil, errors.New("tls: NextProtos values too large")
	}

	quicTransportParams := c.config.QUICTransportParams
	quicTransportParamsLegacy := c.config.QUICTransportParams
	if !c.config.QUICTransportParamsUseLegacyCodepoint.IncludeStandard() {
		quicTransportParams = nil
	}
	if !c.config.QUICTransportParamsUseLegacyCodepoint.IncludeLegacy() {
		quicTransportParamsLegacy = nil
	}

	isInner := innerHello == nil && hs.echHPKEContext != nil

	minVersion := c.config.minVersion(c.isDTLS)
	maxVersion := c.config.maxVersion(c.isDTLS)
	// The ClientHelloInner may not offer TLS 1.2 or below.
	requireTLS13 := isInner && !c.config.Bugs.AllowTLS12InClientHelloInner
	if requireTLS13 && minVersion < VersionTLS13 {
		minVersion = VersionTLS13
		if minVersion > maxVersion {
			return nil, errors.New("tls: ECH requires TLS 1.3")
		}
	}

	hello := &clientHelloMsg{
		isDTLS:                    c.isDTLS,
		compressionMethods:        []uint8{compressionNone},
		random:                    make([]byte, 32),
		ocspStapling:              !c.config.Bugs.NoOCSPStapling,
		sctListSupported:          !c.config.Bugs.NoSignedCertificateTimestamps,
		supportedCurves:           c.config.curvePreferences(),
		supportedPoints:           []uint8{pointFormatUncompressed},
		nextProtoNeg:              len(c.config.NextProtos) > 0,
		secureRenegotiation:       []byte{},
		alpnProtocols:             c.config.NextProtos,
		quicTransportParams:       quicTransportParams,
		quicTransportParamsLegacy: quicTransportParamsLegacy,
		duplicateExtension:        c.config.Bugs.DuplicateExtension,
		channelIDSupported:        c.config.ChannelID != nil,
		tokenBindingParams:        c.config.TokenBindingParams,
		tokenBindingVersion:       c.config.TokenBindingVersion,
		extendedMasterSecret:      maxVersion >= VersionTLS10,
		srtpProtectionProfiles:    c.config.SRTPProtectionProfiles,
		srtpMasterKeyIdentifier:   c.config.Bugs.SRTPMasterKeyIdentifer,
		customExtension:           c.config.Bugs.CustomExtension,
		omitExtensions:            c.config.Bugs.OmitExtensions,
		emptyExtensions:           c.config.Bugs.EmptyExtensions,
		delegatedCredentials:      !c.config.Bugs.DisableDelegatedCredentials,
	}

	// Translate the bugs that modify ClientHello extension order into a
	// list of prefix extensions. The marshal function will try these
	// extensions before any others, followed by any remaining extensions in
	// the default order.
	if innerHello != nil && c.config.Bugs.FirstExtensionInClientHelloOuter != 0 {
		hello.prefixExtensions = append(hello.prefixExtensions, c.config.Bugs.FirstExtensionInClientHelloOuter)
	}
	if c.config.Bugs.PSKBinderFirst && !c.config.Bugs.OnlyCorruptSecondPSKBinder {
		hello.prefixExtensions = append(hello.prefixExtensions, extensionPreSharedKey)
	}
	if c.config.Bugs.SwapNPNAndALPN {
		hello.prefixExtensions = append(hello.prefixExtensions, extensionALPN)
		hello.prefixExtensions = append(hello.prefixExtensions, extensionNextProtoNeg)
	}
	if isInner && len(c.config.ECHOuterExtensions) > 0 && !c.config.Bugs.OnlyCompressSecondClientHelloInner {
		applyECHOuterExtensions(hello, c.config.ECHOuterExtensions)
	}

	if maxVersion >= VersionTLS13 {
		hello.vers = mapClientHelloVersion(VersionTLS12, c.isDTLS)
		if !c.config.Bugs.OmitSupportedVersions {
			hello.supportedVersions = c.config.supportedVersions(c.isDTLS, requireTLS13)
		}
		hello.pskKEModes = []byte{pskDHEKEMode}
	} else {
		hello.vers = mapClientHelloVersion(maxVersion, c.isDTLS)
	}

	if c.config.Bugs.SendClientVersion != 0 {
		hello.vers = c.config.Bugs.SendClientVersion
	}

	if len(c.config.Bugs.SendSupportedVersions) > 0 {
		hello.supportedVersions = c.config.Bugs.SendSupportedVersions
	}

	if innerHello != nil {
		hello.serverName = c.config.ClientECHConfig.PublicName
	} else {
		hello.serverName = c.config.ServerName
	}

	disableEMS := c.config.Bugs.NoExtendedMasterSecret
	if c.cipherSuite != nil {
		disableEMS = c.config.Bugs.NoExtendedMasterSecretOnRenegotiation
	}

	if disableEMS {
		hello.extendedMasterSecret = false
	}

	if c.config.Bugs.NoSupportedCurves {
		hello.supportedCurves = nil
	}

	if c.config.Bugs.SendPSKKeyExchangeModes != nil {
		hello.pskKEModes = c.config.Bugs.SendPSKKeyExchangeModes
	}

	if c.config.Bugs.SendCompressionMethods != nil {
		hello.compressionMethods = c.config.Bugs.SendCompressionMethods
	}

	if c.config.Bugs.SendSupportedPointFormats != nil {
		hello.supportedPoints = c.config.Bugs.SendSupportedPointFormats
	}

	if len(c.clientVerify) > 0 && !c.config.Bugs.EmptyRenegotiationInfo {
		if c.config.Bugs.BadRenegotiationInfo {
			hello.secureRenegotiation = append(hello.secureRenegotiation, c.clientVerify...)
			hello.secureRenegotiation[0] ^= 0x80
		} else {
			hello.secureRenegotiation = c.clientVerify
		}
	}

	if c.config.Bugs.DuplicateCompressedCertAlgs {
		hello.compressedCertAlgs = []uint16{1, 1}
	} else if len(c.config.CertCompressionAlgs) > 0 {
		hello.compressedCertAlgs = make([]uint16, 0, len(c.config.CertCompressionAlgs))
		for id := range c.config.CertCompressionAlgs {
			hello.compressedCertAlgs = append(hello.compressedCertAlgs, uint16(id))
		}
	}

	if c.noRenegotiationInfo() {
		hello.secureRenegotiation = nil
	}

	for protocol := range c.config.ApplicationSettings {
		hello.alpsProtocols = append(hello.alpsProtocols, protocol)
	}

	if maxVersion >= VersionTLS13 {
		// Use the same key shares between ClientHelloInner and ClientHelloOuter.
		if innerHello != nil {
			hello.hasKeyShares = innerHello.hasKeyShares
			hello.keyShares = innerHello.keyShares
		} else {
			hello.hasKeyShares = true
			hello.trailingKeyShareData = c.config.Bugs.TrailingKeyShareData
			curvesToSend := c.config.defaultCurves()
			for _, curveID := range hello.supportedCurves {
				if !curvesToSend[curveID] {
					continue
				}
				curve, ok := curveForCurveID(curveID, c.config)
				if !ok {
					continue
				}
				publicKey, err := curve.offer(c.config.rand())
				if err != nil {
					return nil, err
				}

				if c.config.Bugs.SendCurve != 0 {
					curveID = c.config.Bugs.SendCurve
				}
				if c.config.Bugs.InvalidECDHPoint {
					publicKey[0] ^= 0xff
				}

				hello.keyShares = append(hello.keyShares, keyShareEntry{
					group:       curveID,
					keyExchange: publicKey,
				})
				hs.keyShares[curveID] = curve

				if c.config.Bugs.DuplicateKeyShares {
					hello.keyShares = append(hello.keyShares, hello.keyShares[len(hello.keyShares)-1])
				}
			}

			if c.config.Bugs.MissingKeyShare {
				hello.hasKeyShares = false
			}
		}
	}

	possibleCipherSuites := c.config.cipherSuites()
	hello.cipherSuites = make([]uint16, 0, len(possibleCipherSuites))

NextCipherSuite:
	for _, suiteID := range possibleCipherSuites {
		for _, suite := range cipherSuites {
			if suite.id != suiteID {
				continue
			}
			// Don't advertise TLS 1.2-only cipher suites unless
			// we're attempting TLS 1.2.
			if maxVersion < VersionTLS12 && suite.flags&suiteTLS12 != 0 {
				continue
			}
			hello.cipherSuites = append(hello.cipherSuites, suiteID)
			continue NextCipherSuite
		}
	}

	if c.config.Bugs.AdvertiseAllConfiguredCiphers {
		hello.cipherSuites = possibleCipherSuites
	}

	if c.config.Bugs.SendRenegotiationSCSV {
		hello.cipherSuites = append(hello.cipherSuites, renegotiationSCSV)
	}

	if c.config.Bugs.SendFallbackSCSV {
		hello.cipherSuites = append(hello.cipherSuites, fallbackSCSV)
	}

	_, err := io.ReadFull(c.config.rand(), hello.random)
	if err != nil {
		c.sendAlert(alertInternalError)
		return nil, errors.New("tls: short read from Rand: " + err.Error())
	}

	if maxVersion >= VersionTLS12 && !c.config.Bugs.NoSignatureAlgorithms {
		hello.signatureAlgorithms = c.config.verifySignatureAlgorithms()
	}

	if c.config.ClientSessionCache != nil {
		hello.ticketSupported = !c.config.SessionTicketsDisabled
	}

	session := hs.session

	// ClientHelloOuter cannot offer sessions.
	if innerHello != nil && !c.config.Bugs.OfferSessionInClientHelloOuter {
		session = nil
	}

	if session != nil && c.config.time().Before(session.ticketExpiration) {
		ticket := session.sessionTicket
		if c.config.Bugs.FilterTicket != nil && len(ticket) > 0 {
			// Copy the ticket so FilterTicket may act in-place.
			ticket = make([]byte, len(session.sessionTicket))
			copy(ticket, session.sessionTicket)

			ticket, err = c.config.Bugs.FilterTicket(ticket)
			if err != nil {
				return nil, err
			}
		}

		if session.vers >= VersionTLS13 || c.config.Bugs.SendBothTickets {
			// TODO(nharper): Support sending more
			// than one PSK identity.
			ticketAge := uint32(c.config.time().Sub(session.ticketCreationTime) / time.Millisecond)
			if c.config.Bugs.SendTicketAge != 0 {
				ticketAge = uint32(c.config.Bugs.SendTicketAge / time.Millisecond)
			}
			psk := pskIdentity{
				ticket:              ticket,
				obfuscatedTicketAge: session.ticketAgeAdd + ticketAge,
			}
			hello.pskIdentities = []pskIdentity{psk}

			if c.config.Bugs.ExtraPSKIdentity {
				hello.pskIdentities = append(hello.pskIdentities, psk)
			}
		}

		if session.vers < VersionTLS13 || c.config.Bugs.SendBothTickets {
			if ticket != nil {
				hello.sessionTicket = ticket
				// A random session ID is used to detect when the
				// server accepted the ticket and is resuming a session
				// (see RFC 5077).
				sessionIDLen := 16
				if c.config.Bugs.TicketSessionIDLength != 0 {
					sessionIDLen = c.config.Bugs.TicketSessionIDLength
				}
				if c.config.Bugs.EmptyTicketSessionID {
					sessionIDLen = 0
				}
				hello.sessionID = make([]byte, sessionIDLen)
				if _, err := io.ReadFull(c.config.rand(), hello.sessionID); err != nil {
					c.sendAlert(alertInternalError)
					return nil, errors.New("tls: short read from Rand: " + err.Error())
				}
			} else {
				hello.sessionID = session.sessionID
			}
		}
	}

	if innerHello == nil {
		// Request compatibility mode from the client by sending a fake session
		// ID. Although BoringSSL always enables compatibility mode, other
		// implementations make it conditional on the ClientHello. We test
		// BoringSSL's expected behavior with SendClientHelloSessionID.
		if len(hello.sessionID) == 0 && maxVersion >= VersionTLS13 {
			hello.sessionID = make([]byte, 32)
			if _, err := io.ReadFull(c.config.rand(), hello.sessionID); err != nil {
				c.sendAlert(alertInternalError)
				return nil, errors.New("tls: short read from Rand: " + err.Error())
			}
		}
		if c.config.Bugs.MockQUICTransport != nil && !c.config.Bugs.CompatModeWithQUIC {
			hello.sessionID = []byte{}
		}
		if c.config.Bugs.SendClientHelloSessionID != nil {
			hello.sessionID = c.config.Bugs.SendClientHelloSessionID
		}
	} else {
		// ClientHelloOuter's session ID is copied from ClientHelloINnner.
		hello.sessionID = innerHello.sessionID
	}

	if c.config.Bugs.SendCipherSuites != nil {
		hello.cipherSuites = c.config.Bugs.SendCipherSuites
	}

	if innerHello == nil {
		if len(hello.pskIdentities) > 0 && c.config.Bugs.SendEarlyData != nil {
			hello.hasEarlyData = true
		}
		if c.config.Bugs.SendFakeEarlyDataLength > 0 {
			hello.hasEarlyData = true
		}
		if c.config.Bugs.OmitEarlyDataExtension {
			hello.hasEarlyData = false
		}
	} else {
		hello.hasEarlyData = innerHello.hasEarlyData
	}

	if (isInner && !c.config.Bugs.OmitECHIsInner) || c.config.Bugs.AlwaysSendECHIsInner {
		hello.echIsInner = []byte{}
		if len(c.config.Bugs.SendInvalidECHIsInner) != 0 {
			hello.echIsInner = c.config.Bugs.SendInvalidECHIsInner
		}
	}

	if innerHello != nil {
		if err := hs.encryptClientHello(hello, innerHello, c.config.ClientECHConfig.ConfigID, echEnc); err != nil {
			return nil, err
		}
		if c.config.Bugs.CorruptEncryptedClientHello {
			hello.clientECH.payload[0] ^= 1
		}
	}

	// PSK binders and ECH both must be computed last because they incorporate
	// the rest of the ClientHello and conflict. ECH resolves this by forbidding
	// clients from offering PSKs on ClientHelloOuter, but we still need to test
	// servers handle it correctly so they tolerate GREASE. In other cases, we
	// expect the server to reject ECH, so we put PSK last. Note this renders
	// ECH undecryptable.
	if len(hello.pskIdentities) > 0 {
		version := session.wireVersion
		// We may have a pre-1.3 session if SendBothTickets is set.
		if session.vers < VersionTLS13 {
			version = VersionTLS13
		}
		generatePSKBinders(version, hello, session, nil, nil, c.config)
	}

	if c.config.Bugs.SendClientHelloWithFixes != nil {
		hello, err = replaceClientHello(hello, c.config.Bugs.SendClientHelloWithFixes)
		if err != nil {
			return nil, err
		}
	}

	return hello, nil
}

// encryptClientHello encrypts |innerHello| using the specified HPKE context and
// adds the extension to |hello|.
func (hs *clientHandshakeState) encryptClientHello(hello, innerHello *clientHelloMsg, configID uint8, enc []byte) error {
	c := hs.c

	if c.config.Bugs.MinimalClientHelloOuter {
		*hello = clientHelloMsg{
			vers:               VersionTLS12,
			random:             hello.random,
			sessionID:          hello.sessionID,
			cipherSuites:       []uint16{0x0a0a},
			compressionMethods: hello.compressionMethods,
		}
	}

	if c.config.Bugs.TruncateClientECHEnc {
		enc = enc[:1]
	}

	aad := newByteBuilder()
	aad.addU16(hs.echHPKEContext.KDF())
	aad.addU16(hs.echHPKEContext.AEAD())
	aad.addU8(configID)
	aad.addU16LengthPrefixed().addBytes(enc)
	hello.marshalForOuterAAD(aad.addU24LengthPrefixed())

	encodedInner := innerHello.marshalForEncodedInner()
	payload := hs.echHPKEContext.Seal(encodedInner, aad.finish())

	// Place the ECH extension in the outer CH.
	hello.clientECH = &clientECH{
		hpkeKDF:  hs.echHPKEContext.KDF(),
		hpkeAEAD: hs.echHPKEContext.AEAD(),
		configID: configID,
		enc:      enc,
		payload:  payload,
	}

	if c.config.Bugs.RecordClientHelloInner != nil {
		if err := c.config.Bugs.RecordClientHelloInner(encodedInner, hello.marshal()[4:]); err != nil {
			return err
		}
		// ECH is normally the last extension added to |hello|, but, when
		// OfferSessionInClientHelloOuter is enabled, we may modify it again.
		hello.raw = nil
	}

	return nil
}

func (hs *clientHandshakeState) doTLS13Handshake(msg interface{}) error {
	c := hs.c

	// Once the PRF hash is known, TLS 1.3 does not require a handshake buffer.
	hs.finishedHash.discardHandshakeBuffer()

	// The first server message must be followed by a ChangeCipherSpec.
	c.expectTLS13ChangeCipherSpec = true

	// The first message may be a ServerHello or HelloRetryRequest.
	helloRetryRequest, haveHelloRetryRequest := msg.(*helloRetryRequestMsg)
	if haveHelloRetryRequest {
		hs.finishedHash.UpdateForHelloRetryRequest()
		hs.writeServerHash(helloRetryRequest.marshal())
		if hs.innerHello != nil {
			hs.innerFinishedHash.UpdateForHelloRetryRequest()
			hs.innerFinishedHash.WriteHandshake(helloRetryRequest.marshal(), c.recvHandshakeSeq-1)
		}

		if c.config.Bugs.FailIfHelloRetryRequested {
			return errors.New("tls: unexpected HelloRetryRequest")
		}
		// Explicitly read the ChangeCipherSpec now; it should
		// be attached to the first flight, not the second flight.
		if err := c.readTLS13ChangeCipherSpec(); err != nil {
			return err
		}

		// Reset the encryption state, in case we sent 0-RTT data.
		c.out.resetCipher()

		if hs.innerHello != nil {
			if err := hs.applyHelloRetryRequest(helloRetryRequest, hs.innerHello, nil); err != nil {
				return err
			}
			if err := hs.applyHelloRetryRequest(helloRetryRequest, hs.hello, hs.innerHello); err != nil {
				return err
			}
			hs.innerFinishedHash.WriteHandshake(hs.innerHello.marshal(), c.sendHandshakeSeq)
		} else {
			if err := hs.applyHelloRetryRequest(helloRetryRequest, hs.hello, nil); err != nil {
				return err
			}
		}
		hs.writeClientHash(hs.hello.marshal())
		toWrite := hs.hello.marshal()

		if c.config.Bugs.PartialSecondClientHelloAfterFirst {
			// The first byte has already been sent.
			toWrite = toWrite[1:]
		}

		if c.config.Bugs.InterleaveEarlyData {
			c.sendFakeEarlyData(4)
			c.writeRecord(recordTypeHandshake, toWrite[:16])
			c.sendFakeEarlyData(4)
			c.writeRecord(recordTypeHandshake, toWrite[16:])
		} else if c.config.Bugs.PartialClientFinishedWithSecondClientHello {
			toWrite = append(make([]byte, 0, len(toWrite)+1), toWrite...)
			toWrite = append(toWrite, typeFinished)
			c.writeRecord(recordTypeHandshake, toWrite)
		} else {
			c.writeRecord(recordTypeHandshake, toWrite)
		}
		c.flushHandshake()

		if c.config.Bugs.SendEarlyDataOnSecondClientHello {
			c.sendFakeEarlyData(4)
		}

		var err error
		msg, err = c.readHandshake()
		if err != nil {
			return err
		}
	}

	var ok bool
	hs.serverHello, ok = msg.(*serverHelloMsg)
	if !ok {
		c.sendAlert(alertUnexpectedMessage)
		return unexpectedMessageError(hs.serverHello, msg)
	}

	if c.wireVersion != hs.serverHello.vers {
		c.sendAlert(alertIllegalParameter)
		return fmt.Errorf("tls: server sent non-matching version %x vs %x", c.wireVersion, hs.serverHello.vers)
	}

	if hs.suite.id != hs.serverHello.cipherSuite {
		c.sendAlert(alertIllegalParameter)
		return fmt.Errorf("tls: server sent non-matching cipher suite %04x vs %04x", hs.suite.id, hs.serverHello.cipherSuite)
	}

	if haveHelloRetryRequest && helloRetryRequest.hasSelectedGroup && helloRetryRequest.selectedGroup != hs.serverHello.keyShare.group {
		c.sendAlert(alertHandshakeFailure)
		return errors.New("tls: ServerHello parameters did not match HelloRetryRequest")
	}

	if !bytes.Equal(hs.hello.sessionID, hs.serverHello.sessionID) {
		return errors.New("tls: session IDs did not match.")
	}

	// Resolve PSK and compute the early secret.
	zeroSecret := hs.finishedHash.zeroSecret()
	pskSecret := zeroSecret
	if hs.serverHello.hasPSKIdentity {
		// We send at most one PSK identity.
		if hs.session == nil || hs.serverHello.pskIdentity != 0 {
			c.sendAlert(alertUnknownPSKIdentity)
			return errors.New("tls: server sent unknown PSK identity")
		}
		if hs.session.cipherSuite.hash() != hs.suite.hash() {
			c.sendAlert(alertHandshakeFailure)
			return errors.New("tls: server resumed an invalid session for the cipher suite")
		}
		pskSecret = hs.session.secret
		c.didResume = true
	}
	hs.finishedHash.addEntropy(pskSecret)
	if hs.innerHello != nil {
		hs.innerFinishedHash.addEntropy(pskSecret)
	}

	if !hs.serverHello.hasKeyShare {
		c.sendAlert(alertUnsupportedExtension)
		return errors.New("tls: server omitted KeyShare on resumption.")
	}

	// Resolve ECDHE and compute the handshake secret.
	ecdheSecret := zeroSecret
	if !c.config.Bugs.MissingKeyShare && !c.config.Bugs.SecondClientHelloMissingKeyShare {
		curve, ok := hs.keyShares[hs.serverHello.keyShare.group]
		if !ok {
			c.sendAlert(alertHandshakeFailure)
			return errors.New("tls: server selected an unsupported group")
		}
		c.curveID = hs.serverHello.keyShare.group

		var err error
		ecdheSecret, err = curve.finish(hs.serverHello.keyShare.keyExchange)
		if err != nil {
			return err
		}
	}
	hs.finishedHash.nextSecret()
	hs.finishedHash.addEntropy(ecdheSecret)
	if hs.innerHello != nil {
		hs.innerFinishedHash.nextSecret()
		hs.innerFinishedHash.addEntropy(ecdheSecret)
	}

	// Determine whether the server accepted ECH.
	confirmHash := &hs.finishedHash
	if hs.innerHello != nil {
		confirmHash = &hs.innerFinishedHash
	}
	echConfirmed := bytes.Equal(hs.serverHello.random[24:], confirmHash.deriveSecretPeek([]byte("ech accept confirmation"), hs.serverHello.marshalForECHConf())[:8])
	if hs.innerHello != nil {
		c.echAccepted = echConfirmed
		if c.echAccepted {
			hs.hello = hs.innerHello
			hs.finishedHash = hs.innerFinishedHash
		}
		hs.innerHello = nil
		hs.innerFinishedHash = finishedHash{}
	} else {
		// When not offering ECH, we may still expect a confirmation signal to
		// test the backend server behavior.
		if hs.hello.echIsInner != nil {
			if !echConfirmed {
				return errors.New("tls: server did not send ECH confirmation when requested")
			}
		} else {
			if echConfirmed {
				return errors.New("tls: server did sent ECH confirmation when not requested")
			}
		}
	}

	hs.writeServerHash(hs.serverHello.marshal())

	// Derive handshake traffic keys and switch read key to handshake
	// traffic key.
	clientHandshakeTrafficSecret := hs.finishedHash.deriveSecret(clientHandshakeTrafficLabel)
	serverHandshakeTrafficSecret := hs.finishedHash.deriveSecret(serverHandshakeTrafficLabel)
	if err := c.useInTrafficSecret(encryptionHandshake, c.wireVersion, hs.suite, serverHandshakeTrafficSecret); err != nil {
		return err
	}

	msg, err := c.readHandshake()
	if err != nil {
		return err
	}

	encryptedExtensions, ok := msg.(*encryptedExtensionsMsg)
	if !ok {
		c.sendAlert(alertUnexpectedMessage)
		return unexpectedMessageError(encryptedExtensions, msg)
	}
	hs.writeServerHash(encryptedExtensions.marshal())

	if !bytes.Equal(encryptedExtensions.extensions.echRetryConfigs, c.config.Bugs.ExpectECHRetryConfigs) {
		return errors.New("tls: server sent ECH retry_configs with unexpected contents")
	}

	err = hs.processServerExtensions(&encryptedExtensions.extensions)
	if err != nil {
		return err
	}

	var chainToSend *Certificate
	var certReq *certificateRequestMsg
	if c.didResume {
		// Copy over authentication from the session.
		c.peerCertificates = hs.session.serverCertificates
		c.sctList = hs.session.sctList
		c.ocspResponse = hs.session.ocspResponse
	} else {
		msg, err := c.readHandshake()
		if err != nil {
			return err
		}

		var ok bool
		certReq, ok = msg.(*certificateRequestMsg)
		if ok {
			if len(certReq.requestContext) != 0 {
				return errors.New("tls: non-empty certificate request context sent in handshake")
			}

			if c.config.Bugs.ExpectNoCertificateAuthoritiesExtension && certReq.hasCAExtension {
				return errors.New("tls: expected no certificate_authorities extension")
			}

			if c.config.Bugs.IgnorePeerSignatureAlgorithmPreferences {
				certReq.signatureAlgorithms = c.config.signSignatureAlgorithms()
			}

			hs.writeServerHash(certReq.marshal())

			chainToSend, err = selectClientCertificate(c, certReq)
			if err != nil {
				return err
			}

			msg, err = c.readHandshake()
			if err != nil {
				return err
			}
		}

		var certMsg *certificateMsg

		if compressedCertMsg, ok := msg.(*compressedCertificateMsg); ok {
			hs.writeServerHash(compressedCertMsg.marshal())

			alg, ok := c.config.CertCompressionAlgs[compressedCertMsg.algID]
			if !ok {
				c.sendAlert(alertBadCertificate)
				return fmt.Errorf("tls: received certificate compressed with unknown algorithm %x", compressedCertMsg.algID)
			}

			decompressed := make([]byte, 4+int(compressedCertMsg.uncompressedLength))
			if !alg.Decompress(decompressed[4:], compressedCertMsg.compressed) {
				c.sendAlert(alertBadCertificate)
				return fmt.Errorf("tls: failed to decompress certificate with algorithm %x", compressedCertMsg.algID)
			}

			certMsg = &certificateMsg{
				hasRequestContext: true,
			}

			if !certMsg.unmarshal(decompressed) {
				c.sendAlert(alertBadCertificate)
				return errors.New("tls: failed to parse decompressed certificate")
			}

			if expected := c.config.Bugs.ExpectedCompressedCert; expected != 0 && expected != compressedCertMsg.algID {
				return fmt.Errorf("tls: expected certificate compressed with algorithm %x, but message used %x", expected, compressedCertMsg.algID)
			}
		} else {
			if certMsg, ok = msg.(*certificateMsg); !ok {
				c.sendAlert(alertUnexpectedMessage)
				return unexpectedMessageError(certMsg, msg)
			}
			hs.writeServerHash(certMsg.marshal())

			if c.config.Bugs.ExpectedCompressedCert != 0 {
				return errors.New("tls: uncompressed certificate received")
			}
		}

		// Check for unsolicited extensions.
		for i, cert := range certMsg.certificates {
			if c.config.Bugs.NoOCSPStapling && cert.ocspResponse != nil {
				c.sendAlert(alertUnsupportedExtension)
				return errors.New("tls: unexpected OCSP response in the server certificate")
			}
			if c.config.Bugs.NoSignedCertificateTimestamps && cert.sctList != nil {
				c.sendAlert(alertUnsupportedExtension)
				return errors.New("tls: unexpected SCT list in the server certificate")
			}
			if i > 0 && c.config.Bugs.ExpectNoExtensionsOnIntermediate && (cert.ocspResponse != nil || cert.sctList != nil) {
				c.sendAlert(alertUnsupportedExtension)
				return errors.New("tls: unexpected extensions in the server certificate")
			}
		}

		if err := hs.verifyCertificates(certMsg); err != nil {
			return err
		}
		c.ocspResponse = certMsg.certificates[0].ocspResponse
		c.sctList = certMsg.certificates[0].sctList

		msg, err = c.readHandshake()
		if err != nil {
			return err
		}
		certVerifyMsg, ok := msg.(*certificateVerifyMsg)
		if !ok {
			c.sendAlert(alertUnexpectedMessage)
			return unexpectedMessageError(certVerifyMsg, msg)
		}

		c.peerSignatureAlgorithm = certVerifyMsg.signatureAlgorithm
		input := hs.finishedHash.certificateVerifyInput(serverCertificateVerifyContextTLS13)
		err = verifyMessage(c.vers, hs.peerPublicKey, c.config, certVerifyMsg.signatureAlgorithm, input, certVerifyMsg.signature)
		if err != nil {
			return err
		}

		hs.writeServerHash(certVerifyMsg.marshal())
	}

	msg, err = c.readHandshake()
	if err != nil {
		return err
	}
	serverFinished, ok := msg.(*finishedMsg)
	if !ok {
		c.sendAlert(alertUnexpectedMessage)
		return unexpectedMessageError(serverFinished, msg)
	}

	verify := hs.finishedHash.serverSum(serverHandshakeTrafficSecret)
	if len(verify) != len(serverFinished.verifyData) ||
		subtle.ConstantTimeCompare(verify, serverFinished.verifyData) != 1 {
		c.sendAlert(alertHandshakeFailure)
		return errors.New("tls: server's Finished message was incorrect")
	}

	hs.writeServerHash(serverFinished.marshal())

	// The various secrets do not incorporate the client's final leg, so
	// derive them now before updating the handshake context.
	hs.finishedHash.nextSecret()
	hs.finishedHash.addEntropy(zeroSecret)

	clientTrafficSecret := hs.finishedHash.deriveSecret(clientApplicationTrafficLabel)
	serverTrafficSecret := hs.finishedHash.deriveSecret(serverApplicationTrafficLabel)
	c.exporterSecret = hs.finishedHash.deriveSecret(exporterLabel)

	// Switch to application data keys on read. In particular, any alerts
	// from the client certificate are read over these keys.
	if err := c.useInTrafficSecret(encryptionApplication, c.wireVersion, hs.suite, serverTrafficSecret); err != nil {
		return err
	}

	// If we're expecting 0.5-RTT messages from the server, read them now.
	var deferredTickets []*newSessionTicketMsg
	if encryptedExtensions.extensions.hasEarlyData {
		// BoringSSL will always send two tickets half-RTT when
		// negotiating 0-RTT.
		for i := 0; i < shimConfig.HalfRTTTickets; i++ {
			msg, err := c.readHandshake()
			if err != nil {
				return fmt.Errorf("tls: error reading half-RTT ticket: %s", err)
			}
			newSessionTicket, ok := msg.(*newSessionTicketMsg)
			if !ok {
				return errors.New("tls: expected half-RTT ticket")
			}
			// Defer processing until the resumption secret is computed.
			deferredTickets = append(deferredTickets, newSessionTicket)
		}
		for _, expectedMsg := range c.config.Bugs.ExpectHalfRTTData {
			if err := c.readRecord(recordTypeApplicationData); err != nil {
				return err
			}
			if !bytes.Equal(c.input.data[c.input.off:], expectedMsg) {
				return errors.New("ExpectHalfRTTData: did not get expected message")
			}
			c.in.freeBlock(c.input)
			c.input = nil
		}
	}

	// Send EndOfEarlyData and then switch write key to handshake
	// traffic key.
	if encryptedExtensions.extensions.hasEarlyData && !c.config.Bugs.SkipEndOfEarlyData && c.config.Bugs.MockQUICTransport == nil {
		if c.config.Bugs.SendStrayEarlyHandshake {
			helloRequest := new(helloRequestMsg)
			c.writeRecord(recordTypeHandshake, helloRequest.marshal())
		}
		endOfEarlyData := new(endOfEarlyDataMsg)
		endOfEarlyData.nonEmpty = c.config.Bugs.NonEmptyEndOfEarlyData
		hs.writeClientHash(endOfEarlyData.marshal())
		if c.config.Bugs.PartialEndOfEarlyDataWithClientHello {
			// The first byte has already been sent.
			c.writeRecord(recordTypeHandshake, endOfEarlyData.marshal()[1:])
		} else {
			c.writeRecord(recordTypeHandshake, endOfEarlyData.marshal())
		}
	}

	if !c.config.Bugs.SkipChangeCipherSpec && !hs.hello.hasEarlyData {
		c.writeRecord(recordTypeChangeCipherSpec, []byte{1})
	}

	for i := 0; i < c.config.Bugs.SendExtraChangeCipherSpec; i++ {
		c.writeRecord(recordTypeChangeCipherSpec, []byte{1})
	}

	c.useOutTrafficSecret(encryptionHandshake, c.wireVersion, hs.suite, clientHandshakeTrafficSecret)

	// The client EncryptedExtensions message is sent if some extension uses it.
	// (Currently only ALPS does.)
	hasEncryptedExtensions := c.config.Bugs.AlwaysSendClientEncryptedExtensions
	clientEncryptedExtensions := new(clientEncryptedExtensionsMsg)
	if encryptedExtensions.extensions.hasApplicationSettings || (c.config.Bugs.SendApplicationSettingsWithEarlyData && c.hasApplicationSettings) {
		hasEncryptedExtensions = true
		if !c.config.Bugs.OmitClientApplicationSettings {
			clientEncryptedExtensions.hasApplicationSettings = true
			clientEncryptedExtensions.applicationSettings = c.localApplicationSettings
		}
	}
	if c.config.Bugs.SendExtraClientEncryptedExtension {
		hasEncryptedExtensions = true
		clientEncryptedExtensions.customExtension = []byte{0}
	}
	if hasEncryptedExtensions && !c.config.Bugs.OmitClientEncryptedExtensions {
		hs.writeClientHash(clientEncryptedExtensions.marshal())
		c.writeRecord(recordTypeHandshake, clientEncryptedExtensions.marshal())
	}

	if certReq != nil && !c.config.Bugs.SkipClientCertificate {
		certMsg := &certificateMsg{
			hasRequestContext: true,
			requestContext:    certReq.requestContext,
		}
		if chainToSend != nil {
			for _, certData := range chainToSend.Certificate {
				certMsg.certificates = append(certMsg.certificates, certificateEntry{
					data:           certData,
					extraExtension: c.config.Bugs.SendExtensionOnCertificate,
				})
			}
		}
		hs.writeClientHash(certMsg.marshal())
		c.writeRecord(recordTypeHandshake, certMsg.marshal())

		if chainToSend != nil {
			certVerify := &certificateVerifyMsg{
				hasSignatureAlgorithm: true,
			}

			// Determine the hash to sign.
			privKey := chainToSend.PrivateKey

			var err error
			certVerify.signatureAlgorithm, err = selectSignatureAlgorithm(c.vers, privKey, c.config, certReq.signatureAlgorithms)
			if err != nil {
				c.sendAlert(alertInternalError)
				return err
			}

			input := hs.finishedHash.certificateVerifyInput(clientCertificateVerifyContextTLS13)
			certVerify.signature, err = signMessage(c.vers, privKey, c.config, certVerify.signatureAlgorithm, input)
			if err != nil {
				c.sendAlert(alertInternalError)
				return err
			}
			if c.config.Bugs.SendSignatureAlgorithm != 0 {
				certVerify.signatureAlgorithm = c.config.Bugs.SendSignatureAlgorithm
			}

			if !c.config.Bugs.SkipCertificateVerify {
				hs.writeClientHash(certVerify.marshal())
				c.writeRecord(recordTypeHandshake, certVerify.marshal())
			}
		}
	}

	if encryptedExtensions.extensions.channelIDRequested {
		channelIDHash := crypto.SHA256.New()
		channelIDHash.Write(hs.finishedHash.certificateVerifyInput(channelIDContextTLS13))
		channelIDMsgBytes, err := hs.writeChannelIDMessage(channelIDHash.Sum(nil))
		if err != nil {
			return err
		}
		hs.writeClientHash(channelIDMsgBytes)
		c.writeRecord(recordTypeHandshake, channelIDMsgBytes)
	}

	// Send a client Finished message.
	finished := new(finishedMsg)
	finished.verifyData = hs.finishedHash.clientSum(clientHandshakeTrafficSecret)
	if c.config.Bugs.BadFinished {
		finished.verifyData[0]++
	}
	hs.writeClientHash(finished.marshal())
	if c.config.Bugs.PartialClientFinishedWithClientHello {
		// The first byte has already been sent.
		c.writeRecord(recordTypeHandshake, finished.marshal()[1:])
	} else if c.config.Bugs.InterleaveEarlyData {
		finishedBytes := finished.marshal()
		c.sendFakeEarlyData(4)
		c.writeRecord(recordTypeHandshake, finishedBytes[:1])
		c.sendFakeEarlyData(4)
		c.writeRecord(recordTypeHandshake, finishedBytes[1:])
	} else {
		c.writeRecord(recordTypeHandshake, finished.marshal())
	}
	if c.config.Bugs.SendExtraFinished {
		c.writeRecord(recordTypeHandshake, finished.marshal())
	}
	c.flushHandshake()

	// Switch to application data keys.
	c.useOutTrafficSecret(encryptionApplication, c.wireVersion, hs.suite, clientTrafficSecret)
	c.resumptionSecret = hs.finishedHash.deriveSecret(resumptionLabel)
	for _, ticket := range deferredTickets {
		if err := c.processTLS13NewSessionTicket(ticket, hs.suite); err != nil {
			return err
		}
	}

	return nil
}

// applyHelloRetryRequest updates |hello| in-place based on |helloRetryRequest|.
// If |innerHello| is not nil, this is the second ClientHelloOuter and should
// contain an encrypted copy of |innerHello|
func (hs *clientHandshakeState) applyHelloRetryRequest(helloRetryRequest *helloRetryRequestMsg, hello, innerHello *clientHelloMsg) error {
	c := hs.c
	firstHelloBytes := hello.marshal()
	isInner := innerHello == nil && hs.echHPKEContext != nil
	if len(helloRetryRequest.cookie) > 0 {
		hello.tls13Cookie = helloRetryRequest.cookie
	}

	if innerHello != nil {
		hello.keyShares = innerHello.keyShares
	} else {
		if c.config.Bugs.MisinterpretHelloRetryRequestCurve != 0 {
			helloRetryRequest.hasSelectedGroup = true
			helloRetryRequest.selectedGroup = c.config.Bugs.MisinterpretHelloRetryRequestCurve
		}
		if helloRetryRequest.hasSelectedGroup {
			var hrrCurveFound bool
			group := helloRetryRequest.selectedGroup
			for _, curveID := range hello.supportedCurves {
				if group == curveID {
					hrrCurveFound = true
					break
				}
			}
			if !hrrCurveFound || hs.keyShares[group] != nil {
				c.sendAlert(alertHandshakeFailure)
				return errors.New("tls: received invalid HelloRetryRequest")
			}
			curve, ok := curveForCurveID(group, c.config)
			if !ok {
				return errors.New("tls: Unable to get curve requested in HelloRetryRequest")
			}
			publicKey, err := curve.offer(c.config.rand())
			if err != nil {
				return err
			}
			hs.keyShares[group] = curve
			hello.keyShares = []keyShareEntry{{
				group:       group,
				keyExchange: publicKey,
			}}
		}

		if c.config.Bugs.SecondClientHelloMissingKeyShare {
			hello.hasKeyShares = false
		}
	}

	if isInner && c.config.Bugs.OmitSecondECHIsInner {
		hello.echIsInner = nil
	}

	hello.hasEarlyData = c.config.Bugs.SendEarlyDataOnSecondClientHello
	// The first ClientHello may have skipped this due to OnlyCorruptSecondPSKBinder.
	if c.config.Bugs.PSKBinderFirst && c.config.Bugs.OnlyCorruptSecondPSKBinder {
		hello.prefixExtensions = append(hello.prefixExtensions, extensionPreSharedKey)
	}
	// The first ClientHello may have skipped this due to OnlyCompressSecondClientHelloInner.
	if isInner && len(c.config.ECHOuterExtensions) > 0 && c.config.Bugs.OnlyCompressSecondClientHelloInner {
		applyECHOuterExtensions(hello, c.config.ECHOuterExtensions)
	}
	if c.config.Bugs.OmitPSKsOnSecondClientHello {
		hello.pskIdentities = nil
		hello.pskBinders = nil
	}
	hello.raw = nil

	if innerHello != nil {
		if c.config.Bugs.OmitSecondEncryptedClientHello {
			hello.clientECH = nil
		} else {
			configID := c.config.ClientECHConfig.ConfigID
			if c.config.Bugs.CorruptSecondEncryptedClientHelloConfigID {
				configID ^= 1
			}
			if err := hs.encryptClientHello(hello, innerHello, configID, nil); err != nil {
				return err
			}
			if c.config.Bugs.CorruptSecondEncryptedClientHello {
				hello.clientECH.payload[0] ^= 1
			}
		}
	}

	// PSK binders and ECH both must be inserted last because they incorporate
	// the rest of the ClientHello and conflict. See corresponding comment in
	// |createClientHello|.
	if len(hello.pskIdentities) > 0 {
		generatePSKBinders(c.wireVersion, hello, hs.session, firstHelloBytes, helloRetryRequest.marshal(), c.config)
	}
	return nil
}

// applyECHOuterExtensions updates |hello| to compress |outerExtensions| with
// the ech_outer_extensions mechanism.
func applyECHOuterExtensions(hello *clientHelloMsg, outerExtensions []uint16) {
	// Ensure that the ech_outer_extensions extension and each of the
	// extensions it names are serialized consecutively.
	hello.prefixExtensions = append(hello.prefixExtensions, extensionECHOuterExtensions)
	hello.prefixExtensions = append(hello.prefixExtensions, outerExtensions...)
	hello.outerExtensions = outerExtensions
}

func (hs *clientHandshakeState) doFullHandshake() error {
	c := hs.c

	var leaf *x509.Certificate
	if hs.suite.flags&suitePSK == 0 {
		msg, err := c.readHandshake()
		if err != nil {
			return err
		}

		certMsg, ok := msg.(*certificateMsg)
		if !ok {
			c.sendAlert(alertUnexpectedMessage)
			return unexpectedMessageError(certMsg, msg)
		}
		hs.writeServerHash(certMsg.marshal())

		if err := hs.verifyCertificates(certMsg); err != nil {
			return err
		}
		leaf = c.peerCertificates[0]
	}

	if hs.serverHello.extensions.ocspStapling {
		msg, err := c.readHandshake()
		if err != nil {
			return err
		}
		cs, ok := msg.(*certificateStatusMsg)
		if !ok {
			c.sendAlert(alertUnexpectedMessage)
			return unexpectedMessageError(cs, msg)
		}
		hs.writeServerHash(cs.marshal())

		if cs.statusType == statusTypeOCSP {
			c.ocspResponse = cs.response
		}
	}

	msg, err := c.readHandshake()
	if err != nil {
		return err
	}

	keyAgreement := hs.suite.ka(c.vers)

	skx, ok := msg.(*serverKeyExchangeMsg)
	if ok {
		hs.writeServerHash(skx.marshal())
		err = keyAgreement.processServerKeyExchange(c.config, hs.hello, hs.serverHello, hs.peerPublicKey, skx)
		if err != nil {
			c.sendAlert(alertUnexpectedMessage)
			return err
		}
		if ecdhe, ok := keyAgreement.(*ecdheKeyAgreement); ok {
			c.curveID = ecdhe.curveID
		}

		c.peerSignatureAlgorithm = keyAgreement.peerSignatureAlgorithm()

		msg, err = c.readHandshake()
		if err != nil {
			return err
		}
	}

	var chainToSend *Certificate
	var certRequested bool
	certReq, ok := msg.(*certificateRequestMsg)
	if ok {
		certRequested = true
		if c.config.Bugs.IgnorePeerSignatureAlgorithmPreferences {
			certReq.signatureAlgorithms = c.config.signSignatureAlgorithms()
		}

		hs.writeServerHash(certReq.marshal())

		chainToSend, err = selectClientCertificate(c, certReq)
		if err != nil {
			return err
		}

		msg, err = c.readHandshake()
		if err != nil {
			return err
		}
	}

	shd, ok := msg.(*serverHelloDoneMsg)
	if !ok {
		c.sendAlert(alertUnexpectedMessage)
		return unexpectedMessageError(shd, msg)
	}
	hs.writeServerHash(shd.marshal())

	// If the server requested a certificate then we have to send a
	// Certificate message in TLS, even if it's empty because we don't have
	// a certificate to send.
	if certRequested && !c.config.Bugs.SkipClientCertificate {
		certMsg := new(certificateMsg)
		if chainToSend != nil {
			for _, certData := range chainToSend.Certificate {
				certMsg.certificates = append(certMsg.certificates, certificateEntry{
					data: certData,
				})
			}
		}
		hs.writeClientHash(certMsg.marshal())
		c.writeRecord(recordTypeHandshake, certMsg.marshal())
	}

	preMasterSecret, ckx, err := keyAgreement.generateClientKeyExchange(c.config, hs.hello, leaf)
	if err != nil {
		c.sendAlert(alertInternalError)
		return err
	}
	if ckx != nil {
		if c.config.Bugs.EarlyChangeCipherSpec < 2 {
			hs.writeClientHash(ckx.marshal())
		}
		if c.config.Bugs.PartialClientKeyExchangeWithClientHello {
			// The first byte was already written.
			c.writeRecord(recordTypeHandshake, ckx.marshal()[1:])
		} else {
			c.writeRecord(recordTypeHandshake, ckx.marshal())
		}
	}

	if hs.serverHello.extensions.extendedMasterSecret {
		hs.masterSecret = extendedMasterFromPreMasterSecret(c.vers, hs.suite, preMasterSecret, hs.finishedHash)
		c.extendedMasterSecret = true
	} else {
		if c.config.Bugs.RequireExtendedMasterSecret {
			return errors.New("tls: extended master secret required but not supported by peer")
		}
		hs.masterSecret = masterFromPreMasterSecret(c.vers, hs.suite, preMasterSecret, hs.hello.random, hs.serverHello.random)
	}

	if chainToSend != nil {
		certVerify := &certificateVerifyMsg{
			hasSignatureAlgorithm: c.vers >= VersionTLS12,
		}

		// Determine the hash to sign.
		privKey := c.config.Certificates[0].PrivateKey

		if certVerify.hasSignatureAlgorithm {
			certVerify.signatureAlgorithm, err = selectSignatureAlgorithm(c.vers, privKey, c.config, certReq.signatureAlgorithms)
			if err != nil {
				c.sendAlert(alertInternalError)
				return err
			}
		}

		certVerify.signature, err = signMessage(c.vers, privKey, c.config, certVerify.signatureAlgorithm, hs.finishedHash.buffer)
		if err == nil && c.config.Bugs.SendSignatureAlgorithm != 0 {
			certVerify.signatureAlgorithm = c.config.Bugs.SendSignatureAlgorithm
		}
		if err != nil {
			c.sendAlert(alertInternalError)
			return errors.New("tls: failed to sign handshake with client certificate: " + err.Error())
		}

		if !c.config.Bugs.SkipCertificateVerify {
			hs.writeClientHash(certVerify.marshal())
			c.writeRecord(recordTypeHandshake, certVerify.marshal())
		}
	}
	// flushHandshake will be called in sendFinished.

	hs.finishedHash.discardHandshakeBuffer()

	return nil
}

// delegatedCredentialSignedMessage returns the bytes that are signed in order
// to authenticate a delegated credential.
func delegatedCredentialSignedMessage(credBytes []byte, algorithm signatureAlgorithm, leafDER []byte) []byte {
	// https://tools.ietf.org/html/draft-ietf-tls-subcerts-03#section-3
	ret := make([]byte, 64, 128)
	for i := range ret {
		ret[i] = 0x20
	}

	ret = append(ret, []byte("TLS, server delegated credentials\x00")...)
	ret = append(ret, leafDER...)
	ret = append(ret, byte(algorithm>>8), byte(algorithm))
	ret = append(ret, credBytes...)

	return ret
}

func (hs *clientHandshakeState) verifyCertificates(certMsg *certificateMsg) error {
	c := hs.c

	if len(certMsg.certificates) == 0 {
		c.sendAlert(alertIllegalParameter)
		return errors.New("tls: no certificates sent")
	}

	var dc *delegatedCredential
	certs := make([]*x509.Certificate, len(certMsg.certificates))
	for i, certEntry := range certMsg.certificates {
		cert, err := x509.ParseCertificate(certEntry.data)
		if err != nil {
			c.sendAlert(alertBadCertificate)
			return errors.New("tls: failed to parse certificate from server: " + err.Error())
		}
		certs[i] = cert

		if certEntry.delegatedCredential != nil {
			if c.config.Bugs.FailIfDelegatedCredentials {
				c.sendAlert(alertIllegalParameter)
				return errors.New("tls: unexpected delegated credential")
			}
			if i != 0 {
				c.sendAlert(alertIllegalParameter)
				return errors.New("tls: non-leaf certificate has a delegated credential")
			}
			if c.config.Bugs.DisableDelegatedCredentials {
				c.sendAlert(alertIllegalParameter)
				return errors.New("tls: server sent delegated credential without it being requested")
			}
			dc = certEntry.delegatedCredential
		}
	}

	if !c.config.InsecureSkipVerify {
		opts := x509.VerifyOptions{
			Roots:         c.config.RootCAs,
			CurrentTime:   c.config.time(),
			DNSName:       c.config.ServerName,
			Intermediates: x509.NewCertPool(),
		}

		for i, cert := range certs {
			if i == 0 {
				continue
			}
			opts.Intermediates.AddCert(cert)
		}
		var err error
		c.verifiedChains, err = certs[0].Verify(opts)
		if err != nil {
			c.sendAlert(alertBadCertificate)
			return err
		}
	}

	leafPublicKey := certs[0].PublicKey
	switch leafPublicKey.(type) {
	case *rsa.PublicKey, *ecdsa.PublicKey, ed25519.PublicKey:
		break
	default:
		c.sendAlert(alertUnsupportedCertificate)
		return fmt.Errorf("tls: server's certificate contains an unsupported type of public key: %T", leafPublicKey)
	}

	c.peerCertificates = certs

	if dc != nil {
		// Note that this doesn't check a) the delegated credential temporal
		// validity nor b) that the certificate has the special OID asserted.
		var err error
		if hs.peerPublicKey, err = x509.ParsePKIXPublicKey(dc.pkixPublicKey); err != nil {
			c.sendAlert(alertBadCertificate)
			return errors.New("tls: failed to parse public key from delegated credential: " + err.Error())
		}

		verifier, err := getSigner(c.vers, hs.peerPublicKey, c.config, dc.algorithm, true)
		if err != nil {
			c.sendAlert(alertBadCertificate)
			return errors.New("tls: failed to get verifier for delegated credential: " + err.Error())
		}

		if err := verifier.verifyMessage(leafPublicKey, delegatedCredentialSignedMessage(dc.signedBytes, dc.algorithm, certs[0].Raw), dc.signature); err != nil {
			c.sendAlert(alertBadCertificate)
			return errors.New("tls: failed to verify delegated credential: " + err.Error())
		}
	} else if c.config.Bugs.ExpectDelegatedCredentials {
		c.sendAlert(alertInternalError)
		return errors.New("tls: delegated credentials missing")
	} else {
		hs.peerPublicKey = leafPublicKey
	}

	return nil
}

func (hs *clientHandshakeState) establishKeys() error {
	c := hs.c

	clientMAC, serverMAC, clientKey, serverKey, clientIV, serverIV :=
		keysFromMasterSecret(c.vers, hs.suite, hs.masterSecret, hs.hello.random, hs.serverHello.random, hs.suite.macLen, hs.suite.keyLen, hs.suite.ivLen(c.vers))
	var clientCipher, serverCipher interface{}
	var clientHash, serverHash macFunction
	if hs.suite.cipher != nil {
		clientCipher = hs.suite.cipher(clientKey, clientIV, false /* not for reading */)
		clientHash = hs.suite.mac(c.vers, clientMAC)
		serverCipher = hs.suite.cipher(serverKey, serverIV, true /* for reading */)
		serverHash = hs.suite.mac(c.vers, serverMAC)
	} else {
		clientCipher = hs.suite.aead(c.vers, clientKey, clientIV)
		serverCipher = hs.suite.aead(c.vers, serverKey, serverIV)
	}

	c.in.prepareCipherSpec(c.wireVersion, serverCipher, serverHash)
	c.out.prepareCipherSpec(c.wireVersion, clientCipher, clientHash)
	return nil
}

func (hs *clientHandshakeState) processServerExtensions(serverExtensions *serverExtensions) error {
	c := hs.c

	if c.vers < VersionTLS13 {
		if c.config.Bugs.RequireRenegotiationInfo && serverExtensions.secureRenegotiation == nil {
			return errors.New("tls: renegotiation extension missing")
		}

		if len(c.clientVerify) > 0 && !c.noRenegotiationInfo() {
			var expectedRenegInfo []byte
			expectedRenegInfo = append(expectedRenegInfo, c.clientVerify...)
			expectedRenegInfo = append(expectedRenegInfo, c.serverVerify...)
			if !bytes.Equal(serverExtensions.secureRenegotiation, expectedRenegInfo) {
				c.sendAlert(alertHandshakeFailure)
				return fmt.Errorf("tls: renegotiation mismatch")
			}
		}
	} else if serverExtensions.secureRenegotiation != nil {
		return errors.New("tls: renegotiation info sent in TLS 1.3")
	}

	if expected := c.config.Bugs.ExpectedCustomExtension; expected != nil {
		if serverExtensions.customExtension != *expected {
			return fmt.Errorf("tls: bad custom extension contents %q", serverExtensions.customExtension)
		}
	}

	clientDidNPN := hs.hello.nextProtoNeg
	clientDidALPN := len(hs.hello.alpnProtocols) > 0
	serverHasNPN := serverExtensions.nextProtoNeg
	serverHasALPN := len(serverExtensions.alpnProtocol) > 0

	if !clientDidNPN && serverHasNPN {
		c.sendAlert(alertHandshakeFailure)
		return errors.New("server advertised unrequested NPN extension")
	}

	if !clientDidALPN && serverHasALPN {
		c.sendAlert(alertHandshakeFailure)
		return errors.New("server advertised unrequested ALPN extension")
	}

	if serverHasNPN && serverHasALPN {
		c.sendAlert(alertHandshakeFailure)
		return errors.New("server advertised both NPN and ALPN extensions")
	}

	if serverHasALPN {
		c.clientProtocol = serverExtensions.alpnProtocol
		c.clientProtocolFallback = false
		c.usedALPN = true
	}

	if serverHasNPN && c.vers >= VersionTLS13 {
		c.sendAlert(alertHandshakeFailure)
		return errors.New("server advertised NPN over TLS 1.3")
	}

	if !hs.hello.channelIDSupported && serverExtensions.channelIDRequested {
		c.sendAlert(alertHandshakeFailure)
		return errors.New("server advertised unrequested Channel ID extension")
	}

	if len(serverExtensions.tokenBindingParams) == 1 {
		found := false
		for _, p := range c.config.TokenBindingParams {
			if p == serverExtensions.tokenBindingParams[0] {
				c.tokenBindingParam = p
				found = true
				break
			}
		}
		if !found {
			return errors.New("tls: server advertised unsupported Token Binding key param")
		}
		if serverExtensions.tokenBindingVersion > c.config.TokenBindingVersion {
			return errors.New("tls: server's Token Binding version is too new")
		}
		if c.vers < VersionTLS13 {
			if !serverExtensions.extendedMasterSecret || serverExtensions.secureRenegotiation == nil {
				return errors.New("server sent Token Binding without EMS or RI")
			}
		}
		c.tokenBindingNegotiated = true
	}

	if serverExtensions.extendedMasterSecret && c.vers >= VersionTLS13 {
		return errors.New("tls: server advertised extended master secret over TLS 1.3")
	}

	if serverExtensions.ticketSupported && c.vers >= VersionTLS13 {
		return errors.New("tls: server advertised ticket extension over TLS 1.3")
	}

	if serverExtensions.ocspStapling && c.vers >= VersionTLS13 {
		return errors.New("tls: server advertised OCSP in ServerHello over TLS 1.3")
	}

	if serverExtensions.ocspStapling && c.config.Bugs.NoOCSPStapling {
		return errors.New("tls: server advertised unrequested OCSP extension")
	}

	if len(serverExtensions.sctList) > 0 && c.vers >= VersionTLS13 {
		return errors.New("tls: server advertised SCTs in ServerHello over TLS 1.3")
	}

	if len(serverExtensions.sctList) > 0 && c.config.Bugs.NoSignedCertificateTimestamps {
		return errors.New("tls: server advertised unrequested SCTs")
	}

	if serverExtensions.srtpProtectionProfile != 0 {
		if serverExtensions.srtpMasterKeyIdentifier != "" {
			return errors.New("tls: server selected SRTP MKI value")
		}

		found := false
		for _, p := range c.config.SRTPProtectionProfiles {
			if p == serverExtensions.srtpProtectionProfile {
				found = true
				break
			}
		}
		if !found {
			return errors.New("tls: server advertised unsupported SRTP profile")
		}

		c.srtpProtectionProfile = serverExtensions.srtpProtectionProfile
	}

	if c.vers >= VersionTLS13 && c.didResume {
		if c.config.Bugs.ExpectEarlyDataAccepted && !serverExtensions.hasEarlyData {
			c.sendAlert(alertHandshakeFailure)
			return errors.New("tls: server did not accept early data when expected")
		}

		if !c.config.Bugs.ExpectEarlyDataAccepted && serverExtensions.hasEarlyData {
			c.sendAlert(alertHandshakeFailure)
			return errors.New("tls: server accepted early data when not expected")
		}
	} else if serverExtensions.hasEarlyData {
		return errors.New("tls: server accepted early data when not resuming")
	}

	if len(serverExtensions.quicTransportParams) > 0 {
		if c.vers < VersionTLS13 {
			c.sendAlert(alertHandshakeFailure)
			return errors.New("tls: server sent QUIC transport params for TLS version less than 1.3")
		}
		c.quicTransportParams = serverExtensions.quicTransportParams
	}

	if len(serverExtensions.quicTransportParamsLegacy) > 0 {
		if c.vers < VersionTLS13 {
			c.sendAlert(alertHandshakeFailure)
			return errors.New("tls: server sent QUIC transport params for TLS version less than 1.3")
		}
		c.quicTransportParamsLegacy = serverExtensions.quicTransportParamsLegacy
	}

	if serverExtensions.hasApplicationSettings {
		if c.vers < VersionTLS13 {
			return errors.New("tls: server sent application settings at invalid version")
		}
		if serverExtensions.hasEarlyData {
			return errors.New("tls: server sent application settings with 0-RTT")
		}
		if !serverHasALPN {
			return errors.New("tls: server sent application settings without ALPN")
		}
		settings, ok := c.config.ApplicationSettings[serverExtensions.alpnProtocol]
		if !ok {
			return errors.New("tls: server sent application settings for invalid protocol")
		}
		c.hasApplicationSettings = true
		c.localApplicationSettings = settings
		c.peerApplicationSettings = serverExtensions.applicationSettings
	} else if serverExtensions.hasEarlyData {
		// 0-RTT connections inherit application settings from the session.
		c.hasApplicationSettings = hs.session.hasApplicationSettings
		c.localApplicationSettings = hs.session.localApplicationSettings
		c.peerApplicationSettings = hs.session.peerApplicationSettings
	}

	return nil
}

func (hs *clientHandshakeState) serverResumedSession() bool {
	// If the server responded with the same sessionID then it means the
	// sessionTicket is being used to resume a TLS session.
	//
	// Note that, if hs.hello.sessionID is a non-nil empty array, this will
	// accept an empty session ID from the server as resumption. See
	// EmptyTicketSessionID.
	return hs.session != nil && hs.hello.sessionID != nil &&
		bytes.Equal(hs.serverHello.sessionID, hs.hello.sessionID)
}

func (hs *clientHandshakeState) processServerHello() (bool, error) {
	c := hs.c

	// Check for downgrade signals in the server random, per RFC 8446, section 4.1.3.
	gotDowngrade := hs.serverHello.random[len(hs.serverHello.random)-8:]
	if !c.config.Bugs.IgnoreTLS13DowngradeRandom {
		if c.config.maxVersion(c.isDTLS) >= VersionTLS13 {
			if bytes.Equal(gotDowngrade, downgradeTLS13) {
				c.sendAlert(alertProtocolVersion)
				return false, errors.New("tls: downgrade from TLS 1.3 detected")
			}
		}
		if c.vers <= VersionTLS11 && c.config.maxVersion(c.isDTLS) >= VersionTLS12 {
			if bytes.Equal(gotDowngrade, downgradeTLS12) {
				c.sendAlert(alertProtocolVersion)
				return false, errors.New("tls: downgrade from TLS 1.2 detected")
			}
		}
	}

	if bytes.Equal(gotDowngrade, downgradeJDK11) != c.config.Bugs.ExpectJDK11DowngradeRandom {
		c.sendAlert(alertProtocolVersion)
		if c.config.Bugs.ExpectJDK11DowngradeRandom {
			return false, errors.New("tls: server did not send a JDK 11 downgrade signal")
		}
		return false, errors.New("tls: server sent an unexpected JDK 11 downgrade signal")
	}

	if c.config.Bugs.ExpectOmitExtensions && !hs.serverHello.omitExtensions {
		return false, errors.New("tls: ServerHello did not omit extensions")
	}

	if hs.serverResumedSession() {
		// For test purposes, assert that the server never accepts the
		// resumption offer on renegotiation.
		if c.cipherSuite != nil && c.config.Bugs.FailIfResumeOnRenego {
			return false, errors.New("tls: server resumed session on renegotiation")
		}

		if hs.serverHello.extensions.sctList != nil {
			return false, errors.New("tls: server sent SCT extension on session resumption")
		}

		if hs.serverHello.extensions.ocspStapling {
			return false, errors.New("tls: server sent OCSP extension on session resumption")
		}

		// Restore masterSecret and peerCerts from previous state
		hs.masterSecret = hs.session.secret
		c.peerCertificates = hs.session.serverCertificates
		c.extendedMasterSecret = hs.session.extendedMasterSecret
		c.sctList = hs.session.sctList
		c.ocspResponse = hs.session.ocspResponse
		hs.finishedHash.discardHandshakeBuffer()
		return true, nil
	}

	if hs.serverHello.extensions.sctList != nil {
		c.sctList = hs.serverHello.extensions.sctList
	}

	return false, nil
}

func (hs *clientHandshakeState) readFinished(out []byte) error {
	c := hs.c

	c.readRecord(recordTypeChangeCipherSpec)
	if err := c.in.error(); err != nil {
		return err
	}

	msg, err := c.readHandshake()
	if err != nil {
		return err
	}
	serverFinished, ok := msg.(*finishedMsg)
	if !ok {
		c.sendAlert(alertUnexpectedMessage)
		return unexpectedMessageError(serverFinished, msg)
	}

	if c.config.Bugs.EarlyChangeCipherSpec == 0 {
		verify := hs.finishedHash.serverSum(hs.masterSecret)
		if len(verify) != len(serverFinished.verifyData) ||
			subtle.ConstantTimeCompare(verify, serverFinished.verifyData) != 1 {
			c.sendAlert(alertHandshakeFailure)
			return errors.New("tls: server's Finished message was incorrect")
		}
	}
	c.serverVerify = append(c.serverVerify[:0], serverFinished.verifyData...)
	copy(out, serverFinished.verifyData)
	hs.writeServerHash(serverFinished.marshal())
	return nil
}

func (hs *clientHandshakeState) readSessionTicket() error {
	c := hs.c

	// Create a session with no server identifier. Either a
	// session ID or session ticket will be attached.
	session := &ClientSessionState{
		vers:               c.vers,
		wireVersion:        c.wireVersion,
		cipherSuite:        hs.suite,
		secret:             hs.masterSecret,
		handshakeHash:      hs.finishedHash.Sum(),
		serverCertificates: c.peerCertificates,
		sctList:            c.sctList,
		ocspResponse:       c.ocspResponse,
		ticketExpiration:   c.config.time().Add(time.Duration(7 * 24 * time.Hour)),
	}

	if !hs.serverHello.extensions.ticketSupported {
		if c.config.Bugs.ExpectNewTicket {
			return errors.New("tls: expected new ticket")
		}
		if hs.session == nil && len(hs.serverHello.sessionID) > 0 {
			session.sessionID = hs.serverHello.sessionID
			hs.session = session
		}
		return nil
	}

	if c.config.Bugs.ExpectNoNewSessionTicket {
		return errors.New("tls: received unexpected NewSessionTicket")
	}

	msg, err := c.readHandshake()
	if err != nil {
		return err
	}
	sessionTicketMsg, ok := msg.(*newSessionTicketMsg)
	if !ok {
		c.sendAlert(alertUnexpectedMessage)
		return unexpectedMessageError(sessionTicketMsg, msg)
	}

	session.sessionTicket = sessionTicketMsg.ticket
	hs.session = session

	hs.writeServerHash(sessionTicketMsg.marshal())

	return nil
}

func (hs *clientHandshakeState) sendFinished(out []byte, isResume bool) error {
	c := hs.c

	var postCCSMsgs [][]byte
	seqno := hs.c.sendHandshakeSeq
	if hs.serverHello.extensions.nextProtoNeg {
		nextProto := new(nextProtoMsg)
		proto, fallback := mutualProtocol(c.config.NextProtos, hs.serverHello.extensions.nextProtos)
		nextProto.proto = proto
		c.clientProtocol = proto
		c.clientProtocolFallback = fallback

		nextProtoBytes := nextProto.marshal()
		hs.finishedHash.WriteHandshake(nextProtoBytes, seqno)
		seqno++
		postCCSMsgs = append(postCCSMsgs, nextProtoBytes)
	}

	if hs.serverHello.extensions.channelIDRequested {
		var resumeHash []byte
		if isResume {
			resumeHash = hs.session.handshakeHash
		}
		channelIDMsgBytes, err := hs.writeChannelIDMessage(hs.finishedHash.hashForChannelID(resumeHash))
		if err != nil {
			return err
		}
		hs.finishedHash.WriteHandshake(channelIDMsgBytes, seqno)
		seqno++
		postCCSMsgs = append(postCCSMsgs, channelIDMsgBytes)
	}

	finished := new(finishedMsg)
	if c.config.Bugs.EarlyChangeCipherSpec == 2 {
		finished.verifyData = hs.finishedHash.clientSum(nil)
	} else {
		finished.verifyData = hs.finishedHash.clientSum(hs.masterSecret)
	}
	copy(out, finished.verifyData)
	if c.config.Bugs.BadFinished {
		finished.verifyData[0]++
	}
	c.clientVerify = append(c.clientVerify[:0], finished.verifyData...)
	hs.finishedBytes = finished.marshal()
	hs.finishedHash.WriteHandshake(hs.finishedBytes, seqno)
	if c.config.Bugs.PartialClientFinishedWithClientHello {
		// The first byte has already been written.
		postCCSMsgs = append(postCCSMsgs, hs.finishedBytes[1:])
	} else {
		postCCSMsgs = append(postCCSMsgs, hs.finishedBytes)
	}

	if c.config.Bugs.FragmentAcrossChangeCipherSpec {
		c.writeRecord(recordTypeHandshake, postCCSMsgs[0][:5])
		postCCSMsgs[0] = postCCSMsgs[0][5:]
	} else if c.config.Bugs.SendUnencryptedFinished {
		c.writeRecord(recordTypeHandshake, postCCSMsgs[0])
		postCCSMsgs = postCCSMsgs[1:]
	}

	if !c.config.Bugs.SkipChangeCipherSpec &&
		c.config.Bugs.EarlyChangeCipherSpec == 0 {
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

	if !c.config.Bugs.SkipFinished {
		for _, msg := range postCCSMsgs {
			c.writeRecord(recordTypeHandshake, msg)
		}

		if c.config.Bugs.SendExtraFinished {
			c.writeRecord(recordTypeHandshake, finished.marshal())
		}
	}

	if !isResume || !c.config.Bugs.PackAppDataWithHandshake {
		c.flushHandshake()
	}
	return nil
}

func (hs *clientHandshakeState) writeChannelIDMessage(channelIDHash []byte) ([]byte, error) {
	c := hs.c
	channelIDMsg := new(channelIDMsg)
	if c.config.ChannelID.Curve != elliptic.P256() {
		return nil, fmt.Errorf("tls: Channel ID is not on P-256.")
	}
	r, s, err := ecdsa.Sign(c.config.rand(), c.config.ChannelID, channelIDHash)
	if err != nil {
		return nil, err
	}
	channelID := make([]byte, 128)
	writeIntPadded(channelID[0:32], c.config.ChannelID.X)
	writeIntPadded(channelID[32:64], c.config.ChannelID.Y)
	writeIntPadded(channelID[64:96], r)
	writeIntPadded(channelID[96:128], s)
	if c.config.Bugs.InvalidChannelIDSignature {
		channelID[64] ^= 1
	}
	channelIDMsg.channelID = channelID

	c.channelID = &c.config.ChannelID.PublicKey

	return channelIDMsg.marshal(), nil
}

func (hs *clientHandshakeState) writeClientHash(msg []byte) {
	// writeClientHash is called before writeRecord.
	hs.finishedHash.WriteHandshake(msg, hs.c.sendHandshakeSeq)
}

func (hs *clientHandshakeState) writeServerHash(msg []byte) {
	// writeServerHash is called after readHandshake.
	hs.finishedHash.WriteHandshake(msg, hs.c.recvHandshakeSeq-1)
}

// selectClientCertificate selects a certificate for use with the given
// certificate, or none if none match. It may return a particular certificate or
// nil on success, or an error on internal error.
func selectClientCertificate(c *Conn, certReq *certificateRequestMsg) (*Certificate, error) {
	if len(c.config.Certificates) == 0 {
		return nil, nil
	}

	// The test is assumed to have configured the certificate it meant to
	// send.
	if len(c.config.Certificates) > 1 {
		return nil, errors.New("tls: multiple certificates configured")
	}

	return &c.config.Certificates[0], nil
}

// clientSessionCacheKey returns a key used to cache sessionTickets that could
// be used to resume previously negotiated TLS sessions with a server.
func clientSessionCacheKey(serverAddr net.Addr, config *Config) string {
	if len(config.ServerName) > 0 {
		return config.ServerName
	}
	return serverAddr.String()
}

// mutualProtocol finds the mutual Next Protocol Negotiation or ALPN protocol
// given list of possible protocols and a list of the preference order. The
// first list must not be empty. It returns the resulting protocol and flag
// indicating if the fallback case was reached.
func mutualProtocol(protos, preferenceProtos []string) (string, bool) {
	for _, s := range preferenceProtos {
		for _, c := range protos {
			if s == c {
				return s, false
			}
		}
	}

	return protos[0], true
}

// writeIntPadded writes x into b, padded up with leading zeros as
// needed.
func writeIntPadded(b []byte, x *big.Int) {
	for i := range b {
		b[i] = 0
	}
	xb := x.Bytes()
	copy(b[len(b)-len(xb):], xb)
}

func generatePSKBinders(version uint16, hello *clientHelloMsg, session *ClientSessionState, firstClientHello, helloRetryRequest []byte, config *Config) {
	maybeCorruptBinder := !config.Bugs.OnlyCorruptSecondPSKBinder || len(firstClientHello) > 0
	binderLen := session.cipherSuite.hash().Size()
	numBinders := 1
	if maybeCorruptBinder {
		if config.Bugs.SendNoPSKBinder {
			// The binders may have been set from the previous
			// ClientHello.
			hello.pskBinders = nil
			return
		}

		if config.Bugs.SendShortPSKBinder {
			binderLen--
		}

		if config.Bugs.SendExtraPSKBinder {
			numBinders++
		}
	}

	// Fill hello.pskBinders with appropriate length arrays of zeros so the
	// length prefixes are correct when computing the binder over the truncated
	// ClientHello message.
	hello.pskBinders = make([][]byte, numBinders)
	for i := range hello.pskBinders {
		hello.pskBinders[i] = make([]byte, binderLen)
	}

	helloBytes := hello.marshal()
	binderSize := len(hello.pskBinders)*(binderLen+1) + 2
	truncatedHello := helloBytes[:len(helloBytes)-binderSize]
	binder := computePSKBinder(session.secret, version, resumptionPSKBinderLabel, session.cipherSuite, firstClientHello, helloRetryRequest, truncatedHello)
	if maybeCorruptBinder {
		if config.Bugs.SendShortPSKBinder {
			binder = binder[:binderLen]
		}
		if config.Bugs.SendInvalidPSKBinder {
			binder[0] ^= 1
		}
	}

	for i := range hello.pskBinders {
		hello.pskBinders[i] = binder
	}

	hello.raw = nil
}
