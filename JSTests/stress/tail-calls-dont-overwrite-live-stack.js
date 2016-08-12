"use strict";

function tail(a, b) { }
noInline(tail);

var obj = {
    method: function (x) {
        return tail(x, x);
    },

    get fromNative() { return tail(0, 0); }
};
noInline(obj.method);

function getThis(x) { return this; }
noInline(getThis);

for (var i = 0; i < 10000; ++i) {
    var that = getThis(obj.method(42));

    if (!Object.is(that, undefined))
        throw new Error("Wrong 'this' value in call, expected undefined but got " + that);

    that = getThis(obj.method(...[42]));
    if (!Object.is(that, undefined))
        throw new Error("Wrong 'this' value in varargs call, expected undefined but got " + that);

    if (!Object.is(obj.fromNative, undefined))
        throw new Error("Wrong 'fromNative' value, expected undefined but got " + obj.fromNative);
}
