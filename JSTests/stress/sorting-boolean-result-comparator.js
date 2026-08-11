function checkArray(array) {
    array = array.map((value, index) => { return { value, index }; });
    array = array.sort((a, b) => b.value <= a.value);

    for (let i = 1; i < array.length; i++) {
        if (array[i].value < array[i - 1].value)
            throw new Error();

        if (array[i].value == array[i - 1].value && array[i].index <= array[i - 1].index)
            throw new Error();
    }
}

checkArray([7,4,2,0,5,5,4,3,9]);
checkArray([1,0,1]);
