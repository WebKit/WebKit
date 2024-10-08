
function assert(a, b) {
    if (a === b)
        return;
    if (a === null || b === null || a.length != b.length)
        throw new Error("bad, a=" + a + " b=" + b);
    for (let i = 0; i < b.length; i++) {
        if (a[i] != b[i])
            throw new Error("bad, a=" + a + " b=" + b);
    }
}

let str = "aaaaaaaaaaaaaaa";
let verbose = false;

function test(str, regexp, atomLen, unit) {
    function expectedResult(start, end) {
        let totalLen = end - start;
        if (totalLen < atomLen)
            return null;
        let list = [];
        while (totalLen >= atomLen) {
            list.push(unit);
            totalLen -= atomLen;
        }
        return list;
    }    

    for (let start1 = 0; start1 < str.length; start1++) {
        for (let end1 = start1 + 1; end1 <= str.length; end1++) {
            for (let start2 = 0; start2 < str.length; start2++) {
                for (let end2 = start2 + 1; end2 <= str.length; end2++) {
                    if (verbose)
                        print(start1, end1);
                    let m1 = str.substring(start1, end1).match(regexp);
                    if (verbose)
                        print(start2, end2);
                    let m2 = str.substring(start2, end2).match(regexp);
                    let e1 = expectedResult(start1, end1);
                    let e2 = expectedResult(start2, end2);
                    if (verbose)
                        print(m1, e1, m2, e2);

                    assert(m1, e1);
                    assert(m2, e2);
                }
            }
        }
    }
}
noInline(test);

for (let i = 0; i < 50; i++) {
    test(str, /aaaaa/g, 5, "aaaaa");
    test(str, /a/g, 1, "a");
}


