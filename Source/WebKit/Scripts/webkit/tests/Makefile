
TESTS = \
    TestWithCVPixelBuffer \
    TestWithWantsDispatch \
    TestWithWantsDispatchNoSyncMessages \
    TestWithWantsAsyncDispatch \
    TestWithEnabledBy \
    TestWithEnabledByAndConjunction \
    TestWithEnabledByOrConjunction \
    TestWithEnabledIf \
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
#

all:
	python3 ../../generate-serializers.py cpp TestSerializedType.serialization.in
	python3 ../../generate-message-receiver.py . $(TESTS)
