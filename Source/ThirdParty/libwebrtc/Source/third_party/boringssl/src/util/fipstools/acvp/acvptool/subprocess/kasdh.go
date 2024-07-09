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

type kasDHVectorSet struct {
	Groups []kasDHTestGroup `json:"testGroups"`
}

type kasDHTestGroup struct {
	ID     uint64      `json:"tgId"`
	Type   string      `json:"testType"`
	Role   string      `json:"kasRole"`
	Mode   string      `json:"kasMode"`
	Scheme string      `json:"scheme"`
	PHex   string      `json:"p"`
	QHex   string      `json:"q"`
	GHex   string      `json:"g"`
	Tests  []kasDHTest `json:"tests"`
}

type kasDHTest struct {
	ID            uint64 `json:"tcId"`
	PeerPublicHex string `json:"ephemeralPublicServer"`
	PrivateKeyHex string `json:"ephemeralPrivateIut"`
	PublicKeyHex  string `json:"ephemeralPublicIut"`
	ResultHex     string `json:"z"`
}

type kasDHTestGroupResponse struct {
	ID    uint64              `json:"tgId"`
	Tests []kasDHTestResponse `json:"tests"`
}

type kasDHTestResponse struct {
	ID             uint64 `json:"tcId"`
	LocalPublicHex string `json:"ephemeralPublicIut,omitempty"`
	ResultHex      string `json:"z,omitempty"`
	Passed         *bool  `json:"testPassed,omitempty"`
}

type kasDH struct{}

func (k *kasDH) Process(vectorSet []byte, m Transactable) (any, error) {
	var parsed kasDHVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	// See https://pages.nist.gov/ACVP/draft-hammett-acvp-kas-ffc-sp800-56ar3.html
	var ret []kasDHTestGroupResponse
	for _, group := range parsed.Groups {
		group := group
		response := kasDHTestGroupResponse{
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

		switch group.Role {
		case "initiator", "responder":
			break
		default:
			return nil, fmt.Errorf("unknown role %q", group.Role)
		}

		if group.Scheme != "dhEphem" {
			return nil, fmt.Errorf("unknown scheme %q", group.Scheme)
		}

		p, err := hex.DecodeString(group.PHex)
		if err != nil {
			return nil, err
		}

		q, err := hex.DecodeString(group.QHex)
		if err != nil {
			return nil, err
		}

		g, err := hex.DecodeString(group.GHex)
		if err != nil {
			return nil, err
		}

		const method = "FFDH"
		for _, test := range group.Tests {
			test := test

			if len(test.PeerPublicHex) == 0 {
				return nil, fmt.Errorf("%d/%d is missing peer's key", group.ID, test.ID)
			}

			peerPublic, err := hex.DecodeString(test.PeerPublicHex)
			if err != nil {
				return nil, err
			}

			if (len(test.PrivateKeyHex) != 0) != privateKeyGiven {
				return nil, fmt.Errorf("%d/%d incorrect private key presence", group.ID, test.ID)
			}

			if privateKeyGiven {
				privateKey, err := hex.DecodeString(test.PrivateKeyHex)
				if err != nil {
					return nil, err
				}

				publicKey, err := hex.DecodeString(test.PublicKeyHex)
				if err != nil {
					return nil, err
				}

				expectedOutput, err := hex.DecodeString(test.ResultHex)
				if err != nil {
					return nil, err
				}

				m.TransactAsync(method, 2, [][]byte{p, q, g, peerPublic, privateKey, publicKey}, func(result [][]byte) error {
					ok := bytes.Equal(result[1], expectedOutput)
					response.Tests = append(response.Tests, kasDHTestResponse{
						ID:     test.ID,
						Passed: &ok,
					})
					return nil
				})
			} else {
				m.TransactAsync(method, 2, [][]byte{p, q, g, peerPublic, nil, nil}, func(result [][]byte) error {
					response.Tests = append(response.Tests, kasDHTestResponse{
						ID:             test.ID,
						LocalPublicHex: hex.EncodeToString(result[0]),
						ResultHex:      hex.EncodeToString(result[1]),
					})
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
