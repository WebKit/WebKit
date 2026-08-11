function shouldThrowTDZ(func) {
    var hasThrown = false;
    try {
        func();
    } catch(e) {
        hasThrown = e instanceof ReferenceError;
    }
    if (!hasThrown)
        throw new Error("Did not throw TDZ error");
}

function test(f, n = 1000) {
    for (let i = 0; i < n; i++)
        f();
}

test(function() {
    function foo() {
        eval("20");
        let eval;
    }
    shouldThrowTDZ(foo);
});

test(function() {
    function foo() {
        eval("20");
        let {eval} = {eval:450};
    }
    shouldThrowTDZ(foo);
});

test(function() {
    function foo() {
        eval("20");
        const eval = 45;
    }
    shouldThrowTDZ(foo);
});

test(function() {
    function foo() {
        eval("20");
    }
    shouldThrowTDZ(foo);
    let eval;
});

test(function() {
    function foo() {
        eval("20");
    }
    shouldThrowTDZ(foo);
    let {eval} = {eval:450};
});

test(function() {
    function foo() {
        eval("20");
    }
    shouldThrowTDZ(foo);
    const eval = 45;
});

{
    let threw = false;
    try {
        eval(20);
        let eval;
    } catch(e) {
        threw = e instanceof ReferenceError;
    }
    if (!threw)
        throw new Error("Bad")
}

{
    let threw = false;
    try {
        eval(20);
        const eval = 25;
    } catch(e) {
        threw = e instanceof ReferenceError;
    }
    if (!threw)
        throw new Error("Bad")
}
