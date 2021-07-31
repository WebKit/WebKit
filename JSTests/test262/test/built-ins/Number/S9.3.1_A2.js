// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "The MV of StringNumericLiteral ::: StrWhiteSpace is 0"
es5id: 9.3.1_A2
description: >
    Strings with various WhiteSpaces convert to Number by explicit
    transformation
---*/

// CHECK#1
if (Number("\u0009\u000C\u0020\u00A0\u000B\u000A\u000D\u2028\u2029\u1680\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u202F\u205F\u3000") !== 0) {
  throw new Test262Error('#1.1: Number("\\u0009\\u000C\\u0020\\u00A0\\u000B\\u000A\\u000D\\u2028\\u2029\\u1680\\u2000\\u2001\\u2002\\u2003\\u2004\\u2005\\u2006\\u2007\\u2008\\u2009\\u200A\\u202F\\u205F\\u3000") === 0. Actual: ' + (Number("\u0009\u000C\u0020\u00A0\u000B\u000A\u000D\u2028\u2029\u1680\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u202F\u205F\u3000")));
} else {
  if (1 / Number("\u0009\u000C\u0020\u00A0\u000B\u000A\u000D\u2028\u2029\u1680\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u202F\u205F\u3000") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#1.2: Number("\\u0009\\u000C\\u0020\\u00A0\\u000B\\u000A\\u000D\\u2028\\u2029\\u1680\\u2000\\u2001\\u2002\\u2003\\u2004\\u2005\\u2006\\u2007\\u2008\\u2009\\u200A\\u202F\\u205F\\u3000") === +0. Actual: -0');
  }
}

// CHECK#2
if (Number(" ") !== 0) {
  throw new Test262Error('#2.1: Number(" ") === 0. Actual: ' + (Number(" ")));
} else {
  if (1 / Number(" ") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#2.2: Number(" ") === +0. Actual: -0');
  }
}

// CHECK#3
if (Number("\t") !== 0) {
  throw new Test262Error('#3.1: Number("\\t") === 0. Actual: ' + (Number("\t")));
} else {
  if (1 / Number("\t") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#3.2: Number("\\t") === +0. Actual: -0');
  }
}

// CHECK#4
if (Number("\r") !== 0) {
  throw new Test262Error('#4.1: Number("\\r") === 0. Actual: ' + (Number("\r")));
} else {
  if (1 / Number("\r") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#4.2: Number("\\r") === +0. Actual: -0');
  }
}

// CHECK#5
if (Number("\n") !== 0) {
  throw new Test262Error('#5.1: Number("\\n") === 0. Actual: ' + (Number("\n")));
} else {
  if (1 / Number("\n") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#5.2: Number("\\n") === +0. Actual: -0');
  }
}

// CHECK#6
if (Number("\f") !== 0) {
  throw new Test262Error('#6.1: Number("\\f") === 0. Actual: ' + (Number("\f")));
} else {
  if (1 / Number("\f") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#6.2: Number("\\f") === +0. Actual: -0');
  }
}

// CHECK#7
if (Number("\u0009") !== 0) {
  throw new Test262Error('#7.1: Number("\\u0009") === 0. Actual: ' + (Number("\u0009")));
} else {
  if (1 / Number("\u0009") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#7.2: Number("\\u0009") === +0. Actual: -0');
  }
}

// CHECK#8
if (Number("\u000A") !== 0) {
  throw new Test262Error('#8.1: Number("\\u000A") === 0. Actual: ' + (Number("\u000A")));
} else {
  if (1 / Number("\u000A") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#8.2: Number("\\u000A") === +0. Actual: -0');
  }
}

// CHECK#9
if (Number("\u000B") !== 0) {
  throw new Test262Error('#9.1: Number("\\u000B") === 0. Actual: ' + (Number("\u000B")));
} else {
  if (1 / Number("\u000B") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#9.1.2: Number("\\u000B") === +0. Actual: -0');
  }
}

// CHECK#10
if (Number("\u000C") !== 0) {
  throw new Test262Error('#10.1: Number("\\u000C") === 0. Actual: ' + (Number("\u000C")));
} else {
  if (1 / Number("\u000C") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#10.2: Number("\\u000C") === +0. Actual: -0');
  }
}

// CHECK#11
if (Number("\u000D") !== 0) {
  throw new Test262Error('#11.1: Number("\\u000D") === 0. Actual: ' + (Number("\u000D")));
} else {
  if (1 / Number("\u000D") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#11.2: Number("\\u000D") === +0. Actual: -0');
  }
}

// CHECK#12
if (Number("\u00A0") !== 0) {
  throw new Test262Error('#12.1: Number("\\u00A0") === 0. Actual: ' + (Number("\u00A0")));
} else {
  if (1 / Number("\u00A0") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#12.2: Number("\\u00A0") === +0. Actual: -0');
  }
}

// CHECK#13
if (Number("\u0020") !== 0) {
  throw new Test262Error('#13.1: Number("\\u0020") === 0. Actual: ' + (Number("\u0020")));
} else {
  if (1 / Number("\u0020") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#13.2: Number("\\u0020") === +0. Actual: -0');
  }
}

// CHECK#14
if (Number("\u2028") !== 0) {
  throw new Test262Error('#14.1: Number("\\u2028") === 0. Actual: ' + (Number("\u2028")));
} else {
  if (1 / Number("\u2028") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#14.2: Number("\\u2028") === +0. Actual: -0');
  }
}

// CHECK#15
if (Number("\u2029") !== 0) {
  throw new Test262Error('#15.1: Number("\\u2029") === 0. Actual: ' + (Number("\u2029")));
} else {
  if (1 / Number("\u2029") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#15.2: Number("\\u2029") === +0. Actual: -0');
  }
}

// CHECK#16
if (Number("\u1680") !== 0) {
  throw new Test262Error('#16.1: Number("\\u1680") === 0. Actual: ' + (Number("\u1680")));
} else {
  if (1 / Number("\u1680") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#16.2: Number("\\u1680") === +0. Actual: -0');
  }
}

// CHECK#17
if (Number("\u2000") !== 0) {
  throw new Test262Error('#17.1: Number("\\u2000") === 0. Actual: ' + (Number("\u2000")));
} else {
  if (1 / Number("\u2000") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#17.2: Number("\\u2000") === +0. Actual: -0');
  }
}

// CHECK#18
if (Number("\u2001") !== 0) {
  throw new Test262Error('#18.1: Number("\\u2001") === 0. Actual: ' + (Number("\u2001")));
} else {
  if (1 / Number("\u2001") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#18.2: Number("\\u2001") === +0. Actual: -0');
  }
}

// CHECK#19
if (Number("\u2002") !== 0) {
  throw new Test262Error('#19.1: Number("\\u2002") === 0. Actual: ' + (Number("\u2002")));
} else {
  if (1 / Number("\u2002") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#19.2: Number("\\u2002") === +0. Actual: -0');
  }
}

// CHECK#20
if (Number("\u2003") !== 0) {
  throw new Test262Error('#20.1: Number("\\u2003") === 0. Actual: ' + (Number("\u2003")));
} else {
  if (1 / Number("\u2003") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#20.2: Number("\\u2003") === +0. Actual: -0');
  }
}

// CHECK#21
if (Number("\u2004") !== 0) {
  throw new Test262Error('#21.1: Number("\\u2004") === 0. Actual: ' + (Number("\u2004")));
} else {
  if (1 / Number("\u2004") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#21.2: Number("\\u2004") === +0. Actual: -0');
  }
}

// CHECK#22
if (Number("\u2005") !== 0) {
  throw new Test262Error('#22.1: Number("\\u2005") === 0. Actual: ' + (Number("\u2005")));
} else {
  if (1 / Number("\u2005") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#22.2: Number("\\u2005") === +0. Actual: -0');
  }
}

// CHECK#23
if (Number("\u2006") !== 0) {
  throw new Test262Error('#23.1: Number("\\u2006") === 0. Actual: ' + (Number("\u2006")));
} else {
  if (1 / Number("\u2006") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#23.2: Number("\\u2006") === +0. Actual: -0');
  }
}

// CHECK#24
if (Number("\u2007") !== 0) {
  throw new Test262Error('#24.1: Number("\\u2007") === 0. Actual: ' + (Number("\u2007")));
} else {
  if (1 / Number("\u2007") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#24.2: Number("\\u2007") === +0. Actual: -0');
  }
}

// CHECK#25
if (Number("\u2008") !== 0) {
  throw new Test262Error('#25.1: Number("\\u2008") === 0. Actual: ' + (Number("\u2008")));
} else {
  if (1 / Number("\u2008") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#25.2: Number("\\u2008") === +0. Actual: -0');
  }
}

// CHECK#26
if (Number("\u2009") !== 0) {
  throw new Test262Error('#26.1: Number("\\u2009") === 0. Actual: ' + (Number("\u2009")));
} else {
  if (1 / Number("\u2009") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#26.2: Number("\\u2009") === +0. Actual: -0');
  }
}

// CHECK#27
if (Number("\u200A") !== 0) {
  throw new Test262Error('#27.1: Number("\\u200A") === 0. Actual: ' + (Number("\u200A")));
} else {
  if (1 / Number("\u200A") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#27.2: Number("\\u200A") === +0. Actual: -0');
  }
}

// CHECK#28
if (Number("\u202F") !== 0) {
  throw new Test262Error('#28.1: Number("\\u202F") === 0. Actual: ' + (Number("\u202F")));
} else {
  if (1 / Number("\u202F") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#28.2: Number("\\u202F") === +0. Actual: -0');
  }
}

// CHECK#29
if (Number("\u205F") !== 0) {
  throw new Test262Error('#29.1: Number("\\u205F") === 0. Actual: ' + (Number("\u205F")));
} else {
  if (1 / Number("\u205F") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#29.2: Number("\\u205F") === +0. Actual: -0');
  }
}

// CHECK#30
if (Number("\u3000") !== 0) {
  throw new Test262Error('#30.1: Number("\\u3000") === 0. Actual: ' + (Number("\u3000")));
} else {
  if (1 / Number("\u3000") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#30.2: Number("\\u3000") === +0. Actual: -0');
  }
}
