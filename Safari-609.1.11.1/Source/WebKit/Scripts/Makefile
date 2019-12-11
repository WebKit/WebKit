all:
	python ./generate-message-receiver.py webkit/test-superclass-messages.in --implementation webkit/MessageReceiverSuperclass-expected.cpp --header webkit/MessagesSuperclass-expected.h --reply-header webkit/MessagesRepliesSuperclassReplies-expected.h
	python ./generate-message-receiver.py webkit/test-legacy-messages.in --implementation webkit/LegacyMessageReceiver-expected.cpp --header webkit/LegacyMessages-expected.h --reply-header webkit/LegacyMessagesReplies-expected.h
	python ./generate-message-receiver.py webkit/test-messages.in --implementation webkit/MessageReceiver-expected.cpp --header webkit/Messages-expected.h --reply-header webkit/MessagesReplies-expected.h

