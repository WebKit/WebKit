//@ memoryHog!
//@ runDefault

var exception;
try {
    RegExp.escape('퀀'.repeat?.(2 ** 30));
} catch (e) {
    exception = e;
}
if (exception != 'RangeError: Out of memory')
  throw 'FAILED';
