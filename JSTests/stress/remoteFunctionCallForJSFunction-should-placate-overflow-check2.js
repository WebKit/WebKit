//@ requireOptions("--watchdog=1", "--watchdog-exception-ok")

try {
    let shadowRealm = new ShadowRealm();
    for (let i = 0; i < 1000; i++) {
      let fn = shadowRealm.evaluate(`()=>{}`);
      fn(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
} catch (e) {
}
