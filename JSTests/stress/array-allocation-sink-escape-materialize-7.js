function innerFunction(arr) {
    let len = arr.length;
    if (len > 2) {
        return arr;
    }
    return len * 2;
}

function outerFunction(mode) {
    let a = new Array(3);
    a[0] = mode;
    a[1] = mode + 1;

    let initialLen = a.length;

    if (mode % 5 === 0) {
        let result = innerFunction(a);
        return [result, initialLen];
    } else {
        return a[0] + a[1] + initialLen;
    }
}
noInline(innerFunction);
noInline(outerFunction);

for (let i = 0; i < testLoopCount; i++) {
    outerFunction(i);
}