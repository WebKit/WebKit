function shouldThrowTypeError(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof TypeError))
        throw new Error('Expected TypeError!');
}

shouldThrowTypeError(() => { Array.prototype.push.call(''); });
shouldThrowTypeError(() => { Array.prototype.push.call('', 1); });
shouldThrowTypeError(() => { Array.prototype.push.call('abc'); });
shouldThrowTypeError(() => { Array.prototype.push.call('abc', 1); });

shouldThrowTypeError(() => { Array.prototype.pop.call(''); });
shouldThrowTypeError(() => { Array.prototype.pop.call('abc'); });

shouldThrowTypeError(() => { Array.prototype.shift.call(''); });
shouldThrowTypeError(() => { Array.prototype.shift.call('', 1); });
shouldThrowTypeError(() => { Array.prototype.shift.call('abc'); });
shouldThrowTypeError(() => { Array.prototype.shift.call('abc', 1); });

shouldThrowTypeError(() => { Array.prototype.unshift.call(''); });
shouldThrowTypeError(() => { Array.prototype.unshift.call('abc'); });
