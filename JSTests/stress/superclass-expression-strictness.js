function f() {
    {
        var error;
        try {
            class c extends(d = function() {
                return {};
            }, d) {}
        } catch (e) {
            error = e;
        }

        if (!error || error.message != "Can't find variable: d")
            throw new Error("Test should have thrown a reference error");
    }

    {
        var error;
        var obj = {};
        Object.defineProperty(obj, 'x', { configurable: false, value: 1 });
        try {
            class c extends(delete obj.x, ()=>{}) {}
        } catch (e) {
            error = e;
        }

        if (!error || error.message != "Unable to delete property.")
            throw new Error("Test should have thrown a type error");
    }

    {
        var error;
        var obj = {};
        Object.defineProperty(obj, 'x', { configurable: false, value: 1 });
        try {
            class c extends(eval('delete obj.x'), class{}) {}
        } catch (e) {
            error = e;
        }

        if (!error || error.message != "Unable to delete property.")
            throw new Error("Test should have thrown a type error");
    }

    {
        var o = {};
        o.__defineGetter__("x", function () { return 42; });
        try {
            class c extends (o.x = 13, class { }) { }
        } catch (e) {
            error = e;
        }

        if (!error || error.message != "Attempted to assign to readonly property.")
            throw new Error("Test should have thrown a type error");
    }
}

 for (var i = 0; i < 10000; i++)
    f();
