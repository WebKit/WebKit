//@ requireOptions("--useShadowRealm=1")
let s = new ShadowRealm();
let z = s.evaluate(`(function foo() {}).bind()`);
z.toString();
