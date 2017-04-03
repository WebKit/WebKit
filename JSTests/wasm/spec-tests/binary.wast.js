
// binary.wast:1
let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00");

// binary.wast:2
let $2 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00");

// binary.wast:3
let $3 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00");
let $M1 = $3;

// binary.wast:4
let $4 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00");
let $M2 = $4;

// binary.wast:6
assert_malformed("");

// binary.wast:7
assert_malformed("\x01");

// binary.wast:8
assert_malformed("\x00\x61\x73");

// binary.wast:9
assert_malformed("\x61\x73\x6d\x00");

// binary.wast:10
assert_malformed("\x6d\x73\x61\x00");

// binary.wast:11
assert_malformed("\x6d\x73\x61\x00\x01\x00\x00\x00");

// binary.wast:12
assert_malformed("\x6d\x73\x61\x00\x00\x00\x00\x01");

// binary.wast:14
assert_malformed("\x00\x61\x73\x6d");

// binary.wast:15
assert_malformed("\x00\x61\x73\x6d\x01");

// binary.wast:16
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00");

// binary.wast:17
assert_malformed("\x00\x61\x73\x6d\x0d\x00\x00\x00");

// binary.wast:18
assert_malformed("\x00\x61\x73\x6d\x0e\x00\x00\x00");

// binary.wast:19
assert_malformed("\x00\x61\x73\x6d\x00\x00\x00\x01");
