export function f() { return "f"; }
export function g() { return "g"; }
export function h() { return "h"; }
function s() { return "s"; }
export const before = [f(), g(), h(), s()].join(" ");
g = 42;
h = function () { return "h2"; };
Promise.resolve().then(() => { fullGC(); });
await 0;
export const after = [f(), g, h(), s()].join(" ");
export let blockResult;
{
    function f() { return "block"; }
    blockResult = f();
}
export const fAfterBlock = f();
