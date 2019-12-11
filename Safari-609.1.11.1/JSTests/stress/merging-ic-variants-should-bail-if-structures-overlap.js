//@ runDefault("--validateGraphAtEachPhase=1", "--useLLInt=0")

let items = [];
for (let i = 0; i < 8; ++i) {
    class C {
    }
    items.push(new C());
}
function foo(x) {
    x.z = 0;
}
for (let i = 0; i < 100000; ++i) {
    for (let j = 0; j < items.length; ++j) {
        foo(items[j]);
    }
}
