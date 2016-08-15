var a = 42;

function change(value)
{
    a = value;
}

export { a }
export { a as b }
export { change }
