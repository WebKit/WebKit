// jsc always has arguments, but in browser we do not have arguments, so
// ignore this test in run-jsc-stress-tests, but run in run-webkit-tests.
//@ skip

description('Tests for ES6 arrow function lexical bind of arguments on top level');

let foo = () => arguments;
let boo = () => arguments[0];
let bar = error => error ? arguments : 'no-error';

shouldThrow('foo()');
shouldThrow('boo()');
shouldThrow('bar(true)');
shouldBe('bar(false)', '"no-error"');

var successfullyParsed = true;
