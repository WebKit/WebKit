function log(s)
{
}

// FIXME: Clean up.
// FIXME: Can't use global resolve or built-in functions by name.
// FIXME: Rules about integer truncation.
// FIXME: Need to support missing comparator.
var bottom_up_merge_sort = (function () {
	return function bottom_up_merge_sort(comparator)
	{
		var array = this;
		var length = array.length;
		var copy = [ ];

		// Remove holes and undefineds.
		var undefinedCount = 0;
		for (var p in array) {
			var value = array[p];
			if (value === undefined) {
				++undefinedCount;
				continue;
			}
			copy[copy.length] = value;
		}

		var n = copy.length;
		var a = copy;
		var b = array;

		for (var width = 1; width < n; width = 2 * width) {
			for (var i = 0; i < n; i = i + 2 * width) {
				var iLeft = i;
				var iRight = Math.min(i + width, n);
				var iEnd = Math.min(i + 2 * width, n);
				var i0 = iLeft;
				var i1 = iRight;
				var j;

				for (j = iLeft; j < iEnd; j++) {
					if (i0 < iRight && (i1 >= iEnd || comparator(a[i0], a[i1]) < 0)) {
						b[j] = a[i0];
						i0 = i0 + 1;
					} else {
						b[j] = a[i1];
						i1 = i1 + 1;
					}
				}
			}

			var tmp = a;
			a = b;
			b = tmp;
		}

	    if (a != array) {
		    for(var i = 0; i < a.length; i++)
		        array[i] = a[i];
	    }

 		// Restore holes and undefineds. Result should be [ values..., undefines..., holes... ].
		for (var i = 0; i < undefinedCount; ++i)
			array[array.length] = undefined;
		array.length = length;
	}
})();

function obfuscatedAMinusB(a, b)
{
	var A = a;
	var B = b;
	return A - B;
}

function aMinusB(a, b)
{
	return a - b;
}

var sortFunctions = [ Array.prototype.sort, bottom_up_merge_sort ];
var comparators = [ aMinusB, obfuscatedAMinusB ];

function verify(reference, array)
{
	for (var i in reference) {
		if (array[i] !== reference[i]) {
			log("SORT FAIL:");
			log("reference: " + reference);
			log("array: " + array);
			return;
		}
	}

	if (reference.length != array.length) {
			log("SORT FAIL:");
			log("reference.length: " + reference.length);
			log("array.length: " + array.length);
	}
}

function benchmark(f, c, array)
{
	var start = new Date;
	f.call(array, c);
	var end = new Date;

	// Our time resolution is not very good, so we round down small numbers to
	// zero in order to show that they are not meaningful.
	var result = end - start;
	if (result < 2)
		result = 0;

	log(array.length / 1024 + "k:\t" + result + "ms");
}

function makeArrays()
{
	var arrays = [ ];
	var min = 1024;
	var max = 4 * 1024;
	for (var count = min; count <= max; count *= 2) {
		var array = [ ];
		for (var i = 0; i < count; ++i)
			array[i] = Math.floor(Math.random() * count);
		arrays.push(array);
	}

	arrays.push(new Array(max));

	arrays.push((function() {
		var a = [ ];
		a[max - 1] = 1;
		return a;
	})());

//	arrays.push((function() {
//		var a = [ ];
//		a[Math.pow(2, 21) - 1] = 1;
//		return a;
//	})());

	return arrays;
}

(function main() {
	var arrays = makeArrays();
	for (var c of comparators) {
		log("\n" + "===== " + c.name + " =====");

		for (var f of sortFunctions) {
			log("\n" + f.name);

			for (var array of arrays) {
				var copy = array.slice();
				benchmark(f, c, copy);
				verify(Array.prototype.sort.call(array.slice(), c), copy);
			}
		}
	}
})();
