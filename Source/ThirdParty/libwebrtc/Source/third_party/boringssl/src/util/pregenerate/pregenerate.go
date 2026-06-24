// Copyright 2024 The BoringSSL Authors
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

// pregenerate manages generated files in BoringSSL
package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"maps"
	"os"
	"path/filepath"
	"runtime"
	"slices"
	"strings"
	"sync"
	"sync/atomic"

	"github.com/hexops/gotextdiff"
	"github.com/hexops/gotextdiff/myers"
	"github.com/hexops/gotextdiff/span"

	"boringssl.googlesource.com/boringssl.git/util/build"
)

var (
	check      = flag.Bool("check", false, "Check whether any files need to be updated, without actually updating them")
	numWorkers = flag.Int("num-workers", runtime.NumCPU(), "Runs the given number of workers")
	dryRun     = flag.Bool("dry-run", false, "Skip actually writing any files")
	perlPath   = flag.String("perl", "perl", "Path to the perl command")
	clangPath  = flag.String("clang", "clang", "Path to the clang command")
	list       = flag.Bool("list", false, "List all generated files, rather than actually run them")
	v          = flag.Bool("v", false, "Enable verbose logging")
)

// logV is a discarding logger for use by verbose logging if -v is not set.
var logV = log.New(io.MultiWriter( /* write nowhere */ ), "", log.Flags())

type gotextdiffHandleWrapper struct {
	io.Writer
	fmt.State // Usually left as nil as gotextdiff doesn't use it.
}

func (w gotextdiffHandleWrapper) Write(p []byte) (n int, err error) {
	return w.Writer.Write(p)
}

var generated atomic.Int32

func runTask(t *Task) error {
	expected, err := t.Run()
	if err != nil {
		return err
	}

	generated.Add(1)

	dst := t.Destination
	dstPath := filepath.FromSlash(dst)
	if *check {
		actual, err := os.ReadFile(dstPath)
		if err != nil {
			if os.IsNotExist(err) {
				err = errors.New("missing file")
			}
			return err
		}

		if !bytes.Equal(expected, actual) {
			uri := span.URIFromPath(dstPath)
			// Diff is from actual (i.e. what's in the repo) to expected (i.e. what should be in the repo).
			edits := myers.ComputeEdits(uri, string(actual), string(expected))
			unified := gotextdiff.ToUnified(dstPath, dstPath, string(actual), edits)
			unified.Format(gotextdiffHandleWrapper{Writer: os.Stderr}, 's')
			return errors.New("file out of date")
		}
		return nil
	}

	if *dryRun {
		fmt.Printf("Would write %d bytes to %q\n", len(expected), dst)
		return nil
	}

	if err := os.MkdirAll(filepath.Dir(dstPath), 0777); err != nil {
		return err
	}
	return os.WriteFile(dstPath, expected, 0666)
}

type taskResult struct {
	kind, dst string
	err       error
}

func worker(taskChan <-chan *Task, resultsChan chan<- taskResult, wg *sync.WaitGroup) {
	defer wg.Done()
	for t := range taskChan {
		err := runTask(t)
		resultsChan <- taskResult{kind: t.Kind, dst: t.Destination, err: err}
	}
}

func run() error {
	log.SetFlags(0) // Remove date/time output. This tool is rather fast and shouldn't need it.
	if *v {
		logV = log.Default()
	}
	if *clangPath == "" {
		log.Printf("Use of Clang has been disabled via -clang= flag. Not generating symbol lists.")
	}
	if *perlPath == "" {
		log.Printf("Use of Perl has been disabled via -perl= flag. Not generating assembly files.")
	}

	if _, err := os.Stat("BUILDING.md"); err != nil {
		return fmt.Errorf("must be run from BoringSSL source root")
	}

	buildJSON, err := os.ReadFile("build.json")
	if err != nil {
		return err
	}

	// Remove comments. For now, just do a very basic preprocessing step. If
	// needed, we can switch to something well-defined like one of the many
	// dozen different extended JSONs like JSON5.
	lines := bytes.Split(buildJSON, []byte("\n"))
	for i := range lines {
		if idx := bytes.Index(lines[i], []byte("//")); idx >= 0 {
			lines[i] = lines[i][:idx]
		}
	}
	buildJSON = bytes.Join(lines, []byte("\n"))

	var targetsIn map[string]InputTarget
	if err := json.Unmarshal(buildJSON, &targetsIn); err != nil {
		return fmt.Errorf("error decoding build config: %s", err)
	}

	var tasks []*Task
	var perlAsmTasks []*Task
	var allAsmSrcs []string
	targetsOut := make(map[string]build.Target)
	for name, targetIn := range targetsIn {
		targetOut, targetTasks, targetAsmSrcs, err := targetIn.Pregenerate(name)
		if err != nil {
			return err
		}
		targetsOut[name] = targetOut
		tasks = append(tasks, targetTasks...)
		for _, task := range targetTasks {
			if !slices.Contains(targetAsmSrcs, task.Destination) {
				continue
			}
			perlAsmTasks = append(perlAsmTasks, task)
		}
		allAsmSrcs = append(allAsmSrcs, targetAsmSrcs...)
	}

	tasks = append(tasks, MakePrefixingIncludes(targetsIn, targetsOut)...)
	tasks = append(tasks, MakeCollectAsmGlobalTasks(perlAsmTasks, allAsmSrcs, targetsOut)...)
	tasks = append(tasks, MakeBuildFiles(targetsOut)...)
	tasks = append(tasks, NewSimpleTask("README", "gen/README.md", func() ([]byte, error) {
		return []byte(readme), nil
	}))

	// Filter tasks by command-line argument.
	if args := flag.Args(); len(args) != 0 {
		for _, t := range tasks {
			dst := t.Destination
			matched := false
			for _, arg := range args {
				if strings.Contains(dst, arg) {
					matched = true
					break
				}
			}
			if !matched {
				t.Close(fmt.Errorf("%w: not included by filter", TaskSkipped))
			}
		}
	}

	if *list {
		paths := make([]string, len(tasks))
		for i, t := range tasks {
			paths[i] = t.Destination
		}
		slices.Sort(paths)
		for _, p := range paths {
			fmt.Println(p)
		}
		return nil
	}

	// Schedule tasks in parallel. Perlasm benefits from running in parallel. The
	// others likely do not, but it is simpler to parallelize them all.
	var wg sync.WaitGroup
	taskChan := make(chan *Task, *numWorkers)
	resultsChan := make(chan taskResult, *numWorkers)
	for i := 0; i < *numWorkers; i++ {
		wg.Add(1)
		go worker(taskChan, resultsChan, &wg)
	}

	go func() {
		for _, t := range tasks {
			taskChan <- t
		}
		close(taskChan)
		wg.Wait()
		close(resultsChan)
	}()

	failed := false
	skipped := map[string]int{}
	succeeded := map[string]int{}
	for r := range resultsChan {
		if r.err != nil {
			if errors.Is(r.err, TaskSkipped) {
				logV.Printf("task %q skipped - carrying on with previously saved data: %v", r.dst, r.err)
				skipped[r.kind]++
				continue
			}
			log.Printf("Error in file %q: %s", r.dst, r.err)
			failed = true
			continue
		}
		succeeded[r.kind]++
	}
	printLog := func(intro string, data map[string]int) {
		if len(data) == 0 {
			return
		}
		log.Printf(intro)
		for _, key := range slices.Sorted(maps.Keys(data)) {
			log.Printf("- %s: %d", key, data[key])
		}
	}
	if failed {
		return errors.New("some files had errors")
	}
	if len(succeeded) == 0 {
		return errors.New("everything was filtered out")
	}
	printLog("The following tasks have been skipped:", skipped)
	printLog("The following tasks have succeeded:", succeeded)
	return nil
}

func main() {
	flag.Parse()
	if err := run(); err != nil {
		log.Printf("Error: %s", err)
		os.Exit(1)
	}
}

const readme = `# Pre-generated files

This directory contains a number of pre-generated build artifacts. To simplify
downstream builds, they are checked into the repository, rather than dynamically
generated as part of the build.

When developing on BoringSSL, if any inputs to these files are modified, callers
must run the following command to update the generated files:

    go run ./util/pregenerate

To check that files are up-to-date without updating files, run:

    go run ./util/pregenerate -check

This is run on CI to ensure the generated files remain up-to-date.

To speed up local iteration, the tool accepts additional arguments to filter the
files generated. For example, if editing ` + "`aesni-x86_64.pl`" + `, this
command will only update files with "aesni-x86_64" as a substring.

    go run ./util/pregenerate aesni-x86_64

For convenience, all files in this directory, including this README, are managed
by the tool. This means the whole directory may be deleted and regenerated from
scratch at any time.
`
