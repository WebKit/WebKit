
for (let i = 0; i < 1000; i++) {
    try {
        v69 = 6n; v69 **= -2n;
        1n % 0n;
        1n / 0n;
        512n ** 512n ** 512n;
    } catch (e) {
    }
}
