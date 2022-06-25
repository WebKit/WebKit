description('Test that if an arrity check causes a stack overflow, the exception goes to the right catch');

function funcWith40Args(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8,
                        arg9, arg10, arg11, arg12, arg13, arg14, arg15,
                        arg16, arg17, arg18, arg19, arg20,
                        arg21, arg22, arg23, arg24, arg25, arg26, arg27, arg28,
                        arg29, arg30, arg31, arg32, arg33, arg34, arg35,
                        arg36, arg37, arg38, arg39, arg40)
{
    debug("ERROR: Shouldn't arrive in 40 arg function!");
}

var gotRightCatch = false, gotWrongCatch1 = false, gotWrongCatch2 = false;

function test1()
{
    try {
        test2();
    } catch (err) {
        // Should get here because of stack overflow,
        // now cause a stack overflow exception due to arrity processing
        try {
            var dummy = new Float64Array(128);
        } catch(err) {
            gotWrongCatch1 = true;
        }
        
        try {
            funcWith40Args(1, 2, 3);
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
        gotWrongCatch2 = true;
    }
    
    try {
        test1();
    } catch (err) {
        // Should get here because of stack overflow,
        // now cause a stack overflow exception due to arrity processing
        try {
            funcWith40Args(1, 2, 3, 4, 5, 6);
        } catch (err2) {
            gotRightCatch = true;
        }
    }
}

test1();

shouldBeTrue("gotRightCatch");
shouldBeFalse("gotWrongCatch1");
shouldBeFalse("gotWrongCatch2");
