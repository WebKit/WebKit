description(
'Tests that code generation of statement lists properly reference counts registers.'
);

function f()
{
    for(; ; i++) {
        a = 0;
        
        if (1)
            return true;
    }
}

shouldBeTrue("f()");

var successfullyParsed = true;
