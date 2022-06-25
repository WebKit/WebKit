//@ runDefault

let p = new Proxy([], {
    get: function() {
        return {};
    }
});
JSON.stringify(null, p);
