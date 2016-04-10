function tryGetByIdText(propertyName) { return `(function (base) { return @tryGetById(base, '${propertyName}'); })`; }

function tryGetByIdTextStrict(propertyName) { return `(function (base) { "use strict"; return @tryGetById(base, '${propertyName}'); })`; }

// Test get value off object.
{
    let get = createBuiltin(tryGetByIdText("caller"));
    noInline(get);
    let getStrict = createBuiltin(tryGetByIdTextStrict("caller"));
    noInline(getStrict);

    let obj = {caller: 1};

    for (let i = 0; i < 100000; i++) {
        if (get(obj) !== 1)
            throw new Error("wrong on iteration: " + i);
        if (getStrict(obj) !== 1)
            throw new Error("wrong on iteration: " + i);
    }
}

// Test slot is custom function trap for a value.
{
    let get = createBuiltin(tryGetByIdText("caller"));
    noInline(get);
    let getStrict = createBuiltin(tryGetByIdTextStrict("caller"));
    noInline(getStrict);

    let func = function () {};

    for (let i = 0; i < 100000; i++) {
        if (get(func) !== null)
            throw new Error("wrong on iteration: " + i);
        if (getStrict(func) !== null)
            throw new Error("wrong on iteration: " + i);
    }
}

// Test slot is a GetterSetter.
{
    let get = createBuiltin(tryGetByIdText("getterSetter"));
    noInline(get);
    let getStrict = createBuiltin(tryGetByIdTextStrict("getterSetter"));
    noInline(getStrict);

    let obj = {};
    Object.defineProperty(obj, "getterSetter", { get: function () { throw new Error("should not be called"); } });

    for (let i = 0; i < 100000; i++) {
        if (get(obj) !== getGetterSetter(obj, "getterSetter"))
            throw new Error("wrong on iteration: " + i);
        if (getStrict(obj) !== getGetterSetter(obj, "getterSetter"))
            throw new Error("wrong on iteration: " + i);
    }
}

// Test slot is unset.
{
    let get = createBuiltin(tryGetByIdText("getterSetter"));
    noInline(get);
    let getStrict = createBuiltin(tryGetByIdTextStrict("getterSetter"));
    noInline(getStrict);

    let obj = {};

    for (let i = 0; i < 100000; i++) {
        if (get(obj) !== undefined)
            throw new Error("wrong on iteration: " + i);
        if (getStrict(obj) !== undefined)
            throw new Error("wrong on iteration: " + i);
    }
}

// Test slot is on a proxy with value.
{
    let get = createBuiltin(tryGetByIdText("value"));
    noInline(get);
    let getStrict = createBuiltin(tryGetByIdTextStrict("value"));
    noInline(getStrict);

    let obj = {value: 1};
    let p = new Proxy(obj, { get: function() { throw new Error("should not be called"); } });

    for (let i = 0; i < 100000; i++) {
        if (get(p) !== null)
            throw new Error("wrong on iteration: " + i);
        if (getStrict(p) !== null)
            throw new Error("wrong on iteration: " + i);
    }
}

// Test mutating inline cache.
{
    let get = createBuiltin(tryGetByIdText("caller"));
    noInline(get);
    let getStrict = createBuiltin(tryGetByIdTextStrict("caller"));
    noInline(getStrict);

    let obj = {caller : 1};
    let func = function() {};

    for (let i = 0; i < 100000; i++) {
        if (i % 100 === 0) {
            if (get(func) !== null)
                throw new Error("wrong on iteration: " + i);
            if (getStrict(func) !== null)
            throw new Error("wrong on iteration: " + i);
        } else {
            if (get(obj) !== 1)
                throw new Error("wrong on iteration: " + i);
            if (getStrict(obj) !== 1)
            throw new Error("wrong on iteration: " + i);
        }
    }
}

// Test new type on each iteration.
{
    let get = createBuiltin(tryGetByIdText("caller"));
    noInline(get);
    let getStrict = createBuiltin(tryGetByIdTextStrict("caller"));
    noInline(getStrict);

    let func = function() {};

    for (let i = 0; i < 100000; i++) {
        if (i % 100 === 0) {
            if (get(func) !== null)
                throw new Error("wrong on iteration: " + i);
            if (getStrict(func) !== null)
            throw new Error("wrong on iteration: " + i);
        } else {
            let obj = {caller : 1};
            if (get(obj) !== 1)
                throw new Error("wrong on iteration: " + i);
            if (getStrict(obj) !== 1)
            throw new Error("wrong on iteration: " + i);
        }
    }
}

// Test with array length. This is null because the value is not cacheable.
{
    let get = createBuiltin(tryGetByIdText("length"));
    noInline(get);
    let getStrict = createBuiltin(tryGetByIdTextStrict("length"));
    noInline(getStrict);

    let arr = [];

    for (let i = 0; i < 100000; i++) {
        if (get(arr) !== null)
            throw new Error("wrong on iteration: " + i);
        if (getStrict(arr) !== null)
            throw new Error("wrong on iteration: " + i);
    }
}

// Test with non-object.
{
    let get = createBuiltin(tryGetByIdText("length"));
    noInline(get);
    let getStrict = createBuiltin(tryGetByIdTextStrict("length"));
    noInline(getStrict);

    for (let i = 0; i < 100000; i++) {
        if (get(1) !== undefined)
            throw new Error("wrong on iteration: " + i);
        if (getStrict(1) !== undefined)
            throw new Error("wrong on iteration: " + i);
    }
}
