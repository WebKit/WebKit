function indexOf(string, search) {
    return string.indexOf(search);
}
noInline(indexOf);

function indexOfFrom(string, search, position) {
    return string.indexOf(search, position);
}
noInline(indexOfFrom);

var row = "1042,Widget,\"Blue, large\",19.99,2024-01-15,in stock";
var searches = [",W", "\",", ";;", "-0"];

var result = 0;
for (var i = 0; i < 1e6; ++i) {
    var search = searches[i & 3];
    result += indexOf(row, search);
    result += indexOfFrom(row, search, 12);
}
if (result !== 30250000)
    throw new Error("bad result: " + result);
