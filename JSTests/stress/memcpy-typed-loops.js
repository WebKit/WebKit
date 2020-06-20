function noLoop(arr1, arr2) {
    let i=0
    if (arr1.size%2==0)
        i = 0
    else i = 0
    arr2[i] = arr1[i]
    return arr2
}
noInline(noLoop);

function invalidStart(arr1, arr2) {
    if (arr1.length != arr2.length)
        return []
    let i = 0
    do {
        ++i
        arr2[i] = arr1[i]
    } while (i < arr1.length-1)
    return arr2
}
noInline(invalidStart);

const size = 10
let arr1 = new Int32Array(size)
let arr2 = new Int32Array(size)
for (let i=0; i<arr1.length; ++i) {
    arr1[i] = i
}

const iterationCount = $vm.useJIT() ? 10000000 : 100000;
for (let i=0; i<iterationCount; ++i) noLoop(arr1, arr2)
for (let i=0; i<iterationCount; ++i) invalidStart(arr1, arr2)

arr2 = new Int32Array(arr1.length)
invalidStart(arr1, arr2)

for (let i=1; i<arr1.length; ++i) {
    if (arr2[i] != arr1[i] || arr2[0] != 0)
        throw "Error: bad copy: " + i + " " + arr1[i] + " " + arr2[i]
}
