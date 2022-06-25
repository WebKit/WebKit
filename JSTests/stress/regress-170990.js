function getter(arr, operand, resultArr) { 
     resultArr[0] = arr[operand];
}

function test(resultArr, arr, getter) {
    getter(arr, 0, resultArr);
    getter(arr, 1, resultArr);
    resultArr[0] == arr[1];
}
noInline(run);

var arr = new Uint32Array([0x80000000,1]); 
var resultArr = [];
for (var i = 0; i < 10000; i++)
    test(resultArr, arr, getter);
