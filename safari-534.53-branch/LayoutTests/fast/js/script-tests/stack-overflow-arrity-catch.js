description('Test that if an arrity check causes a stack overflow, the exception goes to the right catch');

function funcWith20Args(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8,
                        arg9, arg10, arg11, arg12, arg13, arg14, arg15,
                        arg16, arg17, arg18, arg19, arg20)
{
    debug("ERROR: Shouldn't arrive in 20 arg function!");
}

gotRightCatch = false;

function test1()
{
    try {
        test2();
    } catch (err) {
        // Should get here because of stack overflow,
        // now cause a stack overflow exception due to arrity processing
        try {
            var dummy = new RegExp('a|b|c');
        } catch(err) {
            debug('Should not get here #1!');
        }
        
        try {
            funcWith20Args(1, 2, 3);
        } catch (err2) {
            gotRightCatch = true;
        }
    }
}

function test2()
{
    try {
        var dummy = new Date();
    } catch(err) {
        debug('Should not get here #2!');
    }
    
    try {
        test1();
    } catch (err) {
        // Should get here because of stack overflow,
        // now cause a stack overflow exception due to arrity processing
        try {
            funcWith20Args(1, 2, 3, 4, 5, 6);
        } catch (err2) {
            gotRightCatch = true;
        }
    }
}

test1();

shouldBeTrue("gotRightCatch");

var successfullyParsed = true;
