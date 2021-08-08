// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The push function is intentionally generic.
    It does not require that its this value be an Array object
esid: sec-array.prototype.push
description: >
    Operator use ToNumber from length.  If Type(value) is Object,
    evaluate ToPrimitive(value, Number)
---*/

var obj = {};
obj.push = Array.prototype.push;

//CHECK#1
obj.length = {
  valueOf: function() {
    return 3
  }
};
var push = obj.push();
if (push !== 3) {
  throw new Test262Error('#1:  obj.length = {valueOf: function() {return 3}}  obj.push() === 3. Actual: ' + (push));
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
var push = obj.push();
if (push !== 3) {
  throw new Test262Error('#0:  obj.length = {valueOf: function() {return 3}, toString: function() {return 1}}  obj.push() === 3. Actual: ' + (push));
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
var push = obj.push();
if (push !== 3) {
  throw new Test262Error('#1:  obj.length = {valueOf: function() {return 3}, toString: function() {return {}}}  obj.push() === 3. Actual: ' + (push));
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
  var push = obj.push();
  if (push !== 3) {
    throw new Test262Error('#4.1:  obj.length = {valueOf: function() {return 3}, toString: function() {throw "error"}}; obj.push() === ",". Actual: ' + (push));
  }
}
catch (e) {
  if (e === "error") {
    throw new Test262Error('#4.2:  obj.length = {valueOf: function() {return 3}, toString: function() {throw "error"}}; obj.push() not throw "error"');
  } else {
    throw new Test262Error('#4.3:  obj.length = {valueOf: function() {return 3}, toString: function() {throw "error"}}; obj.push() not throw Error. Actual: ' + (e));
  }
}

//CHECK#5
obj.length = {
  toString: function() {
    return 1
  }
};
var push = obj.push();
if (push !== 1) {
  throw new Test262Error('#5:  obj.length = {toString: function() {return 1}}  obj.push() === 1. Actual: ' + (push));
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
var push = obj.push();
if (push !== 1) {
  throw new Test262Error('#6:  obj.length = {valueOf: function() {return {}}, toString: function() {return 1}}  obj.push() === 1. Actual: ' + (push));
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
  var push = obj.push();
  throw new Test262Error('#7.1:  obj.length = {valueOf: function() {throw "error"}, toString: function() {return 1}}; obj.push() throw "error". Actual: ' + (push));
}
catch (e) {
  if (e !== "error") {
    throw new Test262Error('#7.2:  obj.length = {valueOf: function() {throw "error"}, toString: function() {return 1}}; obj.push() throw "error". Actual: ' + (e));
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
  var push = obj.push();
  throw new Test262Error('#8.1:  obj.length = {valueOf: function() {return {}}, toString: function() {return {}}}  obj.push() throw TypeError. Actual: ' + (push));
}
catch (e) {
  if ((e instanceof TypeError) !== true) {
    throw new Test262Error('#8.2:  obj.length = {valueOf: function() {return {}}, toString: function() {return {}}}  obj.push() throw TypeError. Actual: ' + (e));
  }
}
