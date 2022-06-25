try {
    $vm.haveABadTime();
    const ten = 10;
    const s = ten.toLocaleString().repeat(2 ** 30 - 1);
    [s].toString();
} catch { }
