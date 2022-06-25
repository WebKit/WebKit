function foo() {
    for (let j = 0; j < 5; j++) {}
    new Promise(()=> {
        new Promise([].values);
    });
}

for (let i = 0; i < 10000; i++)
    foo();
