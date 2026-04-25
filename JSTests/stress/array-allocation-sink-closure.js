for (let i = 0; i < testLoopCount; i++) {
    function func(arg_array) {
      let new_array = (() => new Array(8))();
      new_array[0] = {};
      arg_array[0] = new_array[0];
      var obj = {'f': arg_array[1] ? new_array : 42};
    }
    for (let j = 0; j < 2; j++) {
        func([1.1, 2.2]);
    }
}
