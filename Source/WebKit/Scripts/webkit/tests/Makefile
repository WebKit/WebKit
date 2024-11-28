
TESTS = \
    TestWithCVPixelBuffer \
    TestWithDeferSendingOption \
    TestWithEnabledBy \
    TestWithEnabledByAndConjunction \
    TestWithEnabledByOrConjunction \
    TestWithIfMessage \
    TestWithImageData \
    TestWithLegacyReceiver \
    TestWithoutAttributes \
    TestWithoutUsingIPCConnection \
    TestWithSemaphore \
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
