load("./driver/driver.js");

var afSimple = y => y + 1,
    afBlock = y => { y++; return y + 1;},
    afBlockWithCondition = x => { x > 0 ? x++ : x--; return x;};

checkBasicBlock(afSimple, "y + 1", ShouldNotHaveExecuted);
afSimple(1);
checkBasicBlock(afSimple, "y + 1", ShouldHaveExecuted);


checkBasicBlock(afBlock, "y++", ShouldNotHaveExecuted);
afBlock(2);
checkBasicBlock(afBlock, "y++", ShouldHaveExecuted);
checkBasicBlock(afBlock, "return y + 1", ShouldHaveExecuted);


checkBasicBlock(afBlockWithCondition,'x++', ShouldNotHaveExecuted);
afBlockWithCondition(10);
checkBasicBlock(afBlockWithCondition,'x++', ShouldHaveExecuted);
checkBasicBlock(afBlockWithCondition,'return x', ShouldHaveExecuted);
checkBasicBlock(afBlockWithCondition,'x--', ShouldNotHaveExecuted);


afBlockWithCondition(-10);
checkBasicBlock(afBlockWithCondition,'x--', ShouldHaveExecuted);

function foo1(test) {
   var f1 = () => { "hello"; }
   if (test)
       f1();
}
foo1(false);
checkBasicBlock(foo1, '() =>', ShouldNotHaveExecuted);
checkBasicBlock(foo1, '; }', ShouldNotHaveExecuted);
foo1(true);
checkBasicBlock(foo1, '() =>', ShouldHaveExecuted);
checkBasicBlock(foo1, '; }', ShouldHaveExecuted);

function foo2(test) {
   var f1 = x => { "hello"; }
   if (test)
       f1();
}
foo2(false);
checkBasicBlock(foo2, 'x =>', ShouldNotHaveExecuted);
checkBasicBlock(foo2, '; }', ShouldNotHaveExecuted);
foo2(true);
checkBasicBlock(foo2, 'x =>', ShouldHaveExecuted);
checkBasicBlock(foo2, '; }', ShouldHaveExecuted);


function foo3(test) {
   var f1 = (xyz) => { "hello"; }
   if (test)
       f1();
}
foo3(false);
checkBasicBlock(foo3, '(xyz) =>', ShouldNotHaveExecuted);
checkBasicBlock(foo3, '; }', ShouldNotHaveExecuted);
foo3(true);
checkBasicBlock(foo3, '(xyz) =>', ShouldHaveExecuted);
checkBasicBlock(foo3, '; }', ShouldHaveExecuted);
