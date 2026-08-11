var arr = [0, 0, 0, 0];
Object.defineProperty(arr, 0, {value: 0});
Object.defineProperty(arr, 1, {value: 1, writable: true});
Object.defineProperty(arr, 2, {value: 2, enumerable: true});
Object.defineProperty(arr, 3, {value: 3, configurable: true});

var lastIndex = arr.length - 1;
function reverseTwice() {
  for (var i = 0; i <= lastIndex; i++) {
    var el = arr[lastIndex - i];
    arr[lastIndex - i] = arr[i];
    arr[i] = el;
  }
}
noInline(reverseTwice);

for (var i = 0; i < 1e5; i++)
  reverseTwice();

if (arr.some((el, i) => el !== i))
  throw `Bad array: ${arr}`;
