// Copyright (C) 2017 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-additional-syntax-string-literals
es6id: B.1.2
description: >
    LegacyOctalEscapeSequence is not enabled in strict mode code (regardless of
    the presence of Annex B)
info: |
    EscapeSequence ::
      CharacterEscapeSequence
      LegacyOctalEscapeSequence
      HexEscapeSequence
      UnicodeEscapeSequence

    LegacyOctalEscapeSequence ::
      OctalDigit [lookahead ∉ OctalDigit]
      ZeroToThree OctalDigit [lookahead ∉ OctalDigit]
      FourToSeven OctalDigit
      ZeroToThree OctalDigit OctalDigit

    ZeroToThree :: one of
      0 1 2 3

    FourToSeven :: one of
      4 5 6 7

    This definition of EscapeSequence is not used in strict mode or when
    parsing TemplateCharacter.
flags: [onlyStrict]
negative:
  phase: parse
  type: SyntaxError
---*/

throw "Test262: This statement should not be evaluated.";

'\08';
