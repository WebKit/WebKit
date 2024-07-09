// Copyright (c) 2024, Google Inc.
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
	"bytes"
	"os"
	"os/exec"
	"path"
	"path/filepath"
)

type Task interface {
	// Destination returns the destination path for this task, using forward
	// slashes and relative to the source directory. That is, use the "path"
	// package, not "path/filepath".
	Destination() string

	// Run computes the output for this task. It should be written to the
	// destination path.
	Run() ([]byte, error)
}

type SimpleTask struct {
	Dst     string
	RunFunc func() ([]byte, error)
}

func (t *SimpleTask) Destination() string  { return t.Dst }
func (t *SimpleTask) Run() ([]byte, error) { return t.RunFunc() }

func NewSimpleTask(dst string, runFunc func() ([]byte, error)) *SimpleTask {
	return &SimpleTask{Dst: dst, RunFunc: runFunc}
}

type PerlasmTask struct {
	Src, Dst string
	Args     []string
}

func (t *PerlasmTask) Destination() string { return t.Dst }
func (t *PerlasmTask) Run() ([]byte, error) {
	base := path.Base(t.Dst)
	out, err := os.CreateTemp("", "*."+base)
	if err != nil {
		return nil, err
	}
	defer os.Remove(out.Name())

	args := make([]string, 0, 2+len(t.Args))
	args = append(args, filepath.FromSlash(t.Src))
	args = append(args, t.Args...)
	args = append(args, out.Name())
	cmd := exec.Command(*perlPath, args...)
	cmd.Stderr = os.Stderr
	cmd.Stdout = os.Stdout
	if err := cmd.Run(); err != nil {
		return nil, err
	}

	data, err := os.ReadFile(out.Name())
	if err != nil {
		return nil, err
	}

	// On Windows, perl emits CRLF line endings. Normalize this so that the tool
	// can be run on Windows too.
	data = bytes.ReplaceAll(data, []byte("\r\n"), []byte("\n"))
	return data, nil
}
