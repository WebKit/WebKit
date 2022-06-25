let x = 123

function test(fn, expected) {
    for (let i = 0; i < 10000; i++) {
        const arr = new Uint8ClampedArray(1)
        fn(arr)
        x = arr[0]
    } 
    
    if (x != expected)
        throw new Error(`Clamping was done incorrectly ${x}, expected ${expected} in ${fn}`)
}
noInline(test)

function int32pos1(arr) {
    arr[0] = 0x70000001|0
}
noInline(int32pos1)
test(int32pos1, 255)

function int32pos2(arr) {
    arr[0] = 0x800000001|0
}
noInline(int32pos2)
test(int32pos2, 1)

function int32neg1(arr) {
    arr[0] = 0x80000001|0
}
noInline(int32neg1)
test(int32neg1, 0)

function int32neg2(arr) {
    arr[0] = 0x80000000|0
}
noInline(int32neg2)
test(int32neg2, 0)

function int52pos1(arr) {
    arr[0] = 0x80000001
}
noInline(int52pos1)
test(int52pos1, 255)

function int52pos2(arr) {
    arr[0] = 0x80000000
}
noInline(int52pos2)
test(int52pos2, 255)

function int52neg1(arr) {
    arr[0] = -0x80000001
}
noInline(int52neg1)
test(int52neg1, 0)

function int52neg2(arr) {
    arr[0] = -0x80000000
}
noInline(int52neg2)
test(int52neg2, 0)

function int52neg3(arr) {
    arr[0] = -0x0008_0000_0000_0000
}
noInline(int52neg3)
test(int52neg3, 0)

function int52pos3(arr) {
    arr[0] = 0x0007_FFFF_FFFF_FFFF
}
noInline(int52pos3)
test(int52pos3, 255)

function int8(arr) {
    arr[0] = 0x8000000fe - 0x800000000
}
noInline(int8)
test(int8, 254)