// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator use ToString from separator
esid: sec-array.prototype.join
description: >
    If Type(separator) is Object, evaluate ToPrimitive(separator,
    String)
---*/

var x = new Array(0, 1, 2, 3);
//CHECK#1
var object = {
  valueOf: function() {
    return "+"
  }
};
if (x.join(object) !== "0[object Object]1[object Object]2[object Object]3") {
  throw new Test262Error('#1: var object = {valueOf: function() {return "+"}}; x.join(object) === "0[object Object]1[object Object]2[object Object]3". Actual: ' + (x.join(object)));
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
if (x.join(object) !== "0*1*2*3") {
  throw new Test262Error('#2: var object = {valueOf: function() {return "+"}, toString: function() {return "*"}}; x.join(object) === "0*1*2*3". Actual: ' + (x.join(object)));
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
if (x.join(object) !== "0+1+2+3") {
  throw new Test262Error('#3: var object = {valueOf: function() {return "+"}, toString: function() {return {}}}; x.join(object) === "0+1+2+3". Actual: ' + (x.join(object)));
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
  if (x.join(object) !== "0*1*2*3") {
    throw new Test262Error('#4.1: var object = {valueOf: function() {throw "error"}, toString: function() {return "*"}}; x.join(object) === "0*1*2*3". Actual: ' + (x.join(object)));
  }
}
catch (e) {
  if (e === "error") {
    throw new Test262Error('#4.2: var object = {valueOf: function() {throw "error"}, toString: function() {return "*"}}; x.join(object) not throw "error"');
  } else {
    throw new Test262Error('#4.3: var object = {valueOf: function() {throw "error"}, toString: function() {return "*"}}; x.join(object) not throw Error. Actual: ' + (e));
  }
}

//CHECK#5
var object = {
  toString: function() {
    return "*"
  }
};
if (x.join(object) !== "0*1*2*3") {
  throw new Test262Error('#5: var object = {toString: function() {return "*"}}; x.join(object) === "0*1*2*3". Actual: ' + (x.join(object)));
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
if (x.join(object) !== "0*1*2*3") {
  throw new Test262Error('#6: var object = {valueOf: function() {return {}}, toString: function() {return "*"}}; x.join(object) === "0*1*2*3". Actual: ' + (x.join(object)));
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
  x.join(object);
  throw new Test262Error('#7.1: var object = {valueOf: function() {return "+"}, toString: function() {throw "error"}}; x.join(object) throw "error". Actual: ' + (x.join(object)));
}
catch (e) {
  if (e !== "error") {
    throw new Test262Error('#7.2: var object = {valueOf: function() {return "+"}, toString: function() {throw "error"}}; x.join(object) throw "error". Actual: ' + (e));
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
  x.join(object);
  throw new Test262Error('#8.1: var object = {valueOf: function() {return {}}, toString: function() {return {}}}; x.join(object) throw TypeError. Actual: ' + (x.join(object)));
}
catch (e) {
  if ((e instanceof TypeError) !== true) {
    throw new Test262Error('#8.2: var object = {valueOf: function() {return {}}, toString: function() {return {}}}; x.join(object) throw TypeError. Actual: ' + (e));
  }
}

//CHECK#9
try {
  var object = {
    toString: function() {
      throw "error"
    }
  };
  [].join(object);
  throw new Test262Error('#9.1: var object = {toString: function() {throw "error"}}; [].join(object) throw "error". Actual: ' + ([].join(object)));
}
catch (e) {
  if (e !== "error") {
    throw new Test262Error('#9.2: var object = {toString: function() {throw "error"}}; [].join(object) throw "error". Actual: ' + (e));
  }
}
