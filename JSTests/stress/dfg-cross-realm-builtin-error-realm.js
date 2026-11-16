// Errors thrown by another realm's built-ins must come from that realm, also from optimized callers.

const other = createGlobalObject();
const OtherNumber = other.Number;
const OtherString = other.String;
const OtherMath = other.Math;
const OtherArray = other.Array;
const OtherInt8Array = other.Int8Array;
const OtherArrayBuffer = other.ArrayBuffer;
const OtherRegExp = other.RegExp;
const OtherSymbol = other.Symbol;
const otherParseInt = other.parseInt;
const otherIsNaN = other.isNaN;
const otherIsFinite = other.isFinite;

// The non-throwing calls pass this object so that optimized code keeps the generic path for the throwing argument
// instead of speculating on its type and exiting.
const object = { valueOf() { return 1; }, toString() { return "1"; } };
const symbol = Symbol("symbol");
const noPrimitive = { valueOf() { return {}; }, toString() { return {}; } };

function numberCall(x) { return OtherNumber(x); }
noInline(numberCall);
function stringCall(x) { return OtherString(x); }
noInline(stringCall);
function stringConstruct(x) { return new OtherString(x); }
noInline(stringConstruct);
function arrayConstruct(x) { return new OtherArray(x); }
noInline(arrayConstruct);
function int8ArrayConstruct(x) { return new OtherInt8Array(x); }
noInline(int8ArrayConstruct);
function arrayBufferConstruct(x) { return new OtherArrayBuffer(x); }
noInline(arrayBufferConstruct);
function regExpConstruct(x) { return new OtherRegExp(x, ""); }
noInline(regExpConstruct);
function symbolCall(x) { return OtherSymbol(x); }
noInline(symbolCall);

// These see the same built-in from two realms, so only its executable is known.
function numberClosureCall(f, x) { return f(x); }
noInline(numberClosureCall);
function stringClosureCall(f, x) { return f(x); }
noInline(stringClosureCall);

function mathAbs(x) { return OtherMath.abs(x); }
noInline(mathAbs);
function mathSqrt(x) { return OtherMath.sqrt(x); }
noInline(mathSqrt);
function mathFloor(x) { return OtherMath.floor(x); }
noInline(mathFloor);
function mathCeil(x) { return OtherMath.ceil(x); }
noInline(mathCeil);
function mathRound(x) { return OtherMath.round(x); }
noInline(mathRound);
function mathTrunc(x) { return OtherMath.trunc(x); }
noInline(mathTrunc);
function mathClz32(x) { return OtherMath.clz32(x); }
noInline(mathClz32);
function mathFround(x) { return OtherMath.fround(x); }
noInline(mathFround);
function mathSin(x) { return OtherMath.sin(x); }
noInline(mathSin);
function parseIntCall(x) { return otherParseInt(x); }
noInline(parseIntCall);
function isNaNCall(x) { return otherIsNaN(x); }
noInline(isNaNCall);
function isFiniteCall(x) { return otherIsFinite(x); }
noInline(isFiniteCall);
function fromCharCode(x) { return OtherString.fromCharCode(x); }
noInline(fromCharCode);
function fromCodePoint(x) { return OtherString.fromCodePoint(x); }
noInline(fromCodePoint);
function fromCodePointInt32(x) { return OtherString.fromCodePoint(x); }
noInline(fromCodePointInt32);
function isArrayCall(x) { return OtherArray.isArray(x); }
noInline(isArrayCall);
function revokedProxy() {
    const { proxy, revoke } = Proxy.revocable([], { });
    revoke();
    return proxy;
}

Number.prototype.otherToString = OtherNumber.prototype.toString;
function numberToStringRadix(radix) { return (255).otherToString(radix); }
noInline(numberToStringRadix);
Object.prototype.otherValueOf = OtherString.prototype.valueOf;
const stringObject = new String("s");
function stringValueOf(x) { return x.otherValueOf(); }
noInline(stringValueOf);
Object.prototype.otherExec = OtherRegExp.prototype.exec;
const regExp = /a/;
function regExpExec(x) { return x.otherExec("a"); }
noInline(regExpExec);

// [label, non-throwing call, throwing call, expected error constructor]
const tests = [
    ["other.Number(symbol)", () => numberCall(object), () => numberCall(symbol), other.TypeError],
    ["other.String(noPrimitive)", () => stringCall(object), () => stringCall(noPrimitive), other.TypeError],
    ["new other.String(symbol)", () => stringConstruct(object), () => stringConstruct(symbol), other.TypeError],
    ["new other.Array(-1)", i => arrayConstruct(i & 7), () => arrayConstruct(-1), other.RangeError],
    ["new other.Int8Array(-1)", i => int8ArrayConstruct(i & 7), () => int8ArrayConstruct(-1), other.RangeError],
    ["new other.ArrayBuffer(-1)", i => arrayBufferConstruct(i & 7), () => arrayBufferConstruct(-1), other.RangeError],
    ["new other.RegExp(\"(\")", () => regExpConstruct("a"), () => regExpConstruct("("), other.SyntaxError],
    ["other.Symbol(noPrimitive)", () => symbolCall(object), () => symbolCall(noPrimitive), other.TypeError],
    ["Number / other.Number closure call (symbol)", i => numberClosureCall(i & 1 ? Number : OtherNumber, object), () => numberClosureCall(OtherNumber, symbol), other.TypeError],
    ["String / other.String closure call (noPrimitive)", i => stringClosureCall(i & 1 ? String : OtherString, object), () => stringClosureCall(OtherString, noPrimitive), other.TypeError],
    ["other.Math.abs(symbol)", () => mathAbs(object), () => mathAbs(symbol), other.TypeError],
    ["other.Math.sqrt(symbol)", () => mathSqrt(object), () => mathSqrt(symbol), other.TypeError],
    ["other.Math.floor(symbol)", () => mathFloor(object), () => mathFloor(symbol), other.TypeError],
    ["other.Math.ceil(symbol)", () => mathCeil(object), () => mathCeil(symbol), other.TypeError],
    ["other.Math.round(symbol)", () => mathRound(object), () => mathRound(symbol), other.TypeError],
    ["other.Math.trunc(symbol)", () => mathTrunc(object), () => mathTrunc(symbol), other.TypeError],
    ["other.Math.clz32(symbol)", () => mathClz32(object), () => mathClz32(symbol), other.TypeError],
    ["other.Math.fround(symbol)", () => mathFround(object), () => mathFround(symbol), other.TypeError],
    ["other.Math.sin(symbol)", () => mathSin(object), () => mathSin(symbol), other.TypeError],
    ["other.parseInt(symbol)", () => parseIntCall(object), () => parseIntCall(symbol), other.TypeError],
    ["other.isNaN(symbol)", () => isNaNCall(object), () => isNaNCall(symbol), other.TypeError],
    ["other.isFinite(symbol)", () => isFiniteCall(object), () => isFiniteCall(symbol), other.TypeError],
    ["other.String.fromCharCode(symbol)", () => fromCharCode(object), () => fromCharCode(symbol), other.TypeError],
    ["other.String.fromCodePoint(symbol)", () => fromCodePoint(object), () => fromCodePoint(symbol), other.TypeError],
    ["other.String.fromCodePoint(-1)", i => fromCodePointInt32(i & 0x7f), () => fromCodePointInt32(-1), other.RangeError],
    ["other.Number.prototype.toString(0)", () => numberToStringRadix(10), () => numberToStringRadix(0), other.RangeError],
    ["other.String.prototype.valueOf on 1", () => stringValueOf(stringObject), () => stringValueOf(1), other.TypeError],
    ["other.RegExp.prototype.exec on {}", () => regExpExec(regExp), () => regExpExec({ }), other.TypeError],
    ["other.Array.isArray(revokedProxy)", () => isArrayCall(object), () => isArrayCall(revokedProxy()), other.TypeError],
];

const failures = new Map();
for (let i = 0; i < testLoopCount; ++i) {
    for (const [label, good, bad, ErrorConstructor] of tests) {
        good(i);
        if (i % 64 !== 63 || failures.has(label))
            continue;
        let error = null;
        try {
            bad();
        } catch (e) {
            error = e;
        }
        if (!(error instanceof ErrorConstructor)) {
            const fromCallerRealm = error instanceof globalThis[ErrorConstructor.name];
            failures.set(label, `${label}: expected a ${ErrorConstructor.name} from the callee's realm at iteration ${i}, got ${error}${fromCallerRealm ? " from the caller's realm" : ""}`);
        }
    }
}

if (failures.size)
    throw new Error([...failures.values()].join("\n"));
