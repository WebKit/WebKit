description('Tests for ES6 method syntax in classes and object literals with keyword names');

// Tests keywords / reserved words, and also some non-reserved words with special meaning (in, of, get, set).

class ClassWithKeywordMethodNames {
    constructor() { } // NOTE: This has semantic meaning here, but is allowed.
    abstract() { }
    arguments() { }
    boolean() { }
    break() { }
    byte() { }
    case() { }
    catch() { }
    char() { }
    class() { }
    const() { }
    continue() { }
    debugger() { }
    default() { }
    delete() { }
    do() { }
    double() { }
    else() { }
    enum() { }
    eval() { }
    export() { }
    extends() { }
    false() { }
    final() { }
    finally() { }
    float() { }
    for() { }
    function() { }
    get() { }
    goto() { }
    if() { }
    implements() { }
    import() { }
    in() { }
    instanceof() { }
    int() { }
    interface() { }
    let() { }
    long() { }
    native() { }
    new() { }
    null() { }
    package() { }
    private() { }
    protected() { }
    public() { }
    of() { }
    return() { }
    set() { }
    short() { }
    static() { }
    super() { }
    switch() { }
    synchronized() { }
    this() { }
    throw() { }
    throws() { }
    transient() { }
    true() { }
    try() { }
    typeof() { }
    var() { }
    void() { }
    volatile() { }
    while() { }
    with() { }
    yield() { }
};

class ClassWithKeywordStaticMethodNames {
    static constructor() { }
    static abstract() { }
    // FIXME: <https://webkit.org/b/152985> ES6: Classes: Should be allowed to create a static method with name "arguments"
    // static arguments() { }
    static boolean() { }
    static break() { }
    static byte() { }
    static case() { }
    static catch() { }
    static char() { }
    static class() { }
    static const() { }
    static continue() { }
    static debugger() { }
    static default() { }
    static delete() { }
    static do() { }
    static double() { }
    static else() { }
    static enum() { }
    static eval() { }
    static export() { }
    static extends() { }
    static false() { }
    static final() { }
    static finally() { }
    static float() { }
    static for() { }
    static function() { }
    static get() { }
    static goto() { }
    static if() { }
    static implements() { }
    static import() { }
    static in() { }
    static instanceof() { }
    static int() { }
    static interface() { }
    static let() { }
    static long() { }
    static native() { }
    static new() { }
    static null() { }
    static package() { }
    static private() { }
    static protected() { }
    static public() { }
    static of() { }
    static return() { }
    static set() { }
    static short() { }
    static static() { }
    static super() { }
    static switch() { }
    static synchronized() { }
    static this() { }
    static throw() { }
    static throws() { }
    static transient() { }
    static true() { }
    static try() { }
    static typeof() { }
    static var() { }
    static void() { }
    static volatile() { }
    static while() { }
    static with() { }
    static yield() { }
};

class ClassWithKeywordGetterMethodNames {
    // get constructor() { } (getter `constructor` not allowed)
    get abstract() { }
    get arguments() { }
    get boolean() { }
    get break() { }
    get byte() { }
    get case() { }
    get catch() { }
    get char() { }
    get class() { }
    get const() { }
    get continue() { }
    get debugger() { }
    get default() { }
    get delete() { }
    get do() { }
    get double() { }
    get else() { }
    get enum() { }
    get eval() { }
    get export() { }
    get extends() { }
    get false() { }
    get final() { }
    get finally() { }
    get float() { }
    get for() { }
    get function() { }
    get get() { }
    get goto() { }
    get if() { }
    get implements() { }
    get import() { }
    get in() { }
    get instanceof() { }
    get int() { }
    get interface() { }
    get let() { }
    get long() { }
    get native() { }
    get new() { }
    get null() { }
    get package() { }
    get private() { }
    get protected() { }
    get public() { }
    get of() { }
    get return() { }
    get set() { }
    get short() { }
    get static() { }
    get super() { }
    get switch() { }
    get synchronized() { }
    get this() { }
    get throw() { }
    get throws() { }
    get transient() { }
    get true() { }
    get try() { }
    get typeof() { }
    get var() { }
    get void() { }
    get volatile() { }
    get while() { }
    get with() { }
    get yield() { }
};

class ClassWithKeywordSetterMethodNames {
    // set constructor() { } (setter `constructor` not allowed)
    set abstract(x) { }
    set arguments(x) { }
    set boolean(x) { }
    set break(x) { }
    set byte(x) { }
    set case(x) { }
    set catch(x) { }
    set char(x) { }
    set class(x) { }
    set const(x) { }
    set continue(x) { }
    set debugger(x) { }
    set default(x) { }
    set delete(x) { }
    set do(x) { }
    set double(x) { }
    set else(x) { }
    set enum(x) { }
    set eval(x) { }
    set export(x) { }
    set extends(x) { }
    set false(x) { }
    set final(x) { }
    set finally(x) { }
    set float(x) { }
    set for(x) { }
    set function(x) { }
    set get(x) { }
    set goto(x) { }
    set if(x) { }
    set implements(x) { }
    set import(x) { }
    set in(x) { }
    set instanceof(x) { }
    set int(x) { }
    set interface(x) { }
    set let(x) { }
    set long(x) { }
    set native(x) { }
    set new(x) { }
    set null(x) { }
    set package(x) { }
    set private(x) { }
    set protected(x) { }
    set public(x) { }
    set of(x) { }
    set return(x) { }
    set set(x) { }
    set short(x) { }
    set static(x) { }
    set super(x) { }
    set switch(x) { }
    set synchronized(x) { }
    set this(x) { }
    set throw(x) { }
    set throws(x) { }
    set transient(x) { }
    set true(x) { }
    set try(x) { }
    set typeof(x) { }
    set var(x) { }
    set void(x) { }
    set volatile(x) { }
    set while(x) { }
    set with(x) { }
    set yield(x) { }
};

var objectLiteralWithKeywordMethodNames = {
    constructor() { },
    abstract() { },
    arguments() { },
    boolean() { },
    break() { },
    byte() { },
    case() { },
    catch() { },
    char() { },
    class() { },
    const() { },
    continue() { },
    debugger() { },
    default() { },
    delete() { },
    do() { },
    double() { },
    else() { },
    enum() { },
    eval() { },
    export() { },
    extends() { },
    false() { },
    final() { },
    finally() { },
    float() { },
    for() { },
    function() { },
    get() { },
    goto() { },
    if() { },
    implements() { },
    import() { },
    in() { },
    instanceof() { },
    int() { },
    interface() { },
    let() { },
    long() { },
    native() { },
    new() { },
    null() { },
    package() { },
    private() { },
    protected() { },
    public() { },
    of() { },
    return() { },
    set() { },
    short() { },
    static() { },
    super() { },
    switch() { },
    synchronized() { },
    this() { },
    throw() { },
    throws() { },
    transient() { },
    true() { },
    try() { },
    typeof() { },
    var() { },
    void() { },
    volatile() { },
    while() { },
    with() { },
    yield() { },
};

var objectLiteralWithKeywordGetterNames = {
    get constructor() { },
    get abstract() { },
    get arguments() { },
    get boolean() { },
    get break() { },
    get byte() { },
    get case() { },
    get catch() { },
    get char() { },
    get class() { },
    get const() { },
    get continue() { },
    get debugger() { },
    get default() { },
    get delete() { },
    get do() { },
    get double() { },
    get else() { },
    get enum() { },
    get eval() { },
    get export() { },
    get extends() { },
    get false() { },
    get final() { },
    get finally() { },
    get float() { },
    get for() { },
    get function() { },
    get get() { },
    get goto() { },
    get if() { },
    get implements() { },
    get import() { },
    get in() { },
    get instanceof() { },
    get int() { },
    get interface() { },
    get let() { },
    get long() { },
    get native() { },
    get new() { },
    get null() { },
    get package() { },
    get private() { },
    get protected() { },
    get public() { },
    get of() { },
    get return() { },
    get set() { },
    get short() { },
    get static() { },
    get super() { },
    get switch() { },
    get synchronized() { },
    get this() { },
    get throw() { },
    get throws() { },
    get transient() { },
    get true() { },
    get try() { },
    get typeof() { },
    get var() { },
    get void() { },
    get volatile() { },
    get while() { },
    get with() { },
    get yield() { },
};

var objectLiteralWithKeywordSetterNames = {
    set constructor(x) { },
    set abstract(x) { },
    set arguments(x) { },
    set boolean(x) { },
    set break(x) { },
    set byte(x) { },
    set case(x) { },
    set catch(x) { },
    set char(x) { },
    set class(x) { },
    set const(x) { },
    set continue(x) { },
    set debugger(x) { },
    set default(x) { },
    set delete(x) { },
    set do(x) { },
    set double(x) { },
    set else(x) { },
    set enum(x) { },
    set eval(x) { },
    set export(x) { },
    set extends(x) { },
    set false(x) { },
    set final(x) { },
    set finally(x) { },
    set float(x) { },
    set for(x) { },
    set function(x) { },
    set get(x) { },
    set goto(x) { },
    set if(x) { },
    set implements(x) { },
    set import(x) { },
    set in(x) { },
    set instanceof(x) { },
    set int(x) { },
    set interface(x) { },
    set let(x) { },
    set long(x) { },
    set native(x) { },
    set new(x) { },
    set null(x) { },
    set package(x) { },
    set private(x) { },
    set protected(x) { },
    set public(x) { },
    set of(x) { },
    set return(x) { },
    set set(x) { },
    set short(x) { },
    set static(x) { },
    set super(x) { },
    set switch(x) { },
    set synchronized(x) { },
    set this(x) { },
    set throw(x) { },
    set throws(x) { },
    set transient(x) { },
    set true(x) { },
    set try(x) { },
    set typeof(x) { },
    set var(x) { },
    set void(x) { },
    set volatile(x) { },
    set while(x) { },
    set with(x) { },
    set yield(x) { },
};

var successfullyParsed = true;
