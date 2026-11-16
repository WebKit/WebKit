var desc1 = {value: 1, writable: true, enumerable: false, configurable: true};
var desc2 = {value: 1, writable: false, enumerable: true, configurable: true};

for (var i = 0; i < 1e5; ++i) {
    var obj = {foo: 1};

    Object.defineProperty(obj, "foo", desc1);
    Object.defineProperty(obj, "foo", desc2);
    Object.defineProperty(obj, "foo", desc1);
    Object.defineProperty(obj, "foo", desc2);
    Object.defineProperty(obj, "foo", desc1);
    Object.defineProperty(obj, "foo", desc2);
}
