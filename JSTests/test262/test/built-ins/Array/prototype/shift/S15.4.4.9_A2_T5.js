// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The shift function is intentionally generic.
    It does not require that its this value be an Array object
esid: sec-array.prototype.shift
description: >
    Operator use ToNumber from length.  If Type(value) is Object,
    evaluate ToPrimitive(value, Number)
---*/

var obj = {};
obj.shift = Array.prototype.shift;

//CHECK#1
obj[0] = -1;
obj.length = {
  valueOf: function() {
    return 1
  }
};
var shift = obj.shift();
if (shift !== -1) {
  throw new Test262Error('#1: obj[0] = -1; obj.length = {valueOf: function() {return 1}}  obj.shift() === -1. Actual: ' + (shift));
}

//CHECK#2
obj[0] = -1;
obj.length = {
  valueOf: function() {
    return 1
  },
  toString: function() {
    return 0
  }
};
var shift = obj.shift();
if (shift !== -1) {
  throw new Test262Error('#0: obj[0] = -1; obj.length = {valueOf: function() {return 1}, toString: function() {return 0}}  obj.shift() === -1. Actual: ' + (shift));
}

//CHECK#3
obj[0] = -1;
obj.length = {
  valueOf: function() {
    return 1
  },
  toString: function() {
    return {}
  }
};
var shift = obj.shift();
if (shift !== -1) {
  throw new Test262Error('#3: obj[0] = -1; obj.length = {valueOf: function() {return 1}, toString: function() {return {}}}  obj.shift() === -1. Actual: ' + (shift));
}

//CHECK#4
try {
  obj[0] = -1;
  obj.length = {
    valueOf: function() {
      return 1
    },
    toString: function() {
      throw "error"
    }
  };
  var shift = obj.shift();
  if (shift !== -1) {
    throw new Test262Error('#4.1: obj[0] = -1; obj.length = {valueOf: function() {return 1}, toString: function() {throw "error"}}; obj.shift() === ",". Actual: ' + (shift));
  }
}
catch (e) {
  if (e === "error") {
    throw new Test262Error('#4.2: obj[0] = -1; obj.length = {valueOf: function() {return 1}, toString: function() {throw "error"}}; obj.shift() not throw "error"');
  } else {
    throw new Test262Error('#4.3: obj[0] = -1; obj.length = {valueOf: function() {return 1}, toString: function() {throw "error"}}; obj.shift() not throw Error. Actual: ' + (e));
  }
}

//CHECK#5
obj[0] = -1;
obj.length = {
  toString: function() {
    return 0
  }
};
var shift = obj.shift();
if (shift !== undefined) {
  throw new Test262Error('#5: obj[0] = -1; obj.length = {toString: function() {return 0}}  obj.shift() === undefined. Actual: ' + (shift));
}

//CHECK#6
obj[0] = -1;
obj.length = {
  valueOf: function() {
    return {}
  },
  toString: function() {
    return 0
  }
}
var shift = obj.shift();
if (shift !== undefined) {
  throw new Test262Error('#6: obj[0] = -1; obj.length = {valueOf: function() {return {}}, toString: function() {return 0}}  obj.shift() === undefined. Actual: ' + (shift));
}

//CHECK#7
try {
  obj[0] = -1;
  obj.length = {
    valueOf: function() {
      throw "error"
    },
    toString: function() {
      return 0
    }
  };
  var shift = obj.shift();
  throw new Test262Error('#7.1: obj[0] = -1; obj.length = {valueOf: function() {throw "error"}, toString: function() {return 0}}; obj.shift() throw "error". Actual: ' + (shift));
}
catch (e) {
  if (e !== "error") {
    throw new Test262Error('#7.2: obj[0] = -1; obj.length = {valueOf: function() {throw "error"}, toString: function() {return 0}}; obj.shift() throw "error". Actual: ' + (e));
  }
}

//CHECK#8
try {
  obj[0] = -1;
  obj.length = {
    valueOf: function() {
      return {}
    },
    toString: function() {
      return {}
    }
  };
  var shift = obj.shift();
  throw new Test262Error('#8.1: obj[0] = -1; obj.length = {valueOf: function() {return {}}, toString: function() {return {}}}  obj.shift() throw TypeError. Actual: ' + (shift));
}
catch (e) {
  if ((e instanceof TypeError) !== true) {
    throw new Test262Error('#8.2: obj[0] = -1; obj.length = {valueOf: function() {return {}}, toString: function() {return {}}}  obj.shift() throw TypeError. Actual: ' + (e));
  }
}
