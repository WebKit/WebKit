function assert(b) {
    if (!b)
        throw new Error("Bad!");
}
noInline(assert);

function f1() { return "f1"; }
noInline(f1);
function f2() { return "f2"; }
noInline(f2);
function f3() { return "f3"; }
noInline(f3);

let oException = {
    valueOf() { throw new Error(""); }
};

function foo(arg1, arg2) {
    let a = f1();
    let b = f2();
    let c = f3();
    try {
        arg1 + arg2;
    } catch(e) {
        assert(arg1 === oException);
        assert(arg2 === oException);
    }
    assert(a === "f1");
    assert(b === "f2");
    assert(c === "f3");
}
noInline(foo);

for (let i = 0; i < 1000; i++) {
    foo(i, {});
    foo({}, i);
}
foo(oException, oException);
for (let i = 0; i < 10000; i++) {
    foo(i, {});
    foo({}, i);
}
foo(oException, oException);


function ident(x) { return x; }
noInline(ident);

function bar(arg1, arg2) {
    let a = f1();
    let b = f2();
    let c = f3();
    let x = ident(arg1);
    let y = ident(arg2);

    try {
        arg1 + arg2;
    } catch(e) {
        assert(arg1 === oException);
        assert(arg2 === oException);
        assert(x === oException);
        assert(y === oException);
    }
    assert(a === "f1");
    assert(b === "f2");
    assert(c === "f3");
}
noInline(bar);

for (let i = 0; i < 1000; i++) {
    bar(i, {});
    bar({}, i);
}
bar(oException, oException);
for (let i = 0; i < 10000; i++) {
    bar(i, {});
    bar({}, i);
}
bar(oException, oException);
