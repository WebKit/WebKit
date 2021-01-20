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

// NOTES:
// - subprocess_test does not include testing for all subcomponents. It does
//   include unit tests for the following:
//   - hashPrimitive (for sha2-256 only)
//   - blockCipher (for AES)
//   - drbg (for ctrDRBG)
// - All sample data (the valid & invalid strings) comes from calls to acvp as
//   of 2020-04-02.

import (
	"encoding/hex"
	"encoding/json"
	"fmt"
	"reflect"
	"testing"
)

var validSHA2_256 = []byte(`{
  "vsId" : 182183,
  "algorithm" : "SHA2-256",
  "revision" : "1.0",
  "isSample" : true,
  "testGroups" : [ {
    "tgId" : 1,
    "testType" : "AFT",
    "tests" : [ {
      "tcId" : 1,
      "msg" : "",
      "len" : 0
    }, {
      "tcId" : 2,
      "msg" : "",
      "len" : 0
    }, {
      "tcId" : 3,
      "msg" : "8E",
      "len" : 8
    }, {
      "tcId" : 4,
      "msg" : "7F10",
      "len" : 16
    }, {
      "tcId" : 5,
      "msg" : "F4422F",
      "len" : 24
    }, {
      "tcId" : 6,
      "msg" : "B3EF9698",
      "len" : 32
    }]
  }]
}`)

var callsSHA2_256 = []fakeTransactCall{
	fakeTransactCall{cmd: "SHA2-256", expectedNumResults: 1, args: [][]byte{[]byte{}}},
	fakeTransactCall{cmd: "SHA2-256", expectedNumResults: 1, args: [][]byte{[]byte{}}},
	fakeTransactCall{cmd: "SHA2-256", expectedNumResults: 1, args: [][]byte{fromHex("8E")}},
	fakeTransactCall{cmd: "SHA2-256", expectedNumResults: 1, args: [][]byte{fromHex("7F10")}},
	fakeTransactCall{cmd: "SHA2-256", expectedNumResults: 1, args: [][]byte{fromHex("F4422F")}},
	fakeTransactCall{cmd: "SHA2-256", expectedNumResults: 1, args: [][]byte{fromHex("B3EF9698")}},
}

var invalidSHA2_256 = []byte(`{
  "vsId" : 180207,
  "algorithm" : "SHA2-256",
  "revision" : "1.0",
  "isSample" : true,
  "testGroups" : [ {
    "tgId" : abc,
    "testType" : "AFT",
    "tests" : [ {
      "tcId" : 1,
      "msg" : "",
      "len" : 0
    }, {
      "tcId" : 2,
      "msg" : "",
      "len" : 0
    }]
  }]
}`)

var validKDFJSON = []byte(`{
  "vsId": 1564,
  "algorithm": "counterMode",
  "revision": "1.0",
  "testGroups": [{
    "tgId": 1,
    "kdfMode": "counter",
    "macMode": "CMAC-AES128",
    "counterLocation": "after fixed data",
    "keyOutLength": 1024,
    "counterLength": 8,
    "tests": [{
        "tcId": 1,
        "keyIn": "5DA38931E8D9174BC3279C8942D2DB82",
        "deferred": false
      },
      {
        "tcId": 2,
        "keyIn": "58F5426A40E3D5D2C94F0F97EB30C739",
        "deferred": false
      }
    ]
  }]
}`)

var callsKDF = []fakeTransactCall{
	fakeTransactCall{cmd: "KDF-counter", expectedNumResults: 3, args: [][]byte{
		uint32le(128),                               // outputBytes
		[]byte("CMAC-AES128"),                       // macMode
		[]byte("after fixed data"),                  // counterLocation
		fromHex("5DA38931E8D9174BC3279C8942D2DB82"), // keyIn
		uint32le(8),                                 // counterLength
	}},
	fakeTransactCall{cmd: "KDF-counter", expectedNumResults: 3, args: [][]byte{
		uint32le(128),                               // outputBytes
		[]byte("CMAC-AES128"),                       // macMode
		[]byte("after fixed data"),                  // counterLocation
		fromHex("58F5426A40E3D5D2C94F0F97EB30C739"), // keyIn
		uint32le(8),                                 // counterLength
	}},
}

var invalidKDFJSON = []byte(`{
  "vsId": 1564,
  "algorithm": "counterMode",
  "revision": "1.0",
  "testGroups": [{
    "tgId": 1,
    "kdfMode": "counter",
    "macMode": "CMAC-AES128",
    "counterLocation": "after fixed data",
    "keyOutLength": 1024,
    "counterLength": 8,
    "tests": [{
        "tcId": 1,
        "keyIn": "5DA38931E8D9174BC3279C8942D2DB82",
        "deferred": false
      },
      {
        "tcId": abc,
        "keyIn": "58F5426A40E3D5D2C94F0F97EB30C739",
        "deferred": false
      }
    ]
  }]
}`)

var validACVPAESECB = []byte(`{
  "vsId" : 181726,
  "algorithm" : "ACVP-AES-ECB",
  "revision" : "1.0",
  "isSample" : true,
  "testGroups" : [ {
    "tgId" : 1,
    "testType" : "AFT",
    "direction" : "encrypt",
    "keyLen" : 128,
    "tests" : [ {
      "tcId" : 1,
      "pt" : "F34481EC3CC627BACD5DC3FB08F273E6",
      "key" : "00000000000000000000000000000000"
    }, {
      "tcId" : 2,
      "pt" : "9798C4640BAD75C7C3227DB910174E72",
      "key" : "00000000000000000000000000000000"
    }]
  }]
}`)

var invalidACVPAESECB = []byte(`{
  "vsId" : 181726,
  "algorithm" : "ACVP-AES-ECB",
  "revision" : "1.0",
  "isSample" : true,
  "testGroups" : [ {
    "tgId" : 1,
    "testType" : "AFT",
    "direction" : "encrypt",
    "keyLen" : 128,
    "tests" : [ {
      "tcId" : abc,
      "pt" : "F34481EC3CC627BACD5DC3FB08F273E6",
      "key" : "00000000000000000000000000000000"
    }, {
      "tcId" : 2,
      "pt" : "9798C4640BAD75C7C3227DB910174E72",
      "key" : "00000000000000000000000000000000"
    }]
  }]
}`)

var callsACVPAESECB = []fakeTransactCall{
	fakeTransactCall{cmd: "AES/encrypt", expectedNumResults: 1, args: [][]byte{
		fromHex("00000000000000000000000000000000"),
		fromHex("F34481EC3CC627BACD5DC3FB08F273E6"),
	}},
	fakeTransactCall{cmd: "AES/encrypt", expectedNumResults: 1, args: [][]byte{
		fromHex("00000000000000000000000000000000"),
		fromHex("9798C4640BAD75C7C3227DB910174E72"),
	}},
}

var validCTRDRBG = []byte(`{
  "vsId" : 181791,
  "algorithm" : "ctrDRBG",
  "revision" : "1.0",
  "isSample" : true,
  "testGroups" : [ {
    "tgId" : 1,
    "testType" : "AFT",
    "derFunc" : false,
    "reSeed" : false,
    "predResistance" : false,
    "entropyInputLen" : 384,
    "nonceLen" : 0,
    "persoStringLen" : 0,
    "additionalInputLen" : 0,
    "returnedBitsLen" : 2048,
    "mode" : "AES-256",
    "tests" : [ {
      "tcId" : 1,
      "entropyInput" : "0D9E8EB273307D95C616C7ACC65669C246265E8A850EDCF36990D8A6F7EC3AEA0A7DDB888EE8D7ECC19EA7830310782C",
      "nonce" : "",
      "persoString" : "",
      "otherInput" : [ {
        "intendedUse" : "generate",
        "additionalInput" : "",
        "entropyInput" : ""
      }, {
        "intendedUse" : "generate",
        "additionalInput" : "",
        "entropyInput" : ""
      } ]
    }]
  }]
}`)

var callsCTRDRBG = []fakeTransactCall{
	fakeTransactCall{cmd: "ctrDRBG/AES-256", expectedNumResults: 1, args: [][]byte{
		fromHex("00010000"), // uint32(256)
		fromHex("0D9E8EB273307D95C616C7ACC65669C246265E8A850EDCF36990D8A6F7EC3AEA0A7DDB888EE8D7ECC19EA7830310782C"),
		[]byte{},
		[]byte{},
		[]byte{},
		[]byte{},
	}},
}

var invalidCTRDRBG = []byte(`{
  "vsId" : 181791,
  "algorithm" : "ctrDRBG",
  "revision" : "1.0",
  "isSample" : true,
  "testGroups" : [ {
    "tgId" : 1,
    "testType" : "AFT",
    "derFunc" : false,
    "reSeed" : false,
    "predResistance" : false,
    "entropyInputLen" : 384,
    "nonceLen" : 0,
    "persoStringLen" : 0,
    "additionalInputLen" : 0,
    "returnedBitsLen" : 2048,
    "mode" : "AES-256",
    "tests" : [ {
      "tcId" : abc,
      "entropyInput" : "0D9E8EB273307D95C616C7ACC65669C246265E8A850EDCF36990D8A6F7EC3AEA0A7DDB888EE8D7ECC19EA7830310782C",
      "nonce" : "",
      "persoString" : "",
      "otherInput" : [ {
        "intendedUse" : "generate",
        "additionalInput" : "",
        "entropyInput" : ""
      }, {
        "intendedUse" : "generate",
        "additionalInput" : "",
        "entropyInput" : ""
      } ]
    }]
  }]
}`)

var validCMACAESJSON = []byte(`{
	"vsId": 1,
	"algorithm": "CMAC-AES",
	"revision": "1.0",
	"testGroups": [{
			"tgId": 4,
			"testType": "AFT",
			"direction": "gen",
			"keyLen": 128,
			"msgLen": 2752,
			"macLen": 64,
			"tests": [{
					"tcId": 25,
					"key": "E2547E38B28B2C24892C133FF4770688",
					"message": "89DE09D747FB4B2669B59759A15BAAF068CAF31FD938DFCFFB38ECED53BA91DD659FD91E6CCCFEC5F972B1AD66BF78FE7FE319E58F514362FC75A346C144981B63FD18195A2AD482AF83711C9ADC449F7EAD32EBD5F4DB7EB93348404EAD496B8F4C89AB5FF7ACB2CFEFD96BD0FC9645B6F1F30AB02767ECA8771106DCA47188EE42183121FB9172B8E2133DE084F6CA3924E4BF3638ADA77DAAA6F06A6494E32CBAEFC6C6D0699BB12A425DCFE5974F687B6A71879D42DE08DF018A96429CFA40E32378D35E46A4956C5D7916B6877F353D33075FD4C64F32C3250D74FF070EA358135664CD8C9B82C9454EED75A12EEC758A9E514053533A884560FAC96DDBBA4AEEB8E473F4BFDB8447B22800D7782320D6E2DAC2599111F8CA598D6720CA7C6E4FC5EDC54FC3576460AAD1644B04E1D2C81B93EA49090FDB7E33374C243B2F19177405B94BEC3C69CC24CC686D8F2B01A6B2A350E394"
				}]
	}]
}`)

var callsCMACAES = []fakeTransactCall{
	fakeTransactCall{cmd: "CMAC-AES", expectedNumResults: 1, args: [][]byte{
		uint32le(64 / 8), // outputBytes
		fromHex("E2547E38B28B2C24892C133FF4770688"), // key
		fromHex("89DE09D747FB4B2669B59759A15BAAF068CAF31FD938DFCFFB38ECED53BA91DD659FD91E6CCCFEC5F972B1AD66BF78FE7FE319E58F514362FC75A346C144981B63FD18195A2AD482AF83711C9ADC449F7EAD32EBD5F4DB7EB93348404EAD496B8F4C89AB5FF7ACB2CFEFD96BD0FC9645B6F1F30AB02767ECA8771106DCA47188EE42183121FB9172B8E2133DE084F6CA3924E4BF3638ADA77DAAA6F06A6494E32CBAEFC6C6D0699BB12A425DCFE5974F687B6A71879D42DE08DF018A96429CFA40E32378D35E46A4956C5D7916B6877F353D33075FD4C64F32C3250D74FF070EA358135664CD8C9B82C9454EED75A12EEC758A9E514053533A884560FAC96DDBBA4AEEB8E473F4BFDB8447B22800D7782320D6E2DAC2599111F8CA598D6720CA7C6E4FC5EDC54FC3576460AAD1644B04E1D2C81B93EA49090FDB7E33374C243B2F19177405B94BEC3C69CC24CC686D8F2B01A6B2A350E394"), // msg
	}},
}

var invalidCMACAESJSON = []byte(`{
	"vsId": 1,
	"algorithm": "CMAC-AES",
	"revision": "1.0",
	"testGroups": [{
			"tgId": 4,
			"testType": "AFT",
			"direction": "gen",
			"keyLen": 128,
			"msgLen": 2752,
			"macLen": 64,
			"tests": [{
					"tcId": abc,
					"key": "E2547E38B28B2C24892C133FF4770688",
					"message": "89DE09D747FB4B2669B59759A15BAAF068CAF31FD938DFCFFB38ECED53BA91DD659FD91E6CCCFEC5F972B1AD66BF78FE7FE319E58F514362FC75A346C144981B63FD18195A2AD482AF83711C9ADC449F7EAD32EBD5F4DB7EB93348404EAD496B8F4C89AB5FF7ACB2CFEFD96BD0FC9645B6F1F30AB02767ECA8771106DCA47188EE42183121FB9172B8E2133DE084F6CA3924E4BF3638ADA77DAAA6F06A6494E32CBAEFC6C6D0699BB12A425DCFE5974F687B6A71879D42DE08DF018A96429CFA40E32378D35E46A4956C5D7916B6877F353D33075FD4C64F32C3250D74FF070EA358135664CD8C9B82C9454EED75A12EEC758A9E514053533A884560FAC96DDBBA4AEEB8E473F4BFDB8447B22800D7782320D6E2DAC2599111F8CA598D6720CA7C6E4FC5EDC54FC3576460AAD1644B04E1D2C81B93EA49090FDB7E33374C243B2F19177405B94BEC3C69CC24CC686D8F2B01A6B2A350E394"
				}]
	}]
}`)

// fakeTransactable provides a fake to return results that don't go to the ACVP
// server.
type fakeTransactable struct {
	calls   []fakeTransactCall
	results []fakeTransactResult
}

type fakeTransactCall struct {
	cmd                string
	expectedNumResults int
	args               [][]byte
}

type fakeTransactResult struct {
	bytes [][]byte
	err   error
}

func (f *fakeTransactable) Transact(cmd string, expectedNumResults int, args ...[]byte) ([][]byte, error) {
	f.calls = append(f.calls, fakeTransactCall{cmd, expectedNumResults, args})

	if len(f.results) == 0 {
		return nil, fmt.Errorf("Transact called but no TransactResults remain")
	}

	ret := f.results[0]
	f.results = f.results[1:]
	return ret.bytes, ret.err
}

func newFakeTransactable(numResponses int) *fakeTransactable {
	ret := new(fakeTransactable)

	// Add results requested by caller.
	dummyResult := [][]byte{[]byte("dummy result")}
	for i := 0; i < numResponses; i++ {
		ret.results = append(ret.results, fakeTransactResult{bytes: dummyResult, err: nil})
	}

	return ret
}

// TestPrimitiveParsesJSON verifies that basic JSON parsing with a
// small passing case & a single failing case.
func TestPrimitives(t *testing.T) {
	var tests = []struct {
		algo          string
		p             primitive
		validJSON     []byte
		invalidJSON   []byte
		expectedCalls []fakeTransactCall
		results       []fakeTransactResult
	}{
		{
			algo:          "SHA2-256",
			p:             &hashPrimitive{"SHA2-256", 32},
			validJSON:     validSHA2_256,
			invalidJSON:   invalidSHA2_256,
			expectedCalls: callsSHA2_256,
		},
		{
			algo:          "kdf",
			p:             &kdfPrimitive{},
			validJSON:     validKDFJSON,
			invalidJSON:   invalidKDFJSON,
			expectedCalls: callsKDF,
			results: []fakeTransactResult{
				{bytes: [][]byte{
					fromHex("5DA38931E8D9174BC3279C8942D2DB82"),
					[]byte("data1"),
					[]byte("keyOut1"),
				}},
				{bytes: [][]byte{
					fromHex("58F5426A40E3D5D2C94F0F97EB30C739"),
					[]byte("data2"),
					[]byte("keyOut2"),
				}},
			},
		},
		{
			algo:          "CMAC-AES",
			p:             &keyedMACPrimitive{"CMAC-AES"},
			validJSON:     validCMACAESJSON,
			invalidJSON:   invalidCMACAESJSON,
			expectedCalls: callsCMACAES,
			results: []fakeTransactResult{
				{bytes: [][]byte{
					fromHex("0102030405060708"),
				}},
			},
		},
		{
			algo:          "ACVP-AES-ECB",
			p:             &blockCipher{"AES", 16, false},
			validJSON:     validACVPAESECB,
			invalidJSON:   invalidACVPAESECB,
			expectedCalls: callsACVPAESECB,
		},
		{
			algo:          "ctrDRBG",
			p:             &drbg{"ctrDRBG", map[string]bool{"AES-128": true, "AES-192": true, "AES-256": true}},
			validJSON:     validCTRDRBG,
			invalidJSON:   invalidCTRDRBG,
			expectedCalls: callsCTRDRBG,
			results: []fakeTransactResult{
				fakeTransactResult{bytes: [][]byte{make([]byte, 256)}},
			},
		},
	}

	for _, test := range tests {
		transactable := newFakeTransactable(len(test.expectedCalls))
		if len(test.results) > 0 {
			transactable.results = test.results
		}

		if _, err := test.p.Process(test.validJSON, transactable); err != nil {
			t.Errorf("%s: valid input failed unexpectedly: %v", test.algo, err)
			continue
		}

		if len(transactable.calls) != len(test.expectedCalls) {
			t.Errorf("%s: got %d results, but want %d", test.algo, len(transactable.calls), len(test.expectedCalls))
			continue
		}

		if !reflect.DeepEqual(transactable.calls, test.expectedCalls) {
			t.Errorf("%s: got:\n%#v\n\nwant:\n%#v", test.algo, transactable.calls, test.expectedCalls)
		}

		if _, err := test.p.Process(test.invalidJSON, transactable); !isJSONSyntaxError(err) {
			t.Errorf("Test %v with invalid input either passed or failed with the wrong error (%v)", test.algo, err)
		}
	}
}

// isJSONSyntaxError returns true if the error is a json syntax error.
func isJSONSyntaxError(err error) bool {
	_, ok := err.(*json.SyntaxError)
	return ok
}

// fromHex wraps hex.DecodeString so it can be used in initializers. Panics on error.
func fromHex(s string) []byte {
	key, err := hex.DecodeString(s)
	if err != nil {
		panic(fmt.Sprintf("Failed on hex.DecodeString(%q) with %v", s, err))
	}
	return key
}
