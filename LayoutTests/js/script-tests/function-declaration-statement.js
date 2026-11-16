description(
"This test checks that function declarations are treated as statements."
);

function f()
{
    return false;
}

function ifTest()
{
    if (true)
        function f()
        {
            return true;
        }

    return f();
}

shouldBeTrue("ifTest()");

function ifElseTest()
{
    if (false)
        return false;
    else
        function f()
        {
            return true;
        }

    return f();
}

shouldBeTrue("ifElseTest()");

function labelTest()
{
    label:
        function f()
        {
            return true;
        }

    return f();
}

shouldBeTrue("labelTest()");


function deepNesting () {
    var y = '';
    {
        function foo() { return 'abc'; }
    }
    if (true) {
        {
            {
                {
                    {
                        {
                           {
                                {
                                    {
                                        {
                                            { 
                                                let x = 'abc';
                                                y = x;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return foo(); 
};

shouldBe("deepNesting()", "'abc'");

function deepNestingForFunctionDeclaration () {
    var y = '';
    {
        {
            {
                {
                    {
                        {
                            {
                                {
                                    {
                                        {
                                            {
                                                {
                                                    {
                                                        {
                                                            function foo() { return 'abc'; }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (true) {
        {
            {
                {
                    {
                        {
                           {
                                {
                                    {
                                        {
                                            { 
                                                let x = 'abc';
                                                y = x;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return foo(); 
};

shouldBe("deepNestingForFunctionDeclaration()", "'abc'");

eval(`function deepNestingInEval () {
    var y = '';
    {
        function foo() { return 'abc'; }
    }
    if (true) {
        {
            {
                {
                    {
                        {
                           {
                                {
                                    {
                                        {
                                            { 
                                                let x = 'abc';
                                                y = x;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return foo(); 
};`);


shouldBe("deepNestingInEval()", "'abc'");

eval(`function deepNestingForFunctionDeclarationInEval () {
    var y = '';
    {
        {
            {
                {
                    {
                        {
                            {
                                {
                                    {
                                        {
                                            {
                                                {
                                                    {
                                                        {
                                                            function foo() { return 'abc'; }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (true) {
        {
            {
                {
                    {
                        {
                           {
                                {
                                    {
                                        {
                                            { 
                                                let x = 'abc';
                                                y = x;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return foo(); 
};`);

shouldBe("deepNestingForFunctionDeclarationInEval()", "'abc'");
