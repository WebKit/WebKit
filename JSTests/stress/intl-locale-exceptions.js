function runTest(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if ($vm.icuVersion() >= 76) {
        if (error)
            throw new Error(`Expected no exception but got: ${error}`);
    } else {
        if (!(error instanceof errorType))
            throw new Error(`Expected ${errorType.name}!`);
    }
}

runTest(() => {
    let calendarValue = 'buddhist';
    calendarValue = calendarValue.toLocaleString().padEnd(calendarValue.length + 510 * 4, -169);
    var loc = new Intl.Locale('ko', {
        calendar: calendarValue,
    });
}, RangeError);

runTest(() => {
    let collationValue = 'zhuyin';
    collationValue = collationValue.toLocaleString().padEnd(collationValue.length + 510 * 4, -169);
    var loc = new Intl.Locale('ko', {
        collation: collationValue,
    });
}, RangeError);

runTest(() => {
    let numberingSystemValue = 'latn';
    numberingSystemValue = numberingSystemValue.toLocaleString().padEnd(numberingSystemValue.length + 510 * 4, -169);
    var loc = new Intl.Locale('ko', {
        numberingSystem: numberingSystemValue,
    });
}, RangeError);
