'use strict';

function assert(b) {
    if (!b)
        throw new Error("Bad");
}

function test1(x) {
    for (let x = 20; x < 30; ++x) { }
    return x;
}
function test2(x) {
    for (let x of [1,2,3]) { }
    return x;
}
function test3(x) {
    for (let x in {}) { }
    return x;
}
function test4(x) {
    let i = 0;
    for (const x = 20; i < 1; ++i) { }
    return x;
}
function test5(x) {
    for (const x of [1, 2, 3]) { }
    return x;
}
function test6(x) {
    for (const x in {}) { }
    return x;
}

let test7 = (x) => {
    for (let x = 20; x < 30; ++x) { }
    return x;
}
let test8 = (x) => {
    for (let x of [1,2,3]) { }
    return x;
}
let test9 = (x) => {
    for (let x in {}) { }
    return x;
}
let test10 = (x) => {
    let i = 0;
    for (const x = 20; i < 1; ++i) { }
    return x;
}
let test11 = (x) => {
    for (const x of [1, 2, 3]) { }
    return x;
}
let test12 = (x) => {
    for (const x in {}) { }
    return x;
}

for (let test of [test1, test2, test3, test4, test5, test7, test8, test9, test10, test11, test12])
    assert(test("foo") === "foo");
