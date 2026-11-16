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

// update_bazel_deps updates dependencies in MODULE.bazel files to their latest
// stable versions from the Bazel Central Registry (BCR).
package main

import (
	"bytes"
	"cmp"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
)

// Version represents a parsed non-prerelease Bazel module version. See
// https://bazel.build/external/module#version_format.
type Version struct {
	components []string
	raw        string
}

var bazelVersionRE = regexp.MustCompile(`^([a-zA-Z0-9.]+)(-[a-zA-Z0-9.-]+)?(\+[a-zA-Z0-9.-]+)?$`)

var errPrerelease = errors.New("prerelease versions are not supported")

func parseVersion(v string) (Version, error) {
	matches := bazelVersionRE.FindStringSubmatch(v)
	if matches == nil {
		return Version{}, fmt.Errorf("invalid version %q", v)
	}
	if matches[2] != "" {
		return Version{}, fmt.Errorf("invalid version %q: %w", v, errPrerelease)
	}
	return Version{
		components: strings.Split(matches[1], "."),
		raw:        v,
	}, nil
}

func compareComponents(c1, c2 string) int {
	// Numeric identifiers are compared numerically.
	n1, err1 := strconv.Atoi(c1)
	n2, err2 := strconv.Atoi(c2)
	if err1 == nil && err2 == nil {
		return cmp.Compare(n1, n2)
	}

	// Numeric has lower precedence than non-numeric.
	if err1 == nil {
		return -1
	}
	if err2 == nil {
		return 1
	}

	// Non-numeric identifiers are compared lexically.
	return cmp.Compare(c1, c2)
}

func compareVersions(v1, v2 Version) int {
	minLen := min(len(v1.components), len(v2.components))
	for i := 0; i < minLen; i++ {
		if ret := compareComponents(v1.components[i], v2.components[i]); ret != 0 {
			return ret
		}
	}
	// If one version is a prefix of the other, the shorter compares first.
	return cmp.Compare(len(v1.components), len(v2.components))
}

type moduleMetadata struct {
	Versions       []string          `json:"versions"`
	YankedVersions map[string]string `json:"yanked_versions"`
}

func getLatestVersion(dep string) (Version, error) {
	url := fmt.Sprintf("https://bcr.bazel.build/modules/%s/metadata.json", dep)
	resp, err := http.Get(url)
	if err != nil {
		return Version{}, err
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		return Version{}, fmt.Errorf("bad status %d fetching metadata for %s", resp.StatusCode, dep)
	}
	var meta moduleMetadata
	if err := json.NewDecoder(resp.Body).Decode(&meta); err != nil {
		return Version{}, err
	}

	var latest Version
	var found bool
	for _, vStr := range meta.Versions {
		if _, yanked := meta.YankedVersions[vStr]; yanked {
			continue
		}
		v, err := parseVersion(vStr)
		if err != nil {
			if errors.Is(err, errPrerelease) {
				// Ignore prerelease versions.
				continue
			}
			return Version{}, err
		}
		if !found || compareVersions(v, latest) > 0 {
			latest = v
			found = true
		}
	}
	if !found {
		return Version{}, fmt.Errorf("no suitable version found for %s", dep)
	}
	return latest, nil
}

// Group 1: prefix up to name="
// Group 2: dep name
// Group 3: middle part between name and version
// Group 4: current version
// Group 5: suffix
var bazelDepRE = regexp.MustCompile(`^(\s*bazel_dep\(\s*name\s*=\s*")([^"]+)("\s*,\s*version\s*=\s*")([^"]+)("\s*.*)$`)

func updateFile(path string) (changed bool, err error) {
	content, err := os.ReadFile(path)
	if err != nil {
		return false, err
	}

	lines := bytes.Split(content, []byte("\n"))
	// Account for the trailing newline.
	if len(lines) > 0 && len(lines[len(lines)-1]) == 0 {
		lines = lines[:len(lines)-1]
	}

	var out bytes.Buffer
	for _, line := range lines {
		var lineChanged bool
		matches := bazelDepRE.FindSubmatch(line)
		if matches != nil {
			depName := string(matches[2])
			currentVerStr := string(matches[4])

			currentVer, err := parseVersion(currentVerStr)
			if err != nil {
				return false, fmt.Errorf("could not parse current version for %s: %s", depName, err)
			}

			latestVer, err := getLatestVersion(depName)
			if err != nil {
				return false, fmt.Errorf("could not get latest version for %s: %s", depName, err)
			}

			if compareVersions(latestVer, currentVer) > 0 {
				out.Write(matches[1])
				out.Write(matches[2])
				out.Write(matches[3])
				out.WriteString(latestVer.raw)
				out.Write(matches[5])
				out.WriteByte('\n')

				fmt.Printf("%s: Updating %s: %s -> %s\n", path, depName, currentVerStr, latestVer.raw)
				lineChanged = true
				changed = true
			}
		}

		if !lineChanged {
			out.Write(line)
			out.WriteByte('\n')
		}
	}

	if changed {
		if err := os.WriteFile(path, out.Bytes(), 0666); err != nil {
			return false, err
		}
		return true, nil
	}

	fmt.Printf("%s: All dependencies up to date.\n", path)
	return false, nil
}

func updateLockfile(dir string) error {
	cmd := exec.Command("bazelisk", "mod", "deps", "--lockfile_mode=refresh")
	cmd.Dir = dir
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	fmt.Printf("Running bazelisk mod deps --lockfile_mode=refresh in %s...\n", dir)
	return cmd.Run()
}

func main() {
	flag.Parse()

	files := flag.Args()
	if len(files) == 0 {
		files = []string{"MODULE.bazel", "util/bazel-example/MODULE.bazel"}
	}

	var anyChanged bool
	for _, file := range files {
		changed, err := updateFile(file)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error updating %s: %s\n", file, err)
			os.Exit(1)
		}
		anyChanged = anyChanged || changed
	}

	// If any dependencies changed, update all lock files. The lockfiles for
	// the example project depend on the root project.
	if anyChanged {
		for _, file := range files {
			dir := filepath.Dir(file)
			if err := updateLockfile(dir); err != nil {
				fmt.Fprintf(os.Stderr, "Error updating lockfile in %s: %s\n", dir, err)
				os.Exit(1)
			}
		}
	}
}
