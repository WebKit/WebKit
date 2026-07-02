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

func addServerPaddingTests() {
	// Base server padding test: shim request padding, runner sends padding.
	testCases = append(testCases, testCase{
		name:     "ServerPadding-ExpectPadding",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendServerPaddingLength: ptrTo(uint16(10)),
			},
		},
		flags: []string{
			"-request-server-padding", "10",
			"-expect-server-sent-requested-padding",
		},
	})
	// Test that padding of 0 works.
	testCases = append(testCases, testCase{
		name:     "ServerPadding-ZeroPaddingRequested",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendServerPaddingLength: ptrTo(uint16(0)),
			},
		},
		flags: []string{
			"-request-server-padding", "0",
			"-expect-server-sent-requested-padding",
		},
	})
	// Test that padding of 16k works.
	testCases = append(testCases, testCase{
		name:     "ServerPadding-16kPadding",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendServerPaddingLength: ptrTo(uint16(16384)),
			},
		},
		flags: []string{
			"-request-server-padding", "16384",
			"-expect-server-sent-requested-padding",
		},
	})
	// Server sends less than requested padding; should fail the
	// connection.
	testCases = append(testCases, testCase{
		name:     "ServerPadding-NotEnoughPadding",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendServerPaddingLength: ptrTo(uint16(5)),
			},
		},
		flags: []string{
			"-request-server-padding", "10",
		},
		shouldFail:         true,
		expectedError:      ":DECODE_ERROR:",
		expectedLocalError: "remote error: error decoding message",
	})
	// Server sends more than requested padding; should fail the
	// connection.
	testCases = append(testCases, testCase{
		name:     "ServerPadding-TooMuchPadding",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendServerPaddingLength: ptrTo(uint16(50)),
			},
		},
		flags: []string{
			"-request-server-padding", "10",
		},
		shouldFail:         true,
		expectedError:      ":DECODE_ERROR:",
		expectedLocalError: "remote error: error decoding message",
	})
	// Test server not sending padding in response to a padding request; should
	// not fail connection but also should not be counted as server sent correct
	// amount of padding.
	testCases = append(testCases, testCase{
		name:     "ServerPadding-NoPaddingSent",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
		},
		flags: []string{
			"-request-server-padding", "10",
		},
	})
	// TLS 1.2 connection should send no padding back without error.
	testCases = append(testCases, testCase{
		name:     "ServerPadding-TLS12-NoPadding",
		testType: clientTest,
		config: Config{
			MaxVersion: VersionTLS12,
		},
		flags: []string{
			"-request-server-padding", "10",
		},
	})
	// TLS 1.2 connection sending padding should result in a failed connection.
	testCases = append(testCases, testCase{
		name:     "ServerPadding-TLS12-PaddingSent",
		testType: clientTest,
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				SendServerPaddingLength: ptrTo(uint16(10)),
			},
		},
		flags: []string{
			"-request-server-padding", "10",
		},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_EXTENSION:",
		expectedLocalError: "remote error: unsupported extension",
	})

	// Base test; runner doesn't ask for padding, shim doesn't send padding.
	testCases = append(testCases, testCase{
		name:     "ServerPaddingServer-NotRequested",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS13,
		},
	})
	// Shim doesn't support padding.
	testCases = append(testCases, testCase{
		name:     "ServerPaddingServer-NoServerSupport",
		testType: serverTest,
		config: Config{
			MinVersion:           VersionTLS13,
			RequestServerPadding: ptrTo(uint16(10)),
		},
	})
	// Runner requests padding, shim sends padding.
	testCases = append(testCases, testCase{
		name:     "ServerPaddingServer-BasicPadding",
		testType: serverTest,
		config: Config{
			MinVersion:           VersionTLS13,
			RequestServerPadding: ptrTo(uint16(10)),
			Bugs: ProtocolBugs{
				ExpectedServerPadding: true,
			},
		},
		flags: []string{
			"-server-supports-padding",
		},
	})
	testCases = append(testCases, testCase{
		name:     "ServerPaddingServer-ZeroPadding",
		testType: serverTest,
		config: Config{
			MinVersion:           VersionTLS13,
			RequestServerPadding: ptrTo(uint16(0)),
			Bugs: ProtocolBugs{
				ExpectedServerPadding: true,
			},
		},
		flags: []string{
			"-server-supports-padding",
		},
	})
	testCases = append(testCases, testCase{
		name:     "ServerPaddingServer-16kPadding",
		testType: serverTest,
		config: Config{
			MinVersion:           VersionTLS13,
			RequestServerPadding: ptrTo(uint16(16384)),
			Bugs: ProtocolBugs{
				ExpectedServerPadding: true,
			},
		},
		flags: []string{
			"-server-supports-padding",
		},
	})
	// Runner request over the max amount of padding, server responds without
	// padding extension.
	testCases = append(testCases, testCase{
		name:     "ServerPaddingServer-OverMax",
		testType: serverTest,
		config: Config{
			MinVersion:           VersionTLS13,
			RequestServerPadding: ptrTo(uint16(16385)),
		},
		flags: []string{
			"-server-supports-padding",
		},
	})
	// Runner requests padding but with TLS 1.2 connection. Connection should
	// not fail, but no padding extension should be expected.
	testCases = append(testCases, testCase{
		name:     "ServerPaddingServer-TLS12",
		testType: serverTest,
		config: Config{
			MaxVersion:           VersionTLS12,
			RequestServerPadding: ptrTo(uint16(1024)),
		},
		flags: []string{
			"-server-supports-padding",
		},
	})

	// Runner requests padding, and then does a resumption handshake. Shim should
	// send server padding on initial handshake, but not resumption handshake.
	testCases = append(testCases, testCase{
		name:          "ServerPaddingServer-ResumeSession",
		testType:      serverTest,
		resumeSession: true,
		config: Config{
			MinVersion:           VersionTLS13,
			RequestServerPadding: ptrTo(uint16(10)),
			Bugs: ProtocolBugs{
				ExpectedServerPadding: true,
			},
		},
		flags: []string{
			"-server-supports-padding",
		},
	})
}
