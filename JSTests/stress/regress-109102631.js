//@ skip if $memoryLimited
function main() {
    const buffer = new ArrayBuffer(4294967296);

    const arr = new Uint8ClampedArray(buffer, 50)
    const arr2 = new Uint8ClampedArray(buffer, 4294967296)

    function opt(a, marr) {
        return marr[a.byteOffset]
    }

    const marr = []
    for (let i = 0; i < 1000; i++) {
        marr.push(3)
    }

    for (let i = 0; i < 14; i++) {
        opt(arr, marr)
    }

    if (typeof(opt(arr2, marr)) !== "undefined")
        throw "FAILED";
}
noDFG(main);
noFTL(main);
main();
