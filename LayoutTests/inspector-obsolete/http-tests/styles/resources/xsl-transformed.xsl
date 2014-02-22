<?xml version='1.0' encoding='utf-8'?>
<xsl:stylesheet version="1.0"
                id="stylesheet"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
    <xsl:template match="items">
        <html>
        <head>
        <script src="../inspector-test.js"></script>
        <script src="../elements-test.js"></script>
        <script src="../resources-test.js"></script>
        <script>
        function test()
        {
            InspectorTest.reloadPage(step1);

            function step1()
            {
                InspectorTest.runAfterResourcesAreFinished(["xsl-transformed.xsl"], InspectorTest.expandElementsTree.bind(InspectorTest, step2));
            }

            function step2()
            {
                InspectorTest.dumpElementsTree();
                InspectorTest.completeTest();
            }
        }
        </script>
        </head>
        <body onload="runTest()">
        <p>Tests that XSL-transformed documents in the main frame are rendered correctly in the Elements panel. <a href="https://bugs.webkit.org/show_bug.cgi?id=111313">Bug 111313</a></p>
        <xsl:for-each select="item">
          <span><xsl:value-of select="."/></span>
        </xsl:for-each>
        </body>
        </html>
    </xsl:template>
</xsl:stylesheet>

