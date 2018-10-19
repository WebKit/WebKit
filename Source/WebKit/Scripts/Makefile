all:
	python ./generate-messages-header.py webkit/test-superclass-messages.in > webkit/MessagesSuperclass-expected.h
	python ./generate-messages-header.py webkit/test-legacy-messages.in > webkit/LegacyMessages-expected.h
	python ./generate-messages-header.py webkit/test-messages.in > webkit/Messages-expected.h
	python ./generate-message-receiver.py webkit/test-superclass-messages.in > webkit/MessageReceiverSuperclass-expected.cpp
	python ./generate-message-receiver.py webkit/test-legacy-messages.in > webkit/LegacyMessageReceiver-expected.cpp
	python ./generate-message-receiver.py webkit/test-messages.in > webkit/MessageReceiver-expected.cpp

