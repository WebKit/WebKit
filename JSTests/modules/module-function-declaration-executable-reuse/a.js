import { gSeenInB, hSeenInB, kSeenInB } from "./b.js";

export function f() { return "f"; }
export function g() { return "g"; }
export function h() { return "h"; }
export function k() { return "k"; }
export function setG(value) { g = value; }
export function setH(value) { h = value; }
export function setK(value) { k = value; }
export function* gen() { yield "gen"; }
export async function asyncFn() { return "async"; }
export async function* asyncGen() { yield "asyncGen"; }

function captured() { return "captured"; }
export function callCaptured() { return captured(); }

function local() { return "local"; }
export const localResult = local();

export let blockResult;
{
    function f() { return "block"; }
    blockResult = f();
}

export const gInBody = g;
export const hInBody = h;
export const kInBody = k;
export { gSeenInB, hSeenInB, kSeenInB };
