
/*
Copyright Â© 2001-2004 World Wide Web Consortium, 
(Massachusetts Institute of Technology, European Research Consortium 
for Informatics and Mathematics, Keio University). All 
Rights Reserved. This work is distributed under the W3CÂ® Software License [1] in the 
hope that it will be useful, but WITHOUT ANY WARRANTY; without even 
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 

[1] http://www.w3.org/Consortium/Legal/2002/copyright-software-20021231
*/



   /**
    *  Gets URI that identifies the test.
    *  @return uri identifier of test
    */
function getTargetURI() {
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentnormalizedocument03";
   }

var docsLoaded = -1000000;
var builder = null;

//
//   This function is called by the testing framework before
//      running the test suite.
//
//   If there are no configuration exceptions, asynchronous
//        document loading is started.  Otherwise, the status
//        is set to complete and the exception is immediately
//        raised when entering the body of the test.
//
function setUpPage() {
   setUpPageStatus = 'running';
   try {
     //
     //   creates test document builder, may throw exception
     //
     builder = createConfiguredBuilder();
       setImplementationAttribute("namespaceAware", true);

      docsLoaded = 0;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      docsLoaded += preload(docRef, "doc", "barfoo");
        
       if (docsLoaded == 1) {
          setUpPageStatus = 'complete';
       }
    } catch(ex) {
    	catchInitializationError(builder, ex);
        setUpPageStatus = 'complete';
    }
}



//
//   This method is called on the completion of 
//      each asychronous load started in setUpTests.
//
//   When every synchronous loaded document has completed,
//      the page status is changed which allows the
//      body of the test to be executed.
function loadComplete() {
    if (++docsLoaded == 1) {
        setUpPageStatus = 'complete';
    }
}

//DOMErrorMonitor's require a document level variable named errorMonitor
var errorMonitor;
	 
/**
* 
Normalize a document with a created CDATA section with the 
'cdata-sections' parameter set to true then to false and check if
the CDATASection has been preserved and then coalesced.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-normalizeDocument
* @see http://www.w3.org/Bugs/Public/show_bug.cgi?id=416
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-cdata-sections
*/
function documentnormalizedocument03() {
   var success;
    if(checkInitialization(builder, "documentnormalizedocument03") != null) return;
    var doc;
      var elem;
      var newCdata;
      var cdata;
      var text;
      var nodeName;
      var nodeValue;
      var appendedChild;
      var domConfig;
      var pList;
      errorMonitor = new DOMErrorMonitor();
      
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "barfoo");
      pList = doc.getElementsByTagName("p");
      elem = pList.item(0);
      newCdata = doc.createCDATASection("CDATA");
      appendedChild = elem.appendChild(newCdata);
      domConfig = doc.domConfig;

      domConfig.setParameter("cdata-sections", true);
	 domConfig.setParameter("error-handler", errorMonitor.handleError);
	 doc.normalizeDocument();
      errorMonitor.assertLowerSeverity("normalizationError", 2);
     pList = doc.getElementsByTagName("p");
      elem = pList.item(0);
      cdata = elem.lastChild;

      nodeName = cdata.nodeName;

      assertEquals("documentnormalizedocument03_true","#cdata-section",nodeName);
       domConfig.setParameter("cdata-sections", false);
	 doc.normalizeDocument();
      errorMonitor.assertLowerSeverity("normalization2Error", 2);
     pList = doc.getElementsByTagName("p");
      elem = pList.item(0);
      text = elem.lastChild;

      nodeName = text.nodeName;

      assertEquals("documentnormalizedocument03_false","#text",nodeName);
       nodeValue = text.nodeValue;

      assertEquals("normalizedValue","barCDATA",nodeValue);
       
}




function runTest() {
   documentnormalizedocument03();
}
