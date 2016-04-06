function tryGetByIdText(propertyName) { return `(function (base) { return @tryGetById(base, '${propertyName}'); })`; }


// Test get value off object.
{
    let getCaller = createBuiltin(tryGetByIdText("caller"));
    noInline(getCaller);
    let obj = {caller: 1};

    for (let i = 0; i < 100000; i++) {
        if (getCaller(obj) !== 1)
            throw new Error("wrong on iteration: " + i);
    }
}

// Test slot is custom function trap for a value.
{
    let getCaller = createBuiltin(tryGetByIdText("caller"));
    noInline(getCaller);
    let func = function () {};

    for (let i = 0; i < 100000; i++) {
        if (getCaller(func) !== null)
            throw new Error("wrong on iteration: " + i);
    }
}

// Test slot is a GetterSetter.
{
    let get = createBuiltin(tryGetByIdText("getterSetter"));
    noInline(get);
    let obj = {};
    Object.defineProperty(obj, "getterSetter", { get: function () { throw new Error("should not be called"); } });

    for (let i = 0; i < 100000; i++) {
        if (get(obj) !== getGetterSetter(obj, "getterSetter"))
            throw new Error("wrong on iteration: " + i);
    }
}

// Test slot is unset.
{
    let get = createBuiltin(tryGetByIdText("getterSetter"));
    noInline(get);
    let obj = {};

    for (let i = 0; i < 100000; i++) {
        if (get(obj) !== undefined)
            throw new Error("wrong on iteration: " + i);
    }
}

// Test slot is on a proxy with value.
{
    let get = createBuiltin(tryGetByIdText("value"));
    noInline(get);

    let obj = {value: 1};
    let p = new Proxy(obj, { get: function() { throw new Error("should not be called"); } });

    for (let i = 0; i < 100000; i++) {
        if (get(p) !== null)
            throw new Error("wrong on iteration: " + i);
    }
}

// Test mutating inline cache.
{
    let get = createBuiltin(tryGetByIdText("caller"));
    noInline(get);

    let obj = {caller : 1};
    let func = function() {};

    for (let i = 0; i < 100000; i++) {
        if (i % 100 === 0) {
            if (get(func) !== null)
                throw new Error("wrong on iteration: " + i);
        } else {
            if (get(obj) !== 1)
                throw new Error("wrong on iteration: " + i);
        }
    }
}

// Test new type on each iteration.
{
    let get = createBuiltin(tryGetByIdText("caller"));
    noInline(get);

    let func = function() {};

    for (let i = 0; i < 100000; i++) {
        if (i % 100 === 0) {
            if (get(func) !== null)
                throw new Error("wrong on iteration: " + i);
        } else {
            let obj = {caller : 1};
            if (get(obj) !== 1)
                throw new Error("wrong on iteration: " + i);
        }
    }
}

// Test with array length. This is null because the value is not cacheable.
{
    let get = createBuiltin(tryGetByIdText("length"));
    noInline(get);

    let arr = [];

    for (let i = 0; i < 100000; i++) {
        if (get(arr) !== null)
            throw new Error("wrong on iteration: " + i);

    }
}
