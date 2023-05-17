(function() {
    "use strict";

    var proxy = new Proxy({}, {
        deleteProperty(target, propertyName) {
            return true;
        }
    });

    for (var i = 0; i < 1e6; ++i)
        delete proxy.foo;
})();
