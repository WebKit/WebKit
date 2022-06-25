var arr = [0, 1, 2, 3];

function redefineIndices(i) {
  Object.defineProperty(arr, 0, {value: i});
  Object.defineProperty(arr, 1, {value: i, writable: true});
  Object.defineProperty(arr, 2, {value: i, enumerable: true});
  Object.defineProperty(arr, 3, {value: i, configurable: true});
}
noInline(redefineIndices);

var max = 1e5;
for (var i = 0; i <= max; i++)
  redefineIndices(i);

if (arr.some(el => el !== max))
  throw `Bad array: ${arr}`;
