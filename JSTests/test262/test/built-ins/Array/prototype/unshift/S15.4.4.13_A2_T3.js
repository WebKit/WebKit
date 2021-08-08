// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The unshift function is intentionally generic.
    It does not require that its this value be an Array object
esid: sec-array.prototype.unshift
description: >
    Operator use ToNumber from length.  If Type(value) is Object,
    evaluate ToPrimitive(value, Number)
---*/

var obj = {};
obj.unshift = Array.prototype.unshift;

//CHECK#1
obj.length = {
  valueOf: function() {
    return 3
  }
};
var unshift = obj.unshift();
if (unshift !== 3) {
  throw new Test262Error('#1:  obj.length = {valueOf: function() {return 3}}  obj.unshift() === 3. Actual: ' + (unshift));
}

//CHECK#2
obj.length = {
  valueOf: function() {
    return 3
  },
  toString: function() {
    return 1
  }
};
var unshift = obj.unshift();
if (unshift !== 3) {
  throw new Test262Error('#0:  obj.length = {valueOf: function() {return 3}, toString: function() {return 1}}  obj.unshift() === 3. Actual: ' + (unshift));
}

//CHECK#3
obj.length = {
  valueOf: function() {
    return 3
  },
  toString: function() {
    return {}
  }
};
var unshift = obj.unshift();
if (unshift !== 3) {
  throw new Test262Error('#1:  obj.length = {valueOf: function() {return 3}, toString: function() {return {}}}  obj.unshift() === 3. Actual: ' + (unshift));
}

//CHECK#4
try {
  obj.length = {
    valueOf: function() {
      return 3
    },
    toString: function() {
      throw "error"
    }
  };
  var unshift = obj.unshift();
  if (unshift !== 3) {
    throw new Test262Error('#4.1:  obj.length = {valueOf: function() {return 3}, toString: function() {throw "error"}}; obj.unshift() === ",". Actual: ' + (unshift));
  }
}
catch (e) {
  if (e === "error") {
    throw new Test262Error('#4.2:  obj.length = {valueOf: function() {return 3}, toString: function() {throw "error"}}; obj.unshift() not throw "error"');
  } else {
    throw new Test262Error('#4.3:  obj.length = {valueOf: function() {return 3}, toString: function() {throw "error"}}; obj.unshift() not throw Error. Actual: ' + (e));
  }
}

//CHECK#5
obj.length = {
  toString: function() {
    return 1
  }
};
var unshift = obj.unshift();
if (unshift !== 1) {
  throw new Test262Error('#5:  obj.length = {toString: function() {return 1}}  obj.unshift() === 1. Actual: ' + (unshift));
}

//CHECK#6
obj.length = {
  valueOf: function() {
    return {}
  },
  toString: function() {
    return 1
  }
}
var unshift = obj.unshift();
if (unshift !== 1) {
  throw new Test262Error('#6:  obj.length = {valueOf: function() {return {}}, toString: function() {return 1}}  obj.unshift() === 1. Actual: ' + (unshift));
}

//CHECK#7
try {

  obj.length = {
    valueOf: function() {
      throw "error"
    },
    toString: function() {
      return 1
    }
  };
  var unshift = obj.unshift();
  throw new Test262Error('#7.1:  obj.length = {valueOf: function() {throw "error"}, toString: function() {return 1}}; obj.unshift() throw "error". Actual: ' + (unshift));
}
catch (e) {
  if (e !== "error") {
    throw new Test262Error('#7.2:  obj.length = {valueOf: function() {throw "error"}, toString: function() {return 1}}; obj.unshift() throw "error". Actual: ' + (e));
  }
}

//CHECK#8
try {

  obj.length = {
    valueOf: function() {
      return {}
    },
    toString: function() {
      return {}
    }
  };
  var unshift = obj.unshift();
  throw new Test262Error('#8.1:  obj.length = {valueOf: function() {return {}}, toString: function() {return {}}}  obj.unshift() throw TypeError. Actual: ' + (unshift));
}
catch (e) {
  if ((e instanceof TypeError) !== true) {
    throw new Test262Error('#8.2:  obj.length = {valueOf: function() {return {}}, toString: function() {return {}}}  obj.unshift() throw TypeError. Actual: ' + (e));
  }
}
