// Copyright 2025 The BoringSSL Authors
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
	"bytes"
	"fmt"

	"golang.org/x/crypto/cryptobyte"
)

func trustAnchorListFlagValue(ids ...[]byte) string {
	b := cryptobyte.NewBuilder(nil)
	for _, id := range ids {
		addUint8LengthPrefixedBytes(b, id)
	}
	return base64FlagValue(b.BytesOrPanic())
}

func addTrustAnchorTests() {
	id1 := []byte{1}
	id2 := []byte{2, 2}
	id3 := []byte{3, 3, 3}
	id4 := []byte{4, 4, 4, 4}

	// Unsolicited trust_anchors extensions should be rejected.
	testCases = append(testCases, testCase{
		name: "TrustAnchors-Unsolicited-Certificate",
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				AlwaysMatchTrustAnchorID: true,
			},
		},
		shouldFail:         true,
		expectedLocalError: "remote error: unsupported extension",
		expectedError:      ":UNEXPECTED_EXTENSION:",
	})
	testCases = append(testCases, testCase{
		name: "TrustAnchors-Unsolicited-EncryptedExtensions",
		config: Config{
			MinVersion:            VersionTLS13,
			AvailableTrustAnchors: [][]byte{id1, id2},
			Bugs: ProtocolBugs{
				AlwaysSendAvailableTrustAnchors: true,
			},
		},
		shouldFail:         true,
		expectedLocalError: "remote error: unsupported extension",
		expectedError:      ":UNEXPECTED_EXTENSION:",
	})

	// Test that the client sends trust anchors when configured, and correctly
	// reports the server's response.
	testCases = append(testCases, testCase{
		name: "TrustAnchors-ClientRequest-Match",
		config: Config{
			MinVersion:            VersionTLS13,
			AvailableTrustAnchors: [][]byte{id1, id2},
			Credential:            rsaChainCertificate.WithTrustAnchorID(id1),
			Bugs: ProtocolBugs{
				ExpectPeerRequestedTrustAnchors: [][]byte{id1, id3},
			},
		},
		flags: []string{
			"-requested-trust-anchors", trustAnchorListFlagValue(id1, id3),
			"-expect-peer-match-trust-anchor",
			"-expect-peer-available-trust-anchors", trustAnchorListFlagValue(id1, id2),
		},
	})
	// The client should not like it if the server indicates the match with a non-empty
	// extension.
	testCases = append(testCases, testCase{
		name: "TrustAnchors-ClientRequest-Match-Non-Empty-Extension",
		config: Config{
			MinVersion:            VersionTLS13,
			AvailableTrustAnchors: [][]byte{id1, id2},
			Credential:            rsaChainCertificate.WithTrustAnchorID(id1),
			Bugs: ProtocolBugs{
				SendNonEmptyTrustAnchorMatch:    true,
				ExpectPeerRequestedTrustAnchors: [][]byte{id1, id3},
			},
		},
		flags: []string{
			"-requested-trust-anchors", trustAnchorListFlagValue(id1, id3),
		},
		shouldFail:         true,
		expectedLocalError: "remote error: error decoding message",
		expectedError:      ":ERROR_PARSING_EXTENSION:",
	})
	// The client should not like it if the server indicates the match on the incorrect
	// certificate in the Certificate message.
	testCases = append(testCases, testCase{
		name: "TrustAnchors-ClientRequest-Match-On-Incorrect-Certificate",
		config: Config{
			MinVersion:            VersionTLS13,
			AvailableTrustAnchors: [][]byte{id1, id2},
			Credential:            rsaChainCertificate.WithTrustAnchorID(id1),
			Bugs: ProtocolBugs{
				SendTrustAnchorWrongCertificate: true,
				ExpectPeerRequestedTrustAnchors: [][]byte{id1, id3},
			},
		},
		flags: []string{
			"-requested-trust-anchors", trustAnchorListFlagValue(id1, id3),
		},
		shouldFail:         true,
		expectedLocalError: "remote error: unsupported extension",
		expectedError:      ":UNEXPECTED_EXTENSION:",
	})
	testCases = append(testCases, testCase{
		name: "TrustAnchors-ClientRequest-NoMatch",
		config: Config{
			MinVersion:            VersionTLS13,
			AvailableTrustAnchors: [][]byte{id1, id2},
			Bugs: ProtocolBugs{
				ExpectPeerRequestedTrustAnchors: [][]byte{id3},
			},
		},
		flags: []string{
			"-requested-trust-anchors", trustAnchorListFlagValue(id3),
			"-expect-no-peer-match-trust-anchor",
			"-expect-peer-available-trust-anchors", trustAnchorListFlagValue(id1, id2),
		},
	})

	// Invalid trust anchor IDs are rejected in ClientHello, EncryptedExtensions,
	// and configuration.
	invalidIDs := []struct {
		name string
		id   []byte
	}{
		{"Empty", []byte{}},
		{"TooLong", bytes.Repeat([]byte{5}, 33)},
		{"NonMinimalComponent", []byte{0x80, 0x01}},
		{"TruncatedComponent", []byte{0xff}},
	}
	for _, id := range invalidIDs {
		testCases = append(testCases, testCase{
			testType: serverTest,
			name:     fmt.Sprintf("TrustAnchors-%s-ClientHello", id.name),
			config: Config{
				MinVersion:          VersionTLS13,
				RequestTrustAnchors: [][]byte{id.id},
			},
			shouldFail:    true,
			expectedError: ":DECODE_ERROR:",
		})
		testCases = append(testCases, testCase{
			name: fmt.Sprintf("TrustAnchors-%s-EncryptedExtensions", id.name),
			config: Config{
				MinVersion:            VersionTLS13,
				AvailableTrustAnchors: [][]byte{id.id},
			},
			flags:         []string{"-requested-trust-anchors", trustAnchorListFlagValue(id1)},
			shouldFail:    true,
			expectedError: ":DECODE_ERROR:",
		})
		testCases = append(testCases, testCase{
			testType:        serverTest,
			name:            fmt.Sprintf("TrustAnchors-%s-ConfigCredential", id.name),
			shimCredentials: []*Credential{rsaCertificate.WithTrustAnchorID(id.id)},
			shouldFail:      true,
			expectedError:   ":INVALID_TRUST_ANCHOR_ID:",
		})
		testCases = append(testCases, testCase{
			name:          fmt.Sprintf("TrustAnchors-%s-ConfigRequest", id.name),
			flags:         []string{"-requested-trust-anchors", trustAnchorListFlagValue(id.id)},
			shouldFail:    true,
			expectedError: ":INVALID_TRUST_ANCHOR_LIST:",
		})
	}

	// Test the server selection logic, as well as whether it correctly reports
	// available trust anchors and the match status. (The general selection flow
	// is covered in addCertificateSelectionTests.)
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-ServerSelect-Match",
		config: Config{
			MinVersion:          VersionTLS13,
			RequestTrustAnchors: [][]byte{id2},
			Bugs: ProtocolBugs{
				ExpectPeerAvailableTrustAnchors: [][]byte{id1, id2},
				ExpectPeerMatchTrustAnchor:      new(true),
			},
		},
		shimCredentials: []*Credential{
			rsaCertificate.WithTrustAnchorID(id1),
			rsaCertificate.WithTrustAnchorID(id2),
		},
		flags: []string{"-expect-selected-credential", "1"},
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-ServerSelect-None",
		config: Config{
			MinVersion:          VersionTLS13,
			RequestTrustAnchors: [][]byte{id1},
		},
		shimCredentials: []*Credential{
			rsaCertificate.WithTrustAnchorID(id2),
			rsaCertificate.WithTrustAnchorID(id3),
		},
		shouldFail:    true,
		expectedError: ":NO_MATCHING_ISSUER:",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-ServerSelect-Fallback",
		config: Config{
			MinVersion:          VersionTLS13,
			RequestTrustAnchors: [][]byte{id1},
			Bugs: ProtocolBugs{
				ExpectPeerAvailableTrustAnchors: [][]byte{id2, id3},
				ExpectPeerMatchTrustAnchor:      new(false),
			},
		},
		shimCredentials: []*Credential{
			rsaCertificate.WithTrustAnchorID(id2),
			rsaCertificate.WithTrustAnchorID(id3),
			&rsaCertificate,
		},
		flags: []string{"-expect-selected-credential", "2"},
	})

	// When the server sends available trust anchors, it should filter the list
	// by whether the credential would be usable with the connection at all.
	p256DC := createDelegatedCredential(&rsaCertificate, delegatedCredentialConfig{
		dcAlgo: signatureECDSAWithP256AndSHA256,
		algo:   signatureRSAPSSWithSHA256,
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-Server-FilterAvailable",
		config: Config{
			MinVersion:                VersionTLS13,
			RequestTrustAnchors:       [][]byte{id1},
			VerifySignatureAlgorithms: []signatureAlgorithm{signatureRSAPSSWithSHA256, signatureECDSAWithP256AndSHA256},
			Bugs: ProtocolBugs{
				ExpectPeerAvailableTrustAnchors: [][]byte{id2},
				ExpectPeerMatchTrustAnchor:      new(false),
			},
		},
		shimCredentials: []*Credential{
			rsaCertificate.WithTrustAnchorID(id2),
			// Ineligible because of signature algorithms.
			rsaCertificate.WithTrustAnchorID(id3).WithSignatureAlgorithms(signatureRSAPSSWithSHA384),
			// Ineligible because of credential type.
			p256DC.WithTrustAnchorID(id4),
			&rsaCertificate,
		},
		flags: []string{"-expect-selected-credential", "3"},
	})

	// After filtering, the list of available trust anchors may even be empty.
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-Server-FilterAvailable-Empty",
		config: Config{
			MinVersion:                VersionTLS13,
			RequestTrustAnchors:       [][]byte{id1},
			VerifySignatureAlgorithms: []signatureAlgorithm{signatureRSAPSSWithSHA256, signatureECDSAWithP256AndSHA256},
			Bugs: ProtocolBugs{
				ExpectPeerAvailableTrustAnchors: [][]byte{},
				ExpectPeerMatchTrustAnchor:      new(false),
			},
		},
		shimCredentials: []*Credential{
			// Ineligible because of signature algorithms.
			rsaCertificate.WithTrustAnchorID(id2).WithSignatureAlgorithms(signatureRSAPSSWithSHA384),
			// Ineligible because of credential type.
			p256DC.WithTrustAnchorID(id4),
			&rsaCertificate,
		},
		flags: []string{"-expect-selected-credential", "2"},
	})

	// The caller can override the default available list.
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-Server-OverrideAvailable",
		config: Config{
			RequestTrustAnchors: [][]byte{id1},
			Bugs: ProtocolBugs{
				ExpectPeerAvailableTrustAnchors: [][]byte{id1, id2, id3},
			},
		},
		shimCredentials: []*Credential{&rsaCertificate},
		flags:           []string{"-available-trust-anchors", trustAnchorListFlagValue(id1, id2, id3)},
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-Server-OverrideAvailable-NotRequested",
		config: Config{
			Bugs: ProtocolBugs{
				ExpectPeerAvailableTrustAnchors: [][]byte{},
			},
		},
		shimCredentials: []*Credential{&rsaCertificate},
		flags:           []string{"-available-trust-anchors", trustAnchorListFlagValue(id1, id2, id3)},
	})

	// The ClientHello list may be empty. The client must be able to send it and
	// receive available trust anchors.
	testCases = append(testCases, testCase{
		name: "TrustAnchors-ClientRequestEmpty",
		config: Config{
			MinVersion:            VersionTLS13,
			AvailableTrustAnchors: [][]byte{id1, id2},
			Bugs: ProtocolBugs{
				ExpectPeerRequestedTrustAnchors: [][]byte{},
			},
		},
		flags: []string{
			"-requested-trust-anchors", trustAnchorListFlagValue(),
			"-expect-peer-available-trust-anchors", trustAnchorListFlagValue(id1, id2),
		},
	})
	// The server must be able to process it, and send available trust anchors.
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-ServerReceiveEmptyRequest",
		config: Config{
			MinVersion:          VersionTLS13,
			RequestTrustAnchors: [][]byte{},
			Bugs: ProtocolBugs{
				ExpectPeerAvailableTrustAnchors: [][]byte{id1, id2},
				ExpectPeerMatchTrustAnchor:      new(false),
			},
		},
		shimCredentials: []*Credential{
			rsaCertificate.WithTrustAnchorID(id1),
			rsaCertificate.WithTrustAnchorID(id2),
			&rsaCertificate,
		},
		flags: []string{"-expect-selected-credential", "2"},
	})

	// This extension requires TLS 1.3. If a server receives this and negotiates
	// TLS 1.2, it should ignore the extension and not accidentally send
	// something in ServerHello (implicitly checked by runner).
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-TLS12-Server",
		config: Config{
			MaxVersion:          VersionTLS12,
			RequestTrustAnchors: [][]byte{id1},
		},
		shimCredentials: []*Credential{
			rsaCertificate.WithTrustAnchorID(id1),
			&rsaCertificate,
		},
		// The first credential is skipped because the extension is ignored.
		flags: []string{"-expect-selected-credential", "1"},
	})
	// The client should reject the extension in TLS 1.2 ServerHello.
	testCases = append(testCases, testCase{
		name: "TrustAnchors-TLS12-Client",
		config: Config{
			MaxVersion:            VersionTLS12,
			AvailableTrustAnchors: [][]byte{id1},
			Bugs: ProtocolBugs{
				AlwaysSendAvailableTrustAnchors: true,
			},
		},
		flags:              []string{"-requested-trust-anchors", trustAnchorListFlagValue(id1)},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_EXTENSION:",
		expectedLocalError: "remote error: unsupported extension",
	})

	// Having a trust_anchor_id property does not, on its own, gate the credential
	// on an issuer match.
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-NoTrustAnchorNegotiationProperty",
		config: Config{
			MinVersion:          VersionTLS13,
			RequestTrustAnchors: [][]byte{id2},
		},
		shimCredentials: []*Credential{
			rsaCertificate.WithProperties(CertificatePropertyList{
				TrustAnchorID: id1,
			}),
			ecdsaP256Certificate.WithTrustAnchorID(id2),
		},
		// The first credential is chosen because it is not gated on a trust anchors.
		flags: []string{"-expect-selected-credential", "0"},
		expectations: connectionExpectations{
			peerCertificate: &rsaCertificate,
		},
	})

	// The trust_anchor_negotiation property, however, does gate it.
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-TrustAnchorNegotiationProperty",
		config: Config{
			MinVersion:          VersionTLS13,
			RequestTrustAnchors: [][]byte{id2},
		},
		shimCredentials: []*Credential{
			rsaCertificate.WithProperties(CertificatePropertyList{
				TrustAnchorID:          id1,
				TrustAnchorNegotiation: true,
			}),
			ecdsaP256Certificate.WithTrustAnchorID(id2),
		},
		// The second credential is chosen because the first is gated on trust
		// anchors and does not match.
		flags: []string{"-expect-selected-credential", "1"},
		expectations: connectionExpectations{
			peerCertificate: &ecdsaP256Certificate,
		},
	})

	group := MakeTrustAnchorID(32473, 100, 200, 300, 400)
	// These ranges match the test group.
	match := MakeTrustAnchorIDPattern(32473).Range(50, 100).Exact(200).Range(300, 350).AtLeast(350).Bytes()
	matchExact := MakeTrustAnchorIDPattern(32473, 100, 200, 300, 400).Bytes()
	// These ranges do not.
	tooShort := MakeTrustAnchorIDPattern(32473, 100, 200, 300).Bytes()
	tooLong := MakeTrustAnchorIDPattern(32473, 100, 200, 300, 400, 0).Bytes()
	componentTooLow := MakeTrustAnchorIDPattern(32473, 100, 200).Range(301, 350).Exact(400).Bytes()
	componentTooHigh := MakeTrustAnchorIDPattern(32473, 100, 200).Range(250, 299).Exact(400).Bytes()

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchorGroups-Match-Server",
		config: Config{
			MinVersion:          VersionTLS13,
			RequestTrustAnchors: [][]byte{group},
			Bugs: ProtocolBugs{
				ExpectPeerAvailableTrustAnchors: [][]byte{id1, id2},
				ExpectPeerMatchTrustAnchor:      new(true),
			},
		},
		shimCredentials: []*Credential{
			// Neither the ID nor the group inclusions match.
			rsaCertificate.WithMustMatchIssuer(true).WithProperties(CertificatePropertyList{
				TrustAnchorID:     id1,
				TrustAnchorGroups: [][]byte{tooShort, tooLong, componentTooLow, componentTooHigh},
			}),
			// The group inclusions match, after skipping some irrelevant inclusions.
			ecdsaP256Certificate.WithMustMatchIssuer(true).WithProperties(CertificatePropertyList{
				TrustAnchorID:     id2,
				TrustAnchorGroups: [][]byte{tooShort, match},
			}),
		},
		expectations: connectionExpectations{
			peerCertificate: &ecdsaP256Certificate,
		},
		flags: []string{"-expect-selected-credential", "1"},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchorGroups-MatchExact-Server",
		config: Config{
			MinVersion:          VersionTLS13,
			RequestTrustAnchors: [][]byte{group},
			Bugs: ProtocolBugs{
				ExpectPeerAvailableTrustAnchors: [][]byte{id1, id2},
				ExpectPeerMatchTrustAnchor:      new(true),
			},
		},
		shimCredentials: []*Credential{
			// Neither the ID nor the group inclusions match.
			rsaCertificate.WithMustMatchIssuer(true).WithProperties(CertificatePropertyList{
				TrustAnchorID:     id1,
				TrustAnchorGroups: [][]byte{tooShort, tooLong, componentTooLow, componentTooHigh},
			}),
			// The group inclusions match, after skipping some irrelevant inclusions.
			ecdsaP256Certificate.WithMustMatchIssuer(true).WithProperties(CertificatePropertyList{
				TrustAnchorID:     id2,
				TrustAnchorGroups: [][]byte{tooShort, matchExact},
			}),
		},
		expectations: connectionExpectations{
			peerCertificate: &ecdsaP256Certificate,
		},
		flags: []string{"-expect-selected-credential", "1"},
	})
}
