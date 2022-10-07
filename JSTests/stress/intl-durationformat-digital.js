//@ requireOptions("--useIntlDurationFormat=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeOneOf(actual, expectedArray) {
    if (!expectedArray.some((value) => value === actual))
        throw new Error('bad value: ' + actual + ' expected values: ' + expectedArray);
}

if (Intl.DurationFormat) {
    {
        var fmt = new Intl.DurationFormat('en-DK', {
            style: 'digital'
        });
        shouldBeOneOf(fmt.format({ years: 1, months: 2, weeks: 3, days: 4, hours: 10, minutes: 34, seconds: 33, milliseconds: 32 }), [
            `1y, 2mo, 3w, 4d, 10.34.33`,
            `1y, 2m, 3w, 4d, 10.34.33`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en-DK-u-ca-buddhist', {
            style: 'digital'
        });
        shouldBeOneOf(fmt.format({ years: 1, months: 2, weeks: 3, days: 4, hours: 10, minutes: 34, seconds: 33, milliseconds: 32 }), [
            `1y, 2mo, 3w, 4d, 10.34.33`,
            `1y, 2m, 3w, 4d, 10.34.33`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en-DK-u-nu-hanidec', {
            style: 'digital'
        });
        shouldBeOneOf(fmt.format({ years: 1, months: 2, weeks: 3, days: 4, hours: 10, minutes: 34, seconds: 33, milliseconds: 32 }), [
            `一y, 二mo, 三w, 四d, 一〇:三四:三三`,
            `一y, 二m, 三w, 四d, 一〇:三四:三三`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en', {
            style: 'digital'
        });
        shouldBeOneOf(fmt.format({ years: 1, months: 2, weeks: 3, days: 4, hours: 10, minutes: 34, seconds: 33, milliseconds: 32 }), [
            `1y, 2mo, 3w, 4d, 10:34:33`,
            `1y, 2m, 3w, 4d, 10:34:33`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en', {
            style: 'digital'
        });
        shouldBeOneOf(fmt.format({ years: 1, months: 2, weeks: 3, days: 4, hours: 10, minutes: 34, seconds: 33, milliseconds: 32 }), [
            `1y, 2mo, 3w, 4d, 10:34:33`,
            `1y, 2m, 3w, 4d, 10:34:33`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en', {
            style: 'digital',
            milliseconds: 'numeric',
            millisecondsDisplay: 'always',
            fractionalDigits: 9
        });

        shouldBeOneOf(fmt.format({ years: 1, months: 2, weeks: 3, days: 4, hours: 10, minutes: 34, seconds: 33, milliseconds: 32 }), [
            `1y, 2mo, 3w, 4d, 10:34:33.032000000`,
            `1y, 2m, 3w, 4d, 10:34:33.032000000`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en', {
            style: 'digital',
            milliseconds: 'numeric',
            millisecondsDisplay: 'always',
            fractionalDigits: 2
        });

        shouldBeOneOf(fmt.format({ years: 1, months: 2, weeks: 3, days: 4, hours: 10, minutes: 34, seconds: 33, milliseconds: 32 }), [
            `1y, 2mo, 3w, 4d, 10:34:33.03`,
            `1y, 2m, 3w, 4d, 10:34:33.03`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en', {
            style: 'digital',
            milliseconds: 'numeric',
            fractionalDigits: 9
        });

        shouldBeOneOf(fmt.format({ hours: 10, seconds: 33, milliseconds: 32 }), [
            `10, 33.032000000`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en', {
            style: 'digital',
            milliseconds: 'numeric',
            fractionalDigits: 9
        });

        shouldBeOneOf(fmt.format({ minutes: 10, seconds: 33, milliseconds: 32 }), [
            `10:33.032000000`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en', {
            style: 'digital',
            milliseconds: 'numeric',
            fractionalDigits: 9
        });

        shouldBeOneOf(fmt.format({ hours: 10, minutes: 10, milliseconds: 32, microseconds: 44, nanoseconds: 55 }), [
            `10:10:00.032044055`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en', {
            style: 'digital',
        });

        shouldBeOneOf(fmt.format({ hours: 10, minutes: 10, milliseconds: 32}), [
            `10:10:00`,
        ]);
    }
}
