<?xml version="1.0"?>
<!--
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is XSL:P XSLT processor.
 *
 * The Initial Developer of the Original Code is Keith Visco.
 * Portions created by Keith Visco (C) 1999 Keith Visco.
 * All Rights Reserved..
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *    - original author.
 *
 */
-->

<!--
  This is a test stylesheet used for testing the XSL processor
-->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<!-- set the output properties -->
<xsl:output method="html"/>

<!-- AttributeSet -->
<xsl:attribute-set name="style1">
   <xsl:attribute name="COLOR">blue</xsl:attribute>
   <xsl:attribute name="SIZE">+0</xsl:attribute>
</xsl:attribute-set>

<xsl:attribute-set name="style2">
   <xsl:attribute name="COLOR">red</xsl:attribute>
   <xsl:attribute name="SIZE">+0</xsl:attribute>
</xsl:attribute-set>

<!-- root rule -->
<xsl:template match="/">
   <xsl:processing-instruction name="foo">
      this is a test processing instruction
   </xsl:processing-instruction>
   <xsl:comment>TransforMiiX Test cases, written by Keith Visco.</xsl:comment>
   <xsl:apply-templates/>
</xsl:template>

<!-- named template -->
<xsl:template name="named-template-test">
   <xsl:param name="my-param" select="'default value'"/>
   named template processed with <xsl:text> </xsl:text>
   <xsl:value-of select="$my-param"/>!
   <xsl:if test="$dummy-param">
     <BR/>
     <FONT COLOR="red">
        Error, undeclared parameters should be ignored!
     </FONT>
   </xsl:if>
</xsl:template>

<!-- empty variable for named template test -->
<xsl:variable name="dummy-param" />

<!-- suppress non-selected nodes-->
<xsl:template match="*"/>

<!-- variable tests -->
<xsl:variable name="product-name">
  Transfor<FONT COLOR="blue">Mii</FONT>X
</xsl:variable>
<!-- main rule for document element -->
<xsl:template match="document">
<HTML>
  <HEAD>
    <TITLE>TransforMiiX Test Cases</TITLE>
    <SCRIPT Language="JavaScript">
     <xsl:text>
      // Support for Apple's DumpRenderTree
        if (window.testRunner)
            testRunner.dumpAsText();
      // This is a test for properly printing SCRIPT elements
      // currently there is a bug, so use xsl:text as a wrapper as I
      // have done here
      function foo() {
          var x = 1;
          var y = 2;
          return (x &lt; y);
      }
     //
     </xsl:text>
    </SCRIPT>
  </HEAD>
  <BODY BGColor="#FFFFFF" Text="#000000">
  <CENTER>
      <FONT COLOR="BLUE" FACE="Arial" SIZE="+1">
         <B>Mozilla XSLT</B>
      </FONT>
      <BR/>
      <B>Transfor<FONT COLOR="blue">Mii</FONT>X Test Cases</B>
 </CENTER>
 <P>
  This document serves to test basic XSL expressions.
 </P>
 <!-- new test -->
 <P>
    <B>Testing xsl:variable and xsl:copy-of</B><BR/>
    <B>Test:</B> &lt;xsl:copy-of select="$product-name"/&gt;<BR/>
    <B>Desired Result:</B>Transfor<FONT COLOR="blue">Mii</FONT>X<BR/>
    <B>Result:</B><xsl:copy-of select="$product-name"/>
 </P>
 <!-- new test -->
 <P>
    <B>Testing xsl:if</B><BR/>
    <B>Test:</B> &lt;xsl:if test="x | y | z"&gt;true&lt;/xsl:if&gt;<BR/>
    <B>Desired Result:</B> true<BR/>
    <B>Result:</B> <xsl:if test="x | y | z">true</xsl:if>
 </P>

 <!-- new test -->
 <P>
    <B>Testing xsl:if</B><BR/>
    <B>Test:</B> &lt;xsl:if test="true()"&gt;true&lt;/xsl:if&gt;<BR/>
    <B>Desired Result:</B> true<BR/>
    <B>Result:</B> <xsl:if test="true()">true</xsl:if>
 </P>

 <!-- new test -->
 <P>
    <B>Testing xsl:if</B><BR/>
    <B>Test:</B> &lt;xsl:if test="'a'='b'"&gt;a equals b&lt;/xsl:if&gt;<BR/>
    <B></B> &lt;xsl:if test="'a'!='b'"&gt;a does not equal b&lt;/xsl:if&gt;
    <BR/>
    <B>Desired Result:</B> a does not equal to b<BR/>
    <B>Result:</B>
    <xsl:if test="'a'='b'">a equals b<BR/></xsl:if>
    <xsl:if test="'a'!='b'">a does not equal b</xsl:if>
 </P>

 <!-- new test -->
 <P>
    <B>Testing xsl:if</B><BR/>
    <B>Test:</B> &lt;xsl:if test="2+1-3"&gt; 2+1-3 is true&lt;/xsl:if&gt;<BR/>
    <B>&#160;</B>&lt;xsl:if test="not(2+1-3)"&gt; not(2+1-3) is true&lt;/xsl:if&gt;<BR/>
    <B>Desired Result:</B>not(2+1-3) is true <BR/>
    <B>Result:</B>
        <xsl:if test="2+1-3">2+1-3 is true</xsl:if>
        <xsl:if test="not(2+1-3)">not(2+1-3) is true</xsl:if>
 </P>

 <!-- new test -->
 <P>
    <B>Testing xsl:choose</B><BR/>
    <B>Test:</B>see source<BR/>
    <B>Desired Result:</B> true<BR/>
    <B>Result:</B>
    <xsl:choose>
        <xsl:when test="a">error - a</xsl:when>
        <xsl:when test="abc/def">true</xsl:when>
        <xsl:when test="b">error - b</xsl:when>
        <xsl:otherwise>false</xsl:otherwise>
    </xsl:choose>
 </P>
 <!-- new test -->
 <P>
    <B>Testing parent and ancestor ops</B><BR/>
    <B>Test:</B>see source<BR/>
    <B>Desired Result:</B> true<BR/>
    <B>Result:</B><xsl:if test="//def">true</xsl:if><BR/>

 </P>
 <!-- new test -->

  <P>
    <B>Testing basic xsl:apply-templates</B><BR/>
    <B>Test:</B>&lt;xsl:apply-templates/&gt;<BR/>
    <B>Desired Result:</B>element <B>x</B>, element <B>y</B>, element <B>z</B><BR/>
    <B>Result:</B><xsl:apply-templates select="x|y|z"/>
  </P>
 <!-- new test -->

  <P>
    <B>Testing basic xsl:apply-templates with mode</B><BR/>
    <B>Test:</B>&lt;xsl:apply-templates mode="mode-test"/&gt;<BR/>
    <B>Desired Result:</B>x, y, z<BR/>
    <B>Result:</B><xsl:apply-templates select="x|y|z" mode="mode-test"/>
  </P>
 <!-- new test -->
  <P>
    <B>Testing predicates</B><BR/>
    <B>Test:</B>see source<BR/>
    <B>Desired Result:</B> <B>z</B><BR/>
    <B>Result:</B>
    <xsl:for-each select="*[position()=3]">
      <B><xsl:value-of select="."/></B>
    </xsl:for-each>
  </P>
 <!-- new test -->
  <P>
    <B>Testing predicates</B><BR/>
    <B>Test:</B>see source<BR/>
    <B>Desired Result:</B><BR/>
    <B>Result:</B>
    <xsl:for-each select="*[false()]">
      <B><xsl:value-of select="."/></B>
    </xsl:for-each>
  </P>
 <!-- new test -->
  <P>
    <B>Named Template/Call Template</B>
    <P>
       <B>Test:</B>&lt;xsl:call-template name="named-template-test"/&gt;<BR/>
       <B>Desired Result:</B>named template processed with default value!<BR/>
       <B>Result:</B><xsl:call-template name="named-template-test"/>
    </P>
    <P>
       <B>Test:</B> - passing arguments to named templates (see xsl source)<BR/>
       <B>Desired Result:</B>named template processed with passed value!<BR/>
       <B>Result:</B>
           <xsl:call-template name="named-template-test">
              <xsl:with-param name="my-param" select="'passed value'"/>
              <xsl:with-param name="dummy-param" select="'test'"/>
           </xsl:call-template>
    </P>
  </P>
 <!-- new test -->
  <P>
    <B>Attribute Value Templates and variables</B><BR/>
    <B>Test:</B>
    <UL>
       &lt;xsl:variable name="color"&gt;red&lt;/xsl:variable&gt;<BR/>
       &lt;FONT COLOR="{$color}"&gt;Red Text&lt;/FONT&gt;
    </UL>
    <B>Desired Result:</B>
        <FONT COLOR="red">Red Text</FONT><BR/>
    <B>Result:</B>
       <xsl:variable name="color">red</xsl:variable>
       <FONT COLOR="{$color}">Red Text</FONT>
  </P>
  <HR/>
   <!-- AXIS IDENTIFIER TESTS -->
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>Axis Identifiers (these should work, I need more test cases though)</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:if test="descendant::z"&gt;true&lt;/xsl:if&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">true</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <xsl:if test="descendant::z">
           <FONT COLOR="blue">true</FONT>
         </xsl:if>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:if test="not(descendant-or-self::no-element)"&gt;true&lt;/xsl:if&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">true</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <xsl:if test="not(descendant-or-self::no-element)">
           <FONT COLOR="blue">true</FONT>
         </xsl:if>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="count(x/attribute::*)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">1</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue"><xsl:value-of select="count(x/attribute::*)"/></FONT>
      </TD>
   </TR>
   </TABLE>

  <HR/>
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>Creating Elements with xsl:element and xsl:attribute</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:element name="FONT"&gt;<BR />
         &lt;xsl:attribute name="COLOR"&gt;blue&lt;/xsl:attribute&gt; <BR/>
         Passed <BR/>
         &lt;/xsl:element&gt;
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">Passed</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <xsl:element name="FONT">
           <xsl:attribute name="COLOR">blue</xsl:attribute>
           Passed
         </xsl:element>
      </TD>
   </TR>
   <!-- new test -->
   <TR BGCOLOR="#E0E0FF" ALIGN="CENTER">
      <TD COLSPAN="2"><B>Using Attribute Sets</B></TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;FONT xsl:use-attribute-sets="style1"&gt;<BR />
         Passed <BR/>
         &lt;/FONT&gt;
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">Passed</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT xsl:use-attribute-sets="style1">
            Passed
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:element name="FONT" use-attribute-sets="style1 style2"&gt;<BR />
         Passed <BR/>
         &lt;/xsl:element&gt;
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="red">Passed</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <xsl:element name="FONT" use-attribute-sets="style1 style2">
            Passed
         </xsl:element>
      </TD>
   </TR>
   </TABLE>
   <HR/>
   <!-- NUMBERING Examples -->
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>Numbering (only simple numbering currently implemented)</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:number value="4"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">4</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue"><xsl:number value="4"/></FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         see source<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
           1. x <BR/>1. y<BR/>1. z
         </FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:for-each select="x | y | z">
               <xsl:number/>
               <xsl:text>. </xsl:text><xsl:value-of select="."/><BR/>
            </xsl:for-each>
         </FONT>
      </TD>
   </TR>

   </TABLE>

  <HR/>
  <!-- ADDITIVE EXPRESSION TESTS -->
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>Additive Expressions</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="70+4"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">74</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="70+4"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="-70+4"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">-66</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="-70+4"/>
         </FONT>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="1900+70+8-4"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">1974</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="1900+70+8-4"/>
         </FONT>
      </TD>
   </TR>
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="(4+5)-(9+9)"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">-9</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="(4+5)-(9+9)"/>
         </FONT>
      </TD>
   </TR>

  </TABLE>
  <HR/>
  <!-- MULTIPLICATIVE EXPRESSION TESTS -->
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>Multiplicative Expressions</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="7*4"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">28</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="7*4"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="7mod 4"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">3</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="7mod 4"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="7div 4"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">1.75</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="7div 4"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="7div 0"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">Infinity</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="7div 0"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="0 div 0"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">NaN</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="0 div 0"/>
         </FONT>
      </TD>
   </TR>

   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:variable name="x" select="7*3"/&gt;<BR />
         &lt;xsl:variable name="y" select="3"/&gt;<BR />
         &lt;xsl:value-of select="$x div $y"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">7</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:variable name="x" select="7*3"/>
            <xsl:variable name="y" select="3"/>
            <xsl:value-of select="$x div $y"/>
            <BR/>

         </FONT>
      </TD>
   </TR>
   </TABLE>
  <!-- PRECEDENCE TESTS -->
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>Precedence tests</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="2 mod 2 = 0"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">true</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="2 mod 2 = 0"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="5 mod 2 &lt; 5 and 2*6 &gt;= 12"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">true</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="5 mod 2 &lt; 5 and 2*6>=12"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="5 mod 2 &lt; 5 and 2*6>12"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">false</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="5 mod 2 &lt; 5 and 2*6>12"/>
         </FONT>
      </TD>
   </TR>

   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="4+5*3"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">19</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="4+5*3"/>
         </FONT>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="4+5*3+(6-4)*7"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">33</FONT><BR/>
      </TD>
    </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="4+5*3+(6-4)*7"/>
         </FONT>
      </TD>
   </TR>
   </TABLE>

  <!-- Result Type conversion tests -->
   <TABLE>
   <TR BGColor="#E0E0FF">
      <TD Colspan="2" ALIGN="CENTER">
         <B>Automatic Result Type Conversion</B>
      </TD>
   </TR>
   <!-- new test -->
   <TR>
      <TD VALIGN="TOP"><B>Test:</B></TD>
      <TD>
         &lt;xsl:value-of select="'747' + 8"/&gt;<BR />
      </TD>
   </TR>
   <TR>
      <TD><B>Desired Result:</B></TD>
      <TD>
         <FONT COLOR="blue">755</FONT><BR/>
      </TD>
   </TR>
    <TR>
      <TD><B>Result:</B></TD>
      <TD>
         <FONT COLOR="blue">
            <xsl:value-of select="'747' + 8"/>
         </FONT>
      </TD>
   </TR>
   </TABLE>
  </BODY>
</HTML>
</xsl:template>

<!-- simple union expressions -->
<xsl:template match="x | y | z" priority="1.0">
   element<B><xsl:text> </xsl:text><xsl:value-of select="@*"/></B>
   <xsl:if test="not(position()=3)">,</xsl:if>
</xsl:template>

<xsl:template match="x | y | z" mode="mode-test">
   <xsl:value-of select="@*"/>
   <xsl:if test="not(position()=3)"><xsl:text>, </xsl:text></xsl:if>
</xsl:template>

<xsl:template match="z">
   element (z): <B><xsl:value-of select="."/></B>
</xsl:template>

</xsl:stylesheet>

