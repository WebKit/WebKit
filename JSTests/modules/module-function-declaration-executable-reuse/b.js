import { g, h, k, setG, setH, setK } from "./a.js";

export const gSeenInB = g();
export const hSeenInB = h();
export const kSeenInB = k();
setG(function () { return "replaced"; });
setH(42);
setK(Math.max);
