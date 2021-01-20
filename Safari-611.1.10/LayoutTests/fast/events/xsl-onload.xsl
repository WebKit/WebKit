<?xml version="1.0" encoding="ISO-8859-1"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:output method="html" indent="yes"/>

	<xsl:template match="*|@*|processing-instruction()|comment()">
		<xsl:copy>
			<xsl:apply-templates select="*|@*|text()|processing-instruction()|comment()"/>
		</xsl:copy>
	</xsl:template>

<xsl:template match="body">
	<body onload="document.getElementById('result').appendChild(document.createTextNode('Handled load event.'))">
		<script>if (window.testRunner) { testRunner.dumpAsText(); }</script>
		<p>This is a regression test that checks for a problem where parsing would be stuck true and you'd never get a load event for an XSL-generated page</p>
		<p>If the test succeeds, the next paragraph will say "handled load event". If that message is missing, the test failed.</p>
                <p id="result"/>
	</body>
</xsl:template>

</xsl:stylesheet>
