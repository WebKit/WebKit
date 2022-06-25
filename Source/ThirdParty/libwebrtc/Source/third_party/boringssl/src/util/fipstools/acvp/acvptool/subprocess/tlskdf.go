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
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
)

type tlsKDFVectorSet struct {
	Groups []tlsKDFTestGroup `json:"testGroups"`
}

type tlsKDFTestGroup struct {
	ID           uint64       `json:"tgId"`
	Hash         string       `json:"hashAlg"`
	TLSVersion   string       `json:"tlsVersion"`
	KeyBlockBits uint64       `json:"keyBlockLength"`
	PMSLength    uint64       `json:"preMasterSecretLength"`
	Tests        []tlsKDFTest `json:"tests"`
}

type tlsKDFTest struct {
	ID     uint64 `json:"tcId"`
	PMSHex string `json:"preMasterSecret"`
	// ClientHelloRandomHex and ServerHelloRandomHex are used for deriving the
	// master secret. ClientRandomHex and ServerRandomHex are used for deriving the
	// key block. Having different values for these is not possible in a TLS
	// handshake unless you squint at a resumption handshake and somehow rederive
	// the master secret from the session information during resumption.
	ClientHelloRandomHex string `json:"clientHelloRandom"`
	ServerHelloRandomHex string `json:"serverHelloRandom"`
	ClientRandomHex      string `json:"clientRandom"`
	ServerRandomHex      string `json:"serverRandom"`
}

type tlsKDFTestGroupResponse struct {
	ID    uint64               `json:"tgId"`
	Tests []tlsKDFTestResponse `json:"tests"`
}

type tlsKDFTestResponse struct {
	ID              uint64 `json:"tcId"`
	MasterSecretHex string `json:"masterSecret"`
	KeyBlockHex     string `json:"keyBlock"`
}

type tlsKDF struct{}

func (k *tlsKDF) Process(vectorSet []byte, m Transactable) (interface{}, error) {
	var parsed tlsKDFVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	// See https://pages.nist.gov/ACVP/draft-celi-acvp-kdf-tls.html
	var ret []tlsKDFTestGroupResponse
	for _, group := range parsed.Groups {
		response := tlsKDFTestGroupResponse{
			ID: group.ID,
		}

		var tlsVer string
		switch group.TLSVersion {
		case "v1.0/1.1":
			tlsVer = "1.0"
		case "v1.2":
			tlsVer = "1.2"
		default:
			return nil, fmt.Errorf("unknown TLS version %q", group.TLSVersion)
		}

		hashIsTLS10 := false
		switch group.Hash {
		case "SHA-1":
			hashIsTLS10 = true
		case "SHA2-256", "SHA2-384", "SHA2-512":
			break
		default:
			return nil, fmt.Errorf("unknown hash %q", group.Hash)
		}

		if (tlsVer == "1.0") != hashIsTLS10 {
			return nil, fmt.Errorf("hash %q not permitted with TLS version %q", group.Hash, group.TLSVersion)
		}

		if group.KeyBlockBits%8 != 0 {
			return nil, fmt.Errorf("requested key-block length (%d bits) is not a whole number of bytes", group.KeyBlockBits)
		}

		method := "TLSKDF/" + tlsVer + "/" + group.Hash

		for _, test := range group.Tests {
			pms, err := hex.DecodeString(test.PMSHex)
			if err != nil {
				return nil, err
			}

			clientHelloRandom, err := hex.DecodeString(test.ClientHelloRandomHex)
			if err != nil {
				return nil, err
			}

			serverHelloRandom, err := hex.DecodeString(test.ServerHelloRandomHex)
			if err != nil {
				return nil, err
			}

			clientRandom, err := hex.DecodeString(test.ClientRandomHex)
			if err != nil {
				return nil, err
			}

			serverRandom, err := hex.DecodeString(test.ServerRandomHex)
			if err != nil {
				return nil, err
			}

			const (
				masterSecretLength = 48
				masterSecretLabel  = "master secret"
				keyBlockLabel      = "key expansion"
			)

			var outLenBytes [4]byte
			binary.LittleEndian.PutUint32(outLenBytes[:], uint32(masterSecretLength))
			result, err := m.Transact(method, 1, outLenBytes[:], pms, []byte(masterSecretLabel), clientHelloRandom, serverHelloRandom)
			if err != nil {
				return nil, err
			}

			binary.LittleEndian.PutUint32(outLenBytes[:], uint32(group.KeyBlockBits/8))
			// TLS 1.0, 1.1, and 1.2 use a different order for the client and server
			// randoms when computing the key block.
			result2, err := m.Transact(method, 1, outLenBytes[:], result[0], []byte(keyBlockLabel), serverRandom, clientRandom)
			if err != nil {
				return nil, err
			}

			response.Tests = append(response.Tests, tlsKDFTestResponse{
				ID:              test.ID,
				MasterSecretHex: hex.EncodeToString(result[0]),
				KeyBlockHex:     hex.EncodeToString(result2[0]),
			})
		}

		ret = append(ret, response)
	}

	return ret, nil
}
