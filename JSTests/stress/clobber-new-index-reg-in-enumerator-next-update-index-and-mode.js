
let p = new Proxy(() => { }, { getOwnPropertyDescriptor: 0 });

function f2(a0) {
    for (let _ in a0) {
        return;
    }
    try {
        f2(f2);
    } catch { }
    f2([0]);
    f2(p);
}

try {
    f2();
} catch (e) {
}
