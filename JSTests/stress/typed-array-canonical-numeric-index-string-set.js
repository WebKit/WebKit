'use strict';
const TypedArrays = [Uint8ClampedArray, Uint8Array, Uint16Array, Uint32Array, Int8Array, Int16Array, Int32Array, Float32Array, Float64Array];
const canonicalNumericIndexStrings = ['-0', '-1', '-3.14', 'Infinity', '-Infinity', 'NaN', '0.5', '4294967295', '4294967296'];

function assert(x, key) {
    if (!x)
        throw new Error(`Bad assertion! Key: '${key}'`);
}

for (const TypedArray of TypedArrays) {
    for (const key of canonicalNumericIndexStrings) {
        const assertPrototypePropertyIsUnreachable = () => {
            {
                const tA = new TypedArray;
                tA[key] = 1;
                assert(!tA.hasOwnProperty(key), key);
            }

            {
                const tA = new TypedArray;
                const heir = Object.create(tA);
                heir[key] = 2;

                assert(!tA.hasOwnProperty(key), key);
                assert(!heir.hasOwnProperty(key), key);
            }

            {
                const target = {};
                const receiver = new TypedArray;

                assert(!Reflect.set(target, key, 3, receiver), key);
                assert(!target.hasOwnProperty(key), key);
                assert(!receiver.hasOwnProperty(key), key);
            }

            {
                const tA = new TypedArray;
                const target = Object.create(tA);
                const receiver = {};

                assert(Reflect.set(target, key, 4, receiver), key);
                assert(!tA.hasOwnProperty(key), key);
                assert(!target.hasOwnProperty(key), key);
                assert(!receiver.hasOwnProperty(key), key);
            }
        };

        Object.defineProperty(TypedArray.prototype, key, {
            set() { throw new Error(`${TypedArray.name}.prototype['${key}'] setter should be unreachable!`); },
            configurable: true,
        });
        assertPrototypePropertyIsUnreachable();

        Object.defineProperty(TypedArray.prototype, key, { writable: false });
        assertPrototypePropertyIsUnreachable();
    }
}
