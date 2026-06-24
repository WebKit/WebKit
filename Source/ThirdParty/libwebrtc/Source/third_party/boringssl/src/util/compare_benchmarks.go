// Copyright (c) 2020 The BoringSSL Authors
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

//go:build ignore

// compare_benchmarks takes the JSON-formatted output of bssl speed and
// compares it against a baseline output.
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
	"time"
)

var baseline = flag.String("baseline", "", "the path to the JSON file containing the base results")

type Result struct {
	Name           string  `json:"name"`
	Iterations     int     `json:"iterations"`
	CPUTime        float64 `json:"cpu_time"`
	TimeUnit       string  `json:"time_unit"`
	BytesPerSecond float64 `json:"bytes_per_second"`
}

type googlebenchmarks struct {
	Benchmarks []Result `json:"benchmarks"`
}

func (r *Result) Speed() (float64, string) {
	if r.BytesPerSecond != 0 {
		return r.BytesPerSecond / 1000000, "MB/sec"
	}
	var unit time.Duration
	switch r.TimeUnit {
	case "ns":
		unit = time.Nanosecond
	case "us":
		unit = time.Microsecond
	case "ms":
		unit = time.Millisecond
	default:
		log.Panicf("unsupported time unit: %q", r.TimeUnit)
	}
	return float64(unit) / r.CPUTime, "ops/sec"
}

func printResult(result Result, baseline *Result) error {
	if baseline != nil {
		if result.Name != baseline.Name {
			return fmt.Errorf("result did not match baseline: %q vs %q", result.Name, baseline.Name)
		}
	}

	newSpeed, unit := result.Speed()
	fmt.Printf("Did %d %s operations (%.1f %s)", result.Iterations, result.Name, newSpeed, unit)
	if baseline != nil {
		oldSpeed, _ := baseline.Speed()
		fmt.Printf(" [%+.1f%%]", (newSpeed-oldSpeed)/oldSpeed*100)
	}
	fmt.Printf("\n")
	return nil
}

func readResults(path string) ([]Result, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	var ret googlebenchmarks
	if err := json.Unmarshal(data, &ret); err != nil {
		return nil, err
	}
	return ret.Benchmarks, nil
}

func main() {
	flag.Parse()

	baselineResults, err := readResults(*baseline)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error reading %q: %s\n", *baseline, err)
		os.Exit(1)
	}

	fmt.Println(*baseline)
	for _, result := range baselineResults {
		if err := printResult(result, nil); err != nil {
			fmt.Fprintf(os.Stderr, "Error in %q: %s\n", *baseline, err)
			os.Exit(1)
		}
	}

	for _, arg := range flag.Args() {
		results, err := readResults(arg)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error reading %q: %s\n", arg, err)
			os.Exit(1)
		}

		if len(results) != len(baselineResults) {
			fmt.Fprintf(os.Stderr, "Result files %q and %q have different lengths\n", arg, *baseline)
			os.Exit(1)
		}

		fmt.Printf("\n%s\n", arg)
		for i, result := range results {
			if err := printResult(result, &baselineResults[i]); err != nil {
				fmt.Fprintf(os.Stderr, "Error in %q: %s\n", arg, err)
				os.Exit(1)
			}
		}
	}
}
