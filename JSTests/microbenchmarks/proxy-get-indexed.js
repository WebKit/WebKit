(function() {
var proxy = new Proxy([], {
    get(target, propertyName, receiver) {
        return propertyName;
    }
});

var length = 20;
for (var i = 0; i < (1e7 / length); ++i) {
    for (var j = 0; j < length; ++j)
        var lastPropertyName = proxy[j];
}

if (lastPropertyName !== `${length - 1}`)
    throw new Error("Bad assertion!");
})();
