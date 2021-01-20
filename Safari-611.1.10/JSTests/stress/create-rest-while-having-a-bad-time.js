"use strict";
function f(...v) {
  return g(v);
}
function g() {
  return h();
}
function h() {
}

for (let i = 0; i < 10000; ++i) {
  f(0);
  f(0, 0);
}

Object.defineProperty(Array.prototype, "42", {});
