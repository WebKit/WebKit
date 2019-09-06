"use strict";
function a() {
    let x = 20;
    x;
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
    c();
}
