function assert(cond) {
    if (!cond)
        throw new Error("broke assertion");
}
noInline(assert);

let foo = "foo";
const bar = "bar";

for (let i = 0; i < 1000; i++) {
    assert(foo === "foo");
    assert(bar === "bar");
}

eval("var INJECTION = 20");

for (let i = 0; i < 100; i++) {
    assert(foo === "foo");
    assert(bar === "bar");
    assert(INJECTION === 20);

    let threw = false;
    try {
        eval("var foo;");
    } catch(e) {
        threw = true;
        assert(e.toString() === "SyntaxError: Can't create duplicate variable in eval: 'foo'");
    }
    assert(threw);


    threw = false;
    try {
        eval("var bar;");
    } catch(e) {
        threw = true;
        assert(e.toString() === "SyntaxError: Can't create duplicate variable in eval: 'bar'");
    }
    assert(threw);


    assert(foo === "foo");
    assert(bar === "bar");
    assert(INJECTION === 20);

    threw = false;
    try {
        eval("function foo() {}");
    } catch(e) {
        threw = true;
        assert(e.toString() === "SyntaxError: Can't create duplicate variable in eval: 'foo'");
    }
    assert(threw);


    assert(foo === "foo");
    assert(bar === "bar");
    assert(INJECTION === 20);

    threw = false;
    try {
        eval("function bar() {}");
    } catch(e) {
        threw = true;
        assert(e.toString() === "SyntaxError: Can't create duplicate variable in eval: 'bar'");
    }
    assert(threw);

    assert(foo === "foo");
    assert(bar === "bar");
    assert(INJECTION === 20);
}


var flag = false;
function baz() {
    if (flag) eval("var foo = 20;");
    return foo;
}

for (var i = 0; i < 1000; i++) {
    assert(baz() === "foo");
    assert(baz() === foo);
}
flag = true;
for (var i = 0; i < 1000; i++) {
    assert(baz() === 20);
}
