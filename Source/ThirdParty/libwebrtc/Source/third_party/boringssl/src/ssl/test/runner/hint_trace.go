// Copyright 2026 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package runner

import (
	"bytes"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"sync"
	"time"

	"golang.org/x/crypto/cryptobyte"
)

// hintTracesSubdir is the subdirectory, under the directory named by
// -hint-traces-dir, where hint traces are read and written.
const hintTracesSubdir = "hint-traces"

// hintTracesFile is the name of the file, within hintTracesSubdir, containing
// the serialized hint traces.
const hintTracesFile = "hint_traces.json"

// hintTrace records the inputs and outputs of a handshake hints test run.
type hintTrace struct {
	Connections []connectionTrace `json:"connections"`
	ConfigFlags []string          `json:"config_flags"`
	Date        string            `json:"date"`
}

// connectionTrace is the per-connection data to record in traces for handshake
// hints tests.
type connectionTrace struct {
	ClientHello   []byte `json:"client_hello"`
	HandshakeHint []byte `json:"handshake_hint"`
}

// A hintTraceRecorder collects hint traces from -Hints tests and serializes
// them to a directory. Its methods may be called concurrently from multiple
// worker goroutines.
type hintTraceRecorder struct {
	// dir is the directory in which to write the trace files.
	dir    string
	mu     sync.Mutex
	traces map[string]*hintTrace
}

// newHintTraceRecorder returns a new, empty recorder which reads and writes
// traces under dir.
func newHintTraceRecorder(dir string) *hintTraceRecorder {
	return &hintTraceRecorder{
		dir:    filepath.Join(dir, hintTracesSubdir),
		traces: make(map[string]*hintTrace),
	}
}

// filePrefix returns the prefix of the paths of the per-connection files
// written by the shim for testBaseName.
func (r *hintTraceRecorder) filePrefix(testBaseName string) string {
	return filepath.Join(r.dir, testBaseName)
}

// prepare creates the trace directory and returns the prefix to pass to the
// shim's -write-hint-trace flag for testBaseName.
func (r *hintTraceRecorder) prepare(testBaseName string) (string, error) {
	if err := os.MkdirAll(r.dir, 0755); err != nil {
		return "", err
	}
	return r.filePrefix(testBaseName), nil
}

// cleanup removes the per-connection files the shim wrote for testBaseName.
func (r *hintTraceRecorder) cleanup(testBaseName string, numConnections int) {
	prefix := r.filePrefix(testBaseName)
	for i := 0; i < numConnections; i++ {
		os.Remove(prefix + strconv.Itoa(i))
	}
}

// readConnections reads the per-connection files the shim wrote for
// testBaseName. Connections whose files are missing are skipped.
func (r *hintTraceRecorder) readConnections(testBaseName string, numConnections int) ([]connectionTrace, error) {
	prefix := r.filePrefix(testBaseName)
	var connections []connectionTrace
	for i := 0; i < numConnections; i++ {
		connTrace, err := readConnectionTrace(prefix, i)
		if err != nil {
			return nil, err
		}
		if connTrace != nil {
			connections = append(connections, *connTrace)
		}
	}
	return connections, nil
}

// readConnectionTrace reads the raw data for a single connection, written as a
// binary file by the shim. It returns nil if the file does not exist.
func readConnectionTrace(hintPathPrefix string, connIndex int) (*connectionTrace, error) {
	tracePath := hintPathPrefix + strconv.Itoa(connIndex)
	traceData, err := os.ReadFile(tracePath)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil
		}
		return nil, fmt.Errorf("reading %s: %w", tracePath, err)
	}
	data := cryptobyte.String(traceData)
	var clientHello, hint []byte
	if !readUint24LengthPrefixedBytes(&data, &clientHello) || len(clientHello) == 0 {
		return nil, fmt.Errorf("reading ClientHello for %s", tracePath)
	}
	if !readUint24LengthPrefixedBytes(&data, &hint) || len(hint) == 0 {
		return nil, fmt.Errorf("reading hint for %s", tracePath)
	}
	if !data.Empty() {
		return nil, fmt.Errorf("trailing data found in %s", tracePath)
	}
	return &connectionTrace{
		ClientHello:   clientHello,
		HandshakeHint: hint,
	}, nil
}

// record updates the hint trace entry for a -Hints test.
func (r *hintTraceRecorder) record(testBaseName string, configFlags []string, connections []connectionTrace) {
	trace := &hintTrace{
		Connections: connections,
		ConfigFlags: configFlags,
		Date:        time.Now().Format("20060102"), // YYYYMMDD
	}

	r.mu.Lock()
	defer r.mu.Unlock()
	r.traces[testBaseName] = trace
}

// numTraces returns the number of traces collected so far.
func (r *hintTraceRecorder) numTraces() int {
	r.mu.Lock()
	defer r.mu.Unlock()
	return len(r.traces)
}

// writeTraces writes all collected hint traces to a JSON file in the trace
// directory.
func (r *hintTraceRecorder) writeTraces() error {
	if err := os.MkdirAll(r.dir, 0755); err != nil {
		return err
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	var buf bytes.Buffer
	enc := json.NewEncoder(&buf)
	enc.SetEscapeHTML(false)
	enc.SetIndent("", "  ")
	if err := enc.Encode(r.traces); err != nil {
		return err
	}
	return os.WriteFile(filepath.Join(r.dir, hintTracesFile), buf.Bytes(), 0644)
}
