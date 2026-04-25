// This should not crash.

function foo() {
    for (const v of [X(...(v>>=2))]) {}
}
noInline(foo);

function bar() {
    for (const v of [new X(...(v>>=2))]) {}
}
noInline(bar);

for (let i = 0; i < testLoopCount; ++i) {
    try { foo(); } catch { }

    try { bar(); } catch { }
}
