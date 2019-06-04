//@ requireOptions("--forceEagerCompilation=1")
function f2(...a1) {
    return a1;
}

function f3(...a2) {
    let v1 = f2([]);
    return f2(...a2, ...v1);
}
noInline(f3);

for (let i = 0; i < 1e5; i++) {
    var v3 = f3();
    if (!Array.isArray(v3[v3.length - 1]))
        throw new Error('Should be an array');
}
