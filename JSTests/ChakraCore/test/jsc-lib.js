print = legacyPrint;

WScript = {
    _jscGC: gc,
    _jscLoad: load,
    _jscPrint: print,
    _jscQuit: quit,
    _convertPathname : function(dosStylePath)
    {
        return dosStylePath.replace(/\\/g, "/");
    },
    Arguments : [ "summary" ],
    Echo : function()
    {
        WScript._jscPrint.apply(this, arguments);
    },
    LoadScriptFile : function(path)
    {
        WScript._jscLoad(WScript._convertPathname(path));
    },
    Quit : function()
    {
        WScript._jscQuit();
    },
};

function CollectGarbage()
{
    WScript._jscGC();
}

function $ERROR(e)
{
}
