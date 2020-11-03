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

// The following structures reflect the JSON of CMAC-AES tests. See
// https://usnistgov.github.io/ACVP/artifacts/acvp_sub_mac.html#rfc.section.4.2

type keyedMACTestVectorSet struct {
	Groups []keyedMACTestGroup `json:"testGroups"`
}

type keyedMACTestGroup struct {
	ID        uint64 `json:"tgId"`
	Type      string `json:"testType"`
	Direction string `json:"direction"`
	MsgBits   uint32 `json:"msgLen"`
	KeyBits   uint32 `json:"keyLen"`
	MACBits   uint32 `json:"macLen"`
	Tests     []struct {
		ID     uint64 `json:"tcId"`
		KeyHex string `json:"key"`
		MsgHex string `json:"message"`
		MACHex string `json:"mac"`
	}
}

type keyedMACTestGroupResponse struct {
	ID    uint64                 `json:"tgId"`
	Tests []keyedMACTestResponse `json:"tests"`
}

type keyedMACTestResponse struct {
	ID     uint64 `json:"tcId"`
	MACHex string `json:"mac,omitempty"`
	Passed *bool  `json:"testPassed,omitempty"`
}

type keyedMACPrimitive struct {
	algo string
}

func (k *keyedMACPrimitive) Process(vectorSet []byte, m Transactable) (interface{}, error) {
	var vs keyedMACTestVectorSet
	if err := json.Unmarshal(vectorSet, &vs); err != nil {
		return nil, err
	}

	var respGroups []keyedMACTestGroupResponse
	for _, group := range vs.Groups {
		respGroup := keyedMACTestGroupResponse{ID: group.ID}

		if group.KeyBits%8 != 0 {
			return nil, fmt.Errorf("%d bit key in test group %d: fractional bytes not supported", group.KeyBits, group.ID)
		}
		if group.MsgBits%8 != 0 {
			return nil, fmt.Errorf("%d bit message in test group %d: fractional bytes not supported", group.KeyBits, group.ID)
		}
		if group.MACBits%8 != 0 {
			return nil, fmt.Errorf("%d bit MAC in test group %d: fractional bytes not supported", group.KeyBits, group.ID)
		}

		var generate bool
		switch group.Direction {
		case "gen":
			generate = true
		case "ver":
			generate = false
		default:
			return nil, fmt.Errorf("unknown test direction %q in test group %d", group.Direction, group.ID)
		}

		outputBytes := uint32le(group.MACBits / 8)

		for _, test := range group.Tests {
			respTest := keyedMACTestResponse{ID: test.ID}

			// Validate input.
			if keyBits := uint32(len(test.KeyHex)) * 4; keyBits != group.KeyBits {
				return nil, fmt.Errorf("test case %d/%d contains key of length %d bits, but expected %d-bit value", group.ID, test.ID, keyBits, group.KeyBits)
			}
			if msgBits := uint32(len(test.MsgHex)) * 4; msgBits != group.MsgBits {
				return nil, fmt.Errorf("test case %d/%d contains message of length %d bits, but expected %d-bit value", group.ID, test.ID, msgBits, group.MsgBits)
			}

			if generate {
				if len(test.MACHex) != 0 {
					return nil, fmt.Errorf("test case %d/%d contains MAC but should not", group.ID, test.ID)
				}
			} else {
				if macBits := uint32(len(test.MACHex)) * 4; macBits != group.MACBits {
					return nil, fmt.Errorf("test case %d/%d contains MAC of length %d bits, but expected %d-bit value", group.ID, test.ID, macBits, group.MACBits)
				}
			}

			// Set up Transact parameters.
			key, err := hex.DecodeString(test.KeyHex)
			if err != nil {
				return nil, fmt.Errorf("failed to decode KeyHex in test case %d/%d: %v", group.ID, test.ID, err)
			}

			msg, err := hex.DecodeString(test.MsgHex)
			if err != nil {
				return nil, fmt.Errorf("failed to decode MsgHex in test case %d/%d: %v", group.ID, test.ID, err)
			}

			result, err := m.Transact(k.algo, 1, outputBytes, key, msg)
			if err != nil {
				return nil, fmt.Errorf("wrapper %s operation failed: %s", k.algo, err)
			}

			calculatedMAC := result[0]
			if len(calculatedMAC) != int(group.MACBits/8) {
				return nil, fmt.Errorf("%s operation returned incorrect length value", k.algo)
			}

			if generate {
				respTest.MACHex = hex.EncodeToString(calculatedMAC)
			} else {
				expectedMAC, err := hex.DecodeString(test.MACHex)
				if err != nil {
					return nil, fmt.Errorf("failed to decode MACHex in test case %d/%d: %v", group.ID, test.ID, err)
				}
				ok := bytes.Equal(calculatedMAC, expectedMAC)
				respTest.Passed = &ok
			}

			respGroup.Tests = append(respGroup.Tests, respTest)
		}

		respGroups = append(respGroups, respGroup)
	}

	return respGroups, nil
}
