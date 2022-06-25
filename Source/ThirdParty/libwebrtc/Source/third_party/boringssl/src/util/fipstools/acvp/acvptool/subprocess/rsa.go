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

// See https://pages.nist.gov/ACVP/draft-celi-acvp-rsa.html#name-test-vectors
// although, at the time of writing, that spec doesn't match what the NIST demo
// server actually produces. This code matches the server.

type rsaTestVectorSet struct {
	Mode string `json:"mode"`
}

type rsaKeyGenTestVectorSet struct {
	Groups []rsaKeyGenGroup `json:"testGroups"`
}

type rsaKeyGenGroup struct {
	ID          uint64          `json:"tgId"`
	Type        string          `json:"testType"`
	ModulusBits uint32          `json:"modulo"`
	Tests       []rsaKeyGenTest `json:"tests"`
}

type rsaKeyGenTest struct {
	ID uint64 `json:"tcId"`
}

type rsaKeyGenTestGroupResponse struct {
	ID    uint64                  `json:"tgId"`
	Tests []rsaKeyGenTestResponse `json:"tests"`
}

type rsaKeyGenTestResponse struct {
	ID uint64 `json:"tcId"`
	E  string `json:"e"`
	P  string `json:"p"`
	Q  string `json:"q"`
	N  string `json:"n"`
	D  string `json:"d"`
}

type rsaSigGenTestVectorSet struct {
	Groups []rsaSigGenGroup `json:"testGroups"`
}

type rsaSigGenGroup struct {
	ID          uint64          `json:"tgId"`
	Type        string          `json:"testType"`
	SigType     string          `json:"sigType"`
	ModulusBits uint32          `json:"modulo"`
	Hash        string          `json:"hashAlg"`
	Tests       []rsaSigGenTest `json:"tests"`
}

type rsaSigGenTest struct {
	ID         uint64 `json:"tcId"`
	MessageHex string `json:"message"`
}

type rsaSigGenTestGroupResponse struct {
	ID    uint64                  `json:"tgId"`
	N     string                  `json:"n"`
	E     string                  `json:"e"`
	Tests []rsaSigGenTestResponse `json:"tests"`
}

type rsaSigGenTestResponse struct {
	ID  uint64 `json:"tcId"`
	Sig string `json:"signature"`
}

type rsaSigVerTestVectorSet struct {
	Groups []rsaSigVerGroup `json:"testGroups"`
}

type rsaSigVerGroup struct {
	ID      uint64          `json:"tgId"`
	Type    string          `json:"testType"`
	SigType string          `json:"sigType"`
	Hash    string          `json:"hashAlg"`
	N       string          `json:"n"`
	E       string          `json:"e"`
	Tests   []rsaSigVerTest `json:"tests"`
}

type rsaSigVerTest struct {
	ID           uint64 `json:"tcId"`
	MessageHex   string `json:"message"`
	SignatureHex string `json:"signature"`
}

type rsaSigVerTestGroupResponse struct {
	ID    uint64                  `json:"tgId"`
	Tests []rsaSigVerTestResponse `json:"tests"`
}

type rsaSigVerTestResponse struct {
	ID     uint64 `json:"tcId"`
	Passed bool   `json:"testPassed"`
}

func processKeyGen(vectorSet []byte, m Transactable) (interface{}, error) {
	var parsed rsaKeyGenTestVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	var ret []rsaKeyGenTestGroupResponse

	for _, group := range parsed.Groups {
		// GDT means "Generated data test", i.e. "please generate an RSA key".
		const expectedType = "GDT"
		if group.Type != expectedType {
			return nil, fmt.Errorf("RSA KeyGen test group has type %q, but only generation tests (%q) are supported", group.Type, expectedType)
		}

		response := rsaKeyGenTestGroupResponse{
			ID: group.ID,
		}

		for _, test := range group.Tests {
			results, err := m.Transact("RSA/keyGen", 5, uint32le(group.ModulusBits))
			if err != nil {
				return nil, err
			}

			response.Tests = append(response.Tests, rsaKeyGenTestResponse{
				ID: test.ID,
				E:  hex.EncodeToString(results[0]),
				P:  hex.EncodeToString(results[1]),
				Q:  hex.EncodeToString(results[2]),
				N:  hex.EncodeToString(results[3]),
				D:  hex.EncodeToString(results[4]),
			})
		}

		ret = append(ret, response)
	}

	return ret, nil
}

func processSigGen(vectorSet []byte, m Transactable) (interface{}, error) {
	var parsed rsaSigGenTestVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	var ret []rsaSigGenTestGroupResponse

	for _, group := range parsed.Groups {
		// GDT means "Generated data test", i.e. "please generate an RSA signature".
		const expectedType = "GDT"
		if group.Type != expectedType {
			return nil, fmt.Errorf("RSA SigGen test group has type %q, but only generation tests (%q) are supported", group.Type, expectedType)
		}

		response := rsaSigGenTestGroupResponse{
			ID: group.ID,
		}

		operation := "RSA/sigGen/" + group.Hash + "/" + group.SigType

		for _, test := range group.Tests {
			msg, err := hex.DecodeString(test.MessageHex)
			if err != nil {
				return nil, fmt.Errorf("test case %d/%d contains invalid hex: %s", group.ID, test.ID, err)
			}

			results, err := m.Transact(operation, 3, uint32le(group.ModulusBits), msg)
			if err != nil {
				return nil, err
			}

			if len(response.N) == 0 {
				response.N = hex.EncodeToString(results[0])
				response.E = hex.EncodeToString(results[1])
			} else if response.N != hex.EncodeToString(results[0]) {
				return nil, fmt.Errorf("module wrapper returned different RSA keys for the same SigGen configuration")
			}

			response.Tests = append(response.Tests, rsaSigGenTestResponse{
				ID:  test.ID,
				Sig: hex.EncodeToString(results[2]),
			})
		}

		ret = append(ret, response)
	}

	return ret, nil
}

func processSigVer(vectorSet []byte, m Transactable) (interface{}, error) {
	var parsed rsaSigVerTestVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	var ret []rsaSigVerTestGroupResponse

	for _, group := range parsed.Groups {
		// GDT means "Generated data test", which makes no sense in this context.
		const expectedType = "GDT"
		if group.Type != expectedType {
			return nil, fmt.Errorf("RSA SigVer test group has type %q, but only 'generation' tests (%q) are supported", group.Type, expectedType)
		}

		n, err := hex.DecodeString(group.N)
		if err != nil {
			return nil, fmt.Errorf("test group %d contains invalid hex: %s", group.ID, err)
		}
		e, err := hex.DecodeString(group.E)
		if err != nil {
			return nil, fmt.Errorf("test group %d contains invalid hex: %s", group.ID, err)
		}

		response := rsaSigVerTestGroupResponse{
			ID: group.ID,
		}

		operation := "RSA/sigVer/" + group.Hash + "/" + group.SigType

		for _, test := range group.Tests {
			msg, err := hex.DecodeString(test.MessageHex)
			if err != nil {
				return nil, fmt.Errorf("test case %d/%d contains invalid hex: %s", group.ID, test.ID, err)
			}
			sig, err := hex.DecodeString(test.SignatureHex)
			if err != nil {
				return nil, fmt.Errorf("test case %d/%d contains invalid hex: %s", group.ID, test.ID, err)
			}

			results, err := m.Transact(operation, 1, n, e, msg, sig)
			if err != nil {
				return nil, err
			}

			response.Tests = append(response.Tests, rsaSigVerTestResponse{
				ID:     test.ID,
				Passed: len(results[0]) == 1 && results[0][0] == 1,
			})
		}

		ret = append(ret, response)
	}

	return ret, nil
}

type rsa struct{}

func (*rsa) Process(vectorSet []byte, m Transactable) (interface{}, error) {
	var parsed rsaTestVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	switch parsed.Mode {
	case "keyGen":
		return processKeyGen(vectorSet, m)
	case "sigGen":
		return processSigGen(vectorSet, m)
	case "sigVer":
		return processSigVer(vectorSet, m)
	default:
		return nil, fmt.Errorf("Unknown RSA mode %q", parsed.Mode)
	}
}
