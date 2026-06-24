// Copyright (c) 2025 The BoringSSL Authors
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

// This file defines just the types representing the Clang AST.
// Methods are in clang_ast_parser.go.

package idextractor

// node is a node from the Clang AST dump.
type node struct {
	Kind  string
	Loc   loc
	Range rangeStruct `json:",omitempty"`
	Inner []*node     `json:",omitempty"`

	// Node fields that may or may not matter depending on `Kind`.
	CompleteDefinition bool        `json:",omitempty"`
	ConstExpr          bool        `json:",omitempty"`
	Decl               *node       `json:",omitempty"`
	Inline             bool        `json:",omitempty"`
	IsImplicit         bool        `json:",omitempty"`
	Language           string      `json:",omitempty"`
	Name               string      `json:",omitempty"`
	PreviousDecl       string      `json:",omitempty"`
	StorageClass       string      `json:",omitempty"`
	TagUsed            string      `json:",omitempty"`
	Type               *typeStruct `json:",omitempty"`
}

// typeStruct is the type information from the Clang AST dump.
type typeStruct struct {
	QualType          string `json:",omitempty"`
	DesugaredQualType string `json:",omitempty"`
}

// rangeStruct is a location range from the Clang AST dump.
type rangeStruct struct {
	Begin *loc `json:",omitempty"`
	End   *loc `json:",omitempty"`
}

// loc is a location from the Clang AST dump.
type loc struct {
	File         string `json:",omitempty"`
	Line         uint   `json:",omitempty"`
	Col          uint   `json:",omitempty"`
	Offset       uint   `json:",omitempty"`
	TokLen       uint   `json:",omitempty"`
	SpellingLoc  *loc   `json:",omitempty"`
	ExpansionLoc *loc   `json:",omitempty"`
}
