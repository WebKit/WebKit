// Copyright (c) 2022, Google Inc.
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

package runner

import (
	"fmt"
	"os"
	"strconv"
)

const (
	shardStatusFileEnv = "TEST_SHARD_STATUS_FILE"
	shardTotalEnv      = "TEST_TOTAL_SHARDS"
	shardIndexEnv      = "TEST_SHARD_INDEX"
	shardPrefix        = "RUNNER_"
)

func init() {
	// When run under `go test`, init() functions may be run twice if the
	// test binary ends up forking and execing itself. Therefore we move
	// the environment variables to names that don't interfere with Go's
	// own support for sharding. If we recorded and erased them, then they
	// wouldn't exist the second time the binary runs.
	for _, key := range []string{shardStatusFileEnv, shardTotalEnv, shardIndexEnv} {
		value := os.Getenv(key)
		if len(value) > 0 {
			os.Setenv(shardPrefix+key, value)
			os.Setenv(key, "")
		}
	}
}

// getSharding returns the shard index and count, or zeros if sharding is not
// enabled.
func getSharding() (index, total int, err error) {
	statusFile := os.Getenv(shardPrefix + shardStatusFileEnv)
	totalNumStr := os.Getenv(shardPrefix + shardTotalEnv)
	indexStr := os.Getenv(shardPrefix + shardIndexEnv)
	if len(totalNumStr) == 0 || len(indexStr) == 0 {
		return 0, 0, nil
	}

	totalNum, err := strconv.Atoi(totalNumStr)
	if err != nil {
		return 0, 0, fmt.Errorf("$%s is %q, but expected a number\n", shardTotalEnv, totalNumStr)
	}

	index, err = strconv.Atoi(indexStr)
	if err != nil {
		return 0, 0, fmt.Errorf("$%s is %q, but expected a number\n", shardIndexEnv, indexStr)
	}

	if index < 0 || index >= totalNum {
		return 0, 0, fmt.Errorf("shard index/total of %d/%d is invalid\n", index, totalNum)
	}

	if len(statusFile) > 0 {
		if err := os.WriteFile(statusFile, nil, 0664); err != nil {
			return 0, 0, err
		}
	}

	return index, totalNum, nil
}
