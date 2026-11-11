//@ runDefault("--useConcurrentJIT=1")

// Every realm evaluates the same script, so later realms share the bytecode of the first one,
// which records that it was compiled with the DFG. Their functions tier up with the quick DFG
// tier-up threshold.
const script = `
function orderTotal(prices, quantities) {
    let total = 0;
    for (let i = 0; i < prices.length; ++i)
        total += prices[i] * quantities[i];
    return total;
}
noInline(orderTotal);
orderTotal;
`;

const prices = [3, 1, 4, 1, 5];
const quantities = [2, 7, 1, 8, 2];

function runUntilOptimized(orderTotal) {
    for (let i = 0; i < 1e6 && numberOfDFGCompiles(orderTotal) < 1; ++i)
        orderTotal(prices, quantities);
    if (numberOfDFGCompiles(orderTotal) < 1)
        throw new Error("Optimized code was never installed");
}

runUntilOptimized($.createRealm().evalScript(script));
runUntilOptimized($.createRealm().evalScript(script));
