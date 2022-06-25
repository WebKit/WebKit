description('Test that when the stack overflows, the exception goes to the last frame before the overflow');

var level = 0;
var stackLevel = 0;
var gotWrongCatch = false;

function test1()
{
    var myLevel = level;
    var dummy;

    try {
        level = level + 1;
        // Dummy code to make this funciton different from test2()
        dummy = level * level + 1;
        if (dummy == 0)
            debug('Should never get here!!!!');
    } catch(err) {
        gotWrongCatch = true;
    }

    try {
        test2();
    } catch(err) {
        stackLevel = myLevel;
    }
}

function test2()
{
    var myLevel = level;

    // Dummy code to make this funciton different from test1()
    if (gotWrongCatch)
        debug('Should never get here!!!!');

    try {
        level = level + 1;
    } catch(err) {
        gotWrongCatch = true;
    }

    try {
        test1();
    } catch(err) {
        stackLevel = myLevel;
    }
}

test1();

shouldBeFalse("gotWrongCatch");
shouldBe("(stackLevel)", "(level - 1)");
