package main

import (
	"fmt"
	"math"
	"sort"
	"strconv"
)

const endSymbol rune = 1114112

/* The rule types inferred from the grammar are below. */
type pegRule uint8

const (
	ruleUnknown pegRule = iota
	ruleAsmFile
	ruleStatement
	ruleGlobalDirective
	ruleDirective
	ruleDirectiveName
	ruleLocationDirective
	ruleFileDirective
	ruleLocDirective
	ruleArgs
	ruleArg
	ruleQuotedArg
	ruleQuotedText
	ruleLabelContainingDirective
	ruleLabelContainingDirectiveName
	ruleSymbolArgs
	ruleSymbolShift
	ruleSymbolArg
	ruleOpenParen
	ruleCloseParen
	ruleSymbolType
	ruleDot
	ruleTCMarker
	ruleEscapedChar
	ruleWS
	ruleComment
	ruleLabel
	ruleSymbolName
	ruleLocalSymbol
	ruleLocalLabel
	ruleLocalLabelRef
	ruleInstruction
	ruleInstructionName
	ruleInstructionArg
	ruleGOTLocation
	ruleGOTSymbolOffset
	ruleAVX512Token
	ruleTOCRefHigh
	ruleTOCRefLow
	ruleIndirectionIndicator
	ruleRegisterOrConstant
	ruleARMConstantTweak
	ruleARMRegister
	ruleARMVectorRegister
	ruleMemoryRef
	ruleSymbolRef
	ruleLow12BitsSymbolRef
	ruleARMBaseIndexScale
	ruleARMGOTLow12
	ruleARMPostincrement
	ruleBaseIndexScale
	ruleOperator
	ruleOffset
	ruleSection
	ruleSegmentRegister
)

var rul3s = [...]string{
	"Unknown",
	"AsmFile",
	"Statement",
	"GlobalDirective",
	"Directive",
	"DirectiveName",
	"LocationDirective",
	"FileDirective",
	"LocDirective",
	"Args",
	"Arg",
	"QuotedArg",
	"QuotedText",
	"LabelContainingDirective",
	"LabelContainingDirectiveName",
	"SymbolArgs",
	"SymbolShift",
	"SymbolArg",
	"OpenParen",
	"CloseParen",
	"SymbolType",
	"Dot",
	"TCMarker",
	"EscapedChar",
	"WS",
	"Comment",
	"Label",
	"SymbolName",
	"LocalSymbol",
	"LocalLabel",
	"LocalLabelRef",
	"Instruction",
	"InstructionName",
	"InstructionArg",
	"GOTLocation",
	"GOTSymbolOffset",
	"AVX512Token",
	"TOCRefHigh",
	"TOCRefLow",
	"IndirectionIndicator",
	"RegisterOrConstant",
	"ARMConstantTweak",
	"ARMRegister",
	"ARMVectorRegister",
	"MemoryRef",
	"SymbolRef",
	"Low12BitsSymbolRef",
	"ARMBaseIndexScale",
	"ARMGOTLow12",
	"ARMPostincrement",
	"BaseIndexScale",
	"Operator",
	"Offset",
	"Section",
	"SegmentRegister",
}

type token32 struct {
	pegRule
	begin, end uint32
}

func (t *token32) String() string {
	return fmt.Sprintf("\x1B[34m%v\x1B[m %v %v", rul3s[t.pegRule], t.begin, t.end)
}

type node32 struct {
	token32
	up, next *node32
}

func (node *node32) print(pretty bool, buffer string) {
	var print func(node *node32, depth int)
	print = func(node *node32, depth int) {
		for node != nil {
			for c := 0; c < depth; c++ {
				fmt.Printf(" ")
			}
			rule := rul3s[node.pegRule]
			quote := strconv.Quote(string(([]rune(buffer)[node.begin:node.end])))
			if !pretty {
				fmt.Printf("%v %v\n", rule, quote)
			} else {
				fmt.Printf("\x1B[34m%v\x1B[m %v\n", rule, quote)
			}
			if node.up != nil {
				print(node.up, depth+1)
			}
			node = node.next
		}
	}
	print(node, 0)
}

func (node *node32) Print(buffer string) {
	node.print(false, buffer)
}

func (node *node32) PrettyPrint(buffer string) {
	node.print(true, buffer)
}

type tokens32 struct {
	tree []token32
}

func (t *tokens32) Trim(length uint32) {
	t.tree = t.tree[:length]
}

func (t *tokens32) Print() {
	for _, token := range t.tree {
		fmt.Println(token.String())
	}
}

func (t *tokens32) AST() *node32 {
	type element struct {
		node *node32
		down *element
	}
	tokens := t.Tokens()
	var stack *element
	for _, token := range tokens {
		if token.begin == token.end {
			continue
		}
		node := &node32{token32: token}
		for stack != nil && stack.node.begin >= token.begin && stack.node.end <= token.end {
			stack.node.next = node.up
			node.up = stack.node
			stack = stack.down
		}
		stack = &element{node: node, down: stack}
	}
	if stack != nil {
		return stack.node
	}
	return nil
}

func (t *tokens32) PrintSyntaxTree(buffer string) {
	t.AST().Print(buffer)
}

func (t *tokens32) PrettyPrintSyntaxTree(buffer string) {
	t.AST().PrettyPrint(buffer)
}

func (t *tokens32) Add(rule pegRule, begin, end, index uint32) {
	if tree := t.tree; int(index) >= len(tree) {
		expanded := make([]token32, 2*len(tree))
		copy(expanded, tree)
		t.tree = expanded
	}
	t.tree[index] = token32{
		pegRule: rule,
		begin:   begin,
		end:     end,
	}
}

func (t *tokens32) Tokens() []token32 {
	return t.tree
}

type Asm struct {
	Buffer string
	buffer []rune
	rules  [55]func() bool
	parse  func(rule ...int) error
	reset  func()
	Pretty bool
	tokens32
}

func (p *Asm) Parse(rule ...int) error {
	return p.parse(rule...)
}

func (p *Asm) Reset() {
	p.reset()
}

type textPosition struct {
	line, symbol int
}

type textPositionMap map[int]textPosition

func translatePositions(buffer []rune, positions []int) textPositionMap {
	length, translations, j, line, symbol := len(positions), make(textPositionMap, len(positions)), 0, 1, 0
	sort.Ints(positions)

search:
	for i, c := range buffer {
		if c == '\n' {
			line, symbol = line+1, 0
		} else {
			symbol++
		}
		if i == positions[j] {
			translations[positions[j]] = textPosition{line, symbol}
			for j++; j < length; j++ {
				if i != positions[j] {
					continue search
				}
			}
			break search
		}
	}

	return translations
}

type parseError struct {
	p   *Asm
	max token32
}

func (e *parseError) Error() string {
	tokens, error := []token32{e.max}, "\n"
	positions, p := make([]int, 2*len(tokens)), 0
	for _, token := range tokens {
		positions[p], p = int(token.begin), p+1
		positions[p], p = int(token.end), p+1
	}
	translations := translatePositions(e.p.buffer, positions)
	format := "parse error near %v (line %v symbol %v - line %v symbol %v):\n%v\n"
	if e.p.Pretty {
		format = "parse error near \x1B[34m%v\x1B[m (line %v symbol %v - line %v symbol %v):\n%v\n"
	}
	for _, token := range tokens {
		begin, end := int(token.begin), int(token.end)
		error += fmt.Sprintf(format,
			rul3s[token.pegRule],
			translations[begin].line, translations[begin].symbol,
			translations[end].line, translations[end].symbol,
			strconv.Quote(string(e.p.buffer[begin:end])))
	}

	return error
}

func (p *Asm) PrintSyntaxTree() {
	if p.Pretty {
		p.tokens32.PrettyPrintSyntaxTree(p.Buffer)
	} else {
		p.tokens32.PrintSyntaxTree(p.Buffer)
	}
}

func (p *Asm) Init() {
	var (
		max                  token32
		position, tokenIndex uint32
		buffer               []rune
	)
	p.reset = func() {
		max = token32{}
		position, tokenIndex = 0, 0

		p.buffer = []rune(p.Buffer)
		if len(p.buffer) == 0 || p.buffer[len(p.buffer)-1] != endSymbol {
			p.buffer = append(p.buffer, endSymbol)
		}
		buffer = p.buffer
	}
	p.reset()

	_rules := p.rules
	tree := tokens32{tree: make([]token32, math.MaxInt16)}
	p.parse = func(rule ...int) error {
		r := 1
		if len(rule) > 0 {
			r = rule[0]
		}
		matches := p.rules[r]()
		p.tokens32 = tree
		if matches {
			p.Trim(tokenIndex)
			return nil
		}
		return &parseError{p, max}
	}

	add := func(rule pegRule, begin uint32) {
		tree.Add(rule, begin, position, tokenIndex)
		tokenIndex++
		if begin != position && position > max.end {
			max = token32{rule, begin, position}
		}
	}

	matchDot := func() bool {
		if buffer[position] != endSymbol {
			position++
			return true
		}
		return false
	}

	/*matchChar := func(c byte) bool {
		if buffer[position] == c {
			position++
			return true
		}
		return false
	}*/

	/*matchRange := func(lower byte, upper byte) bool {
		if c := buffer[position]; c >= lower && c <= upper {
			position++
			return true
		}
		return false
	}*/

	_rules = [...]func() bool{
		nil,
		/* 0 AsmFile <- <(Statement* !.)> */
		func() bool {
			position0, tokenIndex0 := position, tokenIndex
			{
				position1 := position
			l2:
				{
					position3, tokenIndex3 := position, tokenIndex
					if !_rules[ruleStatement]() {
						goto l3
					}
					goto l2
				l3:
					position, tokenIndex = position3, tokenIndex3
				}
				{
					position4, tokenIndex4 := position, tokenIndex
					if !matchDot() {
						goto l4
					}
					goto l0
				l4:
					position, tokenIndex = position4, tokenIndex4
				}
				add(ruleAsmFile, position1)
			}
			return true
		l0:
			position, tokenIndex = position0, tokenIndex0
			return false
		},
		/* 1 Statement <- <(WS? (Label / ((GlobalDirective / LocationDirective / LabelContainingDirective / Instruction / Directive / Comment / ) WS? ((Comment? '\n') / ';'))))> */
		func() bool {
			position5, tokenIndex5 := position, tokenIndex
			{
				position6 := position
				{
					position7, tokenIndex7 := position, tokenIndex
					if !_rules[ruleWS]() {
						goto l7
					}
					goto l8
				l7:
					position, tokenIndex = position7, tokenIndex7
				}
			l8:
				{
					position9, tokenIndex9 := position, tokenIndex
					if !_rules[ruleLabel]() {
						goto l10
					}
					goto l9
				l10:
					position, tokenIndex = position9, tokenIndex9
					{
						position11, tokenIndex11 := position, tokenIndex
						if !_rules[ruleGlobalDirective]() {
							goto l12
						}
						goto l11
					l12:
						position, tokenIndex = position11, tokenIndex11
						if !_rules[ruleLocationDirective]() {
							goto l13
						}
						goto l11
					l13:
						position, tokenIndex = position11, tokenIndex11
						if !_rules[ruleLabelContainingDirective]() {
							goto l14
						}
						goto l11
					l14:
						position, tokenIndex = position11, tokenIndex11
						if !_rules[ruleInstruction]() {
							goto l15
						}
						goto l11
					l15:
						position, tokenIndex = position11, tokenIndex11
						if !_rules[ruleDirective]() {
							goto l16
						}
						goto l11
					l16:
						position, tokenIndex = position11, tokenIndex11
						if !_rules[ruleComment]() {
							goto l17
						}
						goto l11
					l17:
						position, tokenIndex = position11, tokenIndex11
					}
				l11:
					{
						position18, tokenIndex18 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l18
						}
						goto l19
					l18:
						position, tokenIndex = position18, tokenIndex18
					}
				l19:
					{
						position20, tokenIndex20 := position, tokenIndex
						{
							position22, tokenIndex22 := position, tokenIndex
							if !_rules[ruleComment]() {
								goto l22
							}
							goto l23
						l22:
							position, tokenIndex = position22, tokenIndex22
						}
					l23:
						if buffer[position] != rune('\n') {
							goto l21
						}
						position++
						goto l20
					l21:
						position, tokenIndex = position20, tokenIndex20
						if buffer[position] != rune(';') {
							goto l5
						}
						position++
					}
				l20:
				}
			l9:
				add(ruleStatement, position6)
			}
			return true
		l5:
			position, tokenIndex = position5, tokenIndex5
			return false
		},
		/* 2 GlobalDirective <- <((('.' ('g' / 'G') ('l' / 'L') ('o' / 'O') ('b' / 'B') ('a' / 'A') ('l' / 'L')) / ('.' ('g' / 'G') ('l' / 'L') ('o' / 'O') ('b' / 'B') ('l' / 'L'))) WS SymbolName)> */
		func() bool {
			position24, tokenIndex24 := position, tokenIndex
			{
				position25 := position
				{
					position26, tokenIndex26 := position, tokenIndex
					if buffer[position] != rune('.') {
						goto l27
					}
					position++
					{
						position28, tokenIndex28 := position, tokenIndex
						if buffer[position] != rune('g') {
							goto l29
						}
						position++
						goto l28
					l29:
						position, tokenIndex = position28, tokenIndex28
						if buffer[position] != rune('G') {
							goto l27
						}
						position++
					}
				l28:
					{
						position30, tokenIndex30 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l31
						}
						position++
						goto l30
					l31:
						position, tokenIndex = position30, tokenIndex30
						if buffer[position] != rune('L') {
							goto l27
						}
						position++
					}
				l30:
					{
						position32, tokenIndex32 := position, tokenIndex
						if buffer[position] != rune('o') {
							goto l33
						}
						position++
						goto l32
					l33:
						position, tokenIndex = position32, tokenIndex32
						if buffer[position] != rune('O') {
							goto l27
						}
						position++
					}
				l32:
					{
						position34, tokenIndex34 := position, tokenIndex
						if buffer[position] != rune('b') {
							goto l35
						}
						position++
						goto l34
					l35:
						position, tokenIndex = position34, tokenIndex34
						if buffer[position] != rune('B') {
							goto l27
						}
						position++
					}
				l34:
					{
						position36, tokenIndex36 := position, tokenIndex
						if buffer[position] != rune('a') {
							goto l37
						}
						position++
						goto l36
					l37:
						position, tokenIndex = position36, tokenIndex36
						if buffer[position] != rune('A') {
							goto l27
						}
						position++
					}
				l36:
					{
						position38, tokenIndex38 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l39
						}
						position++
						goto l38
					l39:
						position, tokenIndex = position38, tokenIndex38
						if buffer[position] != rune('L') {
							goto l27
						}
						position++
					}
				l38:
					goto l26
				l27:
					position, tokenIndex = position26, tokenIndex26
					if buffer[position] != rune('.') {
						goto l24
					}
					position++
					{
						position40, tokenIndex40 := position, tokenIndex
						if buffer[position] != rune('g') {
							goto l41
						}
						position++
						goto l40
					l41:
						position, tokenIndex = position40, tokenIndex40
						if buffer[position] != rune('G') {
							goto l24
						}
						position++
					}
				l40:
					{
						position42, tokenIndex42 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l43
						}
						position++
						goto l42
					l43:
						position, tokenIndex = position42, tokenIndex42
						if buffer[position] != rune('L') {
							goto l24
						}
						position++
					}
				l42:
					{
						position44, tokenIndex44 := position, tokenIndex
						if buffer[position] != rune('o') {
							goto l45
						}
						position++
						goto l44
					l45:
						position, tokenIndex = position44, tokenIndex44
						if buffer[position] != rune('O') {
							goto l24
						}
						position++
					}
				l44:
					{
						position46, tokenIndex46 := position, tokenIndex
						if buffer[position] != rune('b') {
							goto l47
						}
						position++
						goto l46
					l47:
						position, tokenIndex = position46, tokenIndex46
						if buffer[position] != rune('B') {
							goto l24
						}
						position++
					}
				l46:
					{
						position48, tokenIndex48 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l49
						}
						position++
						goto l48
					l49:
						position, tokenIndex = position48, tokenIndex48
						if buffer[position] != rune('L') {
							goto l24
						}
						position++
					}
				l48:
				}
			l26:
				if !_rules[ruleWS]() {
					goto l24
				}
				if !_rules[ruleSymbolName]() {
					goto l24
				}
				add(ruleGlobalDirective, position25)
			}
			return true
		l24:
			position, tokenIndex = position24, tokenIndex24
			return false
		},
		/* 3 Directive <- <('.' DirectiveName (WS Args)?)> */
		func() bool {
			position50, tokenIndex50 := position, tokenIndex
			{
				position51 := position
				if buffer[position] != rune('.') {
					goto l50
				}
				position++
				if !_rules[ruleDirectiveName]() {
					goto l50
				}
				{
					position52, tokenIndex52 := position, tokenIndex
					if !_rules[ruleWS]() {
						goto l52
					}
					if !_rules[ruleArgs]() {
						goto l52
					}
					goto l53
				l52:
					position, tokenIndex = position52, tokenIndex52
				}
			l53:
				add(ruleDirective, position51)
			}
			return true
		l50:
			position, tokenIndex = position50, tokenIndex50
			return false
		},
		/* 4 DirectiveName <- <([a-z] / [A-Z] / ([0-9] / [0-9]) / '_')+> */
		func() bool {
			position54, tokenIndex54 := position, tokenIndex
			{
				position55 := position
				{
					position58, tokenIndex58 := position, tokenIndex
					if c := buffer[position]; c < rune('a') || c > rune('z') {
						goto l59
					}
					position++
					goto l58
				l59:
					position, tokenIndex = position58, tokenIndex58
					if c := buffer[position]; c < rune('A') || c > rune('Z') {
						goto l60
					}
					position++
					goto l58
				l60:
					position, tokenIndex = position58, tokenIndex58
					{
						position62, tokenIndex62 := position, tokenIndex
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l63
						}
						position++
						goto l62
					l63:
						position, tokenIndex = position62, tokenIndex62
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l61
						}
						position++
					}
				l62:
					goto l58
				l61:
					position, tokenIndex = position58, tokenIndex58
					if buffer[position] != rune('_') {
						goto l54
					}
					position++
				}
			l58:
			l56:
				{
					position57, tokenIndex57 := position, tokenIndex
					{
						position64, tokenIndex64 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l65
						}
						position++
						goto l64
					l65:
						position, tokenIndex = position64, tokenIndex64
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l66
						}
						position++
						goto l64
					l66:
						position, tokenIndex = position64, tokenIndex64
						{
							position68, tokenIndex68 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l69
							}
							position++
							goto l68
						l69:
							position, tokenIndex = position68, tokenIndex68
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l67
							}
							position++
						}
					l68:
						goto l64
					l67:
						position, tokenIndex = position64, tokenIndex64
						if buffer[position] != rune('_') {
							goto l57
						}
						position++
					}
				l64:
					goto l56
				l57:
					position, tokenIndex = position57, tokenIndex57
				}
				add(ruleDirectiveName, position55)
			}
			return true
		l54:
			position, tokenIndex = position54, tokenIndex54
			return false
		},
		/* 5 LocationDirective <- <(FileDirective / LocDirective)> */
		func() bool {
			position70, tokenIndex70 := position, tokenIndex
			{
				position71 := position
				{
					position72, tokenIndex72 := position, tokenIndex
					if !_rules[ruleFileDirective]() {
						goto l73
					}
					goto l72
				l73:
					position, tokenIndex = position72, tokenIndex72
					if !_rules[ruleLocDirective]() {
						goto l70
					}
				}
			l72:
				add(ruleLocationDirective, position71)
			}
			return true
		l70:
			position, tokenIndex = position70, tokenIndex70
			return false
		},
		/* 6 FileDirective <- <('.' ('f' / 'F') ('i' / 'I') ('l' / 'L') ('e' / 'E') WS (!('#' / '\n') .)+)> */
		func() bool {
			position74, tokenIndex74 := position, tokenIndex
			{
				position75 := position
				if buffer[position] != rune('.') {
					goto l74
				}
				position++
				{
					position76, tokenIndex76 := position, tokenIndex
					if buffer[position] != rune('f') {
						goto l77
					}
					position++
					goto l76
				l77:
					position, tokenIndex = position76, tokenIndex76
					if buffer[position] != rune('F') {
						goto l74
					}
					position++
				}
			l76:
				{
					position78, tokenIndex78 := position, tokenIndex
					if buffer[position] != rune('i') {
						goto l79
					}
					position++
					goto l78
				l79:
					position, tokenIndex = position78, tokenIndex78
					if buffer[position] != rune('I') {
						goto l74
					}
					position++
				}
			l78:
				{
					position80, tokenIndex80 := position, tokenIndex
					if buffer[position] != rune('l') {
						goto l81
					}
					position++
					goto l80
				l81:
					position, tokenIndex = position80, tokenIndex80
					if buffer[position] != rune('L') {
						goto l74
					}
					position++
				}
			l80:
				{
					position82, tokenIndex82 := position, tokenIndex
					if buffer[position] != rune('e') {
						goto l83
					}
					position++
					goto l82
				l83:
					position, tokenIndex = position82, tokenIndex82
					if buffer[position] != rune('E') {
						goto l74
					}
					position++
				}
			l82:
				if !_rules[ruleWS]() {
					goto l74
				}
				{
					position86, tokenIndex86 := position, tokenIndex
					{
						position87, tokenIndex87 := position, tokenIndex
						if buffer[position] != rune('#') {
							goto l88
						}
						position++
						goto l87
					l88:
						position, tokenIndex = position87, tokenIndex87
						if buffer[position] != rune('\n') {
							goto l86
						}
						position++
					}
				l87:
					goto l74
				l86:
					position, tokenIndex = position86, tokenIndex86
				}
				if !matchDot() {
					goto l74
				}
			l84:
				{
					position85, tokenIndex85 := position, tokenIndex
					{
						position89, tokenIndex89 := position, tokenIndex
						{
							position90, tokenIndex90 := position, tokenIndex
							if buffer[position] != rune('#') {
								goto l91
							}
							position++
							goto l90
						l91:
							position, tokenIndex = position90, tokenIndex90
							if buffer[position] != rune('\n') {
								goto l89
							}
							position++
						}
					l90:
						goto l85
					l89:
						position, tokenIndex = position89, tokenIndex89
					}
					if !matchDot() {
						goto l85
					}
					goto l84
				l85:
					position, tokenIndex = position85, tokenIndex85
				}
				add(ruleFileDirective, position75)
			}
			return true
		l74:
			position, tokenIndex = position74, tokenIndex74
			return false
		},
		/* 7 LocDirective <- <('.' ('l' / 'L') ('o' / 'O') ('c' / 'C') WS (!('#' / '/' / '\n') .)+)> */
		func() bool {
			position92, tokenIndex92 := position, tokenIndex
			{
				position93 := position
				if buffer[position] != rune('.') {
					goto l92
				}
				position++
				{
					position94, tokenIndex94 := position, tokenIndex
					if buffer[position] != rune('l') {
						goto l95
					}
					position++
					goto l94
				l95:
					position, tokenIndex = position94, tokenIndex94
					if buffer[position] != rune('L') {
						goto l92
					}
					position++
				}
			l94:
				{
					position96, tokenIndex96 := position, tokenIndex
					if buffer[position] != rune('o') {
						goto l97
					}
					position++
					goto l96
				l97:
					position, tokenIndex = position96, tokenIndex96
					if buffer[position] != rune('O') {
						goto l92
					}
					position++
				}
			l96:
				{
					position98, tokenIndex98 := position, tokenIndex
					if buffer[position] != rune('c') {
						goto l99
					}
					position++
					goto l98
				l99:
					position, tokenIndex = position98, tokenIndex98
					if buffer[position] != rune('C') {
						goto l92
					}
					position++
				}
			l98:
				if !_rules[ruleWS]() {
					goto l92
				}
				{
					position102, tokenIndex102 := position, tokenIndex
					{
						position103, tokenIndex103 := position, tokenIndex
						if buffer[position] != rune('#') {
							goto l104
						}
						position++
						goto l103
					l104:
						position, tokenIndex = position103, tokenIndex103
						if buffer[position] != rune('/') {
							goto l105
						}
						position++
						goto l103
					l105:
						position, tokenIndex = position103, tokenIndex103
						if buffer[position] != rune('\n') {
							goto l102
						}
						position++
					}
				l103:
					goto l92
				l102:
					position, tokenIndex = position102, tokenIndex102
				}
				if !matchDot() {
					goto l92
				}
			l100:
				{
					position101, tokenIndex101 := position, tokenIndex
					{
						position106, tokenIndex106 := position, tokenIndex
						{
							position107, tokenIndex107 := position, tokenIndex
							if buffer[position] != rune('#') {
								goto l108
							}
							position++
							goto l107
						l108:
							position, tokenIndex = position107, tokenIndex107
							if buffer[position] != rune('/') {
								goto l109
							}
							position++
							goto l107
						l109:
							position, tokenIndex = position107, tokenIndex107
							if buffer[position] != rune('\n') {
								goto l106
							}
							position++
						}
					l107:
						goto l101
					l106:
						position, tokenIndex = position106, tokenIndex106
					}
					if !matchDot() {
						goto l101
					}
					goto l100
				l101:
					position, tokenIndex = position101, tokenIndex101
				}
				add(ruleLocDirective, position93)
			}
			return true
		l92:
			position, tokenIndex = position92, tokenIndex92
			return false
		},
		/* 8 Args <- <(Arg (WS? ',' WS? Arg)*)> */
		func() bool {
			position110, tokenIndex110 := position, tokenIndex
			{
				position111 := position
				if !_rules[ruleArg]() {
					goto l110
				}
			l112:
				{
					position113, tokenIndex113 := position, tokenIndex
					{
						position114, tokenIndex114 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l114
						}
						goto l115
					l114:
						position, tokenIndex = position114, tokenIndex114
					}
				l115:
					if buffer[position] != rune(',') {
						goto l113
					}
					position++
					{
						position116, tokenIndex116 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l116
						}
						goto l117
					l116:
						position, tokenIndex = position116, tokenIndex116
					}
				l117:
					if !_rules[ruleArg]() {
						goto l113
					}
					goto l112
				l113:
					position, tokenIndex = position113, tokenIndex113
				}
				add(ruleArgs, position111)
			}
			return true
		l110:
			position, tokenIndex = position110, tokenIndex110
			return false
		},
		/* 9 Arg <- <(QuotedArg / ([0-9] / [0-9] / ([a-z] / [A-Z]) / '%' / '+' / '-' / '*' / '_' / '@' / '.')*)> */
		func() bool {
			{
				position119 := position
				{
					position120, tokenIndex120 := position, tokenIndex
					if !_rules[ruleQuotedArg]() {
						goto l121
					}
					goto l120
				l121:
					position, tokenIndex = position120, tokenIndex120
				l122:
					{
						position123, tokenIndex123 := position, tokenIndex
						{
							position124, tokenIndex124 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l125
							}
							position++
							goto l124
						l125:
							position, tokenIndex = position124, tokenIndex124
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l126
							}
							position++
							goto l124
						l126:
							position, tokenIndex = position124, tokenIndex124
							{
								position128, tokenIndex128 := position, tokenIndex
								if c := buffer[position]; c < rune('a') || c > rune('z') {
									goto l129
								}
								position++
								goto l128
							l129:
								position, tokenIndex = position128, tokenIndex128
								if c := buffer[position]; c < rune('A') || c > rune('Z') {
									goto l127
								}
								position++
							}
						l128:
							goto l124
						l127:
							position, tokenIndex = position124, tokenIndex124
							if buffer[position] != rune('%') {
								goto l130
							}
							position++
							goto l124
						l130:
							position, tokenIndex = position124, tokenIndex124
							if buffer[position] != rune('+') {
								goto l131
							}
							position++
							goto l124
						l131:
							position, tokenIndex = position124, tokenIndex124
							if buffer[position] != rune('-') {
								goto l132
							}
							position++
							goto l124
						l132:
							position, tokenIndex = position124, tokenIndex124
							if buffer[position] != rune('*') {
								goto l133
							}
							position++
							goto l124
						l133:
							position, tokenIndex = position124, tokenIndex124
							if buffer[position] != rune('_') {
								goto l134
							}
							position++
							goto l124
						l134:
							position, tokenIndex = position124, tokenIndex124
							if buffer[position] != rune('@') {
								goto l135
							}
							position++
							goto l124
						l135:
							position, tokenIndex = position124, tokenIndex124
							if buffer[position] != rune('.') {
								goto l123
							}
							position++
						}
					l124:
						goto l122
					l123:
						position, tokenIndex = position123, tokenIndex123
					}
				}
			l120:
				add(ruleArg, position119)
			}
			return true
		},
		/* 10 QuotedArg <- <('"' QuotedText '"')> */
		func() bool {
			position136, tokenIndex136 := position, tokenIndex
			{
				position137 := position
				if buffer[position] != rune('"') {
					goto l136
				}
				position++
				if !_rules[ruleQuotedText]() {
					goto l136
				}
				if buffer[position] != rune('"') {
					goto l136
				}
				position++
				add(ruleQuotedArg, position137)
			}
			return true
		l136:
			position, tokenIndex = position136, tokenIndex136
			return false
		},
		/* 11 QuotedText <- <(EscapedChar / (!'"' .))*> */
		func() bool {
			{
				position139 := position
			l140:
				{
					position141, tokenIndex141 := position, tokenIndex
					{
						position142, tokenIndex142 := position, tokenIndex
						if !_rules[ruleEscapedChar]() {
							goto l143
						}
						goto l142
					l143:
						position, tokenIndex = position142, tokenIndex142
						{
							position144, tokenIndex144 := position, tokenIndex
							if buffer[position] != rune('"') {
								goto l144
							}
							position++
							goto l141
						l144:
							position, tokenIndex = position144, tokenIndex144
						}
						if !matchDot() {
							goto l141
						}
					}
				l142:
					goto l140
				l141:
					position, tokenIndex = position141, tokenIndex141
				}
				add(ruleQuotedText, position139)
			}
			return true
		},
		/* 12 LabelContainingDirective <- <(LabelContainingDirectiveName WS SymbolArgs)> */
		func() bool {
			position145, tokenIndex145 := position, tokenIndex
			{
				position146 := position
				if !_rules[ruleLabelContainingDirectiveName]() {
					goto l145
				}
				if !_rules[ruleWS]() {
					goto l145
				}
				if !_rules[ruleSymbolArgs]() {
					goto l145
				}
				add(ruleLabelContainingDirective, position146)
			}
			return true
		l145:
			position, tokenIndex = position145, tokenIndex145
			return false
		},
		/* 13 LabelContainingDirectiveName <- <(('.' ('x' / 'X') ('w' / 'W') ('o' / 'O') ('r' / 'R') ('d' / 'D')) / ('.' ('w' / 'W') ('o' / 'O') ('r' / 'R') ('d' / 'D')) / ('.' ('l' / 'L') ('o' / 'O') ('n' / 'N') ('g' / 'G')) / ('.' ('s' / 'S') ('e' / 'E') ('t' / 'T')) / ('.' ('b' / 'B') ('y' / 'Y') ('t' / 'T') ('e' / 'E')) / ('.' '8' ('b' / 'B') ('y' / 'Y') ('t' / 'T') ('e' / 'E')) / ('.' '4' ('b' / 'B') ('y' / 'Y') ('t' / 'T') ('e' / 'E')) / ('.' ('q' / 'Q') ('u' / 'U') ('a' / 'A') ('d' / 'D')) / ('.' ('t' / 'T') ('c' / 'C')) / ('.' ('l' / 'L') ('o' / 'O') ('c' / 'C') ('a' / 'A') ('l' / 'L') ('e' / 'E') ('n' / 'N') ('t' / 'T') ('r' / 'R') ('y' / 'Y')) / ('.' ('s' / 'S') ('i' / 'I') ('z' / 'Z') ('e' / 'E')) / ('.' ('t' / 'T') ('y' / 'Y') ('p' / 'P') ('e' / 'E')) / ('.' ('u' / 'U') ('l' / 'L') ('e' / 'E') ('b' / 'B') '1' '2' '8') / ('.' ('s' / 'S') ('l' / 'L') ('e' / 'E') ('b' / 'B') '1' '2' '8'))> */
		func() bool {
			position147, tokenIndex147 := position, tokenIndex
			{
				position148 := position
				{
					position149, tokenIndex149 := position, tokenIndex
					if buffer[position] != rune('.') {
						goto l150
					}
					position++
					{
						position151, tokenIndex151 := position, tokenIndex
						if buffer[position] != rune('x') {
							goto l152
						}
						position++
						goto l151
					l152:
						position, tokenIndex = position151, tokenIndex151
						if buffer[position] != rune('X') {
							goto l150
						}
						position++
					}
				l151:
					{
						position153, tokenIndex153 := position, tokenIndex
						if buffer[position] != rune('w') {
							goto l154
						}
						position++
						goto l153
					l154:
						position, tokenIndex = position153, tokenIndex153
						if buffer[position] != rune('W') {
							goto l150
						}
						position++
					}
				l153:
					{
						position155, tokenIndex155 := position, tokenIndex
						if buffer[position] != rune('o') {
							goto l156
						}
						position++
						goto l155
					l156:
						position, tokenIndex = position155, tokenIndex155
						if buffer[position] != rune('O') {
							goto l150
						}
						position++
					}
				l155:
					{
						position157, tokenIndex157 := position, tokenIndex
						if buffer[position] != rune('r') {
							goto l158
						}
						position++
						goto l157
					l158:
						position, tokenIndex = position157, tokenIndex157
						if buffer[position] != rune('R') {
							goto l150
						}
						position++
					}
				l157:
					{
						position159, tokenIndex159 := position, tokenIndex
						if buffer[position] != rune('d') {
							goto l160
						}
						position++
						goto l159
					l160:
						position, tokenIndex = position159, tokenIndex159
						if buffer[position] != rune('D') {
							goto l150
						}
						position++
					}
				l159:
					goto l149
				l150:
					position, tokenIndex = position149, tokenIndex149
					if buffer[position] != rune('.') {
						goto l161
					}
					position++
					{
						position162, tokenIndex162 := position, tokenIndex
						if buffer[position] != rune('w') {
							goto l163
						}
						position++
						goto l162
					l163:
						position, tokenIndex = position162, tokenIndex162
						if buffer[position] != rune('W') {
							goto l161
						}
						position++
					}
				l162:
					{
						position164, tokenIndex164 := position, tokenIndex
						if buffer[position] != rune('o') {
							goto l165
						}
						position++
						goto l164
					l165:
						position, tokenIndex = position164, tokenIndex164
						if buffer[position] != rune('O') {
							goto l161
						}
						position++
					}
				l164:
					{
						position166, tokenIndex166 := position, tokenIndex
						if buffer[position] != rune('r') {
							goto l167
						}
						position++
						goto l166
					l167:
						position, tokenIndex = position166, tokenIndex166
						if buffer[position] != rune('R') {
							goto l161
						}
						position++
					}
				l166:
					{
						position168, tokenIndex168 := position, tokenIndex
						if buffer[position] != rune('d') {
							goto l169
						}
						position++
						goto l168
					l169:
						position, tokenIndex = position168, tokenIndex168
						if buffer[position] != rune('D') {
							goto l161
						}
						position++
					}
				l168:
					goto l149
				l161:
					position, tokenIndex = position149, tokenIndex149
					if buffer[position] != rune('.') {
						goto l170
					}
					position++
					{
						position171, tokenIndex171 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l172
						}
						position++
						goto l171
					l172:
						position, tokenIndex = position171, tokenIndex171
						if buffer[position] != rune('L') {
							goto l170
						}
						position++
					}
				l171:
					{
						position173, tokenIndex173 := position, tokenIndex
						if buffer[position] != rune('o') {
							goto l174
						}
						position++
						goto l173
					l174:
						position, tokenIndex = position173, tokenIndex173
						if buffer[position] != rune('O') {
							goto l170
						}
						position++
					}
				l173:
					{
						position175, tokenIndex175 := position, tokenIndex
						if buffer[position] != rune('n') {
							goto l176
						}
						position++
						goto l175
					l176:
						position, tokenIndex = position175, tokenIndex175
						if buffer[position] != rune('N') {
							goto l170
						}
						position++
					}
				l175:
					{
						position177, tokenIndex177 := position, tokenIndex
						if buffer[position] != rune('g') {
							goto l178
						}
						position++
						goto l177
					l178:
						position, tokenIndex = position177, tokenIndex177
						if buffer[position] != rune('G') {
							goto l170
						}
						position++
					}
				l177:
					goto l149
				l170:
					position, tokenIndex = position149, tokenIndex149
					if buffer[position] != rune('.') {
						goto l179
					}
					position++
					{
						position180, tokenIndex180 := position, tokenIndex
						if buffer[position] != rune('s') {
							goto l181
						}
						position++
						goto l180
					l181:
						position, tokenIndex = position180, tokenIndex180
						if buffer[position] != rune('S') {
							goto l179
						}
						position++
					}
				l180:
					{
						position182, tokenIndex182 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l183
						}
						position++
						goto l182
					l183:
						position, tokenIndex = position182, tokenIndex182
						if buffer[position] != rune('E') {
							goto l179
						}
						position++
					}
				l182:
					{
						position184, tokenIndex184 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l185
						}
						position++
						goto l184
					l185:
						position, tokenIndex = position184, tokenIndex184
						if buffer[position] != rune('T') {
							goto l179
						}
						position++
					}
				l184:
					goto l149
				l179:
					position, tokenIndex = position149, tokenIndex149
					if buffer[position] != rune('.') {
						goto l186
					}
					position++
					{
						position187, tokenIndex187 := position, tokenIndex
						if buffer[position] != rune('b') {
							goto l188
						}
						position++
						goto l187
					l188:
						position, tokenIndex = position187, tokenIndex187
						if buffer[position] != rune('B') {
							goto l186
						}
						position++
					}
				l187:
					{
						position189, tokenIndex189 := position, tokenIndex
						if buffer[position] != rune('y') {
							goto l190
						}
						position++
						goto l189
					l190:
						position, tokenIndex = position189, tokenIndex189
						if buffer[position] != rune('Y') {
							goto l186
						}
						position++
					}
				l189:
					{
						position191, tokenIndex191 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l192
						}
						position++
						goto l191
					l192:
						position, tokenIndex = position191, tokenIndex191
						if buffer[position] != rune('T') {
							goto l186
						}
						position++
					}
				l191:
					{
						position193, tokenIndex193 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l194
						}
						position++
						goto l193
					l194:
						position, tokenIndex = position193, tokenIndex193
						if buffer[position] != rune('E') {
							goto l186
						}
						position++
					}
				l193:
					goto l149
				l186:
					position, tokenIndex = position149, tokenIndex149
					if buffer[position] != rune('.') {
						goto l195
					}
					position++
					if buffer[position] != rune('8') {
						goto l195
					}
					position++
					{
						position196, tokenIndex196 := position, tokenIndex
						if buffer[position] != rune('b') {
							goto l197
						}
						position++
						goto l196
					l197:
						position, tokenIndex = position196, tokenIndex196
						if buffer[position] != rune('B') {
							goto l195
						}
						position++
					}
				l196:
					{
						position198, tokenIndex198 := position, tokenIndex
						if buffer[position] != rune('y') {
							goto l199
						}
						position++
						goto l198
					l199:
						position, tokenIndex = position198, tokenIndex198
						if buffer[position] != rune('Y') {
							goto l195
						}
						position++
					}
				l198:
					{
						position200, tokenIndex200 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l201
						}
						position++
						goto l200
					l201:
						position, tokenIndex = position200, tokenIndex200
						if buffer[position] != rune('T') {
							goto l195
						}
						position++
					}
				l200:
					{
						position202, tokenIndex202 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l203
						}
						position++
						goto l202
					l203:
						position, tokenIndex = position202, tokenIndex202
						if buffer[position] != rune('E') {
							goto l195
						}
						position++
					}
				l202:
					goto l149
				l195:
					position, tokenIndex = position149, tokenIndex149
					if buffer[position] != rune('.') {
						goto l204
					}
					position++
					if buffer[position] != rune('4') {
						goto l204
					}
					position++
					{
						position205, tokenIndex205 := position, tokenIndex
						if buffer[position] != rune('b') {
							goto l206
						}
						position++
						goto l205
					l206:
						position, tokenIndex = position205, tokenIndex205
						if buffer[position] != rune('B') {
							goto l204
						}
						position++
					}
				l205:
					{
						position207, tokenIndex207 := position, tokenIndex
						if buffer[position] != rune('y') {
							goto l208
						}
						position++
						goto l207
					l208:
						position, tokenIndex = position207, tokenIndex207
						if buffer[position] != rune('Y') {
							goto l204
						}
						position++
					}
				l207:
					{
						position209, tokenIndex209 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l210
						}
						position++
						goto l209
					l210:
						position, tokenIndex = position209, tokenIndex209
						if buffer[position] != rune('T') {
							goto l204
						}
						position++
					}
				l209:
					{
						position211, tokenIndex211 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l212
						}
						position++
						goto l211
					l212:
						position, tokenIndex = position211, tokenIndex211
						if buffer[position] != rune('E') {
							goto l204
						}
						position++
					}
				l211:
					goto l149
				l204:
					position, tokenIndex = position149, tokenIndex149
					if buffer[position] != rune('.') {
						goto l213
					}
					position++
					{
						position214, tokenIndex214 := position, tokenIndex
						if buffer[position] != rune('q') {
							goto l215
						}
						position++
						goto l214
					l215:
						position, tokenIndex = position214, tokenIndex214
						if buffer[position] != rune('Q') {
							goto l213
						}
						position++
					}
				l214:
					{
						position216, tokenIndex216 := position, tokenIndex
						if buffer[position] != rune('u') {
							goto l217
						}
						position++
						goto l216
					l217:
						position, tokenIndex = position216, tokenIndex216
						if buffer[position] != rune('U') {
							goto l213
						}
						position++
					}
				l216:
					{
						position218, tokenIndex218 := position, tokenIndex
						if buffer[position] != rune('a') {
							goto l219
						}
						position++
						goto l218
					l219:
						position, tokenIndex = position218, tokenIndex218
						if buffer[position] != rune('A') {
							goto l213
						}
						position++
					}
				l218:
					{
						position220, tokenIndex220 := position, tokenIndex
						if buffer[position] != rune('d') {
							goto l221
						}
						position++
						goto l220
					l221:
						position, tokenIndex = position220, tokenIndex220
						if buffer[position] != rune('D') {
							goto l213
						}
						position++
					}
				l220:
					goto l149
				l213:
					position, tokenIndex = position149, tokenIndex149
					if buffer[position] != rune('.') {
						goto l222
					}
					position++
					{
						position223, tokenIndex223 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l224
						}
						position++
						goto l223
					l224:
						position, tokenIndex = position223, tokenIndex223
						if buffer[position] != rune('T') {
							goto l222
						}
						position++
					}
				l223:
					{
						position225, tokenIndex225 := position, tokenIndex
						if buffer[position] != rune('c') {
							goto l226
						}
						position++
						goto l225
					l226:
						position, tokenIndex = position225, tokenIndex225
						if buffer[position] != rune('C') {
							goto l222
						}
						position++
					}
				l225:
					goto l149
				l222:
					position, tokenIndex = position149, tokenIndex149
					if buffer[position] != rune('.') {
						goto l227
					}
					position++
					{
						position228, tokenIndex228 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l229
						}
						position++
						goto l228
					l229:
						position, tokenIndex = position228, tokenIndex228
						if buffer[position] != rune('L') {
							goto l227
						}
						position++
					}
				l228:
					{
						position230, tokenIndex230 := position, tokenIndex
						if buffer[position] != rune('o') {
							goto l231
						}
						position++
						goto l230
					l231:
						position, tokenIndex = position230, tokenIndex230
						if buffer[position] != rune('O') {
							goto l227
						}
						position++
					}
				l230:
					{
						position232, tokenIndex232 := position, tokenIndex
						if buffer[position] != rune('c') {
							goto l233
						}
						position++
						goto l232
					l233:
						position, tokenIndex = position232, tokenIndex232
						if buffer[position] != rune('C') {
							goto l227
						}
						position++
					}
				l232:
					{
						position234, tokenIndex234 := position, tokenIndex
						if buffer[position] != rune('a') {
							goto l235
						}
						position++
						goto l234
					l235:
						position, tokenIndex = position234, tokenIndex234
						if buffer[position] != rune('A') {
							goto l227
						}
						position++
					}
				l234:
					{
						position236, tokenIndex236 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l237
						}
						position++
						goto l236
					l237:
						position, tokenIndex = position236, tokenIndex236
						if buffer[position] != rune('L') {
							goto l227
						}
						position++
					}
				l236:
					{
						position238, tokenIndex238 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l239
						}
						position++
						goto l238
					l239:
						position, tokenIndex = position238, tokenIndex238
						if buffer[position] != rune('E') {
							goto l227
						}
						position++
					}
				l238:
					{
						position240, tokenIndex240 := position, tokenIndex
						if buffer[position] != rune('n') {
							goto l241
						}
						position++
						goto l240
					l241:
						position, tokenIndex = position240, tokenIndex240
						if buffer[position] != rune('N') {
							goto l227
						}
						position++
					}
				l240:
					{
						position242, tokenIndex242 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l243
						}
						position++
						goto l242
					l243:
						position, tokenIndex = position242, tokenIndex242
						if buffer[position] != rune('T') {
							goto l227
						}
						position++
					}
				l242:
					{
						position244, tokenIndex244 := position, tokenIndex
						if buffer[position] != rune('r') {
							goto l245
						}
						position++
						goto l244
					l245:
						position, tokenIndex = position244, tokenIndex244
						if buffer[position] != rune('R') {
							goto l227
						}
						position++
					}
				l244:
					{
						position246, tokenIndex246 := position, tokenIndex
						if buffer[position] != rune('y') {
							goto l247
						}
						position++
						goto l246
					l247:
						position, tokenIndex = position246, tokenIndex246
						if buffer[position] != rune('Y') {
							goto l227
						}
						position++
					}
				l246:
					goto l149
				l227:
					position, tokenIndex = position149, tokenIndex149
					if buffer[position] != rune('.') {
						goto l248
					}
					position++
					{
						position249, tokenIndex249 := position, tokenIndex
						if buffer[position] != rune('s') {
							goto l250
						}
						position++
						goto l249
					l250:
						position, tokenIndex = position249, tokenIndex249
						if buffer[position] != rune('S') {
							goto l248
						}
						position++
					}
				l249:
					{
						position251, tokenIndex251 := position, tokenIndex
						if buffer[position] != rune('i') {
							goto l252
						}
						position++
						goto l251
					l252:
						position, tokenIndex = position251, tokenIndex251
						if buffer[position] != rune('I') {
							goto l248
						}
						position++
					}
				l251:
					{
						position253, tokenIndex253 := position, tokenIndex
						if buffer[position] != rune('z') {
							goto l254
						}
						position++
						goto l253
					l254:
						position, tokenIndex = position253, tokenIndex253
						if buffer[position] != rune('Z') {
							goto l248
						}
						position++
					}
				l253:
					{
						position255, tokenIndex255 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l256
						}
						position++
						goto l255
					l256:
						position, tokenIndex = position255, tokenIndex255
						if buffer[position] != rune('E') {
							goto l248
						}
						position++
					}
				l255:
					goto l149
				l248:
					position, tokenIndex = position149, tokenIndex149
					if buffer[position] != rune('.') {
						goto l257
					}
					position++
					{
						position258, tokenIndex258 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l259
						}
						position++
						goto l258
					l259:
						position, tokenIndex = position258, tokenIndex258
						if buffer[position] != rune('T') {
							goto l257
						}
						position++
					}
				l258:
					{
						position260, tokenIndex260 := position, tokenIndex
						if buffer[position] != rune('y') {
							goto l261
						}
						position++
						goto l260
					l261:
						position, tokenIndex = position260, tokenIndex260
						if buffer[position] != rune('Y') {
							goto l257
						}
						position++
					}
				l260:
					{
						position262, tokenIndex262 := position, tokenIndex
						if buffer[position] != rune('p') {
							goto l263
						}
						position++
						goto l262
					l263:
						position, tokenIndex = position262, tokenIndex262
						if buffer[position] != rune('P') {
							goto l257
						}
						position++
					}
				l262:
					{
						position264, tokenIndex264 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l265
						}
						position++
						goto l264
					l265:
						position, tokenIndex = position264, tokenIndex264
						if buffer[position] != rune('E') {
							goto l257
						}
						position++
					}
				l264:
					goto l149
				l257:
					position, tokenIndex = position149, tokenIndex149
					if buffer[position] != rune('.') {
						goto l266
					}
					position++
					{
						position267, tokenIndex267 := position, tokenIndex
						if buffer[position] != rune('u') {
							goto l268
						}
						position++
						goto l267
					l268:
						position, tokenIndex = position267, tokenIndex267
						if buffer[position] != rune('U') {
							goto l266
						}
						position++
					}
				l267:
					{
						position269, tokenIndex269 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l270
						}
						position++
						goto l269
					l270:
						position, tokenIndex = position269, tokenIndex269
						if buffer[position] != rune('L') {
							goto l266
						}
						position++
					}
				l269:
					{
						position271, tokenIndex271 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l272
						}
						position++
						goto l271
					l272:
						position, tokenIndex = position271, tokenIndex271
						if buffer[position] != rune('E') {
							goto l266
						}
						position++
					}
				l271:
					{
						position273, tokenIndex273 := position, tokenIndex
						if buffer[position] != rune('b') {
							goto l274
						}
						position++
						goto l273
					l274:
						position, tokenIndex = position273, tokenIndex273
						if buffer[position] != rune('B') {
							goto l266
						}
						position++
					}
				l273:
					if buffer[position] != rune('1') {
						goto l266
					}
					position++
					if buffer[position] != rune('2') {
						goto l266
					}
					position++
					if buffer[position] != rune('8') {
						goto l266
					}
					position++
					goto l149
				l266:
					position, tokenIndex = position149, tokenIndex149
					if buffer[position] != rune('.') {
						goto l147
					}
					position++
					{
						position275, tokenIndex275 := position, tokenIndex
						if buffer[position] != rune('s') {
							goto l276
						}
						position++
						goto l275
					l276:
						position, tokenIndex = position275, tokenIndex275
						if buffer[position] != rune('S') {
							goto l147
						}
						position++
					}
				l275:
					{
						position277, tokenIndex277 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l278
						}
						position++
						goto l277
					l278:
						position, tokenIndex = position277, tokenIndex277
						if buffer[position] != rune('L') {
							goto l147
						}
						position++
					}
				l277:
					{
						position279, tokenIndex279 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l280
						}
						position++
						goto l279
					l280:
						position, tokenIndex = position279, tokenIndex279
						if buffer[position] != rune('E') {
							goto l147
						}
						position++
					}
				l279:
					{
						position281, tokenIndex281 := position, tokenIndex
						if buffer[position] != rune('b') {
							goto l282
						}
						position++
						goto l281
					l282:
						position, tokenIndex = position281, tokenIndex281
						if buffer[position] != rune('B') {
							goto l147
						}
						position++
					}
				l281:
					if buffer[position] != rune('1') {
						goto l147
					}
					position++
					if buffer[position] != rune('2') {
						goto l147
					}
					position++
					if buffer[position] != rune('8') {
						goto l147
					}
					position++
				}
			l149:
				add(ruleLabelContainingDirectiveName, position148)
			}
			return true
		l147:
			position, tokenIndex = position147, tokenIndex147
			return false
		},
		/* 14 SymbolArgs <- <(SymbolArg (WS? ',' WS? SymbolArg)*)> */
		func() bool {
			position283, tokenIndex283 := position, tokenIndex
			{
				position284 := position
				if !_rules[ruleSymbolArg]() {
					goto l283
				}
			l285:
				{
					position286, tokenIndex286 := position, tokenIndex
					{
						position287, tokenIndex287 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l287
						}
						goto l288
					l287:
						position, tokenIndex = position287, tokenIndex287
					}
				l288:
					if buffer[position] != rune(',') {
						goto l286
					}
					position++
					{
						position289, tokenIndex289 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l289
						}
						goto l290
					l289:
						position, tokenIndex = position289, tokenIndex289
					}
				l290:
					if !_rules[ruleSymbolArg]() {
						goto l286
					}
					goto l285
				l286:
					position, tokenIndex = position286, tokenIndex286
				}
				add(ruleSymbolArgs, position284)
			}
			return true
		l283:
			position, tokenIndex = position283, tokenIndex283
			return false
		},
		/* 15 SymbolShift <- <((('<' '<') / ('>' '>')) WS? [0-9]+)> */
		func() bool {
			position291, tokenIndex291 := position, tokenIndex
			{
				position292 := position
				{
					position293, tokenIndex293 := position, tokenIndex
					if buffer[position] != rune('<') {
						goto l294
					}
					position++
					if buffer[position] != rune('<') {
						goto l294
					}
					position++
					goto l293
				l294:
					position, tokenIndex = position293, tokenIndex293
					if buffer[position] != rune('>') {
						goto l291
					}
					position++
					if buffer[position] != rune('>') {
						goto l291
					}
					position++
				}
			l293:
				{
					position295, tokenIndex295 := position, tokenIndex
					if !_rules[ruleWS]() {
						goto l295
					}
					goto l296
				l295:
					position, tokenIndex = position295, tokenIndex295
				}
			l296:
				if c := buffer[position]; c < rune('0') || c > rune('9') {
					goto l291
				}
				position++
			l297:
				{
					position298, tokenIndex298 := position, tokenIndex
					if c := buffer[position]; c < rune('0') || c > rune('9') {
						goto l298
					}
					position++
					goto l297
				l298:
					position, tokenIndex = position298, tokenIndex298
				}
				add(ruleSymbolShift, position292)
			}
			return true
		l291:
			position, tokenIndex = position291, tokenIndex291
			return false
		},
		/* 16 SymbolArg <- <((OpenParen WS?)? (Offset / SymbolType / ((Offset / LocalSymbol / SymbolName / Dot) WS? Operator WS? (Offset / LocalSymbol / SymbolName)) / (LocalSymbol TCMarker?) / (SymbolName Offset) / (SymbolName TCMarker?)) (WS? CloseParen)? (WS? SymbolShift)?)> */
		func() bool {
			position299, tokenIndex299 := position, tokenIndex
			{
				position300 := position
				{
					position301, tokenIndex301 := position, tokenIndex
					if !_rules[ruleOpenParen]() {
						goto l301
					}
					{
						position303, tokenIndex303 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l303
						}
						goto l304
					l303:
						position, tokenIndex = position303, tokenIndex303
					}
				l304:
					goto l302
				l301:
					position, tokenIndex = position301, tokenIndex301
				}
			l302:
				{
					position305, tokenIndex305 := position, tokenIndex
					if !_rules[ruleOffset]() {
						goto l306
					}
					goto l305
				l306:
					position, tokenIndex = position305, tokenIndex305
					if !_rules[ruleSymbolType]() {
						goto l307
					}
					goto l305
				l307:
					position, tokenIndex = position305, tokenIndex305
					{
						position309, tokenIndex309 := position, tokenIndex
						if !_rules[ruleOffset]() {
							goto l310
						}
						goto l309
					l310:
						position, tokenIndex = position309, tokenIndex309
						if !_rules[ruleLocalSymbol]() {
							goto l311
						}
						goto l309
					l311:
						position, tokenIndex = position309, tokenIndex309
						if !_rules[ruleSymbolName]() {
							goto l312
						}
						goto l309
					l312:
						position, tokenIndex = position309, tokenIndex309
						if !_rules[ruleDot]() {
							goto l308
						}
					}
				l309:
					{
						position313, tokenIndex313 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l313
						}
						goto l314
					l313:
						position, tokenIndex = position313, tokenIndex313
					}
				l314:
					if !_rules[ruleOperator]() {
						goto l308
					}
					{
						position315, tokenIndex315 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l315
						}
						goto l316
					l315:
						position, tokenIndex = position315, tokenIndex315
					}
				l316:
					{
						position317, tokenIndex317 := position, tokenIndex
						if !_rules[ruleOffset]() {
							goto l318
						}
						goto l317
					l318:
						position, tokenIndex = position317, tokenIndex317
						if !_rules[ruleLocalSymbol]() {
							goto l319
						}
						goto l317
					l319:
						position, tokenIndex = position317, tokenIndex317
						if !_rules[ruleSymbolName]() {
							goto l308
						}
					}
				l317:
					goto l305
				l308:
					position, tokenIndex = position305, tokenIndex305
					if !_rules[ruleLocalSymbol]() {
						goto l320
					}
					{
						position321, tokenIndex321 := position, tokenIndex
						if !_rules[ruleTCMarker]() {
							goto l321
						}
						goto l322
					l321:
						position, tokenIndex = position321, tokenIndex321
					}
				l322:
					goto l305
				l320:
					position, tokenIndex = position305, tokenIndex305
					if !_rules[ruleSymbolName]() {
						goto l323
					}
					if !_rules[ruleOffset]() {
						goto l323
					}
					goto l305
				l323:
					position, tokenIndex = position305, tokenIndex305
					if !_rules[ruleSymbolName]() {
						goto l299
					}
					{
						position324, tokenIndex324 := position, tokenIndex
						if !_rules[ruleTCMarker]() {
							goto l324
						}
						goto l325
					l324:
						position, tokenIndex = position324, tokenIndex324
					}
				l325:
				}
			l305:
				{
					position326, tokenIndex326 := position, tokenIndex
					{
						position328, tokenIndex328 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l328
						}
						goto l329
					l328:
						position, tokenIndex = position328, tokenIndex328
					}
				l329:
					if !_rules[ruleCloseParen]() {
						goto l326
					}
					goto l327
				l326:
					position, tokenIndex = position326, tokenIndex326
				}
			l327:
				{
					position330, tokenIndex330 := position, tokenIndex
					{
						position332, tokenIndex332 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l332
						}
						goto l333
					l332:
						position, tokenIndex = position332, tokenIndex332
					}
				l333:
					if !_rules[ruleSymbolShift]() {
						goto l330
					}
					goto l331
				l330:
					position, tokenIndex = position330, tokenIndex330
				}
			l331:
				add(ruleSymbolArg, position300)
			}
			return true
		l299:
			position, tokenIndex = position299, tokenIndex299
			return false
		},
		/* 17 OpenParen <- <'('> */
		func() bool {
			position334, tokenIndex334 := position, tokenIndex
			{
				position335 := position
				if buffer[position] != rune('(') {
					goto l334
				}
				position++
				add(ruleOpenParen, position335)
			}
			return true
		l334:
			position, tokenIndex = position334, tokenIndex334
			return false
		},
		/* 18 CloseParen <- <')'> */
		func() bool {
			position336, tokenIndex336 := position, tokenIndex
			{
				position337 := position
				if buffer[position] != rune(')') {
					goto l336
				}
				position++
				add(ruleCloseParen, position337)
			}
			return true
		l336:
			position, tokenIndex = position336, tokenIndex336
			return false
		},
		/* 19 SymbolType <- <(('@' / '%') (('f' 'u' 'n' 'c' 't' 'i' 'o' 'n') / ('o' 'b' 'j' 'e' 'c' 't')))> */
		func() bool {
			position338, tokenIndex338 := position, tokenIndex
			{
				position339 := position
				{
					position340, tokenIndex340 := position, tokenIndex
					if buffer[position] != rune('@') {
						goto l341
					}
					position++
					goto l340
				l341:
					position, tokenIndex = position340, tokenIndex340
					if buffer[position] != rune('%') {
						goto l338
					}
					position++
				}
			l340:
				{
					position342, tokenIndex342 := position, tokenIndex
					if buffer[position] != rune('f') {
						goto l343
					}
					position++
					if buffer[position] != rune('u') {
						goto l343
					}
					position++
					if buffer[position] != rune('n') {
						goto l343
					}
					position++
					if buffer[position] != rune('c') {
						goto l343
					}
					position++
					if buffer[position] != rune('t') {
						goto l343
					}
					position++
					if buffer[position] != rune('i') {
						goto l343
					}
					position++
					if buffer[position] != rune('o') {
						goto l343
					}
					position++
					if buffer[position] != rune('n') {
						goto l343
					}
					position++
					goto l342
				l343:
					position, tokenIndex = position342, tokenIndex342
					if buffer[position] != rune('o') {
						goto l338
					}
					position++
					if buffer[position] != rune('b') {
						goto l338
					}
					position++
					if buffer[position] != rune('j') {
						goto l338
					}
					position++
					if buffer[position] != rune('e') {
						goto l338
					}
					position++
					if buffer[position] != rune('c') {
						goto l338
					}
					position++
					if buffer[position] != rune('t') {
						goto l338
					}
					position++
				}
			l342:
				add(ruleSymbolType, position339)
			}
			return true
		l338:
			position, tokenIndex = position338, tokenIndex338
			return false
		},
		/* 20 Dot <- <'.'> */
		func() bool {
			position344, tokenIndex344 := position, tokenIndex
			{
				position345 := position
				if buffer[position] != rune('.') {
					goto l344
				}
				position++
				add(ruleDot, position345)
			}
			return true
		l344:
			position, tokenIndex = position344, tokenIndex344
			return false
		},
		/* 21 TCMarker <- <('[' 'T' 'C' ']')> */
		func() bool {
			position346, tokenIndex346 := position, tokenIndex
			{
				position347 := position
				if buffer[position] != rune('[') {
					goto l346
				}
				position++
				if buffer[position] != rune('T') {
					goto l346
				}
				position++
				if buffer[position] != rune('C') {
					goto l346
				}
				position++
				if buffer[position] != rune(']') {
					goto l346
				}
				position++
				add(ruleTCMarker, position347)
			}
			return true
		l346:
			position, tokenIndex = position346, tokenIndex346
			return false
		},
		/* 22 EscapedChar <- <('\\' .)> */
		func() bool {
			position348, tokenIndex348 := position, tokenIndex
			{
				position349 := position
				if buffer[position] != rune('\\') {
					goto l348
				}
				position++
				if !matchDot() {
					goto l348
				}
				add(ruleEscapedChar, position349)
			}
			return true
		l348:
			position, tokenIndex = position348, tokenIndex348
			return false
		},
		/* 23 WS <- <(' ' / '\t')+> */
		func() bool {
			position350, tokenIndex350 := position, tokenIndex
			{
				position351 := position
				{
					position354, tokenIndex354 := position, tokenIndex
					if buffer[position] != rune(' ') {
						goto l355
					}
					position++
					goto l354
				l355:
					position, tokenIndex = position354, tokenIndex354
					if buffer[position] != rune('\t') {
						goto l350
					}
					position++
				}
			l354:
			l352:
				{
					position353, tokenIndex353 := position, tokenIndex
					{
						position356, tokenIndex356 := position, tokenIndex
						if buffer[position] != rune(' ') {
							goto l357
						}
						position++
						goto l356
					l357:
						position, tokenIndex = position356, tokenIndex356
						if buffer[position] != rune('\t') {
							goto l353
						}
						position++
					}
				l356:
					goto l352
				l353:
					position, tokenIndex = position353, tokenIndex353
				}
				add(ruleWS, position351)
			}
			return true
		l350:
			position, tokenIndex = position350, tokenIndex350
			return false
		},
		/* 24 Comment <- <((('/' '/') / '#') (!'\n' .)*)> */
		func() bool {
			position358, tokenIndex358 := position, tokenIndex
			{
				position359 := position
				{
					position360, tokenIndex360 := position, tokenIndex
					if buffer[position] != rune('/') {
						goto l361
					}
					position++
					if buffer[position] != rune('/') {
						goto l361
					}
					position++
					goto l360
				l361:
					position, tokenIndex = position360, tokenIndex360
					if buffer[position] != rune('#') {
						goto l358
					}
					position++
				}
			l360:
			l362:
				{
					position363, tokenIndex363 := position, tokenIndex
					{
						position364, tokenIndex364 := position, tokenIndex
						if buffer[position] != rune('\n') {
							goto l364
						}
						position++
						goto l363
					l364:
						position, tokenIndex = position364, tokenIndex364
					}
					if !matchDot() {
						goto l363
					}
					goto l362
				l363:
					position, tokenIndex = position363, tokenIndex363
				}
				add(ruleComment, position359)
			}
			return true
		l358:
			position, tokenIndex = position358, tokenIndex358
			return false
		},
		/* 25 Label <- <((LocalSymbol / LocalLabel / SymbolName) ':')> */
		func() bool {
			position365, tokenIndex365 := position, tokenIndex
			{
				position366 := position
				{
					position367, tokenIndex367 := position, tokenIndex
					if !_rules[ruleLocalSymbol]() {
						goto l368
					}
					goto l367
				l368:
					position, tokenIndex = position367, tokenIndex367
					if !_rules[ruleLocalLabel]() {
						goto l369
					}
					goto l367
				l369:
					position, tokenIndex = position367, tokenIndex367
					if !_rules[ruleSymbolName]() {
						goto l365
					}
				}
			l367:
				if buffer[position] != rune(':') {
					goto l365
				}
				position++
				add(ruleLabel, position366)
			}
			return true
		l365:
			position, tokenIndex = position365, tokenIndex365
			return false
		},
		/* 26 SymbolName <- <(([a-z] / [A-Z] / '.' / '_') ([a-z] / [A-Z] / '.' / ([0-9] / [0-9]) / '$' / '_')*)> */
		func() bool {
			position370, tokenIndex370 := position, tokenIndex
			{
				position371 := position
				{
					position372, tokenIndex372 := position, tokenIndex
					if c := buffer[position]; c < rune('a') || c > rune('z') {
						goto l373
					}
					position++
					goto l372
				l373:
					position, tokenIndex = position372, tokenIndex372
					if c := buffer[position]; c < rune('A') || c > rune('Z') {
						goto l374
					}
					position++
					goto l372
				l374:
					position, tokenIndex = position372, tokenIndex372
					if buffer[position] != rune('.') {
						goto l375
					}
					position++
					goto l372
				l375:
					position, tokenIndex = position372, tokenIndex372
					if buffer[position] != rune('_') {
						goto l370
					}
					position++
				}
			l372:
			l376:
				{
					position377, tokenIndex377 := position, tokenIndex
					{
						position378, tokenIndex378 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l379
						}
						position++
						goto l378
					l379:
						position, tokenIndex = position378, tokenIndex378
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l380
						}
						position++
						goto l378
					l380:
						position, tokenIndex = position378, tokenIndex378
						if buffer[position] != rune('.') {
							goto l381
						}
						position++
						goto l378
					l381:
						position, tokenIndex = position378, tokenIndex378
						{
							position383, tokenIndex383 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l384
							}
							position++
							goto l383
						l384:
							position, tokenIndex = position383, tokenIndex383
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l382
							}
							position++
						}
					l383:
						goto l378
					l382:
						position, tokenIndex = position378, tokenIndex378
						if buffer[position] != rune('$') {
							goto l385
						}
						position++
						goto l378
					l385:
						position, tokenIndex = position378, tokenIndex378
						if buffer[position] != rune('_') {
							goto l377
						}
						position++
					}
				l378:
					goto l376
				l377:
					position, tokenIndex = position377, tokenIndex377
				}
				add(ruleSymbolName, position371)
			}
			return true
		l370:
			position, tokenIndex = position370, tokenIndex370
			return false
		},
		/* 27 LocalSymbol <- <('.' 'L' ([a-z] / [A-Z] / ([a-z] / [A-Z]) / '.' / ([0-9] / [0-9]) / '$' / '_')+)> */
		func() bool {
			position386, tokenIndex386 := position, tokenIndex
			{
				position387 := position
				if buffer[position] != rune('.') {
					goto l386
				}
				position++
				if buffer[position] != rune('L') {
					goto l386
				}
				position++
				{
					position390, tokenIndex390 := position, tokenIndex
					if c := buffer[position]; c < rune('a') || c > rune('z') {
						goto l391
					}
					position++
					goto l390
				l391:
					position, tokenIndex = position390, tokenIndex390
					if c := buffer[position]; c < rune('A') || c > rune('Z') {
						goto l392
					}
					position++
					goto l390
				l392:
					position, tokenIndex = position390, tokenIndex390
					{
						position394, tokenIndex394 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l395
						}
						position++
						goto l394
					l395:
						position, tokenIndex = position394, tokenIndex394
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l393
						}
						position++
					}
				l394:
					goto l390
				l393:
					position, tokenIndex = position390, tokenIndex390
					if buffer[position] != rune('.') {
						goto l396
					}
					position++
					goto l390
				l396:
					position, tokenIndex = position390, tokenIndex390
					{
						position398, tokenIndex398 := position, tokenIndex
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l399
						}
						position++
						goto l398
					l399:
						position, tokenIndex = position398, tokenIndex398
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l397
						}
						position++
					}
				l398:
					goto l390
				l397:
					position, tokenIndex = position390, tokenIndex390
					if buffer[position] != rune('$') {
						goto l400
					}
					position++
					goto l390
				l400:
					position, tokenIndex = position390, tokenIndex390
					if buffer[position] != rune('_') {
						goto l386
					}
					position++
				}
			l390:
			l388:
				{
					position389, tokenIndex389 := position, tokenIndex
					{
						position401, tokenIndex401 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l402
						}
						position++
						goto l401
					l402:
						position, tokenIndex = position401, tokenIndex401
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l403
						}
						position++
						goto l401
					l403:
						position, tokenIndex = position401, tokenIndex401
						{
							position405, tokenIndex405 := position, tokenIndex
							if c := buffer[position]; c < rune('a') || c > rune('z') {
								goto l406
							}
							position++
							goto l405
						l406:
							position, tokenIndex = position405, tokenIndex405
							if c := buffer[position]; c < rune('A') || c > rune('Z') {
								goto l404
							}
							position++
						}
					l405:
						goto l401
					l404:
						position, tokenIndex = position401, tokenIndex401
						if buffer[position] != rune('.') {
							goto l407
						}
						position++
						goto l401
					l407:
						position, tokenIndex = position401, tokenIndex401
						{
							position409, tokenIndex409 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l410
							}
							position++
							goto l409
						l410:
							position, tokenIndex = position409, tokenIndex409
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l408
							}
							position++
						}
					l409:
						goto l401
					l408:
						position, tokenIndex = position401, tokenIndex401
						if buffer[position] != rune('$') {
							goto l411
						}
						position++
						goto l401
					l411:
						position, tokenIndex = position401, tokenIndex401
						if buffer[position] != rune('_') {
							goto l389
						}
						position++
					}
				l401:
					goto l388
				l389:
					position, tokenIndex = position389, tokenIndex389
				}
				add(ruleLocalSymbol, position387)
			}
			return true
		l386:
			position, tokenIndex = position386, tokenIndex386
			return false
		},
		/* 28 LocalLabel <- <([0-9] ([0-9] / '$')*)> */
		func() bool {
			position412, tokenIndex412 := position, tokenIndex
			{
				position413 := position
				if c := buffer[position]; c < rune('0') || c > rune('9') {
					goto l412
				}
				position++
			l414:
				{
					position415, tokenIndex415 := position, tokenIndex
					{
						position416, tokenIndex416 := position, tokenIndex
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l417
						}
						position++
						goto l416
					l417:
						position, tokenIndex = position416, tokenIndex416
						if buffer[position] != rune('$') {
							goto l415
						}
						position++
					}
				l416:
					goto l414
				l415:
					position, tokenIndex = position415, tokenIndex415
				}
				add(ruleLocalLabel, position413)
			}
			return true
		l412:
			position, tokenIndex = position412, tokenIndex412
			return false
		},
		/* 29 LocalLabelRef <- <([0-9] ([0-9] / '$')* ('b' / 'f'))> */
		func() bool {
			position418, tokenIndex418 := position, tokenIndex
			{
				position419 := position
				if c := buffer[position]; c < rune('0') || c > rune('9') {
					goto l418
				}
				position++
			l420:
				{
					position421, tokenIndex421 := position, tokenIndex
					{
						position422, tokenIndex422 := position, tokenIndex
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l423
						}
						position++
						goto l422
					l423:
						position, tokenIndex = position422, tokenIndex422
						if buffer[position] != rune('$') {
							goto l421
						}
						position++
					}
				l422:
					goto l420
				l421:
					position, tokenIndex = position421, tokenIndex421
				}
				{
					position424, tokenIndex424 := position, tokenIndex
					if buffer[position] != rune('b') {
						goto l425
					}
					position++
					goto l424
				l425:
					position, tokenIndex = position424, tokenIndex424
					if buffer[position] != rune('f') {
						goto l418
					}
					position++
				}
			l424:
				add(ruleLocalLabelRef, position419)
			}
			return true
		l418:
			position, tokenIndex = position418, tokenIndex418
			return false
		},
		/* 30 Instruction <- <(InstructionName (WS InstructionArg (WS? ',' WS? InstructionArg)*)?)> */
		func() bool {
			position426, tokenIndex426 := position, tokenIndex
			{
				position427 := position
				if !_rules[ruleInstructionName]() {
					goto l426
				}
				{
					position428, tokenIndex428 := position, tokenIndex
					if !_rules[ruleWS]() {
						goto l428
					}
					if !_rules[ruleInstructionArg]() {
						goto l428
					}
				l430:
					{
						position431, tokenIndex431 := position, tokenIndex
						{
							position432, tokenIndex432 := position, tokenIndex
							if !_rules[ruleWS]() {
								goto l432
							}
							goto l433
						l432:
							position, tokenIndex = position432, tokenIndex432
						}
					l433:
						if buffer[position] != rune(',') {
							goto l431
						}
						position++
						{
							position434, tokenIndex434 := position, tokenIndex
							if !_rules[ruleWS]() {
								goto l434
							}
							goto l435
						l434:
							position, tokenIndex = position434, tokenIndex434
						}
					l435:
						if !_rules[ruleInstructionArg]() {
							goto l431
						}
						goto l430
					l431:
						position, tokenIndex = position431, tokenIndex431
					}
					goto l429
				l428:
					position, tokenIndex = position428, tokenIndex428
				}
			l429:
				add(ruleInstruction, position427)
			}
			return true
		l426:
			position, tokenIndex = position426, tokenIndex426
			return false
		},
		/* 31 InstructionName <- <(([a-z] / [A-Z]) ([a-z] / [A-Z] / '.' / ([0-9] / [0-9]))* ('.' / '+' / '-')?)> */
		func() bool {
			position436, tokenIndex436 := position, tokenIndex
			{
				position437 := position
				{
					position438, tokenIndex438 := position, tokenIndex
					if c := buffer[position]; c < rune('a') || c > rune('z') {
						goto l439
					}
					position++
					goto l438
				l439:
					position, tokenIndex = position438, tokenIndex438
					if c := buffer[position]; c < rune('A') || c > rune('Z') {
						goto l436
					}
					position++
				}
			l438:
			l440:
				{
					position441, tokenIndex441 := position, tokenIndex
					{
						position442, tokenIndex442 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l443
						}
						position++
						goto l442
					l443:
						position, tokenIndex = position442, tokenIndex442
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l444
						}
						position++
						goto l442
					l444:
						position, tokenIndex = position442, tokenIndex442
						if buffer[position] != rune('.') {
							goto l445
						}
						position++
						goto l442
					l445:
						position, tokenIndex = position442, tokenIndex442
						{
							position446, tokenIndex446 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l447
							}
							position++
							goto l446
						l447:
							position, tokenIndex = position446, tokenIndex446
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l441
							}
							position++
						}
					l446:
					}
				l442:
					goto l440
				l441:
					position, tokenIndex = position441, tokenIndex441
				}
				{
					position448, tokenIndex448 := position, tokenIndex
					{
						position450, tokenIndex450 := position, tokenIndex
						if buffer[position] != rune('.') {
							goto l451
						}
						position++
						goto l450
					l451:
						position, tokenIndex = position450, tokenIndex450
						if buffer[position] != rune('+') {
							goto l452
						}
						position++
						goto l450
					l452:
						position, tokenIndex = position450, tokenIndex450
						if buffer[position] != rune('-') {
							goto l448
						}
						position++
					}
				l450:
					goto l449
				l448:
					position, tokenIndex = position448, tokenIndex448
				}
			l449:
				add(ruleInstructionName, position437)
			}
			return true
		l436:
			position, tokenIndex = position436, tokenIndex436
			return false
		},
		/* 32 InstructionArg <- <(IndirectionIndicator? (ARMConstantTweak / RegisterOrConstant / LocalLabelRef / TOCRefHigh / TOCRefLow / GOTLocation / GOTSymbolOffset / MemoryRef) AVX512Token*)> */
		func() bool {
			position453, tokenIndex453 := position, tokenIndex
			{
				position454 := position
				{
					position455, tokenIndex455 := position, tokenIndex
					if !_rules[ruleIndirectionIndicator]() {
						goto l455
					}
					goto l456
				l455:
					position, tokenIndex = position455, tokenIndex455
				}
			l456:
				{
					position457, tokenIndex457 := position, tokenIndex
					if !_rules[ruleARMConstantTweak]() {
						goto l458
					}
					goto l457
				l458:
					position, tokenIndex = position457, tokenIndex457
					if !_rules[ruleRegisterOrConstant]() {
						goto l459
					}
					goto l457
				l459:
					position, tokenIndex = position457, tokenIndex457
					if !_rules[ruleLocalLabelRef]() {
						goto l460
					}
					goto l457
				l460:
					position, tokenIndex = position457, tokenIndex457
					if !_rules[ruleTOCRefHigh]() {
						goto l461
					}
					goto l457
				l461:
					position, tokenIndex = position457, tokenIndex457
					if !_rules[ruleTOCRefLow]() {
						goto l462
					}
					goto l457
				l462:
					position, tokenIndex = position457, tokenIndex457
					if !_rules[ruleGOTLocation]() {
						goto l463
					}
					goto l457
				l463:
					position, tokenIndex = position457, tokenIndex457
					if !_rules[ruleGOTSymbolOffset]() {
						goto l464
					}
					goto l457
				l464:
					position, tokenIndex = position457, tokenIndex457
					if !_rules[ruleMemoryRef]() {
						goto l453
					}
				}
			l457:
			l465:
				{
					position466, tokenIndex466 := position, tokenIndex
					if !_rules[ruleAVX512Token]() {
						goto l466
					}
					goto l465
				l466:
					position, tokenIndex = position466, tokenIndex466
				}
				add(ruleInstructionArg, position454)
			}
			return true
		l453:
			position, tokenIndex = position453, tokenIndex453
			return false
		},
		/* 33 GOTLocation <- <('$' '_' 'G' 'L' 'O' 'B' 'A' 'L' '_' 'O' 'F' 'F' 'S' 'E' 'T' '_' 'T' 'A' 'B' 'L' 'E' '_' '-' LocalSymbol)> */
		func() bool {
			position467, tokenIndex467 := position, tokenIndex
			{
				position468 := position
				if buffer[position] != rune('$') {
					goto l467
				}
				position++
				if buffer[position] != rune('_') {
					goto l467
				}
				position++
				if buffer[position] != rune('G') {
					goto l467
				}
				position++
				if buffer[position] != rune('L') {
					goto l467
				}
				position++
				if buffer[position] != rune('O') {
					goto l467
				}
				position++
				if buffer[position] != rune('B') {
					goto l467
				}
				position++
				if buffer[position] != rune('A') {
					goto l467
				}
				position++
				if buffer[position] != rune('L') {
					goto l467
				}
				position++
				if buffer[position] != rune('_') {
					goto l467
				}
				position++
				if buffer[position] != rune('O') {
					goto l467
				}
				position++
				if buffer[position] != rune('F') {
					goto l467
				}
				position++
				if buffer[position] != rune('F') {
					goto l467
				}
				position++
				if buffer[position] != rune('S') {
					goto l467
				}
				position++
				if buffer[position] != rune('E') {
					goto l467
				}
				position++
				if buffer[position] != rune('T') {
					goto l467
				}
				position++
				if buffer[position] != rune('_') {
					goto l467
				}
				position++
				if buffer[position] != rune('T') {
					goto l467
				}
				position++
				if buffer[position] != rune('A') {
					goto l467
				}
				position++
				if buffer[position] != rune('B') {
					goto l467
				}
				position++
				if buffer[position] != rune('L') {
					goto l467
				}
				position++
				if buffer[position] != rune('E') {
					goto l467
				}
				position++
				if buffer[position] != rune('_') {
					goto l467
				}
				position++
				if buffer[position] != rune('-') {
					goto l467
				}
				position++
				if !_rules[ruleLocalSymbol]() {
					goto l467
				}
				add(ruleGOTLocation, position468)
			}
			return true
		l467:
			position, tokenIndex = position467, tokenIndex467
			return false
		},
		/* 34 GOTSymbolOffset <- <(('$' SymbolName ('@' 'G' 'O' 'T') ('O' 'F' 'F')?) / (':' ('g' / 'G') ('o' / 'O') ('t' / 'T') ':' SymbolName))> */
		func() bool {
			position469, tokenIndex469 := position, tokenIndex
			{
				position470 := position
				{
					position471, tokenIndex471 := position, tokenIndex
					if buffer[position] != rune('$') {
						goto l472
					}
					position++
					if !_rules[ruleSymbolName]() {
						goto l472
					}
					if buffer[position] != rune('@') {
						goto l472
					}
					position++
					if buffer[position] != rune('G') {
						goto l472
					}
					position++
					if buffer[position] != rune('O') {
						goto l472
					}
					position++
					if buffer[position] != rune('T') {
						goto l472
					}
					position++
					{
						position473, tokenIndex473 := position, tokenIndex
						if buffer[position] != rune('O') {
							goto l473
						}
						position++
						if buffer[position] != rune('F') {
							goto l473
						}
						position++
						if buffer[position] != rune('F') {
							goto l473
						}
						position++
						goto l474
					l473:
						position, tokenIndex = position473, tokenIndex473
					}
				l474:
					goto l471
				l472:
					position, tokenIndex = position471, tokenIndex471
					if buffer[position] != rune(':') {
						goto l469
					}
					position++
					{
						position475, tokenIndex475 := position, tokenIndex
						if buffer[position] != rune('g') {
							goto l476
						}
						position++
						goto l475
					l476:
						position, tokenIndex = position475, tokenIndex475
						if buffer[position] != rune('G') {
							goto l469
						}
						position++
					}
				l475:
					{
						position477, tokenIndex477 := position, tokenIndex
						if buffer[position] != rune('o') {
							goto l478
						}
						position++
						goto l477
					l478:
						position, tokenIndex = position477, tokenIndex477
						if buffer[position] != rune('O') {
							goto l469
						}
						position++
					}
				l477:
					{
						position479, tokenIndex479 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l480
						}
						position++
						goto l479
					l480:
						position, tokenIndex = position479, tokenIndex479
						if buffer[position] != rune('T') {
							goto l469
						}
						position++
					}
				l479:
					if buffer[position] != rune(':') {
						goto l469
					}
					position++
					if !_rules[ruleSymbolName]() {
						goto l469
					}
				}
			l471:
				add(ruleGOTSymbolOffset, position470)
			}
			return true
		l469:
			position, tokenIndex = position469, tokenIndex469
			return false
		},
		/* 35 AVX512Token <- <(WS? '{' '%'? ([0-9] / [a-z])* '}')> */
		func() bool {
			position481, tokenIndex481 := position, tokenIndex
			{
				position482 := position
				{
					position483, tokenIndex483 := position, tokenIndex
					if !_rules[ruleWS]() {
						goto l483
					}
					goto l484
				l483:
					position, tokenIndex = position483, tokenIndex483
				}
			l484:
				if buffer[position] != rune('{') {
					goto l481
				}
				position++
				{
					position485, tokenIndex485 := position, tokenIndex
					if buffer[position] != rune('%') {
						goto l485
					}
					position++
					goto l486
				l485:
					position, tokenIndex = position485, tokenIndex485
				}
			l486:
			l487:
				{
					position488, tokenIndex488 := position, tokenIndex
					{
						position489, tokenIndex489 := position, tokenIndex
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l490
						}
						position++
						goto l489
					l490:
						position, tokenIndex = position489, tokenIndex489
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l488
						}
						position++
					}
				l489:
					goto l487
				l488:
					position, tokenIndex = position488, tokenIndex488
				}
				if buffer[position] != rune('}') {
					goto l481
				}
				position++
				add(ruleAVX512Token, position482)
			}
			return true
		l481:
			position, tokenIndex = position481, tokenIndex481
			return false
		},
		/* 36 TOCRefHigh <- <('.' 'T' 'O' 'C' '.' '-' (('0' 'b') / ('.' 'L' ([a-z] / [A-Z] / '_' / [0-9])+)) ('@' ('h' / 'H') ('a' / 'A')))> */
		func() bool {
			position491, tokenIndex491 := position, tokenIndex
			{
				position492 := position
				if buffer[position] != rune('.') {
					goto l491
				}
				position++
				if buffer[position] != rune('T') {
					goto l491
				}
				position++
				if buffer[position] != rune('O') {
					goto l491
				}
				position++
				if buffer[position] != rune('C') {
					goto l491
				}
				position++
				if buffer[position] != rune('.') {
					goto l491
				}
				position++
				if buffer[position] != rune('-') {
					goto l491
				}
				position++
				{
					position493, tokenIndex493 := position, tokenIndex
					if buffer[position] != rune('0') {
						goto l494
					}
					position++
					if buffer[position] != rune('b') {
						goto l494
					}
					position++
					goto l493
				l494:
					position, tokenIndex = position493, tokenIndex493
					if buffer[position] != rune('.') {
						goto l491
					}
					position++
					if buffer[position] != rune('L') {
						goto l491
					}
					position++
					{
						position497, tokenIndex497 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l498
						}
						position++
						goto l497
					l498:
						position, tokenIndex = position497, tokenIndex497
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l499
						}
						position++
						goto l497
					l499:
						position, tokenIndex = position497, tokenIndex497
						if buffer[position] != rune('_') {
							goto l500
						}
						position++
						goto l497
					l500:
						position, tokenIndex = position497, tokenIndex497
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l491
						}
						position++
					}
				l497:
				l495:
					{
						position496, tokenIndex496 := position, tokenIndex
						{
							position501, tokenIndex501 := position, tokenIndex
							if c := buffer[position]; c < rune('a') || c > rune('z') {
								goto l502
							}
							position++
							goto l501
						l502:
							position, tokenIndex = position501, tokenIndex501
							if c := buffer[position]; c < rune('A') || c > rune('Z') {
								goto l503
							}
							position++
							goto l501
						l503:
							position, tokenIndex = position501, tokenIndex501
							if buffer[position] != rune('_') {
								goto l504
							}
							position++
							goto l501
						l504:
							position, tokenIndex = position501, tokenIndex501
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l496
							}
							position++
						}
					l501:
						goto l495
					l496:
						position, tokenIndex = position496, tokenIndex496
					}
				}
			l493:
				if buffer[position] != rune('@') {
					goto l491
				}
				position++
				{
					position505, tokenIndex505 := position, tokenIndex
					if buffer[position] != rune('h') {
						goto l506
					}
					position++
					goto l505
				l506:
					position, tokenIndex = position505, tokenIndex505
					if buffer[position] != rune('H') {
						goto l491
					}
					position++
				}
			l505:
				{
					position507, tokenIndex507 := position, tokenIndex
					if buffer[position] != rune('a') {
						goto l508
					}
					position++
					goto l507
				l508:
					position, tokenIndex = position507, tokenIndex507
					if buffer[position] != rune('A') {
						goto l491
					}
					position++
				}
			l507:
				add(ruleTOCRefHigh, position492)
			}
			return true
		l491:
			position, tokenIndex = position491, tokenIndex491
			return false
		},
		/* 37 TOCRefLow <- <('.' 'T' 'O' 'C' '.' '-' (('0' 'b') / ('.' 'L' ([a-z] / [A-Z] / '_' / [0-9])+)) ('@' ('l' / 'L')))> */
		func() bool {
			position509, tokenIndex509 := position, tokenIndex
			{
				position510 := position
				if buffer[position] != rune('.') {
					goto l509
				}
				position++
				if buffer[position] != rune('T') {
					goto l509
				}
				position++
				if buffer[position] != rune('O') {
					goto l509
				}
				position++
				if buffer[position] != rune('C') {
					goto l509
				}
				position++
				if buffer[position] != rune('.') {
					goto l509
				}
				position++
				if buffer[position] != rune('-') {
					goto l509
				}
				position++
				{
					position511, tokenIndex511 := position, tokenIndex
					if buffer[position] != rune('0') {
						goto l512
					}
					position++
					if buffer[position] != rune('b') {
						goto l512
					}
					position++
					goto l511
				l512:
					position, tokenIndex = position511, tokenIndex511
					if buffer[position] != rune('.') {
						goto l509
					}
					position++
					if buffer[position] != rune('L') {
						goto l509
					}
					position++
					{
						position515, tokenIndex515 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l516
						}
						position++
						goto l515
					l516:
						position, tokenIndex = position515, tokenIndex515
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l517
						}
						position++
						goto l515
					l517:
						position, tokenIndex = position515, tokenIndex515
						if buffer[position] != rune('_') {
							goto l518
						}
						position++
						goto l515
					l518:
						position, tokenIndex = position515, tokenIndex515
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l509
						}
						position++
					}
				l515:
				l513:
					{
						position514, tokenIndex514 := position, tokenIndex
						{
							position519, tokenIndex519 := position, tokenIndex
							if c := buffer[position]; c < rune('a') || c > rune('z') {
								goto l520
							}
							position++
							goto l519
						l520:
							position, tokenIndex = position519, tokenIndex519
							if c := buffer[position]; c < rune('A') || c > rune('Z') {
								goto l521
							}
							position++
							goto l519
						l521:
							position, tokenIndex = position519, tokenIndex519
							if buffer[position] != rune('_') {
								goto l522
							}
							position++
							goto l519
						l522:
							position, tokenIndex = position519, tokenIndex519
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l514
							}
							position++
						}
					l519:
						goto l513
					l514:
						position, tokenIndex = position514, tokenIndex514
					}
				}
			l511:
				if buffer[position] != rune('@') {
					goto l509
				}
				position++
				{
					position523, tokenIndex523 := position, tokenIndex
					if buffer[position] != rune('l') {
						goto l524
					}
					position++
					goto l523
				l524:
					position, tokenIndex = position523, tokenIndex523
					if buffer[position] != rune('L') {
						goto l509
					}
					position++
				}
			l523:
				add(ruleTOCRefLow, position510)
			}
			return true
		l509:
			position, tokenIndex = position509, tokenIndex509
			return false
		},
		/* 38 IndirectionIndicator <- <'*'> */
		func() bool {
			position525, tokenIndex525 := position, tokenIndex
			{
				position526 := position
				if buffer[position] != rune('*') {
					goto l525
				}
				position++
				add(ruleIndirectionIndicator, position526)
			}
			return true
		l525:
			position, tokenIndex = position525, tokenIndex525
			return false
		},
		/* 39 RegisterOrConstant <- <((('%' ([a-z] / [A-Z]) ([a-z] / [A-Z] / ([0-9] / [0-9]))*) / ('$'? ((Offset Offset) / Offset)) / ('#' Offset ('*' [0-9]+ ('-' [0-9] [0-9]*)?)?) / ('#' '~'? '(' [0-9] WS? ('<' '<') WS? [0-9] ')') / ARMRegister) !('f' / 'b' / ':' / '(' / '+' / '-'))> */
		func() bool {
			position527, tokenIndex527 := position, tokenIndex
			{
				position528 := position
				{
					position529, tokenIndex529 := position, tokenIndex
					if buffer[position] != rune('%') {
						goto l530
					}
					position++
					{
						position531, tokenIndex531 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l532
						}
						position++
						goto l531
					l532:
						position, tokenIndex = position531, tokenIndex531
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l530
						}
						position++
					}
				l531:
				l533:
					{
						position534, tokenIndex534 := position, tokenIndex
						{
							position535, tokenIndex535 := position, tokenIndex
							if c := buffer[position]; c < rune('a') || c > rune('z') {
								goto l536
							}
							position++
							goto l535
						l536:
							position, tokenIndex = position535, tokenIndex535
							if c := buffer[position]; c < rune('A') || c > rune('Z') {
								goto l537
							}
							position++
							goto l535
						l537:
							position, tokenIndex = position535, tokenIndex535
							{
								position538, tokenIndex538 := position, tokenIndex
								if c := buffer[position]; c < rune('0') || c > rune('9') {
									goto l539
								}
								position++
								goto l538
							l539:
								position, tokenIndex = position538, tokenIndex538
								if c := buffer[position]; c < rune('0') || c > rune('9') {
									goto l534
								}
								position++
							}
						l538:
						}
					l535:
						goto l533
					l534:
						position, tokenIndex = position534, tokenIndex534
					}
					goto l529
				l530:
					position, tokenIndex = position529, tokenIndex529
					{
						position541, tokenIndex541 := position, tokenIndex
						if buffer[position] != rune('$') {
							goto l541
						}
						position++
						goto l542
					l541:
						position, tokenIndex = position541, tokenIndex541
					}
				l542:
					{
						position543, tokenIndex543 := position, tokenIndex
						if !_rules[ruleOffset]() {
							goto l544
						}
						if !_rules[ruleOffset]() {
							goto l544
						}
						goto l543
					l544:
						position, tokenIndex = position543, tokenIndex543
						if !_rules[ruleOffset]() {
							goto l540
						}
					}
				l543:
					goto l529
				l540:
					position, tokenIndex = position529, tokenIndex529
					if buffer[position] != rune('#') {
						goto l545
					}
					position++
					if !_rules[ruleOffset]() {
						goto l545
					}
					{
						position546, tokenIndex546 := position, tokenIndex
						if buffer[position] != rune('*') {
							goto l546
						}
						position++
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l546
						}
						position++
					l548:
						{
							position549, tokenIndex549 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l549
							}
							position++
							goto l548
						l549:
							position, tokenIndex = position549, tokenIndex549
						}
						{
							position550, tokenIndex550 := position, tokenIndex
							if buffer[position] != rune('-') {
								goto l550
							}
							position++
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l550
							}
							position++
						l552:
							{
								position553, tokenIndex553 := position, tokenIndex
								if c := buffer[position]; c < rune('0') || c > rune('9') {
									goto l553
								}
								position++
								goto l552
							l553:
								position, tokenIndex = position553, tokenIndex553
							}
							goto l551
						l550:
							position, tokenIndex = position550, tokenIndex550
						}
					l551:
						goto l547
					l546:
						position, tokenIndex = position546, tokenIndex546
					}
				l547:
					goto l529
				l545:
					position, tokenIndex = position529, tokenIndex529
					if buffer[position] != rune('#') {
						goto l554
					}
					position++
					{
						position555, tokenIndex555 := position, tokenIndex
						if buffer[position] != rune('~') {
							goto l555
						}
						position++
						goto l556
					l555:
						position, tokenIndex = position555, tokenIndex555
					}
				l556:
					if buffer[position] != rune('(') {
						goto l554
					}
					position++
					if c := buffer[position]; c < rune('0') || c > rune('9') {
						goto l554
					}
					position++
					{
						position557, tokenIndex557 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l557
						}
						goto l558
					l557:
						position, tokenIndex = position557, tokenIndex557
					}
				l558:
					if buffer[position] != rune('<') {
						goto l554
					}
					position++
					if buffer[position] != rune('<') {
						goto l554
					}
					position++
					{
						position559, tokenIndex559 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l559
						}
						goto l560
					l559:
						position, tokenIndex = position559, tokenIndex559
					}
				l560:
					if c := buffer[position]; c < rune('0') || c > rune('9') {
						goto l554
					}
					position++
					if buffer[position] != rune(')') {
						goto l554
					}
					position++
					goto l529
				l554:
					position, tokenIndex = position529, tokenIndex529
					if !_rules[ruleARMRegister]() {
						goto l527
					}
				}
			l529:
				{
					position561, tokenIndex561 := position, tokenIndex
					{
						position562, tokenIndex562 := position, tokenIndex
						if buffer[position] != rune('f') {
							goto l563
						}
						position++
						goto l562
					l563:
						position, tokenIndex = position562, tokenIndex562
						if buffer[position] != rune('b') {
							goto l564
						}
						position++
						goto l562
					l564:
						position, tokenIndex = position562, tokenIndex562
						if buffer[position] != rune(':') {
							goto l565
						}
						position++
						goto l562
					l565:
						position, tokenIndex = position562, tokenIndex562
						if buffer[position] != rune('(') {
							goto l566
						}
						position++
						goto l562
					l566:
						position, tokenIndex = position562, tokenIndex562
						if buffer[position] != rune('+') {
							goto l567
						}
						position++
						goto l562
					l567:
						position, tokenIndex = position562, tokenIndex562
						if buffer[position] != rune('-') {
							goto l561
						}
						position++
					}
				l562:
					goto l527
				l561:
					position, tokenIndex = position561, tokenIndex561
				}
				add(ruleRegisterOrConstant, position528)
			}
			return true
		l527:
			position, tokenIndex = position527, tokenIndex527
			return false
		},
		/* 40 ARMConstantTweak <- <(((('l' / 'L') ('s' / 'S') ('l' / 'L')) / (('s' / 'S') ('x' / 'X') ('t' / 'T') ('w' / 'W')) / (('u' / 'U') ('x' / 'X') ('t' / 'T') ('w' / 'W')) / (('u' / 'U') ('x' / 'X') ('t' / 'T') ('b' / 'B')) / (('l' / 'L') ('s' / 'S') ('r' / 'R')) / (('r' / 'R') ('o' / 'O') ('r' / 'R')) / (('a' / 'A') ('s' / 'S') ('r' / 'R'))) (WS '#' Offset)?)> */
		func() bool {
			position568, tokenIndex568 := position, tokenIndex
			{
				position569 := position
				{
					position570, tokenIndex570 := position, tokenIndex
					{
						position572, tokenIndex572 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l573
						}
						position++
						goto l572
					l573:
						position, tokenIndex = position572, tokenIndex572
						if buffer[position] != rune('L') {
							goto l571
						}
						position++
					}
				l572:
					{
						position574, tokenIndex574 := position, tokenIndex
						if buffer[position] != rune('s') {
							goto l575
						}
						position++
						goto l574
					l575:
						position, tokenIndex = position574, tokenIndex574
						if buffer[position] != rune('S') {
							goto l571
						}
						position++
					}
				l574:
					{
						position576, tokenIndex576 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l577
						}
						position++
						goto l576
					l577:
						position, tokenIndex = position576, tokenIndex576
						if buffer[position] != rune('L') {
							goto l571
						}
						position++
					}
				l576:
					goto l570
				l571:
					position, tokenIndex = position570, tokenIndex570
					{
						position579, tokenIndex579 := position, tokenIndex
						if buffer[position] != rune('s') {
							goto l580
						}
						position++
						goto l579
					l580:
						position, tokenIndex = position579, tokenIndex579
						if buffer[position] != rune('S') {
							goto l578
						}
						position++
					}
				l579:
					{
						position581, tokenIndex581 := position, tokenIndex
						if buffer[position] != rune('x') {
							goto l582
						}
						position++
						goto l581
					l582:
						position, tokenIndex = position581, tokenIndex581
						if buffer[position] != rune('X') {
							goto l578
						}
						position++
					}
				l581:
					{
						position583, tokenIndex583 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l584
						}
						position++
						goto l583
					l584:
						position, tokenIndex = position583, tokenIndex583
						if buffer[position] != rune('T') {
							goto l578
						}
						position++
					}
				l583:
					{
						position585, tokenIndex585 := position, tokenIndex
						if buffer[position] != rune('w') {
							goto l586
						}
						position++
						goto l585
					l586:
						position, tokenIndex = position585, tokenIndex585
						if buffer[position] != rune('W') {
							goto l578
						}
						position++
					}
				l585:
					goto l570
				l578:
					position, tokenIndex = position570, tokenIndex570
					{
						position588, tokenIndex588 := position, tokenIndex
						if buffer[position] != rune('u') {
							goto l589
						}
						position++
						goto l588
					l589:
						position, tokenIndex = position588, tokenIndex588
						if buffer[position] != rune('U') {
							goto l587
						}
						position++
					}
				l588:
					{
						position590, tokenIndex590 := position, tokenIndex
						if buffer[position] != rune('x') {
							goto l591
						}
						position++
						goto l590
					l591:
						position, tokenIndex = position590, tokenIndex590
						if buffer[position] != rune('X') {
							goto l587
						}
						position++
					}
				l590:
					{
						position592, tokenIndex592 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l593
						}
						position++
						goto l592
					l593:
						position, tokenIndex = position592, tokenIndex592
						if buffer[position] != rune('T') {
							goto l587
						}
						position++
					}
				l592:
					{
						position594, tokenIndex594 := position, tokenIndex
						if buffer[position] != rune('w') {
							goto l595
						}
						position++
						goto l594
					l595:
						position, tokenIndex = position594, tokenIndex594
						if buffer[position] != rune('W') {
							goto l587
						}
						position++
					}
				l594:
					goto l570
				l587:
					position, tokenIndex = position570, tokenIndex570
					{
						position597, tokenIndex597 := position, tokenIndex
						if buffer[position] != rune('u') {
							goto l598
						}
						position++
						goto l597
					l598:
						position, tokenIndex = position597, tokenIndex597
						if buffer[position] != rune('U') {
							goto l596
						}
						position++
					}
				l597:
					{
						position599, tokenIndex599 := position, tokenIndex
						if buffer[position] != rune('x') {
							goto l600
						}
						position++
						goto l599
					l600:
						position, tokenIndex = position599, tokenIndex599
						if buffer[position] != rune('X') {
							goto l596
						}
						position++
					}
				l599:
					{
						position601, tokenIndex601 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l602
						}
						position++
						goto l601
					l602:
						position, tokenIndex = position601, tokenIndex601
						if buffer[position] != rune('T') {
							goto l596
						}
						position++
					}
				l601:
					{
						position603, tokenIndex603 := position, tokenIndex
						if buffer[position] != rune('b') {
							goto l604
						}
						position++
						goto l603
					l604:
						position, tokenIndex = position603, tokenIndex603
						if buffer[position] != rune('B') {
							goto l596
						}
						position++
					}
				l603:
					goto l570
				l596:
					position, tokenIndex = position570, tokenIndex570
					{
						position606, tokenIndex606 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l607
						}
						position++
						goto l606
					l607:
						position, tokenIndex = position606, tokenIndex606
						if buffer[position] != rune('L') {
							goto l605
						}
						position++
					}
				l606:
					{
						position608, tokenIndex608 := position, tokenIndex
						if buffer[position] != rune('s') {
							goto l609
						}
						position++
						goto l608
					l609:
						position, tokenIndex = position608, tokenIndex608
						if buffer[position] != rune('S') {
							goto l605
						}
						position++
					}
				l608:
					{
						position610, tokenIndex610 := position, tokenIndex
						if buffer[position] != rune('r') {
							goto l611
						}
						position++
						goto l610
					l611:
						position, tokenIndex = position610, tokenIndex610
						if buffer[position] != rune('R') {
							goto l605
						}
						position++
					}
				l610:
					goto l570
				l605:
					position, tokenIndex = position570, tokenIndex570
					{
						position613, tokenIndex613 := position, tokenIndex
						if buffer[position] != rune('r') {
							goto l614
						}
						position++
						goto l613
					l614:
						position, tokenIndex = position613, tokenIndex613
						if buffer[position] != rune('R') {
							goto l612
						}
						position++
					}
				l613:
					{
						position615, tokenIndex615 := position, tokenIndex
						if buffer[position] != rune('o') {
							goto l616
						}
						position++
						goto l615
					l616:
						position, tokenIndex = position615, tokenIndex615
						if buffer[position] != rune('O') {
							goto l612
						}
						position++
					}
				l615:
					{
						position617, tokenIndex617 := position, tokenIndex
						if buffer[position] != rune('r') {
							goto l618
						}
						position++
						goto l617
					l618:
						position, tokenIndex = position617, tokenIndex617
						if buffer[position] != rune('R') {
							goto l612
						}
						position++
					}
				l617:
					goto l570
				l612:
					position, tokenIndex = position570, tokenIndex570
					{
						position619, tokenIndex619 := position, tokenIndex
						if buffer[position] != rune('a') {
							goto l620
						}
						position++
						goto l619
					l620:
						position, tokenIndex = position619, tokenIndex619
						if buffer[position] != rune('A') {
							goto l568
						}
						position++
					}
				l619:
					{
						position621, tokenIndex621 := position, tokenIndex
						if buffer[position] != rune('s') {
							goto l622
						}
						position++
						goto l621
					l622:
						position, tokenIndex = position621, tokenIndex621
						if buffer[position] != rune('S') {
							goto l568
						}
						position++
					}
				l621:
					{
						position623, tokenIndex623 := position, tokenIndex
						if buffer[position] != rune('r') {
							goto l624
						}
						position++
						goto l623
					l624:
						position, tokenIndex = position623, tokenIndex623
						if buffer[position] != rune('R') {
							goto l568
						}
						position++
					}
				l623:
				}
			l570:
				{
					position625, tokenIndex625 := position, tokenIndex
					if !_rules[ruleWS]() {
						goto l625
					}
					if buffer[position] != rune('#') {
						goto l625
					}
					position++
					if !_rules[ruleOffset]() {
						goto l625
					}
					goto l626
				l625:
					position, tokenIndex = position625, tokenIndex625
				}
			l626:
				add(ruleARMConstantTweak, position569)
			}
			return true
		l568:
			position, tokenIndex = position568, tokenIndex568
			return false
		},
		/* 41 ARMRegister <- <((('s' / 'S') ('p' / 'P')) / (('x' / 'w' / 'd' / 'q' / 's') [0-9] [0-9]?) / (('x' / 'X') ('z' / 'Z') ('r' / 'R')) / (('w' / 'W') ('z' / 'Z') ('r' / 'R')) / ARMVectorRegister / ('{' WS? ARMVectorRegister (',' WS? ARMVectorRegister)* WS? '}' ('[' [0-9] [0-9]? ']')?))> */
		func() bool {
			position627, tokenIndex627 := position, tokenIndex
			{
				position628 := position
				{
					position629, tokenIndex629 := position, tokenIndex
					{
						position631, tokenIndex631 := position, tokenIndex
						if buffer[position] != rune('s') {
							goto l632
						}
						position++
						goto l631
					l632:
						position, tokenIndex = position631, tokenIndex631
						if buffer[position] != rune('S') {
							goto l630
						}
						position++
					}
				l631:
					{
						position633, tokenIndex633 := position, tokenIndex
						if buffer[position] != rune('p') {
							goto l634
						}
						position++
						goto l633
					l634:
						position, tokenIndex = position633, tokenIndex633
						if buffer[position] != rune('P') {
							goto l630
						}
						position++
					}
				l633:
					goto l629
				l630:
					position, tokenIndex = position629, tokenIndex629
					{
						position636, tokenIndex636 := position, tokenIndex
						if buffer[position] != rune('x') {
							goto l637
						}
						position++
						goto l636
					l637:
						position, tokenIndex = position636, tokenIndex636
						if buffer[position] != rune('w') {
							goto l638
						}
						position++
						goto l636
					l638:
						position, tokenIndex = position636, tokenIndex636
						if buffer[position] != rune('d') {
							goto l639
						}
						position++
						goto l636
					l639:
						position, tokenIndex = position636, tokenIndex636
						if buffer[position] != rune('q') {
							goto l640
						}
						position++
						goto l636
					l640:
						position, tokenIndex = position636, tokenIndex636
						if buffer[position] != rune('s') {
							goto l635
						}
						position++
					}
				l636:
					if c := buffer[position]; c < rune('0') || c > rune('9') {
						goto l635
					}
					position++
					{
						position641, tokenIndex641 := position, tokenIndex
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l641
						}
						position++
						goto l642
					l641:
						position, tokenIndex = position641, tokenIndex641
					}
				l642:
					goto l629
				l635:
					position, tokenIndex = position629, tokenIndex629
					{
						position644, tokenIndex644 := position, tokenIndex
						if buffer[position] != rune('x') {
							goto l645
						}
						position++
						goto l644
					l645:
						position, tokenIndex = position644, tokenIndex644
						if buffer[position] != rune('X') {
							goto l643
						}
						position++
					}
				l644:
					{
						position646, tokenIndex646 := position, tokenIndex
						if buffer[position] != rune('z') {
							goto l647
						}
						position++
						goto l646
					l647:
						position, tokenIndex = position646, tokenIndex646
						if buffer[position] != rune('Z') {
							goto l643
						}
						position++
					}
				l646:
					{
						position648, tokenIndex648 := position, tokenIndex
						if buffer[position] != rune('r') {
							goto l649
						}
						position++
						goto l648
					l649:
						position, tokenIndex = position648, tokenIndex648
						if buffer[position] != rune('R') {
							goto l643
						}
						position++
					}
				l648:
					goto l629
				l643:
					position, tokenIndex = position629, tokenIndex629
					{
						position651, tokenIndex651 := position, tokenIndex
						if buffer[position] != rune('w') {
							goto l652
						}
						position++
						goto l651
					l652:
						position, tokenIndex = position651, tokenIndex651
						if buffer[position] != rune('W') {
							goto l650
						}
						position++
					}
				l651:
					{
						position653, tokenIndex653 := position, tokenIndex
						if buffer[position] != rune('z') {
							goto l654
						}
						position++
						goto l653
					l654:
						position, tokenIndex = position653, tokenIndex653
						if buffer[position] != rune('Z') {
							goto l650
						}
						position++
					}
				l653:
					{
						position655, tokenIndex655 := position, tokenIndex
						if buffer[position] != rune('r') {
							goto l656
						}
						position++
						goto l655
					l656:
						position, tokenIndex = position655, tokenIndex655
						if buffer[position] != rune('R') {
							goto l650
						}
						position++
					}
				l655:
					goto l629
				l650:
					position, tokenIndex = position629, tokenIndex629
					if !_rules[ruleARMVectorRegister]() {
						goto l657
					}
					goto l629
				l657:
					position, tokenIndex = position629, tokenIndex629
					if buffer[position] != rune('{') {
						goto l627
					}
					position++
					{
						position658, tokenIndex658 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l658
						}
						goto l659
					l658:
						position, tokenIndex = position658, tokenIndex658
					}
				l659:
					if !_rules[ruleARMVectorRegister]() {
						goto l627
					}
				l660:
					{
						position661, tokenIndex661 := position, tokenIndex
						if buffer[position] != rune(',') {
							goto l661
						}
						position++
						{
							position662, tokenIndex662 := position, tokenIndex
							if !_rules[ruleWS]() {
								goto l662
							}
							goto l663
						l662:
							position, tokenIndex = position662, tokenIndex662
						}
					l663:
						if !_rules[ruleARMVectorRegister]() {
							goto l661
						}
						goto l660
					l661:
						position, tokenIndex = position661, tokenIndex661
					}
					{
						position664, tokenIndex664 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l664
						}
						goto l665
					l664:
						position, tokenIndex = position664, tokenIndex664
					}
				l665:
					if buffer[position] != rune('}') {
						goto l627
					}
					position++
					{
						position666, tokenIndex666 := position, tokenIndex
						if buffer[position] != rune('[') {
							goto l666
						}
						position++
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l666
						}
						position++
						{
							position668, tokenIndex668 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l668
							}
							position++
							goto l669
						l668:
							position, tokenIndex = position668, tokenIndex668
						}
					l669:
						if buffer[position] != rune(']') {
							goto l666
						}
						position++
						goto l667
					l666:
						position, tokenIndex = position666, tokenIndex666
					}
				l667:
				}
			l629:
				add(ruleARMRegister, position628)
			}
			return true
		l627:
			position, tokenIndex = position627, tokenIndex627
			return false
		},
		/* 42 ARMVectorRegister <- <(('v' / 'V') [0-9] [0-9]? ('.' [0-9]* ('b' / 's' / 'd' / 'h' / 'q') ('[' [0-9] [0-9]? ']')?)?)> */
		func() bool {
			position670, tokenIndex670 := position, tokenIndex
			{
				position671 := position
				{
					position672, tokenIndex672 := position, tokenIndex
					if buffer[position] != rune('v') {
						goto l673
					}
					position++
					goto l672
				l673:
					position, tokenIndex = position672, tokenIndex672
					if buffer[position] != rune('V') {
						goto l670
					}
					position++
				}
			l672:
				if c := buffer[position]; c < rune('0') || c > rune('9') {
					goto l670
				}
				position++
				{
					position674, tokenIndex674 := position, tokenIndex
					if c := buffer[position]; c < rune('0') || c > rune('9') {
						goto l674
					}
					position++
					goto l675
				l674:
					position, tokenIndex = position674, tokenIndex674
				}
			l675:
				{
					position676, tokenIndex676 := position, tokenIndex
					if buffer[position] != rune('.') {
						goto l676
					}
					position++
				l678:
					{
						position679, tokenIndex679 := position, tokenIndex
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l679
						}
						position++
						goto l678
					l679:
						position, tokenIndex = position679, tokenIndex679
					}
					{
						position680, tokenIndex680 := position, tokenIndex
						if buffer[position] != rune('b') {
							goto l681
						}
						position++
						goto l680
					l681:
						position, tokenIndex = position680, tokenIndex680
						if buffer[position] != rune('s') {
							goto l682
						}
						position++
						goto l680
					l682:
						position, tokenIndex = position680, tokenIndex680
						if buffer[position] != rune('d') {
							goto l683
						}
						position++
						goto l680
					l683:
						position, tokenIndex = position680, tokenIndex680
						if buffer[position] != rune('h') {
							goto l684
						}
						position++
						goto l680
					l684:
						position, tokenIndex = position680, tokenIndex680
						if buffer[position] != rune('q') {
							goto l676
						}
						position++
					}
				l680:
					{
						position685, tokenIndex685 := position, tokenIndex
						if buffer[position] != rune('[') {
							goto l685
						}
						position++
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l685
						}
						position++
						{
							position687, tokenIndex687 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l687
							}
							position++
							goto l688
						l687:
							position, tokenIndex = position687, tokenIndex687
						}
					l688:
						if buffer[position] != rune(']') {
							goto l685
						}
						position++
						goto l686
					l685:
						position, tokenIndex = position685, tokenIndex685
					}
				l686:
					goto l677
				l676:
					position, tokenIndex = position676, tokenIndex676
				}
			l677:
				add(ruleARMVectorRegister, position671)
			}
			return true
		l670:
			position, tokenIndex = position670, tokenIndex670
			return false
		},
		/* 43 MemoryRef <- <((SymbolRef BaseIndexScale) / SymbolRef / Low12BitsSymbolRef / (Offset* BaseIndexScale) / (SegmentRegister Offset BaseIndexScale) / (SegmentRegister BaseIndexScale) / (SegmentRegister Offset) / ARMBaseIndexScale / BaseIndexScale)> */
		func() bool {
			position689, tokenIndex689 := position, tokenIndex
			{
				position690 := position
				{
					position691, tokenIndex691 := position, tokenIndex
					if !_rules[ruleSymbolRef]() {
						goto l692
					}
					if !_rules[ruleBaseIndexScale]() {
						goto l692
					}
					goto l691
				l692:
					position, tokenIndex = position691, tokenIndex691
					if !_rules[ruleSymbolRef]() {
						goto l693
					}
					goto l691
				l693:
					position, tokenIndex = position691, tokenIndex691
					if !_rules[ruleLow12BitsSymbolRef]() {
						goto l694
					}
					goto l691
				l694:
					position, tokenIndex = position691, tokenIndex691
				l696:
					{
						position697, tokenIndex697 := position, tokenIndex
						if !_rules[ruleOffset]() {
							goto l697
						}
						goto l696
					l697:
						position, tokenIndex = position697, tokenIndex697
					}
					if !_rules[ruleBaseIndexScale]() {
						goto l695
					}
					goto l691
				l695:
					position, tokenIndex = position691, tokenIndex691
					if !_rules[ruleSegmentRegister]() {
						goto l698
					}
					if !_rules[ruleOffset]() {
						goto l698
					}
					if !_rules[ruleBaseIndexScale]() {
						goto l698
					}
					goto l691
				l698:
					position, tokenIndex = position691, tokenIndex691
					if !_rules[ruleSegmentRegister]() {
						goto l699
					}
					if !_rules[ruleBaseIndexScale]() {
						goto l699
					}
					goto l691
				l699:
					position, tokenIndex = position691, tokenIndex691
					if !_rules[ruleSegmentRegister]() {
						goto l700
					}
					if !_rules[ruleOffset]() {
						goto l700
					}
					goto l691
				l700:
					position, tokenIndex = position691, tokenIndex691
					if !_rules[ruleARMBaseIndexScale]() {
						goto l701
					}
					goto l691
				l701:
					position, tokenIndex = position691, tokenIndex691
					if !_rules[ruleBaseIndexScale]() {
						goto l689
					}
				}
			l691:
				add(ruleMemoryRef, position690)
			}
			return true
		l689:
			position, tokenIndex = position689, tokenIndex689
			return false
		},
		/* 44 SymbolRef <- <((Offset* '+')? (LocalSymbol / SymbolName) Offset* ('@' Section Offset*)?)> */
		func() bool {
			position702, tokenIndex702 := position, tokenIndex
			{
				position703 := position
				{
					position704, tokenIndex704 := position, tokenIndex
				l706:
					{
						position707, tokenIndex707 := position, tokenIndex
						if !_rules[ruleOffset]() {
							goto l707
						}
						goto l706
					l707:
						position, tokenIndex = position707, tokenIndex707
					}
					if buffer[position] != rune('+') {
						goto l704
					}
					position++
					goto l705
				l704:
					position, tokenIndex = position704, tokenIndex704
				}
			l705:
				{
					position708, tokenIndex708 := position, tokenIndex
					if !_rules[ruleLocalSymbol]() {
						goto l709
					}
					goto l708
				l709:
					position, tokenIndex = position708, tokenIndex708
					if !_rules[ruleSymbolName]() {
						goto l702
					}
				}
			l708:
			l710:
				{
					position711, tokenIndex711 := position, tokenIndex
					if !_rules[ruleOffset]() {
						goto l711
					}
					goto l710
				l711:
					position, tokenIndex = position711, tokenIndex711
				}
				{
					position712, tokenIndex712 := position, tokenIndex
					if buffer[position] != rune('@') {
						goto l712
					}
					position++
					if !_rules[ruleSection]() {
						goto l712
					}
				l714:
					{
						position715, tokenIndex715 := position, tokenIndex
						if !_rules[ruleOffset]() {
							goto l715
						}
						goto l714
					l715:
						position, tokenIndex = position715, tokenIndex715
					}
					goto l713
				l712:
					position, tokenIndex = position712, tokenIndex712
				}
			l713:
				add(ruleSymbolRef, position703)
			}
			return true
		l702:
			position, tokenIndex = position702, tokenIndex702
			return false
		},
		/* 45 Low12BitsSymbolRef <- <(':' ('l' / 'L') ('o' / 'O') '1' '2' ':' (LocalSymbol / SymbolName) Offset?)> */
		func() bool {
			position716, tokenIndex716 := position, tokenIndex
			{
				position717 := position
				if buffer[position] != rune(':') {
					goto l716
				}
				position++
				{
					position718, tokenIndex718 := position, tokenIndex
					if buffer[position] != rune('l') {
						goto l719
					}
					position++
					goto l718
				l719:
					position, tokenIndex = position718, tokenIndex718
					if buffer[position] != rune('L') {
						goto l716
					}
					position++
				}
			l718:
				{
					position720, tokenIndex720 := position, tokenIndex
					if buffer[position] != rune('o') {
						goto l721
					}
					position++
					goto l720
				l721:
					position, tokenIndex = position720, tokenIndex720
					if buffer[position] != rune('O') {
						goto l716
					}
					position++
				}
			l720:
				if buffer[position] != rune('1') {
					goto l716
				}
				position++
				if buffer[position] != rune('2') {
					goto l716
				}
				position++
				if buffer[position] != rune(':') {
					goto l716
				}
				position++
				{
					position722, tokenIndex722 := position, tokenIndex
					if !_rules[ruleLocalSymbol]() {
						goto l723
					}
					goto l722
				l723:
					position, tokenIndex = position722, tokenIndex722
					if !_rules[ruleSymbolName]() {
						goto l716
					}
				}
			l722:
				{
					position724, tokenIndex724 := position, tokenIndex
					if !_rules[ruleOffset]() {
						goto l724
					}
					goto l725
				l724:
					position, tokenIndex = position724, tokenIndex724
				}
			l725:
				add(ruleLow12BitsSymbolRef, position717)
			}
			return true
		l716:
			position, tokenIndex = position716, tokenIndex716
			return false
		},
		/* 46 ARMBaseIndexScale <- <('[' ARMRegister (',' WS? (('#' Offset ('*' [0-9]+)?) / ARMGOTLow12 / Low12BitsSymbolRef / ARMRegister) (',' WS? ARMConstantTweak)?)? ']' ARMPostincrement?)> */
		func() bool {
			position726, tokenIndex726 := position, tokenIndex
			{
				position727 := position
				if buffer[position] != rune('[') {
					goto l726
				}
				position++
				if !_rules[ruleARMRegister]() {
					goto l726
				}
				{
					position728, tokenIndex728 := position, tokenIndex
					if buffer[position] != rune(',') {
						goto l728
					}
					position++
					{
						position730, tokenIndex730 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l730
						}
						goto l731
					l730:
						position, tokenIndex = position730, tokenIndex730
					}
				l731:
					{
						position732, tokenIndex732 := position, tokenIndex
						if buffer[position] != rune('#') {
							goto l733
						}
						position++
						if !_rules[ruleOffset]() {
							goto l733
						}
						{
							position734, tokenIndex734 := position, tokenIndex
							if buffer[position] != rune('*') {
								goto l734
							}
							position++
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l734
							}
							position++
						l736:
							{
								position737, tokenIndex737 := position, tokenIndex
								if c := buffer[position]; c < rune('0') || c > rune('9') {
									goto l737
								}
								position++
								goto l736
							l737:
								position, tokenIndex = position737, tokenIndex737
							}
							goto l735
						l734:
							position, tokenIndex = position734, tokenIndex734
						}
					l735:
						goto l732
					l733:
						position, tokenIndex = position732, tokenIndex732
						if !_rules[ruleARMGOTLow12]() {
							goto l738
						}
						goto l732
					l738:
						position, tokenIndex = position732, tokenIndex732
						if !_rules[ruleLow12BitsSymbolRef]() {
							goto l739
						}
						goto l732
					l739:
						position, tokenIndex = position732, tokenIndex732
						if !_rules[ruleARMRegister]() {
							goto l728
						}
					}
				l732:
					{
						position740, tokenIndex740 := position, tokenIndex
						if buffer[position] != rune(',') {
							goto l740
						}
						position++
						{
							position742, tokenIndex742 := position, tokenIndex
							if !_rules[ruleWS]() {
								goto l742
							}
							goto l743
						l742:
							position, tokenIndex = position742, tokenIndex742
						}
					l743:
						if !_rules[ruleARMConstantTweak]() {
							goto l740
						}
						goto l741
					l740:
						position, tokenIndex = position740, tokenIndex740
					}
				l741:
					goto l729
				l728:
					position, tokenIndex = position728, tokenIndex728
				}
			l729:
				if buffer[position] != rune(']') {
					goto l726
				}
				position++
				{
					position744, tokenIndex744 := position, tokenIndex
					if !_rules[ruleARMPostincrement]() {
						goto l744
					}
					goto l745
				l744:
					position, tokenIndex = position744, tokenIndex744
				}
			l745:
				add(ruleARMBaseIndexScale, position727)
			}
			return true
		l726:
			position, tokenIndex = position726, tokenIndex726
			return false
		},
		/* 47 ARMGOTLow12 <- <(':' ('g' / 'G') ('o' / 'O') ('t' / 'T') '_' ('l' / 'L') ('o' / 'O') '1' '2' ':' SymbolName)> */
		func() bool {
			position746, tokenIndex746 := position, tokenIndex
			{
				position747 := position
				if buffer[position] != rune(':') {
					goto l746
				}
				position++
				{
					position748, tokenIndex748 := position, tokenIndex
					if buffer[position] != rune('g') {
						goto l749
					}
					position++
					goto l748
				l749:
					position, tokenIndex = position748, tokenIndex748
					if buffer[position] != rune('G') {
						goto l746
					}
					position++
				}
			l748:
				{
					position750, tokenIndex750 := position, tokenIndex
					if buffer[position] != rune('o') {
						goto l751
					}
					position++
					goto l750
				l751:
					position, tokenIndex = position750, tokenIndex750
					if buffer[position] != rune('O') {
						goto l746
					}
					position++
				}
			l750:
				{
					position752, tokenIndex752 := position, tokenIndex
					if buffer[position] != rune('t') {
						goto l753
					}
					position++
					goto l752
				l753:
					position, tokenIndex = position752, tokenIndex752
					if buffer[position] != rune('T') {
						goto l746
					}
					position++
				}
			l752:
				if buffer[position] != rune('_') {
					goto l746
				}
				position++
				{
					position754, tokenIndex754 := position, tokenIndex
					if buffer[position] != rune('l') {
						goto l755
					}
					position++
					goto l754
				l755:
					position, tokenIndex = position754, tokenIndex754
					if buffer[position] != rune('L') {
						goto l746
					}
					position++
				}
			l754:
				{
					position756, tokenIndex756 := position, tokenIndex
					if buffer[position] != rune('o') {
						goto l757
					}
					position++
					goto l756
				l757:
					position, tokenIndex = position756, tokenIndex756
					if buffer[position] != rune('O') {
						goto l746
					}
					position++
				}
			l756:
				if buffer[position] != rune('1') {
					goto l746
				}
				position++
				if buffer[position] != rune('2') {
					goto l746
				}
				position++
				if buffer[position] != rune(':') {
					goto l746
				}
				position++
				if !_rules[ruleSymbolName]() {
					goto l746
				}
				add(ruleARMGOTLow12, position747)
			}
			return true
		l746:
			position, tokenIndex = position746, tokenIndex746
			return false
		},
		/* 48 ARMPostincrement <- <'!'> */
		func() bool {
			position758, tokenIndex758 := position, tokenIndex
			{
				position759 := position
				if buffer[position] != rune('!') {
					goto l758
				}
				position++
				add(ruleARMPostincrement, position759)
			}
			return true
		l758:
			position, tokenIndex = position758, tokenIndex758
			return false
		},
		/* 49 BaseIndexScale <- <('(' RegisterOrConstant? WS? (',' WS? RegisterOrConstant WS? (',' [0-9]+)?)? ')')> */
		func() bool {
			position760, tokenIndex760 := position, tokenIndex
			{
				position761 := position
				if buffer[position] != rune('(') {
					goto l760
				}
				position++
				{
					position762, tokenIndex762 := position, tokenIndex
					if !_rules[ruleRegisterOrConstant]() {
						goto l762
					}
					goto l763
				l762:
					position, tokenIndex = position762, tokenIndex762
				}
			l763:
				{
					position764, tokenIndex764 := position, tokenIndex
					if !_rules[ruleWS]() {
						goto l764
					}
					goto l765
				l764:
					position, tokenIndex = position764, tokenIndex764
				}
			l765:
				{
					position766, tokenIndex766 := position, tokenIndex
					if buffer[position] != rune(',') {
						goto l766
					}
					position++
					{
						position768, tokenIndex768 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l768
						}
						goto l769
					l768:
						position, tokenIndex = position768, tokenIndex768
					}
				l769:
					if !_rules[ruleRegisterOrConstant]() {
						goto l766
					}
					{
						position770, tokenIndex770 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l770
						}
						goto l771
					l770:
						position, tokenIndex = position770, tokenIndex770
					}
				l771:
					{
						position772, tokenIndex772 := position, tokenIndex
						if buffer[position] != rune(',') {
							goto l772
						}
						position++
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l772
						}
						position++
					l774:
						{
							position775, tokenIndex775 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l775
							}
							position++
							goto l774
						l775:
							position, tokenIndex = position775, tokenIndex775
						}
						goto l773
					l772:
						position, tokenIndex = position772, tokenIndex772
					}
				l773:
					goto l767
				l766:
					position, tokenIndex = position766, tokenIndex766
				}
			l767:
				if buffer[position] != rune(')') {
					goto l760
				}
				position++
				add(ruleBaseIndexScale, position761)
			}
			return true
		l760:
			position, tokenIndex = position760, tokenIndex760
			return false
		},
		/* 50 Operator <- <('+' / '-')> */
		func() bool {
			position776, tokenIndex776 := position, tokenIndex
			{
				position777 := position
				{
					position778, tokenIndex778 := position, tokenIndex
					if buffer[position] != rune('+') {
						goto l779
					}
					position++
					goto l778
				l779:
					position, tokenIndex = position778, tokenIndex778
					if buffer[position] != rune('-') {
						goto l776
					}
					position++
				}
			l778:
				add(ruleOperator, position777)
			}
			return true
		l776:
			position, tokenIndex = position776, tokenIndex776
			return false
		},
		/* 51 Offset <- <('+'? '-'? (('0' ('b' / 'B') ('0' / '1')+) / ('0' ('x' / 'X') ([0-9] / [0-9] / ([a-f] / [A-F]))+) / [0-9]+))> */
		func() bool {
			position780, tokenIndex780 := position, tokenIndex
			{
				position781 := position
				{
					position782, tokenIndex782 := position, tokenIndex
					if buffer[position] != rune('+') {
						goto l782
					}
					position++
					goto l783
				l782:
					position, tokenIndex = position782, tokenIndex782
				}
			l783:
				{
					position784, tokenIndex784 := position, tokenIndex
					if buffer[position] != rune('-') {
						goto l784
					}
					position++
					goto l785
				l784:
					position, tokenIndex = position784, tokenIndex784
				}
			l785:
				{
					position786, tokenIndex786 := position, tokenIndex
					if buffer[position] != rune('0') {
						goto l787
					}
					position++
					{
						position788, tokenIndex788 := position, tokenIndex
						if buffer[position] != rune('b') {
							goto l789
						}
						position++
						goto l788
					l789:
						position, tokenIndex = position788, tokenIndex788
						if buffer[position] != rune('B') {
							goto l787
						}
						position++
					}
				l788:
					{
						position792, tokenIndex792 := position, tokenIndex
						if buffer[position] != rune('0') {
							goto l793
						}
						position++
						goto l792
					l793:
						position, tokenIndex = position792, tokenIndex792
						if buffer[position] != rune('1') {
							goto l787
						}
						position++
					}
				l792:
				l790:
					{
						position791, tokenIndex791 := position, tokenIndex
						{
							position794, tokenIndex794 := position, tokenIndex
							if buffer[position] != rune('0') {
								goto l795
							}
							position++
							goto l794
						l795:
							position, tokenIndex = position794, tokenIndex794
							if buffer[position] != rune('1') {
								goto l791
							}
							position++
						}
					l794:
						goto l790
					l791:
						position, tokenIndex = position791, tokenIndex791
					}
					goto l786
				l787:
					position, tokenIndex = position786, tokenIndex786
					if buffer[position] != rune('0') {
						goto l796
					}
					position++
					{
						position797, tokenIndex797 := position, tokenIndex
						if buffer[position] != rune('x') {
							goto l798
						}
						position++
						goto l797
					l798:
						position, tokenIndex = position797, tokenIndex797
						if buffer[position] != rune('X') {
							goto l796
						}
						position++
					}
				l797:
					{
						position801, tokenIndex801 := position, tokenIndex
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l802
						}
						position++
						goto l801
					l802:
						position, tokenIndex = position801, tokenIndex801
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l803
						}
						position++
						goto l801
					l803:
						position, tokenIndex = position801, tokenIndex801
						{
							position804, tokenIndex804 := position, tokenIndex
							if c := buffer[position]; c < rune('a') || c > rune('f') {
								goto l805
							}
							position++
							goto l804
						l805:
							position, tokenIndex = position804, tokenIndex804
							if c := buffer[position]; c < rune('A') || c > rune('F') {
								goto l796
							}
							position++
						}
					l804:
					}
				l801:
				l799:
					{
						position800, tokenIndex800 := position, tokenIndex
						{
							position806, tokenIndex806 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l807
							}
							position++
							goto l806
						l807:
							position, tokenIndex = position806, tokenIndex806
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l808
							}
							position++
							goto l806
						l808:
							position, tokenIndex = position806, tokenIndex806
							{
								position809, tokenIndex809 := position, tokenIndex
								if c := buffer[position]; c < rune('a') || c > rune('f') {
									goto l810
								}
								position++
								goto l809
							l810:
								position, tokenIndex = position809, tokenIndex809
								if c := buffer[position]; c < rune('A') || c > rune('F') {
									goto l800
								}
								position++
							}
						l809:
						}
					l806:
						goto l799
					l800:
						position, tokenIndex = position800, tokenIndex800
					}
					goto l786
				l796:
					position, tokenIndex = position786, tokenIndex786
					if c := buffer[position]; c < rune('0') || c > rune('9') {
						goto l780
					}
					position++
				l811:
					{
						position812, tokenIndex812 := position, tokenIndex
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l812
						}
						position++
						goto l811
					l812:
						position, tokenIndex = position812, tokenIndex812
					}
				}
			l786:
				add(ruleOffset, position781)
			}
			return true
		l780:
			position, tokenIndex = position780, tokenIndex780
			return false
		},
		/* 52 Section <- <([a-z] / [A-Z] / '@')+> */
		func() bool {
			position813, tokenIndex813 := position, tokenIndex
			{
				position814 := position
				{
					position817, tokenIndex817 := position, tokenIndex
					if c := buffer[position]; c < rune('a') || c > rune('z') {
						goto l818
					}
					position++
					goto l817
				l818:
					position, tokenIndex = position817, tokenIndex817
					if c := buffer[position]; c < rune('A') || c > rune('Z') {
						goto l819
					}
					position++
					goto l817
				l819:
					position, tokenIndex = position817, tokenIndex817
					if buffer[position] != rune('@') {
						goto l813
					}
					position++
				}
			l817:
			l815:
				{
					position816, tokenIndex816 := position, tokenIndex
					{
						position820, tokenIndex820 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l821
						}
						position++
						goto l820
					l821:
						position, tokenIndex = position820, tokenIndex820
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l822
						}
						position++
						goto l820
					l822:
						position, tokenIndex = position820, tokenIndex820
						if buffer[position] != rune('@') {
							goto l816
						}
						position++
					}
				l820:
					goto l815
				l816:
					position, tokenIndex = position816, tokenIndex816
				}
				add(ruleSection, position814)
			}
			return true
		l813:
			position, tokenIndex = position813, tokenIndex813
			return false
		},
		/* 53 SegmentRegister <- <('%' ([c-g] / 's') ('s' ':'))> */
		func() bool {
			position823, tokenIndex823 := position, tokenIndex
			{
				position824 := position
				if buffer[position] != rune('%') {
					goto l823
				}
				position++
				{
					position825, tokenIndex825 := position, tokenIndex
					if c := buffer[position]; c < rune('c') || c > rune('g') {
						goto l826
					}
					position++
					goto l825
				l826:
					position, tokenIndex = position825, tokenIndex825
					if buffer[position] != rune('s') {
						goto l823
					}
					position++
				}
			l825:
				if buffer[position] != rune('s') {
					goto l823
				}
				position++
				if buffer[position] != rune(':') {
					goto l823
				}
				position++
				add(ruleSegmentRegister, position824)
			}
			return true
		l823:
			position, tokenIndex = position823, tokenIndex823
			return false
		},
	}
	p.rules = _rules
}
