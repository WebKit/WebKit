// Copyright 2026 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package runner

import (
	"crypto/sha256"
	"fmt"
	"slices"
	"strconv"
)

const certTypeBogus CertificateType = 5

var (
	certTypesListRPKOnly     = []CertificateType{certTypeRawPublicKey}
	certTypesListRPKX509     = []CertificateType{certTypeRawPublicKey, certTypeX509}
	certTypesListX509RPK     = []CertificateType{certTypeX509, certTypeRawPublicKey}
	certTypesListX509Only    = []CertificateType{certTypeX509}
	certTypesListUnknown     = []CertificateType{certTypeBogus}
	certTypesListUnknownX509 = []CertificateType{certTypeX509, certTypeBogus}
	certTypesListRPKUnknown  = []CertificateType{certTypeRawPublicKey, certTypeBogus}
)

// A malformed RPK credential which is sent on the wire as an empty SubjectPublicKeyInfo.
var rpkEmpty = Credential{
	Type:        CredentialTypeRawPublicKey,
	Certificate: [][]byte{{}},
}

func addRPKCustomVerifyToFlags(peerCredential *Credential, shimFlags []string) []string {
	if peerCredential.Type == CredentialTypeRawPublicKey {
		peerRPKHash := sha256.Sum256(peerCredential.Certificate[0])
		shimFlags = append(shimFlags, "-expect-peer-rpk-sha256", base64FlagValue(peerRPKHash[:]))
	}
	return shimFlags
}

func addServerCertTypeTests() {
	for _, ver := range allVersions(tls) {
		// Tests sending list of accepted server cert types in the ClientHello and
		// receiving the server's negotiated cert type and credential.
		for _, test := range []struct {
			name                         string
			serverCertTypesAccepted      []CertificateType
			expectedClientHelloExtension []CertificateType
			serverHelloExtension         []CertificateType
			serverCredential             *Credential
			expectedError                string
			expectedLocalError           string
		}{
			{
				name:                         "RPKOnly-NegotiatedRPK",
				serverCertTypesAccepted:      certTypesListRPKOnly,
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverHelloExtension:         certTypesListRPKOnly,
				serverCredential:             &rpkEcdsaP256,
			},
			{
				name:                         "RPKX509-NegotiatedRPK",
				serverCertTypesAccepted:      certTypesListRPKX509,
				expectedClientHelloExtension: certTypesListRPKX509,
				serverHelloExtension:         certTypesListRPKOnly,
				serverCredential:             &rpkEcdsaP256,
			},
			{
				name:                         "RPKX509-NegotiatedX509",
				serverCertTypesAccepted:      certTypesListRPKX509,
				expectedClientHelloExtension: certTypesListRPKX509,
				serverHelloExtension:         certTypesListX509Only,
				serverCredential:             &ecdsaP256Certificate,
			},
			{
				name:                         "RPKX509-NegotiatedX509ByDefault",
				serverCertTypesAccepted:      certTypesListRPKX509,
				expectedClientHelloExtension: certTypesListRPKX509,
				serverHelloExtension:         nil,
				serverCredential:             &ecdsaP256Certificate,
			},
			{
				name:                         "X509RPK-NegotiatedRPK",
				serverCertTypesAccepted:      certTypesListX509RPK,
				expectedClientHelloExtension: certTypesListX509RPK,
				serverHelloExtension:         certTypesListRPKOnly,
				serverCredential:             &rpkEcdsaP256,
			},
			{
				name:                         "X509RPK-NegotiatedX509",
				serverCertTypesAccepted:      certTypesListX509RPK,
				expectedClientHelloExtension: certTypesListX509RPK,
				serverHelloExtension:         certTypesListX509Only,
				serverCredential:             &ecdsaP256Certificate,
			},
			{
				name:                         "X509RPK-NegotiatedX509ByDefault",
				serverCertTypesAccepted:      certTypesListX509RPK,
				expectedClientHelloExtension: certTypesListX509RPK,
				serverHelloExtension:         nil,
				serverCredential:             &ecdsaP256Certificate,
			},
			{
				// Configuring the default cert type only omits the extension.
				name:                         "DefaultOnly-Omitted",
				serverCertTypesAccepted:      certTypesListX509Only,
				expectedClientHelloExtension: []CertificateType{},
				serverHelloExtension:         nil,
				serverCredential:             &ecdsaP256Certificate,
			},
			{
				// Server picked RPK despite client not supporting RPKs. Client should
				// reject the unsolicited extension.
				name:                         "DefaultOnly-ServerPickedRPKInError",
				serverCertTypesAccepted:      certTypesListX509Only,
				expectedClientHelloExtension: []CertificateType{},
				serverHelloExtension:         certTypesListRPKOnly,
				serverCredential:             &rpkEcdsaP256,
				expectedError:                ":UNEXPECTED_EXTENSION:",
			},
			{
				// Server picked X.509 despite client not supporting X.509. Client
				// should reject the invalid extension.
				name:                         "RPKOnly-ServerPickedX509InError",
				serverCertTypesAccepted:      certTypesListRPKOnly,
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverHelloExtension:         certTypesListX509Only,
				serverCredential:             &ecdsaP256Certificate,
				expectedError:                ":UNSUPPORTED_CERTIFICATE:",
			},
			{
				// Server picked X.509 despite client not supporting X.509. Client
				// should reject.
				name:                         "RPKOnly-ServerPickedX509ByDefaultInError",
				serverCertTypesAccepted:      certTypesListRPKOnly,
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverHelloExtension:         nil,
				serverCredential:             &ecdsaP256Certificate,
				expectedError:                ":UNSUPPORTED_CERTIFICATE:",
			},
			{
				// Server sent an X.509 cert despite negotiating RPK. It should fail to
				// parse.
				name:                         "RPKOnly-NegotiatedRPK-ServerIncorrectlySentX509",
				serverCertTypesAccepted:      certTypesListRPKOnly,
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverHelloExtension:         certTypesListRPKOnly,
				serverCredential:             &ecdsaP256Certificate,
				expectedError:                ":DECODE_ERROR:",
			},
			{
				// Server sent a RPK despite negotiating X.509 (explicitly). It should
				// fail to parse.
				name:                         "RPKX509-NegotiatedX509-ServerIncorrectlySentRPK",
				serverCertTypesAccepted:      certTypesListRPKX509,
				expectedClientHelloExtension: certTypesListRPKX509,
				serverHelloExtension:         certTypesListX509Only,
				serverCredential:             &rpkEcdsaP256,
				expectedLocalError:           "remote error: error decoding message",
			},
			{
				// Server sent a RPK despite negotiating X.509 (by default). It should
				// fail to parse.
				name:                         "RPKX509-NegotiatedX509ByDefault-ServerIncorrectlySentRPK",
				serverCertTypesAccepted:      certTypesListRPKX509,
				expectedClientHelloExtension: certTypesListRPKX509,
				serverHelloExtension:         nil,
				serverCredential:             &rpkEcdsaP256,
				expectedLocalError:           "remote error: error decoding message",
			},
		} {
			shimFlags := flagCertTypes("-accepted-peer-cert-types", test.serverCertTypesAccepted)
			if test.expectedError == "" {
				shimFlags = append(shimFlags, "-expect-peer-certificate-type", strconv.Itoa(int(test.serverCredential.Type.CertificateType())))
				shimFlags = addRPKCustomVerifyToFlags(test.serverCredential, shimFlags)
			}
			testCases = append(testCases, testCase{
				testType: clientTest,
				name:     fmt.Sprintf("ServerCertificateType-Client-Requests%s-%s", test.name, ver.name),
				config: Config{
					MinVersion: ver.version,
					MaxVersion: ver.version,
					Credential: test.serverCredential,
					Bugs: ProtocolBugs{
						ExpectServerCertificateTypes: test.expectedClientHelloExtension,
						SendServerCertificateTypes:   test.serverHelloExtension,
					},
				},
				flags:              shimFlags,
				shouldFail:         test.expectedError != "" || test.expectedLocalError != "",
				expectedError:      test.expectedError,
				expectedLocalError: test.expectedLocalError,
				resumeSession:      test.expectedError == "" && test.expectedLocalError == "",
			})
		}
		shimFlags := flagCertTypes("-accepted-peer-cert-types", certTypesListRPKOnly)
		// The Certificate message contains an RPK with an empty SPKI. In TLS 1.2
		// this is considered to indicate the lack of a certificate (so the client
		// will reject), whereas this is illegal for the TLS 1.3 RPK Certificate
		// format.
		expectedError := ":INVALID_RAW_PUBLIC_KEY:"
		if ver.version <= VersionTLS12 {
			// Parsing the Certificate failed to yield a credential.
			expectedError = ":DECODE_ERROR:"
		}
		testCases = append(testCases, testCase{
			testType: clientTest,
			name:     fmt.Sprintf("ServerCertificateType-Client-RequestsRPKOnly-ServerSentEmptyRPK-%s", ver.name),
			config: Config{
				MinVersion: ver.version,
				MaxVersion: ver.version,
				Credential: &rpkEmpty,
				Bugs: ProtocolBugs{
					ExpectServerCertificateTypes: certTypesListRPKOnly,
					SendServerCertificateTypes:   certTypesListRPKOnly,
				},
			},
			flags:         shimFlags,
			shouldFail:    true,
			expectedError: expectedError,
		})
		if ver.version >= VersionTLS13 {
			// If the server sends a Certificate message for an RPK that contains an
			// empty certificate list, client should reject. (For TLS 1.2 and below,
			// this test would be equivalent to the one above.)
			expectedError = ":PEER_DID_NOT_RETURN_A_CERTIFICATE:"
			testCases = append(testCases, testCase{
				testType: clientTest,
				name:     fmt.Sprintf("ServerCertificateType-Client-RequestsRPKOnly-ServerSentEmptyCertificateList-%s", ver.name),
				config: Config{
					MinVersion: ver.version,
					MaxVersion: ver.version,
					Credential: &rpkEcdsaP256,
					Bugs: ProtocolBugs{
						ExpectServerCertificateTypes: certTypesListRPKOnly,
						SendServerCertificateTypes:   certTypesListRPKOnly,
						EmptyCertificateList:         true,
						SkipCertificateVerify:        true,
					},
				},
				flags:         shimFlags,
				shouldFail:    true,
				expectedError: expectedError,
			})
			// If the server sends an otherwise valid Certificate message with an RPK,
			// but does not send CertificateVerify, client should reject.
			shimFlags = addRPKCustomVerifyToFlags(&rpkEcdsaP256, shimFlags)
			testCases = append(testCases, testCase{
				testType: clientTest,
				name:     fmt.Sprintf("ServerCertificateType-Client-RPKWithoutCertificateVerify-%s", ver.name),
				config: Config{
					MinVersion: ver.version,
					MaxVersion: ver.version,
					Credential: &rpkEcdsaP256,
					Bugs: ProtocolBugs{
						ExpectServerCertificateTypes: certTypesListRPKOnly,
						SendServerCertificateTypes:   certTypesListRPKOnly,
						SkipCertificateVerify:        true,
					},
				},
				flags:              shimFlags,
				shouldFail:         true,
				expectedLocalError: "remote error: unexpected message",
			})
		}
		// Test that RPK server cert verification fails if we force it to fail.
		shimFlags = append([]string{"-verify-fail"},
			flagCertTypes("-accepted-peer-cert-types", certTypesListRPKOnly)...)
		testCases = append(testCases, testCase{
			testType: clientTest,
			name:     fmt.Sprintf("ServerCertificateType-Client-RPKVerifyFail-%s", ver.name),
			config: Config{
				MinVersion: ver.version,
				MaxVersion: ver.version,
				Credential: &rpkEcdsaP256,
				Bugs: ProtocolBugs{
					ExpectServerCertificateTypes: certTypesListRPKOnly,
					SendServerCertificateTypes:   certTypesListRPKOnly,
				},
			},
			flags:         shimFlags,
			shouldFail:    true,
			expectedError: ":CERTIFICATE_VERIFY_FAILED:",
		})
		// Tests that server can receive a server_certificate_type extension from
		// the client and select and send its most-preferred shared cert type based
		// on configured server credentials, and test that server sends credential
		// matching the selected cert type if appropriate.
		for _, test := range []struct {
			name                        string
			serverCertTypesRequested    []CertificateType
			serverCredentialsConfigured []*Credential
			expectedCredentialIndex     int
		}{
			{
				name:                        "RPKRequested-RPKAvailable",
				serverCertTypesRequested:    certTypesListRPKOnly,
				serverCredentialsConfigured: []*Credential{&rpkEcdsaP256},
				expectedCredentialIndex:     0,
			},
			{
				// The first matching credential is picked, in the absence of other
				// criteria. (See also: Server-RawPublicKey-* tests in
				// certificate_selection_tests.go.)
				name:                        "RPKRequested-MultipleRPKsAvailable",
				serverCertTypesRequested:    certTypesListRPKOnly,
				serverCredentialsConfigured: []*Credential{&rpkEcdsaP256, &rpkRsa},
				expectedCredentialIndex:     0,
			},
			{
				name:                        "RPKX509Requested-RPKAvailable",
				serverCertTypesRequested:    certTypesListRPKX509,
				serverCredentialsConfigured: []*Credential{&rpkEcdsaP256},
				expectedCredentialIndex:     0,
			},
			{
				name:                        "X509RPKRequested-RPKAvailable",
				serverCertTypesRequested:    certTypesListX509RPK,
				serverCredentialsConfigured: []*Credential{&rpkEcdsaP256},
				expectedCredentialIndex:     0,
			},
			{
				name:                        "RPKRequested-RPKX509Available",
				serverCertTypesRequested:    certTypesListRPKOnly,
				serverCredentialsConfigured: []*Credential{&rpkEcdsaP256, &ecdsaP256Certificate},
				expectedCredentialIndex:     0,
			},
			{
				name:                        "RPKX509Requested-RPKX509Available",
				serverCertTypesRequested:    certTypesListRPKX509,
				serverCredentialsConfigured: []*Credential{&rpkEcdsaP256, &ecdsaP256Certificate},
				expectedCredentialIndex:     0,
			},
			{
				name:                        "X509RPKRequested-RPKX509Available",
				serverCertTypesRequested:    certTypesListX509RPK,
				serverCredentialsConfigured: []*Credential{&rpkEcdsaP256, &ecdsaP256Certificate},
				expectedCredentialIndex:     0,
			},
			{
				name:                        "RPKRequested-X509RPKAvailable",
				serverCertTypesRequested:    certTypesListRPKOnly,
				serverCredentialsConfigured: []*Credential{&ecdsaP256Certificate, &rpkEcdsaP256},
				expectedCredentialIndex:     1,
			},
			{
				name:                        "RPKX509Requested-X509RPKAvailable",
				serverCertTypesRequested:    certTypesListRPKX509,
				serverCredentialsConfigured: []*Credential{&ecdsaP256Certificate, &rpkEcdsaP256},
				expectedCredentialIndex:     0,
			},
			{
				name:                        "X509RPKRequested-X509RPKAvailable",
				serverCertTypesRequested:    certTypesListX509RPK,
				serverCredentialsConfigured: []*Credential{&ecdsaP256Certificate, &rpkEcdsaP256},
				expectedCredentialIndex:     0,
			},
			{
				// The server should ignore any values from the client that are unknown,
				// and use the remaining values in the list.
				name:                        "RPKUnknownRequested-X509RPKAvailable",
				serverCertTypesRequested:    certTypesListRPKUnknown,
				serverCredentialsConfigured: []*Credential{&ecdsaP256Certificate, &rpkEcdsaP256},
				expectedCredentialIndex:     1,
			},
			{
				// If the only known value in the list received from the client is the
				// default X.509, it's still valid if it wasn't the only value.
				name:                        "UnknownX509Requested-X509RPKAvailable",
				serverCertTypesRequested:    certTypesListUnknownX509,
				serverCredentialsConfigured: []*Credential{&rpkEcdsaP256, &ecdsaP256Certificate},
				expectedCredentialIndex:     1,
			},
		} {
			var expectedServerCredential *Credential
			if test.expectedCredentialIndex != -1 {
				expectedServerCredential = test.serverCredentialsConfigured[test.expectedCredentialIndex]
			}
			serverTestCase := testCase{
				testType: serverTest,
				name:     fmt.Sprintf("ServerCertificateType-Server-%s-%s", test.name, ver.name),
				config: Config{
					MinVersion: ver.version,
					MaxVersion: ver.version,
					Bugs: ProtocolBugs{
						SendServerCertificateTypes:   test.serverCertTypesRequested,
						ExpectServerCertificateTypes: []CertificateType{expectedServerCredential.Type.CertificateType()},
					},
				},
				flags: []string{
					"-on-initial-expect-selected-credential", strconv.Itoa(test.expectedCredentialIndex),
				},
				expectations: connectionExpectations{
					peerCertificate: expectedServerCredential,
				},
				shimCredentials:    test.serverCredentialsConfigured,
				resumeSession:      true,
				skipSplitHandshake: true,
			}
			// Test that the server can defer configuring credentials to the cert
			// callback.
			certCallbackTestCase := serverTestCase
			certCallbackTestCase.flags = append(slices.Clip(certCallbackTestCase.flags),
				"-async")
			certCallbackTestCase.name += "-CertCallback"
			// Test that the server can defer configuring credentials to the early
			// callback.
			earlyCallbackTestCase := serverTestCase
			earlyCallbackTestCase.flags = append(slices.Clip(earlyCallbackTestCase.flags),
				"-async", "-use-early-callback")
			earlyCallbackTestCase.name += "-EarlyCallback"

			testCases = append(testCases,
				serverTestCase,
				certCallbackTestCase,
				earlyCallbackTestCase,
			)
		}
		// The server should reject a client's list that contains only the default
		// X.509, which is a syntax error.
		testCases = append(testCases, testCase{
			testType: serverTest,
			name:     fmt.Sprintf("ServerCertificateType-Server-RejectsDefaultOnly-%s", ver.name),
			config: Config{
				MinVersion: ver.version,
				MaxVersion: ver.version,
				Bugs: ProtocolBugs{
					SendServerCertificateTypes: certTypesListX509Only,
				},
			},
			shouldFail:    true,
			expectedError: ":DECODE_ERROR:",
		})
	}
}

func addClientCertTypeTests() {
	for _, ver := range allVersions(tls) {
		// Tests sending client_certificate_type extension in the ClientHello based
		// on configured client credentials, and test that client responds to
		// server's selected cert type by sending the credential if appropriate.
		for _, test := range []struct {
			name                         string
			clientCredentials            []*Credential
			expectedClientHelloExtension []CertificateType
			serverSelectedClientCertType []CertificateType
			skipCertificateRequest       bool
			expectedCredentialIndex      int
			expectedFailure              string
		}{
			{
				name:                         "RPKOnly-SendsRequestedRPK",
				clientCredentials:            []*Credential{&rpkEcdsaP256},
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverSelectedClientCertType: certTypesListRPKOnly,
				expectedCredentialIndex:      0,
			},
			{
				name:                         "RPKOnly-ServerRequestsX509InError",
				clientCredentials:            []*Credential{&rpkEcdsaP256},
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverSelectedClientCertType: certTypesListX509Only,
				expectedFailure:              ":UNSUPPORTED_CERTIFICATE:",
			},
			{
				name:                         "RPKOnly-ServerOmitsExtension",
				clientCredentials:            []*Credential{&rpkEcdsaP256},
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverSelectedClientCertType: []CertificateType{},
				expectedFailure:              ":UNKNOWN_CERTIFICATE_TYPE:",
			},
			// If the server omits the extension and thus requests X.509 by default,
			// it is not an error if the server doesn't send a CertificateRequest
			// after all.
			{
				name:                         "RPKOnly-ServerOmitsExtension-NoCertRequest",
				clientCredentials:            []*Credential{&rpkEcdsaP256},
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverSelectedClientCertType: []CertificateType{},
				skipCertificateRequest:       true,
				expectedCredentialIndex:      -1,
			},
			{
				name:                         "MultipleRPKs-SendsFirstRPK",
				clientCredentials:            []*Credential{&rpkEcdsaP256, &rpkRsa},
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverSelectedClientCertType: certTypesListRPKOnly,
				expectedCredentialIndex:      0,
			},
			{
				name:                         "RPKX509-SendsRequestedRPK",
				clientCredentials:            []*Credential{&rpkEcdsaP256, &ecdsaP256Certificate},
				expectedClientHelloExtension: certTypesListRPKX509,
				serverSelectedClientCertType: certTypesListRPKOnly,
				expectedCredentialIndex:      0,
			},
			{
				name:                         "RPKX509-SendsRequestedX509",
				clientCredentials:            []*Credential{&rpkEcdsaP256, &ecdsaP256Certificate},
				expectedClientHelloExtension: certTypesListRPKX509,
				serverSelectedClientCertType: certTypesListX509Only,
				expectedCredentialIndex:      1,
			},
			{
				name:                         "RPKX509-SendsDefaultX509",
				clientCredentials:            []*Credential{&rpkEcdsaP256, &ecdsaP256Certificate},
				expectedClientHelloExtension: certTypesListRPKX509,
				serverSelectedClientCertType: []CertificateType{},
				expectedCredentialIndex:      1,
			},
			{
				name:                         "X509RPK-SendsRequestedRPK",
				clientCredentials:            []*Credential{&ecdsaP256Certificate, &rpkEcdsaP256},
				expectedClientHelloExtension: certTypesListX509RPK,
				serverSelectedClientCertType: certTypesListRPKOnly,
				expectedCredentialIndex:      1,
			},
			{
				name:                         "X509RPK-SendsRequestedX509",
				clientCredentials:            []*Credential{&ecdsaP256Certificate, &rpkEcdsaP256},
				expectedClientHelloExtension: certTypesListX509RPK,
				serverSelectedClientCertType: certTypesListX509Only,
				expectedCredentialIndex:      0,
			},
			{
				name:                         "X509RPK-SendsDefaultX509",
				clientCredentials:            []*Credential{&ecdsaP256Certificate, &rpkEcdsaP256},
				expectedClientHelloExtension: certTypesListX509RPK,
				serverSelectedClientCertType: []CertificateType{},
				expectedCredentialIndex:      0,
			},
		} {
			clientAuth := RequestClientCert
			if test.skipCertificateRequest {
				clientAuth = NoClientCert
			}
			var expectedClientCredential *Credential
			if test.expectedFailure == "" && test.expectedCredentialIndex != -1 {
				expectedClientCredential = test.clientCredentials[test.expectedCredentialIndex]
			}
			testCases = append(testCases, testCase{
				testType: clientTest,
				name:     fmt.Sprintf("ClientCertificateType-Client-Offers%s-%s", test.name, ver.name),
				config: Config{
					MinVersion: ver.version,
					MaxVersion: ver.version,
					ClientAuth: clientAuth,
					Bugs: ProtocolBugs{
						ExpectClientCertificateTypes: test.expectedClientHelloExtension,
						SendClientCertificateTypes:   test.serverSelectedClientCertType,
					},
				},
				shimCredentials: test.clientCredentials,
				flags: []string{
					"-on-initial-expect-selected-credential", strconv.Itoa(test.expectedCredentialIndex),
				},
				expectations: connectionExpectations{
					peerCertificate: expectedClientCredential,
				},
				shouldFail:    test.expectedFailure != "",
				expectedError: test.expectedFailure,
				resumeSession: test.expectedFailure == "",
			})
		}
		// Tests that overriding the default client_certificate_type logic works, and
		// client can explicitly configure types to send in the ClientHello, and test
		// that client responds to server's selected cert type by sending the
		// credential if appropriate.
		for _, test := range []struct {
			name                         string
			configuredClientCertTypes    []CertificateType
			clientCredentials            []*Credential
			expectedClientHelloExtension []CertificateType
			serverSelectedClientCertType []CertificateType
			skipCertificateRequest       bool
			expectedCredentialIndex      int
			expectedFailure              string
		}{
			{
				name:                         "RPKOnly-ConfiguredAsOnlyCredential-SendsRequestedRPK",
				configuredClientCertTypes:    certTypesListRPKOnly,
				clientCredentials:            []*Credential{&rpkEcdsaP256},
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverSelectedClientCertType: certTypesListRPKOnly,
				expectedCredentialIndex:      0,
			},
			{
				name:                         "RPKOnly-ConfiguredAsOnlyCredential-ServerRequestsX509InError",
				configuredClientCertTypes:    certTypesListRPKOnly,
				clientCredentials:            []*Credential{&rpkEcdsaP256},
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverSelectedClientCertType: certTypesListX509Only,
				expectedFailure:              ":UNSUPPORTED_CERTIFICATE:",
			},
			{
				name:                         "RPKOnly-ConfiguredAsFirstCredential-SendsRequestedRPK",
				configuredClientCertTypes:    certTypesListRPKOnly,
				clientCredentials:            []*Credential{&rpkEcdsaP256, &ecdsaP256Certificate},
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverSelectedClientCertType: certTypesListRPKOnly,
				expectedCredentialIndex:      0,
			},
			{
				name:                         "RPKOnly-ConfiguredAsFirstCredential-ServerRequestsX509InError",
				configuredClientCertTypes:    certTypesListRPKOnly,
				clientCredentials:            []*Credential{&rpkEcdsaP256, &ecdsaP256Certificate},
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverSelectedClientCertType: certTypesListX509Only,
				expectedFailure:              ":UNSUPPORTED_CERTIFICATE:",
			},
			{
				name:                         "RPKOnly-ConfiguredAsSecondCredential-SendsRequestedRPK",
				configuredClientCertTypes:    certTypesListRPKOnly,
				clientCredentials:            []*Credential{&ecdsaP256Certificate, &rpkEcdsaP256},
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverSelectedClientCertType: certTypesListRPKOnly,
				expectedCredentialIndex:      1,
			},
			{
				name:                         "RPKOnly-ConfiguredAsSecondCredential-ServerRequestsX509InError",
				configuredClientCertTypes:    certTypesListRPKOnly,
				clientCredentials:            []*Credential{&ecdsaP256Certificate, &rpkEcdsaP256},
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverSelectedClientCertType: certTypesListX509Only,
				expectedFailure:              ":UNSUPPORTED_CERTIFICATE:",
			},
			{
				name:                         "RPKX509-ConfiguredInOppositeOrder-SendsRequestedRPK",
				configuredClientCertTypes:    certTypesListRPKX509,
				clientCredentials:            []*Credential{&ecdsaP256Certificate, &rpkEcdsaP256},
				expectedClientHelloExtension: certTypesListRPKX509,
				serverSelectedClientCertType: certTypesListRPKOnly,
				expectedCredentialIndex:      1,
			},
			{
				name:                         "RPKX509-ConfiguredInOppositeOrder-SendsRequestedX509",
				configuredClientCertTypes:    certTypesListRPKX509,
				clientCredentials:            []*Credential{&ecdsaP256Certificate, &rpkEcdsaP256},
				expectedClientHelloExtension: certTypesListRPKX509,
				serverSelectedClientCertType: certTypesListX509Only,
				expectedCredentialIndex:      0,
			},
			{
				name:                         "X509RPK-ConfiguredInOppositeOrder-SendsRequestedRPK",
				configuredClientCertTypes:    certTypesListX509RPK,
				clientCredentials:            []*Credential{&rpkEcdsaP256, &ecdsaP256Certificate},
				expectedClientHelloExtension: certTypesListX509RPK,
				serverSelectedClientCertType: certTypesListRPKOnly,
				expectedCredentialIndex:      0,
			},
			{
				name:                         "X509RPK-ConfiguredInOppositeOrder-SendsRequestedX509",
				configuredClientCertTypes:    certTypesListX509RPK,
				clientCredentials:            []*Credential{&rpkEcdsaP256, &ecdsaP256Certificate},
				expectedClientHelloExtension: certTypesListX509RPK,
				serverSelectedClientCertType: certTypesListX509Only,
				expectedCredentialIndex:      1,
			},
			{
				name:                         "DefaultX509Only-ServerRequestsX509InError",
				configuredClientCertTypes:    certTypesListX509Only,
				clientCredentials:            []*Credential{&ecdsaP256Certificate, &rpkEcdsaP256},
				expectedClientHelloExtension: []CertificateType{},
				serverSelectedClientCertType: certTypesListX509Only,
				expectedFailure:              ":UNEXPECTED_EXTENSION:",
			},
			{
				name:                         "DefaultX509Only-ServerRequestsRPKInError",
				configuredClientCertTypes:    certTypesListX509Only,
				clientCredentials:            []*Credential{&ecdsaP256Certificate, &rpkEcdsaP256},
				expectedClientHelloExtension: []CertificateType{},
				serverSelectedClientCertType: certTypesListRPKOnly,
				expectedFailure:              ":UNEXPECTED_EXTENSION:",
			},
			{
				name:                         "DefaultX509Only-SendsDefaultX509",
				configuredClientCertTypes:    certTypesListX509Only,
				clientCredentials:            []*Credential{&ecdsaP256Certificate, &rpkEcdsaP256},
				expectedClientHelloExtension: []CertificateType{},
				serverSelectedClientCertType: []CertificateType{},
				expectedCredentialIndex:      0,
			},
			// The client falsely advertises an RPK and the server selects it. There
			// is no such RPK to send, but the client should be able to proceed
			// without a cert.
			{
				name:                         "RPK-NotActuallyConfigured-ServerRequestsRPK",
				configuredClientCertTypes:    certTypesListRPKOnly,
				clientCredentials:            []*Credential{},
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverSelectedClientCertType: certTypesListRPKOnly,
				expectedCredentialIndex:      -1,
			},
			{
				name:                         "RPK-NotActuallyConfigured-ServerRequestsX509InError",
				configuredClientCertTypes:    certTypesListRPKOnly,
				clientCredentials:            []*Credential{},
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverSelectedClientCertType: certTypesListX509Only,
				expectedFailure:              ":UNSUPPORTED_CERTIFICATE:",
			},
			// If the server omits the extension and thus requests X.509 by default,
			// it is not an error if the server doesn't send a CertificateRequest
			// after all.
			{
				name:                         "RPK-NotActuallyConfigured-ServerOmitsExtension-NoCertRequest",
				configuredClientCertTypes:    certTypesListRPKOnly,
				clientCredentials:            []*Credential{},
				expectedClientHelloExtension: certTypesListRPKOnly,
				serverSelectedClientCertType: []CertificateType{},
				skipCertificateRequest:       true,
				expectedCredentialIndex:      -1,
			},
		} {
			clientAuth := RequestClientCert
			if test.skipCertificateRequest {
				clientAuth = NoClientCert
			}
			var expectedClientCredential *Credential
			if test.expectedFailure == "" && test.expectedCredentialIndex != -1 {
				expectedClientCredential = test.clientCredentials[test.expectedCredentialIndex]
			}
			clientTestCase := testCase{
				testType: clientTest,
				name:     fmt.Sprintf("ClientCertificateType-Client-Explicit-Offers%s-%s", test.name, ver.name),
				config: Config{
					MinVersion: ver.version,
					MaxVersion: ver.version,
					ClientAuth: clientAuth,
					Bugs: ProtocolBugs{
						ExpectClientCertificateTypes: test.expectedClientHelloExtension,
						SendClientCertificateTypes:   test.serverSelectedClientCertType,
					},
				},
				flags: append(
					[]string{"-on-initial-expect-selected-credential", strconv.Itoa(test.expectedCredentialIndex)},
					flagCertTypes("-available-client-cert-types", test.configuredClientCertTypes)...),
				shimCredentials: test.clientCredentials,
				expectations: connectionExpectations{
					peerCertificate: expectedClientCredential,
				},
				shouldFail:    test.expectedFailure != "",
				expectedError: test.expectedFailure,
				resumeSession: test.expectedFailure == "",
			}
			// Test that the client can defer configuring credentials to the cert
			// callback.
			certCallbackTestCase := clientTestCase
			certCallbackTestCase.flags = append(slices.Clip(certCallbackTestCase.flags), "-async")
			certCallbackTestCase.name += "-CertCallback"

			testCases = append(testCases,
				clientTestCase,
				certCallbackTestCase,
			)
		}
		// Tests receiving a client_certificate_type extension from the client and
		// selecting and sending our most-preferred shared cert type.
		for _, test := range []struct {
			name                         string
			clientCertTypesReceived      []CertificateType
			clientCertTypesAccepted      []CertificateType
			expectedServerHelloExtension []CertificateType
			clientCredential             *Credential
			expectedError                string
			expectedLocalError           string
		}{
			{
				name:                         "RPKReceived-RPKAccepted",
				clientCertTypesReceived:      certTypesListRPKOnly,
				clientCertTypesAccepted:      certTypesListRPKOnly,
				expectedServerHelloExtension: certTypesListRPKOnly,
				clientCredential:             &rpkEcdsaP256,
			},
			{
				name:                         "RPKX509Received-RPKAccepted",
				clientCertTypesReceived:      certTypesListRPKX509,
				clientCertTypesAccepted:      certTypesListRPKOnly,
				expectedServerHelloExtension: certTypesListRPKOnly,
				clientCredential:             &rpkEcdsaP256,
			},
			{
				name:                         "X509RPKReceived-RPKAccepted",
				clientCertTypesReceived:      certTypesListX509RPK,
				clientCertTypesAccepted:      certTypesListRPKOnly,
				expectedServerHelloExtension: certTypesListRPKOnly,
				clientCredential:             &rpkEcdsaP256,
			},
			{
				name:                         "RPKX509Received-RPKX509Accepted",
				clientCertTypesReceived:      certTypesListRPKX509,
				clientCertTypesAccepted:      certTypesListRPKX509,
				expectedServerHelloExtension: certTypesListRPKOnly,
				clientCredential:             &rpkEcdsaP256,
			},
			{
				name:                         "X509RPKReceived-RPKX509Accepted",
				clientCertTypesReceived:      certTypesListX509RPK,
				clientCertTypesAccepted:      certTypesListRPKX509,
				expectedServerHelloExtension: certTypesListRPKOnly,
				clientCredential:             &rpkEcdsaP256,
			},
			{
				name:                         "RPKX509Received-X509RPKAccepted",
				clientCertTypesReceived:      certTypesListRPKX509,
				clientCertTypesAccepted:      certTypesListX509RPK,
				expectedServerHelloExtension: certTypesListX509Only,
				clientCredential:             &ecdsaP256Certificate,
			},
			{
				name:                         "X509RPKReceived-X509RPKAccepted",
				clientCertTypesReceived:      certTypesListX509RPK,
				clientCertTypesAccepted:      certTypesListX509RPK,
				expectedServerHelloExtension: certTypesListX509Only,
				clientCredential:             &ecdsaP256Certificate,
			},
			{
				name:                    "RejectsInvalidEmptyExtension",
				clientCertTypesReceived: []CertificateType{},
				clientCertTypesAccepted: certTypesListX509RPK,
				expectedError:           ":DECODE_ERROR:",
				expectedLocalError:      "remote error: illegal parameter",
			},
			{
				// The client should have omitted the extension if only the default is
				// accepted.
				name:                    "RejectsInvalidDefaultOnly",
				clientCertTypesReceived: certTypesListX509Only,
				clientCertTypesAccepted: certTypesListX509RPK,
				expectedError:           ":DECODE_ERROR:",
				expectedLocalError:      "remote error: illegal parameter",
			},
			{
				// The client's list contains only an unknown value, which is ignored.
				// Negotiating a client cert type value fails.
				name:                    "IgnoresUnknownValue-NoOtherType",
				clientCertTypesReceived: certTypesListUnknown,
				clientCertTypesAccepted: certTypesListX509RPK,
				expectedError:           ":UNSUPPORTED_CERTIFICATE:",
				expectedLocalError:      "remote error: unsupported certificate",
			},
			{
				// The client's list contains an unknown value, which is ignored, and
				// a recognized value, which is not shared with the server.
				name:                    "IgnoresUnknownValue-NoSharedType",
				clientCertTypesReceived: certTypesListUnknownX509,
				clientCertTypesAccepted: certTypesListRPKOnly,
				expectedError:           ":UNSUPPORTED_CERTIFICATE:",
				expectedLocalError:      "remote error: unsupported certificate",
			},
			{
				// The client's list contains an unknown value, which is ignored, and
				// a recognized value, which is accepted successfully.
				name:                         "IgnoresUnknownValue-RPKAccepted",
				clientCertTypesReceived:      certTypesListRPKUnknown,
				clientCertTypesAccepted:      certTypesListX509RPK,
				expectedServerHelloExtension: certTypesListRPKOnly,
				clientCredential:             &rpkEcdsaP256,
			},
			{
				// If the client does not send the extension, the server should treat it
				// as X.509 only by default.
				name:                         "NoClientHelloCertTypes-SelectsX509ByDefault",
				clientCertTypesReceived:      nil,
				clientCertTypesAccepted:      certTypesListRPKX509,
				expectedServerHelloExtension: []CertificateType{},
				clientCredential:             &ecdsaP256Certificate,
			},
			{
				// If the client does not send the extension, but the server is
				// configured to only accept RPKs, the connection should fail.
				name:                    "NoClientHelloCertTypes-NoSharedType",
				clientCertTypesReceived: nil,
				clientCertTypesAccepted: certTypesListRPKOnly,
				expectedError:           ":UNSUPPORTED_CERTIFICATE:",
				expectedLocalError:      "remote error: unsupported certificate",
			},
			{
				name:                         "RPKReceived-RPKAccepted-ClientSentX509InError",
				clientCertTypesReceived:      certTypesListRPKOnly,
				clientCertTypesAccepted:      certTypesListRPKOnly,
				expectedServerHelloExtension: certTypesListRPKOnly,
				clientCredential:             &ecdsaP256Certificate,
				expectedError:                ":DECODE_ERROR:",
				expectedLocalError:           "remote error: error decoding message",
			},
		} {
			for _, verifyMode := range []struct {
				name string
				flag string
			}{
				{"FailIfNoClientCert", "-require-any-client-certificate"},
				{"VerifyPeer", "-verify-peer"},
			} {
				shimFlags :=
					append(flagCertTypes("-accepted-peer-cert-types", test.clientCertTypesAccepted),
						verifyMode.flag)
				if test.expectedError == "" {
					shimFlags = append(shimFlags,
						"-expect-peer-certificate-type", strconv.Itoa(int(test.clientCredential.Type.CertificateType())))
					shimFlags = addRPKCustomVerifyToFlags(test.clientCredential, shimFlags)
				}
				testCases = append(testCases, testCase{
					testType: serverTest,
					name:     fmt.Sprintf("ClientCertificateType-Server-%s-%s-%s", test.name, verifyMode.name, ver.name),
					config: Config{
						MinVersion: ver.version,
						MaxVersion: ver.version,
						Credential: test.clientCredential,
						Bugs: ProtocolBugs{
							SendClientCertificateTypes:   test.clientCertTypesReceived,
							ExpectClientCertificateTypes: test.expectedServerHelloExtension,
						},
					},
					flags:              shimFlags,
					shouldFail:         test.expectedError != "" || test.expectedLocalError != "",
					expectedError:      test.expectedError,
					expectedLocalError: test.expectedLocalError,
					resumeSession:      test.expectedError == "" && test.expectedLocalError == "",
					skipSplitHandshake: true,
				})
			}
		}
		// The Certificate message contains an RPK with an empty SPKI. In TLS 1.2
		// this is considered to indicate the lack of a certificate (so a server
		// configured to require a client cert will reject), whereas this is
		// illegal for the TLS 1.3 RPK Certificate format.
		shimFlags := append([]string{"-require-any-client-certificate"},
			flagCertTypes("-accepted-peer-cert-types", certTypesListRPKOnly)...)
		expectedError := ":INVALID_RAW_PUBLIC_KEY:"
		if ver.version <= VersionTLS12 {
			expectedError = ":PEER_DID_NOT_RETURN_A_CERTIFICATE:"
		}
		testCases = append(testCases, testCase{
			testType: serverTest,
			name:     fmt.Sprintf("ClientCertificateType-Server-ClientSentEmptyRPK-%s", ver.name),
			config: Config{
				MinVersion: ver.version,
				MaxVersion: ver.version,
				Credential: &rpkEmpty,
				Bugs: ProtocolBugs{
					SendClientCertificateTypes:   certTypesListRPKOnly,
					ExpectClientCertificateTypes: certTypesListRPKOnly,
				},
			},
			flags:              shimFlags,
			skipSplitHandshake: true,
			shouldFail:         true,
			expectedError:      expectedError,
		})
		shimFlags = append([]string{"-verify-peer"},
			flagCertTypes("-accepted-peer-cert-types", certTypesListRPKOnly)...)
		// If the client sends a Certificate message for an RPK that contains an
		// empty certificate list, and the server isn't configured to require a
		// client cert, it should proceed without a client cert.
		testCases = append(testCases, testCase{
			testType: serverTest,
			name:     fmt.Sprintf("ClientCertificateType-Server-ClientSentEmptyCertificateList-%s", ver.name),
			config: Config{
				MinVersion: ver.version,
				MaxVersion: ver.version,
				Credential: &rpkEcdsaP256,
				Bugs: ProtocolBugs{
					SendClientCertificateTypes:   certTypesListRPKOnly,
					ExpectClientCertificateTypes: certTypesListRPKOnly,
					EmptyCertificateList:         true,
					SkipCertificateVerify:        true,
				},
			},
			flags:              shimFlags,
			skipSplitHandshake: true,
		})
		if ver.version >= VersionTLS13 {
			// If the client sends an otherwise valid Certificate message with an RPK,
			// but does not send CertificateVerify, server should reject.
			shimFlags = addRPKCustomVerifyToFlags(&rpkEcdsaP256, shimFlags)
			testCases = append(testCases, testCase{
				testType: serverTest,
				name:     fmt.Sprintf("ClientCertificateType-Server-RPKWithoutCertificateVerify-%s", ver.name),
				config: Config{
					MinVersion: ver.version,
					MaxVersion: ver.version,
					Credential: &rpkEcdsaP256,
					Bugs: ProtocolBugs{
						SendClientCertificateTypes:   certTypesListRPKOnly,
						ExpectClientCertificateTypes: certTypesListRPKOnly,
						SkipCertificateVerify:        true,
					},
				},
				flags:              shimFlags,
				shouldFail:         true,
				expectedLocalError: "remote error: unexpected message",
				skipSplitHandshake: true,
			})
		}
		// Test that RPK client cert verification fails if we force it to fail.
		shimFlags = append([]string{"-require-any-client-certificate", "-verify-fail"},
			flagCertTypes("-accepted-peer-cert-types", certTypesListRPKOnly)...)
		testCases = append(testCases, testCase{
			testType: serverTest,
			name:     fmt.Sprintf("ClientCertificateType-Server-RPKVerifyFail-%s", ver.name),
			config: Config{
				MinVersion: ver.version,
				MaxVersion: ver.version,
				Credential: &rpkEcdsaP256,
				Bugs: ProtocolBugs{
					SendClientCertificateTypes:   certTypesListRPKOnly,
					ExpectClientCertificateTypes: certTypesListRPKOnly,
				},
			},
			flags:              shimFlags,
			shouldFail:         true,
			expectedError:      ":CERTIFICATE_VERIFY_FAILED:",
			skipSplitHandshake: true,
		})
	}
}

func addRawPublicKeyTests() {
	addServerCertTypeTests()
	addClientCertTypeTests()
}
