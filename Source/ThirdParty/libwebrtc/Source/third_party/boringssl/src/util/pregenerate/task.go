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

package main

import (
	"bytes"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path"
	"path/filepath"
)

// TaskSkipped indicates the task has been skipped.
var TaskSkipped = errors.New("task skipped")

// Task is a task the pregenerate system can perform.
type Task struct {
	// Kind is the kind/type of the task in human readable form.
	Kind string

	// Destination is the destination path for this task, using forward
	// slashes and relative to the source directory. That is, use the "path"
	// package, not "path/filepath".
	Destination string

	// Dependencies are the list of tasks this task depends on.
	Dependencies []*Task

	// Func is the function the task performs when executed. It only runs once all `Dependencies` have finished.
	RunFunc func() ([]byte, error)

	// finishedC gets closed when the task is done.
	finishedC chan struct{}

	// err contains the task's status when done.
	err error
}

// String returns a human readable name for a task; just using the destination path for now.
func (t *Task) String() string {
	return t.Destination
}

// Prepare must be called on a task before it can be used.
func (t *Task) Prepare() *Task {
	if t.finishedC == nil {
		t.finishedC = make(chan struct{})
	}
	return t
}

// runInternal performs the task's job, not accounting to only run once.
func (t *Task) runInternal() (out []byte, err error) {
	defer func() {
		if p := recover(); p != nil {
			err = fmt.Errorf("panic caught: %v", p)
		}
	}()
	for _, dep := range t.Dependencies {
		err := dep.wait()
		if err != nil {
			if errors.Is(err, TaskSkipped) {
				logV.Printf("task %q dependency %q skipped - carrying on with previously saved data: %v", t, dep, err)
				continue
			}
			return nil, fmt.Errorf("task %q dependency %q unfulfilled: %w", t, dep, err)
		}
	}
	return t.RunFunc()
}

// Run runs the task, and closes it - unless the task has already been closed.
//
// Must only be called once, and not concurrently with any `Close()` calls.
func (t *Task) Run() ([]byte, error) {
	select {
	case <-t.finishedC:
		return nil, fmt.Errorf("task already closed: %w", t.err)
	default:
	}
	out, err := t.runInternal()
	t.Close(err)
	return out, err
}

// wait waits for the task to finish, and returns its status.
func (t *Task) wait() error {
	<-t.finishedC
	return t.err
}

// Close marks the task as done with the given status.
func (t *Task) Close(err error) {
	t.err = err
	close(t.finishedC)
}

// NewSimpleTask creates a new task based on a lambda for what it does.
func NewSimpleTask(kind, dst string, runFunc func() ([]byte, error), dependencies ...*Task) *Task {
	return (&Task{
		Kind:         kind,
		Destination:  dst,
		Dependencies: dependencies,
		RunFunc:      runFunc,
	}).Prepare()
}

// NewPerlasmTask creates a new task that runs perlasm.
func NewPerlasmTask(dst, src string, perlasmArgs []string) *Task {
	return NewSimpleTask("perlasm", dst, func() (data []byte, err error) {
		if *perlPath == "" {
			return nil, fmt.Errorf("%w: perl has been disabled by flag", TaskSkipped)
		}

		defer func() {
			if err != nil {
				err = fmt.Errorf("%w; note that this step can be turned off by passing -perl=", err)
			}
		}()

		base := path.Base(dst)
		out, err := os.CreateTemp("", "*."+base)
		if err != nil {
			return nil, err
		}
		defer os.Remove(out.Name())

		args := make([]string, 0, 2+len(perlasmArgs))
		args = append(args, filepath.FromSlash(src))
		args = append(args, perlasmArgs...)
		args = append(args, out.Name())
		cmd := exec.Command(*perlPath, args...)
		cmd.Stderr = os.Stderr
		cmd.Stdout = os.Stdout
		if err := cmd.Run(); err != nil {
			return nil, err
		}

		data, err = os.ReadFile(out.Name())
		if err != nil {
			return nil, err
		}

		// On Windows, perl emits CRLF line endings. Normalize this so that the tool
		// can be run on Windows too.
		data = bytes.ReplaceAll(data, []byte("\r\n"), []byte("\n"))
		return data, nil
	})
}
