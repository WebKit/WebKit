//@ requireOptions("--useShadowRealm=1")
let foo = new ShadowRealm().evaluate(`()=>{}`);
for (let i = 0; i < 1000000; i++) {
    foo();
}
