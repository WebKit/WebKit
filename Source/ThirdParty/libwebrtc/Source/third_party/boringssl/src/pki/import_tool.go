// import_tool is a quick tool for importing Chromium's certificate verifier
// code into google3. In time it might be replaced by Copybara, but this is a
// lighter-weight solution while we're quickly iterating and only going in one
// direction.
//
// Usage: ./import_tool -spec import_spec.json\
//            -source-base ~/src/chromium/src/net\
//            -dest-base .
package main

import (
	"bufio"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"sync"
	"sync/atomic"
)

type specification struct {
	Replacements []replacement `json:"replacements"`
	Files        []string      `json:"files"`
}

type replacement struct {
	Match   string         `json:"match"`
	matchRE *regexp.Regexp `json:"-"`
	Replace string         `json:"replace"`
	Include string         `json:"include"`
	Using   []string       `json:"using"`
	used    uint32
}

var (
	specFile   *string = flag.String("spec", "", "Location of spec JSON")
	sourceBase *string = flag.String("source-base", "", "Path of the source files")
	destBase   *string = flag.String("dest-base", "", "Path of the destination files")
)

func transformFile(spec *specification, filename string) error {
	const newLine = "\n"

	sourcePath := filepath.Join(*sourceBase, filename)
	destPath := filename
	destPath = strings.TrimPrefix(destPath, "net/")
	destPath = strings.TrimPrefix(destPath, "cert/")
	destPath = strings.TrimPrefix(destPath, "der/")
	destPath = strings.TrimPrefix(destPath, "pki/")
	destPath = filepath.Join(*destBase, destPath)
	destDir := filepath.Dir(destPath)
	if err := os.MkdirAll(destDir, 0755); err != nil {
		return err
	}

	source, err := os.Open(sourcePath)
	if err != nil {
		return err
	}
	defer source.Close()

	dest, err := os.OpenFile(destPath, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, 0644)
	if err != nil {
		return err
	}
	defer dest.Close()

	var using []string
	var includeInsertionPoint int
	includes := make(map[string]struct{})
	scanner := bufio.NewScanner(source)
	out := ""
	for scanner.Scan() {
		line := scanner.Text()

		if includeInsertionPoint == 0 && len(line) > 0 &&
			!strings.HasPrefix(line, "// ") &&
			!strings.HasPrefix(line, "#if") &&
			!strings.HasPrefix(line, "#define ") {
			includeInsertionPoint = len(out)
		}

		for i, repl := range spec.Replacements {
			if !repl.matchRE.MatchString(line) {
				continue
			}
			line = repl.matchRE.ReplaceAllString(line, repl.Replace)
			atomic.StoreUint32(&spec.Replacements[i].used, 1)
			using = append(using, repl.Using...)
			if repl.Include != "" {
				includes[repl.Include] = struct{}{}
			}
		}

		for _, u := range using {
			line = strings.Replace(
				line, "namespace chromium_certificate_verifier {",
				"namespace chromium_certificate_verifier {\nusing "+u+";", 1)
		}

		out += line
		out += newLine
	}

	if len(includes) > 0 {
		if includeInsertionPoint == 0 {
			panic("failed to find include insertion point for " + filename)
		}

		var s string
		for include := range includes {
			s = s + "#include \"" + include + "\"\n"
		}

		out = out[:includeInsertionPoint] + s + out[includeInsertionPoint:]
	}

	dest.WriteString(out)
	fmt.Printf("%s\n", filename)

	return nil
}

func do() error {
	flag.Parse()

	specBytes, err := ioutil.ReadFile(*specFile)
	if err != nil {
		return err
	}

	var spec specification
	if err := json.Unmarshal(specBytes, &spec); err != nil {
		if jsonError, ok := err.(*json.SyntaxError); ok {
			return fmt.Errorf("JSON parse error at offset %v: %v", jsonError.Offset, err.Error())
		}
		return errors.New("JSON parse error: " + err.Error())
	}

	for i, repl := range spec.Replacements {
		var err error
		spec.Replacements[i].matchRE, err = regexp.Compile(repl.Match)
		if err != nil {
			return fmt.Errorf("Failed to parse %q: %s", repl.Match, err)
		}
	}

	errors := make(chan error, len(spec.Files))
	var wg sync.WaitGroup

	for _, filename := range spec.Files {
		wg.Add(1)

		go func(filename string) {
			if err := transformFile(&spec, filename); err != nil {
				errors <- err
			}
			wg.Done()
		}(filename)
	}

	wg.Wait()
	select {
	case err := <-errors:
		return err
	default:
		break
	}
	for _, repl := range spec.Replacements {
		if repl.used == 0 {
			fmt.Fprintf(os.Stderr, "replacement for \"%s\" not used\n", repl.Match)
		}
	}
	return nil
}

func main() {
	if err := do(); err != nil {
		fmt.Fprintf(os.Stderr, "%s\n", err)
		os.Exit(1)
	}
}
