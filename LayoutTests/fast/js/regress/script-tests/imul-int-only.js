function f(a,b) {
    var result = 0;
    for (var i = 0; i < 10000; i++)
        result = (result + Math.imul(a,b)) | 0
    return result;
}
var result = 0;
for (var i = 0; i < 10000; i++)
    result = result ^ f(i + 1, i)

if (result != 1656994304) 
    throw "Bad result: " + result;
