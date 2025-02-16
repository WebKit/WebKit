// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-JSON-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
testJSON('"Unterminated string literal', true);
testJSON('["Unclosed array"', true);
testJSON('{unquoted_key: "keys must be quoted"}', true);
testJSON('["extra comma",]', true);
testJSON('["double extra comma",,]', true);
testJSON('[   , "<-- missing value"]', true);
testJSON('["Comma after the close"],', true);
testJSON('["Extra close"]]', true);
testJSON('{"Extra comma": true,}', true);
testJSON('{"Extra value after close": true} "misplaced quoted value"', true);
testJSON('{"Illegal expression": 1 + 2}', true);
testJSON('{"Illegal invocation": alert()}', true);
testJSON('{"Numbers cannot be hex": 0x14}', true);
testJSON('["Illegal backslash escape: \\x15"]', true);
testJSON('[\\naked]', true);
testJSON('["Illegal backslash escape: \\017"]', true);
testJSON('{"Missing colon" null}', true);
testJSON('{"Double colon":: null}', true);
testJSON('{"Comma instead of colon", null}', true);
testJSON('["Colon instead of comma": false]', true);
testJSON('["Bad value", truth]', true);
testJSON("['single quote']", true);
testJSON('["	tab	character	in	string	"]', true);
testJSON('["tab\\   character\\   in\\  string\\  "]', true);
testJSON('["line\rbreak"]', true);
testJSON('["line\nbreak"]', true);
testJSON('["line\r\nbreak"]', true);
testJSON('["line\\\rbreak"]', true);
testJSON('["line\\\nbreak"]', true);
testJSON('["line\\\r\nbreak"]', true);
testJSON('[0e]', true);
testJSON('[0e+]', true);
testJSON('[0e+-1]', true);
testJSON('{"Comma instead of closing brace": true,', true);
testJSON('["mismatch"}', true);
testJSON('0{', true);

/******************************************************************************/

print("Tests complete");
