//@ requireOptions("--watchdog=10", "--watchdog-exception-ok")

try {
    function foo() {
      let shadowRealm = new ShadowRealm();
      for (let i = 0; i < 1000; i++) {
        let fn = shadowRealm.evaluate(`()=>{}`);
        let u = new Uint8Array(10000);
        fn(...u);
      }
    }

    foo();
    for (let i = 0; i < 100; i++) {
      runString(`(${foo.toString()})()`);
    }
} catch (e) {
}
