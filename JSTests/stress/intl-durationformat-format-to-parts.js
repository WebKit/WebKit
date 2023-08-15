//@ requireOptions("--useIntlDurationFormat=1")
//@ skip if $hostOS != "darwin" # We are testing Intl features based on Darwin's ICU. The other port owners can extend it by testing it in their platforms and removing this condition.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldBeOneOf(actual, expectedArray) {
    if (!expectedArray.some((value) => value === actual))
        throw new Error('bad value: ' + actual + ' expected values: ' + expectedArray);
}

const icuVersion = $vm.icuVersion();
function shouldBeForICUVersion(minimumVersion, actual, expected) {
    if (icuVersion < minimumVersion)
        return;

    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldNotThrow(func) {
    func();
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
}

if (Intl.DurationFormat) {
    {
        var fmt = new Intl.DurationFormat('en');
        shouldBe(JSON.stringify(fmt.formatToParts({
            years: 1,
            hours: 22,
            minutes: 30,
            seconds: 34,
        })), `[{"type":"integer","value":"1","unit":"year"},{"type":"literal","value":" ","unit":"year"},{"type":"unit","value":"yr","unit":"year"},{"type":"literal","value":", "},{"type":"integer","value":"22","unit":"hour"},{"type":"literal","value":" ","unit":"hour"},{"type":"unit","value":"hr","unit":"hour"},{"type":"literal","value":", "},{"type":"integer","value":"30","unit":"minute"},{"type":"literal","value":" ","unit":"minute"},{"type":"unit","value":"min","unit":"minute"},{"type":"literal","value":", "},{"type":"integer","value":"34","unit":"second"},{"type":"literal","value":" ","unit":"second"},{"type":"unit","value":"sec","unit":"second"}]`);
    }
    {
        var fmt = new Intl.DurationFormat('en', {
            style: 'digital',
            milliseconds: 'numeric',
            millisecondsDisplay: 'always',
            fractionalDigits: 2
        });

        shouldBeOneOf(JSON.stringify(fmt.formatToParts({ years: 1, months: 2, weeks: 3, days: 4, hours: 10, minutes: 34, seconds: 33, milliseconds: 32 })), [
            `[{"type":"integer","value":"1","unit":"year"},{"type":"literal","value":" ","unit":"year"},{"type":"unit","value":"yr","unit":"year"},{"type":"literal","value":", "},{"type":"integer","value":"2","unit":"month"},{"type":"literal","value":" ","unit":"month"},{"type":"unit","value":"mths","unit":"month"},{"type":"literal","value":", "},{"type":"integer","value":"3","unit":"week"},{"type":"literal","value":" ","unit":"week"},{"type":"unit","value":"wks","unit":"week"},{"type":"literal","value":", "},{"type":"integer","value":"4","unit":"day"},{"type":"literal","value":" ","unit":"day"},{"type":"unit","value":"days","unit":"day"},{"type":"literal","value":", "},{"type":"integer","value":"10","unit":"hour"},{"type":"literal","value":":"},{"type":"integer","value":"34","unit":"minute"},{"type":"literal","value":":"},{"type":"integer","value":"33","unit":"second"},{"type":"decimal","value":".","unit":"second"},{"type":"fraction","value":"03","unit":"second"}]`,
        ]);
    }
    {
        var fmt = new Intl.DurationFormat('en-US', { style: 'digital', fractionalDigits: 9, millseconds: 'numeric' });
        shouldBeOneOf(JSON.stringify(fmt.formatToParts({ hours: 7, minutes: 8, seconds: 9, milliseconds: 123, microseconds: 456, nanoseconds: 789 })), [
            `[{"type":"integer","value":"7","unit":"hour"},{"type":"literal","value":":"},{"type":"integer","value":"08","unit":"minute"},{"type":"literal","value":":"},{"type":"integer","value":"09","unit":"second"},{"type":"decimal","value":".","unit":"second"},{"type":"fraction","value":"123456789","unit":"second"}]`,
        ]);
    }
}
