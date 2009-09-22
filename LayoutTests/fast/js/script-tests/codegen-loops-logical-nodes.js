description(
"Tests loop codegen when the condition is a logical node."
);

function while_or_eq()
{
    var a = 0;
    while (a == 0 || a == 0)
        return true;
    return false;
}

shouldBeTrue("while_or_eq()");

function while_or_neq()
{
    var a = 0;
    while (a != 1 || a != 1)
        return true;
    return false;
}

shouldBeTrue("while_or_neq()");

function while_or_less()
{
    var a = 0;
    while (a < 1 || a < 1)
        return true;
    return false;
}

shouldBeTrue("while_or_less()");

function while_or_lesseq()
{
    var a = 0;
    while (a <= 1 || a <= 1)
        return true;
    return false;
}

shouldBeTrue("while_or_lesseq()");

function while_and_eq()
{
    var a = 0;
    while (a == 0 && a == 0)
        return true;
    return false;
}

shouldBeTrue("while_and_eq()");

function while_and_neq()
{
    var a = 0;
    while (a != 1 && a != 1)
        return true;
    return false;
}

shouldBeTrue("while_and_neq()");

function while_and_less()
{
    var a = 0;
    while (a < 1 && a < 1)
        return true;
    return false;
}

shouldBeTrue("while_and_less()");

function while_and_lesseq()
{
    var a = 0;
    while (a <= 1 && a <= 1)
        return true;
    return false;
}

shouldBeTrue("while_and_lesseq()");

function for_or_eq()
{
    for (var a = 0; a == 0 || a == 0; )
        return true;
    return false;
}

shouldBeTrue("for_or_eq()");

function for_or_neq()
{
    for (var a = 0; a != 1 || a != 1; )
        return true;
    return false;
}

shouldBeTrue("for_or_neq()");

function for_or_less()
{
    for (var a = 0; a < 1 || a < 1; )
        return true;
    return false;
}

shouldBeTrue("for_or_less()");

function for_or_lesseq()
{
    for (var a = 0; a <= 1 || a <= 1; )
        return true;
    return false;
}

shouldBeTrue("for_or_lesseq()");

function for_and_eq()
{
    for (var a = 0; a == 0 && a == 0; )
        return true;
    return false;
}

shouldBeTrue("for_and_eq()");

function for_and_neq()
{
    for (var a = 0; a != 1 && a != 1; )
        return true;
    return false;
}

shouldBeTrue("for_and_neq()");

function for_and_less()
{
    for (var a = 0; a < 1 && a < 1; )
        return true;
    return false;
}

shouldBeTrue("for_and_less()");

function for_and_lesseq()
{
    for (var a = 0; a <= 1 && a <= 1; )
        return true;
    return false;
}

shouldBeTrue("for_and_lesseq()");

function dowhile_or_eq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a == 0 || a == 0)
    return false;
}

shouldBeTrue("dowhile_or_eq()");

function dowhile_or_neq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a != 1 || a != 1)
    return false;
}

shouldBeTrue("dowhile_or_neq()");

function dowhile_or_less()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a < 1 || a < 1)
    return false;
}

shouldBeTrue("dowhile_or_less()");

function dowhile_or_lesseq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a <= 1 || a <= 1)
    return false;
}

shouldBeTrue("dowhile_or_lesseq()");

function dowhile_and_eq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a == 0 && a == 0)
    return false;
}

shouldBeTrue("dowhile_and_eq()");

function dowhile_and_neq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a != 1 && a != 1)
    return false;
}

shouldBeTrue("dowhile_and_neq()");

function dowhile_and_less()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a < 1 && a < 1)
    return false;
}

shouldBeTrue("dowhile_and_less()");

function dowhile_and_lesseq()
{
    var a = 0;
    var i = 0;
    do {
        if (i > 0)
            return true;
        i++;
    } while (a <= 1 && a <= 1)
    return false;
}

shouldBeTrue("dowhile_and_lesseq()");

var successfullyParsed = true;
