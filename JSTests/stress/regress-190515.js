function set(arr, value) {
    arr[0] = value;
}

function getImmutableArrayOrSet(get) {
    let arr = [1];
    if (get)
        return arr;

    set(arr, 42);
    set({}, 1);
}
noInline(getImmutableArrayOrSet);

function test() {
    getImmutableArrayOrSet(true);

    for (let i = 0; i < 10000; i++)
        getImmutableArrayOrSet(false);

    let arr = getImmutableArrayOrSet(true);
    if (arr[0] != 1)
        throw "FAILED";
}

test();
