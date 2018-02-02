// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: unsigned-right-shift operator ToNumeric with BigInt operands
esid: sec-unsigned-right-shift-operator-runtime-semantics-evaluation
info: After ToNumeric type coercion, unsigned-right-shift always throws for BigInt operands
features: [BigInt, Symbol.toPrimitive, computed-property-names]
---*/

function err() {
  throw new Test262Error();
}

function MyError() {}

assert.throws(TypeError, function() {
  ({
    [Symbol.toPrimitive]: function() {
      return 2n;
    },
    valueOf: err,
    toString: err
  }) >>> 0n;
}, "ToPrimitive: @@toPrimitive takes precedence");
assert.throws(TypeError, function() {
  0n >>> {
    [Symbol.toPrimitive]: function() {
      return 2n;
    },
    valueOf: err,
    toString: err
  };
}, "ToPrimitive: @@toPrimitive takes precedence");
assert.throws(TypeError, function() {
  ({
    valueOf: function() {
      return 2n;
    },
    toString: err
  }) >>> 0n;
}, "ToPrimitive: valueOf takes precedence over toString");
assert.throws(TypeError, function() {
  0n >>> {
    valueOf: function() {
      return 2n;
    },
    toString: err
  };
}, "ToPrimitive: valueOf takes precedence over toString");
assert.throws(TypeError, function() {
  ({
    toString: function() {
      return 2n;
    }
  }) >>> 0n;
}, "ToPrimitive: toString with no valueOf");
assert.throws(TypeError, function() {
  0n >>> {
    toString: function() {
      return 2n;
    }
  };
}, "ToPrimitive: toString with no valueOf");
assert.throws(TypeError, function() {
  ({
    [Symbol.toPrimitive]: undefined,
    valueOf: function() {
      return 2n;
    }
  }) >>> 0n;
}, "ToPrimitive: skip @@toPrimitive when it's undefined");
assert.throws(TypeError, function() {
  0n >>> {
    [Symbol.toPrimitive]: undefined,
    valueOf: function() {
      return 2n;
    }
  };
}, "ToPrimitive: skip @@toPrimitive when it's undefined");
assert.throws(TypeError, function() {
  ({
    [Symbol.toPrimitive]: null,
    valueOf: function() {
      return 2n;
    }
  }) >>> 0n;
}, "ToPrimitive: skip @@toPrimitive when it's null");
assert.throws(TypeError, function() {
  0n >>> {
    [Symbol.toPrimitive]: null,
    valueOf: function() {
      return 2n;
    }
  };
}, "ToPrimitive: skip @@toPrimitive when it's null");
assert.throws(TypeError, function() {
  ({
    valueOf: null,
    toString: function() {
      return 2n;
    }
  }) >>> 0n;
}, "ToPrimitive: skip valueOf when it's not callable");
assert.throws(TypeError, function() {
  0n >>> {
    valueOf: null,
    toString: function() {
      return 2n;
    }
  };
}, "ToPrimitive: skip valueOf when it's not callable");
assert.throws(TypeError, function() {
  ({
    valueOf: 1,
    toString: function() {
      return 2n;
    }
  }) >>> 0n;
}, "ToPrimitive: skip valueOf when it's not callable");
assert.throws(TypeError, function() {
  0n >>> {
    valueOf: 1,
    toString: function() {
      return 2n;
    }
  };
}, "ToPrimitive: skip valueOf when it's not callable");
assert.throws(TypeError, function() {
  ({
    valueOf: {},
    toString: function() {
      return 2n;
    }
  }) >>> 0n;
}, "ToPrimitive: skip valueOf when it's not callable");
assert.throws(TypeError, function() {
  0n >>> {
    valueOf: {},
    toString: function() {
      return 2n;
    }
  };
}, "ToPrimitive: skip valueOf when it's not callable");
assert.throws(TypeError, function() {
  ({
    valueOf: function() {
      return {};
    },
    toString: function() {
      return 2n;
    }
  }) >>> 0n;
}, "ToPrimitive: skip valueOf when it returns an object");
assert.throws(TypeError, function() {
  0n >>> {
    valueOf: function() {
      return {};
    },
    toString: function() {
      return 2n;
    }
  };
}, "ToPrimitive: skip valueOf when it returns an object");
assert.throws(TypeError, function() {
  ({
    valueOf: function() {
      return Object(12345);
    },
    toString: function() {
      return 2n;
    }
  }) >>> 0n;
}, "ToPrimitive: skip valueOf when it returns an object");
assert.throws(TypeError, function() {
  0n >>> {
    valueOf: function() {
      return Object(12345);
    },
    toString: function() {
      return 2n;
    }
  };
}, "ToPrimitive: skip valueOf when it returns an object");
assert.throws(TypeError, function() {
  ({
    [Symbol.toPrimitive]: 1
  }) >>> 0n;
}, "ToPrimitive: throw when @@toPrimitive is not callable");
assert.throws(TypeError, function() {
  0n >>> {
    [Symbol.toPrimitive]: 1
  };
}, "ToPrimitive: throw when @@toPrimitive is not callable");
assert.throws(TypeError, function() {
  ({
    [Symbol.toPrimitive]: {}
  }) >>> 0n;
}, "ToPrimitive: throw when @@toPrimitive is not callable");
assert.throws(TypeError, function() {
  0n >>> {
    [Symbol.toPrimitive]: {}
  };
}, "ToPrimitive: throw when @@toPrimitive is not callable");
assert.throws(TypeError, function() {
  ({
    [Symbol.toPrimitive]: function() {
      return Object(1);
    }
  }) >>> 0n;
}, "ToPrimitive: throw when @@toPrimitive returns an object");
assert.throws(TypeError, function() {
  0n >>> {
    [Symbol.toPrimitive]: function() {
      return Object(1);
    }
  };
}, "ToPrimitive: throw when @@toPrimitive returns an object");
assert.throws(TypeError, function() {
  ({
    [Symbol.toPrimitive]: function() {
      return {};
    }
  }) >>> 0n;
}, "ToPrimitive: throw when @@toPrimitive returns an object");
assert.throws(TypeError, function() {
  0n >>> {
    [Symbol.toPrimitive]: function() {
      return {};
    }
  };
}, "ToPrimitive: throw when @@toPrimitive returns an object");
assert.throws(MyError, function() {
  ({
    [Symbol.toPrimitive]: function() {
      throw new MyError();
    }
  }) >>> 0n;
}, "ToPrimitive: propagate errors from @@toPrimitive");
assert.throws(MyError, function() {
  0n >>> {
    [Symbol.toPrimitive]: function() {
      throw new MyError();
    }
  };
}, "ToPrimitive: propagate errors from @@toPrimitive");
assert.throws(MyError, function() {
  ({
    valueOf: function() {
      throw new MyError();
    }
  }) >>> 0n;
}, "ToPrimitive: propagate errors from valueOf");
assert.throws(MyError, function() {
  0n >>> {
    valueOf: function() {
      throw new MyError();
    }
  };
}, "ToPrimitive: propagate errors from valueOf");
assert.throws(MyError, function() {
  ({
    toString: function() {
      throw new MyError();
    }
  }) >>> 0n;
}, "ToPrimitive: propagate errors from toString");
assert.throws(MyError, function() {
  0n >>> {
    toString: function() {
      throw new MyError();
    }
  };
}, "ToPrimitive: propagate errors from toString");
assert.throws(TypeError, function() {
  ({
    valueOf: null,
    toString: null
  }) >>> 0n;
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  0n >>> {
    valueOf: null,
    toString: null
  };
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  ({
    valueOf: 1,
    toString: 1
  }) >>> 0n;
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  0n >>> {
    valueOf: 1,
    toString: 1
  };
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  ({
    valueOf: {},
    toString: {}
  }) >>> 0n;
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  0n >>> {
    valueOf: {},
    toString: {}
  };
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  ({
    valueOf: function() {
      return Object(1);
    },
    toString: function() {
      return Object(1);
    }
  }) >>> 0n;
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  0n >>> {
    valueOf: function() {
      return Object(1);
    },
    toString: function() {
      return Object(1);
    }
  };
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  ({
    valueOf: function() {
      return {};
    },
    toString: function() {
      return {};
    }
  }) >>> 0n;
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  0n >>> {
    valueOf: function() {
      return {};
    },
    toString: function() {
      return {};
    }
  };
}, "ToPrimitive: throw when skipping both valueOf and toString");
