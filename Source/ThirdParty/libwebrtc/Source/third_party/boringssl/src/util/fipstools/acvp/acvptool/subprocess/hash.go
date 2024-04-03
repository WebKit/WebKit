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
)

// The following structures reflect the JSON of ACVP hash tests. See
// https://pages.nist.gov/ACVP/draft-celi-acvp-sha.html#name-test-vectors

type hashTestVectorSet struct {
	Groups []hashTestGroup `json:"testGroups"`
}

type hashTestGroup struct {
	ID    uint64 `json:"tgId"`
	Type  string `json:"testType"`
	Tests []struct {
		ID        uint64 `json:"tcId"`
		BitLength uint64 `json:"len"`
		MsgHex    string `json:"msg"`
	} `json:"tests"`
}

type hashTestGroupResponse struct {
	ID    uint64             `json:"tgId"`
	Tests []hashTestResponse `json:"tests"`
}

type hashTestResponse struct {
	ID         uint64          `json:"tcId"`
	DigestHex  string          `json:"md,omitempty"`
	MCTResults []hashMCTResult `json:"resultsArray,omitempty"`
}

type hashMCTResult struct {
	DigestHex string `json:"md"`
}

// hashPrimitive implements an ACVP algorithm by making requests to the
// subprocess to hash strings.
type hashPrimitive struct {
	// algo is the ACVP name for this algorithm and also the command name
	// given to the subprocess to hash with this hash function.
	algo string
	// size is the number of bytes of digest that the hash produces.
	size int
}

func (h *hashPrimitive) Process(vectorSet []byte, m Transactable) (any, error) {
	var parsed hashTestVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	var ret []hashTestGroupResponse
	// See
	// https://pages.nist.gov/ACVP/draft-celi-acvp-sha.html#name-test-vectors
	// for details about the tests.
	for _, group := range parsed.Groups {
		response := hashTestGroupResponse{
			ID: group.ID,
		}

		for _, test := range group.Tests {
			if uint64(len(test.MsgHex))*4 != test.BitLength {
				return nil, fmt.Errorf("test case %d/%d contains hex message of length %d but specifies a bit length of %d", group.ID, test.ID, len(test.MsgHex), test.BitLength)
			}
			msg, err := hex.DecodeString(test.MsgHex)
			if err != nil {
				return nil, fmt.Errorf("failed to decode hex in test case %d/%d: %s", group.ID, test.ID, err)
			}

			// http://usnistgov.github.io/ACVP/artifacts/draft-celi-acvp-sha-00.html#rfc.section.3
			switch group.Type {
			case "AFT":
				m.TransactAsync(h.algo, 1, [][]byte{msg}, func(result [][]byte) error {
					response.Tests = append(response.Tests, hashTestResponse{
						ID:        test.ID,
						DigestHex: hex.EncodeToString(result[0]),
					})
					return nil
				})

			case "MCT":
				if len(msg) != h.size {
					return nil, fmt.Errorf("MCT test case %d/%d contains message of length %d but the digest length is %d", group.ID, test.ID, len(msg), h.size)
				}

				testResponse := hashTestResponse{ID: test.ID}

				digest := msg
				for i := 0; i < 100; i++ {
					result, err := m.Transact(h.algo+"/MCT", 1, digest)
					if err != nil {
						panic(h.algo + " hash operation failed: " + err.Error())
					}

					digest = result[0]
					testResponse.MCTResults = append(testResponse.MCTResults, hashMCTResult{hex.EncodeToString(digest)})
				}

				response.Tests = append(response.Tests, testResponse)

			default:
				return nil, fmt.Errorf("test group %d has unknown type %q", group.ID, group.Type)
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
