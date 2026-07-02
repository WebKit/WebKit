
function multipleGetButterfly(mode) {
    let a = new Array(4);
    a[0] = 10;
    a[1] = 20;
    a[2] = 30;
    a[3] = 40;

    let len1 = a.length;
    let len2 = a.length;
    let len3 = a.length;

    switch (mode % 4) {
        case 0:
            return a[0] + len1;
        case 1:
            return len1 + len2 + len3;
        case 2:
            let result = a;
            return [result, len1, len2, len3];
        case 3:
            if (len1 > 3) {
                return a;
            }
            return len2 + len3;
    }
}
noInline(multipleGetButterfly);

for (let i = 0; i < testLoopCount; i++) {
    multipleGetButterfly(i);
}