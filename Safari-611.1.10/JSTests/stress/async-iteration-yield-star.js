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
    this.logCustomEvent = event => {
        this.logEvent('custom', event, false);
    };
    this.getLogger = () => log;

    this.clear = () => {
        log = [];
    }
};

const fulfillSpy = logger => result => logger.logFulfilledEvent(result.value, result.done);
const rejectSpy = logger => error => logger.logRejectEvent(error);
const catchSpy = logger => error => logger.logCatchEvent(error);
const customSpy = logger => event => logger.logCustomEvent(event);

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

        this.custom = function (expectedValue, message = 'on custom event') {

            const msg = `step: ${index} - ${message}`;
            let step = logger[index];

            assert(step.type, 'custom', msg);
            assert(step.value, expectedValue, msg);
            assert(step.done, false, msg);

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

var logger = new Logger();
const someValue = 'some-value';
const errorMessage = 'error-message';

async function * foo(value) {
  let re = yield '1:' + value;
  re = yield '2:' + re;
  re = yield '3:' + re;
  return 'end foo:' + re;
}

async function * boo(value) {
  let reply = yield '0:' + value;
  reply = yield* foo(reply);
  yield '4:' + reply;
}

var b = boo('init');

b.next('0').then(fulfillSpy(logger));
b.next('1').then(fulfillSpy(logger));
b.next('2').then(fulfillSpy(logger));
b.next('3').then(fulfillSpy(logger));
b.next('4').then(fulfillSpy(logger));
b.next('5').then(fulfillSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0:init')
    .fullfilled('1:1')
    .fullfilled('2:2')
    .fullfilled('3:3')
    .fullfilled('4:end foo:4')
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();
var b2 = boo(':value');

b2.next(':0').then(fulfillSpy(logger));
b2.next(':1').then(fulfillSpy(logger), rejectSpy(logger));
b2.return(someValue).then(fulfillSpy(logger), rejectSpy(logger));
b2.next(':2').then(fulfillSpy(logger));
b2.next(':3').then(fulfillSpy(logger));
b2.next(':4').then(fulfillSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0::value')
    .fullfilled('1::1')
    .fullfilledDone(someValue)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();
var b2 = boo('#value');

b2.next('#0').then(fulfillSpy(logger), rejectSpy(logger));
b2.next('#1').then(fulfillSpy(logger), rejectSpy(logger));
b2.next('#2').then(fulfillSpy(logger), rejectSpy(logger));
b2.throw(new Error(errorMessage)).then(fulfillSpy(logger), rejectSpy(logger));
b2.next('#3').then(fulfillSpy(logger), rejectSpy(logger));
b2.next('#4').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0:#value')
    .fullfilled('1:#1')
    .fullfilled('2:#2')
    .rejected(new Error(errorMessage))
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

async function * bar() {
  yield '1';
  yield '2';
  throw new Error(errorMessage);
  yield '3';
  return 'end foo';
}

async function * baz() {
  yield '0';
  yield* bar();
  yield '4';
}

logger.clear();
var bz1 = baz();

bz1.next().then(fulfillSpy(logger), rejectSpy(logger));
bz1.next().then(fulfillSpy(logger), rejectSpy(logger));
bz1.next().then(fulfillSpy(logger), rejectSpy(logger));
bz1.next().then(fulfillSpy(logger), rejectSpy(logger));
bz1.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .fullfilled('2')
    .rejected(new Error(errorMessage))
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();
let promiseHolder = {};

async function *joo() {
    yield '1';
    yield getPromise(promiseHolder);
}

async function *goo () {
    yield '0';
    yield* joo();
    yield '3';
}

let g = goo();

g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .isFinal();

promiseHolder.resolve('2');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .fullfilled('2')
    .fullfilled('3')
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

g = goo();

g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .isFinal();

promiseHolder.reject('#2');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .rejected('#2')
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

g = goo();

g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.return(someValue).then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .fullfilledDone(someValue)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

g = goo();

g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.return(someValue).then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .isFinal();

promiseHolder.resolve('#2');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .fullfilled('#2')
    .fullfilledDone(someValue)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

g = goo();

g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.return(someValue).then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .isFinal();

promiseHolder.reject('#2');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .rejected('#2')
    .fullfilledDone(someValue)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();
g = goo();

g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.throw(new Error(errorMessage)).then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .rejected(new Error(errorMessage))
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

promiseHolder.resolve('#2');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .rejected(new Error(errorMessage))
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();
g = goo();

g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.throw(new Error(errorMessage)).then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .isFinal();

promiseHolder.resolve('#2');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .fullfilled('#2')
    .rejected(new Error(errorMessage))
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

g = goo();

g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.throw(new Error(errorMessage)).then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));
g.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .isFinal();

promiseHolder.reject('#2');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .rejected('#2')
    .rejected(new Error(errorMessage))
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

async function *koo() {
    yield '1';
    await getPromise(promiseHolder);
}

async function *loo () {
    yield '0';
    yield* joo();
    yield '3';
}

let l = loo();

l.next().then(fulfillSpy(logger), rejectSpy(logger));
l.next().then(fulfillSpy(logger), rejectSpy(logger));
l.next().then(fulfillSpy(logger), rejectSpy(logger));
l.next().then(fulfillSpy(logger), rejectSpy(logger));
l.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

 assertLogger(logger)
    .fullfilled("0")
    .fullfilled("1")
    .isFinal();

promiseHolder.resolve('#2');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .fullfilled('#2')
    .fullfilled("3")
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();
l = loo();

l.next().then(fulfillSpy(logger), rejectSpy(logger));
l.next().then(fulfillSpy(logger), rejectSpy(logger));
l.next().then(fulfillSpy(logger), rejectSpy(logger));
l.next().then(fulfillSpy(logger), rejectSpy(logger));
l.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled("0")
    .fullfilled("1")
    .isFinal();

promiseHolder.reject('#2');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('1')
    .rejected('#2')
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

let asyncIter = {
    [Symbol.asyncIterator]() { return this; },
    next (value) {
        customSpy(logger)('next:' + value);
        return { value: value, done: 'iter:Finish' === value };
    },
    throw (error) {
        customSpy(logger)('throw:' + error);
        return error;
    },
    return(value) {
        customSpy(logger)('return:' + value);
        return { value: value, done: true };
    }
  };

async function *moo () {
    yield '0';
    yield* asyncIter;
    yield '3';
}

let m = moo('Init');

m.next('A').then(fulfillSpy(logger), rejectSpy(logger));
m.next('B').then(fulfillSpy(logger), rejectSpy(logger));
m.next('C').then(fulfillSpy(logger), rejectSpy(logger));
m.next('D').then(fulfillSpy(logger), rejectSpy(logger));
m.next('E').then(fulfillSpy(logger), rejectSpy(logger));
m.next('iter:Finish').then(fulfillSpy(logger), rejectSpy(logger));
m.next('Finish').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .custom('next:undefined')
    .fullfilled('0')
    .custom('next:C')
    .fullfilled(undefined)
    .custom('next:D')
    .fullfilled('C')
    .custom('next:E')
    .fullfilled('D')
    .custom('next:iter:Finish')
    .fullfilled('E')
    .fullfilled('3')
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

m = moo('Init');

m.next('A').then(fulfillSpy(logger), rejectSpy(logger));
m.next('B').then(fulfillSpy(logger), rejectSpy(logger));
m.return('C').then(fulfillSpy(logger), rejectSpy(logger));
m.next('D').then(fulfillSpy(logger), rejectSpy(logger));
m.next('E').then(fulfillSpy(logger), rejectSpy(logger));
m.next('iter:Finish').then(fulfillSpy(logger), rejectSpy(logger));
m.next('Finish').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .custom('next:undefined')
    .fullfilled('0')
    .fullfilled(undefined)
    .custom('return:C')
    .fullfilledDone('C')
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

m = moo('Init');

m.next('A').then(fulfillSpy(logger), rejectSpy(logger));
m.next('B').then(fulfillSpy(logger), rejectSpy(logger));
m.throw(new Error(errorMessage)).then(fulfillSpy(logger), rejectSpy(logger));
m.next('D').then(fulfillSpy(logger), rejectSpy(logger));
m.next('E').then(fulfillSpy(logger), rejectSpy(logger));
m.next('iter:Finish').then(fulfillSpy(logger), rejectSpy(logger));
m.next('Finish').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .custom('next:undefined')
    .fullfilled('0')
    .custom('throw:' + new Error(errorMessage))
    .fullfilled(undefined)
    .custom('next:D')
    .fullfilled(undefined)
    .custom('next:E')
    .fullfilled('D')
    .custom('next:iter:Finish')
    .fullfilled('E')
    .fullfilled('3')
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

async function* noo() {
  try {
    await Promise.reject("doop");
  } finally {
    yield* [1, 2, 3]; // Is this line reachable in this implementation?
  }
}

const n = noo();

n.next().then(fulfillSpy(logger), rejectSpy(logger));
n.next().then(fulfillSpy(logger), rejectSpy(logger));
n.next().then(fulfillSpy(logger), rejectSpy(logger));
n.next().then(fulfillSpy(logger), rejectSpy(logger));
n.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .fullfilled(2)
    .fullfilled(3)
    .rejected('doop');