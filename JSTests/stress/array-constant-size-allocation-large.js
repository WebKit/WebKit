function f0() {
    const v56 = new Array(1600);
    let data = v56.fill(null)
    for (const v66 of data) {}
}
noInline(f0);

for (let i = 0; i < testLoopCount; ++i)
    f0();
