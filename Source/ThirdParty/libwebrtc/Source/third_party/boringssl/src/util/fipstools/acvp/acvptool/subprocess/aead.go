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
	"encoding/hex"
	"encoding/json"
	"fmt"
)

// aead implements an ACVP algorithm by making requests to the subprocess
// to encrypt and decrypt with an AEAD.
type aead struct {
	algo                    string
	tagMergedWithCiphertext bool
}

type aeadVectorSet struct {
	Groups []aeadTestGroup `json:"testGroups"`
}

type aeadTestGroup struct {
	ID        uint64 `json:"tgId"`
	Type      string `json:"testType"`
	Direction string `json:"direction"`
	KeyBits   int    `json:"keyLen"`
	TagBits   int    `json:"tagLen"`
	Tests     []struct {
		ID            uint64 `json:"tcId"`
		PlaintextHex  string `json:"pt"`
		CiphertextHex string `json:"ct"`
		IVHex         string `json:"iv"`
		KeyHex        string `json:"key"`
		AADHex        string `json:"aad"`
		TagHex        string `json:"tag"`
	} `json:"tests"`
}

type aeadTestGroupResponse struct {
	ID    uint64             `json:"tgId"`
	Tests []aeadTestResponse `json:"tests"`
}

type aeadTestResponse struct {
	ID            uint64  `json:"tcId"`
	CiphertextHex *string `json:"ct,omitempty"`
	TagHex        string  `json:"tag,omitempty"`
	PlaintextHex  *string `json:"pt,omitempty"`
	Passed        *bool   `json:"testPassed,omitempty"`
}

func (a *aead) Process(vectorSet []byte, m Transactable) (interface{}, error) {
	var parsed aeadVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	var ret []aeadTestGroupResponse
	// See draft-celi-acvp-symmetric.html#table-6. (NIST no longer publish HTML
	// versions of the ACVP documents. You can find fragments in
	// https://github.com/usnistgov/ACVP.)
	for _, group := range parsed.Groups {
		response := aeadTestGroupResponse{
			ID: group.ID,
		}

		var encrypt bool
		switch group.Direction {
		case "encrypt":
			encrypt = true
		case "decrypt":
			encrypt = false
		default:
			return nil, fmt.Errorf("test group %d has unknown direction %q", group.ID, group.Direction)
		}

		op := a.algo + "/seal"
		if !encrypt {
			op = a.algo + "/open"
		}

		if group.KeyBits%8 != 0 || group.KeyBits < 0 {
			return nil, fmt.Errorf("test group %d contains non-byte-multiple key length %d", group.ID, group.KeyBits)
		}
		keyBytes := group.KeyBits / 8

		if group.TagBits%8 != 0 || group.TagBits < 0 {
			return nil, fmt.Errorf("test group %d contains non-byte-multiple tag length %d", group.ID, group.TagBits)
		}
		tagBytes := group.TagBits / 8

		for _, test := range group.Tests {
			if len(test.KeyHex) != keyBytes*2 {
				return nil, fmt.Errorf("test case %d/%d contains key %q of length %d, but expected %d-bit key", group.ID, test.ID, test.KeyHex, len(test.KeyHex), group.KeyBits)
			}

			key, err := hex.DecodeString(test.KeyHex)
			if err != nil {
				return nil, fmt.Errorf("failed to decode key in test case %d/%d: %s", group.ID, test.ID, err)
			}

			nonce, err := hex.DecodeString(test.IVHex)
			if err != nil {
				return nil, fmt.Errorf("failed to decode nonce in test case %d/%d: %s", group.ID, test.ID, err)
			}

			aad, err := hex.DecodeString(test.AADHex)
			if err != nil {
				return nil, fmt.Errorf("failed to decode aad in test case %d/%d: %s", group.ID, test.ID, err)
			}

			var inputHex, otherHex string
			if encrypt {
				inputHex, otherHex = test.PlaintextHex, test.CiphertextHex
			} else {
				inputHex, otherHex = test.CiphertextHex, test.PlaintextHex
			}

			if len(otherHex) != 0 {
				return nil, fmt.Errorf("test case %d/%d has unexpected plain/ciphertext input", group.ID, test.ID)
			}

			input, err := hex.DecodeString(inputHex)
			if err != nil {
				return nil, fmt.Errorf("failed to decode hex in test case %d/%d: %s", group.ID, test.ID, err)
			}

			var tag []byte
			if a.tagMergedWithCiphertext {
				if len(test.TagHex) != 0 {
					return nil, fmt.Errorf("test case %d/%d has unexpected tag input (should be merged into ciphertext)", group.ID, test.ID)
				}
				if !encrypt && len(input) < tagBytes {
					return nil, fmt.Errorf("test case %d/%d has ciphertext shorter than the tag, but the tag should be included in it", group.ID, test.ID)
				}
			} else {
				if !encrypt {
					if tag, err = hex.DecodeString(test.TagHex); err != nil {
						return nil, fmt.Errorf("failed to decode tag in test case %d/%d: %s", group.ID, test.ID, err)
					}
					if len(tag) != tagBytes {
						return nil, fmt.Errorf("tag in test case %d/%d is %d bytes long, but should be %d", group.ID, test.ID, len(tag), tagBytes)
					}
				} else if len(test.TagHex) != 0 {
					return nil, fmt.Errorf("test case %d/%d has unexpected tag input", group.ID, test.ID)
				}
			}

			testResp := aeadTestResponse{ID: test.ID}

			if encrypt {
				result, err := m.Transact(op, 1, uint32le(uint32(tagBytes)), key, input, nonce, aad)
				if err != nil {
					return nil, err
				}

				if len(result[0]) < tagBytes {
					return nil, fmt.Errorf("ciphertext from subprocess for test case %d/%d is shorter than the tag (%d vs %d)", group.ID, test.ID, len(result[0]), tagBytes)
				}

				if a.tagMergedWithCiphertext {
					ciphertextHex := hex.EncodeToString(result[0])
					testResp.CiphertextHex = &ciphertextHex
				} else {
					ciphertext := result[0][:len(result[0])-tagBytes]
					ciphertextHex := hex.EncodeToString(ciphertext)
					testResp.CiphertextHex = &ciphertextHex
					tag := result[0][len(result[0])-tagBytes:]
					testResp.TagHex = hex.EncodeToString(tag)
				}
			} else {
				result, err := m.Transact(op, 2, uint32le(uint32(tagBytes)), key, append(input, tag...), nonce, aad)
				if err != nil {
					return nil, err
				}

				if len(result[0]) != 1 || (result[0][0]&0xfe) != 0 {
					return nil, fmt.Errorf("invalid AEAD status result from subprocess")
				}
				passed := result[0][0] == 1
				testResp.Passed = &passed
				if passed {
					plaintextHex := hex.EncodeToString(result[1])
					testResp.PlaintextHex = &plaintextHex
				}
			}

			response.Tests = append(response.Tests, testResp)
		}

		ret = append(ret, response)
	}

	return ret, nil
}
