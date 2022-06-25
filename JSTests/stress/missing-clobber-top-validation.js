//@ runDefault("--validateDFGClobberize=1")

for (let i=0; i<10000; i++) {
    Number([]);
}
