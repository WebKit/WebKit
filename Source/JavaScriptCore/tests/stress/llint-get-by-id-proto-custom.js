var v = Object.create(createCustomGetterObject());
var expected = v.customGetter;

for (var i = 0; i < 10; i++) {
    if (i == 9) {
        v = {customGetter: 10};
        expected = 10;
    }
    if (v.customGetter != expected) {
        throw Error("");
    }
}

