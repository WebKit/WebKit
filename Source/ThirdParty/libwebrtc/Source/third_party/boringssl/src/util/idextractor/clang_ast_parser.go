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

// Implementation to extract identifier declarations from a Clang AST.

package idextractor

import (
	"encoding/json"
	"fmt"
	"log"
	"path"
	"regexp"
	"strings"
)

// desugaredQualType returns the canonical type after expanding typedefs.
func (t typeStruct) desugaredQualType() string {
	if t.DesugaredQualType != "" {
		return t.DesugaredQualType
	}
	return t.QualType
}

var typeTag = regexp.MustCompile(`^(?:enum|struct|union)\b`)

// qualTypeTag returns the tag of the qualified type, if any.
func (t typeStruct) qualTypeTag() string {
	return typeTag.FindString(t.QualType)
}

// constTypeRE is an approximate regex for types that are const.
// It errs on the side of returning constants as non-const.
var constType = regexp.MustCompile(`.*\bconst\b[^\[\]()*&]*$`)

// isConst returns whether the given type is likely a constant.
func (t typeStruct) isConst() bool {
	return constType.MatchString(t.desugaredQualType())
}

// expansionLoc returns the expansion location of the given loc.
func (l loc) expansionLoc() loc {
	if l.ExpansionLoc != nil {
		return *l.ExpansionLoc
	}
	if l.SpellingLoc != nil {
		return *l.SpellingLoc
	}
	return l
}

type decompressCtx struct {
	file string
	line uint
}

// decompress undoes the filename field compression from
// JSONNodeDumper::writeSourceLocation and JSONNodeDumper::writeBareSourceLocation.
func (l *loc) decompress(last *decompressCtx) {
	if l == nil {
		return
	}
	l.SpellingLoc.decompress(last)
	l.ExpansionLoc.decompress(last)
	if l.SpellingLoc != nil || l.ExpansionLoc != nil {
		return
	}
	if l.File == "" {
		l.File = last.file
	} else {
		last.file = l.File
	}
	if l.Line == 0 {
		l.Line = last.line
	} else {
		last.line = l.Line
	}
}

// path returns the loc's file path in clean, unique form.
func (l loc) path() string {
	return path.Clean(l.File)
}

// decompressLocsInternal is a helper for decompressLocs.
//
// It keeps state in its lastFile pointer.
func (n *node) decompressLocsInternal(last *decompressCtx) {
	n.Loc.decompress(last)
	n.Range.Begin.decompress(last)
	n.Range.End.decompress(last)
	for _, child := range n.Inner {
		child.decompressLocsInternal(last)
	}
}

// decompressLocs decompresses all Loc fields below a node.
//
// Should be called right after parsing.
func (n *node) decompressLocs() {
	n.decompressLocsInternal(&decompressCtx{})
}

// storage represents the storage class of a node.
type storage int

const (
	noStorage storage = iota
	externStorage
	staticStorage
)

// storage finds the storage class of the node.
func (n node) storage(language string) (storage, error) {
	var storage storage
	switch n.StorageClass {
	case "":
		if n.Kind == "VarDecl" && (n.ConstExpr || (language == "C++" && n.Type.isConst())) {
			storage = staticStorage
		} else {
			storage = externStorage
		}
	case "extern":
		storage = externStorage
	case "static":
		storage = staticStorage
	default:
		return noStorage, fmt.Errorf("no handling for storage class %q", n.StorageClass)
	}
	return storage, nil
}

// functionArgs returns all the function arg nodes of a node.
func (n node) functionArgs() []*node {
	var result []*node
	for _, child := range n.Inner {
		if child.Kind == "ParmVarDecl" {
			result = append(result, child)
		}
	}
	return result
}

func (n node) locationRange() (file string, line, start, end uint, ok bool) {
	if n.Range.Begin == nil || n.Range.End == nil {
		return "", 0, 0, 0, false
	}
	from := n.Range.Begin.expansionLoc()
	to := n.Range.End.expansionLoc()
	if from.path() != to.path() {
		return "", 0, 0, 0, false
	}
	return from.path(), from.Line, from.Offset, to.Offset + to.TokLen, true
}

func (n node) locationRangeWithoutBody() (file string, line, start, end uint, ok bool) {
	file, line, start, end, ok = n.locationRange()
	if !ok {
		return
	}
	// Cut bad nodes from the end.
	for i := len(n.Inner) - 1; i >= 0; i-- {
		child := n.Inner[i]
		if child.Kind != "CompoundStmt" {
			continue
		}
		cfile, _, cstart, cend, ok := child.locationRange()
		if !ok {
			continue
		}
		if cfile == file && cend == end {
			end = cstart
		}
	}
	return
}

// namespacing indicates how the identifier respects namespaces.
type namespacing int

const (
	alwaysGlobal     namespacing = iota // Never in namespace (such as preprocessor macros).
	globalIfC                           // Respects namespace unless in extern "C" (such as functions).
	alwaysNamespaced                    // Always respects namespace (such as types).
)

// linking indicates how the identifier responds to extern "C" or similar.
type linking int

const (
	neverLinked     linking = iota // Ignores linkage information (such as types).
	respectsLinkage                // Respects linkage information (such as functions).
)

// extractor is data that is transported to inner nodes while parsing.
type extractor struct {
	*extractorStatic // Data that can be mutated even by downstream nodes.

	inBoringSSL   bool     // Whether the code originates from BoringSSL.
	depth         int      // Nesting depth (for -dump_tree output).
	namespace     []string // C++ namespace sequence the node is in.
	anonNamespace bool     // Whether the node is in a C++ anonymous namespace.
	language      string   // Can be "C" or "C++".
	record        bool     // Whether the current node is part of a record.
}

type IdentifierInfo struct {
	Identifier string // Just the identifier.
	Symbol     string // Name as seen by linker.
	FQName     string // Fully qualified name in C++.
	Linkage    string // Linkage string (can be stuff like 'extern "C"' or "static").
	Tag        string // Tag (`struct`, `union`, `var` etc.)

	File         string   // File where the identifier is declared.
	Line         uint     // Line in the file.
	Start        uint     // Start byte position in the file.
	End          uint     // End byte position in the file.
	FunctionArgs []string // List of function arguments by name.
}

// extractorStatic is data that is transported in reading direction while parsing.
type extractorStatic struct {
	reportIdentifier func(IdentifierInfo) error
	options          Options
}

// Consider files with a non-absolute path to be BoringSSL,
// whereas absolute paths usually indicate system header locations.
//
// Note that any non-word character in the first two characters is treated as
// indicating an absolute path to catch "<built-in>", "/foo/bar.h" and "C:\foo\bar.h".
var (
	boringSSLPath = regexp.MustCompile(`^\w\w`)
)

// updateInBoringSSL checks whether the given directive is a file/line directive,
// and if so, checks if it's likely part of BoringSSL or not.
//
// The return value indicates whether it's a file/line directive.
// If it is, `*in` will be updated to the current status of whether this is BoringSSL.
func (x *extractor) updateInBoringSSL(kind string, loc loc) {
	if kind == "TranslationUnitDecl" {
		x.inBoringSSL = true
		return
	}
	x.inBoringSSL = boringSSLPath.MatchString(loc.expansionLoc().path())
}

// visit traverses a node in the AST and analyzes it for identifiers contained therein.
func (x extractor) visit(n *node) (err error) {
	nodeWithoutChildren := *n
	nodeWithoutChildren.Inner = nil
	nodeCode, err := json.Marshal(nodeWithoutChildren)
	if err != nil {
		return err
	}

	if (x.options.DumpTree && x.inBoringSSL) || x.options.DumpFullTree {
		log.Printf("%*s[%s] %s: %s (%d children)",
			x.depth, "",
			strings.Join(x.namespace, "::"),
			n.Kind,
			nodeCode,
			len(n.Inner))
	}

	// Allow to ignore errors.
	defer func() {
		if x.options.KeepGoing && err != nil {
			log.Printf("ERROR: %v", err)
			err = nil
		}
	}()

	// Update "x".
	x.depth++

	// Update "in BoringSSL".
	x.updateInBoringSSL(n.Kind, n.Loc)

	if !x.inBoringSSL || n.IsImplicit {
		// If suppressed, below nodes are not interesting.
		// Also, skip any non-BoringSSL code such as system headers.
		return nil
	}

	switch n.Kind {
	// Nodes that need handling.
	case "CXXRecordDecl", "RecordDecl":
		if x.record && n.CompleteDefinition {
			return nil
		}
		if n.Name != "" {
			if err := x.collectIdentifier(n, n.TagUsed, alwaysNamespaced, neverLinked, noStorage); err != nil {
				return err
			}
		}
		x.record = true
	case "EnumDecl":
		if x.record {
			return nil
		}
		if n.Name != "" {
			if err := x.collectIdentifier(n, "enum", alwaysNamespaced, neverLinked, noStorage); err != nil {
				return err
			}
		}
	case "EnumConstantDecl":
		if x.record {
			return nil
		}
		if err := x.collectIdentifier(n, "enumerator", alwaysNamespaced, neverLinked, noStorage); err != nil {
			return err
		}
		return nil // Do not recurse.
	case "FunctionDecl":
		if x.record {
			return nil
		}
		if n.PreviousDecl != "" {
			return // Definition or redeclaration doesn't need to be looked at again (and may have incomplete qualifiers).
		}
		storage, err := n.storage(x.language)
		if err != nil {
			return fmt.Errorf("could not find storage class of function: %x: %s", err, nodeCode)
		}
		if err := x.collectIdentifier(n, "function", globalIfC, respectsLinkage, storage); err != nil {
			return err
		}
	case "LinkageSpecDecl":
		if n.Language != "" {
			x.language = n.Language
		}
	case "NamespaceDecl":
		if x.language != "C++" {
			return fmt.Errorf("entering namespace while in extern %q is probably unintended: %s", x.language, nodeCode)
		}
		if n.Name == "" {
			x.anonNamespace = true
		} else {
			x.namespace = append(append([]string(nil), x.namespace...), n.Name)
		}
	case "TypeAliasDecl", "TypeAliasTemplateDecl":
		if x.record {
			return nil
		}
		if err := x.collectIdentifier(n, "using", alwaysNamespaced, neverLinked, noStorage); err != nil {
			return err
		}
	case "TypedefDecl":
		if x.record {
			return nil
		}
		if len(n.Inner) == 1 && n.Inner[0].Kind == "ElaboratedType" && len(n.Inner[0].Inner) == 1 && n.Inner[0].Inner[0].Decl != nil && n.Inner[0].Inner[0].Decl.Name == n.Name {
			// typedef struct X { ... } X;
			return nil
		}
		tag := "typedef"
		if len(n.Inner) == 1 && n.Inner[0].Kind == "ElaboratedType" && len(n.Inner[0].Inner) == 1 && n.Inner[0].Inner[0].Decl != nil && n.Inner[0].Inner[0].Decl.Name == "" {
			tag = n.Type.qualTypeTag()
			if tag == "" {
				return fmt.Errorf("typedef refers to an anonymous type but has no tag: %s", nodeCode)
			}
		}
		if err := x.collectIdentifier(n, tag, alwaysNamespaced, neverLinked, noStorage); err != nil {
			return err
		}
	case "VarDecl":
		if n.PreviousDecl != "" {
			return // Definition or redeclaration doesn't need to be looked at again (and may have incomplete qualifiers).
		}
		storage, err := n.storage(x.language)
		if err != nil {
			return fmt.Errorf("could not find storage class of variable: %x: %s", err, nodeCode)
		}
		if err := x.collectIdentifier(n, "var", globalIfC, respectsLinkage, storage); err != nil {
			return err
		}
		return nil // Do not recurse. (Maybe should, to catch `struct ...` in variable types?)
	case "":
		// Currently this happens to the function body of `bssl::PushToStack` on Windows.
		log.Printf("WARNING: ignoring kind-less AST node: %s", nodeCode)
		return nil
	// Singletons that should be skipped.
	case
		"AccessSpecDecl",
		"AlwaysInlineAttr",
		"AsmLabelAttr",
		"BuiltinAttr",
		"BuiltinType",
		"CXX11NoReturnAttr",
		"ConstAttr",
		"DependentNameType",
		"DeprecatedAttr",
		"EnumType",
		"FinalAttr",
		"FormatAttr",
		"MaxFieldAlignmentAttr",
		"NoThrowAttr",
		"RecordType",
		"TemplateTypeParmType",
		"UnresolvedUsingValueDecl",
		"UnusedAttr",
		"UsingDecl",
		"UsingDirectiveDecl",
		"VisibilityAttr",
		"WarnUnusedResultAttr",
		"WeakAttr":
		if len(n.Inner) != 0 {
			// If this ever fires, check AST to see if any of the node's children could be useful,
			// then categorize the node type into one of the following two cases.
			return fmt.Errorf("singleton node of kind %q has children: %s", n.Kind, nodeCode)
		}
	// Nodes that should be skipped including possible children.
	case
		"AlignedAttr",
		"CXXConstructorDecl",
		"CXXConversionDecl",
		"CXXDeductionGuideDecl",
		"CXXDestructorDecl",
		"CXXMethodDecl",
		"ClassTemplatePartialSpecializationDecl",
		"ClassTemplateSpecializationDecl",
		"CompoundStmt",
		"DecltypeType",
		"FieldDecl",
		"FriendDecl",
		"NonTypeTemplateParmDecl",
		"ParmVarDecl",
		"StaticAssertDecl",
		"TemplateArgument",
		"TemplateTemplateParmDecl",
		"TemplateTypeParmDecl",
		"VarTemplateDecl",
		"VarTemplateSpecializationDecl":
		return nil // Do not recurse.
	// Nodes that should just be recursed into.
	case
		"ClassTemplateDecl",
		"ConstantArrayType",
		"DecayedType",
		"ElaboratedType",
		"FunctionProtoType",
		"FunctionTemplateDecl",
		"IncompleteArrayType",
		"IndirectFieldDecl",
		"LValueReferenceType",
		"ParenType",
		"PointerType",
		"QualType",
		"TemplateSpecializationType",
		"TranslationUnitDecl",
		"TypedefType",
		"VectorType":
		// Just recurse.
	default:
		return fmt.Errorf("no handling for node kind %q: %s", n.Kind, nodeCode)
	}

	// If we get here (via fallthrough usually), we want to recurse.
	// To avoid recursing, use return.
	for _, child := range n.Inner {
		err = x.visit(child)
		if err != nil {
			break
		}
	}

	return err
}

// collectIdentifier sends an identifier to the output.
func (x extractor) collectIdentifier(n *node, tag string, namespacing namespacing, linking linking, storage storage) error {
	identifier := n.Name
	var fqn string
	if x.anonNamespace {
		fqn = "<anonymous>::" + identifier
	} else {
		fqn = strings.Join(append(append([]string(nil), x.namespace...), identifier), "::")
	}

	// With this taken out of the way, for all intents and purposes
	// anything in an anonymous namespace behaves as if it were static.
	//
	// Helps in some cases where the static keyword
	// isn't repeated in template specializations or similar.
	if x.anonNamespace && x.language == "C++" {
		storage = staticStorage
	}

	var linkage string
	switch linking {
	case neverLinked:
		linkage = ""
	case respectsLinkage:
		switch storage {
		case externStorage:
			linkage = fmt.Sprintf("extern %q", x.language)
		case staticStorage:
			linkage = "static"
		default:
			return fmt.Errorf("respecting linkage, but storage not set for %v", fqn)
		}
	}

	if n.Inline {
		if linkage != "" {
			linkage += " "
		}
		linkage += "inline"
	}

	var namespaced bool
	switch namespacing {
	case alwaysGlobal:
		namespaced = false
	case globalIfC:
		namespaced = x.language == "C++"
	case alwaysNamespaced:
		namespaced = true
	}

	var symbol string
	if namespaced {
		symbol = fqn
	} else {
		symbol = identifier
	}

	file, line, start, end, ok := n.locationRangeWithoutBody()
	if !ok {
		file = ""
	}

	var args []string
	for _, arg := range n.functionArgs() {
		args = append(args, arg.Name)
	}

	id := IdentifierInfo{
		Identifier:   identifier,
		Symbol:       symbol,
		FQName:       fqn,
		Linkage:      linkage,
		Tag:          tag,
		File:         file,
		Line:         line,
		Start:        start,
		End:          end,
		FunctionArgs: args,
	}
	return x.reportIdentifier(id)
}
