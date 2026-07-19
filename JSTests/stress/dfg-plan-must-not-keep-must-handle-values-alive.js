//@ requireOptions("--numberOfDFGCompilerThreads=0")

// With no DFG compiler threads, the DFG plan for work() is enqueued at the loop tier-up
// point and stays in the JITWorklist queue forever. Its mustHandleValues snapshot contains
// the object that was live in that frame; a queued plan must not keep such objects alive.

const rounds = 12;
const refs = [];

function work(o) {
    let s = 0;
    for (let i = 0; i < 300000; i++)
        s += o.v;
    return s;
}
noInline(work);

function iter() {
    const o = { v: 1 };
    refs.push(new WeakRef(o));
    return work(o);
}
noInline(iter);

function clobber(d) { let a = { x: d }; if (d) return clobber(d - 1) + a.x; return 0; }
noInline(clobber);

for (let k = 0; k < rounds; k++) {
    iter();
    $.clearKeptObjects(); // WeakRef construction adds the target to [[KeptAlive]]; drop it before GC.
    clobber(300);
    fullGC();
}
fullGC();

// The last couple of objects may still be reachable via stale stack slots (conservative
// scan), but everything older than that must have been collected.
let survivors = [];
for (let i = 0; i < rounds - 2; i++) {
    if (refs[i].deref() !== undefined)
        survivors.push(i);
}
if (survivors.length)
    throw new Error("Objects captured by a queued DFG plan were kept alive: " + survivors);
