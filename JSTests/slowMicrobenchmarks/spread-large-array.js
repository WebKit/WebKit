function foo(arg) {
    return [...arg];
}
noInline(foo);

let arrays = [ ];
const size = 500;
{
    let arr = [];
    for (let i = 0; i < size; i++) {
        arr.push(i);
    }
    arrays.push(arr);
}

{
    let arr = [];
    for (let i = 0; i < size; i++) {
        arr.push(i + 0.5);
    }
    arrays.push(arr);
}

{
    let arr = [];
    for (let i = 0; i < size; i++) {
        arr.push({i: i});
    }
    arrays.push(arr);
}

let start = Date.now();
for (let i = 0; i < 100000; i++) {
    let array = arrays[i % arrays.length];
    foo(array);
}
const verbose = false;
if (verbose)
    print(Date.now() - start);
