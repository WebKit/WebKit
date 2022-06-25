
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level2/events/dispatchEvent12";
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
      docsLoaded += preload(docRef, "doc", "hc_staff");
        
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

//EventMonitor's require a document level variable named monitor
var monitor;
	 
     /**
      *    Inner class implementation for variable other 
      */
var other;

/**
        * Constructor

        */
	      
function EventListenerN10035() { 
           }
   
        /**
         *    
This method is called whenever an event occurs of the type for which theEventListenerinterface was registered.

         * @param evt 
TheEventcontains contextual information about the event. It also contains thestopPropagationand preventDefaultmethods which are used in determining the event's flow and default action.

         */
EventListenerN10035.prototype.handleEvent = function(evt) {
         //
         //   bring class variables into function scope
         //
        }

/**
* 
A monitor is added, multiple calls to removeEventListener
are mde with similar but not identical arguments, and an event is dispatched.  
The monitor should recieve handleEvent calls.

* @author Curt Arnold
* @see http://www.w3.org/TR/DOM-Level-2-Events/events#Events-EventTarget-dispatchEvent
* @see http://www.w3.org/TR/DOM-Level-2-Events/events#xpointer(id('Events-EventTarget-dispatchEvent')/raises/exception[@name='EventException']/descr/p[substring-before(.,':')='UNSPECIFIED_EVENT_TYPE_ERR'])
*/
function dispatchEvent12() {
   var success;
    if(checkInitialization(builder, "dispatchEvent12") != null) return;
    var doc;
      var target;
      var evt;
      var preventDefault;
      monitor = new EventMonitor();
      
      other = new EventListenerN10035();
      
      var events = new Array();

      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      doc.addEventListener("foo", monitor.handleEvent, false);
      doc.removeEventListener("foo", monitor.handleEvent, true);
	 doc.removeEventListener("food", monitor.handleEvent, false);
	 doc.removeEventListener("foo", other.handleEvent, false);
	 evt = doc.createEvent("Events");
      evt.initEvent("foo",true,false);
      preventDefault = doc.dispatchEvent(evt);
      events = monitor.allEvents;
assertSize("eventCount",1,events);

}




function runTest() {
   dispatchEvent12();
}
