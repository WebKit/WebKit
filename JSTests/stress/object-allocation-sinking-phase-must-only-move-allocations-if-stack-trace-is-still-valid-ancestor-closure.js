//@ runDefault("--useConcurrentJIT=0", "--forceGCSlowPaths=1", "--useExceptionFuzz=1", "--fireExceptionFuzzAt=70000")

function alloc() { return {}; }

function makeInner() {
    return function inner(q) { return alloc(); };
}

function clobber(a, b, c, d, e, f, g, h) { return a; }

let escaped;
function sink(o) { escaped = o; }
noInline(sink);

let empty = {};
let arr = [empty, empty, empty, empty, empty, empty, empty, empty];

function opt(ff, cond) {
    let v = cond;
    let c = ff(0);
    clobber(...arr);
    if (v)
        sink(c);
}
noInline(opt);

let inner0 = makeInner(), inner1 = makeInner();
function drive(i, cond) {
    opt((i & 1) ? inner1 : inner0, cond);
}
noInline(drive);

function jitter() { return {}; }
noInline(jitter);

// --fireExceptionFuzzAt selects which exception check throws, counted across the whole
// process, so no exact value survives a JSC change. Instead of relying on one: after
// warm-up every iteration of the second loop performs the sunk allocation and so can
// crash, and that loop is long enough to cover most of the run, so a target in the middle
// stays inside it even when the count shifts. Only about one check in four in there is the
// crashing one, and the jitter varies which check the target selects, so expect about one
// run in four to catch a regression.
try {
    for (let i = (Math.random() * 32) | 0; i--; )
        jitter();

    for (let i = 0; i < 8000; ++i)
        drive(i, (i & 1) == 0);
    for (let j = 0; j < 20000; ++j)
        drive(0, true);
} catch (e) { }
