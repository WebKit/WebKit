var value = 10; var sum = 1; debug(window.internals.parserMetaData()); debug("\n  On first line:"); function f0() { dump(f0); function f0a() { dump(f0a); function f0b() { dump(f0b); eval('sum += value; debug(window.internals.parserMetaData());'); } f0b(); } f0a(); } f0();  var x0 = function(key) { dump(x0);var x0a = function() { dump(x0a); var x0b = function() { dump(x0b); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; x0b(); }; x0a(); }; x0();

function globalFunc() {
    var array = [2];
    var sum = 0;
    var value = 5;

    debug("\n  Functions Declarations in a function:");
    function f1(key) { dump(f1); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
    f1();

    function f2(key) {
        dump(f2);
        function f2a() { dump(f2a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
        f2a();
    }
    f2();

    function f3(key) {
        dump(f3);
        function f3a() {
            dump(f3a);
            function f3b() {
                dump(f3b);
                sum += value; sum *= value;
                eval('sum += value; debug(window.internals.parserMetaData());');
            }
            f3b();
        }
        f3a();
    }
    f3();

    function f4(key) { dump(f4); function f4a() {}; dump(f4a); f4a(); }
    f4();
    function f5(key) { dump(f5); function f5a() { dump(f5a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; f5a(); }
    f5();
    function f6(key) { dump(f6); function f6a() { dump(f6a); function f6b() { dump(f6b); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; f6b(); }; f6a(); }
    f6();

    debug("\n  Indented Functions Declarations in a function:");
    {
        function fi1(key) { dump(fi1); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
        fi1();

        function fi2(key) {
            dump(fi2);
            function fi2a() { dump(fi2a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
            fi2a();
        }
        fi2();

        function fi3(key) {
            dump(fi3);
            function fi3a() {
                dump(fi3a);
                function fi3b() {
                    dump(fi3b);
                    sum += value; sum *= value;
                    eval('sum += value; debug(window.internals.parserMetaData());');
                }
                fi3b();
            }
            fi3a();
        }
        fi3();

        function fi4(key) { dump(fi4); function fi4a() {}; dump(fi4a); fi4a(); }
        fi4();
        function fi5(key) { dump(fi5); function fi5a() { dump(fi5a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; fi5a(); }
        fi5();
        function fi6(key) { dump(fi6); function fi6a() { dump(fi6a); function fi6b() { dump(fi6b); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; fi6b(); }; fi6a(); }
        fi6();
    }

    debug("\n  Functions Expressions in a function:");
    var x1 = function(key) { dump(x1); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
    x1();

    var x2 = function(key) {
        dump(x2);
        var x2a = function() { dump(x2a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
        x2a();
    }
    x2();

    var x3 = function(key) {
        dump(x3);
        var x3a = function() {
            dump(x3a);
            var x3b = function() {
                dump(x3b);
                sum += value;
                eval('sum += value; debug(window.internals.parserMetaData());');
            }
            x3b();
        }
        x3a();
    }
    x3();

    var x4 = function(key) { dump(x4); var x4a = function() {}; dump(x4a); x4a(); }
    x4();
    var x5 = function(key) { dump(x5); var x5a = function() { dump(x5a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; x5a(); }
    x5();
    var x6 = function(key) { dump(x6); var x6a = function() { dump(x6a); var x6b = function() { dump(x6b); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; x6b(); }; x6a(); }
    x6();

    debug("\n  Indented Functions Expressions in a function:");
    {
        var xi1 = function(key) { dump(xi1); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
        xi1();

        var xi2 = function(key) {
            dump(xi2);
            var xi2a = function() { dump(xi2a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
            xi2a();
        }
        xi2();

        var xi3 = function(key) {
            dump(xi3);
            var xi3a = function() {
                dump(xi3a);
                var xi3b = function() {
                    dump(xi3b);
                    sum += value; sum *= value;
                    eval('sum += value; debug(window.internals.parserMetaData());');
                }
                xi3b();
            }
            xi3a();
        }
        xi3();

        var xi4 = function(key) { dump(xi4); var xi4a = function() {}; dump(xi4a); xi4a(); }
        xi4();
        var xi5 = function(key) { dump(xi5); var xi5a = function() { dump(xi5a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; xi5a(); }
        xi5();
        var xi6 = function(key) { dump(xi6); var xi6a = function() { dump(xi6a); var xi6b = function() { dump(xi6b); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; xi6b(); }; xi6a(); }
        xi6();
    }

    debug("\n  Anonymous Function Declaration in a function:");
    var forEach = [ ].forEach;
    var boundForEach = forEach.bind(array, function(key) {
        debug(window.internals.parserMetaData());
        sum += value;
        sum *= value;
        eval('sum += value; debug(window.internals.parserMetaData());');
    });
    boundForEach();

}
globalFunc();

debug("\n  Global eval:");
eval('sum += value; debug(window.internals.parserMetaData());');

debug("\n  Global Functions Declarations:");
function gf1(key) { dump(gf1); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
gf1();

function gf2(key) {
    dump(gf2);
    function gf2a() { dump(gf2a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
    gf2a();
}
gf2();

function gf3(key) {
    dump(gf3);
    function gf3a() {
        dump(gf3a);
        function gf3b() {
            dump(gf3b);
            sum += value; sum *= value;
            eval('sum += value; debug(window.internals.parserMetaData());');
        }
        gf3b();
    }
    gf3a();
}
gf3();

function gf4(key) { dump(gf4); function gf4a() {}; dump(gf4a); gf4a(); }
gf4();
function gf5(key) { dump(gf5); function gf5a() { dump(gf5a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; gf5a(); }
gf5();
function gf6(key) { dump(gf6); function gf6a() { dump(gf6a); function gf6b() { dump(gf6b); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; gf6b(); }; gf6a(); }
gf6();

debug("\n  Indented Global Functions Declarations:");
{
    function gfi1(key) { dump(gfi1); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
    gfi1();

    function gfi2(key) {
        dump(gfi2);
        function gfi2a() { dump(gfi2a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
        gfi2a();
    }
    gfi2();

    function gfi3(key) {
        dump(gfi3);
        function gfi3a() {
            dump(gfi3a);
            function gfi3b() {
                dump(gfi3b);
                sum += value; sum *= value;
                eval('sum += value; debug(window.internals.parserMetaData());');
            }
            gfi3b();
        }
        gfi3a();
    }
    gfi3();

    function gfi4(key) { dump(gfi4); function gfi4a() {}; dump(gfi4a); gfi4a(); }
    gfi4();
    function gfi5(key) { dump(gfi5); function gfi5a() { dump(gfi5a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; gfi5a(); }
    gfi5();
    function gfi6(key) { dump(gfi6); function gfi6a() { dump(gfi6a); function gfi6b() { dump(gfi6b); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; gfi6b(); }; gfi6a(); }
    gfi6();
}

debug("\n  Global Functions Expressions:");
var gx1 = function(key) { dump(gx1); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
gx1();

var gx2 = function(key) {
    dump(gx2);
    var gx2a = function() { dump(gx2a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
    gx2a();
}
gx2();

var gx3 = function(key) {
    dump(gx3);
    var gx3a = function() {
        dump(gx3a);
        var gx3b = function() {
            dump(gx3b);
            sum += value;
            eval('sum += value; debug(window.internals.parserMetaData());');
        }
        gx3b();
    }
    gx3a();
}
gx3();

var gx4 = function(key) { dump(gx4); var gx4a = function() {}; dump(gx4a); gx4a(); }
gx4();
var gx5 = function(key) { dump(gx5); var gx5a = function() { dump(gx5a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; gx5a(); }
gx5();
var gx6 = function(key) { dump(gx6); var gx6a = function() { dump(gx6a); var gx6b = function() { dump(gx6b); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; gx6b(); }; gx6a(); }
gx6();

debug("\n  Indented Functions Declarations:");
{
    var gxi1 = function(key) { dump(gxi1); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
    gxi1();

    var gxi2 = function(key) {
        dump(gxi2);
        var gxi2a = function() { dump(gxi2a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }
        gxi2a();
    }
    gxi2();

    var gxi3 = function(key) {
        dump(gxi3);
        var gxi3a = function() {
            dump(gxi3a);
            var gxi3b = function() {
                dump(gxi3b);
                sum += value; sum *= value;
                eval('sum += value; debug(window.internals.parserMetaData());');
            }
            gxi3b();
        }
        gxi3a();
    }
    gxi3();

    var gxi4 = function(key) { dump(gxi4); var gxi4a = function() {}; dump(gxi4a); gxi4a(); }
    gxi4();
    var gxi5 = function(key) { dump(gxi5); var gxi5a = function() { dump(gxi5a); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; gxi5a(); }
    gxi5();
    var gxi6 = function(key) { dump(gxi6); var gxi6a = function() { dump(gxi6a); var gxi6b = function() { dump(gxi6b); sum += value; eval('sum += value; debug(window.internals.parserMetaData());'); }; gxi6b(); }; gxi6a(); }
    gxi6();
}

debug("\n  Anonymous Global Function Declarations:");
var array = [2];
var gforEach = [ ].forEach;
var gboundForEach = gforEach.bind(array, function(key) {
    debug(window.internals.parserMetaData());
    sum += value;
    sum *= value;
    eval('sum += value; debug(window.internals.parserMetaData());');
});
gboundForEach();

debug("\n  Function Declarations in an eval:");
eval(" \n" +
"    debug(window.internals.parserMetaData());\n" +
"    function ef1() {\n" +
"        dump(ef1);\n" +
"        function ef1a() {\n" +
"            dump(ef1a);\n" +
"            function ef1b() {\n" +
"                dump(ef1b);\n" +
"                eval('sum += value; debug(window.internals.parserMetaData());');\n" +
"            }\n" +
"            ef1b();\n" +
"        }\n" +
"        ef1a();\n" +
"    }\n" +
"\n" +
"ef1();"
);

eval(" debug(window.internals.parserMetaData()); function ef2() { dump(ef2); function ef2a() { dump(ef2a); function ef2b() { dump(ef2b); eval('sum += value; debug(window.internals.parserMetaData());'); } ef2b(); } ef2a(); } ef2();");

{
    eval(" \n" +
         "    debug(window.internals.parserMetaData());\n" +
         "    // Test indented function with inner functions. \n" +
         "    function efi1() {\n" +
         "        dump(efi1);\n" +
         "        function efi1a() {\n" +
         "            dump(efi1a);\n" +
         "            function efi1b() {\n" +
         "                dump(efi1b);\n" +
         "                eval('sum += value; debug(window.internals.parserMetaData());');\n" +
         "            }\n" +
         "            efi1b();\n" +
         "        }\n" +
         "        efi1a();\n" +
         "    }\n" +
         "\n" +
         "efi1();"
        );

    eval(" debug(window.internals.parserMetaData()); function efi2() { dump(efi2); function efi2a() { dump(efi2a); function efi2b() { dump(efi2b); eval('sum += value; debug(window.internals.parserMetaData());'); } efi2b(); } efi2a(); } efi2();");
}

debug("\n  Function Expressions in an eval:");
eval(" \n" +
"    debug(window.internals.parserMetaData());\n" +
"    var ex1 = function() {\n" +
"        dump(ex1);\n" +
"        var ex1a = function() {\n" +
"            dump(ex1a);\n" +
"            var ex1b = function() {\n" +
"                dump(ex1b);\n" +
"                eval('sum += value; debug(window.internals.parserMetaData());');\n" +
"            }\n" +
"            ex1b();\n" +
"        }\n" +
"        ex1a();\n" +
"    }\n" +
"\n" +
"ex1();"
);

eval(" debug(window.internals.parserMetaData()); var ex2 = function() { dump(ex2); var ex2a = function() { dump(ex2a); var ex2b = function() { dump(ex2b); eval('sum += value; debug(window.internals.parserMetaData());'); }; ex2b(); }; ex2a(); }; ex2();");

{
    eval(" \n" +
         "    debug(window.internals.parserMetaData());\n" +
         "    // Test indented function with inner functions. \n" +
         "    var exi1 = function() {\n" +
         "        dump(exi1);\n" +
         "        var exi1a = function() {\n" +
         "            dump(exi1a);\n" +
         "            var exi1b = function() {\n" +
         "                dump(exi1b);\n" +
         "                eval('sum += value; debug(window.internals.parserMetaData());');\n" +
         "            }\n" +
         "            exi1b();\n" +
         "        }\n" +
         "        exi1a();\n" +
         "    }\n" +
         "\n" +
         "exi1();"
        );

    eval(" debug(window.internals.parserMetaData()); var exi2 = function() { dump(exi2); var exi2a = function() { dump(exi2a); var exi2b = function() { dump(exi2b); eval('sum += value; debug(window.internals.parserMetaData());'); }; exi2b(); }; exi2a(); }; exi2();");
}

debug("\n  new Function Object:");
var nf1Str =
" debug(window.internals.parserMetaData()); function nf1a() { dump(nf1a); function nf1b() { dump(nf1b); function nf1c() { dump(nf1c); eval('sum += value; debug(window.internals.parserMetaData());'); } nf1c(); } nf1b(); } nf1a();";
var nf1 = new Function("a", "b", nf1Str);
nf1();

var nf2Str =
" \n" +
"    debug(window.internals.parserMetaData());\n" +
"    // Test indented function with inner functions. \n" +
"    function nf2a() {\n" +
"        dump(nf2a);\n" +
"        function nf2b() {\n" +
"            dump(nf2b);\n" +
"            function nf2c() {\n" +
"                dump(nf2c);\n" +
"                eval('sum += value; debug(window.internals.parserMetaData());');\n" +
"            }\n" +
"            nf2c();\n" +
"        }\n" +
"        nf2b();\n" +
"    }\n" +
"\n" +
"nf2a();";
var nf2 = new Function("a", "b", nf2Str);
nf2();

{
    // nfi1_0 has an identical script string as nf1 except we indented it here.
    var nfi1_0 = new Function("a", "b", " debug(window.internals.parserMetaData()); function nf1a() { dump(nf1a); function nf1b() { dump(nf1b); function nf1c() { dump(nf1c); eval('sum += value; debug(window.internals.parserMetaData());'); } nf1c(); } nf1b(); } nf1a();");
    nfi1_0();

    var nfi1 = new Function("a", "b", " debug(window.internals.parserMetaData()); function nfi1a() { dump(nfi1a); function nfi1b() { dump(nfi1b); function nfi1c() { dump(nfi1c); eval('sum += value; debug(window.internals.parserMetaData());'); } nfi1c(); } nfi1b(); } nfi1a();");
    nfi1();

    // nfi2_0 has an identical script string as nf2 except we indented it here.
    var nfi2_0 = new Function("a", "b",
        " \n" +
        "    debug(window.internals.parserMetaData());\n" +
        "    // Test indented function with inner functions. \n" +
        "    function nf2a() {\n" +
        "        dump(nf2a);\n" +
        "        function nf2b() {\n" +
        "            dump(nf2b);\n" +
        "            function nf2c() {\n" +
        "                dump(nf2c);\n" +
        "                eval('sum += value; debug(window.internals.parserMetaData());');\n" +
        "            }\n" +
        "            nf2c();\n" +
        "        }\n" +
        "        nf2b();\n" +
        "    }\n" +
        "\n" +
        "nf2a();"
    );
    nfi2_0();

    var nfi2 = new Function("a", "b",
        " \n" +
        "    debug(window.internals.parserMetaData());\n" +
        "    // Test indented function with inner functions. \n" +
        "    function nfi2a() {\n" +
        "        dump(nfi2a);\n" +
        "        function nfi2b() {\n" +
        "            dump(nfi2b);\n" +
        "            function nfi2c() {\n" +
        "                dump(nfi2c);\n" +
        "                eval('sum += value; debug(window.internals.parserMetaData());');\n" +
        "            }\n" +
        "            nfi2c();\n" +
        "        }\n" +
        "        nfi2b();\n" +
        "    }\n" +
        "\n" +
        "nfi2a();"
    );
    nfi2();
}
debug("");
