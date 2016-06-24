{
    let errorType = errorTypes[errorTypeIndexToTest];
    let errorTypeName = errorTypeNames[errorTypeIndexToTest];

    logTitle("Test thrown " + errorTypeName + " from a script from another domain");
    incCaseIndex();

    try {
        console.log("   " + caseStr + " var e = new " + errorTypeName + "('Error thrown from other script with Secret');");
        var e = new errorType("Error thrown from other script with Secret");
        console.log("   " + caseStr + " e.name = 'OtherScript" + errorTypeName + "'");
        e.name = "OtherScript" + errorTypeName;

        let oldInMainScript = inMainScript;
        inMainScript = false;

        console.log("      [" + errorTypeName + "] e = '" + e + "'");
        console.log("      [" + errorTypeName + "] e.name = '" + e.name + "'");
        console.log("      [" + errorTypeName + "] e.message = '" + e.message + "'");
        console.log("      [" + errorTypeName + "] e.toString() = '" + e.toString() + "'");

        inMainScript = oldInMainScript;

        console.log("   " + caseStr + " throw e;");
        throw e;

    } catch (e) {
        let oldInMainScript = inMainScript;
        inMainScript = false;

        console.log("   " + caseStr + " Caught: " + e);
        console.log("   " + caseStr + "    re-throw e;");

        inMainScript = oldInMainScript;

        throw e;
    }
}