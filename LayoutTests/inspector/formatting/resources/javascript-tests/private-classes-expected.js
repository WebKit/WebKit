class ClassWithPrivate {
    publicField;
    publicFieldWithInitializer = 42;
    #privateField;
    #privateFieldWithInitializer = 42;
    publicMethod()
    {
        this.publicField = "👋";
    }
    #privateMethod()
    {
        this.#privateField = "🔒";
    }
    static publicStaticField;
    static publicStaticFieldWithInitializer = 42;
    static #privateStaticField;
    static #privateStaticFieldWithInitializer = 42;
    static publicStaticMethod()
    {
        ClassWithPrivate.publicStaticField = "🖍️";
    }
    static #privateStaticMethod()
    {
        ClassWithPrivate.#privateStaticField = "🗝️";
    }
}
