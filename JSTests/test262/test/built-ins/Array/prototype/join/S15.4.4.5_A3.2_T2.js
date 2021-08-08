// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator use ToString from array arguments
esid: sec-array.prototype.join
description: If Type(argument) is Object, evaluate ToPrimitive(argument, String)
---*/

//CHECK#1
var object = {
  valueOf: function() {
    return "+"
  }
};
var x = new Array(object);
if (x.join() !== "[object Object]") {
  throw new Test262Error('#1: var object = {valueOf: function() {return "+"}} var x = new Array(object); x.join() === "[object Object]". Actual: ' + (x.join()));
}

//CHECK#2
var object = {
  valueOf: function() {
    return "+"
  },
  toString: function() {
    return "*"
  }
};
var x = new Array(object);
if (x.join() !== "*") {
  throw new Test262Error('#2: var object = {valueOf: function() {return "+"}, toString: function() {return "*"}} var x = new Array(object); x.join() === "*". Actual: ' + (x.join()));
}

//CHECK#3
var object = {
  valueOf: function() {
    return "+"
  },
  toString: function() {
    return {}
  }
};
var x = new Array(object);
if (x.join() !== "+") {
  throw new Test262Error('#3: var object = {valueOf: function() {return "+"}, toString: function() {return {}}} var x = new Array(object); x.join() === "+". Actual: ' + (x.join()));
}

//CHECK#4
try {
  var object = {
    valueOf: function() {
      throw "error"
    },
    toString: function() {
      return "*"
    }
  };
  var x = new Array(object);
  if (x.join() !== "*") {
    throw new Test262Error('#4.1: var object = {valueOf: function() {throw "error"}, toString: function() {return "*"}} var x = new Array(object); x.join() === "*". Actual: ' + (x.join()));
  }
}
catch (e) {
  if (e === "error") {
    throw new Test262Error('#4.2: var object = {valueOf: function() {throw "error"}, toString: function() {return "*"}} var x = new Array(object); x.join() not throw "error"');
  } else {
    throw new Test262Error('#4.3: var object = {valueOf: function() {throw "error"}, toString: function() {return "*"}} var x = new Array(object); x.join() not throw Error. Actual: ' + (e));
  }
}

//CHECK#5
var object = {
  toString: function() {
    return "*"
  }
};
var x = new Array(object);
if (x.join() !== "*") {
  throw new Test262Error('#5: var object = {toString: function() {return "*"}} var x = new Array(object); x.join() === "*". Actual: ' + (x.join()));
}

//CHECK#6
var object = {
  valueOf: function() {
    return {}
  },
  toString: function() {
    return "*"
  }
}
var x = new Array(object);
if (x.join() !== "*") {
  throw new Test262Error('#6: var object = {valueOf: function() {return {}}, toString: function() {return "*"}} var x = new Array(object); x.join() === "*". Actual: ' + (x.join()));
}

//CHECK#7
try {
  var object = {
    valueOf: function() {
      return "+"
    },
    toString: function() {
      throw "error"
    }
  };
  var x = new Array(object);
  x.join();
  throw new Test262Error('#7.1: var object = {valueOf: function() {return "+"}, toString: function() {throw "error"}} var x = new Array(object); x.join() throw "error". Actual: ' + (x.join()));
}
catch (e) {
  if (e !== "error") {
    throw new Test262Error('#7.2: var object = {valueOf: function() {return "+"}, toString: function() {throw "error"}} var x = new Array(object); x.join() throw "error". Actual: ' + (e));
  }
}

//CHECK#8
try {
  var object = {
    valueOf: function() {
      return {}
    },
    toString: function() {
      return {}
    }
  };
  var x = new Array(object);
  x.join();
  throw new Test262Error('#8.1: var object = {valueOf: function() {return {}}, toString: function() {return {}}} var x = new Array(object); x.join() throw TypeError. Actual: ' + (x.join()));
}
catch (e) {
  if ((e instanceof TypeError) !== true) {
    throw new Test262Error('#8.2: var object = {valueOf: function() {return {}}, toString: function() {return {}}} var x = new Array(object); x.join() throw TypeError. Actual: ' + (e));
  }
}
