//@ runDefault("--validateGraphAtEachPhase=1")
for (let i = 0; i <= 1e5; i++) {
    const t1 = Array(128);
    t1[4] = 128;
}
