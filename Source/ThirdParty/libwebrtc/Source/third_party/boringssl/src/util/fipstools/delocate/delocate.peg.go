package main

//go:generate peg delocate.peg

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
	ruleArgs
	ruleArg
	ruleQuotedArg
	ruleQuotedText
	ruleLabelContainingDirective
	ruleLabelContainingDirectiveName
	ruleSymbolArgs
	ruleSymbolArg
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
	ruleAVX512Token
	ruleTOCRefHigh
	ruleTOCRefLow
	ruleIndirectionIndicator
	ruleRegisterOrConstant
	ruleMemoryRef
	ruleSymbolRef
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
	"Args",
	"Arg",
	"QuotedArg",
	"QuotedText",
	"LabelContainingDirective",
	"LabelContainingDirectiveName",
	"SymbolArgs",
	"SymbolArg",
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
	"AVX512Token",
	"TOCRefHigh",
	"TOCRefLow",
	"IndirectionIndicator",
	"RegisterOrConstant",
	"MemoryRef",
	"SymbolRef",
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
	rules  [41]func() bool
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
		/* 5 LocationDirective <- <((('.' ('f' / 'F') ('i' / 'I') ('l' / 'L') ('e' / 'E')) / ('.' ('l' / 'L') ('o' / 'O') ('c' / 'C'))) WS (!('#' / '\n') .)+)> */
		func() bool {
			position70, tokenIndex70 := position, tokenIndex
			{
				position71 := position
				{
					position72, tokenIndex72 := position, tokenIndex
					if buffer[position] != rune('.') {
						goto l73
					}
					position++
					{
						position74, tokenIndex74 := position, tokenIndex
						if buffer[position] != rune('f') {
							goto l75
						}
						position++
						goto l74
					l75:
						position, tokenIndex = position74, tokenIndex74
						if buffer[position] != rune('F') {
							goto l73
						}
						position++
					}
				l74:
					{
						position76, tokenIndex76 := position, tokenIndex
						if buffer[position] != rune('i') {
							goto l77
						}
						position++
						goto l76
					l77:
						position, tokenIndex = position76, tokenIndex76
						if buffer[position] != rune('I') {
							goto l73
						}
						position++
					}
				l76:
					{
						position78, tokenIndex78 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l79
						}
						position++
						goto l78
					l79:
						position, tokenIndex = position78, tokenIndex78
						if buffer[position] != rune('L') {
							goto l73
						}
						position++
					}
				l78:
					{
						position80, tokenIndex80 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l81
						}
						position++
						goto l80
					l81:
						position, tokenIndex = position80, tokenIndex80
						if buffer[position] != rune('E') {
							goto l73
						}
						position++
					}
				l80:
					goto l72
				l73:
					position, tokenIndex = position72, tokenIndex72
					if buffer[position] != rune('.') {
						goto l70
					}
					position++
					{
						position82, tokenIndex82 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l83
						}
						position++
						goto l82
					l83:
						position, tokenIndex = position82, tokenIndex82
						if buffer[position] != rune('L') {
							goto l70
						}
						position++
					}
				l82:
					{
						position84, tokenIndex84 := position, tokenIndex
						if buffer[position] != rune('o') {
							goto l85
						}
						position++
						goto l84
					l85:
						position, tokenIndex = position84, tokenIndex84
						if buffer[position] != rune('O') {
							goto l70
						}
						position++
					}
				l84:
					{
						position86, tokenIndex86 := position, tokenIndex
						if buffer[position] != rune('c') {
							goto l87
						}
						position++
						goto l86
					l87:
						position, tokenIndex = position86, tokenIndex86
						if buffer[position] != rune('C') {
							goto l70
						}
						position++
					}
				l86:
				}
			l72:
				if !_rules[ruleWS]() {
					goto l70
				}
				{
					position90, tokenIndex90 := position, tokenIndex
					{
						position91, tokenIndex91 := position, tokenIndex
						if buffer[position] != rune('#') {
							goto l92
						}
						position++
						goto l91
					l92:
						position, tokenIndex = position91, tokenIndex91
						if buffer[position] != rune('\n') {
							goto l90
						}
						position++
					}
				l91:
					goto l70
				l90:
					position, tokenIndex = position90, tokenIndex90
				}
				if !matchDot() {
					goto l70
				}
			l88:
				{
					position89, tokenIndex89 := position, tokenIndex
					{
						position93, tokenIndex93 := position, tokenIndex
						{
							position94, tokenIndex94 := position, tokenIndex
							if buffer[position] != rune('#') {
								goto l95
							}
							position++
							goto l94
						l95:
							position, tokenIndex = position94, tokenIndex94
							if buffer[position] != rune('\n') {
								goto l93
							}
							position++
						}
					l94:
						goto l89
					l93:
						position, tokenIndex = position93, tokenIndex93
					}
					if !matchDot() {
						goto l89
					}
					goto l88
				l89:
					position, tokenIndex = position89, tokenIndex89
				}
				add(ruleLocationDirective, position71)
			}
			return true
		l70:
			position, tokenIndex = position70, tokenIndex70
			return false
		},
		/* 6 Args <- <(Arg (WS? ',' WS? Arg)*)> */
		func() bool {
			position96, tokenIndex96 := position, tokenIndex
			{
				position97 := position
				if !_rules[ruleArg]() {
					goto l96
				}
			l98:
				{
					position99, tokenIndex99 := position, tokenIndex
					{
						position100, tokenIndex100 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l100
						}
						goto l101
					l100:
						position, tokenIndex = position100, tokenIndex100
					}
				l101:
					if buffer[position] != rune(',') {
						goto l99
					}
					position++
					{
						position102, tokenIndex102 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l102
						}
						goto l103
					l102:
						position, tokenIndex = position102, tokenIndex102
					}
				l103:
					if !_rules[ruleArg]() {
						goto l99
					}
					goto l98
				l99:
					position, tokenIndex = position99, tokenIndex99
				}
				add(ruleArgs, position97)
			}
			return true
		l96:
			position, tokenIndex = position96, tokenIndex96
			return false
		},
		/* 7 Arg <- <(QuotedArg / ([0-9] / [0-9] / ([a-z] / [A-Z]) / '%' / '+' / '-' / '*' / '_' / '@' / '.')*)> */
		func() bool {
			{
				position105 := position
				{
					position106, tokenIndex106 := position, tokenIndex
					if !_rules[ruleQuotedArg]() {
						goto l107
					}
					goto l106
				l107:
					position, tokenIndex = position106, tokenIndex106
				l108:
					{
						position109, tokenIndex109 := position, tokenIndex
						{
							position110, tokenIndex110 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l111
							}
							position++
							goto l110
						l111:
							position, tokenIndex = position110, tokenIndex110
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l112
							}
							position++
							goto l110
						l112:
							position, tokenIndex = position110, tokenIndex110
							{
								position114, tokenIndex114 := position, tokenIndex
								if c := buffer[position]; c < rune('a') || c > rune('z') {
									goto l115
								}
								position++
								goto l114
							l115:
								position, tokenIndex = position114, tokenIndex114
								if c := buffer[position]; c < rune('A') || c > rune('Z') {
									goto l113
								}
								position++
							}
						l114:
							goto l110
						l113:
							position, tokenIndex = position110, tokenIndex110
							if buffer[position] != rune('%') {
								goto l116
							}
							position++
							goto l110
						l116:
							position, tokenIndex = position110, tokenIndex110
							if buffer[position] != rune('+') {
								goto l117
							}
							position++
							goto l110
						l117:
							position, tokenIndex = position110, tokenIndex110
							if buffer[position] != rune('-') {
								goto l118
							}
							position++
							goto l110
						l118:
							position, tokenIndex = position110, tokenIndex110
							if buffer[position] != rune('*') {
								goto l119
							}
							position++
							goto l110
						l119:
							position, tokenIndex = position110, tokenIndex110
							if buffer[position] != rune('_') {
								goto l120
							}
							position++
							goto l110
						l120:
							position, tokenIndex = position110, tokenIndex110
							if buffer[position] != rune('@') {
								goto l121
							}
							position++
							goto l110
						l121:
							position, tokenIndex = position110, tokenIndex110
							if buffer[position] != rune('.') {
								goto l109
							}
							position++
						}
					l110:
						goto l108
					l109:
						position, tokenIndex = position109, tokenIndex109
					}
				}
			l106:
				add(ruleArg, position105)
			}
			return true
		},
		/* 8 QuotedArg <- <('"' QuotedText '"')> */
		func() bool {
			position122, tokenIndex122 := position, tokenIndex
			{
				position123 := position
				if buffer[position] != rune('"') {
					goto l122
				}
				position++
				if !_rules[ruleQuotedText]() {
					goto l122
				}
				if buffer[position] != rune('"') {
					goto l122
				}
				position++
				add(ruleQuotedArg, position123)
			}
			return true
		l122:
			position, tokenIndex = position122, tokenIndex122
			return false
		},
		/* 9 QuotedText <- <(EscapedChar / (!'"' .))*> */
		func() bool {
			{
				position125 := position
			l126:
				{
					position127, tokenIndex127 := position, tokenIndex
					{
						position128, tokenIndex128 := position, tokenIndex
						if !_rules[ruleEscapedChar]() {
							goto l129
						}
						goto l128
					l129:
						position, tokenIndex = position128, tokenIndex128
						{
							position130, tokenIndex130 := position, tokenIndex
							if buffer[position] != rune('"') {
								goto l130
							}
							position++
							goto l127
						l130:
							position, tokenIndex = position130, tokenIndex130
						}
						if !matchDot() {
							goto l127
						}
					}
				l128:
					goto l126
				l127:
					position, tokenIndex = position127, tokenIndex127
				}
				add(ruleQuotedText, position125)
			}
			return true
		},
		/* 10 LabelContainingDirective <- <(LabelContainingDirectiveName WS SymbolArgs)> */
		func() bool {
			position131, tokenIndex131 := position, tokenIndex
			{
				position132 := position
				if !_rules[ruleLabelContainingDirectiveName]() {
					goto l131
				}
				if !_rules[ruleWS]() {
					goto l131
				}
				if !_rules[ruleSymbolArgs]() {
					goto l131
				}
				add(ruleLabelContainingDirective, position132)
			}
			return true
		l131:
			position, tokenIndex = position131, tokenIndex131
			return false
		},
		/* 11 LabelContainingDirectiveName <- <(('.' ('l' / 'L') ('o' / 'O') ('n' / 'N') ('g' / 'G')) / ('.' ('s' / 'S') ('e' / 'E') ('t' / 'T')) / ('.' '8' ('b' / 'B') ('y' / 'Y') ('t' / 'T') ('e' / 'E')) / ('.' '4' ('b' / 'B') ('y' / 'Y') ('t' / 'T') ('e' / 'E')) / ('.' ('q' / 'Q') ('u' / 'U') ('a' / 'A') ('d' / 'D')) / ('.' ('t' / 'T') ('c' / 'C')) / ('.' ('l' / 'L') ('o' / 'O') ('c' / 'C') ('a' / 'A') ('l' / 'L') ('e' / 'E') ('n' / 'N') ('t' / 'T') ('r' / 'R') ('y' / 'Y')) / ('.' ('s' / 'S') ('i' / 'I') ('z' / 'Z') ('e' / 'E')) / ('.' ('t' / 'T') ('y' / 'Y') ('p' / 'P') ('e' / 'E')))> */
		func() bool {
			position133, tokenIndex133 := position, tokenIndex
			{
				position134 := position
				{
					position135, tokenIndex135 := position, tokenIndex
					if buffer[position] != rune('.') {
						goto l136
					}
					position++
					{
						position137, tokenIndex137 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l138
						}
						position++
						goto l137
					l138:
						position, tokenIndex = position137, tokenIndex137
						if buffer[position] != rune('L') {
							goto l136
						}
						position++
					}
				l137:
					{
						position139, tokenIndex139 := position, tokenIndex
						if buffer[position] != rune('o') {
							goto l140
						}
						position++
						goto l139
					l140:
						position, tokenIndex = position139, tokenIndex139
						if buffer[position] != rune('O') {
							goto l136
						}
						position++
					}
				l139:
					{
						position141, tokenIndex141 := position, tokenIndex
						if buffer[position] != rune('n') {
							goto l142
						}
						position++
						goto l141
					l142:
						position, tokenIndex = position141, tokenIndex141
						if buffer[position] != rune('N') {
							goto l136
						}
						position++
					}
				l141:
					{
						position143, tokenIndex143 := position, tokenIndex
						if buffer[position] != rune('g') {
							goto l144
						}
						position++
						goto l143
					l144:
						position, tokenIndex = position143, tokenIndex143
						if buffer[position] != rune('G') {
							goto l136
						}
						position++
					}
				l143:
					goto l135
				l136:
					position, tokenIndex = position135, tokenIndex135
					if buffer[position] != rune('.') {
						goto l145
					}
					position++
					{
						position146, tokenIndex146 := position, tokenIndex
						if buffer[position] != rune('s') {
							goto l147
						}
						position++
						goto l146
					l147:
						position, tokenIndex = position146, tokenIndex146
						if buffer[position] != rune('S') {
							goto l145
						}
						position++
					}
				l146:
					{
						position148, tokenIndex148 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l149
						}
						position++
						goto l148
					l149:
						position, tokenIndex = position148, tokenIndex148
						if buffer[position] != rune('E') {
							goto l145
						}
						position++
					}
				l148:
					{
						position150, tokenIndex150 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l151
						}
						position++
						goto l150
					l151:
						position, tokenIndex = position150, tokenIndex150
						if buffer[position] != rune('T') {
							goto l145
						}
						position++
					}
				l150:
					goto l135
				l145:
					position, tokenIndex = position135, tokenIndex135
					if buffer[position] != rune('.') {
						goto l152
					}
					position++
					if buffer[position] != rune('8') {
						goto l152
					}
					position++
					{
						position153, tokenIndex153 := position, tokenIndex
						if buffer[position] != rune('b') {
							goto l154
						}
						position++
						goto l153
					l154:
						position, tokenIndex = position153, tokenIndex153
						if buffer[position] != rune('B') {
							goto l152
						}
						position++
					}
				l153:
					{
						position155, tokenIndex155 := position, tokenIndex
						if buffer[position] != rune('y') {
							goto l156
						}
						position++
						goto l155
					l156:
						position, tokenIndex = position155, tokenIndex155
						if buffer[position] != rune('Y') {
							goto l152
						}
						position++
					}
				l155:
					{
						position157, tokenIndex157 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l158
						}
						position++
						goto l157
					l158:
						position, tokenIndex = position157, tokenIndex157
						if buffer[position] != rune('T') {
							goto l152
						}
						position++
					}
				l157:
					{
						position159, tokenIndex159 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l160
						}
						position++
						goto l159
					l160:
						position, tokenIndex = position159, tokenIndex159
						if buffer[position] != rune('E') {
							goto l152
						}
						position++
					}
				l159:
					goto l135
				l152:
					position, tokenIndex = position135, tokenIndex135
					if buffer[position] != rune('.') {
						goto l161
					}
					position++
					if buffer[position] != rune('4') {
						goto l161
					}
					position++
					{
						position162, tokenIndex162 := position, tokenIndex
						if buffer[position] != rune('b') {
							goto l163
						}
						position++
						goto l162
					l163:
						position, tokenIndex = position162, tokenIndex162
						if buffer[position] != rune('B') {
							goto l161
						}
						position++
					}
				l162:
					{
						position164, tokenIndex164 := position, tokenIndex
						if buffer[position] != rune('y') {
							goto l165
						}
						position++
						goto l164
					l165:
						position, tokenIndex = position164, tokenIndex164
						if buffer[position] != rune('Y') {
							goto l161
						}
						position++
					}
				l164:
					{
						position166, tokenIndex166 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l167
						}
						position++
						goto l166
					l167:
						position, tokenIndex = position166, tokenIndex166
						if buffer[position] != rune('T') {
							goto l161
						}
						position++
					}
				l166:
					{
						position168, tokenIndex168 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l169
						}
						position++
						goto l168
					l169:
						position, tokenIndex = position168, tokenIndex168
						if buffer[position] != rune('E') {
							goto l161
						}
						position++
					}
				l168:
					goto l135
				l161:
					position, tokenIndex = position135, tokenIndex135
					if buffer[position] != rune('.') {
						goto l170
					}
					position++
					{
						position171, tokenIndex171 := position, tokenIndex
						if buffer[position] != rune('q') {
							goto l172
						}
						position++
						goto l171
					l172:
						position, tokenIndex = position171, tokenIndex171
						if buffer[position] != rune('Q') {
							goto l170
						}
						position++
					}
				l171:
					{
						position173, tokenIndex173 := position, tokenIndex
						if buffer[position] != rune('u') {
							goto l174
						}
						position++
						goto l173
					l174:
						position, tokenIndex = position173, tokenIndex173
						if buffer[position] != rune('U') {
							goto l170
						}
						position++
					}
				l173:
					{
						position175, tokenIndex175 := position, tokenIndex
						if buffer[position] != rune('a') {
							goto l176
						}
						position++
						goto l175
					l176:
						position, tokenIndex = position175, tokenIndex175
						if buffer[position] != rune('A') {
							goto l170
						}
						position++
					}
				l175:
					{
						position177, tokenIndex177 := position, tokenIndex
						if buffer[position] != rune('d') {
							goto l178
						}
						position++
						goto l177
					l178:
						position, tokenIndex = position177, tokenIndex177
						if buffer[position] != rune('D') {
							goto l170
						}
						position++
					}
				l177:
					goto l135
				l170:
					position, tokenIndex = position135, tokenIndex135
					if buffer[position] != rune('.') {
						goto l179
					}
					position++
					{
						position180, tokenIndex180 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l181
						}
						position++
						goto l180
					l181:
						position, tokenIndex = position180, tokenIndex180
						if buffer[position] != rune('T') {
							goto l179
						}
						position++
					}
				l180:
					{
						position182, tokenIndex182 := position, tokenIndex
						if buffer[position] != rune('c') {
							goto l183
						}
						position++
						goto l182
					l183:
						position, tokenIndex = position182, tokenIndex182
						if buffer[position] != rune('C') {
							goto l179
						}
						position++
					}
				l182:
					goto l135
				l179:
					position, tokenIndex = position135, tokenIndex135
					if buffer[position] != rune('.') {
						goto l184
					}
					position++
					{
						position185, tokenIndex185 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l186
						}
						position++
						goto l185
					l186:
						position, tokenIndex = position185, tokenIndex185
						if buffer[position] != rune('L') {
							goto l184
						}
						position++
					}
				l185:
					{
						position187, tokenIndex187 := position, tokenIndex
						if buffer[position] != rune('o') {
							goto l188
						}
						position++
						goto l187
					l188:
						position, tokenIndex = position187, tokenIndex187
						if buffer[position] != rune('O') {
							goto l184
						}
						position++
					}
				l187:
					{
						position189, tokenIndex189 := position, tokenIndex
						if buffer[position] != rune('c') {
							goto l190
						}
						position++
						goto l189
					l190:
						position, tokenIndex = position189, tokenIndex189
						if buffer[position] != rune('C') {
							goto l184
						}
						position++
					}
				l189:
					{
						position191, tokenIndex191 := position, tokenIndex
						if buffer[position] != rune('a') {
							goto l192
						}
						position++
						goto l191
					l192:
						position, tokenIndex = position191, tokenIndex191
						if buffer[position] != rune('A') {
							goto l184
						}
						position++
					}
				l191:
					{
						position193, tokenIndex193 := position, tokenIndex
						if buffer[position] != rune('l') {
							goto l194
						}
						position++
						goto l193
					l194:
						position, tokenIndex = position193, tokenIndex193
						if buffer[position] != rune('L') {
							goto l184
						}
						position++
					}
				l193:
					{
						position195, tokenIndex195 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l196
						}
						position++
						goto l195
					l196:
						position, tokenIndex = position195, tokenIndex195
						if buffer[position] != rune('E') {
							goto l184
						}
						position++
					}
				l195:
					{
						position197, tokenIndex197 := position, tokenIndex
						if buffer[position] != rune('n') {
							goto l198
						}
						position++
						goto l197
					l198:
						position, tokenIndex = position197, tokenIndex197
						if buffer[position] != rune('N') {
							goto l184
						}
						position++
					}
				l197:
					{
						position199, tokenIndex199 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l200
						}
						position++
						goto l199
					l200:
						position, tokenIndex = position199, tokenIndex199
						if buffer[position] != rune('T') {
							goto l184
						}
						position++
					}
				l199:
					{
						position201, tokenIndex201 := position, tokenIndex
						if buffer[position] != rune('r') {
							goto l202
						}
						position++
						goto l201
					l202:
						position, tokenIndex = position201, tokenIndex201
						if buffer[position] != rune('R') {
							goto l184
						}
						position++
					}
				l201:
					{
						position203, tokenIndex203 := position, tokenIndex
						if buffer[position] != rune('y') {
							goto l204
						}
						position++
						goto l203
					l204:
						position, tokenIndex = position203, tokenIndex203
						if buffer[position] != rune('Y') {
							goto l184
						}
						position++
					}
				l203:
					goto l135
				l184:
					position, tokenIndex = position135, tokenIndex135
					if buffer[position] != rune('.') {
						goto l205
					}
					position++
					{
						position206, tokenIndex206 := position, tokenIndex
						if buffer[position] != rune('s') {
							goto l207
						}
						position++
						goto l206
					l207:
						position, tokenIndex = position206, tokenIndex206
						if buffer[position] != rune('S') {
							goto l205
						}
						position++
					}
				l206:
					{
						position208, tokenIndex208 := position, tokenIndex
						if buffer[position] != rune('i') {
							goto l209
						}
						position++
						goto l208
					l209:
						position, tokenIndex = position208, tokenIndex208
						if buffer[position] != rune('I') {
							goto l205
						}
						position++
					}
				l208:
					{
						position210, tokenIndex210 := position, tokenIndex
						if buffer[position] != rune('z') {
							goto l211
						}
						position++
						goto l210
					l211:
						position, tokenIndex = position210, tokenIndex210
						if buffer[position] != rune('Z') {
							goto l205
						}
						position++
					}
				l210:
					{
						position212, tokenIndex212 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l213
						}
						position++
						goto l212
					l213:
						position, tokenIndex = position212, tokenIndex212
						if buffer[position] != rune('E') {
							goto l205
						}
						position++
					}
				l212:
					goto l135
				l205:
					position, tokenIndex = position135, tokenIndex135
					if buffer[position] != rune('.') {
						goto l133
					}
					position++
					{
						position214, tokenIndex214 := position, tokenIndex
						if buffer[position] != rune('t') {
							goto l215
						}
						position++
						goto l214
					l215:
						position, tokenIndex = position214, tokenIndex214
						if buffer[position] != rune('T') {
							goto l133
						}
						position++
					}
				l214:
					{
						position216, tokenIndex216 := position, tokenIndex
						if buffer[position] != rune('y') {
							goto l217
						}
						position++
						goto l216
					l217:
						position, tokenIndex = position216, tokenIndex216
						if buffer[position] != rune('Y') {
							goto l133
						}
						position++
					}
				l216:
					{
						position218, tokenIndex218 := position, tokenIndex
						if buffer[position] != rune('p') {
							goto l219
						}
						position++
						goto l218
					l219:
						position, tokenIndex = position218, tokenIndex218
						if buffer[position] != rune('P') {
							goto l133
						}
						position++
					}
				l218:
					{
						position220, tokenIndex220 := position, tokenIndex
						if buffer[position] != rune('e') {
							goto l221
						}
						position++
						goto l220
					l221:
						position, tokenIndex = position220, tokenIndex220
						if buffer[position] != rune('E') {
							goto l133
						}
						position++
					}
				l220:
				}
			l135:
				add(ruleLabelContainingDirectiveName, position134)
			}
			return true
		l133:
			position, tokenIndex = position133, tokenIndex133
			return false
		},
		/* 12 SymbolArgs <- <(SymbolArg (WS? ',' WS? SymbolArg)*)> */
		func() bool {
			position222, tokenIndex222 := position, tokenIndex
			{
				position223 := position
				if !_rules[ruleSymbolArg]() {
					goto l222
				}
			l224:
				{
					position225, tokenIndex225 := position, tokenIndex
					{
						position226, tokenIndex226 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l226
						}
						goto l227
					l226:
						position, tokenIndex = position226, tokenIndex226
					}
				l227:
					if buffer[position] != rune(',') {
						goto l225
					}
					position++
					{
						position228, tokenIndex228 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l228
						}
						goto l229
					l228:
						position, tokenIndex = position228, tokenIndex228
					}
				l229:
					if !_rules[ruleSymbolArg]() {
						goto l225
					}
					goto l224
				l225:
					position, tokenIndex = position225, tokenIndex225
				}
				add(ruleSymbolArgs, position223)
			}
			return true
		l222:
			position, tokenIndex = position222, tokenIndex222
			return false
		},
		/* 13 SymbolArg <- <(Offset / SymbolType / ((Offset / LocalSymbol / SymbolName / Dot) WS? Operator WS? (Offset / LocalSymbol / SymbolName)) / (LocalSymbol TCMarker?) / (SymbolName Offset) / (SymbolName TCMarker?))> */
		func() bool {
			position230, tokenIndex230 := position, tokenIndex
			{
				position231 := position
				{
					position232, tokenIndex232 := position, tokenIndex
					if !_rules[ruleOffset]() {
						goto l233
					}
					goto l232
				l233:
					position, tokenIndex = position232, tokenIndex232
					if !_rules[ruleSymbolType]() {
						goto l234
					}
					goto l232
				l234:
					position, tokenIndex = position232, tokenIndex232
					{
						position236, tokenIndex236 := position, tokenIndex
						if !_rules[ruleOffset]() {
							goto l237
						}
						goto l236
					l237:
						position, tokenIndex = position236, tokenIndex236
						if !_rules[ruleLocalSymbol]() {
							goto l238
						}
						goto l236
					l238:
						position, tokenIndex = position236, tokenIndex236
						if !_rules[ruleSymbolName]() {
							goto l239
						}
						goto l236
					l239:
						position, tokenIndex = position236, tokenIndex236
						if !_rules[ruleDot]() {
							goto l235
						}
					}
				l236:
					{
						position240, tokenIndex240 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l240
						}
						goto l241
					l240:
						position, tokenIndex = position240, tokenIndex240
					}
				l241:
					if !_rules[ruleOperator]() {
						goto l235
					}
					{
						position242, tokenIndex242 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l242
						}
						goto l243
					l242:
						position, tokenIndex = position242, tokenIndex242
					}
				l243:
					{
						position244, tokenIndex244 := position, tokenIndex
						if !_rules[ruleOffset]() {
							goto l245
						}
						goto l244
					l245:
						position, tokenIndex = position244, tokenIndex244
						if !_rules[ruleLocalSymbol]() {
							goto l246
						}
						goto l244
					l246:
						position, tokenIndex = position244, tokenIndex244
						if !_rules[ruleSymbolName]() {
							goto l235
						}
					}
				l244:
					goto l232
				l235:
					position, tokenIndex = position232, tokenIndex232
					if !_rules[ruleLocalSymbol]() {
						goto l247
					}
					{
						position248, tokenIndex248 := position, tokenIndex
						if !_rules[ruleTCMarker]() {
							goto l248
						}
						goto l249
					l248:
						position, tokenIndex = position248, tokenIndex248
					}
				l249:
					goto l232
				l247:
					position, tokenIndex = position232, tokenIndex232
					if !_rules[ruleSymbolName]() {
						goto l250
					}
					if !_rules[ruleOffset]() {
						goto l250
					}
					goto l232
				l250:
					position, tokenIndex = position232, tokenIndex232
					if !_rules[ruleSymbolName]() {
						goto l230
					}
					{
						position251, tokenIndex251 := position, tokenIndex
						if !_rules[ruleTCMarker]() {
							goto l251
						}
						goto l252
					l251:
						position, tokenIndex = position251, tokenIndex251
					}
				l252:
				}
			l232:
				add(ruleSymbolArg, position231)
			}
			return true
		l230:
			position, tokenIndex = position230, tokenIndex230
			return false
		},
		/* 14 SymbolType <- <(('@' 'f' 'u' 'n' 'c' 't' 'i' 'o' 'n') / ('@' 'o' 'b' 'j' 'e' 'c' 't'))> */
		func() bool {
			position253, tokenIndex253 := position, tokenIndex
			{
				position254 := position
				{
					position255, tokenIndex255 := position, tokenIndex
					if buffer[position] != rune('@') {
						goto l256
					}
					position++
					if buffer[position] != rune('f') {
						goto l256
					}
					position++
					if buffer[position] != rune('u') {
						goto l256
					}
					position++
					if buffer[position] != rune('n') {
						goto l256
					}
					position++
					if buffer[position] != rune('c') {
						goto l256
					}
					position++
					if buffer[position] != rune('t') {
						goto l256
					}
					position++
					if buffer[position] != rune('i') {
						goto l256
					}
					position++
					if buffer[position] != rune('o') {
						goto l256
					}
					position++
					if buffer[position] != rune('n') {
						goto l256
					}
					position++
					goto l255
				l256:
					position, tokenIndex = position255, tokenIndex255
					if buffer[position] != rune('@') {
						goto l253
					}
					position++
					if buffer[position] != rune('o') {
						goto l253
					}
					position++
					if buffer[position] != rune('b') {
						goto l253
					}
					position++
					if buffer[position] != rune('j') {
						goto l253
					}
					position++
					if buffer[position] != rune('e') {
						goto l253
					}
					position++
					if buffer[position] != rune('c') {
						goto l253
					}
					position++
					if buffer[position] != rune('t') {
						goto l253
					}
					position++
				}
			l255:
				add(ruleSymbolType, position254)
			}
			return true
		l253:
			position, tokenIndex = position253, tokenIndex253
			return false
		},
		/* 15 Dot <- <'.'> */
		func() bool {
			position257, tokenIndex257 := position, tokenIndex
			{
				position258 := position
				if buffer[position] != rune('.') {
					goto l257
				}
				position++
				add(ruleDot, position258)
			}
			return true
		l257:
			position, tokenIndex = position257, tokenIndex257
			return false
		},
		/* 16 TCMarker <- <('[' 'T' 'C' ']')> */
		func() bool {
			position259, tokenIndex259 := position, tokenIndex
			{
				position260 := position
				if buffer[position] != rune('[') {
					goto l259
				}
				position++
				if buffer[position] != rune('T') {
					goto l259
				}
				position++
				if buffer[position] != rune('C') {
					goto l259
				}
				position++
				if buffer[position] != rune(']') {
					goto l259
				}
				position++
				add(ruleTCMarker, position260)
			}
			return true
		l259:
			position, tokenIndex = position259, tokenIndex259
			return false
		},
		/* 17 EscapedChar <- <('\\' .)> */
		func() bool {
			position261, tokenIndex261 := position, tokenIndex
			{
				position262 := position
				if buffer[position] != rune('\\') {
					goto l261
				}
				position++
				if !matchDot() {
					goto l261
				}
				add(ruleEscapedChar, position262)
			}
			return true
		l261:
			position, tokenIndex = position261, tokenIndex261
			return false
		},
		/* 18 WS <- <(' ' / '\t')+> */
		func() bool {
			position263, tokenIndex263 := position, tokenIndex
			{
				position264 := position
				{
					position267, tokenIndex267 := position, tokenIndex
					if buffer[position] != rune(' ') {
						goto l268
					}
					position++
					goto l267
				l268:
					position, tokenIndex = position267, tokenIndex267
					if buffer[position] != rune('\t') {
						goto l263
					}
					position++
				}
			l267:
			l265:
				{
					position266, tokenIndex266 := position, tokenIndex
					{
						position269, tokenIndex269 := position, tokenIndex
						if buffer[position] != rune(' ') {
							goto l270
						}
						position++
						goto l269
					l270:
						position, tokenIndex = position269, tokenIndex269
						if buffer[position] != rune('\t') {
							goto l266
						}
						position++
					}
				l269:
					goto l265
				l266:
					position, tokenIndex = position266, tokenIndex266
				}
				add(ruleWS, position264)
			}
			return true
		l263:
			position, tokenIndex = position263, tokenIndex263
			return false
		},
		/* 19 Comment <- <('#' (!'\n' .)*)> */
		func() bool {
			position271, tokenIndex271 := position, tokenIndex
			{
				position272 := position
				if buffer[position] != rune('#') {
					goto l271
				}
				position++
			l273:
				{
					position274, tokenIndex274 := position, tokenIndex
					{
						position275, tokenIndex275 := position, tokenIndex
						if buffer[position] != rune('\n') {
							goto l275
						}
						position++
						goto l274
					l275:
						position, tokenIndex = position275, tokenIndex275
					}
					if !matchDot() {
						goto l274
					}
					goto l273
				l274:
					position, tokenIndex = position274, tokenIndex274
				}
				add(ruleComment, position272)
			}
			return true
		l271:
			position, tokenIndex = position271, tokenIndex271
			return false
		},
		/* 20 Label <- <((LocalSymbol / LocalLabel / SymbolName) ':')> */
		func() bool {
			position276, tokenIndex276 := position, tokenIndex
			{
				position277 := position
				{
					position278, tokenIndex278 := position, tokenIndex
					if !_rules[ruleLocalSymbol]() {
						goto l279
					}
					goto l278
				l279:
					position, tokenIndex = position278, tokenIndex278
					if !_rules[ruleLocalLabel]() {
						goto l280
					}
					goto l278
				l280:
					position, tokenIndex = position278, tokenIndex278
					if !_rules[ruleSymbolName]() {
						goto l276
					}
				}
			l278:
				if buffer[position] != rune(':') {
					goto l276
				}
				position++
				add(ruleLabel, position277)
			}
			return true
		l276:
			position, tokenIndex = position276, tokenIndex276
			return false
		},
		/* 21 SymbolName <- <(([a-z] / [A-Z] / '.' / '_') ([a-z] / [A-Z] / '.' / ([0-9] / [0-9]) / '$' / '_')*)> */
		func() bool {
			position281, tokenIndex281 := position, tokenIndex
			{
				position282 := position
				{
					position283, tokenIndex283 := position, tokenIndex
					if c := buffer[position]; c < rune('a') || c > rune('z') {
						goto l284
					}
					position++
					goto l283
				l284:
					position, tokenIndex = position283, tokenIndex283
					if c := buffer[position]; c < rune('A') || c > rune('Z') {
						goto l285
					}
					position++
					goto l283
				l285:
					position, tokenIndex = position283, tokenIndex283
					if buffer[position] != rune('.') {
						goto l286
					}
					position++
					goto l283
				l286:
					position, tokenIndex = position283, tokenIndex283
					if buffer[position] != rune('_') {
						goto l281
					}
					position++
				}
			l283:
			l287:
				{
					position288, tokenIndex288 := position, tokenIndex
					{
						position289, tokenIndex289 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l290
						}
						position++
						goto l289
					l290:
						position, tokenIndex = position289, tokenIndex289
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l291
						}
						position++
						goto l289
					l291:
						position, tokenIndex = position289, tokenIndex289
						if buffer[position] != rune('.') {
							goto l292
						}
						position++
						goto l289
					l292:
						position, tokenIndex = position289, tokenIndex289
						{
							position294, tokenIndex294 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l295
							}
							position++
							goto l294
						l295:
							position, tokenIndex = position294, tokenIndex294
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l293
							}
							position++
						}
					l294:
						goto l289
					l293:
						position, tokenIndex = position289, tokenIndex289
						if buffer[position] != rune('$') {
							goto l296
						}
						position++
						goto l289
					l296:
						position, tokenIndex = position289, tokenIndex289
						if buffer[position] != rune('_') {
							goto l288
						}
						position++
					}
				l289:
					goto l287
				l288:
					position, tokenIndex = position288, tokenIndex288
				}
				add(ruleSymbolName, position282)
			}
			return true
		l281:
			position, tokenIndex = position281, tokenIndex281
			return false
		},
		/* 22 LocalSymbol <- <('.' 'L' ([a-z] / [A-Z] / '.' / ([0-9] / [0-9]) / '$' / '_')+)> */
		func() bool {
			position297, tokenIndex297 := position, tokenIndex
			{
				position298 := position
				if buffer[position] != rune('.') {
					goto l297
				}
				position++
				if buffer[position] != rune('L') {
					goto l297
				}
				position++
				{
					position301, tokenIndex301 := position, tokenIndex
					if c := buffer[position]; c < rune('a') || c > rune('z') {
						goto l302
					}
					position++
					goto l301
				l302:
					position, tokenIndex = position301, tokenIndex301
					if c := buffer[position]; c < rune('A') || c > rune('Z') {
						goto l303
					}
					position++
					goto l301
				l303:
					position, tokenIndex = position301, tokenIndex301
					if buffer[position] != rune('.') {
						goto l304
					}
					position++
					goto l301
				l304:
					position, tokenIndex = position301, tokenIndex301
					{
						position306, tokenIndex306 := position, tokenIndex
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l307
						}
						position++
						goto l306
					l307:
						position, tokenIndex = position306, tokenIndex306
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l305
						}
						position++
					}
				l306:
					goto l301
				l305:
					position, tokenIndex = position301, tokenIndex301
					if buffer[position] != rune('$') {
						goto l308
					}
					position++
					goto l301
				l308:
					position, tokenIndex = position301, tokenIndex301
					if buffer[position] != rune('_') {
						goto l297
					}
					position++
				}
			l301:
			l299:
				{
					position300, tokenIndex300 := position, tokenIndex
					{
						position309, tokenIndex309 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l310
						}
						position++
						goto l309
					l310:
						position, tokenIndex = position309, tokenIndex309
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l311
						}
						position++
						goto l309
					l311:
						position, tokenIndex = position309, tokenIndex309
						if buffer[position] != rune('.') {
							goto l312
						}
						position++
						goto l309
					l312:
						position, tokenIndex = position309, tokenIndex309
						{
							position314, tokenIndex314 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l315
							}
							position++
							goto l314
						l315:
							position, tokenIndex = position314, tokenIndex314
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l313
							}
							position++
						}
					l314:
						goto l309
					l313:
						position, tokenIndex = position309, tokenIndex309
						if buffer[position] != rune('$') {
							goto l316
						}
						position++
						goto l309
					l316:
						position, tokenIndex = position309, tokenIndex309
						if buffer[position] != rune('_') {
							goto l300
						}
						position++
					}
				l309:
					goto l299
				l300:
					position, tokenIndex = position300, tokenIndex300
				}
				add(ruleLocalSymbol, position298)
			}
			return true
		l297:
			position, tokenIndex = position297, tokenIndex297
			return false
		},
		/* 23 LocalLabel <- <([0-9] ([0-9] / '$')*)> */
		func() bool {
			position317, tokenIndex317 := position, tokenIndex
			{
				position318 := position
				if c := buffer[position]; c < rune('0') || c > rune('9') {
					goto l317
				}
				position++
			l319:
				{
					position320, tokenIndex320 := position, tokenIndex
					{
						position321, tokenIndex321 := position, tokenIndex
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l322
						}
						position++
						goto l321
					l322:
						position, tokenIndex = position321, tokenIndex321
						if buffer[position] != rune('$') {
							goto l320
						}
						position++
					}
				l321:
					goto l319
				l320:
					position, tokenIndex = position320, tokenIndex320
				}
				add(ruleLocalLabel, position318)
			}
			return true
		l317:
			position, tokenIndex = position317, tokenIndex317
			return false
		},
		/* 24 LocalLabelRef <- <([0-9] ([0-9] / '$')* ('b' / 'f'))> */
		func() bool {
			position323, tokenIndex323 := position, tokenIndex
			{
				position324 := position
				if c := buffer[position]; c < rune('0') || c > rune('9') {
					goto l323
				}
				position++
			l325:
				{
					position326, tokenIndex326 := position, tokenIndex
					{
						position327, tokenIndex327 := position, tokenIndex
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l328
						}
						position++
						goto l327
					l328:
						position, tokenIndex = position327, tokenIndex327
						if buffer[position] != rune('$') {
							goto l326
						}
						position++
					}
				l327:
					goto l325
				l326:
					position, tokenIndex = position326, tokenIndex326
				}
				{
					position329, tokenIndex329 := position, tokenIndex
					if buffer[position] != rune('b') {
						goto l330
					}
					position++
					goto l329
				l330:
					position, tokenIndex = position329, tokenIndex329
					if buffer[position] != rune('f') {
						goto l323
					}
					position++
				}
			l329:
				add(ruleLocalLabelRef, position324)
			}
			return true
		l323:
			position, tokenIndex = position323, tokenIndex323
			return false
		},
		/* 25 Instruction <- <(InstructionName (WS InstructionArg (WS? ',' WS? InstructionArg)*)?)> */
		func() bool {
			position331, tokenIndex331 := position, tokenIndex
			{
				position332 := position
				if !_rules[ruleInstructionName]() {
					goto l331
				}
				{
					position333, tokenIndex333 := position, tokenIndex
					if !_rules[ruleWS]() {
						goto l333
					}
					if !_rules[ruleInstructionArg]() {
						goto l333
					}
				l335:
					{
						position336, tokenIndex336 := position, tokenIndex
						{
							position337, tokenIndex337 := position, tokenIndex
							if !_rules[ruleWS]() {
								goto l337
							}
							goto l338
						l337:
							position, tokenIndex = position337, tokenIndex337
						}
					l338:
						if buffer[position] != rune(',') {
							goto l336
						}
						position++
						{
							position339, tokenIndex339 := position, tokenIndex
							if !_rules[ruleWS]() {
								goto l339
							}
							goto l340
						l339:
							position, tokenIndex = position339, tokenIndex339
						}
					l340:
						if !_rules[ruleInstructionArg]() {
							goto l336
						}
						goto l335
					l336:
						position, tokenIndex = position336, tokenIndex336
					}
					goto l334
				l333:
					position, tokenIndex = position333, tokenIndex333
				}
			l334:
				add(ruleInstruction, position332)
			}
			return true
		l331:
			position, tokenIndex = position331, tokenIndex331
			return false
		},
		/* 26 InstructionName <- <(([a-z] / [A-Z]) ([a-z] / [A-Z] / ([0-9] / [0-9]))* ('.' / '+' / '-')?)> */
		func() bool {
			position341, tokenIndex341 := position, tokenIndex
			{
				position342 := position
				{
					position343, tokenIndex343 := position, tokenIndex
					if c := buffer[position]; c < rune('a') || c > rune('z') {
						goto l344
					}
					position++
					goto l343
				l344:
					position, tokenIndex = position343, tokenIndex343
					if c := buffer[position]; c < rune('A') || c > rune('Z') {
						goto l341
					}
					position++
				}
			l343:
			l345:
				{
					position346, tokenIndex346 := position, tokenIndex
					{
						position347, tokenIndex347 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l348
						}
						position++
						goto l347
					l348:
						position, tokenIndex = position347, tokenIndex347
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l349
						}
						position++
						goto l347
					l349:
						position, tokenIndex = position347, tokenIndex347
						{
							position350, tokenIndex350 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l351
							}
							position++
							goto l350
						l351:
							position, tokenIndex = position350, tokenIndex350
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l346
							}
							position++
						}
					l350:
					}
				l347:
					goto l345
				l346:
					position, tokenIndex = position346, tokenIndex346
				}
				{
					position352, tokenIndex352 := position, tokenIndex
					{
						position354, tokenIndex354 := position, tokenIndex
						if buffer[position] != rune('.') {
							goto l355
						}
						position++
						goto l354
					l355:
						position, tokenIndex = position354, tokenIndex354
						if buffer[position] != rune('+') {
							goto l356
						}
						position++
						goto l354
					l356:
						position, tokenIndex = position354, tokenIndex354
						if buffer[position] != rune('-') {
							goto l352
						}
						position++
					}
				l354:
					goto l353
				l352:
					position, tokenIndex = position352, tokenIndex352
				}
			l353:
				add(ruleInstructionName, position342)
			}
			return true
		l341:
			position, tokenIndex = position341, tokenIndex341
			return false
		},
		/* 27 InstructionArg <- <(IndirectionIndicator? (RegisterOrConstant / LocalLabelRef / TOCRefHigh / TOCRefLow / MemoryRef) AVX512Token*)> */
		func() bool {
			position357, tokenIndex357 := position, tokenIndex
			{
				position358 := position
				{
					position359, tokenIndex359 := position, tokenIndex
					if !_rules[ruleIndirectionIndicator]() {
						goto l359
					}
					goto l360
				l359:
					position, tokenIndex = position359, tokenIndex359
				}
			l360:
				{
					position361, tokenIndex361 := position, tokenIndex
					if !_rules[ruleRegisterOrConstant]() {
						goto l362
					}
					goto l361
				l362:
					position, tokenIndex = position361, tokenIndex361
					if !_rules[ruleLocalLabelRef]() {
						goto l363
					}
					goto l361
				l363:
					position, tokenIndex = position361, tokenIndex361
					if !_rules[ruleTOCRefHigh]() {
						goto l364
					}
					goto l361
				l364:
					position, tokenIndex = position361, tokenIndex361
					if !_rules[ruleTOCRefLow]() {
						goto l365
					}
					goto l361
				l365:
					position, tokenIndex = position361, tokenIndex361
					if !_rules[ruleMemoryRef]() {
						goto l357
					}
				}
			l361:
			l366:
				{
					position367, tokenIndex367 := position, tokenIndex
					if !_rules[ruleAVX512Token]() {
						goto l367
					}
					goto l366
				l367:
					position, tokenIndex = position367, tokenIndex367
				}
				add(ruleInstructionArg, position358)
			}
			return true
		l357:
			position, tokenIndex = position357, tokenIndex357
			return false
		},
		/* 28 AVX512Token <- <(WS? '{' '%'? ([0-9] / [a-z])* '}')> */
		func() bool {
			position368, tokenIndex368 := position, tokenIndex
			{
				position369 := position
				{
					position370, tokenIndex370 := position, tokenIndex
					if !_rules[ruleWS]() {
						goto l370
					}
					goto l371
				l370:
					position, tokenIndex = position370, tokenIndex370
				}
			l371:
				if buffer[position] != rune('{') {
					goto l368
				}
				position++
				{
					position372, tokenIndex372 := position, tokenIndex
					if buffer[position] != rune('%') {
						goto l372
					}
					position++
					goto l373
				l372:
					position, tokenIndex = position372, tokenIndex372
				}
			l373:
			l374:
				{
					position375, tokenIndex375 := position, tokenIndex
					{
						position376, tokenIndex376 := position, tokenIndex
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l377
						}
						position++
						goto l376
					l377:
						position, tokenIndex = position376, tokenIndex376
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l375
						}
						position++
					}
				l376:
					goto l374
				l375:
					position, tokenIndex = position375, tokenIndex375
				}
				if buffer[position] != rune('}') {
					goto l368
				}
				position++
				add(ruleAVX512Token, position369)
			}
			return true
		l368:
			position, tokenIndex = position368, tokenIndex368
			return false
		},
		/* 29 TOCRefHigh <- <('.' 'T' 'O' 'C' '.' '-' (('0' 'b') / ('.' 'L' ([a-z] / [A-Z] / '_' / [0-9])+)) ('@' ('h' / 'H') ('a' / 'A')))> */
		func() bool {
			position378, tokenIndex378 := position, tokenIndex
			{
				position379 := position
				if buffer[position] != rune('.') {
					goto l378
				}
				position++
				if buffer[position] != rune('T') {
					goto l378
				}
				position++
				if buffer[position] != rune('O') {
					goto l378
				}
				position++
				if buffer[position] != rune('C') {
					goto l378
				}
				position++
				if buffer[position] != rune('.') {
					goto l378
				}
				position++
				if buffer[position] != rune('-') {
					goto l378
				}
				position++
				{
					position380, tokenIndex380 := position, tokenIndex
					if buffer[position] != rune('0') {
						goto l381
					}
					position++
					if buffer[position] != rune('b') {
						goto l381
					}
					position++
					goto l380
				l381:
					position, tokenIndex = position380, tokenIndex380
					if buffer[position] != rune('.') {
						goto l378
					}
					position++
					if buffer[position] != rune('L') {
						goto l378
					}
					position++
					{
						position384, tokenIndex384 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l385
						}
						position++
						goto l384
					l385:
						position, tokenIndex = position384, tokenIndex384
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l386
						}
						position++
						goto l384
					l386:
						position, tokenIndex = position384, tokenIndex384
						if buffer[position] != rune('_') {
							goto l387
						}
						position++
						goto l384
					l387:
						position, tokenIndex = position384, tokenIndex384
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l378
						}
						position++
					}
				l384:
				l382:
					{
						position383, tokenIndex383 := position, tokenIndex
						{
							position388, tokenIndex388 := position, tokenIndex
							if c := buffer[position]; c < rune('a') || c > rune('z') {
								goto l389
							}
							position++
							goto l388
						l389:
							position, tokenIndex = position388, tokenIndex388
							if c := buffer[position]; c < rune('A') || c > rune('Z') {
								goto l390
							}
							position++
							goto l388
						l390:
							position, tokenIndex = position388, tokenIndex388
							if buffer[position] != rune('_') {
								goto l391
							}
							position++
							goto l388
						l391:
							position, tokenIndex = position388, tokenIndex388
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l383
							}
							position++
						}
					l388:
						goto l382
					l383:
						position, tokenIndex = position383, tokenIndex383
					}
				}
			l380:
				if buffer[position] != rune('@') {
					goto l378
				}
				position++
				{
					position392, tokenIndex392 := position, tokenIndex
					if buffer[position] != rune('h') {
						goto l393
					}
					position++
					goto l392
				l393:
					position, tokenIndex = position392, tokenIndex392
					if buffer[position] != rune('H') {
						goto l378
					}
					position++
				}
			l392:
				{
					position394, tokenIndex394 := position, tokenIndex
					if buffer[position] != rune('a') {
						goto l395
					}
					position++
					goto l394
				l395:
					position, tokenIndex = position394, tokenIndex394
					if buffer[position] != rune('A') {
						goto l378
					}
					position++
				}
			l394:
				add(ruleTOCRefHigh, position379)
			}
			return true
		l378:
			position, tokenIndex = position378, tokenIndex378
			return false
		},
		/* 30 TOCRefLow <- <('.' 'T' 'O' 'C' '.' '-' (('0' 'b') / ('.' 'L' ([a-z] / [A-Z] / '_' / [0-9])+)) ('@' ('l' / 'L')))> */
		func() bool {
			position396, tokenIndex396 := position, tokenIndex
			{
				position397 := position
				if buffer[position] != rune('.') {
					goto l396
				}
				position++
				if buffer[position] != rune('T') {
					goto l396
				}
				position++
				if buffer[position] != rune('O') {
					goto l396
				}
				position++
				if buffer[position] != rune('C') {
					goto l396
				}
				position++
				if buffer[position] != rune('.') {
					goto l396
				}
				position++
				if buffer[position] != rune('-') {
					goto l396
				}
				position++
				{
					position398, tokenIndex398 := position, tokenIndex
					if buffer[position] != rune('0') {
						goto l399
					}
					position++
					if buffer[position] != rune('b') {
						goto l399
					}
					position++
					goto l398
				l399:
					position, tokenIndex = position398, tokenIndex398
					if buffer[position] != rune('.') {
						goto l396
					}
					position++
					if buffer[position] != rune('L') {
						goto l396
					}
					position++
					{
						position402, tokenIndex402 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l403
						}
						position++
						goto l402
					l403:
						position, tokenIndex = position402, tokenIndex402
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l404
						}
						position++
						goto l402
					l404:
						position, tokenIndex = position402, tokenIndex402
						if buffer[position] != rune('_') {
							goto l405
						}
						position++
						goto l402
					l405:
						position, tokenIndex = position402, tokenIndex402
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l396
						}
						position++
					}
				l402:
				l400:
					{
						position401, tokenIndex401 := position, tokenIndex
						{
							position406, tokenIndex406 := position, tokenIndex
							if c := buffer[position]; c < rune('a') || c > rune('z') {
								goto l407
							}
							position++
							goto l406
						l407:
							position, tokenIndex = position406, tokenIndex406
							if c := buffer[position]; c < rune('A') || c > rune('Z') {
								goto l408
							}
							position++
							goto l406
						l408:
							position, tokenIndex = position406, tokenIndex406
							if buffer[position] != rune('_') {
								goto l409
							}
							position++
							goto l406
						l409:
							position, tokenIndex = position406, tokenIndex406
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l401
							}
							position++
						}
					l406:
						goto l400
					l401:
						position, tokenIndex = position401, tokenIndex401
					}
				}
			l398:
				if buffer[position] != rune('@') {
					goto l396
				}
				position++
				{
					position410, tokenIndex410 := position, tokenIndex
					if buffer[position] != rune('l') {
						goto l411
					}
					position++
					goto l410
				l411:
					position, tokenIndex = position410, tokenIndex410
					if buffer[position] != rune('L') {
						goto l396
					}
					position++
				}
			l410:
				add(ruleTOCRefLow, position397)
			}
			return true
		l396:
			position, tokenIndex = position396, tokenIndex396
			return false
		},
		/* 31 IndirectionIndicator <- <'*'> */
		func() bool {
			position412, tokenIndex412 := position, tokenIndex
			{
				position413 := position
				if buffer[position] != rune('*') {
					goto l412
				}
				position++
				add(ruleIndirectionIndicator, position413)
			}
			return true
		l412:
			position, tokenIndex = position412, tokenIndex412
			return false
		},
		/* 32 RegisterOrConstant <- <((('%' ([a-z] / [A-Z]) ([a-z] / [A-Z] / ([0-9] / [0-9]))*) / ('$'? ((Offset Offset) / Offset))) !('f' / 'b' / ':' / '(' / '+' / '-'))> */
		func() bool {
			position414, tokenIndex414 := position, tokenIndex
			{
				position415 := position
				{
					position416, tokenIndex416 := position, tokenIndex
					if buffer[position] != rune('%') {
						goto l417
					}
					position++
					{
						position418, tokenIndex418 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l419
						}
						position++
						goto l418
					l419:
						position, tokenIndex = position418, tokenIndex418
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l417
						}
						position++
					}
				l418:
				l420:
					{
						position421, tokenIndex421 := position, tokenIndex
						{
							position422, tokenIndex422 := position, tokenIndex
							if c := buffer[position]; c < rune('a') || c > rune('z') {
								goto l423
							}
							position++
							goto l422
						l423:
							position, tokenIndex = position422, tokenIndex422
							if c := buffer[position]; c < rune('A') || c > rune('Z') {
								goto l424
							}
							position++
							goto l422
						l424:
							position, tokenIndex = position422, tokenIndex422
							{
								position425, tokenIndex425 := position, tokenIndex
								if c := buffer[position]; c < rune('0') || c > rune('9') {
									goto l426
								}
								position++
								goto l425
							l426:
								position, tokenIndex = position425, tokenIndex425
								if c := buffer[position]; c < rune('0') || c > rune('9') {
									goto l421
								}
								position++
							}
						l425:
						}
					l422:
						goto l420
					l421:
						position, tokenIndex = position421, tokenIndex421
					}
					goto l416
				l417:
					position, tokenIndex = position416, tokenIndex416
					{
						position427, tokenIndex427 := position, tokenIndex
						if buffer[position] != rune('$') {
							goto l427
						}
						position++
						goto l428
					l427:
						position, tokenIndex = position427, tokenIndex427
					}
				l428:
					{
						position429, tokenIndex429 := position, tokenIndex
						if !_rules[ruleOffset]() {
							goto l430
						}
						if !_rules[ruleOffset]() {
							goto l430
						}
						goto l429
					l430:
						position, tokenIndex = position429, tokenIndex429
						if !_rules[ruleOffset]() {
							goto l414
						}
					}
				l429:
				}
			l416:
				{
					position431, tokenIndex431 := position, tokenIndex
					{
						position432, tokenIndex432 := position, tokenIndex
						if buffer[position] != rune('f') {
							goto l433
						}
						position++
						goto l432
					l433:
						position, tokenIndex = position432, tokenIndex432
						if buffer[position] != rune('b') {
							goto l434
						}
						position++
						goto l432
					l434:
						position, tokenIndex = position432, tokenIndex432
						if buffer[position] != rune(':') {
							goto l435
						}
						position++
						goto l432
					l435:
						position, tokenIndex = position432, tokenIndex432
						if buffer[position] != rune('(') {
							goto l436
						}
						position++
						goto l432
					l436:
						position, tokenIndex = position432, tokenIndex432
						if buffer[position] != rune('+') {
							goto l437
						}
						position++
						goto l432
					l437:
						position, tokenIndex = position432, tokenIndex432
						if buffer[position] != rune('-') {
							goto l431
						}
						position++
					}
				l432:
					goto l414
				l431:
					position, tokenIndex = position431, tokenIndex431
				}
				add(ruleRegisterOrConstant, position415)
			}
			return true
		l414:
			position, tokenIndex = position414, tokenIndex414
			return false
		},
		/* 33 MemoryRef <- <((SymbolRef BaseIndexScale) / SymbolRef / (Offset* BaseIndexScale) / (SegmentRegister Offset BaseIndexScale) / (SegmentRegister BaseIndexScale) / (SegmentRegister Offset) / BaseIndexScale)> */
		func() bool {
			position438, tokenIndex438 := position, tokenIndex
			{
				position439 := position
				{
					position440, tokenIndex440 := position, tokenIndex
					if !_rules[ruleSymbolRef]() {
						goto l441
					}
					if !_rules[ruleBaseIndexScale]() {
						goto l441
					}
					goto l440
				l441:
					position, tokenIndex = position440, tokenIndex440
					if !_rules[ruleSymbolRef]() {
						goto l442
					}
					goto l440
				l442:
					position, tokenIndex = position440, tokenIndex440
				l444:
					{
						position445, tokenIndex445 := position, tokenIndex
						if !_rules[ruleOffset]() {
							goto l445
						}
						goto l444
					l445:
						position, tokenIndex = position445, tokenIndex445
					}
					if !_rules[ruleBaseIndexScale]() {
						goto l443
					}
					goto l440
				l443:
					position, tokenIndex = position440, tokenIndex440
					if !_rules[ruleSegmentRegister]() {
						goto l446
					}
					if !_rules[ruleOffset]() {
						goto l446
					}
					if !_rules[ruleBaseIndexScale]() {
						goto l446
					}
					goto l440
				l446:
					position, tokenIndex = position440, tokenIndex440
					if !_rules[ruleSegmentRegister]() {
						goto l447
					}
					if !_rules[ruleBaseIndexScale]() {
						goto l447
					}
					goto l440
				l447:
					position, tokenIndex = position440, tokenIndex440
					if !_rules[ruleSegmentRegister]() {
						goto l448
					}
					if !_rules[ruleOffset]() {
						goto l448
					}
					goto l440
				l448:
					position, tokenIndex = position440, tokenIndex440
					if !_rules[ruleBaseIndexScale]() {
						goto l438
					}
				}
			l440:
				add(ruleMemoryRef, position439)
			}
			return true
		l438:
			position, tokenIndex = position438, tokenIndex438
			return false
		},
		/* 34 SymbolRef <- <((Offset* '+')? (LocalSymbol / SymbolName) Offset* ('@' Section Offset*)?)> */
		func() bool {
			position449, tokenIndex449 := position, tokenIndex
			{
				position450 := position
				{
					position451, tokenIndex451 := position, tokenIndex
				l453:
					{
						position454, tokenIndex454 := position, tokenIndex
						if !_rules[ruleOffset]() {
							goto l454
						}
						goto l453
					l454:
						position, tokenIndex = position454, tokenIndex454
					}
					if buffer[position] != rune('+') {
						goto l451
					}
					position++
					goto l452
				l451:
					position, tokenIndex = position451, tokenIndex451
				}
			l452:
				{
					position455, tokenIndex455 := position, tokenIndex
					if !_rules[ruleLocalSymbol]() {
						goto l456
					}
					goto l455
				l456:
					position, tokenIndex = position455, tokenIndex455
					if !_rules[ruleSymbolName]() {
						goto l449
					}
				}
			l455:
			l457:
				{
					position458, tokenIndex458 := position, tokenIndex
					if !_rules[ruleOffset]() {
						goto l458
					}
					goto l457
				l458:
					position, tokenIndex = position458, tokenIndex458
				}
				{
					position459, tokenIndex459 := position, tokenIndex
					if buffer[position] != rune('@') {
						goto l459
					}
					position++
					if !_rules[ruleSection]() {
						goto l459
					}
				l461:
					{
						position462, tokenIndex462 := position, tokenIndex
						if !_rules[ruleOffset]() {
							goto l462
						}
						goto l461
					l462:
						position, tokenIndex = position462, tokenIndex462
					}
					goto l460
				l459:
					position, tokenIndex = position459, tokenIndex459
				}
			l460:
				add(ruleSymbolRef, position450)
			}
			return true
		l449:
			position, tokenIndex = position449, tokenIndex449
			return false
		},
		/* 35 BaseIndexScale <- <('(' RegisterOrConstant? WS? (',' WS? RegisterOrConstant WS? (',' [0-9]+)?)? ')')> */
		func() bool {
			position463, tokenIndex463 := position, tokenIndex
			{
				position464 := position
				if buffer[position] != rune('(') {
					goto l463
				}
				position++
				{
					position465, tokenIndex465 := position, tokenIndex
					if !_rules[ruleRegisterOrConstant]() {
						goto l465
					}
					goto l466
				l465:
					position, tokenIndex = position465, tokenIndex465
				}
			l466:
				{
					position467, tokenIndex467 := position, tokenIndex
					if !_rules[ruleWS]() {
						goto l467
					}
					goto l468
				l467:
					position, tokenIndex = position467, tokenIndex467
				}
			l468:
				{
					position469, tokenIndex469 := position, tokenIndex
					if buffer[position] != rune(',') {
						goto l469
					}
					position++
					{
						position471, tokenIndex471 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l471
						}
						goto l472
					l471:
						position, tokenIndex = position471, tokenIndex471
					}
				l472:
					if !_rules[ruleRegisterOrConstant]() {
						goto l469
					}
					{
						position473, tokenIndex473 := position, tokenIndex
						if !_rules[ruleWS]() {
							goto l473
						}
						goto l474
					l473:
						position, tokenIndex = position473, tokenIndex473
					}
				l474:
					{
						position475, tokenIndex475 := position, tokenIndex
						if buffer[position] != rune(',') {
							goto l475
						}
						position++
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l475
						}
						position++
					l477:
						{
							position478, tokenIndex478 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l478
							}
							position++
							goto l477
						l478:
							position, tokenIndex = position478, tokenIndex478
						}
						goto l476
					l475:
						position, tokenIndex = position475, tokenIndex475
					}
				l476:
					goto l470
				l469:
					position, tokenIndex = position469, tokenIndex469
				}
			l470:
				if buffer[position] != rune(')') {
					goto l463
				}
				position++
				add(ruleBaseIndexScale, position464)
			}
			return true
		l463:
			position, tokenIndex = position463, tokenIndex463
			return false
		},
		/* 36 Operator <- <('+' / '-')> */
		func() bool {
			position479, tokenIndex479 := position, tokenIndex
			{
				position480 := position
				{
					position481, tokenIndex481 := position, tokenIndex
					if buffer[position] != rune('+') {
						goto l482
					}
					position++
					goto l481
				l482:
					position, tokenIndex = position481, tokenIndex481
					if buffer[position] != rune('-') {
						goto l479
					}
					position++
				}
			l481:
				add(ruleOperator, position480)
			}
			return true
		l479:
			position, tokenIndex = position479, tokenIndex479
			return false
		},
		/* 37 Offset <- <('+'? '-'? (('0' ('b' / 'B') ('0' / '1')+) / ('0' ('x' / 'X') ([0-9] / [0-9] / ([a-f] / [A-F]))+) / [0-9]+))> */
		func() bool {
			position483, tokenIndex483 := position, tokenIndex
			{
				position484 := position
				{
					position485, tokenIndex485 := position, tokenIndex
					if buffer[position] != rune('+') {
						goto l485
					}
					position++
					goto l486
				l485:
					position, tokenIndex = position485, tokenIndex485
				}
			l486:
				{
					position487, tokenIndex487 := position, tokenIndex
					if buffer[position] != rune('-') {
						goto l487
					}
					position++
					goto l488
				l487:
					position, tokenIndex = position487, tokenIndex487
				}
			l488:
				{
					position489, tokenIndex489 := position, tokenIndex
					if buffer[position] != rune('0') {
						goto l490
					}
					position++
					{
						position491, tokenIndex491 := position, tokenIndex
						if buffer[position] != rune('b') {
							goto l492
						}
						position++
						goto l491
					l492:
						position, tokenIndex = position491, tokenIndex491
						if buffer[position] != rune('B') {
							goto l490
						}
						position++
					}
				l491:
					{
						position495, tokenIndex495 := position, tokenIndex
						if buffer[position] != rune('0') {
							goto l496
						}
						position++
						goto l495
					l496:
						position, tokenIndex = position495, tokenIndex495
						if buffer[position] != rune('1') {
							goto l490
						}
						position++
					}
				l495:
				l493:
					{
						position494, tokenIndex494 := position, tokenIndex
						{
							position497, tokenIndex497 := position, tokenIndex
							if buffer[position] != rune('0') {
								goto l498
							}
							position++
							goto l497
						l498:
							position, tokenIndex = position497, tokenIndex497
							if buffer[position] != rune('1') {
								goto l494
							}
							position++
						}
					l497:
						goto l493
					l494:
						position, tokenIndex = position494, tokenIndex494
					}
					goto l489
				l490:
					position, tokenIndex = position489, tokenIndex489
					if buffer[position] != rune('0') {
						goto l499
					}
					position++
					{
						position500, tokenIndex500 := position, tokenIndex
						if buffer[position] != rune('x') {
							goto l501
						}
						position++
						goto l500
					l501:
						position, tokenIndex = position500, tokenIndex500
						if buffer[position] != rune('X') {
							goto l499
						}
						position++
					}
				l500:
					{
						position504, tokenIndex504 := position, tokenIndex
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l505
						}
						position++
						goto l504
					l505:
						position, tokenIndex = position504, tokenIndex504
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l506
						}
						position++
						goto l504
					l506:
						position, tokenIndex = position504, tokenIndex504
						{
							position507, tokenIndex507 := position, tokenIndex
							if c := buffer[position]; c < rune('a') || c > rune('f') {
								goto l508
							}
							position++
							goto l507
						l508:
							position, tokenIndex = position507, tokenIndex507
							if c := buffer[position]; c < rune('A') || c > rune('F') {
								goto l499
							}
							position++
						}
					l507:
					}
				l504:
				l502:
					{
						position503, tokenIndex503 := position, tokenIndex
						{
							position509, tokenIndex509 := position, tokenIndex
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l510
							}
							position++
							goto l509
						l510:
							position, tokenIndex = position509, tokenIndex509
							if c := buffer[position]; c < rune('0') || c > rune('9') {
								goto l511
							}
							position++
							goto l509
						l511:
							position, tokenIndex = position509, tokenIndex509
							{
								position512, tokenIndex512 := position, tokenIndex
								if c := buffer[position]; c < rune('a') || c > rune('f') {
									goto l513
								}
								position++
								goto l512
							l513:
								position, tokenIndex = position512, tokenIndex512
								if c := buffer[position]; c < rune('A') || c > rune('F') {
									goto l503
								}
								position++
							}
						l512:
						}
					l509:
						goto l502
					l503:
						position, tokenIndex = position503, tokenIndex503
					}
					goto l489
				l499:
					position, tokenIndex = position489, tokenIndex489
					if c := buffer[position]; c < rune('0') || c > rune('9') {
						goto l483
					}
					position++
				l514:
					{
						position515, tokenIndex515 := position, tokenIndex
						if c := buffer[position]; c < rune('0') || c > rune('9') {
							goto l515
						}
						position++
						goto l514
					l515:
						position, tokenIndex = position515, tokenIndex515
					}
				}
			l489:
				add(ruleOffset, position484)
			}
			return true
		l483:
			position, tokenIndex = position483, tokenIndex483
			return false
		},
		/* 38 Section <- <([a-z] / [A-Z] / '@')+> */
		func() bool {
			position516, tokenIndex516 := position, tokenIndex
			{
				position517 := position
				{
					position520, tokenIndex520 := position, tokenIndex
					if c := buffer[position]; c < rune('a') || c > rune('z') {
						goto l521
					}
					position++
					goto l520
				l521:
					position, tokenIndex = position520, tokenIndex520
					if c := buffer[position]; c < rune('A') || c > rune('Z') {
						goto l522
					}
					position++
					goto l520
				l522:
					position, tokenIndex = position520, tokenIndex520
					if buffer[position] != rune('@') {
						goto l516
					}
					position++
				}
			l520:
			l518:
				{
					position519, tokenIndex519 := position, tokenIndex
					{
						position523, tokenIndex523 := position, tokenIndex
						if c := buffer[position]; c < rune('a') || c > rune('z') {
							goto l524
						}
						position++
						goto l523
					l524:
						position, tokenIndex = position523, tokenIndex523
						if c := buffer[position]; c < rune('A') || c > rune('Z') {
							goto l525
						}
						position++
						goto l523
					l525:
						position, tokenIndex = position523, tokenIndex523
						if buffer[position] != rune('@') {
							goto l519
						}
						position++
					}
				l523:
					goto l518
				l519:
					position, tokenIndex = position519, tokenIndex519
				}
				add(ruleSection, position517)
			}
			return true
		l516:
			position, tokenIndex = position516, tokenIndex516
			return false
		},
		/* 39 SegmentRegister <- <('%' ([c-g] / 's') ('s' ':'))> */
		func() bool {
			position526, tokenIndex526 := position, tokenIndex
			{
				position527 := position
				if buffer[position] != rune('%') {
					goto l526
				}
				position++
				{
					position528, tokenIndex528 := position, tokenIndex
					if c := buffer[position]; c < rune('c') || c > rune('g') {
						goto l529
					}
					position++
					goto l528
				l529:
					position, tokenIndex = position528, tokenIndex528
					if buffer[position] != rune('s') {
						goto l526
					}
					position++
				}
			l528:
				if buffer[position] != rune('s') {
					goto l526
				}
				position++
				if buffer[position] != rune(':') {
					goto l526
				}
				position++
				add(ruleSegmentRegister, position527)
			}
			return true
		l526:
			position, tokenIndex = position526, tokenIndex526
			return false
		},
	}
	p.rules = _rules
}
