//@ runFTLEagerWatchdog

for (let i = 0; i < 7000; ++i) {
    mallocInALoop();
}
