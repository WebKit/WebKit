function shouldBe(actual, expected) {
    if (Number.isNaN(expected)) {
        if (!Number.isNaN(actual)) {
            throw new Error(`expected ${expected} but got ${actual}`);
        }
    } else if (actual !== expected) {
        throw new Error(`expected ${expected} but got ${actual}`);
    }
}

function validateDate(dateString, expected) {
    shouldBe(new Date(dateString).getTime(), expected);
    shouldBe(Date.parse(dateString), expected);
    shouldBe(new Date(dateString).getTime(), Date.parse(dateString));
}

for (let i = 0; i < testLoopCount; ++i) {
    validateDate("2024-12-3", 1733212800000);
    validateDate("2024-12-03", 1733184000000);

    validateDate("2024-1-3", 1704268800000);
    validateDate("2024-01-3", 1704268800000);
    validateDate("2024-1-03", 1704268800000);
    validateDate("2024-01-03", 1704240000000);

    validateDate("2024-11-03", 1730592000000);
    validateDate("2024-11-3", 1730617200000);

    validateDate("1970-01-01", 0);
    validateDate("1970-01-1", 28800000);
    validateDate("1970-1-01", 28800000);
    validateDate("1970-1-1", 28800000);
    validateDate("1970-01-1T", NaN);
    validateDate("1970-01-01T", NaN);

    validateDate("1970-01-01T00:00:00", 28800000);
    validateDate("1970-01-01T00:00:00Z", 0);
    validateDate("1970-1-01T00:00:00", NaN);
    validateDate("1970-01-1T00:00:00", NaN);
    validateDate("1970-1-1T00:00:00", NaN);
}
