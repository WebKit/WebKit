//@ runDefault("--useConcurrentJIT=false", "--jitPolicyScale=0.1")
var proto = {};

function g1() { return 1; }
function g2() { return 2; }
function g3() { return 3; }

proto.__defineGetter__("p", g1);

var o = Object.create(proto);

function getP(obj) {
    return obj.p;
}
noInline(getP);

for (var i = 0; i < 80; ++i)
    getP(o);

proto.__defineGetter__("p", g2);
getP(o);
proto.__defineGetter__("p", g3);
getP(o);

var evil = new Proxy(function() {}, {
    apply: function(target, thisArg, args) {
        delete proto.p;
        return 0x42;
    }
});
proto.__defineGetter__("p", evil);

getP(o);
