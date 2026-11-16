function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrowTypeError(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof TypeError))
        throw new Error('Expected TypeError!');
}

shouldBe(Array.prototype.at.length, 1);
shouldThrowTypeError(() => Array.prototype.at.call(undefined));
shouldThrowTypeError(() => Array.prototype.at.call(null));

const array = [42, 'b', true];
// intentionally go one too far to ensure that we get undefined instead of wrapping
for (let i = 0; i <= array.length; i++) {
  shouldBe(array.at(i), array[i]);
  shouldBe(array.at(-i - 1), array[array.length - i - 1]);
}
shouldBe(array.at(), array[0]);
shouldBe(array.at(null), array[0]);
shouldBe(array.at({ valueOf: () => -1 }), array[array.length - 1]);

const weirdArrayLike = { length: 1, get '0'() { return 3; }, get '1'() { throw 'oops'; } };
shouldBe(Array.prototype.at.call(weirdArrayLike, 0), 3);
shouldBe(Array.prototype.at.call(weirdArrayLike, 1), undefined);

for (const TA of [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array]) {
  shouldBe(TA.prototype.at.length, 1);
  shouldThrowTypeError(() => TA.prototype.at.call([]));

  const ta = [1, 2, 3];
  // intentionally go one too far to ensure that we get undefined instead of wrapping
  for (let i = 0; i <= ta.length; i++) {
    shouldBe(ta.at(i), ta[i]);
    shouldBe(ta.at(-i - 1), ta[ta.length - i - 1]);
  }
  shouldBe(ta.at(), ta[0]);
  shouldBe(ta.at(null), ta[0]);
  shouldBe(ta.at({ valueOf: () => -1 }), ta[ta.length - 1]);
}

shouldBe(String.prototype.at.length, 1);  
shouldThrowTypeError(() => String.prototype.at.call(undefined));  
shouldThrowTypeError(() => String.prototype.at.call(null)); 

const string = 'abc'; 
// intentionally go one too far to ensure that we get undefined instead of wrapping 
for (let i = 0; i <= string.length; i++) {  
  shouldBe(string.at(i), string[i]);  
  shouldBe(string.at(-i - 1), string[string.length - i - 1]); 
} 
shouldBe(string.at(), string[0]); 
shouldBe(string.at(null), string[0]); 
shouldBe(string.at({ valueOf: () => -1 }), string[string.length - 1]);  

const emojiPseudoString = { toString: () => '😅' };  
shouldBe(String.prototype.at.call(emojiPseudoString, 0), '\u{d83d}'); 
shouldBe(String.prototype.at.call(emojiPseudoString, -1), '\u{de05}');
