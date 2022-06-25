// Copyright (C) 2020 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: prod-StringLiteral
description: >
  \u is at the end of string, Hex4Digits is required.
info: |
  StringLiteral ::
    " DoubleStringCharacters_opt "
    ' SingleStringCharacters_opt '

  DoubleStringCharacters ::
    DoubleStringCharacter DoubleStringCharacters_opt

  DoubleStringCharacter ::
    SourceCharacter but not one of " or \ or LineTerminator
    <LS>
    <PS>
    \ EscapeSequence
    LineContinuation

  EscapeSequence ::
    CharacterEscapeSequence
    0 [lookahead ∉ DecimalDigit]
    HexEscapeSequence
    UnicodeEscapeSequence

  UnicodeEscapeSequence ::
    u Hex4Digits
    u{ CodePoint }

  Hex4Digits ::
    HexDigit HexDigit HexDigit HexDigit

  HexDigit :: one of
    0 1 2 3 4 5 6 7 8 9 a b c d e f A B C D E F

negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

"\u"
