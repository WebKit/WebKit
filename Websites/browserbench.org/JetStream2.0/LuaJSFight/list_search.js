function firstWhere(list, fn) {
    for (var x of list) {
        if (fn(x)) {
            return x;
        }
    }
    return null;
}
nums = [1, 2, 3, 4, 5, 6, 7];
function isEven(x) { return (x & 1) == 0; }

function run()
{
    firstEven = firstWhere(nums, isEven);
    print('First even: ' + firstEven)
}

class Benchmark {
    runIteration() {
        run();
    }
}

