//@ runDefault("--validateAbstractInterpreterState=1")

for (var i = 0; i < 10000000; ++i) {
    fiatInt52(0.0)
}
