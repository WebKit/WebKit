description(
"Tests that the DFG treats the operand to PutScopedVar as escaping in an unconstrained way."
);

function sum(nums) {
    var total = 0;
    nums.forEach(function (num) {
        total += num;
    });
    return total;
}

for (var i = 0; i < 200; ++i)
    shouldBe("sum([2147483646, 2147483644])", "4294967290");

