var assert = function (result, expected, message = "") {
  if (result !== expected) {
    throw new Error('Error in assert. Expected "' + expected + '" but was "' + result + '":' + message );
  }
};

const getPromise = promiseHolder => {
    return new Promise((resolve, reject) => {
        promiseHolder.resolve = resolve;
        promiseHolder.reject = reject;
    });
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

var logger = new Logger();
const someValue = 'some-value';
const errorMessage = 'error-message';

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

async function *foo () {
    yield '0';
    yield* asyncIter;
    yield '3';
}

let f = foo('Init');

f.next('A').then(fulfillSpy(logger), rejectSpy(logger));
f.next('B').then(fulfillSpy(logger), rejectSpy(logger));
f.next('C').then(fulfillSpy(logger), rejectSpy(logger));
f.next('D').then(fulfillSpy(logger), rejectSpy(logger));
f.next('E').then(fulfillSpy(logger), rejectSpy(logger));
f.next('iter:Finish').then(fulfillSpy(logger), rejectSpy(logger));
f.next('Finish').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .custom('next:undefined')
    .fullfilled('0')
    .custom('next:C')
    .fullfilled(undefined)
    .custom('next:D')
    .fullfilled("C")
    .custom('next:E')
    .fullfilled("D")
    .custom('next:iter:Finish')
    .fullfilled("E")
    .fullfilled("3")
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

f = foo('Init');

f.next('A').then(fulfillSpy(logger), rejectSpy(logger));
f.next('B').then(fulfillSpy(logger), rejectSpy(logger));
f.return('C').then(fulfillSpy(logger), rejectSpy(logger));
f.next('D').then(fulfillSpy(logger), rejectSpy(logger));
f.next('E').then(fulfillSpy(logger), rejectSpy(logger));
f.next('iter:Finish').then(fulfillSpy(logger), rejectSpy(logger));
f.next('Finish').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .custom('next:undefined')
    .fullfilled('0')
    .fullfilled(undefined)
    .custom('return:C')
    .fullfilledDone("C")
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

f = foo('Init');

f.next('A').then(fulfillSpy(logger), rejectSpy(logger));
f.next('B').then(fulfillSpy(logger), rejectSpy(logger));
f.throw(new Error(errorMessage)).then(fulfillSpy(logger), rejectSpy(logger));
f.next('D').then(fulfillSpy(logger), rejectSpy(logger));
f.next('E').then(fulfillSpy(logger), rejectSpy(logger));
f.next('iter:Finish').then(fulfillSpy(logger), rejectSpy(logger));
f.next('Finish').then(fulfillSpy(logger), rejectSpy(logger));

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

asyncIter = {
    [Symbol.asyncIterator]() { return this; },
    next (value) {
        customSpy(logger)('next:' + value);
        return { value: value, done: 'iter:Finish' === value };
    }
  };

async function *boo () {
    yield '0';
    yield* asyncIter;
    yield '3';
}

let b = boo('Init');

logger.clear();

b.next('A').then(fulfillSpy(logger), rejectSpy(logger));
b.next('B').then(fulfillSpy(logger), rejectSpy(logger));
b.next('C').then(fulfillSpy(logger), rejectSpy(logger));
b.next('D').then(fulfillSpy(logger), rejectSpy(logger));
b.next('iter:Finish').then(fulfillSpy(logger), rejectSpy(logger));
b.next('Finish').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .custom('next:undefined')
    .fullfilled('0')
    .custom('next:C')
    .fullfilled(undefined)
    .custom('next:D')
    .fullfilled("C")
    .custom("next:iter:Finish")
    .fullfilled("D")
    .fullfilled("3")
    .fullfilledDone(undefined)
    .isFinal();

logger.clear();

b = boo('Init');

b.next('A').then(fulfillSpy(logger), rejectSpy(logger));
b.next('B').then(fulfillSpy(logger), rejectSpy(logger));
b.throw(new Error(errorMessage)).then(fulfillSpy(logger), rejectSpy(logger));
b.next('D').then(fulfillSpy(logger), rejectSpy(logger));
b.next('iter:Finish').then(fulfillSpy(logger), rejectSpy(logger));
b.next('Finish').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .custom('next:undefined')
    .fullfilled('0')
    .fullfilled(undefined)
    .rejected('TypeError: Delegated generator does not have a \'throw\' method.')
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

asyncIter = {
    [Symbol.asyncIterator]() { return this; },
    next (value) {
        customSpy(logger)('next:' + value);
        return { value: value, done: 'iter:Finish' === value };
    },
    return (value) {
        customSpy(logger)('return:' + value);
        return { value: value, done: true };
    }
  };

async function *bar () {
    yield '0';
    yield* asyncIter;
    yield '3';
}

b = bar('Init');

logger.clear();

b.next('A').then(fulfillSpy(logger), rejectSpy(logger));
b.next('B').then(fulfillSpy(logger), rejectSpy(logger));
b.throw(new Error(errorMessage)).then(fulfillSpy(logger), rejectSpy(logger));
b.next('D').then(fulfillSpy(logger), rejectSpy(logger));
b.next('iter:Finish').then(fulfillSpy(logger), rejectSpy(logger));
b.next('Finish').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .custom('next:undefined')
    .fullfilled('0')
    .custom('return:undefined')
    .fullfilled(undefined)
    .rejected('TypeError: Delegated generator does not have a \'throw\' method.')
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

let ph = {};

asyncIter = {
    [Symbol.asyncIterator]() { return this; },
    next (value) {
        customSpy(logger)('next:' + value);
        return { value: value, done: 'iter:Finish' === value };
    },
    return (value) {
        customSpy(logger)('return:' + value);
        return { value: getPromise(ph), done: true };
    }
  };

async function *baz () {
    yield '0';
    yield* asyncIter;
    yield '3';
}

b = baz('Init');

logger.clear();

b.next('A').then(fulfillSpy(logger), rejectSpy(logger));
b.next('B').then(fulfillSpy(logger), rejectSpy(logger));
b.throw(new Error(errorMessage)).then(fulfillSpy(logger), rejectSpy(logger));
b.next('D').then(fulfillSpy(logger), rejectSpy(logger));
b.next('iter:Finish').then(fulfillSpy(logger), rejectSpy(logger));
b.next('Finish').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .custom('next:undefined')
    .fullfilled('0')
    .custom('return:undefined')
    .fullfilled(undefined)
    .rejected('TypeError: Delegated generator does not have a \'throw\' method.')
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

ph.resolve('accept');

drainMicrotasks();

assertLogger(logger)
    .custom('next:undefined')
    .fullfilled('0')
    .custom('return:undefined')
    .fullfilled(undefined)
    .rejected('TypeError: Delegated generator does not have a \'throw\' method.')
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

ph = {};

asyncIter = {
    [Symbol.asyncIterator]() { return this; },
    next (value) {
        customSpy(logger)('next:' + value);
        return { value: value, done: 'iter:Finish' === value };
    },
    return (value) {
        customSpy(logger)('return:' + value);
        return getPromise(ph);
    }
  };

async function *foobar () {
    yield '0';
    yield* asyncIter;
    yield '3';
}

fb = foobar('Init');

logger.clear();

fb.next('A').then(fulfillSpy(logger), rejectSpy(logger));
fb.next('B').then(fulfillSpy(logger), rejectSpy(logger));
fb.throw(new Error(errorMessage)).then(fulfillSpy(logger), rejectSpy(logger));
fb.next('D').then(fulfillSpy(logger), rejectSpy(logger));
fb.next('iter:Finish').then(fulfillSpy(logger), rejectSpy(logger));
fb.next('Finish').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .custom('next:undefined')
    .fullfilled('0')
    .custom('return:undefined')
    .fullfilled(undefined)
    .isFinal();

ph.resolve({ value: 'value', done: true });

drainMicrotasks();

assertLogger(logger)
    .custom('next:undefined')
    .fullfilled('0')
    .custom('return:undefined')
    .fullfilled(undefined)
    .rejected('TypeError: Delegated generator does not have a \'throw\' method.')
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();

fb = foobar('Init');

logger.clear();

fb.next('A').then(fulfillSpy(logger), rejectSpy(logger));
fb.next('B').then(fulfillSpy(logger), rejectSpy(logger));
fb.throw(new Error(errorMessage)).then(fulfillSpy(logger), rejectSpy(logger));
fb.next('D').then(fulfillSpy(logger), rejectSpy(logger));
fb.next('iter:Finish').then(fulfillSpy(logger), rejectSpy(logger));
fb.next('Finish').then(fulfillSpy(logger), rejectSpy(logger));

drainMicrotasks();

assertLogger(logger)
    .custom('next:undefined')
    .fullfilled('0')
    .custom('return:undefined')
    .fullfilled(undefined)
    .isFinal();

ph.resolve('X');

drainMicrotasks();

assertLogger(logger)
    .custom('next:undefined')
    .fullfilled('0')
    .custom('return:undefined')
    .fullfilled(undefined)
    .rejected('TypeError: Iterator result interface is not an object.')
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .fullfilledDone(undefined)
    .isFinal();
