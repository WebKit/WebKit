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

shouldThrow(() => {
    let calendarValue = 'buddhist';
    calendarValue = calendarValue.toLocaleString().padEnd(calendarValue.length + 510 * 5, -169);
    var loc = new Intl.Locale('ko', {
        calendar: calendarValue,
    });
}, RangeError);

shouldThrow(() => {
    let collationValue = 'zhuyin';
    collationValue = collationValue.toLocaleString().padEnd(collationValue.length + 510 * 5, -169);
    var loc = new Intl.Locale('ko', {
        collation: collationValue,
    });
}, RangeError);

shouldThrow(() => {
    let numberingSystemValue = 'latn';
    numberingSystemValue = numberingSystemValue.toLocaleString().padEnd(numberingSystemValue.length + 510 * 5, -169);
    var loc = new Intl.Locale('ko', {
        numberingSystem: numberingSystemValue,
    });
}, RangeError);
