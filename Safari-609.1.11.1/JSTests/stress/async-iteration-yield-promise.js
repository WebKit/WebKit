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

var logger = new Logger();
const someValue = 'some-value';
const errorMessage = 'error-message';
const promiseHolder = {};

async function * foo(val) {
    let reply = yield '0:' + val;
    reply = yield getPromise(promiseHolder);
    return 'end foo:' + reply;
}

let f = foo(someValue);

f.next(someValue + ':0').then(fulfillSpy(logger));
f.next(someValue + ':1').then(fulfillSpy(logger));
f.next(someValue + ':2').then(fulfillSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0:' + someValue)
    .isFinal();

promiseHolder.resolve('1');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0:' + someValue)
    .fullfilled('1')
    .fullfilledDone('end foo:' +  someValue + ':2')
    .isFinal();

logger.clear();

f = foo('init');

f.next('0').then(fulfillSpy(logger), rejectSpy(logger));
f.next('1').then(fulfillSpy(logger), rejectSpy(logger));
f.next('2').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0:init')
    .isFinal();

promiseHolder.reject('1');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0:init')
    .rejected('1')
    .fullfilledDone()
    .isFinal();

logger.clear();

f = foo('init');

f.next('0').then(fulfillSpy(logger)).catch(catchSpy(logger));
f.next('1').then(fulfillSpy(logger)).catch(catchSpy(logger));
f.next('2').then(fulfillSpy(logger)).catch(catchSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0:init')
    .isFinal();

promiseHolder.reject('1');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0:init')
    .fullfilledDone(undefined)
    .catched('1')
    .isFinal();

logger.clear();

const promiseHolder1 = {};
const promiseHolder2 = {};

async function * boo() {
    const promise1 = getPromise(promiseHolder1);
    const promise2 = getPromise(promiseHolder2);
    yield '0';
    yield promise1;
    yield '1';
    yield promise2;
    return 'end foo';
}

let b = boo();

b.next().then(fulfillSpy(logger)).catch(catchSpy(logger));
b.next().then(fulfillSpy(logger)).catch(catchSpy(logger));
b.next().then(fulfillSpy(logger)).catch(catchSpy(logger));
b.next().then(fulfillSpy(logger)).catch(catchSpy(logger));
b.next().then(fulfillSpy(logger)).catch(catchSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .isFinal();

promiseHolder1.resolve('#1');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('#1')
    .fullfilled('1')
    .isFinal();

promiseHolder2.resolve('#2');
drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('#1')
    .fullfilled('1')
    .fullfilled('#2')
    .fullfilledDone('end foo')
    .isFinal();


logger.clear();

b = boo();

b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .isFinal();

promiseHolder1.resolve('#1');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('#1')
    .fullfilled('1')
    .isFinal();

promiseHolder2.reject('#2');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('#1')
    .fullfilled('1')
    .rejected('#2')
    .fullfilledDone()
    .isFinal();

logger.clear();

b = boo();

b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .isFinal();

promiseHolder1.reject('#1');
promiseHolder2.resolve('#2');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .rejected('#1')
    .fullfilledDone()
    .fullfilledDone()
    .fullfilledDone()
    .isFinal();

logger.clear();

b = boo();

b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .isFinal();

promiseHolder2.reject('#2');
promiseHolder1.reject('#1');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .rejected('#1')
    .fullfilledDone()
    .fullfilledDone()
    .fullfilledDone()
    .isFinal();

async function * bar() {
  yield '0';
  yield getPromise(promiseHolder1);
  throw new Error(errorMessage);
  yield getPromise(promiseHolder2);
  return 'end foo';
}

logger.clear();
b = bar();

b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .isFinal();
 

promiseHolder1.resolve('#1');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled('#1')
    .rejected(new Error(errorMessage))
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();
b = bar();

b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .isFinal();

promiseHolder1.reject('#1');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .rejected('#1')
    .fullfilledDone()
    .fullfilledDone()
    .isFinal();

logger.clear();
b = bar();

b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.return(someValue).then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilledDone(someValue)
    .fullfilledDone(undefined)
    .isFinal();

promiseHolder1.resolve('#1');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilledDone(someValue)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();
b = bar();

b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.return(someValue).then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .isFinal();

promiseHolder1.resolve('#1');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .fullfilled("#1")
    .fullfilledDone(someValue)
    .isFinal();

logger.clear();
b = bar();

b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.next().then(fulfillSpy(logger), rejectSpy(logger));
b.return(someValue).then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .isFinal();

promiseHolder1.reject('#1');

drainMicrotasks();

assertLogger(logger)
    .fullfilled('0')
    .rejected("#1")
    .fullfilledDone(someValue)
    .isFinal();
