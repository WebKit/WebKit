"use strict";
function a() {
    let x = 20;
    x;
    return x;
}
function b() {
    let y = 40;
    return a.call({aThis: 2});
}
function c() {
    let z = 60;
    return b.call({bThis: 1}); 
}
function startABC() {
    c.call({cThis: 0});
}
