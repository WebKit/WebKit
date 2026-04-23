//@ requireOptions('--useConcurrentJIT=true')
//@ skip if !$jitTests || $architecture != "arm64e" || $platform == "tvos" || $platform == "watchos"

// Test that we can stall a compiler thread and get its stack bounds.
// This is useful for testing memory access to compiler thread stacks.

function dummyFunction() {
    // This function exists just to provide a CodeBlock for the stall plan.
    let x = 0;
    for (let i = 0; i < 100; i++) {
        x += i;
    }
    return x;
}

function doubleToUint(d) {
    let buffer = new ArrayBuffer(8);
    let uintView = new BigUint64Array(buffer);
    return uintView[0];
}

// Call it once to ensure it gets a baseline CodeBlock
dummyFunction();

let handle = $vm.stallCompilerThread(dummyFunction);

if (typeof handle.stackBase !== 'number')
    throw new Error("stackBase should be a number");

if (typeof handle.stackBound !== 'number')
    throw new Error("stackBound should be a number");

// The values are double-encoded pointers. To get the actual address,
// we will need to reinterpret the double bits as a 64-bit integer.
let base = handle.stackBase;
let bound = handle.stackBound;

if (base === 0 || bound === 0)
    throw new Error("Neither stack bound should be zero");

if (base === bound)
    throw new Error("Stack base and bound should be different");

if (base < bound)
    throw new Error("Stack base should be a higher address than its bound");


// Helper to convert double-encoded pointer to hex string for display
function doubleToHex(d) {
    let buffer = new ArrayBuffer(8);
    let floatView = new Float64Array(buffer);
    let uintView = new BigUint64Array(buffer);
    floatView[0] = d;
    return "0x" + uintView[0].toString(16);
}

print("Compiler thread stack base: " + doubleToHex(base));
print("Compiler thread stack bound: " + doubleToHex(bound));

// Release the stalled thread
$vm.releaseStallHandle(handle);

print("PASSED")
