function test(x) {
    let sum = 0;
    for (let i = 0; i < x.length; i++) {
        switch (x[i]) {
            case 'a':
                sum += 1;
                break;
            case 'b':
                sum += 2;
                break;
            case 'c':
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
    let res = test(['a', 'b', 'c', 'd']);
    if (i == 0)
        expected = res;
    if (expected != res)
        throw new Error("bad");
}
