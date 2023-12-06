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

package main

import (
	"bufio"
	"bytes"
	"crypto"
	"crypto/hmac"
	"crypto/sha256"
	"crypto/x509"
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"encoding/pem"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"net/http"
	neturl "net/url"
	"os"
	"path/filepath"
	"strings"
	"time"

	"boringssl.googlesource.com/boringssl/util/fipstools/acvp/acvptool/acvp"
	"boringssl.googlesource.com/boringssl/util/fipstools/acvp/acvptool/subprocess"
)

var (
	dumpRegcap      = flag.Bool("regcap", false, "Print module capabilities JSON to stdout")
	configFilename  = flag.String("config", "config.json", "Location of the configuration JSON file")
	jsonInputFile   = flag.String("json", "", "Location of a vector-set input file")
	uploadInputFile = flag.String("upload", "", "Location of a JSON results file to upload")
	runFlag         = flag.String("run", "", "Name of primitive to run tests for")
	fetchFlag       = flag.String("fetch", "", "Name of primitive to fetch vectors for")
	expectedOutFlag = flag.String("expected-out", "", "Name of a file to write the expected results to")
	wrapperPath     = flag.String("wrapper", "../../../../build/util/fipstools/acvp/modulewrapper/modulewrapper", "Path to the wrapper binary")
)

type Config struct {
	CertPEMFile        string
	PrivateKeyFile     string
	PrivateKeyDERFile  string
	TOTPSecret         string
	ACVPServer         string
	SessionTokensCache string
	LogFile            string
}

func isCommentLine(line []byte) bool {
	var foundCommentStart bool
	for _, b := range line {
		if !foundCommentStart {
			if b == ' ' || b == '\t' {
				continue
			}
			if b != '/' {
				return false
			}
			foundCommentStart = true
		} else {
			return b == '/'
		}
	}
	return false
}

func jsonFromFile(out any, filename string) error {
	in, err := os.Open(filename)
	if err != nil {
		return err
	}
	defer in.Close()

	scanner := bufio.NewScanner(in)
	var commentsRemoved bytes.Buffer
	for scanner.Scan() {
		if isCommentLine(scanner.Bytes()) {
			continue
		}
		commentsRemoved.Write(scanner.Bytes())
		commentsRemoved.WriteString("\n")
	}
	if err := scanner.Err(); err != nil {
		return err
	}

	decoder := json.NewDecoder(&commentsRemoved)
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(out); err != nil {
		return err
	}
	if decoder.More() {
		return errors.New("trailing garbage found")
	}
	return nil
}

// TOTP implements the time-based one-time password algorithm with the suggested
// granularity of 30 seconds. See https://tools.ietf.org/html/rfc6238 and then
// https://tools.ietf.org/html/rfc4226#section-5.3
func TOTP(secret []byte) string {
	const timeStep = 30
	now := uint64(time.Now().Unix()) / 30
	var nowBuf [8]byte
	binary.BigEndian.PutUint64(nowBuf[:], now)
	mac := hmac.New(sha256.New, secret)
	mac.Write(nowBuf[:])
	digest := mac.Sum(nil)
	value := binary.BigEndian.Uint32(digest[digest[31]&15:])
	value &= 0x7fffffff
	value %= 100000000
	return fmt.Sprintf("%08d", value)
}

type Middle interface {
	Close()
	Config() ([]byte, error)
	Process(algorithm string, vectorSet []byte) (any, error)
}

func loadCachedSessionTokens(server *acvp.Server, cachePath string) error {
	cacheDir, err := os.Open(cachePath)
	if err != nil {
		if os.IsNotExist(err) {
			if err := os.Mkdir(cachePath, 0700); err != nil {
				return fmt.Errorf("Failed to create session token cache directory %q: %s", cachePath, err)
			}
			return nil
		}
		return fmt.Errorf("Failed to open session token cache directory %q: %s", cachePath, err)
	}
	defer cacheDir.Close()
	names, err := cacheDir.Readdirnames(0)
	if err != nil {
		return fmt.Errorf("Failed to list session token cache directory %q: %s", cachePath, err)
	}

	loaded := 0
	for _, name := range names {
		if !strings.HasSuffix(name, ".token") {
			continue
		}
		path := filepath.Join(cachePath, name)
		contents, err := os.ReadFile(path)
		if err != nil {
			return fmt.Errorf("Failed to read session token cache entry %q: %s", path, err)
		}
		urlPath, err := neturl.PathUnescape(name[:len(name)-6])
		if err != nil {
			return fmt.Errorf("Failed to unescape token filename %q: %s", name, err)
		}
		server.PrefixTokens[urlPath] = string(contents)
		loaded++
	}

	log.Printf("Loaded %d cached tokens", loaded)
	return nil
}

func trimLeadingSlash(s string) string {
	if strings.HasPrefix(s, "/") {
		return s[1:]
	}
	return s
}

// looksLikeVectorSetHeader returns true iff element looks like it's a
// vectorSetHeader, not a test. Some ACVP files contain a header as the first
// element that should be duplicated into the response, and some don't. If the
// element contains a "url" field, or if it's missing an "algorithm" field,
// then we guess that it's a header.
func looksLikeVectorSetHeader(element json.RawMessage) bool {
	var headerFields struct {
		URL       string `json:"url"`
		Algorithm string `json:"algorithm"`
	}
	if err := json.Unmarshal(element, &headerFields); err != nil {
		return false
	}
	return len(headerFields.URL) > 0 || len(headerFields.Algorithm) == 0
}

// processFile reads a file containing vector sets, at least in the format
// preferred by our lab, and writes the results to stdout.
func processFile(filename string, supportedAlgos []map[string]any, middle Middle) error {
	jsonBytes, err := os.ReadFile(filename)
	if err != nil {
		return err
	}

	var elements []json.RawMessage
	if err := json.Unmarshal(jsonBytes, &elements); err != nil {
		return err
	}

	// There must be at least one element in the file.
	if len(elements) < 1 {
		return errors.New("JSON input is empty")
	}

	var header json.RawMessage
	if looksLikeVectorSetHeader(elements[0]) {
		header, elements = elements[0], elements[1:]
		if len(elements) == 0 {
			return errors.New("JSON input is empty")
		}
	}

	// Build a map of which algorithms our Middle supports.
	algos := make(map[string]struct{})
	for _, supportedAlgo := range supportedAlgos {
		algoInterface, ok := supportedAlgo["algorithm"]
		if !ok {
			continue
		}
		algo, ok := algoInterface.(string)
		if !ok {
			continue
		}
		algos[algo] = struct{}{}
	}

	var result bytes.Buffer
	result.WriteString("[")

	if header != nil {
		headerBytes, err := json.MarshalIndent(header, "", "    ")
		if err != nil {
			return err
		}
		result.Write(headerBytes)
		result.WriteString(",")
	}

	for i, element := range elements {
		var commonFields struct {
			Algo string `json:"algorithm"`
			ID   uint64 `json:"vsId"`
		}
		if err := json.Unmarshal(element, &commonFields); err != nil {
			return fmt.Errorf("failed to extract common fields from vector set #%d", i+1)
		}

		algo := commonFields.Algo
		if _, ok := algos[algo]; !ok {
			return fmt.Errorf("vector set #%d contains unsupported algorithm %q", i+1, algo)
		}

		replyGroups, err := middle.Process(algo, element)
		if err != nil {
			return fmt.Errorf("while processing vector set #%d: %s", i+1, err)
		}

		group := map[string]any{
			"vsId":       commonFields.ID,
			"testGroups": replyGroups,
			"algorithm":  algo,
		}
		replyBytes, err := json.MarshalIndent(group, "", "    ")
		if err != nil {
			return err
		}

		if i != 0 {
			result.WriteString(",")
		}
		result.Write(replyBytes)
	}

	result.WriteString("]\n")
	os.Stdout.Write(result.Bytes())

	return nil
}

// getVectorsWithRetry fetches the given url from the server and parses it as a
// set of vectors. Any server requested retry is handled.
func getVectorsWithRetry(server *acvp.Server, url string) (out acvp.Vectors, vectorsBytes []byte, err error) {
	for {
		if vectorsBytes, err = server.GetBytes(url); err != nil {
			return out, nil, err
		}

		var vectors acvp.Vectors
		if err := json.Unmarshal(vectorsBytes, &vectors); err != nil {
			return out, nil, err
		}

		retry := vectors.Retry
		if retry == 0 {
			return vectors, vectorsBytes, nil
		}

		log.Printf("Server requested %d seconds delay", retry)
		if retry > 10 {
			retry = 10
		}
		time.Sleep(time.Duration(retry) * time.Second)
	}
}

func uploadResult(server *acvp.Server, setURL string, resultData []byte) error {
	resultSize := uint64(len(resultData)) + 32 /* for framing overhead */
	if server.SizeLimit == 0 || resultSize < server.SizeLimit {
		log.Printf("Result size %d bytes", resultSize)
		return server.Post(nil, trimLeadingSlash(setURL)+"/results", resultData)
	}

	// The NIST ACVP server no longer requires the large-upload process,
	// suggesting that this may no longer be needed.
	log.Printf("Result is %d bytes, too much given server limit of %d bytes. Using large-upload process.", resultSize, server.SizeLimit)
	largeRequestBytes, err := json.Marshal(acvp.LargeUploadRequest{
		Size: resultSize,
		URL:  setURL,
	})
	if err != nil {
		return errors.New("failed to marshal large-upload request: " + err.Error())
	}

	var largeResponse acvp.LargeUploadResponse
	if err := server.Post(&largeResponse, "/large", largeRequestBytes); err != nil {
		return errors.New("failed to request large-upload endpoint: " + err.Error())
	}

	log.Printf("Directed to large-upload endpoint at %q", largeResponse.URL)
	req, err := http.NewRequest("POST", largeResponse.URL, bytes.NewBuffer(resultData))
	if err != nil {
		return errors.New("failed to create POST request: " + err.Error())
	}
	token := largeResponse.AccessToken
	if len(token) == 0 {
		token = server.AccessToken
	}
	req.Header.Add("Authorization", "Bearer "+token)
	req.Header.Add("Content-Type", "application/json")

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return errors.New("failed writing large upload: " + err.Error())
	}
	resp.Body.Close()
	if resp.StatusCode != 200 {
		return fmt.Errorf("large upload resulted in status code %d", resp.StatusCode)
	}

	return nil
}

func connect(config *Config, sessionTokensCacheDir string) (*acvp.Server, error) {
	if len(config.TOTPSecret) == 0 {
		return nil, errors.New("config file missing TOTPSecret")
	}
	totpSecret, err := base64.StdEncoding.DecodeString(config.TOTPSecret)
	if err != nil {
		return nil, fmt.Errorf("failed to base64-decode TOTP secret from config file: %s. (Note that the secret _itself_ should be in the config, not the name of a file that contains it.)", err)
	}

	if len(config.CertPEMFile) == 0 {
		return nil, errors.New("config file missing CertPEMFile")
	}
	certPEM, err := os.ReadFile(config.CertPEMFile)
	if err != nil {
		return nil, fmt.Errorf("failed to read certificate from %q: %s", config.CertPEMFile, err)
	}
	block, _ := pem.Decode(certPEM)
	certDER := block.Bytes

	if len(config.PrivateKeyDERFile) == 0 && len(config.PrivateKeyFile) == 0 {
		return nil, errors.New("config file missing PrivateKeyDERFile and PrivateKeyFile")
	}
	if len(config.PrivateKeyDERFile) != 0 && len(config.PrivateKeyFile) != 0 {
		return nil, errors.New("config file has both PrivateKeyDERFile and PrivateKeyFile. Can only have one.")
	}
	privateKeyFile := config.PrivateKeyDERFile
	if len(config.PrivateKeyFile) > 0 {
		privateKeyFile = config.PrivateKeyFile
	}

	keyBytes, err := os.ReadFile(privateKeyFile)
	if err != nil {
		return nil, fmt.Errorf("failed to read private key from %q: %s", privateKeyFile, err)
	}

	var keyDER []byte
	pemBlock, _ := pem.Decode(keyBytes)
	if pemBlock != nil {
		keyDER = pemBlock.Bytes
	} else {
		keyDER = keyBytes
	}

	var certKey crypto.PrivateKey
	if certKey, err = x509.ParsePKCS1PrivateKey(keyDER); err != nil {
		if certKey, err = x509.ParsePKCS8PrivateKey(keyDER); err != nil {
			return nil, fmt.Errorf("failed to parse private key from %q: %s", privateKeyFile, err)
		}
	}

	serverURL := "https://demo.acvts.nist.gov/"
	if len(config.ACVPServer) > 0 {
		serverURL = config.ACVPServer
	}
	server := acvp.NewServer(serverURL, config.LogFile, [][]byte{certDER}, certKey, func() string {
		return TOTP(totpSecret[:])
	})

	if len(sessionTokensCacheDir) > 0 {
		if err := loadCachedSessionTokens(server, sessionTokensCacheDir); err != nil {
			return nil, err
		}
	}

	return server, nil
}

func getResultsWithRetry(server *acvp.Server, url string) (bool, error) {
FetchResults:
	for {
		var results acvp.SessionResults
		if err := server.Get(&results, trimLeadingSlash(url)+"/results"); err != nil {
			return false, errors.New("failed to fetch session results: " + err.Error())
		}

		if results.Passed {
			log.Print("Test passed")
			return true, nil
		}

		for _, result := range results.Results {
			if result.Status == "incomplete" {
				log.Print("Server hasn't finished processing results. Waiting 10 seconds.")
				time.Sleep(10 * time.Second)
				continue FetchResults
			}
		}

		log.Printf("Server did not accept results: %#v", results)
		return false, nil
	}
}

// vectorSetHeader is the first element in the array of JSON elements that makes
// up the on-disk format for a vector set.
type vectorSetHeader struct {
	URL           string   `json:"url,omitempty"`
	VectorSetURLs []string `json:"vectorSetUrls,omitempty"`
	Time          string   `json:"time,omitempty"`
}

func uploadFromFile(file string, config *Config, sessionTokensCacheDir string) {
	if len(*jsonInputFile) > 0 {
		log.Fatalf("-upload cannot be used with -json")
	}
	if len(*runFlag) > 0 {
		log.Fatalf("-upload cannot be used with -run")
	}
	if len(*fetchFlag) > 0 {
		log.Fatalf("-upload cannot be used with -fetch")
	}
	if len(*expectedOutFlag) > 0 {
		log.Fatalf("-upload cannot be used with -expected-out")
	}
	if *dumpRegcap {
		log.Fatalf("-upload cannot be used with -regcap")
	}

	in, err := os.Open(file)
	if err != nil {
		log.Fatalf("Cannot open input: %s", err)
	}
	defer in.Close()

	decoder := json.NewDecoder(in)

	var input []json.RawMessage
	if err := decoder.Decode(&input); err != nil {
		log.Fatalf("Failed to parse input: %s", err)
	}

	if len(input) < 2 {
		log.Fatalf("Input JSON has fewer than two elements")
	}

	var header vectorSetHeader
	if err := json.Unmarshal(input[0], &header); err != nil {
		log.Fatalf("Failed to parse input header: %s", err)
	}

	if numGroups := len(input) - 1; numGroups != len(header.VectorSetURLs) {
		log.Fatalf("have %d URLs from header, but only %d result groups", len(header.VectorSetURLs), numGroups)
	}

	server, err := connect(config, sessionTokensCacheDir)
	if err != nil {
		log.Fatal(err)
	}

	for i, url := range header.VectorSetURLs {
		log.Printf("Uploading result for %q", url)
		if err := uploadResult(server, url, input[i+1]); err != nil {
			log.Fatalf("Failed to upload: %s", err)
		}
	}

	if ok, err := getResultsWithRetry(server, header.URL); err != nil {
		log.Fatal(err)
	} else if !ok {
		os.Exit(1)
	}
}

func main() {
	flag.Parse()

	middle, err := subprocess.New(*wrapperPath)
	if err != nil {
		log.Fatalf("failed to initialise middle: %s", err)
	}
	defer middle.Close()

	configBytes, err := middle.Config()
	if err != nil {
		log.Fatalf("failed to get config from middle: %s", err)
	}

	var supportedAlgos []map[string]any
	if err := json.Unmarshal(configBytes, &supportedAlgos); err != nil {
		log.Fatalf("failed to parse configuration from Middle: %s", err)
	}

	if *dumpRegcap {
		nonTestAlgos := make([]map[string]any, 0, len(supportedAlgos))
		for _, algo := range supportedAlgos {
			if value, ok := algo["acvptoolTestOnly"]; ok {
				testOnly, ok := value.(bool)
				if !ok {
					log.Fatalf("modulewrapper config contains acvptoolTestOnly field with non-boolean value %#v", value)
				}
				if testOnly {
					continue
				}
			}
			if value, ok := algo["algorithm"]; ok {
				algorithm, ok := value.(string)
				if ok && algorithm == "acvptool" {
					continue
				}
			}
			nonTestAlgos = append(nonTestAlgos, algo)
		}

		regcap := []map[string]any{
			{"acvVersion": "1.0"},
			{"algorithms": nonTestAlgos},
		}
		regcapBytes, err := json.MarshalIndent(regcap, "", "    ")
		if err != nil {
			log.Fatalf("failed to marshal regcap: %s", err)
		}
		os.Stdout.Write(regcapBytes)
		os.Stdout.WriteString("\n")
		return
	}

	if len(*jsonInputFile) > 0 {
		if err := processFile(*jsonInputFile, supportedAlgos, middle); err != nil {
			log.Fatalf("failed to process input file: %s", err)
		}
		return
	}

	var requestedAlgosFlag string
	// The output file to which expected results are written, if requested.
	var expectedOut *os.File
	// A tee that outputs to both stdout (for vectors) and the file for
	// expected results, if any.
	var fetchOutputTee io.Writer

	if len(*runFlag) > 0 && len(*fetchFlag) > 0 {
		log.Fatalf("cannot specify both -run and -fetch")
	}
	if len(*expectedOutFlag) > 0 && len(*fetchFlag) == 0 {
		log.Fatalf("-expected-out can only be used with -fetch")
	}
	if len(*runFlag) > 0 {
		requestedAlgosFlag = *runFlag
	} else {
		requestedAlgosFlag = *fetchFlag
		if len(*expectedOutFlag) > 0 {
			if expectedOut, err = os.Create(*expectedOutFlag); err != nil {
				log.Fatalf("cannot open %q: %s", *expectedOutFlag, err)
			}
			fetchOutputTee = io.MultiWriter(os.Stdout, expectedOut)
			defer expectedOut.Close()
		} else {
			fetchOutputTee = os.Stdout
		}
	}

	runAlgos := make(map[string]bool)
	if len(requestedAlgosFlag) > 0 {
		for _, substr := range strings.Split(requestedAlgosFlag, ",") {
			runAlgos[substr] = false
		}
	}

	var algorithms []map[string]any
	for _, supportedAlgo := range supportedAlgos {
		algoInterface, ok := supportedAlgo["algorithm"]
		if !ok {
			continue
		}

		algo, ok := algoInterface.(string)
		if !ok {
			continue
		}

		if _, ok := runAlgos[algo]; ok {
			algorithms = append(algorithms, supportedAlgo)
			runAlgos[algo] = true
		}
	}

	for algo, recognised := range runAlgos {
		if !recognised {
			log.Fatalf("requested algorithm %q was not recognised", algo)
		}
	}

	var config Config
	if err := jsonFromFile(&config, *configFilename); err != nil {
		log.Fatalf("Failed to load config file: %s", err)
	}

	var sessionTokensCacheDir string
	if len(config.SessionTokensCache) > 0 {
		sessionTokensCacheDir = config.SessionTokensCache
		if strings.HasPrefix(sessionTokensCacheDir, "~/") {
			home := os.Getenv("HOME")
			if len(home) == 0 {
				log.Fatal("~ used in config file but $HOME not set")
			}
			sessionTokensCacheDir = filepath.Join(home, sessionTokensCacheDir[2:])
		}
	}

	if len(*uploadInputFile) > 0 {
		uploadFromFile(*uploadInputFile, &config, sessionTokensCacheDir)
		return
	}

	server, err := connect(&config, sessionTokensCacheDir)
	if err != nil {
		log.Fatal(err)
	}

	if err := server.Login(); err != nil {
		log.Fatalf("failed to login: %s", err)
	}

	if len(requestedAlgosFlag) == 0 {
		if interactiveModeSupported {
			runInteractive(server, config)
		} else {
			log.Fatalf("no arguments given but interactive mode not supported")
		}
		return
	}

	requestBytes, err := json.Marshal(acvp.TestSession{
		IsSample:    true,
		Publishable: false,
		Algorithms:  algorithms,
	})
	if err != nil {
		log.Fatalf("Failed to serialise JSON: %s", err)
	}

	var result acvp.TestSession
	if err := server.Post(&result, "acvp/v1/testSessions", requestBytes); err != nil {
		log.Fatalf("Request to create test session failed: %s", err)
	}

	url := trimLeadingSlash(result.URL)
	log.Printf("Created test session %q", url)
	if token := result.AccessToken; len(token) > 0 {
		server.PrefixTokens[url] = token
		if len(sessionTokensCacheDir) > 0 {
			os.WriteFile(filepath.Join(sessionTokensCacheDir, neturl.PathEscape(url))+".token", []byte(token), 0600)
		}
	}

	log.Printf("Have vector sets %v", result.VectorSetURLs)

	if len(*fetchFlag) > 0 {
		io.WriteString(fetchOutputTee, "[\n")
		json.NewEncoder(fetchOutputTee).Encode(vectorSetHeader{
			URL:           url,
			VectorSetURLs: result.VectorSetURLs,
			Time:          time.Now().Format(time.RFC3339),
		})
	}

	for _, setURL := range result.VectorSetURLs {
		log.Printf("Fetching test vectors %q", setURL)

		vectors, vectorsBytes, err := getVectorsWithRetry(server, trimLeadingSlash(setURL))
		if err != nil {
			log.Fatalf("Failed to fetch vector set %q: %s", setURL, err)
		}

		if len(*fetchFlag) > 0 {
			os.Stdout.WriteString(",\n")
			os.Stdout.Write(vectorsBytes)
		}

		if expectedOut != nil {
			log.Printf("Fetching expected results")

			_, expectedResultsBytes, err := getVectorsWithRetry(server, trimLeadingSlash(setURL)+"/expected")
			if err != nil {
				log.Fatalf("Failed to fetch expected results: %s", err)
			}

			expectedOut.WriteString(",")
			expectedOut.Write(expectedResultsBytes)
		}

		if len(*fetchFlag) > 0 {
			continue
		}

		replyGroups, err := middle.Process(vectors.Algo, vectorsBytes)
		if err != nil {
			log.Printf("Failed: %s", err)
			log.Printf("Deleting test set")
			server.Delete(url)
			os.Exit(1)
		}

		headerBytes, err := json.Marshal(acvp.Vectors{
			ID:   vectors.ID,
			Algo: vectors.Algo,
		})
		if err != nil {
			log.Printf("Failed to marshal result: %s", err)
			log.Printf("Deleting test set")
			server.Delete(url)
			os.Exit(1)
		}

		var resultBuf bytes.Buffer
		resultBuf.Write(headerBytes[:len(headerBytes)-1])
		resultBuf.WriteString(`,"testGroups":`)
		replyBytes, err := json.Marshal(replyGroups)
		if err != nil {
			log.Printf("Failed to marshal result: %s", err)
			log.Printf("Deleting test set")
			server.Delete(url)
			os.Exit(1)
		}
		resultBuf.Write(replyBytes)
		resultBuf.WriteString("}")

		if err := uploadResult(server, setURL, resultBuf.Bytes()); err != nil {
			log.Printf("Deleting test set")
			server.Delete(url)
			log.Fatal(err)
		}
	}

	if len(*fetchFlag) > 0 {
		io.WriteString(fetchOutputTee, "]\n")
		return
	}

	if ok, err := getResultsWithRetry(server, url); err != nil {
		log.Fatal(err)
	} else if !ok {
		os.Exit(1)
	}
}
