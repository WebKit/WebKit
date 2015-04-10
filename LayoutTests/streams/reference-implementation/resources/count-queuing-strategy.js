// FIXME: Remove this file when implemented in WebCore.

function typeIsObject(x) {
  return (typeof x === 'object' && x !== null) || typeof x === 'function';
}

function CountQueuingStrategy(obj) {
    if (!typeIsObject(obj)) {
      throw new TypeError('Parameter must be an object.');
    }

    highWaterMark = Number(obj.highWaterMark);

    if (Number.isNaN(highWaterMark)) {
      throw new TypeError('highWaterMark must be a number.');
    }
    if (highWaterMark < 0) {
      throw new RangeError('highWaterMark must be nonnegative.');
    }

    this._cqsHighWaterMark = highWaterMark;
}

CountQueuingStrategy.prototype = {
    shouldApplyBackpressure: function(queueSize) {
        if (!typeIsObject(this)) {
            throw new TypeError('CountQueuingStrategy.prototype.shouldApplyBackpressure can only be applied to objects');
        }
        if (!Object.prototype.hasOwnProperty.call(this, '_cqsHighWaterMark')) {
            throw new TypeError('CountQueuingStrategy.prototype.shouldApplyBackpressure can only be applied to a ' +
                                'CountQueuingStrategy');
        }

        return queueSize > this._cqsHighWaterMark;
    },
    size: function(chunk) {
        return 1;
    }
}
