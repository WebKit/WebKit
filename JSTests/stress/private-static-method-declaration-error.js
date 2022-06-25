function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

class C {
  get #x() {}

  foo() {
    class D {
      static #a() {}
      b() {
        return ''.#a;
      }
    }
    let d = new D();
    d.b();
  }
}

shouldThrow(() => {
    let c = new C();
    c.foo();
}, `TypeError: Cannot access static private method or accessor`);
