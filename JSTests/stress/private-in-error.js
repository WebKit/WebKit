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
  static #x() {}

  foo() {
      return #x in 42;
  }
}

shouldThrow(() => {
    let c = new C();
    c.foo();
}, `TypeError: Cannot access static private method or accessor of a non-Object`);
