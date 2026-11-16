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
  let set = new Set([1, 2, 3]);

  let setLike = {
    size: 0,
    has() {
      throw new Error("Unexpected call to |has| method");
    },
    keys() {
      return {
        get next() {
          set.clear();
          set.add(4);
          return function() {
            return {done: true};
          };
        }
      };
    },
  };

  compareSet(set.union(setLike), [4]);
}

