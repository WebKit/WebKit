function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// Simple assignment (not FunctionCallBracketNode).

shouldBe(function () {
    var object = {
        null: 'ok'
    };

    return object[object = null];
}(), 'ok');

shouldBe(function (value) {
    var object = { };
    object.null = 'ok';

    return object[object = value];
}(null), 'ok');

shouldBe(function () {
    var object = {
        null: 'ok'
    };

    return object['null'];
}(), 'ok');

shouldBe(function (value) {
    var object = { };
    object.null = 'ok';

    return object['null'];
}(null), 'ok');

shouldBe(function () {
    var object = {
        null: 'ok'
    };

    function fill() {
        return object = null;
    }

    return object[fill()];
}(), 'ok');

shouldBe(function (value) {
    var object = { };
    object.null = 'ok';

    function fill() {
        return object = value;
    }

    return object[fill()];
}(null), 'ok');

// FunctionCallBracketNode.

shouldBe(function () {
    var object = {
        null: function () {
            return 'ok';
        }
    };

    return object[object = null]();
}(), 'ok');

shouldBe(function (value) {
    var object = { };
    object.null = function () {
        return 'ok';
    };

    return object[object = value]();
}(null), 'ok');

shouldBe(function () {
    var object = {
        null: function () {
            return 'ok';
        }
    };

    return object['null']();
}(), 'ok');

shouldBe(function (value) {
    var object = { };
    object.null = function () {
        return 'ok';
    };

    return object['null']();
}(null), 'ok');

shouldBe(function () {
    var object = {
        null: function () {
            return 'ok';
        }
    };

    function fill() {
        return object = null;
    }

    return object[fill()]();
}(), 'ok');

shouldBe(function (value) {
    var object = { };
    object.null = function () {
        return 'ok';
    };

    function fill() {
        return object = value;
    }

    return object[fill()]();
}(null), 'ok');
