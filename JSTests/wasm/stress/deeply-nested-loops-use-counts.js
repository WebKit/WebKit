import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

function generateNestedLoops(depth, numAccumulators) {
    let wat = `(module
  (func (export "nestedLoops") (param $iterations i32) (result i32)
`;

    // Declare local variables for loop counters
    for (let i = 0; i < depth; i++)
        wat += `    (local $i${i} i32)\n`;

    // Declare accumulator variables for register pressure
    for (let i = 0; i < numAccumulators; i++)
        wat += `    (local $acc${i} i32)\n`;
    wat += `\n`;

    let loopCode = '';

    // Generate opening of all nested loops
    for (let i = 0; i < depth; i++)
        loopCode += `    ${'  '.repeat(i)}(loop $loop${i}\n`;

    // Generate innermost body - increment all accumulators with different amounts
    for (let i = 0; i < numAccumulators; i++)
        loopCode += `    ${'  '.repeat(depth)}(local.set $acc${i} (i32.add (local.get $acc${i}) (i32.const ${i + 1})))\n`;

    // Generate closing of all nested loops (in reverse)
    for (let i = depth - 1; i >= 0; i--) {
        // Increment counter
        loopCode += `    ${'  '.repeat(i)}  (local.set $i${i} (i32.add (local.get $i${i}) (i32.const 1)))\n`;
        // Loop condition - continue if counter < iterations
        loopCode += `    ${'  '.repeat(i)}  (br_if $loop${i} (i32.lt_u (local.get $i${i}) (local.get $iterations)))\n`;
        // Reset counter for next outer loop iteration (except for the outermost loop)
        if (i > 0)
            loopCode += `    ${'  '.repeat(i)}  (local.set $i${i} (i32.const 0))\n`;
        // Close the loop
        loopCode += `    ${'  '.repeat(i)})\n`;
    }

    wat += loopCode;

    // Return the sum of all accumulators
    if (numAccumulators === 0)
        wat += `    (i32.const 0)\n`;
    else if (numAccumulators === 1)
        wat += `    (local.get $acc0)\n`;
    else {
        // Generate a left-associative nested chain of additions
        // ((acc0 + acc1) + acc2) + acc3) ...
        wat += `    `;
        for (let i = 1; i < numAccumulators; i++) {
            wat += `(i32.add `;
        }
        wat += `(local.get $acc0)`;
        for (let i = 1; i < numAccumulators; i++) {
            wat += ` (local.get $acc${i}))`;
        }
        wat += `\n`;
    }

    wat += `  )
)`;
    return wat;
}

async function test() {
    const depth = 50;
    const numAccumulators = 40;
 
    const wat = generateNestedLoops(depth, numAccumulators);

    // Compile and instantiate the module
    const instance = await instantiate(wat, {}, {});

    // Calculate expected result: sum of 1+2+3+...+numAccumulators = n*(n+1)/2
    const expectedSum = (numAccumulators * (numAccumulators + 1)) / 2;

    for (let i = 0; i < wasmTestLoopCount; i++) {
        const result1 = instance.exports.nestedLoops(1);
        assert.eq(result1, expectedSum);
    }
}

await assert.asyncTest(test());
