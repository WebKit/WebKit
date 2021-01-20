//@ runDefault("--useConcurrentJIT=0")

function opt(arr, start, end) {
    parseInt();
    for (var i = start; i < end; i++) {
        if (i === 10) {
            end |= 0;
        }
        arr[i] = 2.3023e-320;
    }
}

function main() {
    let arr = new Array(1000);
    arr.fill(1.1);

    for (let i = 0; i < 10000; i++) {
        opt(arr, 0, 1000);
    }

    opt(arr, 0, 100000);
    opt(arr, 0, 0x80000001);
}

main();
main();
