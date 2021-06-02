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
	"bytes"
	"encoding/hex"
	"encoding/json"
	"fmt"
)

// The following structures reflect the JSON of ACVP hash tests. See
// https://pages.nist.gov/ACVP/draft-fussell-acvp-ecdsa.html#name-test-vectors

type ecdsaTestVectorSet struct {
	Groups []ecdsaTestGroup `json:"testGroups"`
	Mode   string           `json:"mode"`
}

type ecdsaTestGroup struct {
	ID                   uint64 `json:"tgId"`
	Curve                string `json:"curve"`
	SecretGenerationMode string `json:"secretGenerationMode,omitempty"`
	HashAlgo             string `json:"hashAlg,omitEmpty"`
	ComponentTest        bool   `json:"componentTest"`
	Tests                []struct {
		ID     uint64 `json:"tcId"`
		QxHex  string `json:"qx,omitempty"`
		QyHex  string `json:"qy,omitempty"`
		RHex   string `json:"r,omitempty"`
		SHex   string `json:"s,omitempty"`
		MsgHex string `json:"message,omitempty"`
	} `json:"tests"`
}

type ecdsaTestGroupResponse struct {
	ID    uint64              `json:"tgId"`
	Tests []ecdsaTestResponse `json:"tests"`
	QxHex string              `json:"qx,omitempty"`
	QyHex string              `json:"qy,omitempty"`
}

type ecdsaTestResponse struct {
	ID     uint64 `json:"tcId"`
	DHex   string `json:"d,omitempty"`
	QxHex  string `json:"qx,omitempty"`
	QyHex  string `json:"qy,omitempty"`
	RHex   string `json:"r,omitempty"`
	SHex   string `json:"s,omitempty"`
	Passed *bool  `json:"testPassed,omitempty"` // using pointer so value is not omitted when it is false
}

// ecdsa implements an ACVP algorithm by making requests to the
// subprocess to generate and verify ECDSA keys and signatures.
type ecdsa struct {
	// algo is the ACVP name for this algorithm and also the command name
	// given to the subprocess to hash with this hash function.
	algo       string
	curves     map[string]bool // supported curve names
	primitives map[string]primitive
}

func (e *ecdsa) Process(vectorSet []byte, m Transactable) (interface{}, error) {
	var parsed ecdsaTestVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	var ret []ecdsaTestGroupResponse
	// See
	// https://pages.nist.gov/ACVP/draft-fussell-acvp-ecdsa.html#name-test-vectors
	// for details about the tests.
	for _, group := range parsed.Groups {
		if _, ok := e.curves[group.Curve]; !ok {
			return nil, fmt.Errorf("curve %q in test group %d not supported", group.Curve, group.ID)
		}

		response := ecdsaTestGroupResponse{
			ID: group.ID,
		}
		var sigGenPrivateKey []byte

		for _, test := range group.Tests {
			var testResp ecdsaTestResponse

			switch parsed.Mode {
			case "keyGen":
				if group.SecretGenerationMode != "testing candidates" {
					return nil, fmt.Errorf("invalid secret generation mode in test group %d: %q", group.ID, group.SecretGenerationMode)
				}
				result, err := m.Transact(e.algo+"/"+"keyGen", 3, []byte(group.Curve))
				if err != nil {
					return nil, fmt.Errorf("key generation failed for test case %d/%d: %s", group.ID, test.ID, err)
				}
				testResp.DHex = hex.EncodeToString(result[0])
				testResp.QxHex = hex.EncodeToString(result[1])
				testResp.QyHex = hex.EncodeToString(result[2])

			case "keyVer":
				qx, err := hex.DecodeString(test.QxHex)
				if err != nil {
					return nil, fmt.Errorf("failed to decode qx in test case %d/%d: %s", group.ID, test.ID, err)
				}
				qy, err := hex.DecodeString(test.QyHex)
				if err != nil {
					return nil, fmt.Errorf("failed to decode qy in test case %d/%d: %s", group.ID, test.ID, err)
				}
				result, err := m.Transact(e.algo+"/"+"keyVer", 1, []byte(group.Curve), qx, qy)
				if err != nil {
					return nil, fmt.Errorf("key verification failed for test case %d/%d: %s", group.ID, test.ID, err)
				}
				// result[0] should be a single byte: zero if false, one if true
				switch {
				case bytes.Equal(result[0], []byte{00}):
					f := false
					testResp.Passed = &f
				case bytes.Equal(result[0], []byte{01}):
					t := true
					testResp.Passed = &t
				default:
					return nil, fmt.Errorf("key verification returned unexpected result: %q", result[0])
				}

			case "sigGen":
				p := e.primitives[group.HashAlgo]
				h, ok := p.(*hashPrimitive)
				if !ok {
					return nil, fmt.Errorf("unsupported hash algorithm %q in test group %d", group.HashAlgo, group.ID)
				}

				if len(sigGenPrivateKey) == 0 {
					// Ask the subprocess to generate a key for this test group.
					result, err := m.Transact(e.algo+"/"+"keyGen", 3, []byte(group.Curve))
					if err != nil {
						return nil, fmt.Errorf("key generation failed for test case %d/%d: %s", group.ID, test.ID, err)
					}

					sigGenPrivateKey = result[0]
					response.QxHex = hex.EncodeToString(result[1])
					response.QyHex = hex.EncodeToString(result[2])
				}

				msg, err := hex.DecodeString(test.MsgHex)
				if err != nil {
					return nil, fmt.Errorf("failed to decode message hex in test case %d/%d: %s", group.ID, test.ID, err)
				}
				op := e.algo + "/" + "sigGen"
				if group.ComponentTest {
					if len(msg) != h.size {
						return nil, fmt.Errorf("test case %d/%d contains message %q of length %d, but expected length %d", group.ID, test.ID, test.MsgHex, len(msg), h.size)
					}
					op += "/componentTest"
				}
				result, err := m.Transact(op, 2, []byte(group.Curve), sigGenPrivateKey, []byte(group.HashAlgo), msg)
				if err != nil {
					return nil, fmt.Errorf("signature generation failed for test case %d/%d: %s", group.ID, test.ID, err)
				}
				testResp.RHex = hex.EncodeToString(result[0])
				testResp.SHex = hex.EncodeToString(result[1])

			case "sigVer":
				p := e.primitives[group.HashAlgo]
				_, ok := p.(*hashPrimitive)
				if !ok {
					return nil, fmt.Errorf("unsupported hash algorithm %q in test group %d", group.HashAlgo, group.ID)
				}

				msg, err := hex.DecodeString(test.MsgHex)
				if err != nil {
					return nil, fmt.Errorf("failed to decode message hex in test case %d/%d: %s", group.ID, test.ID, err)
				}
				qx, err := hex.DecodeString(test.QxHex)
				if err != nil {
					return nil, fmt.Errorf("failed to decode qx in test case %d/%d: %s", group.ID, test.ID, err)
				}
				qy, err := hex.DecodeString(test.QyHex)
				if err != nil {
					return nil, fmt.Errorf("failed to decode qy in test case %d/%d: %s", group.ID, test.ID, err)
				}
				r, err := hex.DecodeString(test.RHex)
				if err != nil {
					return nil, fmt.Errorf("failed to decode R in test case %d/%d: %s", group.ID, test.ID, err)
				}
				s, err := hex.DecodeString(test.SHex)
				if err != nil {
					return nil, fmt.Errorf("failed to decode S in test case %d/%d: %s", group.ID, test.ID, err)
				}
				result, err := m.Transact(e.algo+"/"+"sigVer", 1, []byte(group.Curve), []byte(group.HashAlgo), msg, qx, qy, r, s)
				if err != nil {
					return nil, fmt.Errorf("signature verification failed for test case %d/%d: %s", group.ID, test.ID, err)
				}
				// result[0] should be a single byte: zero if false, one if true
				switch {
				case bytes.Equal(result[0], []byte{00}):
					f := false
					testResp.Passed = &f
				case bytes.Equal(result[0], []byte{01}):
					t := true
					testResp.Passed = &t
				default:
					return nil, fmt.Errorf("signature verification returned unexpected result: %q", result[0])
				}

			default:
				return nil, fmt.Errorf("invalid mode %q in ECDSA vector set", parsed.Mode)
			}

			testResp.ID = test.ID
			response.Tests = append(response.Tests, testResp)
		}

		ret = append(ret, response)
	}

	return ret, nil
}
