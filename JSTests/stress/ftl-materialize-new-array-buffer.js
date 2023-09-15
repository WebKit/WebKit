'use strict';

const object = {};

function opt() {
    return Object.keys(object);
}

for (let i = 0; i < 1000000; i++)
    opt();

const tmp = new Array();
