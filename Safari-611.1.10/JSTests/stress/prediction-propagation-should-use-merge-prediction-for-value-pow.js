//@ runDefault("--useRandomizingFuzzerAgent=1")
for (let i = 0; i < 100000; i = i + 1 | 0) {
    if (i ** 2) {}
}
