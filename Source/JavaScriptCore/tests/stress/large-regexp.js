if (''.match(/(,9111111111{2257483648,}[:lower:])|(ab)/))
    throw new Error("Incorrect result, should not have matched")

if (''.match(/(1{1,2147483648})|(ab)/))
    throw new Error("Incorrect result, should not have matched")

if (''.match(/(1{2147480000,}2{3648,})|(ab)/))
    throw new Error("Incorrect result, should not have matched")

if (!'1234'.match(/1{1,2147483645}2{1,2147483645}3{1,2147483645}4{1,2147483645}/))
    throw new Error("Incorrect result, should have matched")
