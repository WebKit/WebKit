function assert(b) {
    if (!b)
        throw new Error("Bad");
}

disableRichSourceInfo(); // This is needed for the JSONP path to be taken for calls.

let a = 22;
loadString("a = 42");
assert(a === 42);

let b = {f: 22};
loadString("b.f = 42");
assert(b.f === 42);

let c = [1,2,3];
loadString("c[0] = 42;");
assert(c[0] === 42);

let d = {
    foo: [[1,2,3]]
};
loadString("d.foo[0][0] = 42;");
assert(d.foo[0][0] === 42);

let e = [[[1,2,3]]];
loadString("e[0][0][0] = 42;");
assert(e[0][0][0] === 42);

let foo = null;
let bar = 42;
loadString("foo = 'root'; bar = 5")
assert(foo === "root");
assert(bar === 5);

let called = false;
let func = (a) => {
    assert(a.foo === 20);
    called = true;
};

loadString("func({foo:20})");
assert(called);

called = false;
let o = {
    foo(arg) {
        assert(arg.foo === 20);
        called = true;
    }
};

loadString("o.foo({foo:20})");
assert(called);

var theVar = 20;
loadString("theVar = 42");
assert(theVar === 42);
assert(this.theVar === 42);

called = false;
var varFunc = (a) => {
    assert(a.foo === 20);
    called = true;
};
loadString("varFunc({foo:20})");
assert(called);
