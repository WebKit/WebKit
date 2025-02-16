const x = new Proxy(Promise, {});
function foo() {
    new x(()=>{});
}
for (let i=0; i<testLoopCount; i++) {
    foo();
}
