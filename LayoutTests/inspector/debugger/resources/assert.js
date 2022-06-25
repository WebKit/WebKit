function assertTrue()
{
    log("inside assertOkay");
    console.assert(true, "Should never happen.");
}

function assertFalse()
{
    log("inside assertFalse");
    console.assert(false, "Should always fail.");
}

function assertCondition(condition)
{
    log("inside assertCondition, and condition is " + !!condition);
    console.assert(condition);
}

function assertConditionWithMessage(condition, message)
{
    log("inside assertConditionWithMessage, and condition is " + !!condition);
    console.assert(condition, message);
}
