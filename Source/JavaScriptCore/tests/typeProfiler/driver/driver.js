// Types matching those in runtime/TypeSet
var T = {
    Boolean:"Boolean",
    Integer: "Integer",
    Null: "Null",
    Number: "Number",
    Many: "(many)",
    String: "String",
    Undefined: "Undefined",
    UndefinedOrNull: "(?)"
};

var TOptional = {
    Boolean:"Boolean?",
    Integer: "Integer?",
    Number: "Number?",
    String: "String?"
};

function assert(condition, reason) {
    if (!condition)
        throw new Error(reason);
}

var MaxStructureCountWithoutOverflow = 100;
