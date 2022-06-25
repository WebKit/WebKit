
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodegetfeature07";
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


/**
* 
Check implementation of Node.getFeature on namespaced attribute.

* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Node3-getFeature
*/
function nodegetfeature07() {
   var success;
    if(checkInitialization(builder, "nodegetfeature07") != null) return;
    var doc;
      var node;
      var nullVersion = null;

      var featureImpl;
      var isSupported;
      var domImpl;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "barfoo");
      domImpl = doc.implementation;
node = doc.createAttributeNS("http://www.w3.org/XML/1998/namespace","xml:lang");
      featureImpl = node.getFeature("Core",nullVersion);
      assertSame("coreUnspecifiedVersion",node,featureImpl);
featureImpl = node.getFeature("cOrE",nullVersion);
      assertSame("cOrEUnspecifiedVersion",node,featureImpl);
featureImpl = node.getFeature("+cOrE",nullVersion);
      assertSame("PlusCoreUnspecifiedVersion",node,featureImpl);
featureImpl = node.getFeature("org.w3c.domts.bogus.feature",nullVersion);
      assertNull("unrecognizedFeature",featureImpl);
    featureImpl = node.getFeature("cOrE","2.0");
      assertSame("Core20",node,featureImpl);
featureImpl = node.getFeature("cOrE","3.0");
      assertSame("Core30",node,featureImpl);
isSupported = node.isSupported("XML",nullVersion);
      featureImpl = node.getFeature("XML",nullVersion);
      
	if(
	isSupported
	) {
	assertSame("XMLUnspecified",node,featureImpl);

	}
	isSupported = node.isSupported("SVG",nullVersion);
      featureImpl = node.getFeature("SVG",nullVersion);
      
	if(
	isSupported
	) {
	assertSame("SVGUnspecified",node,featureImpl);

	}
	isSupported = node.isSupported("HTML",nullVersion);
      featureImpl = node.getFeature("HTML",nullVersion);
      
	if(
	isSupported
	) {
	assertSame("HTMLUnspecified",node,featureImpl);

	}
	isSupported = node.isSupported("Events",nullVersion);
      featureImpl = node.getFeature("Events",nullVersion);
      
	if(
	isSupported
	) {
	assertSame("EventsUnspecified",node,featureImpl);

	}
	isSupported = node.isSupported("LS",nullVersion);
      featureImpl = node.getFeature("LS",nullVersion);
      
	if(
	isSupported
	) {
	assertSame("LSUnspecified",node,featureImpl);

	}
	isSupported = node.isSupported("LS-Async",nullVersion);
      featureImpl = node.getFeature("LS-Async",nullVersion);
      
	if(
	isSupported
	) {
	assertSame("LSAsyncUnspecified",node,featureImpl);

	}
	isSupported = node.isSupported("XPath",nullVersion);
      featureImpl = node.getFeature("XPath",nullVersion);
      
	if(
	isSupported
	) {
	assertSame("XPathUnspecified",node,featureImpl);

	}
	isSupported = node.isSupported("+HTML",nullVersion);
      featureImpl = node.getFeature("HTML",nullVersion);
      
	if(
	isSupported
	) {
	assertNotNull("PlusHTMLUnspecified",featureImpl);

	}
	isSupported = node.isSupported("+SVG",nullVersion);
      featureImpl = node.getFeature("SVG",nullVersion);
      
	if(
	isSupported
	) {
	assertNotNull("PlusSVGUnspecified",featureImpl);

	}
	
}




function runTest() {
   nodegetfeature07();
}
