// TLA module (generic path): identical to async-iterator-tla-ruler-fast.js, but first breaks the
// pristine-next identity by wrapping %AsyncGeneratorPrototype%.next. This forces op_async_iterator_open
// to skip the fast sentinel so op_async_iterator_next takes its generic real-call branch, and the
// module suspends/resumes through the ordinary AsyncModuleExecutionResume promise path. The wrapper
// forwards synchronously, so any divergence from the fast log is a fast-path-only tick/ordering bug.
const asyncGenProto = Object.getPrototypeOf(Object.getPrototypeOf((async function* () {})()));
const origNext = asyncGenProto.next;
asyncGenProto.next = function (...args) { return origNext.apply(this, args); };

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

asyncGenProto.next = origNext;
globalThis.__tlaRulerGeneric = log.join(",");
export { };
