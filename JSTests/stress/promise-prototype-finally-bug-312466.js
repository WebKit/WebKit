function shouldThrow(caseName, fn, expectedErrorCtor, expectedErrorMessage) {
    if (!caseName)
        throw new Error(`must specify test case name`);

    const expected = `${expectedErrorCtor.name}(${expectedErrorMessage})`;
    try {
        fn();
        throw new Error(`${caseName}: Expected to throw ${expected}, but succeeded`);
    } catch (e) {
        const actual = `${e.name}(${e.message})`;
        if (!(e instanceof expectedErrorCtor) || e.message !== expectedErrorMessage)
            throw new Error(`${caseName}: Expected ${expected} but got ${actual}`);
    }
}

/**
 *  By ECMA-262 (April 10, 2026),
 *  _27.2.5.3 Promise.prototype.finally_ checks and raises the error
 *  if the |this|'s species constructor is not a constructor in its step 3 which invokes _7.3.22 SpeciesConstructor_.
 *
 *  - https://tc39.es/ecma262/#sec-promise.prototype.finally
 *  - https://tc39.es/ecma262/#sec-speciesconstructor
 */

{
    const TEST_CASE = [
        [1],
        [0],
        [1n],
        [true],
        [false],
        [''],
        [null],
        [Symbol.iterator],
    ];
    for (const [property, label] of TEST_CASE) {
        const mimic = {
            constructor: property,
        };

        const name = label ?? String(property);
        shouldThrow(`mimic this' constructor does not have [[Construct]]: ${name}`, () => {
            Promise.prototype.finally.call(mimic, undefined);
        }, TypeError, '|this|.constructor is not an Object or undefined');
    }
}

const SYMBOL_SPECIES_TEST_CASE = [
    [() => { return Promise }, 'arrow-func'],
    [1],
    [0],
    [1n],
    [true],
    [false],
    [''],
    [Symbol.iterator],
];

{
    for (const [property, label] of SYMBOL_SPECIES_TEST_CASE) {
        const mimic = {
            constructor: {
                [Symbol.species]: property,
            },
        };

        const name = label ?? String(property);
        shouldThrow(`mimic this' constructor[Symbol.species] is: ${name}`, () => {
            Promise.prototype.finally.call(mimic, undefined);
        }, TypeError, '|this|.constructor[Symbol.species] is not a constructor');
    }
}

{
    for (const [property, label] of SYMBOL_SPECIES_TEST_CASE) {
        class Derived extends Promise {
            constructor(...args) {
                super(...args);
            }
            static [Symbol.species] = property;
        }
        const mimic = new Derived((r) => r(1));

        const name = label ?? String(property);
        shouldThrow(`|this| is derived Promise instance but it's [Symbol.species] is weird: ${name}`, () => {
            Promise.prototype.finally.call(mimic, undefined);
        }, TypeError, '|this|.constructor[Symbol.species] is not a constructor');
    }
}
