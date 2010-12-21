<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
  <xsl:output method="html"/>
  <xsl:template match="status">
    <html xmlns="http://www.w3.org/1999/xhtml">
      <head>
        <title>WebKit SVG Status</title>
        <link rel="StyleSheet" href="svg-status.css" />
      </head>
      <body>
        <xsl:copy-of select="description"/>
        <xsl:apply-templates select="module"/>
      </body>
    </html>
  </xsl:template>
  
  <xsl:template match="status/module">
    <xsl:variable name="url" select="url/text()"/>
    <h3 class="modulename"><a href="{$url}"><xsl:value-of select="name/text()"/> Module</a></h3>
    <table class="elements">
    <colgroup><col class="element"/><col class="status"/></colgroup>
    <tr><th>Element</th><th>Status</th></tr>
    <xsl:apply-templates select="elements"/>
    </table>
  </xsl:template>
  
  <xsl:template match="elements/element">
    <xsl:variable name="status" select="status/text()"/>
    <xsl:variable name="statusclass" select="translate($status, 'UNIMPLETD', 'unimpletd')"/>
    <tr class="element {$statusclass}">
        <xsl:variable name="url" select="url/text()"/>
        <xsl:variable name="elementText">
            &lt;<xsl:value-of select="name/text()"/>&gt;
        </xsl:variable>
        <td class="tagname">
        <xsl:choose>
        <xsl:when test="$url"><a href="{$url}"><xsl:value-of select="$elementText"/></a></xsl:when>
        <xsl:otherwise><xsl:value-of select="$elementText"/></xsl:otherwise>
        </xsl:choose>
        </td>
        <td class="status">
            <ul>
                <li>
                     <xsl:choose>
                      <xsl:when test="$status">
                        <xsl:value-of select="$status"/>
                      </xsl:when>
                      <xsl:otherwise>
                        <xsl:text>Partial</xsl:text>
                      </xsl:otherwise>
                    </xsl:choose>
                </li>
                <xsl:apply-templates select="issues"/>
            </ul>
        </td>
    </tr>
  </xsl:template>
  
  <xsl:template match="issues/issue">
    <li><xsl:apply-templates /></li>
  </xsl:template>
  
  <xsl:template match="bug">
    <xsl:variable name="number" select="text()"/>
    <a href="https://bugs.webkit.org/show_bug.cgi?id={$number}">
    <xsl:value-of select="$number"/>
    </a>
  </xsl:template>
  
</xsl:stylesheet>
