
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level2/events/dispatchEvent13";
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


     /**
      *    Inner class implementation for variable listener1 
      */
var listener1;

/**
        * Constructor

        * @param events Value from value attribute of nested var element
        * @param listeners Value from value attribute of nested var element
        */
	      
function EventListenerN1003B(events, listeners) { 
           this.events = events;
           this.listeners = listeners;
           }
   
        /**
         *    
This method is called whenever an event occurs of the type for which theEventListenerinterface was registered.

         * @param evt 
TheEventcontains contextual information about the event. It also contains thestopPropagationand preventDefaultmethods which are used in determining the event's flow and default action.

         */
EventListenerN1003B.prototype.handleEvent = function(evt) {
         //
         //   bring class variables into function scope
         //
        var events = listener1.events;
           var listeners = listener1.listeners;
           var target;
      var listener;
      events[events.length] = evt;
target = evt.currentTarget;

      for(var indexN10065 = 0;indexN10065 < listeners.length; indexN10065++) {
      listener = listeners[indexN10065];
      target.removeEventListener("foo", listener.handleEvent, false);
	 
	}
   }

     /**
      *    Inner class implementation for variable listener2 
      */
var listener2;

/**
        * Constructor

        * @param events Value from value attribute of nested var element
        * @param listeners Value from value attribute of nested var element
        */
	      
function EventListenerN10074(events, listeners) { 
           this.events = events;
           this.listeners = listeners;
           }
   
        /**
         *    
This method is called whenever an event occurs of the type for which theEventListenerinterface was registered.

         * @param evt 
TheEventcontains contextual information about the event. It also contains thestopPropagationand preventDefaultmethods which are used in determining the event's flow and default action.

         */
EventListenerN10074.prototype.handleEvent = function(evt) {
         //
         //   bring class variables into function scope
         //
        var events = listener2.events;
           var listeners = listener2.listeners;
           var target;
      var listener;
      events[events.length] = evt;
target = evt.currentTarget;

      for(var indexN10098 = 0;indexN10098 < listeners.length; indexN10098++) {
      listener = listeners[indexN10098];
      target.removeEventListener("foo", listener.handleEvent, false);
	 
	}
   }

/**
* 
Two listeners are registered on the same target, each of which will remove both itself and 
the other on the first event.  Only one should see the event since event listeners
can never be invoked after being removed.

* @author Curt Arnold
* @see http://www.w3.org/TR/DOM-Level-2-Events/events#Events-EventTarget-dispatchEvent
* @see http://www.w3.org/TR/DOM-Level-2-Events/events#xpointer(id('Events-EventTarget-dispatchEvent')/raises/exception[@name='EventException']/descr/p[substring-before(.,':')='UNSPECIFIED_EVENT_TYPE_ERR'])
*/
function dispatchEvent13() {
   var success;
    if(checkInitialization(builder, "dispatchEvent13") != null) return;
    var doc;
      var target;
      var evt;
      var preventDefault;
      var listeners = new Array();

      var events = new Array();

      listener1 = new EventListenerN1003B(events, listeners);
	  
      listener2 = new EventListenerN10074(events, listeners);
	  
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      listeners[listeners.length] = listener1;
listeners[listeners.length] = listener2;
doc.addEventListener("foo", listener1.handleEvent, false);
	 doc.addEventListener("foo", listener2.handleEvent, false);
	 evt = doc.createEvent("Events");
      evt.initEvent("foo",true,false);
      preventDefault = doc.dispatchEvent(evt);
      assertSize("eventCount",1,events);

}




function runTest() {
   dispatchEvent13();
}
