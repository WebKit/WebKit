// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: addition operator ToNumeric with BigInt operands
esid: sec-addition-operator-plus-runtime-semantics-evaluation
features: [BigInt, Symbol.toPrimitive, computed-property-names]
---*/

function err() {
  throw new Test262Error();
}

function MyError() {}

assert.sameValue(({
  [Symbol.toPrimitive]: function() {
    return 2n;
  },
  valueOf: err,
  toString: err
}) + 1n, 3n, "ToPrimitive: @@toPrimitive takes precedence");
assert.sameValue(1n + {
  [Symbol.toPrimitive]: function() {
    return 2n;
  },
  valueOf: err,
  toString: err
}, 3n, "ToPrimitive: @@toPrimitive takes precedence");
assert.sameValue(({
  valueOf: function() {
    return 2n;
  },
  toString: err
}) + 1n, 3n, "ToPrimitive: valueOf takes precedence over toString");
assert.sameValue(1n + {
  valueOf: function() {
    return 2n;
  },
  toString: err
}, 3n, "ToPrimitive: valueOf takes precedence over toString");
assert.sameValue(({
  toString: function() {
    return 2n;
  }
}) + 1n, 3n, "ToPrimitive: toString with no valueOf");
assert.sameValue(1n + {
  toString: function() {
    return 2n;
  }
}, 3n, "ToPrimitive: toString with no valueOf");
assert.sameValue(({
  [Symbol.toPrimitive]: undefined,
  valueOf: function() {
    return 2n;
  }
}) + 1n, 3n, "ToPrimitive: skip @@toPrimitive when it's undefined");
assert.sameValue(1n + {
  [Symbol.toPrimitive]: undefined,
  valueOf: function() {
    return 2n;
  }
}, 3n, "ToPrimitive: skip @@toPrimitive when it's undefined");
assert.sameValue(({
  [Symbol.toPrimitive]: null,
  valueOf: function() {
    return 2n;
  }
}) + 1n, 3n, "ToPrimitive: skip @@toPrimitive when it's null");
assert.sameValue(1n + {
  [Symbol.toPrimitive]: null,
  valueOf: function() {
    return 2n;
  }
}, 3n, "ToPrimitive: skip @@toPrimitive when it's null");
assert.sameValue(({
  valueOf: null,
  toString: function() {
    return 2n;
  }
}) + 1n, 3n, "ToPrimitive: skip valueOf when it's not callable");
assert.sameValue(1n + {
  valueOf: null,
  toString: function() {
    return 2n;
  }
}, 3n, "ToPrimitive: skip valueOf when it's not callable");
assert.sameValue(({
  valueOf: 1,
  toString: function() {
    return 2n;
  }
}) + 1n, 3n, "ToPrimitive: skip valueOf when it's not callable");
assert.sameValue(1n + {
  valueOf: 1,
  toString: function() {
    return 2n;
  }
}, 3n, "ToPrimitive: skip valueOf when it's not callable");
assert.sameValue(({
  valueOf: {},
  toString: function() {
    return 2n;
  }
}) + 1n, 3n, "ToPrimitive: skip valueOf when it's not callable");
assert.sameValue(1n + {
  valueOf: {},
  toString: function() {
    return 2n;
  }
}, 3n, "ToPrimitive: skip valueOf when it's not callable");
assert.sameValue(({
  valueOf: function() {
    return {};
  },
  toString: function() {
    return 2n;
  }
}) + 1n, 3n, "ToPrimitive: skip valueOf when it returns an object");
assert.sameValue(1n + {
  valueOf: function() {
    return {};
  },
  toString: function() {
    return 2n;
  }
}, 3n, "ToPrimitive: skip valueOf when it returns an object");
assert.sameValue(({
  valueOf: function() {
    return Object(12345);
  },
  toString: function() {
    return 2n;
  }
}) + 1n, 3n, "ToPrimitive: skip valueOf when it returns an object");
assert.sameValue(1n + {
  valueOf: function() {
    return Object(12345);
  },
  toString: function() {
    return 2n;
  }
}, 3n, "ToPrimitive: skip valueOf when it returns an object");
assert.throws(TypeError, function() {
  ({
    [Symbol.toPrimitive]: 1
  }) + 0n;
}, "ToPrimitive: throw when @@toPrimitive is not callable");
assert.throws(TypeError, function() {
  0n + {
    [Symbol.toPrimitive]: 1
  };
}, "ToPrimitive: throw when @@toPrimitive is not callable");
assert.throws(TypeError, function() {
  ({
    [Symbol.toPrimitive]: {}
  }) + 0n;
}, "ToPrimitive: throw when @@toPrimitive is not callable");
assert.throws(TypeError, function() {
  0n + {
    [Symbol.toPrimitive]: {}
  };
}, "ToPrimitive: throw when @@toPrimitive is not callable");
assert.throws(TypeError, function() {
  ({
    [Symbol.toPrimitive]: function() {
      return Object(1);
    }
  }) + 0n;
}, "ToPrimitive: throw when @@toPrimitive returns an object");
assert.throws(TypeError, function() {
  0n + {
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
  }) + 0n;
}, "ToPrimitive: throw when @@toPrimitive returns an object");
assert.throws(TypeError, function() {
  0n + {
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
  }) + 0n;
}, "ToPrimitive: propagate errors from @@toPrimitive");
assert.throws(MyError, function() {
  0n + {
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
  }) + 0n;
}, "ToPrimitive: propagate errors from valueOf");
assert.throws(MyError, function() {
  0n + {
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
  }) + 0n;
}, "ToPrimitive: propagate errors from toString");
assert.throws(MyError, function() {
  0n + {
    toString: function() {
      throw new MyError();
    }
  };
}, "ToPrimitive: propagate errors from toString");
assert.throws(TypeError, function() {
  ({
    valueOf: null,
    toString: null
  }) + 0n;
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  0n + {
    valueOf: null,
    toString: null
  };
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  ({
    valueOf: 1,
    toString: 1
  }) + 0n;
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  0n + {
    valueOf: 1,
    toString: 1
  };
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  ({
    valueOf: {},
    toString: {}
  }) + 0n;
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  0n + {
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
  }) + 0n;
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  0n + {
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
  }) + 0n;
}, "ToPrimitive: throw when skipping both valueOf and toString");
assert.throws(TypeError, function() {
  0n + {
    valueOf: function() {
      return {};
    },
    toString: function() {
      return {};
    }
  };
}, "ToPrimitive: throw when skipping both valueOf and toString");
