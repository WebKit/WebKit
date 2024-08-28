
TESTS = \
    TestWithSuperclass \
    TestWithLegacyReceiver \
    TestWithoutAttributes \
    TestWithoutUsingIPCConnection \
    TestWithIfMessage \
    TestWithSemaphore \
    TestWithImageData \
    TestWithStream \
    TestWithStreamBatched \
    TestWithStreamBuffer \
    TestWithCVPixelBuffer \
    TestWithStreamServerConnectionHandle \
    TestWithEnabledBy \
    TestWithEnabledIf \
    TestWithEnabledByAndConjunction \
    TestWithEnabledByOrConjunction \
#

all:
	python3 ../../generate-serializers.py cpp TestSerializedType.serialization.in
	python3 ../../generate-message-receiver.py . $(TESTS)
