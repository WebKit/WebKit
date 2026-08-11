function test() {
    const nf = new Intl.NumberFormat('en-US', { style: 'currency', currency: 'USD' });
    const testValues = [
        12345.67,
        -9876.54,
        0.99,
        1000000,
        0.001,
        -0.5,
        123456789.12,
    ];

    let count = 0;
    for (let i = 0; i < 1e4; i++) {
        for (const value of testValues) {
            const parts = nf.formatToParts(value);
            count += parts.length;
        }
    }
    return count;
}

const result = test();
if (result !== 420000)
    throw new Error("Bad result: " + result);
