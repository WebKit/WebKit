// This benchmark tests IntlRelativeTimeFormat.formatToParts with numeric:"always"
// which produces {type, value, unit} objects with numeric sub-parts
function test() {
    const rtf = new Intl.RelativeTimeFormat('en', { numeric: 'always' });

    let count = 0;
    for (let i = 0; i < 1e4; i++) {
        count += rtf.formatToParts(10000.5, 'day').length;
        count += rtf.formatToParts(-3, 'hour').length;
        count += rtf.formatToParts(1, 'year').length;
        count += rtf.formatToParts(-100, 'second').length;
        count += rtf.formatToParts(0, 'week').length;
    }
    return count;
}

const result = test();
if (result !== 170000)
    throw new Error("Bad result: " + result);
