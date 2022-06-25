var assert = function (result, expected, message = "") {
  if (result !== expected) {
    throw new Error('Error in assert. Expected "' + expected + '" but was "' + result + '":' + message );
  }
};

const Logger = function () {
    var log = [];

    this.logEvent = (type, value) => {
        log.push({ type, value});
    };
    this.logFulfilledEvent = (value) => {
        this.logEvent('fulfilled', value);
    };
    this.logRejectEvent = error => {
        this.logEvent('reject', error.toString());
    };
    this.logFinallyEvent = value => {
        this.logEvent('finally', value === undefined ? 'undefined' : value.toString());
    };
    this.logCatchEvent = value => {
        this.logEvent('catch', value.toString());
    };
    this.getLogger = () => log;

    this.clear = () => {
        log = [];
    }
};

var logger = new Logger();

const fulfillSpy = (logger => result => logger.logFulfilledEvent(result))(logger);
const rejectSpy = (logger => error => logger.logRejectEvent(error))(logger);
const catchSpy = (logger => error => logger.logCatchEvent(error))(logger);
const finallySpy = (logger => value => logger.logFinallyEvent(value))(logger);

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

            index++;
            return this;
        };

        this.rejected = function (error, message = 'on reject') {
            isNotOutOfLength();

            const msg = `step: ${index} - ${message}`;
            let step = logger[index];

            assert(step.type, 'reject', msg);
            assert(step.value, error === undefined ? 'undefined' :  error.toString(), msg);

            index++;
            return this;
        };

        this.finally = function (value, message = 'on reject') {
            isNotOutOfLength();

            const msg = `step: ${index} - ${message}`;
            let step = logger[index];

            assert(step.type, 'finally', msg);
            assert(step.value, value === undefined ? 'undefined' : value.toString(), msg);
            index++;
            return this;
        };

        this.catched = function (expectedError, message = 'on catch') {
            isNotOutOfLength();

            const msg = `step: ${index} - ${message}`;
            let step = logger[index];

            assert(step.type, 'catch', msg);
            assert(step.value, expectedError === undefined ? 'undefined' : expectedError.toString(), msg);

            index++;
            return this;
        };

        this.isFinal = function (message = '') {
            assert(index, logger.length, `expected final step: ${message}`);
        }; 
    }; 
    
    return new _assertLogger();
};

logger.clear();
Promise.resolve(1).finally(finallySpy);
drainMicrotasks();

assertLogger(logger)
    .finally()
    .isFinal();


logger.clear();
Promise.reject(1).finally(finallySpy);

drainMicrotasks();

assertLogger(logger)
    .finally()
    .isFinal();

logger.clear();
Promise.resolve(1)
        .then(VALUE => { fulfillSpy(VALUE);  return 'some-value'}, rejectSpy)
        .finally(finallySpy);

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .finally()
    .isFinal();

logger.clear();
Promise.resolve(1)
        .then(VALUE => { fulfillSpy(VALUE);  return 'some-value-1'}, rejectSpy)
        .then(VALUE => { fulfillSpy(VALUE);  return 'some-value-2'}, rejectSpy)
        .finally(finallySpy);

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .fullfilled("some-value-1")
    .finally()
    .isFinal();

logger.clear();
Promise.resolve(1)
        .then(VALUE => { fulfillSpy(VALUE);  throw new Error('error#'); return 'some-value-1'; }, rejectSpy)
        .finally(finallySpy);

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .finally()
    .isFinal();

logger.clear();
Promise.resolve(1)
        .then(VALUE => { fulfillSpy(VALUE);  throw new Error('error#'); return 'some-value-1'; }, rejectSpy)
        .then(VALUE => { fulfillSpy(VALUE);  return 'some-value-2'; }, rejectSpy)
        .finally(finallySpy);

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .rejected(new Error("error#"))
    .finally()
    .isFinal();

logger.clear();
Promise.resolve(1)
    .then(VALUE => { fulfillSpy(VALUE);  throw new Error('error#'); return 'some-value-1'; }, rejectSpy)
    .then(VALUE => { fulfillSpy(VALUE);  return 'some-value-2'; }, rejectSpy)
    .catch(catchSpy)
    .finally(finallySpy);

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .rejected(new Error("error#"))
    .finally()
    .isFinal();

logger.clear();

Promise.resolve(1)
    .then(VALUE => { fulfillSpy(VALUE);  throw new Error('error#'); return 'some-value-1'; }, rejectSpy)
    .catch(catchSpy)
    .finally(finallySpy)
    .then(fulfillSpy, rejectSpy);

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .catched(new Error('error#'))
    .finally()
    .fullfilled()
    .isFinal();

logger.clear();

Promise.resolve(1)
    .then(VALUE => { fulfillSpy(VALUE);  throw new Error('error#'); return 'some-value-1'; }, rejectSpy)
    .catch(catchSpy)
    .finally(finallySpy)
    .catch(catchSpy);

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .catched(new Error('error#'))
    .finally()
    .isFinal();

logger.clear();

Promise.resolve(1)
    .then(VALUE => { fulfillSpy(VALUE);  throw new Error('error#'); return 'some-value-1'; }, rejectSpy)
    .catch(catchSpy)
    .finally(finallySpy)
    .then(fulfillSpy, rejectSpy)
    .finally(finallySpy);

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .catched(new Error('error#'))
    .finally()
    .fullfilled()
    .finally()
    .isFinal();

logger.clear();

Promise.resolve(1)
    .then(VALUE => { fulfillSpy(VALUE);  throw new Error('error#1'); return 'some-value-1'; }, rejectSpy)
    .catch(catchSpy)
    .finally(() => { finallySpy(); throw new Error('error#2'); } )
    .then(fulfillSpy, rejectSpy)
    .finally(finallySpy);

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .catched(new Error('error#1'))
    .finally()
    .rejected(new Error('error#2'))
    .finally()
    .isFinal();

logger.clear();

Promise.resolve(1)
    .then(VALUE => { fulfillSpy(VALUE);  throw new Error('error#1'); return 'some-value-1'; }, rejectSpy)
    .catch(catchSpy)
    .finally(() => { finallySpy(); throw new Error('error#2'); } )
    .catch(catchSpy)
    .finally();

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .catched(new Error('error#1'))
    .finally()
    .catched(new Error('error#2'))
    .isFinal();

logger.clear();

Promise.resolve(1)
    .then(VALUE => { fulfillSpy(VALUE); return 'some-value-1'; })
    .finally(VALUE => { finallySpy(VALUE); return 'value'; } )
    .then(fulfillSpy, rejectSpy)
    .finally(finallySpy);

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .finally()
    .fullfilled('some-value-1')
    .finally()
    .isFinal();

logger.clear();

Promise.resolve(1)
    .then(VALUE => { fulfillSpy(VALUE);  return 'some-value-1'; })
    .finally(VALUE => { finallySpy(VALUE); return 'value'; } )
    .then(fulfillSpy)
    .finally(finallySpy);

drainMicrotasks();

assertLogger(logger)
    .fullfilled(1)
    .finally()
    .fullfilled('some-value-1')
    .finally()
    .isFinal();

logger.clear();

Promise.resolve(1)
    .finally(VALUE => { finallySpy(VALUE); return 'value'; } )
    .then(fulfillSpy, rejectSpy)
    .finally(finallySpy);

drainMicrotasks();

assertLogger(logger)
    .finally()
    .fullfilled(1)
    .finally()
    .isFinal();

logger.clear();

Promise.reject(1)
    .then(fulfillSpy, VALUE => { rejectSpy(VALUE);  return 'some-value-1'; })
    .finally(VALUE => { finallySpy(VALUE); return 'value'; } )
    .then(fulfillSpy)
    .finally(finallySpy);

drainMicrotasks();

assertLogger(logger)
    .rejected(1)
    .finally()
    .fullfilled('some-value-1')
    .finally()
    .isFinal();

logger.clear();

var obj = {};

var p = Promise.resolve(obj);

p.finally(function () {
  finallySpy(arguments.length);
  return {};
}).then(function (x) {
    fulfillSpy(x === obj);
});

drainMicrotasks();

assertLogger(logger)
    .finally(0)
    .fullfilled(true)
    .isFinal(); 


logger.clear();

p = Promise.reject(obj);

p.finally(function () {
  finallySpy(arguments.length);
  return {};
}).then(function (x) {
    fulfillSpy(x === obj);
}, function (x) {
    rejectSpy(x === obj);
});

drainMicrotasks();

assertLogger(logger)
    .finally(0)
    .rejected(true)
    .isFinal(); 
