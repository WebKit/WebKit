function assert(successCondition) {
    if (!successCondition) {
        $vm.print("FAILED at:");
        $vm.dumpStack();
        throw "FAIL";
    }
}

function testNonStrict() {
    let foo = function () { }
    let bar = function () { }
    let arrow = () => { }
    let arrow2 = () => { }
    let native = $vm.dataLog;
    let native2 = $vm.print;

    // This test relies on invoking hasOwnProperty on a builtin first before invoking
    // it on a regular function. So, the following order is important here.
    assert(isNaN.hasOwnProperty("prototype") == false);
    assert(foo.hasOwnProperty("prototype") == true);
    assert(arrow.hasOwnProperty("prototype") == false);
    assert(native.hasOwnProperty("prototype") == false);

    assert(isFinite.hasOwnProperty("prototype") == false);
    assert(bar.hasOwnProperty("prototype") == true);
    assert(arrow2.hasOwnProperty("prototype") == false);
    assert(native2.hasOwnProperty("prototype") == false);

    // Repeat to exercise HasOwnPropertyCache.
    assert(isNaN.hasOwnProperty("prototype") == false);
    assert(foo.hasOwnProperty("prototype") == true);
    assert(arrow.hasOwnProperty("prototype") == false);
    assert(native.hasOwnProperty("prototype") == false);

    assert(isFinite.hasOwnProperty("prototype") == false);
    assert(bar.hasOwnProperty("prototype") == true);
    assert(arrow2.hasOwnProperty("prototype") == false);
    assert(native2.hasOwnProperty("prototype") == false);
}
noInline(testNonStrict);

function testStrict() {
    "use strict";

    let foo = function () { }
    let bar = function () { }
    let arrow = () => { }
    let arrow2 = () => { }
    let native = $vm.dataLog;
    let native2 = $vm.print;

    // This test relies on invoking hasOwnProperty on a builtin first before invoking
    // it on a regular function. So, the following order is important here.
    assert(isNaN.hasOwnProperty("prototype") == false);
    assert(foo.hasOwnProperty("prototype") == true);
    assert(arrow.hasOwnProperty("prototype") == false);
    assert(native.hasOwnProperty("prototype") == false);

    assert(isFinite.hasOwnProperty("prototype") == false);
    assert(bar.hasOwnProperty("prototype") == true);
    assert(arrow2.hasOwnProperty("prototype") == false);
    assert(native2.hasOwnProperty("prototype") == false);

    // Repeat to exercise HasOwnPropertyCache.
    assert(isNaN.hasOwnProperty("prototype") == false);
    assert(foo.hasOwnProperty("prototype") == true);
    assert(arrow.hasOwnProperty("prototype") == false);
    assert(native.hasOwnProperty("prototype") == false);

    assert(isFinite.hasOwnProperty("prototype") == false);
    assert(bar.hasOwnProperty("prototype") == true);
    assert(arrow2.hasOwnProperty("prototype") == false);
    assert(native2.hasOwnProperty("prototype") == false);
}
noInline(testStrict);

for (var i = 0; i < 10000; ++i) {
    testNonStrict();
    testStrict();
}
