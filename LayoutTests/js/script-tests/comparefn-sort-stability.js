description(
"This tests that sort(compareFn) is a stable sort."
);

var count = 100;

var ones = [];
for (var i = 0; i < count; ++i)
	ones.push(new Number(1));

var twos = [];
for (var i = 0; i < count; ++i)
	twos.push(new Number(2));

var array = [];
for (var i = 0; i < count; ++i) {
	array.push(ones[i]);
	array.push(twos[i]);
}

array.sort(function(a,b) { return a - b; });
for (var i = 0; i < count; ++i)
	shouldBe('array[i]', 'ones[i]');

for (var i = 0; i < count; ++i)
	shouldBe('array[count + i]', 'twos[i]');
