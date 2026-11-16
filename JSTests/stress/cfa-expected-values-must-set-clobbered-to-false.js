//@ runDefault("--useFTLJIT=0", "--useConcurrentJIT=false")

let num = 150;

function foo(comp, o, b) {
    let sum = o.f;
    if (b)
        OSRExit();
    for (let i = 0; i < comp; ++i) {
        sum += o.f;
    }
    return sum;
}
noInline(foo);

let o = {f:25};
let o2 = {f:25, g:44};
o2.f = 45;
o2.f = 45;
o2.f = 45;
o2.f = 45;
let comp = {
    valueOf() { return num; }
}

foo(comp, o2, true);
foo(comp, o2, true);
for (let i = 0; i < 500; ++i) {
    foo(comp, o2, false);
}

let o3 = {g:24, f:73};
num = 10000000;
let result = foo(comp, o3, false);

if (result !== (num + 1)*73) {
    throw new Error("Bad: " + result);
}
