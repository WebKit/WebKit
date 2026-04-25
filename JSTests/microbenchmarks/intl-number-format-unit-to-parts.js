// This benchmark tests NumberFormat with unit style which creates {type, value, unit} objects
// The pre-built Structure with unit property optimization should show improvement here
function test() {
    const formatters = [
        new Intl.NumberFormat('en', { style: 'unit', unit: 'kilometer' }),
        new Intl.NumberFormat('en', { style: 'unit', unit: 'kilogram' }),
        new Intl.NumberFormat('en', { style: 'unit', unit: 'celsius' }),
        new Intl.NumberFormat('en', { style: 'unit', unit: 'meter-per-second' }),
        new Intl.NumberFormat('en', { style: 'unit', unit: 'liter' }),
    ];

    const values = [1, 10, 100, 1000, 12345.67, 0.5, -25];

    let count = 0;
    for (let i = 0; i < 1e4; i++) {
        for (const nf of formatters) {
            for (const value of values) {
                const parts = nf.formatToParts(value);
                count += parts.length;
            }
        }
    }
    return count;
}

const result = test();
if (result !== 1430000)
    throw new Error("Bad result: " + result);
