function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function compareArray(array1, array2) {
    if (array1.length !== array2.length)
        throw new Error(`Expected array length ${array2.length} but got array length ${array1.length}`);

    for (let i = 0; i < array1.length; i++) {
        if (array1[i] !== array2[i])
            throw new Error(`Expected ${array2[i]} but got ${array1[i]} (${i})`);
    }
}


function compareSet(set, array) {
    let setArray = Array.from(set);
    compareArray(setArray, array);
}

{
  let originalKeys = null;

  let setLike = {
    size: 100,
    has(v) {
      if (!originalKeys) {
        compareSet(set, [1, 2, 3, 4]);

        originalKeys = [...set.keys()];

        set.clear();

        set.add(11);
        set.add(22);
      }

      shouldBe(originalKeys.includes(v), true);

      originalKeys.splice(originalKeys.indexOf(v), 1);

      return true;
    },
    keys() {
      throw new Error("Unexpected call to |keys| method");
    },
  };

  let set = new Set([1, 2, 3, 4]);

  compareSet(set.difference(setLike), []);
  compareSet(set, [11, 22]);
  compareArray(originalKeys, []);
}

{
  let setLike = {
    size: 0,
    has() {
      throw new Error("Unexpected call to |has| method");
    },
    *keys() {
      compareSet(set, [1, 2, 3, 4]);

      let originalKeys = [...set.keys()];

      set.clear();

      set.add(11);
      set.add(22);

      yield* originalKeys;
    },
  };

  let set = new Set([1, 2, 3, 4]);

  compareSet(set.difference(setLike), []);
  compareSet(set, [11, 22]);
}

