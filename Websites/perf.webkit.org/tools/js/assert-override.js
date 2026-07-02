const assert = require('assert');

function makeConsoleAssertThrow() 
{
    console.assert = assert.ok;
}

module.exports.makeConsoleAssertThrow = makeConsoleAssertThrow;