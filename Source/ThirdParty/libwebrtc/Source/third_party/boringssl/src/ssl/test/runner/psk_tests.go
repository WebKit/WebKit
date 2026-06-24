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
	"crypto"
	"fmt"
	"slices"
)

func hashToString(hash crypto.Hash) string {
	switch hash {
	case crypto.SHA256:
		return "SHA256"
	case crypto.SHA384:
		return "SHA384"
	default:
		panic(fmt.Sprintf("unknown hash %d", hash))
	}
}

func addPSKTests() {
	pskSHA256Credential := Credential{
		Type:         CredentialTypePreSharedKey,
		PreSharedKey: slices.Repeat([]byte{'A', 'B', 'C', 'D'}, 8),
		PSKIdentity:  []byte("psk1"),
		PSKContext:   []byte("context1"),
		PSKHash:      crypto.SHA256,
	}
	pskSHA384Credential := Credential{
		Type:         CredentialTypePreSharedKey,
		PreSharedKey: slices.Repeat([]byte{'E', 'F', 'G', 'H'}, 12),
		PSKIdentity:  []byte("psk2"),
		PSKContext:   []byte("context2"),
		PSKHash:      crypto.SHA384,
	}
	pskSHA256Credential2 := Credential{
		Type:         CredentialTypePreSharedKey,
		PreSharedKey: slices.Repeat([]byte{'I', 'J', 'K', 'L'}, 8),
		PSKIdentity:  []byte("psk3"),
		PSKContext:   []byte("context3"),
		PSKHash:      crypto.SHA256,
	}

	hashToPSK := func(hash crypto.Hash) *Credential {
		switch hash {
		case crypto.SHA256:
			return &pskSHA256Credential
		case crypto.SHA384:
			return &pskSHA384Credential
		default:
			panic(fmt.Sprintf("unknown hash %d", hash))
		}
	}
	hashToCipher := func(hash crypto.Hash) uint16 {
		switch hash {
		case crypto.SHA256:
			return TLS_AES_128_GCM_SHA256
		case crypto.SHA384:
			return TLS_AES_256_GCM_SHA384
		default:
			panic(fmt.Sprintf("unknown hash %d", hash))
		}
	}

	for _, protocol := range []protocol{tls, dtls, quic} {
		// Test that SHA-256 and SHA-384 PSKs can be used with SHA-256 and
		// SHA-384 ciphers.
		for _, pskHash := range []crypto.Hash{crypto.SHA256, crypto.SHA384} {
			psk := hashToPSK(pskHash)
			for _, cipherHash := range []crypto.Hash{crypto.SHA256, crypto.SHA384} {
				cipher := hashToCipher(cipherHash)
				testCases = append(testCases, testCase{
					protocol: protocol,
					name:     fmt.Sprintf("PSK-Client-%s-%s-%s", hashToString(pskHash), hashToString(cipherHash), protocol),
					config: Config{
						Credential:   psk,
						MaxVersion:   VersionTLS13,
						CipherSuites: []uint16{cipher},
					},
					shimCredentials: []*Credential{psk},
					// Also test that the resulting session can be reused.
					resumeSession: true,
					// Override the default behavior of expecting a peer certificate on
					// resumption connections.
					flags: []string{"-expect-no-peer-cert"},
				})
				testCases = append(testCases, testCase{
					testType: serverTest,
					protocol: protocol,
					name:     fmt.Sprintf("PSK-Server-%s-%s-%s", hashToString(pskHash), hashToString(cipherHash), protocol),
					config: Config{
						Credential:   psk,
						MaxVersion:   VersionTLS13,
						CipherSuites: []uint16{cipher},
					},
					shimCredentials: []*Credential{psk},
					expectations: connectionExpectations{
						selectedPSK: psk,
					},
					// Also test that the resulting session can be reused.
					resumeSession:      true,
					resumeExpectations: &connectionExpectations{},
				})

				// Test with HelloRetryRequest to ensure the client computes
				// the second ClientHello's binder correctly, and also accounts
				// for the PSK list getting smaller once the cipher is known.
				testCases = append(testCases, testCase{
					protocol: protocol,
					name:     fmt.Sprintf("PSK-Client-HRR-%s-%s-%s", hashToString(pskHash), hashToString(cipherHash), protocol),
					config: Config{
						Credential:   psk,
						MaxVersion:   VersionTLS13,
						CipherSuites: []uint16{cipher},
						Bugs: ProtocolBugs{
							SendHelloRetryRequestCookie: []byte("cookie"),
						},
					},
					shimCredentials: []*Credential{psk},
					// Also test that the resulting session can be reused.
					resumeSession: true,
					// Override the default behavior of expecting a peer certificate on
					// resumption connections.
					flags: []string{"-expect-no-peer-cert"},
				})
			}
		}

		// If the client is configured to offer multiple PSKs, it should accept
		// either from the server.
		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Client-AcceptFirst-%s", protocol),
			testType: clientTest,
			config: Config{
				MaxVersion: VersionTLS13,
				Credential: &pskSHA256Credential,
			},
			shimCredentials: []*Credential{&pskSHA256Credential, &pskSHA384Credential},
			flags:           []string{"-expect-selected-credential", "0"},
		})
		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Client-AcceptSecond-%s", protocol),
			testType: clientTest,
			config: Config{
				MaxVersion: VersionTLS13,
				Credential: &pskSHA384Credential,
			},
			shimCredentials: []*Credential{&pskSHA256Credential, &pskSHA384Credential},
			flags:           []string{"-expect-selected-credential", "1"},
		})

		// If the client is configured (on the second connection) with both PSKs and
		// a session, the PSK is still usable.
		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Client-DeclineSession-%s", protocol),
			config: Config{
				MaxVersion: VersionTLS13,
				Credential: &pskSHA256Credential,
			},
			resumeConfig: &Config{
				MaxVersion:             VersionTLS13,
				Credential:             &pskSHA256Credential,
				SessionTicketsDisabled: true,
			},
			shimCredentials:      []*Credential{&pskSHA256Credential},
			resumeSession:        true,
			expectResumeRejected: true,
			// The runner will not provision a ticket on the second connection.
			flags: []string{"-on-resume-expect-no-session"},
		})

		// The client should reject out-of-bounds PSK indices.
		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Client-OutOfBoundsIndex-%s", protocol),
			testType: clientTest,
			config: Config{
				MaxVersion: VersionTLS13,
				Credential: &pskSHA256Credential,
				Bugs: ProtocolBugs{
					// The shim will import two PSKs from the credential, so
					// only indices 0 and 1 are valid,
					AlwaysSelectPSKIdentity: ptrTo(uint16(2)),
				},
			},
			shimCredentials: []*Credential{&pskSHA256Credential},
			shouldFail:      true,
			expectedError:   ":PSK_IDENTITY_NOT_FOUND:",
		})
		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Client-OutOfBoundsIndex-HRR-%s", protocol),
			testType: clientTest,
			config: Config{
				MaxVersion: VersionTLS13,
				Credential: &pskSHA256Credential,
				Bugs: ProtocolBugs{
					SendHelloRetryRequestCookie: []byte("cookie"),
					// The shim will import two PSKs from the credential, but
					// then prune them in the second ClientHello, so only index
					// 0 is valid.
					AlwaysSelectPSKIdentity: ptrTo(uint16(1)),
				},
			},
			shimCredentials: []*Credential{&pskSHA256Credential},
			shouldFail:      true,
			expectedError:   ":PSK_IDENTITY_NOT_FOUND:",
		})

		// The client should reject psk_ke connections. We require psk_dhe_ke.
		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Client-NoKeyShare-%s", protocol),
			testType: clientTest,
			config: Config{
				MaxVersion: VersionTLS13,
				Credential: &pskSHA256Credential,
				Bugs: ProtocolBugs{
					NegotiatePSKResumption: true,
				},
			},
			shimCredentials: []*Credential{&pskSHA256Credential},
			shouldFail:      true,
			expectedError:   ":MISSING_KEY_SHARE:",
		})

		// By default, if the client configures PSKs, it should reject server
		// responses that do use certificates, including TLS 1.2.
		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Client-PSKRequired-%s", protocol),
			testType: clientTest,
			config: Config{
				MaxVersion: VersionTLS13,
				Credential: &rsaCertificate,
				Bugs: ProtocolBugs{
					// Ignore the client's lack of signature_algorithms.
					IgnorePeerSignatureAlgorithmPreferences: true,
				},
			},
			shimCredentials: []*Credential{&pskSHA256Credential},
			shouldFail:      true,
			expectedError:   ":MISSING_EXTENSION:",
			// The shim sends an alert, but alerts immediately after TLS 1.3
			// ServerHello have an encryption mismatch.
		})
		if protocol != quic {
			testCases = append(testCases, testCase{
				protocol: protocol,
				name:     fmt.Sprintf("PSK-Client-PSKRequired-TLS12-%s", protocol),
				testType: clientTest,
				config: Config{
					MaxVersion: VersionTLS12,
					Credential: &rsaCertificate,
					Bugs: ProtocolBugs{
						// Ignore the client's lack of signature_algorithms.
						IgnorePeerSignatureAlgorithmPreferences: true,
					},
				},
				shimCredentials:    []*Credential{&pskSHA256Credential},
				shouldFail:         true,
				expectedError:      ":UNSUPPORTED_PROTOCOL:",
				expectedLocalError: "remote error: protocol version not supported",
			})
		}

		// The client can be configured to accept certificates or PSKs. In this
		// case, even TLS 1.2 certificates are acceptable.
		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Client-PSKOrCert-PSK-%s", protocol),
			testType: clientTest,
			config: Config{
				MaxVersion: VersionTLS13,
				Credential: &pskSHA256Credential,
			},
			shimCredentials: []*Credential{&pskSHA256Credential},
			flags:           []string{"-verify-peer"},
		})
		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Client-PSKOrCert-Cert-%s", protocol),
			testType: clientTest,
			config: Config{
				MaxVersion: VersionTLS13,
				Credential: &rsaCertificate,
			},
			shimCredentials: []*Credential{&pskSHA256Credential},
			flags:           []string{"-verify-peer"},
		})
		if protocol != quic {
			testCases = append(testCases, testCase{
				protocol: protocol,
				name:     fmt.Sprintf("PSK-Client-PSKOrCert-Cert-TLS12-%s", protocol),
				testType: clientTest,
				config: Config{
					MaxVersion: VersionTLS12,
					Credential: &rsaCertificate,
				},
				shimCredentials: []*Credential{&pskSHA256Credential},
				flags:           []string{"-verify-peer"},
			})
		}

		// When a client is configured with PSKs or certificates, it can even send
		// client certificates, configured from the credential list.
		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Client-PSKOrCert-CertRequest-%s", protocol),
			testType: clientTest,
			config: Config{
				MaxVersion: VersionTLS13,
				Credential: &rsaCertificate,
				ClientAuth: RequireAnyClientCert,
			},
			shimCredentials: []*Credential{&pskSHA256Credential, &rsaCertificate},
			flags:           []string{"-verify-peer"},
		})
		if protocol != quic {
			testCases = append(testCases, testCase{
				protocol: protocol,
				name:     fmt.Sprintf("PSK-Client-PSKOrCert-CertRequest-TLS12-%s", protocol),
				testType: clientTest,
				config: Config{
					MaxVersion: VersionTLS12,
					Credential: &rsaCertificate,
					ClientAuth: RequireAnyClientCert,
				},
				shimCredentials: []*Credential{&pskSHA256Credential, &rsaCertificate},
				flags:           []string{"-verify-peer"},
			})
		}

		// When a client is configured with PSKs or certificates, the server picks certificates,
		// and the server sends CertificateRequests, it must be possible for the client to
		// proceed without sending any client certificate, even if the credential list has a
		// PSK credential.
		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Client-PSKOrCert-CertRequest-NoClientCert-%s", protocol),
			testType: clientTest,
			config: Config{
				MaxVersion: VersionTLS13,
				Credential: &rsaCertificate,
				ClientAuth: RequestClientCert,
			},
			shimCredentials: []*Credential{&pskSHA256Credential},
			flags:           []string{"-verify-peer"},
		})
		if protocol != quic {
			testCases = append(testCases, testCase{
				protocol: protocol,
				name:     fmt.Sprintf("PSK-Client-PSKOrCert-CertRequest-NoClientCert-TLS12-%s", protocol),
				testType: clientTest,
				config: Config{
					MaxVersion: VersionTLS12,
					Credential: &rsaCertificate,
					ClientAuth: RequestClientCert,
				},
				shimCredentials: []*Credential{&pskSHA256Credential},
				flags:           []string{"-verify-peer"},
			})
		}

		// The client should reject CertificateRequest messages on PSK connections.
		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Client-UnexpectedCertificateRequest-%s", protocol),
			testType: clientTest,
			config: Config{
				MaxVersion: VersionTLS13,
				Credential: &pskSHA256Credential,
				ClientAuth: RequireAnyClientCert,
				Bugs: ProtocolBugs{
					AlwaysSendCertificateRequest: true,
				},
			},
			shimCredentials:    []*Credential{&pskSHA256Credential},
			shouldFail:         true,
			expectedError:      ":UNEXPECTED_MESSAGE:",
			expectedLocalError: "remote error: unexpected message",
		})

		// The client should reject Certificate messages on PSK connections.
		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Client-UnexpectedCertificate-%s", protocol),
			config: Config{
				Credential: &pskSHA256Credential,
				MaxVersion: VersionTLS13,
				Bugs: ProtocolBugs{
					AlwaysSendCertificate:    true,
					UseCertificateCredential: &rsaCertificate,
					// Ignore the client's lack of signature_algorithms.
					IgnorePeerSignatureAlgorithmPreferences: true,
				},
			},
			shimCredentials:    []*Credential{&pskSHA256Credential},
			shouldFail:         true,
			expectedError:      ":UNEXPECTED_MESSAGE:",
			expectedLocalError: "remote error: unexpected message",
		})

		// If a server is configured to request client certificates, it should
		// still not do so when negotiating a PSK.
		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-DoNotRequestClientCertificate-%s", protocol),
			testType: serverTest,
			config: Config{
				Credential: &pskSHA256Credential,
				MaxVersion: VersionTLS13,
			},
			shimCredentials: []*Credential{&pskSHA256Credential},
			flags:           []string{"-require-any-client-certificate"},
		})

		// The server should notice if the second binder is wrong.
		for _, secondBinder := range []bool{false, true} {
			binderStr := "FirstBinder"
			var defaultCurves []CurveID
			if secondBinder {
				binderStr = "SecondBinder"
				// Force a HelloRetryRequest by predicting an empty curve list.
				defaultCurves = []CurveID{}
			}

			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: serverTest,
				name:     fmt.Sprintf("PSK-Server-BinderWrongLength-%s-%s", binderStr, protocol),
				config: Config{
					MaxVersion:    VersionTLS13,
					Credential:    &pskSHA256Credential,
					DefaultCurves: defaultCurves,
					Bugs: ProtocolBugs{
						SendShortPSKBinder:         true,
						OnlyCorruptSecondPSKBinder: secondBinder,
					},
				},
				shimCredentials:    []*Credential{&pskSHA256Credential},
				shouldFail:         true,
				expectedLocalError: "remote error: error decrypting message",
				expectedError:      ":DIGEST_CHECK_FAILED:",
			})

			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: serverTest,
				name:     fmt.Sprintf("PSK-Server-NoPSKBinder-%s-%s", binderStr, protocol),
				config: Config{
					MaxVersion:    VersionTLS13,
					Credential:    &pskSHA256Credential,
					DefaultCurves: defaultCurves,
					Bugs: ProtocolBugs{
						SendNoPSKBinder:            true,
						OnlyCorruptSecondPSKBinder: secondBinder,
					},
				},
				shimCredentials:    []*Credential{&pskSHA256Credential},
				shouldFail:         true,
				expectedLocalError: "remote error: error decoding message",
				expectedError:      ":DECODE_ERROR:",
			})

			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: serverTest,
				name:     fmt.Sprintf("PSK-Server-ExtraPSKBinder-%s-%s", binderStr, protocol),
				config: Config{
					MaxVersion:    VersionTLS13,
					Credential:    &pskSHA256Credential,
					DefaultCurves: defaultCurves,
					Bugs: ProtocolBugs{
						SendExtraPSKBinder:         true,
						OnlyCorruptSecondPSKBinder: secondBinder,
					},
				},
				shimCredentials:    []*Credential{&pskSHA256Credential},
				shouldFail:         true,
				expectedLocalError: "remote error: illegal parameter",
				expectedError:      ":PSK_IDENTITY_BINDER_COUNT_MISMATCH:",
			})

			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: serverTest,
				name:     fmt.Sprintf("PSK-Server-ExtraIdentityNoBinder-%s-%s", binderStr, protocol),
				config: Config{
					MaxVersion:    VersionTLS13,
					Credential:    &pskSHA256Credential,
					DefaultCurves: defaultCurves,
					Bugs: ProtocolBugs{
						ExtraPSKIdentity:           true,
						OnlyCorruptSecondPSKBinder: secondBinder,
					},
				},
				shimCredentials:    []*Credential{&pskSHA256Credential},
				shouldFail:         true,
				expectedLocalError: "remote error: illegal parameter",
				expectedError:      ":PSK_IDENTITY_BINDER_COUNT_MISMATCH:",
			})

			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: serverTest,
				name:     fmt.Sprintf("PSK-Server-InvalidPSKBinder-%s-%s", binderStr, protocol),
				config: Config{
					MaxVersion:    VersionTLS13,
					Credential:    &pskSHA256Credential,
					DefaultCurves: defaultCurves,
					Bugs: ProtocolBugs{
						SendInvalidPSKBinder:       true,
						OnlyCorruptSecondPSKBinder: secondBinder,
					},
				},
				shimCredentials:    []*Credential{&pskSHA256Credential},
				shouldFail:         true,
				expectedLocalError: "remote error: error decrypting message",
				expectedError:      ":DIGEST_CHECK_FAILED:",
			})

			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: serverTest,
				name:     fmt.Sprintf("PSK-Server-PSKBinderFirstExtension-%s-%s", binderStr, protocol),
				config: Config{
					MaxVersion:    VersionTLS13,
					Credential:    &pskSHA256Credential,
					DefaultCurves: defaultCurves,
					Bugs: ProtocolBugs{
						PSKBinderFirst:             true,
						OnlyCorruptSecondPSKBinder: secondBinder,
					},
				},
				shimCredentials:    []*Credential{&pskSHA256Credential},
				shouldFail:         true,
				expectedLocalError: "remote error: illegal parameter",
				expectedError:      ":PRE_SHARED_KEY_MUST_BE_LAST:",
			})
		}

		// The server can defer configuring PSKs to either the early callback
		// or the SSL_CTX_set_cert_cb callback. (-async causes the shim to defer
		// installing credentials. -use-early-callback controls which callback
		// installs it.)
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-CertCallback-%s", protocol),
			config: Config{
				MaxVersion: VersionTLS13,
				Credential: &pskSHA256Credential,
			},
			shimCredentials: []*Credential{&pskSHA256Credential},
			flags:           []string{"-async", "-expect-selected-credential", "0"},
			expectations: connectionExpectations{
				selectedPSK: &pskSHA256Credential,
			},
		})
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-EarlyCallback-%s", protocol),
			config: Config{
				MaxVersion: VersionTLS13,
				Credential: &pskSHA256Credential,
			},
			shimCredentials: []*Credential{&pskSHA256Credential},
			flags:           []string{"-async", "-use-early-callback", "-expect-selected-credential", "0"},
			expectations: connectionExpectations{
				selectedPSK: &pskSHA256Credential,
			},
		})

		// If a server is configured with multiple PSKs, it selects the
		// first common one.
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-ConsiderMultiplePSKs-%s", protocol),
			config: Config{
				MaxVersion:     VersionTLS13,
				PSKCredentials: []*Credential{&pskSHA256Credential, &pskSHA384Credential},
			},
			shimCredentials: []*Credential{&pskSHA256Credential2, &pskSHA384Credential},
			flags:           []string{"-expect-selected-credential", "1"},
			expectations: connectionExpectations{
				selectedPSK: &pskSHA384Credential,
			},
		})

		// The client and server have no PSKs in common.
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-NoCommonPSKs-%s", protocol),
			config: Config{
				MaxVersion:     VersionTLS13,
				PSKCredentials: []*Credential{&pskSHA256Credential, &pskSHA384Credential},
			},
			shimCredentials:    []*Credential{&pskSHA256Credential2},
			shouldFail:         true,
			expectedError:      ":PSK_IDENTITY_NOT_FOUND:",
			expectedLocalError: "remote error: handshake failure",
		})

		// If the server sends HelloRetryRequest, the client may filter its PSK list
		// based on the selected cipher. The server must send its PSK index based
		// on the second list, not the first.
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-HRR-UpdateIndex-%s", protocol),
			config: Config{
				MaxVersion:     VersionTLS13,
				PSKCredentials: []*Credential{&pskSHA256Credential, &pskSHA384Credential},
				DefaultCurves:  []CurveID{}, // Trigger HRR
				Bugs: ProtocolBugs{
					OmitPSKsOnSecondClientHello: 1,
				},
			},
			shimCredentials: []*Credential{&pskSHA384Credential},
			flags:           []string{"-expect-selected-credential", "0"},
			expectations: connectionExpectations{
				selectedPSK: &pskSHA384Credential,
			},
		})

		// If the PSK is missing from the second ClientHello, the server should
		// reject the connection.
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-HRR-PSKMissing-%s", protocol),
			config: Config{
				MaxVersion:     VersionTLS13,
				PSKCredentials: []*Credential{&pskSHA256Credential, &pskSHA384Credential},
				DefaultCurves:  []CurveID{}, // Trigger HRR
				Bugs: ProtocolBugs{
					OmitPSKsOnSecondClientHello: 2, // Delete both SHA-256 and SHA-384 variants.
				},
			},
			shimCredentials:    []*Credential{&pskSHA256Credential},
			shouldFail:         true,
			expectedLocalError: "remote error: illegal parameter",
			expectedError:      ":PSK_IDENTITY_NOT_FOUND:",
		})

		testCases = append(testCases, testCase{
			protocol: protocol,
			testType: serverTest,
			name:     fmt.Sprintf("PSK-Server-OmitAllPSKsOnSecondClientHello-%s", protocol),
			config: Config{
				MaxVersion:    VersionTLS13,
				Credential:    &pskSHA256Credential,
				DefaultCurves: []CurveID{}, // Trigger HRR
				Bugs: ProtocolBugs{
					OmitAllPSKsOnSecondClientHello: true,
				},
			},
			shimCredentials:    []*Credential{&pskSHA256Credential},
			shouldFail:         true,
			expectedLocalError: "remote error: missing extension",
			expectedError:      ":MISSING_EXTENSION:",
		})

		// The imported PSK must match exactly, or it is not in common.
		extraBytes := pskSHA256Credential
		extraBytes.AppendToImportedPSKIdentity = []byte("extra")
		wrongHash := pskSHA256Credential
		wrongHash.ImportTargetPSKHashes = []crypto.Hash{0}
		wrongProtocol := pskSHA256Credential
		wrongProtocol.ImportTargetPSKProtocol = 0x1234
		wrongProtocol2 := pskSHA256Credential
		wrongProtocol2.ImportTargetPSKProtocol = VersionDTLS13
		wrongContext := pskSHA256Credential
		wrongContext.PSKContext = []byte("wrong context")
		if protocol == dtls {
			wrongProtocol2.ImportTargetPSKProtocol = VersionTLS13
		}
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-IdentityDoesNotMatch-%s", protocol),
			config: Config{
				MaxVersion: VersionTLS13,
				PSKCredentials: []*Credential{
					&extraBytes,
					&wrongHash,
					&wrongProtocol,
					&wrongProtocol2,
					&wrongContext,
				},
			},
			shimCredentials:    []*Credential{&pskSHA256Credential},
			shouldFail:         true,
			expectedError:      ":PSK_IDENTITY_NOT_FOUND:",
			expectedLocalError: "remote error: handshake failure",
		})

		// If multiple PSKs match, the server's order is used.
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-ServerPreferenceOrder-%s", protocol),
			config: Config{
				MaxVersion:     VersionTLS13,
				PSKCredentials: []*Credential{&pskSHA384Credential, &pskSHA256Credential},
			},
			shimCredentials: []*Credential{&pskSHA256Credential, &pskSHA384Credential},
			flags:           []string{"-expect-selected-credential", "0"},
			expectations: connectionExpectations{
				selectedPSK: &pskSHA256Credential,
			},
		})

		// Servers can be configured with both PSKs and certificates,
		// in which case they evaluate based on their preference order.
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-PSKOrCert-PSK-%s", protocol),
			config: Config{
				MaxVersion:     VersionTLS13,
				PSKCredentials: []*Credential{&pskSHA256Credential},
			},
			shimCredentials: []*Credential{
				&pskSHA256Credential,
				&rsaCertificate,
			},
			// The ClientHello works for both, but the shim should
			// pick the PSK.
			flags: []string{"-expect-selected-credential", "0"},
			expectations: connectionExpectations{
				selectedPSK: &pskSHA256Credential,
			},
		})
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-PSKOrCert-Cert-%s", protocol),
			config: Config{
				MaxVersion:     VersionTLS13,
				PSKCredentials: []*Credential{&pskSHA384Credential}, // Wrong PSK
			},
			shimCredentials: []*Credential{
				&pskSHA256Credential,
				&rsaCertificate,
			},
			// The ClientHello is not good for the PSK, so the shim
			// should pick the certificate.
			flags: []string{"-expect-selected-credential", "1"},
			expectations: connectionExpectations{
				peerCertificate: &rsaCertificate,
			},
		})
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-CertOrPSK-Cert-%s", protocol),
			config: Config{
				MaxVersion:     VersionTLS13,
				PSKCredentials: []*Credential{&pskSHA256Credential},
			},
			shimCredentials: []*Credential{
				&rsaCertificate,
				&pskSHA256Credential,
			},
			// The ClientHello works for both, but the shim should
			// pick the certificate.
			flags: []string{"-expect-selected-credential", "0"},
			expectations: connectionExpectations{
				peerCertificate: &rsaCertificate,
			},
		})
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-CertOrPSK-PSK-%s", protocol),
			config: Config{
				MaxVersion:                VersionTLS13,
				PSKCredentials:            []*Credential{&pskSHA256Credential},
				VerifySignatureAlgorithms: []signatureAlgorithm{}, // No common algs
			},
			shimCredentials: []*Credential{
				&rsaCertificate,
				&pskSHA256Credential,
			},
			// The ClientHello is not good for the certficate, so the
			// shim should pick the PSK.
			flags: []string{"-expect-selected-credential", "1"},
			expectations: connectionExpectations{
				selectedPSK: &pskSHA256Credential,
			},
		})

		// Clients should import PSKs for each of their supported ciphers.
		// If one does not, the server should skip any PSKs that do not
		// work with the chosen cipher.
		importSHA384Only := pskSHA256Credential
		importSHA384Only.ImportTargetPSKHashes = []crypto.Hash{crypto.SHA384}
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-PartialImport-Match-%s", protocol),
			config: Config{
				MaxVersion:     VersionTLS13,
				CipherSuites:   []uint16{TLS_AES_256_GCM_SHA384},
				PSKCredentials: []*Credential{&importSHA384Only},
			},
			shimCredentials: []*Credential{&pskSHA256Credential},
			flags:           []string{"-expect-selected-credential", "0"},
			expectations: connectionExpectations{
				selectedPSK: &importSHA384Only,
			},
		})
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-PartialImport-NoMatch-%s", protocol),
			config: Config{
				MaxVersion:     VersionTLS13,
				CipherSuites:   []uint16{TLS_AES_128_GCM_SHA256},
				PSKCredentials: []*Credential{&importSHA384Only},
			},
			shimCredentials:    []*Credential{&pskSHA256Credential},
			shouldFail:         true,
			expectedError:      ":PSK_IDENTITY_NOT_FOUND:",
			expectedLocalError: "remote error: handshake failure",
		})
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-PartialImport-NoMatch-Fallback-%s", protocol),
			config: Config{
				MaxVersion:     VersionTLS13,
				CipherSuites:   []uint16{TLS_AES_128_GCM_SHA256},
				PSKCredentials: []*Credential{&importSHA384Only},
			},
			shimCredentials: []*Credential{&pskSHA256Credential, &rsaCertificate},
			flags:           []string{"-expect-selected-credential", "1"},
			expectations: connectionExpectations{
				peerCertificate: &rsaCertificate,
			},
		})

		// We only implement psk_dhe_ke. If the client does not offer it, PSKs
		// are not eligible.
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-MissingPSKMode-NoMatch-%s", protocol),
			config: Config{
				MaxVersion:     VersionTLS13,
				PSKCredentials: []*Credential{&pskSHA256Credential},
				Bugs: ProtocolBugs{
					SendPSKKeyExchangeModes: []byte{0x1a},
				},
			},
			shimCredentials:    []*Credential{&pskSHA256Credential},
			shouldFail:         true,
			expectedError:      ":NO_SUPPORTED_PSK_MODE:",
			expectedLocalError: "remote error: handshake failure",
		})
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     fmt.Sprintf("PSK-Server-MissingPSKMode-NoMatch-Fallback-%s", protocol),
			config: Config{
				MaxVersion:     VersionTLS13,
				PSKCredentials: []*Credential{&pskSHA256Credential},
				Bugs: ProtocolBugs{
					SendPSKKeyExchangeModes: []byte{0x1a},
				},
			},
			shimCredentials: []*Credential{&pskSHA256Credential, &rsaCertificate},
			flags:           []string{"-expect-selected-credential", "1"},
			expectations: connectionExpectations{
				peerCertificate: &rsaCertificate,
			},
		})
	}
}
