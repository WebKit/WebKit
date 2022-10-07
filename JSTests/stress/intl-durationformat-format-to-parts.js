//@ requireOptions("--useIntlDurationFormat=1")

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
        })), `[{"type":"years","value":"1 yr"},{"type":"literal","value":", "},{"type":"hours","value":"22 hr"},{"type":"literal","value":", "},{"type":"minutes","value":"30 min"},{"type":"literal","value":", "},{"type":"seconds","value":"34 sec"}]`);
    }
    {
        var fmt = new Intl.DurationFormat('en', {
            style: 'digital',
            milliseconds: 'numeric',
            millisecondsDisplay: 'always',
            fractionalDigits: 2
        });

        shouldBeOneOf(JSON.stringify(fmt.formatToParts({ years: 1, months: 2, weeks: 3, days: 4, hours: 10, minutes: 34, seconds: 33, milliseconds: 32 })), [
            `[{"type":"years","value":"1y"},{"type":"literal","value":", "},{"type":"months","value":"2mo"},{"type":"literal","value":", "},{"type":"weeks","value":"3w"},{"type":"literal","value":", "},{"type":"days","value":"4d"},{"type":"literal","value":", "},{"type":"hours","value":"10"},{"type":"literal","value":":"},{"type":"minutes","value":"34"},{"type":"literal","value":":"},{"type":"seconds","value":"33.03"}]`,
            `[{"type":"years","value":"1y"},{"type":"literal","value":", "},{"type":"months","value":"2m"},{"type":"literal","value":", "},{"type":"weeks","value":"3w"},{"type":"literal","value":", "},{"type":"days","value":"4d"},{"type":"literal","value":", "},{"type":"hours","value":"10"},{"type":"literal","value":":"},{"type":"minutes","value":"34"},{"type":"literal","value":":"},{"type":"seconds","value":"33.03"}]`,
        ]);
    }
}
