function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}

function funcName() { return "func"; }
function klassName() { return "klass"; }

// Anonymous.
assert( (function(){}).name === "" );
assert( (function*(){}).name === "" );
assert( (()=>{}).name === "" );
assert( (class{}).name === "" );

// Named functions, do not infer name.
let f1 = function namedFunction1(){};
let f2 = function* namedFunction2(){};
let k1 = class namedClass{};
assert(f1.name === "namedFunction1");
assert(f2.name === "namedFunction2");
assert(k1.name === "namedClass");

// Assignment, infer name.
let func1 = function(){};
let func2 = function*(){};
let func3 = ()=>{};
assert(func1.name === "func1");
assert(func2.name === "func2");
assert(func3.name === "func3");

// Destructuring assignment default value.
let [ arrFunc1 = function(){} ] = [];
let [ arrFunc2 = function*(){} ] = [];
let [ arrFunc3 = ()=>{} ] = [];
let { objFunc1 = function(){} } = {};
let { objFunc2 = function*(){} } = {};
let { objFunc3 = ()=>{} } = {};
let [ arrClass = class{} ] = [];
let { objClass = class{} } = {};
assert(arrFunc1.name === "arrFunc1");
assert(arrFunc2.name === "arrFunc2");
assert(arrFunc3.name === "arrFunc3");
assert(objFunc1.name === "objFunc1");
assert(objFunc2.name === "objFunc2");
assert(objFunc3.name === "objFunc3");
assert(arrClass.name === "arrClass");
assert(objClass.name === "objClass");

for ([ forArrFunc1 = function(){} ] of [[]])
    assert(forArrFunc1.name === "forArrFunc1");
for ([ forArrFunc2 = function*(){} ] of [[]])
    assert(forArrFunc2.name === "forArrFunc2");
for ([ forArrFunc3 = ()=>{} ] of [[]])
    assert(forArrFunc3.name === "forArrFunc3");
for ([ forArrClass = class{} ] of [[]])
    assert(forArrClass.name === "forArrClass");

for ({ forObjFunc1 = function(){} } of [{}])
    assert(forObjFunc1.name === "forObjFunc1");
for ({ forObjFunc2 = function*(){} } of [{}])
    assert(forObjFunc2.name === "forObjFunc2");
for ({ forObjFunc3 = ()=>{} } of [{}])
    assert(forObjFunc3.name === "forObjFunc3");
for ({ forObjClass = class{} } of [{}])
    assert(forObjClass.name === "forObjClass");

// Global variable assignment.
assert( (globalFunc = function(){}).name === "globalFunc" );
assert( (globalFunc = function*(){}).name === "globalFunc" );
assert( (globalFunc = ()=>{}).name === "globalFunc" );
assert( (globalKlass = class{}).name === "globalKlass" );

// Named properties.
assert( ({"func": function(){}}).func.name === "func" );
assert( ({"func": function*(){}}).func.name === "func" );
assert( ({func: function(){}}).func.name === "func" );
assert( ({func: function*(){}}).func.name === "func" );
assert( ({func(){}}).func.name === "func" );
assert( ({*func(){}}).func.name === "func" );
assert( ({["func"]: function(){}}).func.name === "func" );
assert( ({["func"]: function*(){}}).func.name === "func" );
assert( ({["func"](){}}).func.name === "func" );
assert( ({*["func"](){}}).func.name === "func" );
assert( ({[funcName()]: function(){}}).func.name === "func" );
assert( ({[funcName()]: function*(){}}).func.name === "func" );
assert( ({[funcName()](){}}).func.name === "func" );
assert( ({*[funcName()](){}}).func.name === "func" );

assert( ({"func": ()=>{}}).func.name === "func" );
assert( ({func: ()=>{}}).func.name === "func" );
assert( ({["func"]: ()=>{}}).func.name === "func" );
assert( ({[funcName()]: ()=>{}}).func.name === "func" );

assert( ({"klass": class{}}).klass.name === "klass" );
assert( ({klass: class{}}).klass.name === "klass" );
assert( ({["klass"]: class{}}).klass.name === "klass" );
assert( ({[klassName()]: class{}}).klass.name === "klass" );

// Unnamed computed properties.
let sym = Symbol();
assert( ({[sym]: function(){}})[sym].name === "" );
assert( ({[sym]: function*(){}})[sym].name === "" );
assert( ({[sym]: ()=>{}})[sym].name === "" );
assert( ({[sym](){}})[sym].name === "" );
assert( ({*[sym](){}})[sym].name === "" );
assert( ({[sym]: class{}})[sym].name === "" );

// Parameter default value.
assert( (function(func = function(){}) { return func.name })() === "func" );
assert( (function(func = function*(){}) { return func.name })() === "func" );
assert( (function(func = ()=>{}) { return func.name })() === "func" );
assert( (function(klass = class{}) { return klass.name })() === "klass" );

// Parameter Destructuring default value.
assert( (function({func = function(){}}) { return func.name })({}) === "func" );
assert( (function({func = function*(){}}) { return func.name })({}) === "func" );
assert( (function({func = ()=>{}}) { return func.name })({}) === "func" );
assert( (function([func = function(){}]) { return func.name })([]) === "func" );
assert( (function([func = function*(){}]) { return func.name })([]) === "func" );
assert( (function([func = ()=>{}]) { return func.name })([]) === "func" );
assert( (function({klass = class{}}) { return klass.name })({}) === "klass" );
assert( (function([klass = class{}]) { return klass.name })([]) === "klass" );

assert( (({func = function(){}}) => { return func.name })({}) === "func" );
assert( (({func = function*(){}}) => { return func.name })({}) === "func" );
assert( (({func = ()=>{}}) => { return func.name })({}) === "func" );
assert( (([func = function(){}]) => { return func.name })([]) === "func" );
assert( (([func = function*(){}]) => { return func.name })([]) === "func" );
assert( (([func = ()=>{}]) => { return func.name })([]) === "func" );
assert( (({klass = class{}}) => { return klass.name })({}) === "klass" );
assert( (([klass = class{}]) => { return klass.name })([]) === "klass" );

assert( ({ method({func = function(){}}) { return func.name } }).method({}) === "func" );
assert( ({ method({func = function*(){}}) { return func.name } }).method({}) === "func" );
assert( ({ method({func = ()=>{}}) { return func.name } }).method({}) === "func" );
assert( ({ method([func = function(){}]) { return func.name } }).method([]) === "func" );
assert( ({ method([func = function*(){}]) { return func.name } }).method([]) === "func" );
assert( ({ method([func = ()=>{}]) { return func.name } }).method([]) === "func" );
assert( ({ method({klass = class{}}) { return klass.name } }).method({}) === "klass" );
assert( ({ method([klass = class{}]) { return klass.name } }).method([]) === "klass" );

// B.3.1__proto__ Property Names in Object Initializers

assert( ({__proto__: function(){}}).__proto__.name === "" );
assert( ({__proto__: function*(){}}).__proto__.name === "" );
assert( ({__proto__: ()=>{}}).__proto__.name === "" );
assert( ({["__proto__"]: function(){}}).__proto__.name === "__proto__" );
assert( ({["__proto__"]: function*(){}}).__proto__.name === "__proto__" );
assert( ({["__proto__"]: ()=>{}}).__proto__.name === "__proto__" );
assert( ({__proto__(){}}).__proto__.name === "__proto__" );
assert( ({*__proto__(){}}).__proto__.name === "__proto__" );
assert( ({__proto__(){}}).__proto__.name === "__proto__" );
