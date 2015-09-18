function assert(cond) {
    if (!cond)
        throw new Error("broke assertion");
}
noInline(assert);

function shouldThrowInvalidConstAssignment(f) {
    var threw = false;
    try {
        f();
    } catch(e) {
        //print(e);
        if (e.name.indexOf("TypeError") !== -1 && e.message.indexOf("readonly") !== -1)
            threw = true;
    }
    assert(threw);
}
noInline(shouldThrowInvalidConstAssignment);

function baz(){}
noInline(baz);

function foo() {
    for (const item of [1,2,3]) {
        item = 20;
    }
}
for (var i = 0; i < 1000; i++)
    shouldThrowInvalidConstAssignment(foo);
