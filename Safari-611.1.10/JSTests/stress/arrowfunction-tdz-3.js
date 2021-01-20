var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};

var A = class A {
   constructor() {
      this.id = 'A'
   }
};

var B = class B extends A {
  constructor(beforeSuper) {
    var f = () => {
      if (this.id === 'A') {
        return 'ok';
      }
      return 'ok';
    };
    let val;
    if (beforeSuper) {
      val = f();
      super();
    } else {
      super();
      val = f();
    }
    this.res = val;
  }
};

var C = class C extends A {
  constructor(beforeSuper) {
    var f = () => {
      if (this > 5) {
        return 'ok';
      }
      return 'ok';
    };
    let val;
    if (beforeSuper) {
      val = f();
      super();
    } else {
      super();
      val = f();
    }
    this.res = val;
  }
};

var D = class D extends A {
  constructor(beforeSuper) {
    var f = () => {
      if (this < 5) {
        return 'ok';
      }
      return 'ok';
    };
    let val;
    if (beforeSuper) {
      val = f();
      super();
    } else {
      super();
      val = f();
    }
    this.res = val;
  }
};

var E = class E extends A {
  constructor(beforeSuper) {
    var f = () => {
      if (this !== 5) {
        return 'ok';
      }
      return 'ok';
    };
    let val;
    if (beforeSuper) {
      val = f();
      super();
    } else {
      super();
       val = f();
    }
    this.res = val;
  }
};

var F = class F extends A {
  constructor(beforeSuper) {
    var f = () => {
      if (this <= 5) {
        return 'ok';
      }
      return 'ok';
    };
    let val;
    if (beforeSuper) {
      val = f();
      super();
    } else {
      super();
      val = f();
    }
    this.res = val;
  }
};

var G = class G extends A {
  constructor(beforeSuper) {
    var f = () => {
      if (this >= 5) {
        return 'ok';
      }
      return 'ok';
    };
    let val;
    if (beforeSuper) {
      val = f();
      super();
    } else {
      super();
      val = f();
    }
    this.res = val;
  }
};

var G = class G extends A {
  constructor(beforeSuper) {
    var f = () => {
      if (this === 5) {
        return 'ok';
      }
      return 'ok';
    };
    let val;
    if (beforeSuper) {
      val = f();
      super();
    } else {
      super();
      val = f();
    }
    this.res = val;
  }
};

var tryToCreate = function (classForCreate) {
  var result = false;
  try {
       new classForCreate(true);
  } catch (e) {
      result = e instanceof ReferenceError;
  }

  return result;
}

var check = function (classForCheck) {
  testCase(tryToCreate(classForCheck), true, 'Exception wasn\'t thrown or was not a reference error');
  var result = new classForCheck(false);
  testCase(result.res, 'ok', 'Error in setting id ');
}

for (var i = 0; i < 10000; i++) {
  check(B);
  check(C);
  check(D);
  check(E);
  check(F);
  check(G);
}
