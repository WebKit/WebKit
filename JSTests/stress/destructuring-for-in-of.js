function shouldThrow(test, func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error(`test ${test} not thrown`);
    if (String(error) !== errorMessage)
        throw new Error(`test ${test} bad error: ${String(error)}`);
}

let expected = "TypeError: Attempted to assign to readonly property.";

shouldThrow(1, () => {
    eval(`
        const x = 0;
        for ({ x } in [x]);
    `);
}, expected);

shouldThrow(1, () => {
    eval(`
        const x = 0;
        for ({ a, x } in [x]);
    `);
}, expected);

shouldThrow(1, () => {
    eval(`
        const x = 0;
        for ({ x } of [x]);
    `);
}, expected);

shouldThrow(1, () => {
    eval(`
        const x = 0;
        for ({ a, x } of [x]);
    `);
}, expected);
