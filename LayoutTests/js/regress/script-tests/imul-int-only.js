function f(a,b) {
    var result = 0;
    for (var i = 0; i < 1000; i++)
        result = (result + Math.imul(a,b)) | 0
    return result;
}
var result = 0;
for (var i = 0; i < 5000; i++)
    result = result ^ f(i + 1, i)

if (result != -1676196992) 
    throw "Bad result: " + result;
