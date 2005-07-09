<?xml version="1.0" ?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns="http://www.w3.org/1999/xhtml">
 <xsl:output method="xml" encoding="UTF-8" indent="yes" doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN" doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd"/>
 <xsl:template match="/">
  <html xmlns="http://www.w3.org/1999/xhtml">
   <head>
    <title>KDOM/KSVG2 Status - <xsl:value-of select="//@date"/></title>
    <meta http-equiv="Content-Type" content="application/xhtml+xml; charset=UTF-8" />
    <style type="text/css">
    <xsl:text> .green { color: green; }</xsl:text>
    <xsl:text> .red { color: red; }</xsl:text>
    <xsl:text> .blue { color: blue; }</xsl:text>
    <xsl:text> .purple { color: purple; }</xsl:text>
    <xsl:text> .yellow { color: yellow; }</xsl:text>
    </style>
   </head>
   <body>
    <h1>KDOM/KSVG2 Status - <xsl:value-of select="//@date"/></h1>
    <table border="1">
     <thead>
      <th>PASS</th>
      <th>FAIL</th>
	  <th>CRASH</th>
      <th>NOEVAL</th>
      <th>UNKNOWN</th>
      <th>SKIP</th>
      <th>TOTAL</th>
      <th>TIME</th>
     </thead>
     <tbody>
      <tr>
       <td><xsl:value-of select="//passTotal" /></td>
       <td><xsl:value-of select="//failTotal" /></td>
       <td><xsl:value-of select="//crashTotal" /></td>
       <td><xsl:value-of select="//noevalTotal" /></td>
       <td><xsl:value-of select="//unknownTotal" /></td>
       <td><xsl:value-of select="//skipTotal" /></td>
       <td><xsl:value-of select="//total" /></td>
       <td><xsl:value-of select="//timeTotal" /></td>
      </tr>
     </tbody>
    </table>
    <table border="1">
     <thead>
      <th>STATUS</th>
      <th>FILE NAME</th>
      <th>ERROR MESSAGE</th>
      <th>TIME</th>
     </thead>
     <tbody>
      <xsl:apply-templates select="//entry" />
     </tbody>
    </table>
   </body>
  </html>
 </xsl:template>
 
 <xsl:template match="entry">
  <tr>
   <td><xsl:for-each select="node()"><xsl:if test="name(current())='result'"><xsl:choose>
    <xsl:when test=".='PASS'"><span class="green"><xsl:value-of select="normalize-space(.)"/></span></xsl:when>
    <xsl:when test=".='FAIL'"><span class="red"><xsl:value-of select="normalize-space(.)"/></span></xsl:when>
    <xsl:when test=".='CRASH'"><span class="black"><xsl:value-of select="normalize-space(.)"/></span></xsl:when>
    <xsl:when test=".='UNKNOWN'"><span class="blue"><xsl:value-of select="normalize-space(.)"/></span></xsl:when>
    <xsl:when test=".='NOEVAL'"><span class="purple"><xsl:value-of select="normalize-space(.)"/></span></xsl:when>
    <xsl:when test=".='SKIP'"><span class="yellow"><xsl:value-of select="normalize-space(.)"/></span></xsl:when>
   </xsl:choose></xsl:if></xsl:for-each></td>
   <td><xsl:for-each select="node()"><xsl:if test="name(current())='file'"><xsl:value-of select="."/></xsl:if></xsl:for-each></td>
   <td><xsl:for-each select="node()"><xsl:choose><xsl:when test="name(current())='text'"><xsl:if test="@y!='120'"><xsl:value-of select="."/></xsl:if></xsl:when><xsl:otherwise><xsl:text> </xsl:text></xsl:otherwise></xsl:choose></xsl:for-each></td>
   <td><xsl:for-each select="node()"><xsl:if test="name(current())='time'"><xsl:value-of select="."/></xsl:if></xsl:for-each></td>
  </tr>
 </xsl:template>

</xsl:stylesheet>
