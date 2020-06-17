let OtherArray = $vm.createGlobalObject().Array;
if ($vm.isHavingABadTime(OtherArray))
    throw new Error();
$vm.haveABadTime(OtherArray);
if (!$vm.isHavingABadTime(OtherArray))
    throw new Error();

if ($vm.isHavingABadTime(globalThis))
    throw new Error();
$vm.haveABadTime([]);
if (!$vm.isHavingABadTime(globalThis))
    throw new Error();
