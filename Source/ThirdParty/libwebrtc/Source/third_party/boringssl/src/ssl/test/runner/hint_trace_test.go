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
	"os"
	"path/filepath"
	"slices"
	"strconv"
	"testing"

	"golang.org/x/crypto/cryptobyte"
)

// writeConnectionTrace writes a file in the format the shim writes, so that
// the recorder can read it back.
func writeConnectionTrace(t *testing.T, recorder *hintTraceRecorder, testBaseName string, connIndex int, clientHello, hint []byte) {
	t.Helper()
	if _, err := recorder.prepare(testBaseName); err != nil {
		t.Fatalf("failed to create hint trace directory: %v", err)
	}
	trace := cryptobyte.NewBuilder(nil)
	addUint24LengthPrefixedBytes(trace, clientHello)
	addUint24LengthPrefixedBytes(trace, hint)
	path := recorder.filePrefix(testBaseName) + strconv.Itoa(connIndex)
	if err := os.WriteFile(path, trace.BytesOrPanic(), 0644); err != nil {
		t.Fatalf("failed to write %s: %v", path, err)
	}
}

func TestReadConnections(t *testing.T) {
	recorder := newHintTraceRecorder(t.TempDir())

	// Simulate the files written by the C++ shim to transmit the hints and
	// ClientHellos.
	clientHellos := [][]byte{
		{0x03, 0x03, 0x01, 0x02, 0x03},
		{0x03, 0x03, 0x04, 0x05, 0x06},
	}
	hints := [][]byte{
		{0xaa, 0xbb, 0xcc},
		{0xdd, 0xee, 0xff},
	}
	for i := range clientHellos {
		writeConnectionTrace(t, recorder, "TestA", i, clientHellos[i], hints[i])
	}

	connections, err := recorder.readConnections("TestA", len(clientHellos))
	if err != nil {
		t.Fatalf("readConnections failed: %v", err)
	}
	if len(connections) != len(clientHellos) {
		t.Fatalf("connections length mismatch: got %d, want %d", len(connections), len(clientHellos))
	}
	for i, conn := range connections {
		if !bytes.Equal(conn.ClientHello, clientHellos[i]) {
			t.Errorf("connection %d ClientHello mismatch: got %x, want %x", i, conn.ClientHello, clientHellos[i])
		}
		if !bytes.Equal(conn.HandshakeHint, hints[i]) {
			t.Errorf("connection %d hint mismatch: got %x, want %x", i, conn.HandshakeHint, hints[i])
		}
	}
}

// TestReadConnectionsMissing tests that connections the shim did not write are
// skipped.
func TestReadConnectionsMissing(t *testing.T) {
	recorder := newHintTraceRecorder(t.TempDir())
	writeConnectionTrace(t, recorder, "TestA", 0, []byte{1, 2, 3}, []byte{4, 5, 6})

	connections, err := recorder.readConnections("TestA", 2)
	if err != nil {
		t.Fatalf("readConnections failed: %v", err)
	}
	if len(connections) != 1 {
		t.Fatalf("connections length mismatch: got %d, want 1", len(connections))
	}
}

func TestCleanup(t *testing.T) {
	recorder := newHintTraceRecorder(t.TempDir())
	writeConnectionTrace(t, recorder, "TestA", 0, []byte{1, 2, 3}, []byte{4, 5, 6})

	recorder.cleanup("TestA", 1)

	path := recorder.filePrefix("TestA") + "0"
	if _, err := os.Stat(path); !os.IsNotExist(err) {
		t.Errorf("%s was not removed: %v", path, err)
	}
}

func TestWriteTraces(t *testing.T) {
	testDir := t.TempDir()
	recorder := newHintTraceRecorder(testDir)

	// Record some traces.
	recorder.record("TestA", []string{"-flag1", "val1"}, []connectionTrace{
		{
			ClientHello:   []byte{1, 2, 3},
			HandshakeHint: []byte{4, 5, 6},
		},
	})
	recorder.record("TestB", []string{"-flag2"}, []connectionTrace{
		{
			ClientHello:   []byte{7, 8},
			HandshakeHint: []byte{9, 10},
		},
		{
			ClientHello:   []byte{11, 12},
			HandshakeHint: []byte{13, 14},
		},
	})

	if got := recorder.numTraces(); got != 2 {
		t.Errorf("numTraces mismatch: got %d, want 2", got)
	}

	// Write them.
	if err := recorder.writeTraces(); err != nil {
		t.Fatalf("writeTraces failed: %v", err)
	}

	// Load them back.
	path := filepath.Join(testDir, hintTracesSubdir, hintTracesFile)
	data, err := os.ReadFile(path)
	if err != nil {
		t.Errorf("failed to read hint traces from %s: %v", path, err)
	}
	var traces map[string]*hintTrace
	if err := json.Unmarshal(data, &traces); err != nil {
		t.Errorf("failed to parse hint traces from %s: %v", path, err)
	}

	if len(traces) != 2 {
		t.Fatalf("expected 2 traces, got %d", len(traces))
	}

	// Verify TestA.
	traceA, ok := traces["TestA"]
	if !ok {
		t.Fatal("missing trace for TestA")
	}
	if len(traceA.Connections) != 1 {
		t.Fatalf("TestA Connections length mismatch: got %d, want 1", len(traceA.Connections))
	}
	if !bytes.Equal(traceA.Connections[0].ClientHello, []byte{1, 2, 3}) {
		t.Errorf("TestA ClientHello mismatch")
	}
	if !bytes.Equal(traceA.Connections[0].HandshakeHint, []byte{4, 5, 6}) {
		t.Errorf("TestA HandshakeHint mismatch")
	}
	if !slices.Equal(traceA.ConfigFlags, []string{"-flag1", "val1"}) {
		t.Errorf("TestA ConfigFlags mismatch: %v", traceA.ConfigFlags)
	}
	if traceA.Date == "" {
		t.Error("TestA Date is empty")
	}

	// Verify TestB (multi-connection / resumption).
	traceB, ok := traces["TestB"]
	if !ok {
		t.Fatal("missing trace for TestB")
	}
	if len(traceB.Connections) != 2 {
		t.Fatalf("TestB Connections length mismatch: got %d, want 2", len(traceB.Connections))
	}
	if !bytes.Equal(traceB.Connections[0].ClientHello, []byte{7, 8}) {
		t.Errorf("TestB Connection 0 ClientHello mismatch")
	}
	if !bytes.Equal(traceB.Connections[1].HandshakeHint, []byte{13, 14}) {
		t.Errorf("TestB Connection 1 HandshakeHint mismatch")
	}
}
