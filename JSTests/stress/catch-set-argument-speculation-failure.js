"use strict";

let flag = true;
function o() {
    if (flag)
        return {x:20};
    return {y:20, x:20};
}
noInline(o);

let counter = 0;
function e() {
    if ((++counter) % 50 === 0)
        throw new Error;
}
noInline(e);

let counter2 = 0;
function e2() {
    if ((++counter2) % 2 === 0)
        throw new Error;
}
noInline(e2);

function escape(){ }
noInline(escape);

function baz(o) {
    try {
        e();
        escape(o.x);
    } catch(e) {
        escape(o.x);
        e2();
    } finally {
        o.x;
    }
}
noInline(baz);

{
    let o = {x:20};
    function run() {
        for (let i = 0; i < 1000; ++i) {
            try {
                baz(o);
            } catch { }
        }
    }
    run();
    o = {y:40, x:20};
    run();
}
