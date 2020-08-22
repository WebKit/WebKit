function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error("not thrown");
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

let options = {
    "weekday": "narrow",
    "era": "narrow",
    "year": "2-digit",
    "month": "2-digit",
    "day": "2-digit",
    "hour": "2-digit",
    "minute": "2-digit",
    "second": "2-digit",
    "timeZoneName": "short",
};

for (let [key, value] of Object.entries(options)) {
    shouldThrow(() => {
        new Intl.DateTimeFormat("en", {
            dateStyle: "full",
            [key]: value
        });
    }, `TypeError: dateStyle and timeStyle may not be used with other DateTimeFormat options`);
    shouldThrow(() => {
        new Intl.DateTimeFormat("en", {
            timeStyle: "full",
            [key]: value
        });
    }, `TypeError: dateStyle and timeStyle may not be used with other DateTimeFormat options`);
    shouldThrow(() => {
        new Intl.DateTimeFormat("en", {
            dateStyle: "full",
            timeStyle: "full",
            [key]: value
        });
    }, `TypeError: dateStyle and timeStyle may not be used with other DateTimeFormat options`);
}
