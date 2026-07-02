"use strict";
function a() {
    let x = 20;
    x;
    return x;
}
function b() {
    let y = 40;
    return a.call({aThis: 3});
}
function c() {
    let z = 60;
    return b.call({bThis: 2}); 
}
function startABC() {
    c.call({cThis: 1});
}
