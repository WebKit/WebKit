description(
'This test checks break and continue behaviour in the presence of multiple labels.'
);

function test1()
{
    var s = "";
    
    a:
    b:
    for (var i = 1; i < 10; i++) {
       if (i == 4)
            continue a;
       s += i;
    }
    
    return s;
}

shouldBe("test1()", "'12356789'");

function test2()
{
    var s = "";
    
    a:
    b:
    for (var i = 1; i < 10; i++) {
        if (i == 4)
            break a;
        s += i;
    }
    
    return s;
}

shouldBe("test2()", "'123'");

function test3()
{
    var i;
    for (i = 1; i < 10; i++) {
        try {
            continue;
        } finally {
            innerLoop:
            while (1) {
                break innerLoop;
            }
        }
    }
    
    return i;
}

shouldBe("test3()", "10");

function test4()
{
    var i = 0;
    
    a:
    i++;
    while (1) {
        break;
    }
    
    return i;
}

shouldBe("test4()", "1");

function test5()
{
    var i = 0;
    
    switch (1) {
    default:
        while (1) {
            break;
        }
        i++;
    }
    
    return i;
}

shouldBe("test5()", "1");

var successfullyParsed = true;
