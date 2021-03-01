'use strict';

if (self.importScripts) {
    self.importScripts('../resources/testharness.js');
}

function teeRepetitively(stream)
{
    const [t1, t2] = stream.tee();
    setTimeout(() => {
        teeRepetitively(t1);
    }, 0);
}

test(function() {
    const stream = new ReadableStream({ start: () => { } });
    teeRepetitively(stream);
}, "Tee a ReadableStream in worker");

done();
