/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-JSON-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
testJSON('[', true);
testJSON('[1', true);
testJSON('[1,]', true);
testJSON('[1,{', true);
testJSON('[1,}', true);
testJSON('[1,{]', true);
testJSON('[1,}]', true);
testJSON('[1,{"', true);
testJSON('[1,}"', true);
testJSON('[1,{"\\', true);
testJSON('[1,}"\\', true);
testJSON('[1,"', true);
testJSON('[1,"\\', true);

testJSON('{', true);
testJSON('{1', true);
testJSON('{,', true);
testJSON('{"', true);
testJSON('{"\\', true);
testJSON('{"\\u', true);
testJSON('{"\\uG', true);
testJSON('{"\\u0', true);
testJSON('{"\\u01', true);
testJSON('{"\\u012', true);
testJSON('{"\\u0123', true);
testJSON('{"\\u0123"', true);
testJSON('{"a"', true);
testJSON('{"a"}', true);
testJSON('{"a":', true);
testJSON('{"a",}', true);
testJSON('{"a":}', true);
testJSON('{"a":,}', true);
testJSON('{"a":5,}', true);
testJSON('{"a":5,[', true);
testJSON('{"a":5,"', true);
testJSON('{"a":5,"', true);
testJSON('{"a":5,"\\', true);
testJSON("a[false ]".substring(1, 7) /* "[false" */, true);

testJSON('this', true);

testJSON('[1,{}]', false);
testJSON('{}', false);
testJSON('{"a":5}', false);
testJSON('{"\\u0123":5}', false);

/******************************************************************************/

print("Tests complete");
