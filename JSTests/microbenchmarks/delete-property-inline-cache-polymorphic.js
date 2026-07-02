//@ skip if $model =~ /^Apple Watch/

function C(prop) {
    this[prop] = 4
    delete this[prop]
}
noInline(C)

function foo(o, prop) {
    delete o[prop]
}
noInline(foo)

function F(prop) {
    this[prop] = 4
    foo(this, prop)
}
noInline(F)

for (let i = 0; i < 100000; ++i) {
    new C("foo1")
    new F("foo1")
    new C("foo2")
    new F("foo2")
    new C("foo3")
    new F("foo3")
    new C("foo4")
    new F("foo4")
    new C("foo5")
    new F("foo5")
    new C("foo6")
    new F("foo6")
    new C("foo7")
    new F("foo7")
    new C("foo8")
    new F("foo8")
    new C("foo9")
    new F("foo9")
    new C("foo10")
    new F("foo10")
}
