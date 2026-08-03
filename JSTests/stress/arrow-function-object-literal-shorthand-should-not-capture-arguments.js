function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual);
}

// An object-literal shorthand property inside an arrow function must not force the
// enclosing function to capture its `arguments`. Otherwise the arguments passed to `make`
// are retained by the closures it returns.
function make(opts) {
    const host = {};
    const worker = (r) => ({ r });
    return { worker, inner };
    function inner() { return host; }
}
noInline(make);

const refs = [];
let cur = make({ previous: null });
for (let i = 0; i < 128; i++) {
    const opts = { previous: cur, payload: new Uint8Array(1024) };
    refs.push(new WeakRef(opts));
    cur = make(opts);
}

// `{ eval }` and `{ arguments }` shorthands inside arrow functions still resolve to the
// enclosing function's bindings.
function shorthandEvalAndArguments(a, b, c) {
    const f = () => ({ eval, arguments, len: arguments.length });
    return f();
}
noInline(shorthandEvalAndArguments);
for (let i = 0; i < 100; i++) {
    const result = shorthandEvalAndArguments(1, 2, 3);
    shouldBe(result.eval, eval);
    shouldBe(result.arguments[1], 2);
    shouldBe(result.len, 3);
}

// Direct eval next to a shorthand property inside an arrow function still sees the
// enclosing function's variables.
function shorthandWithDirectEval(code) {
    const x = 42;
    const f = (s) => ({ x, v: eval(s) });
    return f(code);
}
noInline(shorthandWithDirectEval);
for (let i = 0; i < 100; i++) {
    const result = shorthandWithDirectEval("arguments[0].length");
    shouldBe(result.x, 42);
    shouldBe(result.v, "arguments[0].length".length);
}

// WeakRef targets are kept alive until the end of the current job, so check liveness
// from a later task.
setTimeout(() => {
    gc();
    gc();
    let alive = 0;
    for (const ref of refs) {
        if (ref.deref())
            alive++;
    }
    if (alive > refs.length / 4)
        throw new Error("Too many option objects are retained: " + alive + " / " + refs.length);
}, 0);
