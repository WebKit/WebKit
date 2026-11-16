//@ skip if !$jitTests
//@ skip if !$isFTLPlatform
//@ runWithoutBaseOption("default", "--slowPathAllocsBetweenGCs=10", "--jitPolicyScale=0", "--useConcurrentJIT=0", "--validateExceptionChecks=1")
'use strict';
let o = {
    x0: ()=>0,
    x1: ()=>0,
    x2: ()=>0,
};

function module(bytes) {
    let buffer = new ArrayBuffer(bytes.length);
    let view = new Uint8Array(buffer);
    for (let i = 0; i < bytes.length; ++i) {
        view[i] = bytes.charCodeAt(i);
    }
    return new WebAssembly.Module(buffer);
}

function instance(bytes, imports = {o}) {
    return new WebAssembly.Instance(module(bytes), imports);
}

function call(instance_, name) {
    return instance_.exports[name]();
}

function exports(name, instance_) {
    return { [name]: instance_.exports };
}

function run(action) {
    action();
}

function fn1() {
}
function fn1() {
}
function fn1() {
}
function fn1() {
}
function fn1() {
}
function fn1() {
}
function fn1() {
}
function fn1() {
}

try {
    (function f() {
        f();
    }());
} catch (e) {
}

let $1 = instance('\0asm\x01\0\0\0\x01\x91\x80\x80\x80\0\x04`\0\0`\0\x01\x7F`\0\x01}`\x01\x7F\x01\x7F\x03\x87\x80\x80\x80\0\x06\0\x01\x01\x02\x03\x01\x05\x84\x80\x80\x80\0\x01\x01\x01\x01\x07ë\x80\x80\x80\0\x06\x0Fzero_everything\0\0\x12test_store_to_load\0\x01\x13test_redundant_load\0\x02\x0Ftest_dead_store\0\x03\x06malloc\0\x04\x0Fmalloc_aliasing\0\x05\n\xBD\x81\x80\x80\0\x06\x9E\x80\x80\x80\0\0A\0A\x006\x02\0A\x04A\x006\x02\0A\bA\x006\x02\0A\fA\x006\x02\0\x0B\x98\x80\x80\x80\0\0A\bA\x006\x02\0A\x05C\0\0\0\x808\x02\0A\b(\x02\0\x0B\xA2\x80\x80\x80\0\x01\x02\x7FA\b(\x02\0!\0A\x05A\x80\x80\x80\x80x6\x02\0A\b(\x02\0!\x01 \0 \x01j\x0B\x9F\x80\x80\x80\0\x01\x01}A\bA\xA3Æ\x8C\x99\x026\x02\0A\x0B*\x02\0!\0A\bA\x006\x02\0 \0\x0B\x84\x80\x80\x80\0\0A\x10\x0B\xA3\x80\x80\x80\0\x01\x02\x7FA\x04\x10\x04!\0A\x04\x10\x04!\x01 \0A*6\x02\0 \x01A+6\x02\0 \0(\x02\0\x0B');

call($1, 'zero_everything');
run(() => call($1, 'zero_everything', []));
run(() => call(instance('\0asm\x01\0\0\0\x01\x88\x80\x80\x80\0\x02`\0\0`\0\x01}\x02\x96\x80\x80\x80\0\x01\x02$1\x0Ftest_dead_store\0\x01\x03\x82\x80\x80\x80\0\x01\0\x07\x87\x80\x80\x80\0\x01\x03run\0\x01\n\x9A\x80\x80\x80\0\x01\x94\x80\x80\x80\0\0\x02@\x10\0\xBCC#\0\0\0\xBCFE\r\0\x0F\x0B\0\x0B', exports('$1', $1)), 'run', []));
run(() => call($1, 'malloc_aliasing', []));
