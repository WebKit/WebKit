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
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
)

// The following structures reflect the JSON of ACVP DRBG tests. See
// https://pages.nist.gov/ACVP/draft-vassilev-acvp-drbg.html#name-test-vectors

type drbgTestVectorSet struct {
	Groups []drbgTestGroup `json:"testGroups"`
}

type drbgTestGroup struct {
	ID                    uint64 `json:"tgId"`
	Mode                  string `json:"mode"`
	UseDerivationFunction bool   `json:"derFunc,omitempty"`
	PredictionResistance  bool   `json:"predResistance"`
	Reseed                bool   `json:"reSeed"`
	EntropyBits           uint64 `json:"entropyInputLen"`
	NonceBits             uint64 `json:"nonceLen"`
	PersonalizationBits   uint64 `json:"persoStringLen"`
	AdditionalDataBits    uint64 `json:"additionalInputLen"`
	RetBits               uint64 `json:"returnedBitsLen"`
	Tests                 []struct {
		ID                 uint64 `json:"tcId"`
		EntropyHex         string `json:"entropyInput"`
		NonceHex           string `json:"nonce"`
		PersonalizationHex string `json:"persoString"`
		Other              []struct {
			AdditionalDataHex string `json:"additionalInput"`
			EntropyHex        string `json:"entropyInput"`
			Use               string `json:"intendedUse"`
		} `json:"otherInput"`
	} `json:"tests"`
}

type drbgTestGroupResponse struct {
	ID    uint64             `json:"tgId"`
	Tests []drbgTestResponse `json:"tests"`
}

type drbgTestResponse struct {
	ID     uint64 `json:"tcId"`
	OutHex string `json:"returnedBits,omitempty"`
}

// drbg implements an ACVP algorithm by making requests to the
// subprocess to generate random bits with the given entropy and other paramaters.
type drbg struct {
	// algo is the ACVP name for this algorithm and also the command name
	// given to the subprocess to generate random bytes.
	algo  string
	modes map[string]bool // the supported underlying primitives for the DRBG
}

func (d *drbg) Process(vectorSet []byte, m Transactable) (interface{}, error) {
	var parsed drbgTestVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	var ret []drbgTestGroupResponse
	// See
	// https://pages.nist.gov/ACVP/draft-vassilev-acvp-drbg.html#name-test-vectors
	// for details about the tests.
	for _, group := range parsed.Groups {
		response := drbgTestGroupResponse{
			ID: group.ID,
		}

		if _, ok := d.modes[group.Mode]; !ok {
			return nil, fmt.Errorf("test group %d specifies mode %q, which is not supported for the %s algorithm", group.ID, group.Mode, d.algo)
		}

		if group.PredictionResistance {
			return nil, fmt.Errorf("Test group %d specifies prediction-resistance mode, which is not supported", group.ID)
		}

		if group.Reseed {
			return nil, fmt.Errorf("Test group %d requests re-seeding, which is not supported", group.ID)
		}

		if group.RetBits%8 != 0 {
			return nil, fmt.Errorf("Test group %d requests %d-bit outputs, but fractional-bytes are not supported", group.ID, group.RetBits)
		}

		for _, test := range group.Tests {
			ent, err := extractField(test.EntropyHex, group.EntropyBits)
			if err != nil {
				return nil, fmt.Errorf("failed to extract entropy hex from test case %d/%d: %s", group.ID, test.ID, err)
			}

			nonce, err := extractField(test.NonceHex, group.NonceBits)
			if err != nil {
				return nil, fmt.Errorf("failed to extract nonce hex from test case %d/%d: %s", group.ID, test.ID, err)
			}

			perso, err := extractField(test.PersonalizationHex, group.PersonalizationBits)
			if err != nil {
				return nil, fmt.Errorf("failed to extract personalization hex from test case %d/%d: %s", group.ID, test.ID, err)
			}

			const numAdditionalInputs = 2
			if len(test.Other) != numAdditionalInputs {
				return nil, fmt.Errorf("test case %d/%d provides %d additional inputs, but subprocess only expects %d", group.ID, test.ID, len(test.Other), numAdditionalInputs)
			}

			var additionalInputs [numAdditionalInputs][]byte
			for i, other := range test.Other {
				if other.Use != "generate" {
					return nil, fmt.Errorf("other %d from test case %d/%d has use %q, but expected 'generate'", i, group.ID, test.ID, other.Use)
				}
				additionalInputs[i], err = extractField(other.AdditionalDataHex, group.AdditionalDataBits)
				if err != nil {
					return nil, fmt.Errorf("failed to extract additional input %d from test case %d/%d: %s", i, group.ID, test.ID, err)
				}
			}

			outLen := group.RetBits / 8
			var outLenBytes [4]byte
			binary.LittleEndian.PutUint32(outLenBytes[:], uint32(outLen))
			result, err := m.Transact(d.algo+"/"+group.Mode, 1, outLenBytes[:], ent, perso, additionalInputs[0], additionalInputs[1], nonce)
			if err != nil {
				return nil, fmt.Errorf("DRBG operation failed: %s", err)
			}

			if l := uint64(len(result[0])); l != outLen {
				return nil, fmt.Errorf("wrong length DRBG result: %d bytes but wanted %d", l, outLen)
			}

			// https://pages.nist.gov/ACVP/draft-vassilev-acvp-drbg.html#name-responses
			response.Tests = append(response.Tests, drbgTestResponse{
				ID:     test.ID,
				OutHex: hex.EncodeToString(result[0]),
			})
		}

		ret = append(ret, response)
	}

	return ret, nil
}

// validate the length and hex of a JSON field in test vectors
func extractField(fieldHex string, bits uint64) ([]byte, error) {
	if uint64(len(fieldHex))*4 != bits {
		return nil, fmt.Errorf("expected %d bits but have %d-byte hex string", bits, len(fieldHex))
	}
	return hex.DecodeString(fieldHex)
}
