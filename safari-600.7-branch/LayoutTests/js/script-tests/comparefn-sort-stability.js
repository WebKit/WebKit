description(
"This tests that sort(compareFn) is a stable sort."
);

function clone(source, target) {
    for (i = 0; i < source.length; i++) {
        target[i] = source[i];
    }
}

var arr = [];
arr[0] = new Number(1);
arr[1] = new Number(2);
arr[2] = new Number(1);
arr[3] = new Number(2);

var sortArr = [];
clone(arr, sortArr);
sortArr.sort(function(a,b) { return a - b; });

shouldBe('arr[0]', 'sortArr[0]');
shouldBe('arr[1]', 'sortArr[2]');
shouldBe('arr[2]', 'sortArr[1]');
shouldBe('arr[3]', 'sortArr[3]');

// Just try again...
sortArr.sort(function(a,b) { return a - b; });
shouldBe('arr[0]', 'sortArr[0]');
shouldBe('arr[1]', 'sortArr[2]');
shouldBe('arr[2]', 'sortArr[1]');
shouldBe('arr[3]', 'sortArr[3]');
