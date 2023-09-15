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
            `1 yr, 2 mths, 3 wks, 4 days, 10.34.33`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en-DK-u-ca-buddhist', {
            style: 'digital'
        });
        shouldBeOneOf(fmt.format({ years: 1, months: 2, weeks: 3, days: 4, hours: 10, minutes: 34, seconds: 33, milliseconds: 32 }), [
            `1 yr, 2 mths, 3 wks, 4 days, 10.34.33`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en-DK-u-nu-hanidec', {
            style: 'digital'
        });
        shouldBeOneOf(fmt.format({ years: 1, months: 2, weeks: 3, days: 4, hours: 10, minutes: 34, seconds: 33, milliseconds: 32 }), [
            `一 yr, 二 mths, 三 wks, 四 days, 一〇:三四:三三`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en', {
            style: 'digital'
        });
        shouldBeOneOf(fmt.format({ years: 1, months: 2, weeks: 3, days: 4, hours: 10, minutes: 34, seconds: 33, milliseconds: 32 }), [
            `1 yr, 2 mths, 3 wks, 4 days, 10:34:33`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en', {
            style: 'digital'
        });
        shouldBeOneOf(fmt.format({ years: 1, months: 2, weeks: 3, days: 4, hours: 10, minutes: 34, seconds: 33, milliseconds: 32 }), [
            `1 yr, 2 mths, 3 wks, 4 days, 10:34:33`,
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
            `1 yr, 2 mths, 3 wks, 4 days, 10:34:33.032000000`,
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
            `1 yr, 2 mths, 3 wks, 4 days, 10:34:33.03`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en', {
            style: 'digital',
            milliseconds: 'numeric',
            fractionalDigits: 9
        });

        shouldBeOneOf(fmt.format({ hours: 10, seconds: 33, milliseconds: 32 }), [
            `10:00:33.032000000`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en', {
            style: 'digital',
            milliseconds: 'numeric',
            fractionalDigits: 9
        });

        shouldBeOneOf(fmt.format({ minutes: 10, seconds: 33, milliseconds: 32 }), [
            `0:10:33.032000000`,
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
    {
        var fmt = new Intl.DurationFormat('en', {
            style: 'digital',
        });

        shouldBeOneOf(fmt.format({ hours: 0, minutes: 10}), [
            `0:10:00`,
        ]);
        shouldBeOneOf(fmt.format({ hours: 5, minutes: 6}), [
            `5:06:00`,
        ]);
        shouldBeOneOf(fmt.format({ minutes: 5, seconds:6}), [
            `0:05:06`,
        ]);
    }
}
