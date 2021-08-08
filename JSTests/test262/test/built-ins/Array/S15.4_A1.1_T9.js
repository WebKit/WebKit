// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    A property name P (in the form of a string value) is an array index
    if and only if ToString(ToUint32(P)) is equal to P and ToUint32(P) is not equal to 2^32 - 1
es5id: 15.4_A1.1_T9
description: If Type(value) is Object, evaluate ToPrimitive(value, String)
---*/

//CHECK#1
var x = [];
var object = {
  valueOf: function() {
    return 1
  }
};
x[object] = 0;
if (x["[object Object]"] !== 0) {
  throw new Test262Error('#1: x = []; var object = {valueOf: function() {return 1}}; x[object] = 0; x["[object Object]"] === 0. Actual: ' + (x["[object Object]"]));
}

//CHECK#2
x = [];
var object = {
  valueOf: function() {
    return 1
  },
  toString: function() {
    return 0
  }
};
x[object] = 0;
if (x[0] !== 0) {
  throw new Test262Error('#2: x = []; var object = {valueOf: function() {return 1}, toString: function() {return 0}}; x[object] = 0; x[0] === 0. Actual: ' + (x[0]));
}

//CHECK#3
x = [];
var object = {
  valueOf: function() {
    return 1
  },
  toString: function() {
    return {}
  }
};
x[object] = 0;
if (x[1] !== 0) {
  throw new Test262Error('#3: x = []; var object = {valueOf: function() {return 1}, toString: function() {return {}}}; x[object] = 0; x[1] === 0. Actual: ' + (x[1]));
}

//CHECK#4
try {
  x = [];
  var object = {
    valueOf: function() {
      throw "error"
    },
    toString: function() {
      return 1
    }
  };
  x[object] = 0;
  if (x[1] !== 0) {
    throw new Test262Error('#4.1: x = []; var object = {valueOf: function() {throw "error"}, toString: function() {return 1}}; x[object] = 0; x[1] === 1. Actual: ' + (x[1]));
  }
}
catch (e) {
  if (e === "error") {
    throw new Test262Error('#4.2: x = []; var object = {valueOf: function() {throw "error"}, toString: function() {return 1}}; x[object] = 0; x[1] === 1. Actual: ' + ("error"));
  } else {
    throw new Test262Error('#4.3: x = []; var object = {valueOf: function() {throw "error"}, toString: function() {return 1}}; x[object] = 0; x[1] === 1. Actual: ' + (e));
  }
}

//CHECK#5
x = [];
var object = {
  toString: function() {
    return 1
  }
};
x[object] = 0;
if (x[1] !== 0) {
  throw new Test262Error('#5: x = []; var object = {toString: function() {return 1}}; x[object] = 0; x[1] === 0. Actual: ' + (x[1]));
}

//CHECK#6
x = [];
var object = {
  valueOf: function() {
    return {}
  },
  toString: function() {
    return 1
  }
}
x[object] = 0;
if (x[1] !== 0) {
  throw new Test262Error('#6: x = []; var object = {valueOf: function() {return {}}, toString: function() {return 1}}; x[object] = 0; x[1] === 0. Actual: ' + (x[1]));
}

//CHECK#7
try {
  x = [];
  var object = {
    valueOf: function() {
      return 1
    },
    toString: function() {
      throw "error"
    }
  };
  x[object];
  throw new Test262Error('#7.1: x = []; var object = {valueOf: function() {return 1}, toString: function() {throw "error"}}; x[object] throw "error". Actual: ' + (x[object]));
}
catch (e) {
  if (e !== "error") {
    throw new Test262Error('#7.2: x = []; var object = {valueOf: function() {return 1}, toString: function() {throw "error"}}; x[object] throw "error". Actual: ' + (e));
  }
}

//CHECK#8
try {
  x = [];
  var object = {
    valueOf: function() {
      return {}
    },
    toString: function() {
      return {}
    }
  };
  x[object];
  throw new Test262Error('#8.1: x = []; var object = {valueOf: function() {return {}}, toString: function() {return {}}}; x[object] throw TypeError. Actual: ' + (x[object]));
}
catch (e) {
  if ((e instanceof TypeError) !== true) {
    throw new Test262Error('#8.2: x = []; var object = {valueOf: function() {return {}}, toString: function() {return {}}}; x[object] throw TypeError. Actual: ' + (e));
  }
}
