load("./resources/iro-test-helpers.js", "caller relative");
function fn(arr, arg) {
    let sum = 0|0;
    for (let i = 0; i < arr.length; i = (i + 1)|0) {
        if (arg === 1337 && i === -1)
            i = -1;
        sum = ((sum|0) + (arr[i]|0))|0;
    }
    return sum;
}
noInline(fn);
const arr = new Int32Array(8);
for (let i = 0; i < arr.length; ++i) arr[i] = i + 1;
for (let i = 0; i < testLoopCount; ++i) fn(arr, 1337);
const iro = makeIROHelper(fn);
print("CheckInBounds:", iro.opCount("CheckInBounds"));
print("GetUndetachedTypeArrayLength:", iro.opCount("GetUndetachedTypeArrayLength"));
