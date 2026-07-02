//@ memoryHog!
//@ skip if $addressBits <= 32
//@ runDefault

try {
    let iterations = 0;
    const array = [-1, undefined, undefined];
    array[Symbol.iterator] = function* () {
        for (const _ of Array.prototype[Symbol.iterator].call(this)) {
            if (++iterations > 28)
                return;
            this.push(Array.prototype.flat.call(array, 1 << 30));
        }
    };
    [...array];
} catch (e) { }
