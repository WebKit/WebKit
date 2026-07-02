function hot(i) {
    let a = new Array(4);
    a[0] = 0;
    if (i & 1) a[3] = null;
    if (i === 500000) Object.defineProperty(Array.prototype, '1', {get() {}, configurable: true});
    if (i & 4) return a;
}
noInline(hot);
for (let i = 0; i < 1500000; i++) hot(i);
