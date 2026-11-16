//@ runDefault("--useRandomizingFuzzerAgent=1")
for (let i = 0; i < testLoopCount; i = i + 1 | 0) {
    if (i ** 2) {}
}
