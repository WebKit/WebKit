// Copyright (C) Copyright 2016 the v8 project authors. All rights reserved.

//@ runNoCJIT("--gcMaxHeapSize=2000000")

function shouldBe(expected, actual, msg = "") {
    if (msg)
        msg = " for " + msg;
    if (actual !== expected)
        throw new Error("bad value" + msg + ": " + actual + ". Expected " + expected);
}


let out;

async function thrower() { throw undefined; }

async function throwLoop() {
    for (let i = 0; i < 8000; i++) {
        try {
            await thrower();
        } catch (error) {
            void 0;
        }
    }
    out = 2;
}

throwLoop();

drainMicrotasks();

shouldBe(out, 2);
