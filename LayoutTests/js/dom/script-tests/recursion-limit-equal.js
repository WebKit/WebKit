description('Tests hitting the recursion limit with equality comparisons. At one point this crashed due to lack of exception checking inside the engine.');

ch = 0;

function test()
{
    if (ch == 0)
        ch = document.getElementsByTagName('html');
    test();
}

debug('If the test did not crash, it has passed.');
debug('');

shouldThrow("test()");

