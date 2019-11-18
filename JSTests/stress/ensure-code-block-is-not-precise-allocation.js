//@ if $architecture == "mips" then skip else slow! end
// Does not crash.
function foo() {
    +new Proxy({}, {get: foo});
}

for (let i=0; i< 500; i++) {
    new Promise(foo);
    const a0 = [];
    const a1 = [0];
}
