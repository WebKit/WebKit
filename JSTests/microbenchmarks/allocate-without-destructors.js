let arr = []

for (let i = 0; ; ++i) {
    arr.push({ i })
    if (i % 5 == 0) {
        arr = arr.slice(i % 7, arr.length - i % 3)
    }
    // print(arr.length)

    // if (i % 1000 == 0)
    //     $vm.gcSweepAsynchronously()
}