function opt(date, float64_array) {
    date.setTime(float64_array[0]);
}

function main() {
    const uint64_array = new BigUint64Array(1);
    const float64_array = new Float64Array(uint64_array.buffer);

    const date = new Date();

    for (let i = 0; i < 1000000; i++) {
        opt(date, float64_array);
    }

    uint64_array[0] = 0xfffe000000001234n;
    opt(date, float64_array);

    print(date.getTime());
}

main();
