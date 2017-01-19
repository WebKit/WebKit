// Copyright 2010 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package runner

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/rsa"
	"crypto/subtle"
	"crypto/x509"
	"errors"
	"fmt"
	"io"
	"math/big"

	"./curve25519"
	"./newhope"
)

type keyType int

const (
	keyTypeRSA keyType = iota + 1
	keyTypeECDSA
)

var errClientKeyExchange = errors.New("tls: invalid ClientKeyExchange message")
var errServerKeyExchange = errors.New("tls: invalid ServerKeyExchange message")

// rsaKeyAgreement implements the standard TLS key agreement where the client
// encrypts the pre-master secret to the server's public key.
type rsaKeyAgreement struct {
	version       uint16
	clientVersion uint16
	exportKey     *rsa.PrivateKey
}

func (ka *rsaKeyAgreement) generateServerKeyExchange(config *Config, cert *Certificate, clientHello *clientHelloMsg, hello *serverHelloMsg) (*serverKeyExchangeMsg, error) {
	// Save the client version for comparison later.
	ka.clientVersion = clientHello.vers

	if !config.Bugs.RSAEphemeralKey {
		return nil, nil
	}

	// Generate an ephemeral RSA key to use instead of the real
	// one, as in RSA_EXPORT.
	key, err := rsa.GenerateKey(config.rand(), 512)
	if err != nil {
		return nil, err
	}
	ka.exportKey = key

	modulus := key.N.Bytes()
	exponent := big.NewInt(int64(key.E)).Bytes()
	serverRSAParams := make([]byte, 0, 2+len(modulus)+2+len(exponent))
	serverRSAParams = append(serverRSAParams, byte(len(modulus)>>8), byte(len(modulus)))
	serverRSAParams = append(serverRSAParams, modulus...)
	serverRSAParams = append(serverRSAParams, byte(len(exponent)>>8), byte(len(exponent)))
	serverRSAParams = append(serverRSAParams, exponent...)

	var sigAlg signatureAlgorithm
	if ka.version >= VersionTLS12 {
		sigAlg, err = selectSignatureAlgorithm(ka.version, cert.PrivateKey, config, clientHello.signatureAlgorithms)
		if err != nil {
			return nil, err
		}
	}

	sig, err := signMessage(ka.version, cert.PrivateKey, config, sigAlg, serverRSAParams)
	if err != nil {
		return nil, errors.New("failed to sign RSA parameters: " + err.Error())
	}

	skx := new(serverKeyExchangeMsg)
	sigAlgsLen := 0
	if ka.version >= VersionTLS12 {
		sigAlgsLen = 2
	}
	skx.key = make([]byte, len(serverRSAParams)+sigAlgsLen+2+len(sig))
	copy(skx.key, serverRSAParams)
	k := skx.key[len(serverRSAParams):]
	if ka.version >= VersionTLS12 {
		k[0] = byte(sigAlg >> 8)
		k[1] = byte(sigAlg)
		k = k[2:]
	}
	k[0] = byte(len(sig) >> 8)
	k[1] = byte(len(sig))
	copy(k[2:], sig)

	return skx, nil
}

func (ka *rsaKeyAgreement) processClientKeyExchange(config *Config, cert *Certificate, ckx *clientKeyExchangeMsg, version uint16) ([]byte, error) {
	preMasterSecret := make([]byte, 48)
	_, err := io.ReadFull(config.rand(), preMasterSecret[2:])
	if err != nil {
		return nil, err
	}

	if len(ckx.ciphertext) < 2 {
		return nil, errClientKeyExchange
	}

	ciphertext := ckx.ciphertext
	if version != VersionSSL30 {
		ciphertextLen := int(ckx.ciphertext[0])<<8 | int(ckx.ciphertext[1])
		if ciphertextLen != len(ckx.ciphertext)-2 {
			return nil, errClientKeyExchange
		}
		ciphertext = ckx.ciphertext[2:]
	}

	key := cert.PrivateKey.(*rsa.PrivateKey)
	if ka.exportKey != nil {
		key = ka.exportKey
	}
	err = rsa.DecryptPKCS1v15SessionKey(config.rand(), key, ciphertext, preMasterSecret)
	if err != nil {
		return nil, err
	}
	// This check should be done in constant-time, but this is a testing
	// implementation. See the discussion at the end of section 7.4.7.1 of
	// RFC 4346.
	vers := uint16(preMasterSecret[0])<<8 | uint16(preMasterSecret[1])
	if ka.clientVersion != vers {
		return nil, fmt.Errorf("tls: invalid version in RSA premaster (got %04x, wanted %04x)", vers, ka.clientVersion)
	}
	return preMasterSecret, nil
}

func (ka *rsaKeyAgreement) processServerKeyExchange(config *Config, clientHello *clientHelloMsg, serverHello *serverHelloMsg, cert *x509.Certificate, skx *serverKeyExchangeMsg) error {
	return errors.New("tls: unexpected ServerKeyExchange")
}

func (ka *rsaKeyAgreement) generateClientKeyExchange(config *Config, clientHello *clientHelloMsg, cert *x509.Certificate) ([]byte, *clientKeyExchangeMsg, error) {
	bad := config.Bugs.BadRSAClientKeyExchange
	preMasterSecret := make([]byte, 48)
	vers := clientHello.vers
	if bad == RSABadValueWrongVersion {
		vers ^= 1
	}
	preMasterSecret[0] = byte(vers >> 8)
	preMasterSecret[1] = byte(vers)
	_, err := io.ReadFull(config.rand(), preMasterSecret[2:])
	if err != nil {
		return nil, nil, err
	}

	sentPreMasterSecret := preMasterSecret
	if bad == RSABadValueTooLong {
		sentPreMasterSecret = make([]byte, len(sentPreMasterSecret)+1)
		copy(sentPreMasterSecret, preMasterSecret)
	} else if bad == RSABadValueTooShort {
		sentPreMasterSecret = sentPreMasterSecret[:len(sentPreMasterSecret)-1]
	}

	encrypted, err := rsa.EncryptPKCS1v15(config.rand(), cert.PublicKey.(*rsa.PublicKey), sentPreMasterSecret)
	if err != nil {
		return nil, nil, err
	}
	if bad == RSABadValueCorrupt {
		encrypted[len(encrypted)-1] ^= 1
		// Clear the high byte to ensure |encrypted| is still below the RSA modulus.
		encrypted[0] = 0
	}
	ckx := new(clientKeyExchangeMsg)
	if clientHello.vers != VersionSSL30 {
		ckx.ciphertext = make([]byte, len(encrypted)+2)
		ckx.ciphertext[0] = byte(len(encrypted) >> 8)
		ckx.ciphertext[1] = byte(len(encrypted))
		copy(ckx.ciphertext[2:], encrypted)
	} else {
		ckx.ciphertext = encrypted
	}
	return preMasterSecret, ckx, nil
}

func (ka *rsaKeyAgreement) peerSignatureAlgorithm() signatureAlgorithm {
	return 0
}

// A ecdhCurve is an instance of ECDH-style key agreement for TLS.
type ecdhCurve interface {
	// offer generates a keypair using rand. It returns the encoded |publicKey|.
	offer(rand io.Reader) (publicKey []byte, err error)

	// accept responds to the |peerKey| generated by |offer| with the acceptor's
	// |publicKey|, and returns agreed-upon |preMasterSecret| to the acceptor.
	accept(rand io.Reader, peerKey []byte) (publicKey []byte, preMasterSecret []byte, err error)

	// finish returns the computed |preMasterSecret|, given the |peerKey|
	// generated by |accept|.
	finish(peerKey []byte) (preMasterSecret []byte, err error)
}

// ellipticECDHCurve implements ecdhCurve with an elliptic.Curve.
type ellipticECDHCurve struct {
	curve      elliptic.Curve
	privateKey []byte
}

func (e *ellipticECDHCurve) offer(rand io.Reader) (publicKey []byte, err error) {
	var x, y *big.Int
	e.privateKey, x, y, err = elliptic.GenerateKey(e.curve, rand)
	if err != nil {
		return nil, err
	}
	return elliptic.Marshal(e.curve, x, y), nil
}

func (e *ellipticECDHCurve) accept(rand io.Reader, peerKey []byte) (publicKey []byte, preMasterSecret []byte, err error) {
	publicKey, err = e.offer(rand)
	if err != nil {
		return nil, nil, err
	}
	preMasterSecret, err = e.finish(peerKey)
	if err != nil {
		return nil, nil, err
	}
	return
}

func (e *ellipticECDHCurve) finish(peerKey []byte) (preMasterSecret []byte, err error) {
	x, y := elliptic.Unmarshal(e.curve, peerKey)
	if x == nil {
		return nil, errors.New("tls: invalid peer key")
	}
	x, _ = e.curve.ScalarMult(x, y, e.privateKey)
	preMasterSecret = make([]byte, (e.curve.Params().BitSize+7)>>3)
	xBytes := x.Bytes()
	copy(preMasterSecret[len(preMasterSecret)-len(xBytes):], xBytes)

	return preMasterSecret, nil
}

// x25519ECDHCurve implements ecdhCurve with X25519.
type x25519ECDHCurve struct {
	privateKey [32]byte
}

func (e *x25519ECDHCurve) offer(rand io.Reader) (publicKey []byte, err error) {
	_, err = io.ReadFull(rand, e.privateKey[:])
	if err != nil {
		return
	}
	var out [32]byte
	curve25519.ScalarBaseMult(&out, &e.privateKey)
	return out[:], nil
}

func (e *x25519ECDHCurve) accept(rand io.Reader, peerKey []byte) (publicKey []byte, preMasterSecret []byte, err error) {
	publicKey, err = e.offer(rand)
	if err != nil {
		return nil, nil, err
	}
	preMasterSecret, err = e.finish(peerKey)
	if err != nil {
		return nil, nil, err
	}
	return
}

func (e *x25519ECDHCurve) finish(peerKey []byte) (preMasterSecret []byte, err error) {
	if len(peerKey) != 32 {
		return nil, errors.New("tls: invalid peer key")
	}
	var out, peerKeyCopy [32]byte
	copy(peerKeyCopy[:], peerKey)
	curve25519.ScalarMult(&out, &e.privateKey, &peerKeyCopy)

	// Per RFC 7748, reject the all-zero value in constant time.
	var zeros [32]byte
	if subtle.ConstantTimeCompare(zeros[:], out[:]) == 1 {
		return nil, errors.New("tls: X25519 value with wrong order")
	}

	return out[:], nil
}

// cecpq1Curve is combined elliptic curve (X25519) and post-quantum (new hope) key
// agreement.
type cecpq1Curve struct {
	x25519  *x25519ECDHCurve
	newhope *newhope.Poly
}

func (e *cecpq1Curve) offer(rand io.Reader) (publicKey []byte, err error) {
	var x25519OfferMsg, newhopeOfferMsg []byte

	e.x25519 = new(x25519ECDHCurve)
	if x25519OfferMsg, err = e.x25519.offer(rand); err != nil {
		return nil, err
	}

	newhopeOfferMsg, e.newhope = newhope.Offer(rand)

	return append(x25519OfferMsg, newhopeOfferMsg[:]...), nil
}

func (e *cecpq1Curve) accept(rand io.Reader, peerKey []byte) (publicKey []byte, preMasterSecret []byte, err error) {
	if len(peerKey) != 32+newhope.OfferMsgLen {
		return nil, nil, errors.New("cecpq1: invalid offer message")
	}

	var x25519AcceptMsg, newhopeAcceptMsg []byte
	var x25519Secret []byte
	var newhopeSecret newhope.Key

	x25519 := new(x25519ECDHCurve)
	if x25519AcceptMsg, x25519Secret, err = x25519.accept(rand, peerKey[:32]); err != nil {
		return nil, nil, err
	}

	if newhopeSecret, newhopeAcceptMsg, err = newhope.Accept(rand, peerKey[32:]); err != nil {
		return nil, nil, err
	}

	return append(x25519AcceptMsg, newhopeAcceptMsg[:]...), append(x25519Secret, newhopeSecret[:]...), nil
}

func (e *cecpq1Curve) finish(peerKey []byte) (preMasterSecret []byte, err error) {
	if len(peerKey) != 32+newhope.AcceptMsgLen {
		return nil, errors.New("cecpq1: invalid accept message")
	}

	var x25519Secret []byte
	var newhopeSecret newhope.Key

	if x25519Secret, err = e.x25519.finish(peerKey[:32]); err != nil {
		return nil, err
	}

	if newhopeSecret, err = e.newhope.Finish(peerKey[32:]); err != nil {
		return nil, err
	}

	return append(x25519Secret, newhopeSecret[:]...), nil
}

func curveForCurveID(id CurveID) (ecdhCurve, bool) {
	switch id {
	case CurveP224:
		return &ellipticECDHCurve{curve: elliptic.P224()}, true
	case CurveP256:
		return &ellipticECDHCurve{curve: elliptic.P256()}, true
	case CurveP384:
		return &ellipticECDHCurve{curve: elliptic.P384()}, true
	case CurveP521:
		return &ellipticECDHCurve{curve: elliptic.P521()}, true
	case CurveX25519:
		return &x25519ECDHCurve{}, true
	default:
		return nil, false
	}

}

// keyAgreementAuthentication is a helper interface that specifies how
// to authenticate the ServerKeyExchange parameters.
type keyAgreementAuthentication interface {
	signParameters(config *Config, cert *Certificate, clientHello *clientHelloMsg, hello *serverHelloMsg, params []byte) (*serverKeyExchangeMsg, error)
	verifyParameters(config *Config, clientHello *clientHelloMsg, serverHello *serverHelloMsg, cert *x509.Certificate, params []byte, sig []byte) error
}

// nilKeyAgreementAuthentication does not authenticate the key
// agreement parameters.
type nilKeyAgreementAuthentication struct{}

func (ka *nilKeyAgreementAuthentication) signParameters(config *Config, cert *Certificate, clientHello *clientHelloMsg, hello *serverHelloMsg, params []byte) (*serverKeyExchangeMsg, error) {
	skx := new(serverKeyExchangeMsg)
	skx.key = params
	return skx, nil
}

func (ka *nilKeyAgreementAuthentication) verifyParameters(config *Config, clientHello *clientHelloMsg, serverHello *serverHelloMsg, cert *x509.Certificate, params []byte, sig []byte) error {
	return nil
}

// signedKeyAgreement signs the ServerKeyExchange parameters with the
// server's private key.
type signedKeyAgreement struct {
	keyType                keyType
	version                uint16
	peerSignatureAlgorithm signatureAlgorithm
}

func (ka *signedKeyAgreement) signParameters(config *Config, cert *Certificate, clientHello *clientHelloMsg, hello *serverHelloMsg, params []byte) (*serverKeyExchangeMsg, error) {
	// The message to be signed is prepended by the randoms.
	var msg []byte
	msg = append(msg, clientHello.random...)
	msg = append(msg, hello.random...)
	msg = append(msg, params...)

	var sigAlg signatureAlgorithm
	var err error
	if ka.version >= VersionTLS12 {
		sigAlg, err = selectSignatureAlgorithm(ka.version, cert.PrivateKey, config, clientHello.signatureAlgorithms)
		if err != nil {
			return nil, err
		}
	}

	sig, err := signMessage(ka.version, cert.PrivateKey, config, sigAlg, msg)
	if err != nil {
		return nil, err
	}
	if config.Bugs.SendSignatureAlgorithm != 0 {
		sigAlg = config.Bugs.SendSignatureAlgorithm
	}

	skx := new(serverKeyExchangeMsg)
	if config.Bugs.UnauthenticatedECDH {
		skx.key = params
	} else {
		sigAlgsLen := 0
		if ka.version >= VersionTLS12 {
			sigAlgsLen = 2
		}
		skx.key = make([]byte, len(params)+sigAlgsLen+2+len(sig))
		copy(skx.key, params)
		k := skx.key[len(params):]
		if ka.version >= VersionTLS12 {
			k[0] = byte(sigAlg >> 8)
			k[1] = byte(sigAlg)
			k = k[2:]
		}
		k[0] = byte(len(sig) >> 8)
		k[1] = byte(len(sig))
		copy(k[2:], sig)
	}

	return skx, nil
}

func (ka *signedKeyAgreement) verifyParameters(config *Config, clientHello *clientHelloMsg, serverHello *serverHelloMsg, cert *x509.Certificate, params []byte, sig []byte) error {
	// The peer's key must match the cipher type.
	switch ka.keyType {
	case keyTypeECDSA:
		_, ok := cert.PublicKey.(*ecdsa.PublicKey)
		if !ok {
			return errors.New("tls: ECDHE ECDSA requires a ECDSA server public key")
		}
	case keyTypeRSA:
		_, ok := cert.PublicKey.(*rsa.PublicKey)
		if !ok {
			return errors.New("tls: ECDHE RSA requires a RSA server public key")
		}
	default:
		return errors.New("tls: unknown key type")
	}

	// The message to be signed is prepended by the randoms.
	var msg []byte
	msg = append(msg, clientHello.random...)
	msg = append(msg, serverHello.random...)
	msg = append(msg, params...)

	var sigAlg signatureAlgorithm
	if ka.version >= VersionTLS12 {
		if len(sig) < 2 {
			return errServerKeyExchange
		}
		sigAlg = signatureAlgorithm(sig[0])<<8 | signatureAlgorithm(sig[1])
		sig = sig[2:]
		// Stash the signature algorithm to be extracted by the handshake.
		ka.peerSignatureAlgorithm = sigAlg
	}

	if len(sig) < 2 {
		return errServerKeyExchange
	}
	sigLen := int(sig[0])<<8 | int(sig[1])
	if sigLen+2 != len(sig) {
		return errServerKeyExchange
	}
	sig = sig[2:]

	return verifyMessage(ka.version, cert.PublicKey, config, sigAlg, msg, sig)
}

// ecdheKeyAgreement implements a TLS key agreement where the server
// generates a ephemeral EC public/private key pair and signs it. The
// pre-master secret is then calculated using ECDH. The signature may
// either be ECDSA or RSA.
type ecdheKeyAgreement struct {
	auth    keyAgreementAuthentication
	curve   ecdhCurve
	curveID CurveID
	peerKey []byte
}

func (ka *ecdheKeyAgreement) generateServerKeyExchange(config *Config, cert *Certificate, clientHello *clientHelloMsg, hello *serverHelloMsg) (*serverKeyExchangeMsg, error) {
	var curveid CurveID
	preferredCurves := config.curvePreferences()

NextCandidate:
	for _, candidate := range preferredCurves {
		for _, c := range clientHello.supportedCurves {
			if candidate == c {
				curveid = c
				break NextCandidate
			}
		}
	}

	if curveid == 0 {
		return nil, errors.New("tls: no supported elliptic curves offered")
	}

	var ok bool
	if ka.curve, ok = curveForCurveID(curveid); !ok {
		return nil, errors.New("tls: preferredCurves includes unsupported curve")
	}
	ka.curveID = curveid

	publicKey, err := ka.curve.offer(config.rand())
	if err != nil {
		return nil, err
	}

	// http://tools.ietf.org/html/rfc4492#section-5.4
	serverECDHParams := make([]byte, 1+2+1+len(publicKey))
	serverECDHParams[0] = 3 // named curve
	if config.Bugs.SendCurve != 0 {
		curveid = config.Bugs.SendCurve
	}
	serverECDHParams[1] = byte(curveid >> 8)
	serverECDHParams[2] = byte(curveid)
	serverECDHParams[3] = byte(len(publicKey))
	copy(serverECDHParams[4:], publicKey)
	if config.Bugs.InvalidECDHPoint {
		serverECDHParams[4] ^= 0xff
	}

	return ka.auth.signParameters(config, cert, clientHello, hello, serverECDHParams)
}

func (ka *ecdheKeyAgreement) processClientKeyExchange(config *Config, cert *Certificate, ckx *clientKeyExchangeMsg, version uint16) ([]byte, error) {
	if len(ckx.ciphertext) == 0 || int(ckx.ciphertext[0]) != len(ckx.ciphertext)-1 {
		return nil, errClientKeyExchange
	}
	return ka.curve.finish(ckx.ciphertext[1:])
}

func (ka *ecdheKeyAgreement) processServerKeyExchange(config *Config, clientHello *clientHelloMsg, serverHello *serverHelloMsg, cert *x509.Certificate, skx *serverKeyExchangeMsg) error {
	if len(skx.key) < 4 {
		return errServerKeyExchange
	}
	if skx.key[0] != 3 { // named curve
		return errors.New("tls: server selected unsupported curve")
	}
	curveid := CurveID(skx.key[1])<<8 | CurveID(skx.key[2])
	ka.curveID = curveid

	var ok bool
	if ka.curve, ok = curveForCurveID(curveid); !ok {
		return errors.New("tls: server selected unsupported curve")
	}

	publicLen := int(skx.key[3])
	if publicLen+4 > len(skx.key) {
		return errServerKeyExchange
	}
	// Save the peer key for later.
	ka.peerKey = skx.key[4 : 4+publicLen]

	// Check the signature.
	serverECDHParams := skx.key[:4+publicLen]
	sig := skx.key[4+publicLen:]
	return ka.auth.verifyParameters(config, clientHello, serverHello, cert, serverECDHParams, sig)
}

func (ka *ecdheKeyAgreement) generateClientKeyExchange(config *Config, clientHello *clientHelloMsg, cert *x509.Certificate) ([]byte, *clientKeyExchangeMsg, error) {
	if ka.curve == nil {
		return nil, nil, errors.New("missing ServerKeyExchange message")
	}

	publicKey, preMasterSecret, err := ka.curve.accept(config.rand(), ka.peerKey)
	if err != nil {
		return nil, nil, err
	}

	ckx := new(clientKeyExchangeMsg)
	ckx.ciphertext = make([]byte, 1+len(publicKey))
	ckx.ciphertext[0] = byte(len(publicKey))
	copy(ckx.ciphertext[1:], publicKey)
	if config.Bugs.InvalidECDHPoint {
		ckx.ciphertext[1] ^= 0xff
	}

	return preMasterSecret, ckx, nil
}

func (ka *ecdheKeyAgreement) peerSignatureAlgorithm() signatureAlgorithm {
	if auth, ok := ka.auth.(*signedKeyAgreement); ok {
		return auth.peerSignatureAlgorithm
	}
	return 0
}

// cecpq1RSAKeyAgreement is like an ecdheKeyAgreement, but using the cecpq1Curve
// pseudo-curve, and without any parameters (e.g. curve name) other than the
// keys being exchanged. The signature may either be ECDSA or RSA.
type cecpq1KeyAgreement struct {
	auth    keyAgreementAuthentication
	curve   ecdhCurve
	peerKey []byte
}

func (ka *cecpq1KeyAgreement) generateServerKeyExchange(config *Config, cert *Certificate, clientHello *clientHelloMsg, hello *serverHelloMsg) (*serverKeyExchangeMsg, error) {
	ka.curve = &cecpq1Curve{}
	publicKey, err := ka.curve.offer(config.rand())
	if err != nil {
		return nil, err
	}

	if config.Bugs.CECPQ1BadX25519Part {
		publicKey[0] ^= 1
	}
	if config.Bugs.CECPQ1BadNewhopePart {
		publicKey[32] ^= 1
		publicKey[33] ^= 1
		publicKey[34] ^= 1
		publicKey[35] ^= 1
	}

	var params []byte
	params = append(params, byte(len(publicKey)>>8))
	params = append(params, byte(len(publicKey)&0xff))
	params = append(params, publicKey[:]...)

	return ka.auth.signParameters(config, cert, clientHello, hello, params)
}

func (ka *cecpq1KeyAgreement) processClientKeyExchange(config *Config, cert *Certificate, ckx *clientKeyExchangeMsg, version uint16) ([]byte, error) {
	if len(ckx.ciphertext) < 2 {
		return nil, errClientKeyExchange
	}
	peerKeyLen := int(ckx.ciphertext[0])<<8 + int(ckx.ciphertext[1])
	peerKey := ckx.ciphertext[2:]
	if peerKeyLen != len(peerKey) {
		return nil, errClientKeyExchange
	}
	return ka.curve.finish(peerKey)
}

func (ka *cecpq1KeyAgreement) processServerKeyExchange(config *Config, clientHello *clientHelloMsg, serverHello *serverHelloMsg, cert *x509.Certificate, skx *serverKeyExchangeMsg) error {
	if len(skx.key) < 2 {
		return errServerKeyExchange
	}
	peerKeyLen := int(skx.key[0])<<8 + int(skx.key[1])
	// Save the peer key for later.
	if len(skx.key) < 2+peerKeyLen {
		return errServerKeyExchange
	}
	ka.peerKey = skx.key[2 : 2+peerKeyLen]
	if peerKeyLen != len(ka.peerKey) {
		return errServerKeyExchange
	}

	// Check the signature.
	params := skx.key[:2+peerKeyLen]
	sig := skx.key[2+peerKeyLen:]
	return ka.auth.verifyParameters(config, clientHello, serverHello, cert, params, sig)
}

func (ka *cecpq1KeyAgreement) generateClientKeyExchange(config *Config, clientHello *clientHelloMsg, cert *x509.Certificate) ([]byte, *clientKeyExchangeMsg, error) {
	curve := &cecpq1Curve{}
	publicKey, preMasterSecret, err := curve.accept(config.rand(), ka.peerKey)
	if err != nil {
		return nil, nil, err
	}

	if config.Bugs.CECPQ1BadX25519Part {
		publicKey[0] ^= 1
	}
	if config.Bugs.CECPQ1BadNewhopePart {
		publicKey[32] ^= 1
		publicKey[33] ^= 1
		publicKey[34] ^= 1
		publicKey[35] ^= 1
	}

	ckx := new(clientKeyExchangeMsg)
	ckx.ciphertext = append(ckx.ciphertext, byte(len(publicKey)>>8))
	ckx.ciphertext = append(ckx.ciphertext, byte(len(publicKey)&0xff))
	ckx.ciphertext = append(ckx.ciphertext, publicKey[:]...)

	return preMasterSecret, ckx, nil
}

func (ka *cecpq1KeyAgreement) peerSignatureAlgorithm() signatureAlgorithm {
	if auth, ok := ka.auth.(*signedKeyAgreement); ok {
		return auth.peerSignatureAlgorithm
	}
	return 0
}

// dheRSAKeyAgreement implements a TLS key agreement where the server generates
// an ephemeral Diffie-Hellman public/private key pair and signs it. The
// pre-master secret is then calculated using Diffie-Hellman.
type dheKeyAgreement struct {
	auth    keyAgreementAuthentication
	p, g    *big.Int
	yTheirs *big.Int
	xOurs   *big.Int
}

func (ka *dheKeyAgreement) generateServerKeyExchange(config *Config, cert *Certificate, clientHello *clientHelloMsg, hello *serverHelloMsg) (*serverKeyExchangeMsg, error) {
	var q *big.Int
	if p := config.Bugs.DHGroupPrime; p != nil {
		ka.p = p
		ka.g = big.NewInt(2)
		q = p
	} else {
		// 2048-bit MODP Group with 256-bit Prime Order Subgroup (RFC
		// 5114, Section 2.3)
		ka.p, _ = new(big.Int).SetString("87A8E61DB4B6663CFFBBD19C651959998CEEF608660DD0F25D2CEED4435E3B00E00DF8F1D61957D4FAF7DF4561B2AA3016C3D91134096FAA3BF4296D830E9A7C209E0C6497517ABD5A8A9D306BCF67ED91F9E6725B4758C022E0B1EF4275BF7B6C5BFC11D45F9088B941F54EB1E59BB8BC39A0BF12307F5C4FDB70C581B23F76B63ACAE1CAA6B7902D52526735488A0EF13C6D9A51BFA4AB3AD8347796524D8EF6A167B5A41825D967E144E5140564251CCACB83E6B486F6B3CA3F7971506026C0B857F689962856DED4010ABD0BE621C3A3960A54E710C375F26375D7014103A4B54330C198AF126116D2276E11715F693877FAD7EF09CADB094AE91E1A1597", 16)
		ka.g, _ = new(big.Int).SetString("3FB32C9B73134D0B2E77506660EDBD484CA7B18F21EF205407F4793A1A0BA12510DBC15077BE463FFF4FED4AAC0BB555BE3A6C1B0C6B47B1BC3773BF7E8C6F62901228F8C28CBB18A55AE31341000A650196F931C77A57F2DDF463E5E9EC144B777DE62AAAB8A8628AC376D282D6ED3864E67982428EBC831D14348F6F2F9193B5045AF2767164E1DFC967C1FB3F2E55A4BD1BFFE83B9C80D052B985D182EA0ADB2A3B7313D3FE14C8484B1E052588B9B7D2BBD2DF016199ECD06E1557CD0915B3353BBB64E0EC377FD028370DF92B52C7891428CDC67EB6184B523D1DB246C32F63078490F00EF8D647D148D47954515E2327CFEF98C582664B4C0F6CC41659", 16)
		q, _ = new(big.Int).SetString("8CF83642A709A097B447997640129DA299B1A47D1EB3750BA308B0FE64F5FBD3", 16)
	}

	var err error
	ka.xOurs, err = rand.Int(config.rand(), q)
	if err != nil {
		return nil, err
	}
	yOurs := new(big.Int).Exp(ka.g, ka.xOurs, ka.p)

	// http://tools.ietf.org/html/rfc5246#section-7.4.3
	pBytes := ka.p.Bytes()
	gBytes := ka.g.Bytes()
	yBytes := yOurs.Bytes()
	serverDHParams := make([]byte, 0, 2+len(pBytes)+2+len(gBytes)+2+len(yBytes))
	serverDHParams = append(serverDHParams, byte(len(pBytes)>>8), byte(len(pBytes)))
	serverDHParams = append(serverDHParams, pBytes...)
	serverDHParams = append(serverDHParams, byte(len(gBytes)>>8), byte(len(gBytes)))
	serverDHParams = append(serverDHParams, gBytes...)
	serverDHParams = append(serverDHParams, byte(len(yBytes)>>8), byte(len(yBytes)))
	serverDHParams = append(serverDHParams, yBytes...)

	return ka.auth.signParameters(config, cert, clientHello, hello, serverDHParams)
}

func (ka *dheKeyAgreement) processClientKeyExchange(config *Config, cert *Certificate, ckx *clientKeyExchangeMsg, version uint16) ([]byte, error) {
	if len(ckx.ciphertext) < 2 {
		return nil, errClientKeyExchange
	}
	yLen := (int(ckx.ciphertext[0]) << 8) | int(ckx.ciphertext[1])
	if yLen != len(ckx.ciphertext)-2 {
		return nil, errClientKeyExchange
	}
	yTheirs := new(big.Int).SetBytes(ckx.ciphertext[2:])
	if yTheirs.Sign() <= 0 || yTheirs.Cmp(ka.p) >= 0 {
		return nil, errClientKeyExchange
	}
	return new(big.Int).Exp(yTheirs, ka.xOurs, ka.p).Bytes(), nil
}

func (ka *dheKeyAgreement) processServerKeyExchange(config *Config, clientHello *clientHelloMsg, serverHello *serverHelloMsg, cert *x509.Certificate, skx *serverKeyExchangeMsg) error {
	// Read dh_p
	k := skx.key
	if len(k) < 2 {
		return errServerKeyExchange
	}
	pLen := (int(k[0]) << 8) | int(k[1])
	k = k[2:]
	if len(k) < pLen {
		return errServerKeyExchange
	}
	ka.p = new(big.Int).SetBytes(k[:pLen])
	k = k[pLen:]

	// Read dh_g
	if len(k) < 2 {
		return errServerKeyExchange
	}
	gLen := (int(k[0]) << 8) | int(k[1])
	k = k[2:]
	if len(k) < gLen {
		return errServerKeyExchange
	}
	ka.g = new(big.Int).SetBytes(k[:gLen])
	k = k[gLen:]

	// Read dh_Ys
	if len(k) < 2 {
		return errServerKeyExchange
	}
	yLen := (int(k[0]) << 8) | int(k[1])
	k = k[2:]
	if len(k) < yLen {
		return errServerKeyExchange
	}
	ka.yTheirs = new(big.Int).SetBytes(k[:yLen])
	k = k[yLen:]
	if ka.yTheirs.Sign() <= 0 || ka.yTheirs.Cmp(ka.p) >= 0 {
		return errServerKeyExchange
	}

	if l := config.Bugs.RequireDHPublicValueLen; l != 0 && l != yLen {
		return fmt.Errorf("RequireDHPublicValueLen set to %d, but server's public value was %d bytes on the wire and %d bytes if minimal", l, yLen, (ka.yTheirs.BitLen()+7)/8)
	}

	sig := k
	serverDHParams := skx.key[:len(skx.key)-len(sig)]

	return ka.auth.verifyParameters(config, clientHello, serverHello, cert, serverDHParams, sig)
}

func (ka *dheKeyAgreement) generateClientKeyExchange(config *Config, clientHello *clientHelloMsg, cert *x509.Certificate) ([]byte, *clientKeyExchangeMsg, error) {
	if ka.p == nil || ka.g == nil || ka.yTheirs == nil {
		return nil, nil, errors.New("missing ServerKeyExchange message")
	}

	xOurs, err := rand.Int(config.rand(), ka.p)
	if err != nil {
		return nil, nil, err
	}
	preMasterSecret := new(big.Int).Exp(ka.yTheirs, xOurs, ka.p).Bytes()

	yOurs := new(big.Int).Exp(ka.g, xOurs, ka.p)
	yBytes := yOurs.Bytes()
	ckx := new(clientKeyExchangeMsg)
	ckx.ciphertext = make([]byte, 2+len(yBytes))
	ckx.ciphertext[0] = byte(len(yBytes) >> 8)
	ckx.ciphertext[1] = byte(len(yBytes))
	copy(ckx.ciphertext[2:], yBytes)

	return preMasterSecret, ckx, nil
}

func (ka *dheKeyAgreement) peerSignatureAlgorithm() signatureAlgorithm {
	if auth, ok := ka.auth.(*signedKeyAgreement); ok {
		return auth.peerSignatureAlgorithm
	}
	return 0
}

// nilKeyAgreement is a fake key agreement used to implement the plain PSK key
// exchange.
type nilKeyAgreement struct{}

func (ka *nilKeyAgreement) generateServerKeyExchange(config *Config, cert *Certificate, clientHello *clientHelloMsg, hello *serverHelloMsg) (*serverKeyExchangeMsg, error) {
	return nil, nil
}

func (ka *nilKeyAgreement) processClientKeyExchange(config *Config, cert *Certificate, ckx *clientKeyExchangeMsg, version uint16) ([]byte, error) {
	if len(ckx.ciphertext) != 0 {
		return nil, errClientKeyExchange
	}

	// Although in plain PSK, otherSecret is all zeros, the base key
	// agreement does not access to the length of the pre-shared
	// key. pskKeyAgreement instead interprets nil to mean to use all zeros
	// of the appropriate length.
	return nil, nil
}

func (ka *nilKeyAgreement) processServerKeyExchange(config *Config, clientHello *clientHelloMsg, serverHello *serverHelloMsg, cert *x509.Certificate, skx *serverKeyExchangeMsg) error {
	if len(skx.key) != 0 {
		return errServerKeyExchange
	}
	return nil
}

func (ka *nilKeyAgreement) generateClientKeyExchange(config *Config, clientHello *clientHelloMsg, cert *x509.Certificate) ([]byte, *clientKeyExchangeMsg, error) {
	// Although in plain PSK, otherSecret is all zeros, the base key
	// agreement does not access to the length of the pre-shared
	// key. pskKeyAgreement instead interprets nil to mean to use all zeros
	// of the appropriate length.
	return nil, &clientKeyExchangeMsg{}, nil
}

func (ka *nilKeyAgreement) peerSignatureAlgorithm() signatureAlgorithm {
	return 0
}

// makePSKPremaster formats a PSK pre-master secret based on otherSecret from
// the base key exchange and psk.
func makePSKPremaster(otherSecret, psk []byte) []byte {
	out := make([]byte, 0, 2+len(otherSecret)+2+len(psk))
	out = append(out, byte(len(otherSecret)>>8), byte(len(otherSecret)))
	out = append(out, otherSecret...)
	out = append(out, byte(len(psk)>>8), byte(len(psk)))
	out = append(out, psk...)
	return out
}

// pskKeyAgreement implements the PSK key agreement.
type pskKeyAgreement struct {
	base         keyAgreement
	identityHint string
}

func (ka *pskKeyAgreement) generateServerKeyExchange(config *Config, cert *Certificate, clientHello *clientHelloMsg, hello *serverHelloMsg) (*serverKeyExchangeMsg, error) {
	// Assemble the identity hint.
	bytes := make([]byte, 2+len(config.PreSharedKeyIdentity))
	bytes[0] = byte(len(config.PreSharedKeyIdentity) >> 8)
	bytes[1] = byte(len(config.PreSharedKeyIdentity))
	copy(bytes[2:], []byte(config.PreSharedKeyIdentity))

	// If there is one, append the base key agreement's
	// ServerKeyExchange.
	baseSkx, err := ka.base.generateServerKeyExchange(config, cert, clientHello, hello)
	if err != nil {
		return nil, err
	}

	if baseSkx != nil {
		bytes = append(bytes, baseSkx.key...)
	} else if config.PreSharedKeyIdentity == "" && !config.Bugs.AlwaysSendPreSharedKeyIdentityHint {
		// ServerKeyExchange is optional if the identity hint is empty
		// and there would otherwise be no ServerKeyExchange.
		return nil, nil
	}

	skx := new(serverKeyExchangeMsg)
	skx.key = bytes
	return skx, nil
}

func (ka *pskKeyAgreement) processClientKeyExchange(config *Config, cert *Certificate, ckx *clientKeyExchangeMsg, version uint16) ([]byte, error) {
	// First, process the PSK identity.
	if len(ckx.ciphertext) < 2 {
		return nil, errClientKeyExchange
	}
	identityLen := (int(ckx.ciphertext[0]) << 8) | int(ckx.ciphertext[1])
	if 2+identityLen > len(ckx.ciphertext) {
		return nil, errClientKeyExchange
	}
	identity := string(ckx.ciphertext[2 : 2+identityLen])

	if identity != config.PreSharedKeyIdentity {
		return nil, errors.New("tls: unexpected identity")
	}

	if config.PreSharedKey == nil {
		return nil, errors.New("tls: pre-shared key not configured")
	}

	// Process the remainder of the ClientKeyExchange to compute the base
	// pre-master secret.
	newCkx := new(clientKeyExchangeMsg)
	newCkx.ciphertext = ckx.ciphertext[2+identityLen:]
	otherSecret, err := ka.base.processClientKeyExchange(config, cert, newCkx, version)
	if err != nil {
		return nil, err
	}

	if otherSecret == nil {
		// Special-case for the plain PSK key exchanges.
		otherSecret = make([]byte, len(config.PreSharedKey))
	}
	return makePSKPremaster(otherSecret, config.PreSharedKey), nil
}

func (ka *pskKeyAgreement) processServerKeyExchange(config *Config, clientHello *clientHelloMsg, serverHello *serverHelloMsg, cert *x509.Certificate, skx *serverKeyExchangeMsg) error {
	if len(skx.key) < 2 {
		return errServerKeyExchange
	}
	identityLen := (int(skx.key[0]) << 8) | int(skx.key[1])
	if 2+identityLen > len(skx.key) {
		return errServerKeyExchange
	}
	ka.identityHint = string(skx.key[2 : 2+identityLen])

	// Process the remainder of the ServerKeyExchange.
	newSkx := new(serverKeyExchangeMsg)
	newSkx.key = skx.key[2+identityLen:]
	return ka.base.processServerKeyExchange(config, clientHello, serverHello, cert, newSkx)
}

func (ka *pskKeyAgreement) generateClientKeyExchange(config *Config, clientHello *clientHelloMsg, cert *x509.Certificate) ([]byte, *clientKeyExchangeMsg, error) {
	// The server only sends an identity hint but, for purposes of
	// test code, the server always sends the hint and it is
	// required to match.
	if ka.identityHint != config.PreSharedKeyIdentity {
		return nil, nil, errors.New("tls: unexpected identity")
	}

	// Serialize the identity.
	bytes := make([]byte, 2+len(config.PreSharedKeyIdentity))
	bytes[0] = byte(len(config.PreSharedKeyIdentity) >> 8)
	bytes[1] = byte(len(config.PreSharedKeyIdentity))
	copy(bytes[2:], []byte(config.PreSharedKeyIdentity))

	// Append the base key exchange's ClientKeyExchange.
	otherSecret, baseCkx, err := ka.base.generateClientKeyExchange(config, clientHello, cert)
	if err != nil {
		return nil, nil, err
	}
	ckx := new(clientKeyExchangeMsg)
	ckx.ciphertext = append(bytes, baseCkx.ciphertext...)

	if config.PreSharedKey == nil {
		return nil, nil, errors.New("tls: pre-shared key not configured")
	}
	if otherSecret == nil {
		otherSecret = make([]byte, len(config.PreSharedKey))
	}
	return makePSKPremaster(otherSecret, config.PreSharedKey), ckx, nil
}

func (ka *pskKeyAgreement) peerSignatureAlgorithm() signatureAlgorithm {
	return 0
}
