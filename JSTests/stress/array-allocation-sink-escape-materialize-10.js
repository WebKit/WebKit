function testExceptionPath(shouldThrow) {
    let a = new Array(3);
    a[0] = 10;
    a[1] = 20;
    a[2] = 30;
    
    let len = a.length;
    
    try {
        if (shouldThrow) {
            throw new Error("Test exception");
        }
        return len + a[0];
    } catch (e) {
        return a.length + 100;
    }
}

function testNestedTryFinally(idx) {
    let a = new Array(2);
    a[0] = idx;
    a[1] = idx * 2;
    
    let len = a.length;
    
    try {
        try {
            if (idx % 7 === 0) {
                throw new Error("Inner");
            }
            return len;
        } finally {
            if (idx % 3 === 0) {
                len += a.length;
            }
        }
    } catch (e) {
        return a[0] + a[1];
    }
}

function testArrayEscapeInCatch(idx) {
    let a = new Array(4);
    for (let i = 0; i < 4; i++) {
        a[i] = i * idx;
    }
    
    let len = a.length; 
    
    try {
        if (idx % 5 === 0) {
            throw a;
        }
        return len;
    } catch (escaped) {
        return escaped.length + escaped[0];
    }
}

noInline(testExceptionPath);
noInline(testNestedTryFinally);
noInline(testArrayEscapeInCatch);

for (let i = 0; i < testLoopCount; i++) {
    testExceptionPath(i % 10 === 0);
    testNestedTryFinally(i);
    testArrayEscapeInCatch(i);
}