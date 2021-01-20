function doIndexOf(a) {
    a.indexOf(a);
}

function bar(f) {
    f();
}

let array = [20];
for (let i = 0; i < 100000; ++i) {
    bar(() => {
        return doIndexOf(array.concat());
    });
}
