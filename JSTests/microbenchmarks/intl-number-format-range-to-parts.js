// This benchmark tests formatRangeToParts which creates {type, value, source} objects
// The pre-built Structure optimization should show significant improvement here
function test() {
    const nf = new Intl.NumberFormat('en-US', {
        style: 'currency',
        currency: 'USD',
        minimumFractionDigits: 2,
        maximumFractionDigits: 2
    });

    const ranges = [
        [100, 200],
        [1000, 5000],
        [0.01, 0.99],
        [10000, 50000],
        [123.45, 678.90],
        [-100, 100],
        [1, 1000000],
    ];

    let count = 0;
    for (let i = 0; i < 1e4; i++) {
        for (const [start, end] of ranges) {
            const parts = nf.formatRangeToParts(start, end);
            count += parts.length;
        }
    }
    return count;
}

const result = test();
if (result !== 760000)
    throw new Error("Bad result: " + result);
