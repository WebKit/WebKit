"use strict";

load("all.js");
load("payload-gbemu-executeIteration.js");
load("payload-imaging-gaussian-blur-gaussianBlur.js");
load("payload-jsair-ACLj8C.js");
load("payload-typescript-scanIdentifier.js");
load("benchmark.js");

let result = benchmark();
print("That took " + result + " ms.");
