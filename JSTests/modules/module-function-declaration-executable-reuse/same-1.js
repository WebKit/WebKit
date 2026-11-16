import "./same-setter.js";
export function f() { return "f"; }
export function setF(value) { f = value; }
export const fInBody = f;
