// run_cavp.go processes CAVP input files and generates suitable response
// files, optionally comparing the results against the provided FAX files.
package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"time"
)

var (
	oraclePath = flag.String("oracle-bin", "", "Path to the oracle binary")
	suiteDir   = flag.String("suite-dir", "", "Base directory containing the CAVP test suite")
	noFAX      = flag.Bool("no-fax", false, "Skip comparing against FAX files")
)

// test describes a single request file.
type test struct {
	// inFile is the base of the filename without an extension, i.e.
	// “ECBMCT128”.
	inFile string
	// args are the arguments (not including the input filename) to the
	// oracle binary.
	args []string
	// noFAX, if true, indicates that the output cannot be compared against
	// the FAX file. (E.g. because the primitive is non-deterministic.)
	noFAX bool
}

// testSuite describes a series of tests that are handled by a single oracle
// binary.
type testSuite struct {
	// directory is the name of the directory in the CAVP input, i.e. “AES”.
	directory string
	// suite names the test suite to pass as the first command-line argument.
	suite string
	// faxScanFunc, if not nil, is the function to use instead of
	// (*bufio.Scanner).Scan. This can be used to skip lines.
	faxScanFunc func(*bufio.Scanner) bool
	tests       []test
}

func (t *testSuite) getDirectory() string {
	return filepath.Join(*suiteDir, t.directory)
}

var aesGCMTests = testSuite{
	"AES_GCM",
	"aes_gcm",
	nil,
	[]test{
		{"gcmDecrypt128", []string{"dec", "aes-128-gcm"}, false},
		{"gcmDecrypt256", []string{"dec", "aes-256-gcm"}, false},
		{"gcmEncryptExtIV128", []string{"enc", "aes-128-gcm"}, false},
		{"gcmEncryptExtIV256", []string{"enc", "aes-256-gcm"}, false},
	},
}

var aesTests = testSuite{
	"AES",
	"aes",
	nil,
	[]test{
		{"CBCGFSbox128", []string{"kat", "aes-128-cbc"}, false},
		{"CBCGFSbox192", []string{"kat", "aes-192-cbc"}, false},
		{"CBCGFSbox256", []string{"kat", "aes-256-cbc"}, false},
		{"CBCKeySbox128", []string{"kat", "aes-128-cbc"}, false},
		{"CBCKeySbox192", []string{"kat", "aes-192-cbc"}, false},
		{"CBCKeySbox256", []string{"kat", "aes-256-cbc"}, false},
		{"CBCMMT128", []string{"kat", "aes-128-cbc"}, false},
		{"CBCMMT192", []string{"kat", "aes-192-cbc"}, false},
		{"CBCMMT256", []string{"kat", "aes-256-cbc"}, false},
		{"CBCVarKey128", []string{"kat", "aes-128-cbc"}, false},
		{"CBCVarKey192", []string{"kat", "aes-192-cbc"}, false},
		{"CBCVarKey256", []string{"kat", "aes-256-cbc"}, false},
		{"CBCVarTxt128", []string{"kat", "aes-128-cbc"}, false},
		{"CBCVarTxt192", []string{"kat", "aes-192-cbc"}, false},
		{"CBCVarTxt256", []string{"kat", "aes-256-cbc"}, false},
		{"ECBGFSbox128", []string{"kat", "aes-128-ecb"}, false},
		{"ECBGFSbox192", []string{"kat", "aes-192-ecb"}, false},
		{"ECBGFSbox256", []string{"kat", "aes-256-ecb"}, false},
		{"ECBKeySbox128", []string{"kat", "aes-128-ecb"}, false},
		{"ECBKeySbox192", []string{"kat", "aes-192-ecb"}, false},
		{"ECBKeySbox256", []string{"kat", "aes-256-ecb"}, false},
		{"ECBMMT128", []string{"kat", "aes-128-ecb"}, false},
		{"ECBMMT192", []string{"kat", "aes-192-ecb"}, false},
		{"ECBMMT256", []string{"kat", "aes-256-ecb"}, false},
		{"ECBVarKey128", []string{"kat", "aes-128-ecb"}, false},
		{"ECBVarKey192", []string{"kat", "aes-192-ecb"}, false},
		{"ECBVarKey256", []string{"kat", "aes-256-ecb"}, false},
		{"ECBVarTxt128", []string{"kat", "aes-128-ecb"}, false},
		{"ECBVarTxt192", []string{"kat", "aes-192-ecb"}, false},
		{"ECBVarTxt256", []string{"kat", "aes-256-ecb"}, false},
		// AES Monte-Carlo tests
		{"ECBMCT128", []string{"mct", "aes-128-ecb"}, false},
		{"ECBMCT192", []string{"mct", "aes-192-ecb"}, false},
		{"ECBMCT256", []string{"mct", "aes-256-ecb"}, false},
		{"CBCMCT128", []string{"mct", "aes-128-cbc"}, false},
		{"CBCMCT192", []string{"mct", "aes-192-cbc"}, false},
		{"CBCMCT256", []string{"mct", "aes-256-cbc"}, false},
	},
}

var ecdsa2KeyPairTests = testSuite{
	"ECDSA2",
	"ecdsa2_keypair",
	nil,
	[]test{{"KeyPair", nil, true}},
}

var ecdsa2PKVTests = testSuite{
	"ECDSA2",
	"ecdsa2_pkv",
	nil,
	[]test{{"PKV", nil, false}},
}

var ecdsa2SigGenTests = testSuite{
	"ECDSA2",
	"ecdsa2_siggen",
	nil,
	[]test{
		{"SigGen", []string{"SigGen"}, true},
		{"SigGenComponent", []string{"SigGenComponent"}, true},
	},
}

var ecdsa2SigVerTests = testSuite{
	"ECDSA2",
	"ecdsa2_sigver",
	nil,
	[]test{{"SigVer", nil, false}},
}

var rsa2KeyGenTests = testSuite{
	"RSA2",
	"rsa2_keygen",
	nil,
	[]test{
		{"KeyGen_RandomProbablyPrime3_3", nil, true},
	},
}

var rsa2SigGenTests = testSuite{
	"RSA2",
	"rsa2_siggen",
	nil,
	[]test{
		{"SigGen15_186-3", []string{"pkcs15"}, true},
		{"SigGenPSS_186-3", []string{"pss"}, true},
	},
}

var rsa2SigVerTests = testSuite{
	"RSA2",
	"rsa2_sigver",
	func(s *bufio.Scanner) bool {
		for {
			if !s.Scan() {
				return false
			}

			line := s.Text()
			if strings.HasPrefix(line, "p = ") || strings.HasPrefix(line, "d = ") || strings.HasPrefix(line, "SaltVal = ") || strings.HasPrefix(line, "EM with ") {
				continue
			}
			if strings.HasPrefix(line, "q = ") {
				// Skip the "q = " line and an additional blank line.
				if !s.Scan() {
					return false
				}
				if len(strings.TrimSpace(s.Text())) > 0 {
					return false
				}
				continue
			}
			return true
		}
	},
	[]test{
		{"SigVer15_186-3", []string{"pkcs15"}, false},
		{"SigVerPSS_186-3", []string{"pss"}, false},
	},
}

var hmacTests = testSuite{
	"HMAC",
	"hmac",
	nil,
	[]test{{"HMAC", nil, false}},
}

var shaTests = testSuite{
	"SHA",
	"sha",
	nil,
	[]test{
		{"SHA1LongMsg", []string{"SHA1"}, false},
		{"SHA1ShortMsg", []string{"SHA1"}, false},
		{"SHA224LongMsg", []string{"SHA224"}, false},
		{"SHA224ShortMsg", []string{"SHA224"}, false},
		{"SHA256LongMsg", []string{"SHA256"}, false},
		{"SHA256ShortMsg", []string{"SHA256"}, false},
		{"SHA384LongMsg", []string{"SHA384"}, false},
		{"SHA384ShortMsg", []string{"SHA384"}, false},
		{"SHA512LongMsg", []string{"SHA512"}, false},
		{"SHA512ShortMsg", []string{"SHA512"}, false},
	},
}

var shaMonteTests = testSuite{
	"SHA",
	"sha_monte",
	nil,
	[]test{
		{"SHA1Monte", []string{"SHA1"}, false},
		{"SHA224Monte", []string{"SHA224"}, false},
		{"SHA256Monte", []string{"SHA256"}, false},
		{"SHA384Monte", []string{"SHA384"}, false},
		{"SHA512Monte", []string{"SHA512"}, false},
	},
}

var ctrDRBGTests = testSuite{
	"DRBG800-90A",
	"ctr_drbg",
	nil,
	[]test{{"CTR_DRBG", nil, false}},
}

var tdesTests = testSuite{
	"TDES",
	"tdes",
	nil,
	[]test{
		{"TCBCMMT2", []string{"kat", "des-ede-cbc"}, false},
		{"TCBCMMT3", []string{"kat", "des-ede3-cbc"}, false},
		{"TCBCMonte2", []string{"mct", "des-ede3-cbc"}, false},
		{"TCBCMonte3", []string{"mct", "des-ede3-cbc"}, false},
		{"TCBCinvperm", []string{"kat", "des-ede3-cbc"}, false},
		{"TCBCpermop", []string{"kat", "des-ede3-cbc"}, false},
		{"TCBCsubtab", []string{"kat", "des-ede3-cbc"}, false},
		{"TCBCvarkey", []string{"kat", "des-ede3-cbc"}, false},
		{"TCBCvartext", []string{"kat", "des-ede3-cbc"}, false},
		{"TECBMMT2", []string{"kat", "des-ede"}, false},
		{"TECBMMT3", []string{"kat", "des-ede3"}, false},
		{"TECBMonte2", []string{"mct", "des-ede3"}, false},
		{"TECBMonte3", []string{"mct", "des-ede3"}, false},
		{"TECBinvperm", []string{"kat", "des-ede3"}, false},
		{"TECBpermop", []string{"kat", "des-ede3"}, false},
		{"TECBsubtab", []string{"kat", "des-ede3"}, false},
		{"TECBvarkey", []string{"kat", "des-ede3"}, false},
		{"TECBvartext", []string{"kat", "des-ede3"}, false},
	},
}

var keyWrapTests = testSuite{
	"KeyWrap38F",
	"keywrap",
	nil,
	[]test{
		{"KW_AD_128", []string{"dec", "128"}, false},
		{"KW_AD_256", []string{"dec", "256"}, false},
		{"KW_AE_128", []string{"enc", "128"}, false},
		{"KW_AE_256", []string{"enc", "256"}, false},
	},
}

var allTestSuites = []*testSuite{
	&aesGCMTests,
	&aesTests,
	&ctrDRBGTests,
	&ecdsa2KeyPairTests,
	&ecdsa2PKVTests,
	&ecdsa2SigGenTests,
	&ecdsa2SigVerTests,
	&hmacTests,
	&keyWrapTests,
	&rsa2KeyGenTests,
	&rsa2SigGenTests,
	&rsa2SigVerTests,
	&shaTests,
	&shaMonteTests,
	&tdesTests,
}

// testInstance represents a specific test in a testSuite.
type testInstance struct {
	suite     *testSuite
	testIndex int
}

func worker(wg *sync.WaitGroup, work <-chan testInstance) {
	defer wg.Done()

	for ti := range work {
		test := ti.suite.tests[ti.testIndex]

		if err := doTest(ti.suite, test); err != nil {
			fmt.Fprintf(os.Stderr, "%s\n", err)
			os.Exit(2)
		}

		if !*noFAX && !test.noFAX {
			if err := compareFAX(ti.suite, test); err != nil {
				fmt.Fprintf(os.Stderr, "%s\n", err)
				os.Exit(3)
			}
		}
	}
}

func main() {
	flag.Parse()

	if len(*oraclePath) == 0 {
		fmt.Fprintf(os.Stderr, "Must give -oracle-bin\n")
		os.Exit(1)
	}

	work := make(chan testInstance)
	var wg sync.WaitGroup

	for i := 0; i < runtime.NumCPU(); i++ {
		wg.Add(1)
		go worker(&wg, work)
	}

	for _, suite := range allTestSuites {
		for i := range suite.tests {
			work <- testInstance{suite, i}
		}
	}

	close(work)
	wg.Wait()
}

func doTest(suite *testSuite, test test) error {
	args := []string{suite.suite}
	args = append(args, test.args...)
	args = append(args, filepath.Join(suite.getDirectory(), "req", test.inFile+".req"))

	respDir := filepath.Join(suite.getDirectory(), "resp")
	if err := os.Mkdir(respDir, 0755); err != nil && !os.IsExist(err) {
		return fmt.Errorf("cannot create resp directory: %s", err)
	}
	outPath := filepath.Join(respDir, test.inFile+".rsp")
	outFile, err := os.OpenFile(outPath, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, 0644)
	if err != nil {
		return fmt.Errorf("cannot open output file for %q %q: %s", suite.getDirectory(), test.inFile, err)
	}
	defer outFile.Close()

	cmd := exec.Command(*oraclePath, args...)
	cmd.Stdout = outFile
	cmd.Stderr = os.Stderr

	cmdLine := strings.Join(append([]string{*oraclePath}, args...), " ")
	startTime := time.Now()
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("cannot run command for %q %q (%s): %s", suite.getDirectory(), test.inFile, cmdLine, err)
	}

	fmt.Printf("%s (%ds)\n", cmdLine, int(time.Since(startTime).Seconds()))

	return nil
}

func canonicalizeLine(in string) string {
	if strings.HasPrefix(in, "Result = P (") {
		return "Result = P"
	}
	if strings.HasPrefix(in, "Result = F (") {
		return "Result = F"
	}
	return in
}

func compareFAX(suite *testSuite, test test) error {
	faxScanFunc := suite.faxScanFunc
	if faxScanFunc == nil {
		faxScanFunc = (*bufio.Scanner).Scan
	}

	respPath := filepath.Join(suite.getDirectory(), "resp", test.inFile+".rsp")
	respFile, err := os.Open(respPath)
	if err != nil {
		return fmt.Errorf("cannot read output of %q %q: %s", suite.getDirectory(), test.inFile, err)
	}
	defer respFile.Close()

	faxPath := filepath.Join(suite.getDirectory(), "fax", test.inFile+".fax")
	faxFile, err := os.Open(faxPath)
	if err != nil {
		return fmt.Errorf("cannot open fax file for %q %q: %s", suite.getDirectory(), test.inFile, err)
	}
	defer faxFile.Close()

	respScanner := bufio.NewScanner(respFile)
	faxScanner := bufio.NewScanner(faxFile)

	lineNo := 0
	inHeader := true

	for respScanner.Scan() {
		lineNo++
		respLine := respScanner.Text()
		var faxLine string

		if inHeader && (len(respLine) == 0 || respLine[0] == '#') {
			continue
		}

		for {
			haveFaxLine := false

			if inHeader {
				for faxScanFunc(faxScanner) {
					faxLine = faxScanner.Text()
					if len(faxLine) != 0 && faxLine[0] != '#' {
						haveFaxLine = true
						break
					}
				}

				inHeader = false
			} else {
				if faxScanFunc(faxScanner) {
					faxLine = faxScanner.Text()
					haveFaxLine = true
				}
			}

			if !haveFaxLine {
				// Ignore blank lines at the end of the generated file.
				if len(respLine) == 0 {
					break
				}
				return fmt.Errorf("resp file is longer than fax for %q %q", suite.getDirectory(), test.inFile)
			}

			if strings.HasPrefix(faxLine, " (Reason: ") {
				continue
			}

			break
		}

		if canonicalizeLine(faxLine) == canonicalizeLine(respLine) {
			continue
		}

		return fmt.Errorf("resp and fax differ at line %d for %q %q: %q vs %q", lineNo, suite.getDirectory(), test.inFile, respLine, faxLine)
	}

	if faxScanFunc(faxScanner) {
		return fmt.Errorf("fax file is longer than resp for %q %q", suite.getDirectory(), test.inFile)
	}

	return nil
}
