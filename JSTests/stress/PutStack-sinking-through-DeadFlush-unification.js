function test(a, flag, flag2, bailout) {
    a = 3333
    if (flag) {
        if (flag2) {
            a = 1111;
            Object.toString();
        } else {
            a = 2222;
            Object.toString();
        }
        Object.toString();
    }
    Object.toString();
    bailout ++;
    return a;
}

function main() {
    for(let i = 0; i < 1000; i ++) {
        test(0, true, true, 0);
        test(0, true, false, 0);
        test(0, false, true, 0);
        test(0, false, false, 0);
    }
    
    let result = test(0, true, true, 0x7fffffff);
    if (result !== 1111)
        throw 'Expected test() to return 1111, got ' + result;
}
main()
