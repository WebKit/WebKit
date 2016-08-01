(function testCase() {
    var target = {};
    var handler = {
        has: function (target, name) {
            if (name === 'ng')
                throw new Error('ng');
            return false;
        }
    };
    var proxy = new Proxy(target, handler);
    var base = {
        __proto__: proxy
    };
    (function a() {
        var thrown;
        for (var i = 0; i < 1e4; ++i) {
            thrown = null;
            try {
                'ng' in base;
            } catch (e) {
                thrown = e;
            }

            if (thrown === null)
                throw new Error(`not thrown ${i}`);
        }
    }());
}());
