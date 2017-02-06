"use strict";

load("ast.js");
load("basic.js");
load("caseless_map.js");
load("lexer.js");
load("number.js");
load("parser.js");
load("random.js");
load("state.js");
load("util.js");

load("benchmark.js");

let result = runBenchmark();
print("That took " + result + " ms.");
