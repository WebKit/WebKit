function shouldThrowTDZ(func) {
    let hasThrown = false;
    try {
        func();
    } catch(e) {
        if (e.name.indexOf("ReferenceError") !== -1)
            hasThrown = true;
    }
    if (!hasThrown)
        throw new Error("Did not throw TDZ error");
}
noInline(shouldThrowTDZ);

function test(f) {
    for (let i = 0; i < 1000; i++)
        f();
}

test(function() {
    shouldThrowTDZ(function() {
        (a)``;
        let a;
    });
});

test(function() {
    shouldThrowTDZ(function() {
        (a)``;
        let a;
        function capture() { return a; }
    });
});

test(function() {
    shouldThrowTDZ(()=> { (a)``; });
    let a;
});

test(function() {
    shouldThrowTDZ(()=> { eval("(a)``"); });
    let a;
});


test(function() {
    shouldThrowTDZ(()=> { (globalLet)``; });
});
test(function() {
    shouldThrowTDZ(()=> { eval("(globalLet)``;")});
});
let globalLet;
