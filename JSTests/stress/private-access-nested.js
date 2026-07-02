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

shouldThrow(() => {
    class D {
      #x() {}
      m() {
        class C {
          #yy;
          #z() {}
          a() {
            this.#x();
          }
        }
        let c = new C();
        c.a();
      }
    }
    let d = new D();
    d.m();
}, `TypeError: Cannot access private method or acessor (evaluating 'this.#x')`);
