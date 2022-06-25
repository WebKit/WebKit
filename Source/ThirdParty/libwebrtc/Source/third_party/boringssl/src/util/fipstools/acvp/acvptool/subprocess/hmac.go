// Copyright (c) 2019, Google Inc.
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
	"encoding/hex"
	"encoding/json"
	"fmt"
	"strconv"
)

// The following structures reflect the JSON of ACVP HMAC tests. See
// https://pages.nist.gov/ACVP/draft-fussell-acvp-mac.html#name-test-vectors

type hmacTestVectorSet struct {
	Groups []hmacTestGroup `json:"testGroups"`
}

type hmacTestGroup struct {
	ID      uint64 `json:"tgId"`
	Type    string `json:"testType"`
	MsgBits int    `json:"msgLen"`
	KeyBits int    `json:"keyLen"` // maximum possible value is 524288
	MACBits int    `json:"macLen"` // maximum possible value is 512
	Tests   []struct {
		ID     uint64 `json:"tcId"`
		KeyHex string `json:"key"`
		MsgHex string `json:"msg"`
	} `json:"tests"`
}

type hmacTestGroupResponse struct {
	ID    uint64             `json:"tgId"`
	Tests []hmacTestResponse `json:"tests"`
}

type hmacTestResponse struct {
	ID     uint64 `json:"tcId"`
	MACHex string `json:"mac,omitempty"`
}

// hmacPrimitive implements an ACVP algorithm by making requests to the
// subprocess to HMAC strings with the given key.
type hmacPrimitive struct {
	// algo is the ACVP name for this algorithm and also the command name
	// given to the subprocess to HMAC with this hash function.
	algo  string
	mdLen int // mdLen is the number of bytes of output that the underlying hash produces.
}

// hmac uses the subprocess to compute HMAC and returns the result.
func (h *hmacPrimitive) hmac(msg []byte, key []byte, outBits int, m Transactable) []byte {
	if outBits%8 != 0 {
		panic("fractional-byte output length requested: " + strconv.Itoa(outBits))
	}
	outBytes := outBits / 8
	result, err := m.Transact(h.algo, 1, msg, key)
	if err != nil {
		panic("HMAC operation failed: " + err.Error())
	}
	if l := len(result[0]); l < outBytes {
		panic(fmt.Sprintf("HMAC result too short: %d bytes but wanted %d", l, outBytes))
	}
	return result[0][:outBytes]
}

func (h *hmacPrimitive) Process(vectorSet []byte, m Transactable) (interface{}, error) {
	var parsed hmacTestVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	var ret []hmacTestGroupResponse
	// See
	// https://pages.nist.gov/ACVP/draft-fussell-acvp-mac.html#name-test-vectors
	// for details about the tests.
	for _, group := range parsed.Groups {
		response := hmacTestGroupResponse{
			ID: group.ID,
		}
		if group.MACBits > h.mdLen*8 {
			return nil, fmt.Errorf("test group %d specifies MAC length should be %d, but maximum possible length is %d", group.ID, group.MACBits, h.mdLen*8)
		}

		for _, test := range group.Tests {
			if len(test.MsgHex)*4 != group.MsgBits {
				return nil, fmt.Errorf("test case %d/%d contains hex message of length %d but specifies a bit length of %d", group.ID, test.ID, len(test.MsgHex), group.MsgBits)
			}
			msg, err := hex.DecodeString(test.MsgHex)
			if err != nil {
				return nil, fmt.Errorf("failed to decode hex in test case %d/%d: %s", group.ID, test.ID, err)
			}

			if len(test.KeyHex)*4 != group.KeyBits {
				return nil, fmt.Errorf("test case %d/%d contains hex key of length %d but specifies a bit length of %d", group.ID, test.ID, len(test.KeyHex), group.KeyBits)
			}
			key, err := hex.DecodeString(test.KeyHex)
			if err != nil {
				return nil, fmt.Errorf("failed to decode key in test case %d/%d: %s", group.ID, test.ID, err)
			}

			// https://pages.nist.gov/ACVP/draft-fussell-acvp-mac.html#name-test-vectors
			response.Tests = append(response.Tests, hmacTestResponse{
				ID:     test.ID,
				MACHex: hex.EncodeToString(h.hmac(msg, key, group.MACBits, m)),
			})
		}

		ret = append(ret, response)
	}

	return ret, nil
}
