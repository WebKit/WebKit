function assert(x) {
  if (!x)
    throw new Error("Bad assertion!");
}

(function superCallReturnPrimitive() {
  var iterations = 1e4;
  var caughtInner = 0;
  var caughtOuter = 0;

  class C extends class {} {
    constructor(i) {
      super();

      {
        if (typeof i === "number") {
          try {
            return i;
          } catch {
            caughtInner++;
            return;
          }
        }
      }
    }
  }

  for (var i = 0; i < iterations; i++) {
    try {
      new C(i);
    } catch (err) {
      if (err instanceof TypeError)
        caughtOuter++;
    }
  }

  assert(caughtInner === 0);
  assert(caughtOuter === iterations);
})();

(function noSuperCallReturnThis() {
  var iterations = 1e4;
  var caughtInner = 0;
  var caughtOuter = 0;

  class C extends class {} {
    constructor(i) {
      {
        if (typeof i === "number") {
          try {
            return;
          } catch {
            caughtInner++;
          }
        }
      }
    }
  }

  for (var i = 0; i < iterations; i++) {
    try {
      new C(i);
    } catch (err) {
      if (err instanceof ReferenceError)
        caughtOuter++;
    }
  }

  assert(caughtInner === 0);
  assert(caughtOuter === iterations);
})();
