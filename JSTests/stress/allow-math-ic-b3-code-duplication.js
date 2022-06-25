//@ skip if $architecture == "x86"

function test1() {
    var o1;
    for (let i = 0; i < 1000000; ++i) {
        var o2 = { f: { f: { f: { f: { f: { f: { f: { f: { f: { f: { f: { f: { } } } } } } } } } } } } };
    }
    return -o2;
}
test1();

function test2() {
    var o1;
    for (let i = 0; i < 1000000; ++i) {
        var o2 = { f: { f: { f: { f: { f: { f: { f: { f: { f: { f: { f: { f: { } } } } } } } } } } } } };
    }
    return o1 - o2;
}
test2();

function test3() {
    var o1;
    for (let i = 0; i < 1000000; ++i) {
        var o2 = { f: { f: { f: { f: { f: { f: { f: { f: { f: { f: { f: { f: { } } } } } } } } } } } } };
    }
    return o1 + o2;
}
test3();

function test4() {
    var o1;
    for (let i = 0; i < 1000000; ++i) {
        var o2 = { f: { f: { f: { f: { f: { f: { f: { f: { f: { f: { f: { f: { } } } } } } } } } } } } };
    }
    return o1 * o2;
}
test4();
