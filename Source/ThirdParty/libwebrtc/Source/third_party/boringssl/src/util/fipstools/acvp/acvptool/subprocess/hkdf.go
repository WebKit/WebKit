// Copyright (c) 2021, Google Inc.
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

// The following structures reflect the JSON of ACVP KAS KDF tests. See
// https://pages.nist.gov/ACVP/draft-hammett-acvp-kas-kdf-hkdf.html

type hkdfTestVectorSet struct {
	Groups []hkdfTestGroup `json:"testGroups"`
}

type hkdfTestGroup struct {
	ID     uint64            `json:"tgId"`
	Type   string            `json:"testType"` // AFT or VAL
	Config hkdfConfiguration `json:"kdfConfiguration"`
	Tests  []hkdfTest        `json:"tests"`
}

type hkdfTest struct {
	ID          uint64         `json:"tcId"`
	Params      hkdfParameters `json:"kdfParameter"`
	PartyU      hkdfPartyInfo  `json:"fixedInfoPartyU"`
	PartyV      hkdfPartyInfo  `json:"fixedInfoPartyV"`
	ExpectedHex string         `json:"dkm"`
}

type hkdfConfiguration struct {
	Type               string `json:"kdfType"`
	OutputBits         uint32 `json:"l"`
	HashName           string `json:"hmacAlg"`
	FixedInfoPattern   string `json:"fixedInfoPattern"`
	FixedInputEncoding string `json:"fixedInfoEncoding"`
}

func (c *hkdfConfiguration) extract() (outBytes uint32, hashName string, err error) {
	if c.Type != "hkdf" ||
		c.FixedInfoPattern != "uPartyInfo||vPartyInfo" ||
		c.FixedInputEncoding != "concatenation" ||
		c.OutputBits%8 != 0 {
		return 0, "", fmt.Errorf("KDA not configured for HKDF: %#v", c)
	}

	return c.OutputBits / 8, c.HashName, nil
}

type hkdfParameters struct {
	SaltHex string `json:"salt"`
	KeyHex  string `json:"z"`
}

func (p *hkdfParameters) extract() (key, salt []byte, err error) {
	salt, err = hex.DecodeString(p.SaltHex)
	if err != nil {
		return nil, nil, err
	}

	key, err = hex.DecodeString(p.KeyHex)
	if err != nil {
		return nil, nil, err
	}

	return key, salt, nil
}

type hkdfPartyInfo struct {
	IDHex    string `json:"partyId"`
	ExtraHex string `json:"ephemeralData"`
}

func (p *hkdfPartyInfo) data() ([]byte, error) {
	ret, err := hex.DecodeString(p.IDHex)
	if err != nil {
		return nil, err
	}

	if len(p.ExtraHex) > 0 {
		extra, err := hex.DecodeString(p.ExtraHex)
		if err != nil {
			return nil, err
		}
		ret = append(ret, extra...)
	}

	return ret, nil
}

type hkdfTestGroupResponse struct {
	ID    uint64             `json:"tgId"`
	Tests []hkdfTestResponse `json:"tests"`
}

type hkdfTestResponse struct {
	ID     uint64 `json:"tcId"`
	KeyOut string `json:"dkm,omitempty"`
	Passed *bool  `json:"testPassed,omitempty"`
}

type hkdf struct{}

func (k *hkdf) Process(vectorSet []byte, m Transactable) (any, error) {
	var parsed hkdfTestVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	var respGroups []hkdfTestGroupResponse
	for _, group := range parsed.Groups {
		group := group
		groupResp := hkdfTestGroupResponse{ID: group.ID}

		var isValidationTest bool
		switch group.Type {
		case "VAL":
			isValidationTest = true
		case "AFT":
			isValidationTest = false
		default:
			return nil, fmt.Errorf("unknown test type %q", group.Type)
		}

		outBytes, hashName, err := group.Config.extract()
		if err != nil {
			return nil, err
		}

		for _, test := range group.Tests {
			test := test
			testResp := hkdfTestResponse{ID: test.ID}

			key, salt, err := test.Params.extract()
			if err != nil {
				return nil, err
			}
			uData, err := test.PartyU.data()
			if err != nil {
				return nil, err
			}
			vData, err := test.PartyV.data()
			if err != nil {
				return nil, err
			}

			var expected []byte
			if isValidationTest {
				expected, err = hex.DecodeString(test.ExpectedHex)
				if err != nil {
					return nil, err
				}
			}

			info := make([]byte, 0, len(uData)+len(vData))
			info = append(info, uData...)
			info = append(info, vData...)

			m.TransactAsync("HKDF/"+hashName, 1, [][]byte{key, salt, info, uint32le(outBytes)}, func(result [][]byte) error {
				if len(result[0]) != int(outBytes) {
					return fmt.Errorf("HKDF operation resulted in %d bytes but wanted %d", len(result[0]), outBytes)
				}
				if isValidationTest {
					passed := bytes.Equal(expected, result[0])
					testResp.Passed = &passed
				} else {
					testResp.KeyOut = hex.EncodeToString(result[0])
				}

				groupResp.Tests = append(groupResp.Tests, testResp)
				return nil
			})
		}

		m.Barrier(func() {
			respGroups = append(respGroups, groupResp)
		})
	}

	if err := m.Flush(); err != nil {
		return nil, err
	}

	return respGroups, nil
}
