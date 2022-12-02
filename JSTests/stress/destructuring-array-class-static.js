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
        let b = 0;
        class x {
            static #y = [ b = 0, x = 0 ] = [0];
        }
    `);
}, expected);

shouldThrow(2, () => {
    eval(`
        class x {
            static #y = [ x = 0 ] = [0];
        }
    `);
}, expected);

shouldThrow(3, () => {
    eval(`
        let b = 0;
        class x {
            y = [ b = 1, x = 0 ] = [0];
        }
        let a = new x();
    `);
}, expected);

shouldThrow(4, () => {
    eval(`
        class x {
            y = [ x = 0 ] = [0];
        }
        let a = new x();
    `);
}, expected);

shouldThrow(5, () => {
    eval(`
        let b = 0;
        class x {
            static {
                let a = [ b = 1, x = 0 ] = [0];
            }
        }
    `);
}, expected);

shouldThrow(6, () => {
    eval(`
        class x {
            static {
                let a = [ x = 0 ] = [0];
            }
        }
    `);
}, expected);

shouldThrow(7, () => {
    eval(`
        let b = 0;
        class x {
            static #y = [ b, x ] = [0];
        }
    `);
}, expected);

shouldThrow(8, () => {
    eval(`
        class x {
            static #y = [ x ] = [0];
        }
    `);
}, expected);

shouldThrow(9, () => {
    eval(`
        let b = 0;
        class x {
            y = [ b, x ] = [0];
        }
        let a = new x();
    `);
}, expected);

shouldThrow(10, () => {
    eval(`
        class x {
            y = [ x ] = [0];
        }
        let a = new x();
    `);
}, expected);

shouldThrow(11, () => {
    eval(`
        let b = 0;
        class x {
            static {
                let a = [ b, x ] = [0];
            }
        }
    `);
}, expected);

shouldThrow(12, () => {
    eval(`
        class x {
            static {
                let a = [ x ] = [0];
            }
        }
    `);
}, expected);
