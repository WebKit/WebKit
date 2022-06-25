var assert = function (result, expected, message = "") {
  if (result !== expected) {
    throw new Error('Error in assert. Expected "' + expected + '" but was "' + result + '":' + message );
  }
};

const Logger = function () {
    var log = [];

    this.logEvent = (type, value, done) => {
        log.push({ type, value, done});
    };
    this.logFulfilledEvent = (value, done) => {
        this.logEvent('fulfilled', value, done);
    };
    this.logRejectEvent = error => {
        this.logEvent('reject', error.toString(), true);
    };
    this.logCatchEvent = value => {
        this.logEvent('catch', value, true);
    };
    this.getLogger = () => log;

    this.clear = () => {
        log = [];
    }
};

const fulfillSpy = logger => result => logger.logFulfilledEvent(result.value, result.done);
const rejectSpy = logger => error => logger.logRejectEvent(error);
const catchSpy = logger => error => logger.logCatchEvent(error);

const assertLogger = function (loggerObject) {
    const logger = loggerObject.getLogger();

    var _assertLogger = function () {
        let index = 0;

        const isNotOutOfLength = () => {
            assert(index < logger.length, true, `Index is greater then log length`);   
        }

        this.fullfilled = function (expectedValue, message = 'on fulfill') {
            isNotOutOfLength();

            const msg = `step: ${index} - ${message}`;
            let step = logger[index];

            assert(step.type, 'fulfilled', msg);
            assert(step.value, expectedValue, msg);
            assert(step.done, false, msg);

            index++;
            return this;
        };

        this.fullfilledDone = function (expectedValue, message = 'on fulfill with done true') {
            isNotOutOfLength();

            const msg = `step: ${index} - ${message}`;
            let step = logger[index];

            assert(step.type, 'fulfilled', msg);
            assert(step.value, expectedValue, msg);
            assert(step.done, true, msg);

            index++;
            return this;
        };

        this.rejected = function (error, message = 'on reject') {
            isNotOutOfLength();

            const msg = `step: ${index} - ${message}`;
            let step = logger[index];

            assert(step.type, 'reject', msg);
            assert(step.value, error.toString(), msg);
            assert(step.done, true, msg);

            index++;
            return this;
        };

        this.catched = function (expectedError, message = 'on catch') {
            isNotOutOfLength();

            const msg = `step: ${index} - ${message}`;
            let step = logger[index];

            assert(step.type, 'catch', msg);
            assert(step.value, expectedError, msg);
            assert(step.done, true, msg);

            index++;
            return this;
        };

        this.isFinal = function (message = '') {
            assert(index, logger.length, `expected final step: ${message}`);
        }; 
    }; 
    
    return new _assertLogger();
};

const getPromise = promiseHolder => {
    return new Promise((resolve, reject) => {
        promiseHolder.resolve = resolve;
        promiseHolder.reject = reject;
    });
};

async function*generator () {
    yield 42;
}

var passed = false;
var iterator = generator();

iterator.next().then(function(step) {
    if(iterator[Symbol.asyncIterator]() === iterator && step.done === false && step.value === 42) passed = true;
});

drainMicrotasks();

assert(passed, true, '');

assert(generator[Symbol.toStringTag], 'AsyncGeneratorFunction');
assert(Object.prototype.toString.call(generator), '[object AsyncGeneratorFunction]');


var holder = {};

var promise = getPromise(holder);
var logger = new Logger();

async function * baz(value) {
    var t = await promise;
    let reply = yield t + ' data-0' + value;
    let last = yield t + ' data-1' + reply;
    return 'data-result' + last;
}

var bz1 = baz(':init');

bz1.next(':0').then(fulfillSpy(logger), rejectSpy(logger));
bz1.next(':1').then(fulfillSpy(logger), rejectSpy(logger));
bz1.next(':2').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .isFinal();

holder.resolve('abcd');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('abcd data-0:init')
    .fullfilled('abcd data-1:1')
    .fullfilledDone('data-result:2')
    .isFinal();

logger.clear();

promise = getPromise(holder);

var bz2 = baz(':init');

bz2.next(':0').then(fulfillSpy(logger), rejectSpy(logger));
bz2.next(':1').then(fulfillSpy(logger), rejectSpy(logger));
bz2.next(':2').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .isFinal();

holder.reject('abcd');

drainMicrotasks();

assertLogger(logger)
    .rejected('abcd')
    .fullfilledDone()
    .fullfilledDone()
    .isFinal();

var holder1 = {}; holder2 = {};
var promise1 = getPromise(holder1);
var promise2 = getPromise(holder2);

logger.clear();

async function *boo() {
    var t1 = await promise1;
    yield t1 + ' data-1';
    yield t1 + ' data-2';
    var t2 = await promise2;
    yield t2 + ' data-3';
    yield t2 + ' data-4';
    return 'data-5';
};

var b = boo();

b.next().then(fulfillSpy(logger));
b.next().then(fulfillSpy(logger));
b.next().then(fulfillSpy(logger));
b.next().then(fulfillSpy(logger));
b.next().then(fulfillSpy(logger));
b.next().then(fulfillSpy(logger));

assertLogger(logger)
    .isFinal('async generator should not resolve any promise until await is no resolved');

holder1.resolve('abcd');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('abcd data-1')
    .fullfilled('abcd data-2')
    .isFinal();

holder2.resolve('efgh');
drainMicrotasks();

assertLogger(logger)
    .fullfilled('abcd data-1')
    .fullfilled('abcd data-2')
    .fullfilled('efgh data-3')
    .fullfilled('efgh data-4')
    .fullfilledDone('data-5')
    .fullfilledDone(undefined, error => print(error))
    .isFinal();

holder = {};

promise = getPromise(holder);

async function *foo() {
    var t = await 'abcd';
    yield t + ' data-5';
    var t2 = await promise;
    yield t2 + ' data-6';
};

logger.clear();

var f = foo();

f.next().then(fulfillSpy(logger));
f.next().then(fulfillSpy(logger));
f.next().then(fulfillSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('abcd data-5')
    .isFinal('async generator should not resolve any promise until await is not resolved');

holder.resolve('xyz');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('abcd data-5')
    .fullfilled('xyz data-6')
    .fullfilledDone(undefined)
    .isFinal('async generator should not resolve any promise until await is not resolved');

holder = {};
promise = getPromise(holder);

async function *goo() {
    yield 'data-5';
    var t = await promise;
    yield t + ' data-6';
    yield t + ' data-7';
};

logger.clear();
var errorText = 'error-reject';
var g = goo();

drainMicrotasks();

g.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('data-5')
    .isFinal('async generator should not resolve any promise until await is not resolved');

g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));

assertLogger(logger)
    .fullfilled('data-5')
    .isFinal('async generator should not resolve any promise until await is not resolved');

holder.reject(errorText);

drainMicrotasks();

assertLogger(logger)
    .fullfilled('data-5')
    .rejected(errorText)
    .fullfilledDone(undefined, 'After reject all resolved value have undefined')
    .isFinal();

/* Method in class */

const someText = 'foo';
const resolveText = 'bar';

logger.clear();

holder = {};
promise = getPromise(holder);

class A { 
    async * foo() { yield someText; } 
    async * boo() { var text = await promise; yield text + someText; }
}
var a = new A;
var gf = a.foo();

gf.next().then(fulfillSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled(someText)
    .isFinal();

const gb = a.boo();

gb.next().then(fulfillSpy(logger));

assertLogger(logger)
    .fullfilled(someText)
    .isFinal();

holder.resolve(resolveText);

drainMicrotasks();

assertLogger(logger)
    .fullfilled(someText)
    .fullfilled(resolveText + someText)
    .isFinal();

/* Async generator function expression */
logger.clear();
holder = {};
promise = getPromise(holder);

var asyncGenExp = async function *() { var t = await promise; yield t + someText + someText; };

var gAsyncGenExp = asyncGenExp();

gAsyncGenExp.next().then(fulfillSpy(logger));

holder.resolve(resolveText);
drainMicrotasks();

assertLogger(logger)
    .fullfilled(resolveText + someText + someText)
    .isFinal();

logger.clear();

/*Test throw*/
async function *joo() {
    yield 'data-1';
    yield 'data-2';
    yield 'data-3';
};

var j = joo();
var errorTextInFunction = "ErrorInFunction";
const errorWrongGenerator = "|this| should be an async generator";
let errorRaised = false;

j.next()
.then(function(value) {
    fulfillSpy(logger)(value);
    return j.throw(new Error(errorTextInFunction));
})
.then(function (value) {
    fulfillSpy(logger)(value);
}, function (error) {
    rejectSpy(logger)(error);
    return j.next();
})
.then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('data-1')
    .rejected((new Error(errorTextInFunction)).toString())
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();
var j1 = joo();

j1.throw(new Error(errorText)).then(fulfillSpy(logger), rejectSpy(logger));
j1.next().then(fulfillSpy(logger), rejectSpy(logger));
j1.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .rejected(new Error(errorText))
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();
var j2 = joo();

const returnValue = 'return-value';

j1.return(returnValue).then(fulfillSpy(logger), rejectSpy(logger));
j1.throw(new Error(errorText)).then(fulfillSpy(logger), rejectSpy(logger));
j1.next().then(fulfillSpy(logger), rejectSpy(logger));
j1.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilledDone(returnValue)
    .rejected(new Error(errorText))
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();
let j3 = joo();

j3.next.call(undefined).then(fulfillSpy(logger), rejectSpy(logger));
drainMicrotasks();

assertLogger(logger)
    .rejected(new TypeError(errorWrongGenerator))
    .isFinal();

logger.clear();
j3 = joo();

j3.next.call(undefined).then(fulfillSpy(logger), rejectSpy(logger));
j3.next().then(fulfillSpy(logger), rejectSpy(logger));
j3.next().then(fulfillSpy(logger), rejectSpy(logger));
j3.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .rejected(new TypeError(errorWrongGenerator))
    .fullfilled('data-1')
    .fullfilled('data-2')
    .fullfilled('data-3')
    .isFinal();

logger.clear();
j3 = joo();

j3.next().then(fulfillSpy(logger), rejectSpy(logger));
j3.next.call(undefined).then(fulfillSpy(logger), rejectSpy(logger));
j3.next().then(fulfillSpy(logger), rejectSpy(logger));
j3.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .rejected(new TypeError(errorWrongGenerator))
    .fullfilled('data-1')
    .fullfilled('data-2')
    .fullfilled('data-3')
    .isFinal();

logger.clear();
j3 = joo();

j3.next().then(fulfillSpy(logger), rejectSpy(logger));
j3.next().then(fulfillSpy(logger), rejectSpy(logger));
j3.next.call(undefined).then(fulfillSpy(logger), rejectSpy(logger));
j3.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .rejected(new TypeError(errorWrongGenerator))
    .fullfilled('data-1')
    .fullfilled('data-2')
    .fullfilled('data-3')
    .isFinal();

logger.clear();
j3 = joo();

j3.next.call({}).catch(rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .rejected(new TypeError(errorWrongGenerator))

logger.clear();
j3 = joo();
j3.next.call('string').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .rejected(new TypeError(errorWrongGenerator))
    .isFinal();

logger.clear();
j3 = joo();
let gen = generator();
j3.next.call(gen).then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled(42)
    .isFinal();

logger.clear();
holder = {};
promise = getPromise(holder);

async function *koo() {
    var t = await 'abcd';
    yield t + ' data-first';
    var t2 = await promise;
    yield t2 + ' data-second';
    yield t2 + ' data-third';
};

const k1 = koo();

k1.next().then(fulfillSpy(logger));
k1.next().then(fulfillSpy(logger));

k1.throw(new Error(errorText)).then(fulfillSpy(logger), rejectSpy(logger));
k1.next().then(fulfillSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('abcd data-first')
    .isFinal();

holder.resolve('xyz');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('abcd data-first')
    .fullfilled('xyz data-second')
    .rejected(new Error(errorText))
    .fullfilledDone(undefined)
    .isFinal();


logger.clear();
holder = {};
promise = getPromise(holder);

const k2 = koo();

k2.next().then(fulfillSpy(logger));
k2.next().then(fulfillSpy(logger));

k2.return(returnValue).then(fulfillSpy(logger));

holder.resolve('xyz');

k2.throw(new Error(errorText)).then(fulfillSpy(logger), rejectSpy(logger));
k2.next().then(fulfillSpy(logger));
k2.next().then(fulfillSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('abcd data-first')
    .fullfilled('xyz data-second')
    .fullfilledDone(returnValue)
    .rejected(new Error(errorText))
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();


logger.clear();
holder = {};
promise = getPromise(holder);

async function *loo() {
    var t = await promise;
    throw new Error(errorText);
    yield t + 'data-first';
    yield t + 'data-second';
};

const l1 = loo();

l1.next().then(fulfillSpy(logger), rejectSpy(logger));
l1.next().then(fulfillSpy(logger), rejectSpy(logger));
l1.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .isFinal();

holder.resolve('qwe');

drainMicrotasks();

assertLogger(logger)
    .rejected(new Error(errorText))
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();
holder = {};
promise = getPromise(holder);

const l2 = loo();

l2.throw(new Error('another error')).then(fulfillSpy(logger), rejectSpy(logger));
l2.next().then(fulfillSpy(logger), rejectSpy(logger));
l2.next().then(fulfillSpy(logger), rejectSpy(logger));

holder.resolve('abc');

drainMicrotasks();

assertLogger(logger)
    .rejected(new Error('another error'))
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();
holder = {};
promise = getPromise(holder);

const l3 = loo();

l3.return(someText).then(fulfillSpy(logger), rejectSpy(logger));
l3.next().then(fulfillSpy(logger), rejectSpy(logger));
l3.next().then(fulfillSpy(logger), rejectSpy(logger));

holder.resolve(resolveText);

drainMicrotasks();

assertLogger(logger)
    .fullfilledDone(someText)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

async function *moo() {
    throw new Error(errorText);
    yield t + 'data-first';
    yield t + 'data-second';
};

const m1 = moo();

m1.next().then(fulfillSpy(logger), rejectSpy(logger));
m1.next().then(fulfillSpy(logger), rejectSpy(logger));
m1.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .rejected(new Error(errorText))
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

const m2 = moo();

m2.throw(new Error('another error')).then(fulfillSpy(logger), rejectSpy(logger));
m2.next().then(fulfillSpy(logger), rejectSpy(logger));
m2.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .rejected(new Error('another error'))
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

const m3 = moo();

m3.return(someText).then(fulfillSpy(logger), rejectSpy(logger));
m3.next().then(fulfillSpy(logger), rejectSpy(logger));
m3.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilledDone(someText)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

async function* noo() {
  const x = Promise.resolve(1);
  const y = Promise.resolve(2);
  
  const fromX = yield x;
  return y;
}

const n1 = noo();

let value1 = Promise.resolve("a");
let value2 = Promise.resolve("b");

n1.next(value1).then(fulfillSpy(logger), rejectSpy(logger));
n1.next(value2).then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .fullfilledDone(2)
    .isFinal();

logger.clear();

const n2 = noo();

value1 = Promise.resolve("a");
value2 = Promise.resolve("b");

n2.return(value1).then(fulfillSpy(logger), rejectSpy(logger));
n2.next(value2).then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilledDone('a')
    .fullfilledDone()
    .isFinal();

logger.clear();

promise = getPromise(holder);

async function *ooo() {
    yield promise; 
}

const o1 = ooo();

o1.next(value1).then(fulfillSpy(logger), rejectSpy(logger));

holder.reject("a");

drainMicrotasks();

assertLogger(logger)
    .rejected('a')
    .isFinal();

logger.clear();

promise = getPromise(holder);

async function *roo() {
    try {
        yield promise; 
    } catch (e) {
        yield e;
    }
}

const r1 = roo();
value1 = 'value-1';
value2 = 'value-2';
value3 = 'value-3';

r1.next().then(fulfillSpy(logger), rejectSpy(logger));
r1.next(value1).then(fulfillSpy(logger), rejectSpy(logger));
r1.next(value1).then(fulfillSpy(logger), rejectSpy(logger));

holder.reject("abc");

drainMicrotasks();

assertLogger(logger)
    .fullfilled('abc')
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

holder1 = {};
holder2 = {};
promise1 = getPromise(holder1);
promise2 = getPromise(holder2);

async function *poo() {
    try {
        yield promise1; 
    } catch (e) {
        yield promise2;
    }
}

const p1 = poo();

p1.next().then(fulfillSpy(logger), rejectSpy(logger));
p1.next(value3).then(fulfillSpy(logger), rejectSpy(logger));
p1.next(value3).then(fulfillSpy(logger), rejectSpy(logger));

holder1.reject(value1);
holder2.reject(value2);

drainMicrotasks();

assertLogger(logger)
    .rejected(value2)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

async function *soo() {
    yield 1;
}

const endValue = 'end-value-1';

logger.clear();

const s1 = soo();

s1.next().then(fulfillSpy(logger), rejectSpy(logger));
s1.next().then(fulfillSpy(logger), rejectSpy(logger));
s1.return(Promise.resolve(endValue)).then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .fullfilledDone()
    .fullfilledDone(endValue)
    .isFinal();

logger.clear();

const s2 = soo();

s2.next().then(fulfillSpy(logger), rejectSpy(logger));
s2.next().then(fulfillSpy(logger), rejectSpy(logger));
s2.return(Promise.reject(endValue)).then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .fullfilledDone()
    .rejected(endValue)
    .isFinal();

logger.clear();
const s3 = soo();

s3.next().then(fulfillSpy(logger), rejectSpy(logger));
s3.next().then(fulfillSpy(logger), rejectSpy(logger));
s3.return(endValue).then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .fullfilledDone()
    .fullfilledDone(endValue)
    .isFinal();

logger.clear();

const s4 = soo();
promise1 = Promise.resolve(endValue);

s4.next().then(fulfillSpy(logger), rejectSpy(logger));
s4.next().then(fulfillSpy(logger), rejectSpy(logger));
s4.throw(promise1).then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .fullfilledDone()
    .rejected(promise1)
    .isFinal();

logger.clear();

const s5 = soo();

s5.next().then(fulfillSpy(logger), rejectSpy(logger));
s5.next().then(fulfillSpy(logger), rejectSpy(logger));
s5.throw(endValue).then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .fullfilledDone()
    .rejected(endValue)
    .isFinal();

async function *too() {
    return Promise.resolve('abcd');
}

logger.clear();

const t = too();

t.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilledDone('abcd')
    .isFinal();
