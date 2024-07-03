// Copyright (c) 2020, Google Inc.
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
// OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

package subprocess

import (
	"bytes"
	"encoding/hex"
	"encoding/json"
	"fmt"
)

type kasVectorSet struct {
	Groups []kasTestGroup `json:"testGroups"`
}

type kasTestGroup struct {
	ID     uint64    `json:"tgId"`
	Type   string    `json:"testType"`
	Curve  string    `json:"domainParameterGenerationMode"`
	Role   string    `json:"kasRole"`
	Scheme string    `json:"scheme"`
	Tests  []kasTest `json:"tests"`
}

type kasTest struct {
	ID uint64 `json:"tcId"`

	EphemeralXHex          string `json:"ephemeralPublicServerX"`
	EphemeralYHex          string `json:"ephemeralPublicServerY"`
	EphemeralPrivateKeyHex string `json:"ephemeralPrivateIut"`

	StaticXHex          string `json:"staticPublicServerX"`
	StaticYHex          string `json:"staticPublicServerY"`
	StaticPrivateKeyHex string `json:"staticPrivateIut"`

	ResultHex string `json:"z"`
}

type kasTestGroupResponse struct {
	ID    uint64            `json:"tgId"`
	Tests []kasTestResponse `json:"tests"`
}

type kasTestResponse struct {
	ID uint64 `json:"tcId"`

	EphemeralXHex string `json:"ephemeralPublicIutX,omitempty"`
	EphemeralYHex string `json:"ephemeralPublicIutY,omitempty"`

	StaticXHex string `json:"staticPublicIutX,omitempty"`
	StaticYHex string `json:"staticPublicIutY,omitempty"`

	ResultHex string `json:"z,omitempty"`
	Passed    *bool  `json:"testPassed,omitempty"`
}

type kas struct{}

func (k *kas) Process(vectorSet []byte, m Transactable) (any, error) {
	var parsed kasVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	// See https://pages.nist.gov/ACVP/draft-fussell-acvp-kas-ecc.html#name-test-vectors
	var ret []kasTestGroupResponse
	for _, group := range parsed.Groups {
		group := group
		response := kasTestGroupResponse{
			ID: group.ID,
		}

		var privateKeyGiven bool
		switch group.Type {
		case "AFT":
			privateKeyGiven = false
		case "VAL":
			privateKeyGiven = true
		default:
			return nil, fmt.Errorf("unknown test type %q", group.Type)
		}

		switch group.Curve {
		case "P-224", "P-256", "P-384", "P-521":
			break
		default:
			return nil, fmt.Errorf("unknown curve %q", group.Curve)
		}

		switch group.Role {
		case "initiator", "responder":
			break
		default:
			return nil, fmt.Errorf("unknown role %q", group.Role)
		}

		var useStaticNamedFields bool
		switch group.Scheme {
		case "ephemeralUnified":
			break
		case "staticUnified":
			useStaticNamedFields = true
			break
		default:
			return nil, fmt.Errorf("unknown scheme %q", group.Scheme)
		}

		method := "ECDH/" + group.Curve

		for _, test := range group.Tests {
			test := test

			var xHex, yHex, privateKeyHex string
			if useStaticNamedFields {
				xHex, yHex, privateKeyHex = test.StaticXHex, test.StaticYHex, test.StaticPrivateKeyHex
			} else {
				xHex, yHex, privateKeyHex = test.EphemeralXHex, test.EphemeralYHex, test.EphemeralPrivateKeyHex
			}

			if len(xHex) == 0 || len(yHex) == 0 {
				return nil, fmt.Errorf("%d/%d is missing peer's point", group.ID, test.ID)
			}

			peerX, err := hex.DecodeString(xHex)
			if err != nil {
				return nil, err
			}

			peerY, err := hex.DecodeString(yHex)
			if err != nil {
				return nil, err
			}

			if (len(privateKeyHex) != 0) != privateKeyGiven {
				return nil, fmt.Errorf("%d/%d incorrect private key presence", group.ID, test.ID)
			}

			if privateKeyGiven {
				privateKey, err := hex.DecodeString(privateKeyHex)
				if err != nil {
					return nil, err
				}

				expectedOutput, err := hex.DecodeString(test.ResultHex)
				if err != nil {
					return nil, err
				}

				m.TransactAsync(method, 3, [][]byte{peerX, peerY, privateKey}, func(result [][]byte) error {
					ok := bytes.Equal(result[2], expectedOutput)
					response.Tests = append(response.Tests, kasTestResponse{
						ID:     test.ID,
						Passed: &ok,
					})
					return nil
				})
			} else {
				m.TransactAsync(method, 3, [][]byte{peerX, peerY, nil}, func(result [][]byte) error {
					testResponse := kasTestResponse{
						ID:        test.ID,
						ResultHex: hex.EncodeToString(result[2]),
					}

					if useStaticNamedFields {
						testResponse.StaticXHex = hex.EncodeToString(result[0])
						testResponse.StaticYHex = hex.EncodeToString(result[1])
					} else {
						testResponse.EphemeralXHex = hex.EncodeToString(result[0])
						testResponse.EphemeralYHex = hex.EncodeToString(result[1])
					}

					response.Tests = append(response.Tests, testResponse)
					return nil
				})
			}
		}

		m.Barrier(func() {
			ret = append(ret, response)
		})
	}

	if err := m.Flush(); err != nil {
		return nil, err
	}

	return ret, nil
}
