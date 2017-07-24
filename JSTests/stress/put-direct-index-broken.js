function whatToTest(code){return {allowExec: true,};}
function tryRunning(f, code, wtt)
{
    uneval = true
    try {var rv = f();} catch(runError) {}
    try {if ('__defineSetter__' in this) {delete this.uneval;} } catch(e) {}
}
function tryItOut(code)
{
    var wtt = true;
    var f;
    try {f = new Function(code);} catch(compileError) {}
        tryRunning(f, code, wtt);
}
tryItOut(`a0 = []; 
        r0 = /x/; 
        t0 = new Uint8ClampedArray;
        o1 = {};
        g1 = this;
        v2 = null;`);

tryItOut("func = (function(x, y) {});");
tryItOut("for (var p in g1) { this.a0[new func([].map(q => q, null), x)]; }");
tryItOut("a0.push(o1.m1);a0.length = (4277);a0.__proto__ = this.t0;");
tryItOut("\"use strict\"; a0 = Array.prototype.map.call(a0, (function() {}));");
