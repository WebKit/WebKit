var assert = require('assert');

function almostEqual(actual, expected, precision, message)
{
    var suffiedMessage = (message ? message + ' ' : '');
    if (isNaN(expected)) {
        assert(isNaN(actual), `${suffiedMessage}expected NaN but got ${actual}`);
        return;
    }

    if (expected == 0) {
        assert.equal(actual, expected, message);
        return;
    }

    if (!precision)
        precision = 6;
    var tolerance = 1 / Math.pow(10, precision);
    var relativeDifference = Math.abs((actual - expected) / expected);
    var percentDifference = (relativeDifference * 100).toFixed(2);
    assert(relativeDifference < tolerance,
        `${suffiedMessage}expected ${expected} but got ${actual} (${percentDifference}% difference)`);
}

if (typeof module != 'undefined')
    module.exports = almostEqual;
