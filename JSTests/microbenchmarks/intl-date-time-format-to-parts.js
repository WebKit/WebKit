function test() {
    const dtf = new Intl.DateTimeFormat('en-US', {
        year: 'numeric',
        month: 'long',
        day: 'numeric',
        hour: 'numeric',
        minute: 'numeric',
        second: 'numeric',
        timeZoneName: 'short'
    });
    const testDates = [
        new Date(2024, 0, 1, 12, 0, 0),
        new Date(2024, 6, 15, 18, 30, 45),
        new Date(2024, 11, 31, 23, 59, 59),
        new Date(2000, 0, 1, 0, 0, 0),
        new Date(1999, 11, 31, 12, 30, 0),
        new Date(2024, 2, 15, 6, 45, 30),
        new Date(2024, 8, 20, 14, 15, 0),
    ];

    let count = 0;
    for (let i = 0; i < 1e4; i++) {
        for (const date of testDates) {
            const parts = dtf.formatToParts(date);
            count += parts.length;
        }
    }
    return count;
}

const result = test();
if (result !== 1050000)
    throw new Error("Bad result: " + result);
