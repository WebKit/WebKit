// Copyright (C) 2017 Valerie Young. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: prod-Pattern
description: NumericLiteralSeperator disallowed in unicode CodePoint sequence
info: |
 Pattern[U, N]::
   Disjunction[?U, ?N]

 Disjunction[U, N]::
   Alternative[?U, ?N]
   Alternative[?U, ?N]|Disjunction[?U, ?N]

 Alternative[U, N]::
   [empty]
   Alternative[?U, ?N]Term[?U, ?N]

 Term[U, N]::
   Assertion[?U, ?N]
   Atom[?U, ?N]
   Atom[?U, ?N]Quantifier

 Atom[U, N]::
   PatternCharacter
   .
   \AtomEscape[?U, ?N]
   CharacterClass[?U]
   (GroupSpecifier[?U]Disjunction[?U, ?N])
   (?:Disjunction[?U, ?N])

 AtomEscape[U, N]::
   DecimalEscape
   CharacterClassEscape[?U]
   CharacterEscape[?U]
   [+N]kGroupName[?U]

 CharacterEscape[U]::
   ControlEscape
   cControlLetter
   0[lookahead ∉ DecimalDigit]
   HexEscapeSequence
   RegExpUnicodeEscapeSequence[?U]
   IdentityEscape[?U]

 RegExpUnicodeEscapeSequence[U]::
   [+U]uLeadSurrogate\uTrailSurrogate
   [+U]uLeadSurrogate
   [+U]uTrailSurrogate
   [+U]uNonSurrogate
   [~U]uHex4Digits
   [+U]u{CodePoint}

 CodePoint ::
   HexDigit but only if MV of HexDigits ≤ 0x10FFFF
   CodePointDigits but only if MV of HexDigits ≤ 0x10FFFF

 CodePointDigits ::
   HexDigit
   CodePointDigitsHexDigit

  HexDigit :: one of
    0 1 2 3 4 5 6 7 8 9 a b c d e f A B C D E F

features: [numeric-separator-literal]
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

/\u{1F_639}/u;
