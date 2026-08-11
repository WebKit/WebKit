'use strict';

class MockLogger {
    constructor()
    {
        this._logs = [];
    }

    log(text) { this._logs.push(text); }
    error(text) { this._logs.push(text); }
}

if (typeof module != 'undefined')
    module.exports.MockLogger = MockLogger;
