//@ runDefault("--useTypeProfiler=1")

function foo(z) {
    bar(z);
}
function bar(o) {
    o.x = 0;
}
let p = 0;
let k = {};
for (var i = 0; i < 100000; ++i) {
    bar(p);
    foo(k);
}
