function opt() {
    const arr = [0,0,0,0,0,0,0];
    function f() {
        arr[0];
        (0)[0];
        return arr;
    }
    const ret = arr.map(f)
    ret[0] = 1
}
for (let i = 0; i < 200; i++) {
    opt()
}
