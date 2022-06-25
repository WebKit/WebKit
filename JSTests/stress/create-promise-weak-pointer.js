const x = new Proxy(Promise, {});
function foo() {
    new x(()=>{});
}
for (let i=0; i<100000; i++) {
    foo();
}
