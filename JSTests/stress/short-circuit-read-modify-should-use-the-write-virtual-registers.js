eval(`
    for (var i=0; i < 1000; i++) {
        const x = 1;
        let y = 42;
        try {
            y ||= x *= some;
        }
        catch { }
        finally { }
    }
`);

eval(`
    for (var i=0; i < 1000; i++) {
        const x = 1;
        try {
            foo.x ||= x *= some;
        }
        catch { }
        finally { }
    }
`);

eval(`
    for(var i = 0; i < 1000; i++) {
        const x = 1;
        try {
            foo[0] ||= x *= some;
        }
        catch { }
        finally { }
    }
`);
