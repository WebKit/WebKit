//@ skip if $model =~ /^Apple Watch/
//@ $skipModes << :lockdown if $buildType == "debug"

function C() {
    this.x = 4;
    delete this.x
}
noInline(C)

function D() {
    delete this.x
}
noInline(D)

function foo(o) {
    delete o.x
}
noInline(foo)

function E() {
    this.x = 4
    foo(this)
}
noInline(E)

function F() {
    foo(this)
}
noInline(F)

for (let i = 0; i < 10000000; ++i) {
    new C
    new D
    new E
    new F
}
