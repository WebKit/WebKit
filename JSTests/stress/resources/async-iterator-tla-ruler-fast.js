// TLA module (fast path): a top-level for-await over a pristine async generator, interleaved with a
// fixed 8-turn microtask "ruler". The interleaving of ruler ticks (Rn) with consumer events makes
// the per-step microtask cadence observable. Result is compared against the generic-path module.
const log = [];
const E = s => log.push(s);

let p = Promise.resolve();
for (let i = 0; i < 8; i++) {
    const j = i;
    p = p.then(() => E("R" + j));
}

async function* g() { yield 1; yield 2; yield 3; }
for await (const x of g())
    E("v" + x);
E("done");

globalThis.__tlaRulerFast = log.join(",");
export { };
