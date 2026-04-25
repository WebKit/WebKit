// This benchmark tests IntlRelativeTimeFormat.formatToParts with numeric:"auto"
// which can produce literal-only output (e.g. "today", "yesterday")
function test() {
    const rtf = new Intl.RelativeTimeFormat('en', { numeric: 'auto' });

    let count = 0;
    for (let i = 0; i < 1e4; i++) {
        count += rtf.formatToParts(0, 'day').length;
        count += rtf.formatToParts(-1, 'week').length;
        count += rtf.formatToParts(1, 'month').length;
        count += rtf.formatToParts(-1, 'day').length;
        count += rtf.formatToParts(1, 'day').length;
    }
    return count;
}

const result = test();
if (result !== 50000)
    throw new Error("Bad result: " + result);
