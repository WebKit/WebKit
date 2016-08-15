(function () {
    var object = {
        2: 2
    };

    var result = object[2];
    if (result !== 2)
        throw new Error('bad value:' + result);
}());


(function () {
    var object = {
        get 2() {
            return 1;
        },
        set 2(value) {
            throw new Error(2);
        },
    };

    var result = object[2];
    if (result !== 1)
        throw new Error('bad value:' + result);
}());

(function () {
    var object = {
        get 2() {
            return 1;
        },
        set 2(value) {
            throw new Error(2);
        },
        2: 2,  // Do not throw new Error(2)
    };

    var result = object[2];
    if (result !== 2)
        throw new Error('bad value:' + result);
}());
