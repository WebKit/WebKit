"use strict";
function a() {
    let x = 20;
    debugger;
    return x;
}
function b() {
    let y = 40;
    return a();
}
function c() {
    let z = 60;
    return b();
}
function startABC() {
    for (let i = 0; i < 5; ++i)
        THIS_DOES_NOT_CALL_c();
    c();
}
function THIS_DOES_NOT_CALL_c() {
    return Math.random();
}
