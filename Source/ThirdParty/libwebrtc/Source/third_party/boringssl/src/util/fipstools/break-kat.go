// break-kat corrupts a known-answer-test input in a binary and writes the
// corrupted binary to stdout. This is used to demonstrate that the KATs in the
// binary notice the error.
package main

import (
	"bytes"
	"encoding/hex"
	"flag"
	"fmt"
	"os"
	"sort"
)

var (
	kats = map[string]string{
		"HMAC-SHA-256":    "dad91293dfcf2a7c8ecd13fe353fa75b",
		"AES-CBC-encrypt": "078609a6c5ac2544699adf682fa377f9be8ab6aef563e8c56a36b84f557fadd3",
		"AES-CBC-decrypt": "347aa5a024b28257b36510be583d4f47adb7bbeedc6005bbbd0d0a9f06bb7b10",
		"AES-GCM-encrypt": "8fcc4099808e75caaff582898848a88d808b55ab4e9370797d940be8cc1d7884",
		"AES-GCM-decrypt": "35f3058f875760ff09d3120f70c4bc9ed7a86872e13452202176f7371ae04faae1dd391920f5d13953d896785994823c",
		"DRBG":            "c4da0740d505f1ee280b95e58c4931ac6de846a0152fbb4a3f174cf4787a4f1a40c2b50babe14aae530be5886d910a27",
		"DRBG-reseed":     "c7161ca36c2309b716e9859bb96c6d49bdc8352103a18cd24ef42ec97ef46bf446eb1a4576c186e9351803763a7912fe",
		"HKDF":            "68678504b9b3add17d5967a1a7bd37993fd8a33ce7303071f39c096d1635b3c9",
		"SHA-1":           "132fd9bad5c1826263bafbb699f707a5",
		"SHA-256":         "ff3b857da7236a2baa0f396b51522217",
		"SHA-512":         "212512f8d2ad8322781c6c4d69a9daa1",
		"TLS10-KDF":       "abc3657b094c7628a0b282996fe75a75f4984fd94d4ecc2fcf53a2c469a3f731",
		"TLS12-KDF":       "c5438ee26fd4acbd259fc91855dc69bf884ee29322fcbfd2966a4623d42ec781",
		"TLS13-KDF":       "024a0d80f357f2499a1244dac26dab66fc13ed85fca71dace146211119525874",
		"RSA-sign":        "d2b56e53306f720d7929d8708bf46f1c22300305582b115bedcac722d8aa5ab2",
		"RSA-verify":      "abe2cbc13d6bd39d48db5334ddbf8d070a93bdcb104e2cc5d0ee486ee295f6b31bda126c41890b98b73e70e6b65d82f95c663121755a90744c8d1c21148a1960be0eca446e9ff497f1345c537ef8119b9a4398e95c5c6de2b1c955905c5299d8ce7a3b6ab76380d9babdd15f610237e1f3f2aa1c1f1e770b62fbb596381b2ebdd77ecef9c90d4c92f7b6b05fed2936285fa94826e62055322a33b6f04c74ce69e5d8d737fb838b79d2d48e3daf71387531882531a95ac964d02ea413bf85952982bbc089527daff5b845c9a0f4d14ef1956d9c3acae882d12da66da0f35794f5ee32232333517db9315232a183b991654dbea41615345c885325926744a53915",
		"ECDSA-sign":      "1e35930be860d0942ca7bbd6f6ded87f157e4de24f81ed4b875c0e018e89a81f",
		"ECDSA-verify":    "6780c5fc70275e2c7061a0e7877bb174deadeb9887027f3fa83654158ba7f50c2d36e5799790bfbe2183d33e96f3c51f6a232f2a24488c8e5f64c37ea2cf0529",
		"Z-computation":   "e7604491269afb5b102d6ea52cb59feb70aede6ce3bfb3e0105485abd861d77b",
		"FFDH":            "a14f8ad36be37b18b8f35864392f150ab7ee22c47e1870052a3f17918274af18aaeaf4cf6aacfde96c9d586eb7ebaff6b03fe3b79a8e2ff9dd6df34caaf2ac70fd3771d026b41a561ee90e4337d0575f8a0bd160c868e7e3cef88aa1d88448b1e4742ba11480a9f8a8b737347c408d74a7d57598c48875629df0c85327a124ddec1ad50cd597a985588434ce19c6f044a1696b5f244b899b7e77d4f6f20213ae8eb15d37eb8e67e6c8bdbc4fd6e17426283da96f23a897b210058c7c70fb126a5bf606dbeb1a6d5cca04184c4e95c2e8a70f50f5c1eabd066bd79c180456316ac02d366eb3b0e7ba82fb70dcbd737ca55734579dd250fffa8e0584be99d32b35",
	}

	listTests = flag.Bool("list-tests", false, "List known test values and exit")
)

func main() {
	flag.Parse()

	if *listTests {
		for _, kat := range sortedKATs() {
			fmt.Println(kat)
		}
		os.Exit(0)
	}

	if flag.NArg() != 2 || kats[flag.Arg(1)] == "" {
		fmt.Fprintln(os.Stderr, "Usage: break-kat <binary path> <test to break> > output")
		fmt.Fprintln(os.Stderr, "Possible values for <test to break>:")
		for _, kat := range sortedKATs() {
			fmt.Fprintln(os.Stderr, " ", kat)
		}
		os.Exit(1)
	}

	inPath := flag.Arg(0)
	test := flag.Arg(1)
	testInputValue, err := hex.DecodeString(kats[test])
	if err != nil {
		panic("invalid kat data: " + err.Error())
	}

	binaryContents, err := os.ReadFile(inPath)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}

	i := bytes.Index(binaryContents, testInputValue)
	if i < 0 {
		fmt.Fprintln(os.Stderr, "Expected test input value was not found in binary.")
		os.Exit(3)
	}

	// Zero out the entire value because the compiler may produce code
	// where parts of the value are embedded in the instructions.
	for j := range testInputValue {
		binaryContents[i+j] = 0
	}

	if bytes.Index(binaryContents, testInputValue) >= 0 {
		fmt.Fprintln(os.Stderr, "Test input value was still found after erasing it. Second copy?")
		os.Exit(4)
	}

	os.Stdout.Write(binaryContents)
}

func sortedKATs() []string {
	var ret []string
	for kat := range kats {
		ret = append(ret, kat)
	}
	sort.Strings(ret)
	return ret
}
