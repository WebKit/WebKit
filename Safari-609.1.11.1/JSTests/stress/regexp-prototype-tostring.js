function assert(b) {
    if (!b)
        throw new Error("bad assertion")
}
function test(f) {
    for (let i = 0; i < 100; i++)
        f();
}

test(function() {
    var get = [];
    var p = new Proxy({}, { get: function(o, k) { get.push(k); return o[k]; }});
    RegExp.prototype.toString.call(p);
    assert(get + '' === "source,flags");
});

test(function() {
    let handler = {
        get: function(o, propName) {
            switch(propName) {
            case 'source': 
                return "foobar";
            case 'flags': 
                return "whatever";
            default:
                assert(false, "should not be reached");
            }
        }
    }
    let proxy = new Proxy({}, handler);
    let result = RegExp.prototype.toString.call(proxy);
    assert(result === "/foobar/whatever");
});

test(function() {
    let handler = {
        get: function(o, propName) {
            switch(propName) {
            case 'source': 
                return "hello";
            case 'flags': 
                return "y";
            default:
                assert(false, "should not be reached");
            }
        }
    }

    let proxy = new Proxy({}, handler);
    let result = RegExp.prototype.toString.call(proxy);
    assert(result === "/hello/y");
});

test(function() {
    let error = null;
    let obj = {
        get flags() {
            error = new Error;
            throw error;
        }
    }

    let threw = false;
    try {
        RegExp.prototype.toString.call(obj);
    } catch(e) {
        assert(e === error);
        threw = true;
    }
    assert(threw);
});

test(function() {
    let error = null;
    let obj = {
        get source() {
            error = new Error;
            throw error;
        }
    }

    let threw = false;
    try {
        RegExp.prototype.toString.call(obj);
    } catch(e) {
        assert(e === error);
        threw = true;
    }
    assert(threw);
});
