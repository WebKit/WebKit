let firstArray = [];
firstArray[0] = 1;

let secondArray = [3, 4, 5];

Object.defineProperty(firstArray, 1, {
    get: function() {
        secondArray[Symbol.isConcatSpreadable] = false;
        return 2;
    }
});

let result = firstArray.concat(secondArray);
if (result.length !== 3) {
    throw new Error("secondArray should not be spread");
}
