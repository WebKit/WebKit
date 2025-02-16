//@ requireOptions("--useShadowRealm=1")
let foo = new ShadowRealm().evaluate(`()=>{}`);
for (let i = 0; i < testLoopCount; i++) {
    foo();
}
