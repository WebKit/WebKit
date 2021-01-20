<stylesheet version="1.0" xmlns="http://www.w3.org/1999/XSL/Transform">
	<template match="/">
		<processing-instruction name="xml-stylesheet">
			href="resources/xslt-recursion-chain2.xsl" type="text/xsl"
		</processing-instruction>
		<html xml:lang="en-us" xmlns="http://www.w3.org/1999/xhtml">
			<body>
<style>
p.success {
    display: none;
}
</style> 
			<link rel="stylesheet" href="resources/xslt-recursion-chain1.css" />
<script>
if (window.testRunner)
	testRunner.dumpAsText();
</script> 
			<p class="success">Success!</p>
			<p class="failure">Failure! (external CSS sheets were ignored)</p>
		</body>
	</html>
</template>
</stylesheet>
