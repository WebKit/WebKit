// This depends on audit.js to define the |should| function used herein.
//
// Test that setPosition throws an error if there is already a
// setValueCurve scheduled during the same time period.
function testPositionSetterVsCurve(should, context, options) {
  // Create the graph consisting of a source node and the panner.
  let src = new ConstantSourceNode(context, {offset: 1});
  let panner = new PannerNode(context);
  src.connect(panner).connect(context.destination);

  let curve = Float32Array.from([-10, 10]);

  // Determine if we're testing the panner or the listener and set the node
  // appropriately.
  let testNode = options.nodeName === 'panner' ? panner : context.listener;

  let prefix = options.nodeName === 'panner' ? 'panner.' : 'listener.';

  let message = prefix + options.paramName + '.setValueCurve(..., 0, ' +
      options.curveDuration + ')';

  // If the coordinate name has 'position', we're testing setPosition;
  // otherwise assume we're testing setOrientation.
  let methodName =
      options.paramName.includes('position') ? 'setPosition' : 'setOrientation';

  // Sanity check that we're testing the right node. (The |testName| prefix is
  // to make each message unique for testharness.)
  if (options.nodeName === 'panner') {
    should(
        testNode instanceof PannerNode,
        options.testName + ': Node under test is a PannerNode')
        .beTrue();
  } else {
    should(
        testNode instanceof AudioListener,
        options.testName + ': Node under test is an AudioLIstener')
        .beTrue();
  }

  // Set the curve automation on the specified axis.
  should(() => {
    testNode[options.paramName].setValueCurveAtTime(
        curve, 0, options.curveDuration);
  }, message).notThrow();

  let resumeContext = context.resume.bind(context);

  // Get correct argument string for the setter for printing the message.
  let setterArguments;
  if (options.nodeName === 'panner') {
    setterArguments = '(1,1,1)';
  } else {
    if (methodName === 'setPosition') {
      setterArguments = '(1,1,1)';
    } else {
      setterArguments = '(1,1,1,1,1,1)';
    }
  }

  let setterMethod = () => {
    // It's ok to give extra args.
    testNode[methodName](1, 1, 1, 1, 1, 1);
  };

  if (options.suspendFrame) {
    // We're testing setPosition after the curve has ended to verify that
    // we don't throw an error.  Thus, suspend the context and call
    // setPosition.
    let suspendTime = options.suspendFrame / context.sampleRate;
    context.suspend(suspendTime)
        .then(() => {
          should(
              setterMethod,
              prefix + methodName + setterArguments + ' for ' +
                  options.paramName + ' at time ' + suspendTime)
              .notThrow();
        })
        .then(resumeContext);
  } else {
    // Basic test where setPosition is called where setValueCurve is
    // already active.
    context.suspend(0)
        .then(() => {
          should(
              setterMethod,
              prefix + methodName + setterArguments + ' for ' +
                  options.paramName)
              .throw(DOMException, 'NotSupportedError');
        })
        .then(resumeContext);
  }

  src.start();
  return context.startRendering();
}
