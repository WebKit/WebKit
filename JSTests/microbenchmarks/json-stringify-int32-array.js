var arr = [];
for (var i = 0; i < 1000; i++)
    arr.push(i);
var json;
for (var j = 0; j < 1e4; j++)
    json = JSON.stringify(arr);
if (json.length !== 3891)
    throw new Error("Bad result: " + json.length);
