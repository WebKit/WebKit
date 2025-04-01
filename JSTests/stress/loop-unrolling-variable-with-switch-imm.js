function test(x) {
    let sum = 0;
    for (let i = 0; i < x; i++) {
        switch (i) {
            case 0:
                sum += 1;
                break;
            case 1:
                sum += 2;
                break;
            case 2:
                sum += 3;
                break;
            default:
                sum += 10;
                break;
        }
    }
    return sum;
}
noInline(test);

let expected = 0;
for (let i = 0; i < 1e5; i++) {
    let res = test(4);
    if (i == 0)
        expected = res;
    if (expected != res)
        throw new Error("bad");
}
