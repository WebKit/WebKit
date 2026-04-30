// padEnd a short string and consume the result via charCodeAt.
function bench() {
    let total = 0;
    const base = "hello";
    for (let i = 0; i < 1e6; ++i) {
        const s = base.padEnd(80);
        total += s.charCodeAt(79);
    }
    return total;
}
noInline(bench);

bench();
