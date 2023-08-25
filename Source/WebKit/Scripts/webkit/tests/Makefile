
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
    TestWithEnabledIf \
#

all:
	python3 ../../generate-serializers.py cpp DIRECTORY ./ TestSerializedType.serialization.in
	python3 ../../generate-message-receiver.py . $(TESTS)
