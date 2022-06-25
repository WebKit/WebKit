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

// The following structures reflect the JSON of ACVP KDF tests. See
// https://pages.nist.gov/ACVP/draft-celi-acvp-kdf-tls.html#name-test-vectors

type kdfTestVectorSet struct {
	Groups []kdfTestGroup `json:"testGroups"`
}

type kdfTestGroup struct {
	ID uint64 `json:"tgId"`
	// KDFMode can take the values "counter", "feedback", or
	// "double pipeline iteration".
	KDFMode         string `json:"kdfMode"`
	MACMode         string `json:"macMode"`
	CounterLocation string `json:"counterLocation"`
	OutputBits      uint32 `json:"keyOutLength"`
	CounterBits     uint32 `json:"counterLength"`
	ZeroIV          bool   `json:"zeroLengthIv"`

	Tests []struct {
		ID       uint64 `json:"tcId"`
		Key      string `json:"keyIn"`
		Deferred bool   `json:"deferred"`
	}
}

type kdfTestGroupResponse struct {
	ID    uint64            `json:"tgId"`
	Tests []kdfTestResponse `json:"tests"`
}

type kdfTestResponse struct {
	ID        uint64 `json:"tcId"`
	KeyIn     string `json:"keyIn,omitempty"`
	FixedData string `json:"fixedData"`
	KeyOut    string `json:"keyOut"`
}

type kdfPrimitive struct{}

func (k *kdfPrimitive) Process(vectorSet []byte, m Transactable) (interface{}, error) {
	var parsed kdfTestVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	var respGroups []kdfTestGroupResponse
	for _, group := range parsed.Groups {
		groupResp := kdfTestGroupResponse{ID: group.ID}

		if group.OutputBits%8 != 0 {
			return nil, fmt.Errorf("%d bit key in test group %d: fractional bytes not supported", group.OutputBits, group.ID)
		}

		if group.KDFMode != "counter" {
			// feedback mode would need the IV to be handled.
			// double-pipeline mode is not useful.
			return nil, fmt.Errorf("KDF mode %q not supported", group.KDFMode)
		}

		switch group.CounterLocation {
		case "after fixed data", "before fixed data":
			break
		default:
			return nil, fmt.Errorf("Label location %q not supported", group.CounterLocation)
		}

		counterBits := uint32le(group.CounterBits)
		outputBytes := uint32le(group.OutputBits / 8)

		for _, test := range group.Tests {
			testResp := kdfTestResponse{ID: test.ID}

			var key []byte
			if test.Deferred {
				if len(test.Key) != 0 {
					return nil, fmt.Errorf("key provided in deferred test case %d/%d", group.ID, test.ID)
				}
			} else {
				var err error
				if key, err = hex.DecodeString(test.Key); err != nil {
					return nil, fmt.Errorf("failed to decode Key in test case %d/%d: %v", group.ID, test.ID, err)
				}
			}

			// Make the call to the crypto module.
			resp, err := m.Transact("KDF-counter", 3, outputBytes, []byte(group.MACMode), []byte(group.CounterLocation), key, counterBits)
			if err != nil {
				return nil, fmt.Errorf("wrapper KDF operation failed: %s", err)
			}

			// Parse results.
			testResp.ID = test.ID
			if test.Deferred {
				testResp.KeyIn = hex.EncodeToString(resp[0])
			}
			testResp.FixedData = hex.EncodeToString(resp[1])
			testResp.KeyOut = hex.EncodeToString(resp[2])

			if !test.Deferred && !bytes.Equal(resp[0], key) {
				return nil, fmt.Errorf("wrapper returned a different key for non-deferred KDF operation")
			}

			groupResp.Tests = append(groupResp.Tests, testResp)
		}
		respGroups = append(respGroups, groupResp)
	}

	return respGroups, nil
}
