Object.defineProperty(Function.prototype, 'prototype', {
    get: function () {
        throw new Error('hello');
    }
});

new Promise(resolve => {
    new resolve();
});
