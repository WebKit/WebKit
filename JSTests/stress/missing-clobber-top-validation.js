//@ runDefault("--validateDFGClobberize=1")

for (let i=0; i<testLoopCount; i++) {
    Number([]);
}
