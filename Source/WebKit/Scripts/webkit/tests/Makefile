
TESTS = \
    TestWithCVPixelBuffer \
    TestWithDeferSendingOption \
    TestWithDispatchedFromAndTo \
    TestWithEnabledBy \
    TestWithEnabledByAndConjunction \
    TestWithEnabledByOrConjunction \
    TestWithIfMessage \
    TestWithImageData \
    TestWithLegacyReceiver \
    TestWithMultiLineExtendedAttributes \
    TestWithoutAttributes \
    TestWithoutUsingIPCConnection \
    TestWithSemaphore \
    TestWithSpanOfConst \
    TestWithStream \
    TestWithStreamBatched \
    TestWithStreamBuffer \
    TestWithStreamServerConnectionHandle \
    TestWithSuperclass \
    TestWithSuperclassAndWantsAsyncDispatch \
    TestWithSuperclassAndWantsDispatch \
    TestWithValidator \
    TestWithWantsAsyncDispatch \
    TestWithWantsDispatch \
    TestWithWantsDispatchNoSyncMessages \
#

all:
	python3 ../../generate-serializers.py cpp TestSerializedType.serialization.in
	python3 ../../generate-message-receiver.py . $(TESTS)
	python3 ../opaque_ipc_types.py
	# The following are negative tests which are expected to throw exceptions.
	python3 -c "import sys, subprocess; result = subprocess.call(['python3', '../../generate-serializers.py', 'cpp', 'TestInvalidAttributes.serialization.in'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL); sys.exit(0) if result == 1 else sys.exit(1)"
