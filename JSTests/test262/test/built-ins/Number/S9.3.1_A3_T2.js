// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The MV of StringNumericLiteral ::: StrWhiteSpaceopt StrNumericLiteral
    StrWhiteSpaceopt is the MV of StrNumericLiteral, no matter whether white
    space is present or not
es5id: 9.3.1_A3_T2
description: dynamic string
---*/

function dynaString(s1, s2) {
  return String(s1) + String(s2);
}

// CHECK#1
if (Number(dynaString("\u0009\u000C\u0020\u00A0\u000B", "\u000A\u000D\u2028\u2029\u1680\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u202F\u205F\u3000")) !== Number("")) {
  throw new Test262Error('#1: Number("\\u0009\\u000C\\u0020\\u00A0\\u000B"+"\\u000A\\u000D\\u2028\\u2029\\u1680\\u2000\\u2001\\u2002\\u2003\\u2004\\u2005\\u2006\\u2007\\u2008\\u2009\\u200A\\u202F\\u205F\\u3000") === Number("")');
}

// CHECK#2
if (+(dynaString("\u0009\u000C\u0020\u00A0\u000A\u000D\u2028\u2029\u000B12345", "67890\u0009\u000C\u0020\u00A0\u000B\u000A\u000D\u2028\u2029\u1680\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u202F\u205F\u3000")) !== Number("1234567890")) {
  throw new Test262Error('#2: +("\\u0009\\u000C\\u0020\\u00A0\\u000A\\u000D\\u2028\\u2029\\u000B12345"+"67890\\u0009\\u000C\\u0020\\u00A0\\u000B\\u000A\\u000D\\u2028\\u2029\\u1680\\u2000\\u2001\\u2002\\u2003\\u2004\\u2005\\u2006\\u2007\\u2008\\u2009\\u200A\\u202F\\u205F\\u3000") === Number("1234567890")');
}

// CHECK#3
if (!(Number(dynaString("\u0009\u000C\u0020\u00A0\u000B\u000A\u000D\u2028\u2029Infi", "nity\u0009\u000C\u0020\u00A0\u000B\u000A\u000D\u2028\u2029\u1680\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u202F\u205F\u3000")) == Number("Infinity"))) {
  throw new Test262Error('#3: Number("\\u0009\\u000C\\u0020\\u00A0\\u000B\\u000A\\u000D\\u2028\\u2029Infi"+"nity\\u0009\\u000C\\u0020\\u00A0\\u000B\\u000A\\u000D\\u2028\\u2029\\u1680\\u2000\\u2001\\u2002\\u2003\\u2004\\u2005\\u2006\\u2007\\u2008\\u2009\\u200A\\u202F\\u205F\\u3000") == Number("Infinity")');
}

// CHECK#4
if (!(Number(dynaString("\u0009\u000C\u0020\u00A0\u000B\u000A\u000D\u2028\u2029-Infi", "nity\u0009\u000C\u0020\u00A0\u000B\u000A\u000D\u2028\u2029\u1680\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u202F\u205F\u3000")) == Number(-"Infinity"))) {
  throw new Test262Error('#4: Number("\\u0009\\u000C\\u0020\\u00A0\\u000B\\u000A\\u000D\\u2028\\u2029-Infi"+"nity\\u0009\\u000C\\u0020\\u00A0\\u000B\\u000A\\u000D\\u2028\\u2029\\u1680\\u2000\\u2001\\u2002\\u2003\\u2004\\u2005\\u2006\\u2007\\u2008\\u2009\\u200A\\u202F\\u205F\\u3000") == Number("-Infinity")');
}
