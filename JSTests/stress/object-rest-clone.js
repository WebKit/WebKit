function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

function makeState(count) {
    var state = {};
    for (var i = 0; i < count; ++i)
        state["k" + i] = i;
    return state;
}

function restOnly(object) {
    var { ...rest } = object;
    return rest;
}
noInline(restOnly);

function restParameter({ ...rest }) {
    return rest;
}
noInline(restParameter);

function restAssignment(object) {
    var rest;
    ({ ...rest } = object);
    return rest;
}
noInline(restAssignment);

function restExcluding(object) {
    var { k0, ...rest } = object;
    return rest;
}
noInline(restExcluding);

function spreadThenGetter(object) {
    return { ...object, get extra() { return 42; } };
}
noInline(spreadThenGetter);

function protoThenSpread(object) {
    return { __proto__: Object.prototype, ...object };
}
noInline(protoThenSpread);

function nullProtoThenSpread(object) {
    return { __proto__: null, ...object };
}
noInline(nullProtoThenSpread);

class WithPrivateField {
    #secret = 1;
    constructor(count) {
        for (var i = 0; i < count; ++i)
            this["k" + i] = i;
    }
    static hasSecret(object) { return #secret in object; }
}

class WithPrivateMethod {
    #m() { return 1; }
    constructor(count) {
        for (var i = 0; i < count; ++i)
            this["k" + i] = i;
    }
    static hasBrand(object) { return #m in object; }
}

var sym = Symbol("s");
var proto = { inherited: 1 };
var otherGlobal = createGlobalObject();

for (var i = 0; i < testLoopCount; ++i) {
    var count = [4, 8, 20][i % 3];
    var state = makeState(count);
    var keys = Object.keys(state);

    for (var copy of [restOnly(state), restParameter(state), restAssignment(state)]) {
        shouldBeArray(Object.keys(copy), keys);
        shouldBe(copy.k0, 0);
        shouldBe(copy["k" + (count - 1)], count - 1);
        shouldBe(Object.getPrototypeOf(copy), Object.prototype);
        shouldBe(Object.isExtensible(copy), true);
        copy.k0 = "changed";
        copy.added = i;
        shouldBe(state.k0, 0);
        shouldBe(state.added, undefined);
        shouldBe(copy.added, i);
    }

    var excluded = restExcluding(state);
    shouldBeArray(Object.keys(excluded), keys.slice(1));
    shouldBe(excluded.k0, undefined);

    state.afterCopy = "x";
    var copyBeforeMutation = restOnly(state);
    state.k1 = "mutated";
    shouldBe(copyBeforeMutation.k1, 1);
    shouldBe(copyBeforeMutation.afterCopy, "x");

    var withGetter = spreadThenGetter(makeState(count));
    shouldBeArray(Object.keys(withGetter), keys.concat(["extra"]));
    shouldBe(withGetter.extra, 42);

    var frozen = Object.freeze(makeState(count));
    var fromFrozen = restOnly(frozen);
    shouldBe(Object.isFrozen(fromFrozen), false);
    fromFrozen.k0 = "y";
    shouldBe(fromFrozen.k0, "y");
    shouldBe(frozen.k0, 0);

    var withSymbol = makeState(count);
    withSymbol[sym] = "sym";
    Object.defineProperty(withSymbol, "hidden", { value: 1, enumerable: false });
    var copied = restOnly(withSymbol);
    shouldBe(copied[sym], "sym");
    shouldBe(Object.getOwnPropertyDescriptor(copied, "hidden"), undefined);

    var getterCalls = 0;
    var withAccessor = makeState(count);
    Object.defineProperty(withAccessor, "accessor", { get() { return ++getterCalls; }, enumerable: true });
    var fromAccessor = restOnly(withAccessor);
    shouldBe(getterCalls, 1);
    shouldBe(fromAccessor.accessor, 1);
    shouldBe(Object.getOwnPropertyDescriptor(fromAccessor, "accessor").writable, true);

    var withIndex = makeState(count);
    withIndex[0] = "zero";
    var fromIndexed = restOnly(withIndex);
    shouldBe(fromIndexed[0], "zero");
    shouldBe(Object.keys(fromIndexed).length, count + 1);

    var withProto = Object.create(proto);
    for (var j = 0; j < count; ++j)
        withProto["k" + j] = j;
    var fromProto = restOnly(withProto);
    shouldBe(Object.getPrototypeOf(fromProto), Object.prototype);
    shouldBe(fromProto.inherited, undefined);
    shouldBeArray(Object.keys(fromProto), keys);

    var dictionary = makeState(count);
    dictionary.extra = 1;
    delete dictionary.extra;
    var fromDictionary = restOnly(dictionary);
    shouldBeArray(Object.keys(fromDictionary), keys);

    var readOnly = makeState(count);
    Object.defineProperty(readOnly, "k1", { writable: false });
    var fromReadOnly = restOnly(readOnly);
    shouldBe(Object.getOwnPropertyDescriptor(fromReadOnly, "k1").writable, true);
    fromReadOnly.k1 = "w";
    shouldBe(fromReadOnly.k1, "w");
    shouldBe(readOnly.k1, 1);

    var nonConfigurable = makeState(count);
    Object.defineProperty(nonConfigurable, "k2", { configurable: false });
    var fromNonConfigurable = restOnly(nonConfigurable);
    shouldBe(Object.getOwnPropertyDescriptor(fromNonConfigurable, "k2").configurable, true);
    shouldBe(delete fromNonConfigurable.k2, true);
    shouldBe(nonConfigurable.k2, 2);

    var sealed = Object.seal(makeState(count));
    var fromSealed = restOnly(sealed);
    shouldBe(Object.isSealed(fromSealed), false);
    fromSealed.added = 1;
    shouldBe(fromSealed.added, 1);

    var withPrivateField = new WithPrivateField(count);
    var fromPrivateField = restOnly(withPrivateField);
    shouldBeArray(Object.keys(fromPrivateField), keys);
    shouldBe(WithPrivateField.hasSecret(withPrivateField), true);
    shouldBe(WithPrivateField.hasSecret(fromPrivateField), false);

    var withPrivateMethod = new WithPrivateMethod(count);
    var fromPrivateMethod = restOnly(withPrivateMethod);
    shouldBeArray(Object.keys(fromPrivateMethod), keys);
    shouldBe(WithPrivateMethod.hasBrand(fromPrivateMethod), false);

    var otherRealm = otherGlobal.Function("count", "var o = {}; for (var i = 0; i < count; ++i) o['k' + i] = i; return o;")(count);
    var fromOtherRealm = restOnly(otherRealm);
    shouldBe(Object.getPrototypeOf(fromOtherRealm), Object.prototype);
    shouldBeArray(Object.keys(fromOtherRealm), keys);

    var withProtoLiteral = protoThenSpread(makeState(count));
    shouldBe(Object.getPrototypeOf(withProtoLiteral), Object.prototype);
    shouldBeArray(Object.keys(withProtoLiteral), keys);

    var withNullProtoLiteral = nullProtoThenSpread(makeState(count));
    shouldBe(Object.getPrototypeOf(withNullProtoLiteral), null);
    shouldBeArray(Object.keys(withNullProtoLiteral), keys);

    shouldBeArray(Object.keys(restOnly({})), []);
    shouldBeArray(Object.keys(restOnly("ab")), ["0", "1"]);
    shouldBeArray(Object.keys(restOnly([1, 2])), ["0", "1"]);
}
