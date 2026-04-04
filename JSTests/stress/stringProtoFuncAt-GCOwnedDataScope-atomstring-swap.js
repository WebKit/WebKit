const target = "A".repeat(128);
const dummy = {};
Reflect.set(dummy, target, 1);

const freshRope = "A".repeat(128);

let nonAtom = "D".repeat(64) + "D".repeat(64);
String.prototype.at.call(nonAtom, 0);

let thatObj = {
    [Symbol.toPrimitive]() {
        // Trigger atomization via Reflect.set to avoid Inline Cache holding the string
        Reflect.set(dummy, freshRope, 1);

        // Overwrite the VM's lastAtomizedIdentifierStringImpl cache
        Reflect.set(dummy, nonAtom, 1);

        gc();

        return 0;
    }
};

// Verify that GCOwnedDataScope/Heap keeps the StringImpl alive across the callback
String.prototype.at.call(freshRope, thatObj);
