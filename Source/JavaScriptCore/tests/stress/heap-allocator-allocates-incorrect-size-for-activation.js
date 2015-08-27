
// Consider the following scenario:
// - On OS X, WTF::pageSize() is 4*1024 bytes.
// - JSEnvironmentRecord::allocationSizeForScopeSize(6621) == 53000
// - sizeof(MarkedBlock) == 248
// - (248 + 53000) is a multiple of 4*1024.
// - (248 + 53000)/(4*1024) == 13

// We will allocate a chunk of memory of size 53248 bytes that looks like this:
// 0            248       256                       53248       53256
// [Marked Block | 8 bytes |  payload     ......      ]  8 bytes  |
//                         ^                                      ^
//                    Our Environment record starts here.         ^
//                                                                ^
//                                                        Our last JSValue in the environment record will go from byte 53248 to 53256. But, we don't own this memory.

var numberOfCapturedVariables = 6621;
function use() { }
function makeFunction() { 
    var varName;
    var outerFunction = "";
    var innerFunction = "";

    for (var i = 0; i < numberOfCapturedVariables; i++) {
        varName = "_" + i;
        outerFunction += "var " + varName + ";";
        innerFunction += "use(" + varName + ");";
    }
    outerFunction += "function foo() {" + innerFunction + "}";
    var functionString = "(function() { " + outerFunction + "})";
    var result = eval(functionString);
    return result;
}

var arr = [];
for (var i = 0; i < 50; i++) {
    var f = makeFunction();
    f();
    fullGC();
}

