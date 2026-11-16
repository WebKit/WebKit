// padEnd then indexOf forces full rope resolution + linear scan.
function bench() {
    let total = 0;
    const base = "hello";
    for (let i = 0; i < 1e6; ++i) {
        const s = base.padEnd(80);
        total += s.indexOf("x");
    }
    return total;
}
noInline(bench);

bench();
