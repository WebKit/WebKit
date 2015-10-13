function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

(function() {
    "use strict";
    var cols = {"col":{"title":"&nbsp;","type":"sys","events":[],"name":0,"id":0,"_i":0}};
    var len = 0;
    var remapcols = ['col'];
    for (var i = 0; i < remapcols.length; i++) {
        cols[cols[remapcols[i]].name] = cols[remapcols[i]];
        delete cols[remapcols[i]];
    }
    var count = 0;
    for (var col2 in cols) {
        count++;
        shouldBe(col2, '0');
    }
    shouldBe(count, 1);
}());

(function() {
    "use strict";
    var cols = {"col":{"title":"&nbsp;","type":"sys","events":[],"name":0,"id":0,"_i":0}};
    var len = 0;
    var remapcols = ['col'];
    for (var i = 0; i < remapcols.length; i++) {
        cols[cols[remapcols[i]].name] = cols[remapcols[i]];
        delete cols[remapcols[i]];
    }
    var count = 0;
    for (var col2 of Reflect.enumerate(cols)) {
        count++;
        shouldBe(col2, '0');
    }
    shouldBe(count, 1);
}());
