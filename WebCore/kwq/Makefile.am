NULL =

SUBDIRS = \
	kde \
	qt \
	$(NULL)

noinst_LIBRARIES = libkwq.o
libkwq_LDFLAGS = -Wl,-r -nostdlib
libkwq_o_AR = $(OBJCXXLD) $(AM_OBJCXXFLAGS) $(OBJCXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) $(libkwq_LDFLAGS) -o

libkwq_o_SOURCES = \
	KWQApplication.mm \
	KWQArrayImpl.mm \
	KWQBitmap.mm \
	KWQBrush.mm \
	KWQBuffer.mm \
	KWQButton.mm \
	KWQCString.mm \
	KWQChar.mm \
	KWQCharsets.mm \
	KWQCheckBox.mm \
	KWQCollection.mm \
	KWQColor.mm \
	KWQColorGroup.mm \
	KWQComboBox.mm \
	KWQCompletion.mm \
	KWQCursor.mm \
	KWQDCOPClient.mm \
	KWQDateTime.mm \
	KWQDictImpl.mm \
	KWQDir.mm \
	KWQDrawUtil.mm \
	KWQEvent.mm \
	KWQFile.mm \
	KWQFont.mm \
	KWQFontDatabase.mm \
	KWQFontInfo.mm \
	KWQFontMetrics.mm \
	KWQFrame.mm \
	KWQGuardedPtr.mm \
	KWQPoint.mm \
	KWQPtrDictImpl.mm \
	KWQRect.mm \
	KWQSize.mm \
	KWQHBox.mm \
	KWQImage.mm \
	KWQInputDialog.mm \
	KWQKApplication.mm \
	KWQKCharsets.mm \
	KWQKComboBox.mm \
	KWQKCompletionBox.mm \
	KWQKConfigBase.mm \
	KWQKCursor.mm \
	KWQKDebug.mm \
	KWQKFactory.mm \
	KWQKFileDialog.mm \
	KWQKFileItem.mm \
	KWQKGlobal.mm \
	KWQKGlobalSettings.mm \
	KWQKHTMLFactory.mm \
	KWQKHTMLPart.mm \
	KWQKHTMLPartBrowserExtension.mm \
	KWQKHTMLSettings.mm \
	KWQKHTMLView.mm \
	KWQKHistoryProvider.mm \
	KWQKIconLoader.mm \
	KWQKImageIO.mm \
	KWQKInstance.mm \
	KWQKJavaEmbed.mm \
	KWQKJob.mm \
	KWQKLineEdit.mm \
	KWQKListBox.mm \
	KWQKLocale.mm \
	KWQKMessageBox.mm \
	KWQKMimeType.mm \
	KWQKPartsEvent.mm \
	KWQKPrinter.mm \
	KWQKProcess.mm \
	KWQKProtocolManager.mm \
	KWQKScheduler.mm \
	KWQKSimpleConfig.mm \
	KWQKStringHandler.mm \
	KWQKURL.mm \
	KWQKWin.mm \
	KWQKWinModule.mm \
	KWQKjobclasses.mm \
	KWQKloader.mm \
	KWQKnetaccess.mm \
	KWQKstddirs.mm \
	KWQLabel.mm \
	KWQLineEdit.mm \
	KWQListBox.mm \
	KWQListImpl.mm \
	KWQMapImpl.mm \
	KWQMovie.mm \
	KWQNSTextField.mm \
	KWQObject.mm \
	KWQPaintDevice.mm \
	KWQPaintDeviceMetrics.mm \
	KWQPainter.mm \
	KWQPalette.mm \
	KWQPart.mm \
	KWQPen.mm \
	KWQPixmap.mm \
	KWQPointArray.mm \
	KWQPushButton.mm \
	KWQRadioButton.mm \
	KWQRegExp.mm \
	KWQRegion.mm \
	KWQScrollView.mm \
	KWQString.mm \
	KWQStringList.mm \
	KWQStyle.mm \
	KWQTextArea.mm \
	KWQTextContainer.mm \
	KWQTextEdit.mm \
	KWQTextStorage.mm \
	KWQTimer.mm \
	KWQToolTip.mm \
	KWQValueListImpl.mm \
	KWQVariant.mm \
	KWQVectorImpl.mm \
	KWQView.mm \
	KWQWMatrix.mm \
	KWQWidget.mm \
	KWQasyncimageio.mm \
	KWQasyncio.mm \
	KWQbrowserextension.mm \
	KWQbrowserinterface.mm \
	KWQsignals.mm \
	KWQtextcodec.mm \
	KWQtextstream.mm \
	KWQxml.mm \
	kwqdebug.mm \
	WCPlugin.mm \
	WCPluginDatabase.mm \
	WCPluginWidget.mm \
        WCJavaAppletWidget.mm \
	npapi.mm \
	$(NULL)

libkwq_o_LIBADD = \
	./qt/libqt_borrowed.o \
	./kde/libkde_borrowed.o \
	$(NULL)

INCLUDES = \
	-DNEED_BOGUS_X_DEFINES \
	$(KWQ_INCLUDES) \
	$(KDELIBS_INCLUDES) \
	$(WEBCORE_INCLUDES) \
	-I/System/Library/Frameworks/ApplicationServices.framework/Frameworks/ \
	-F/System/Library/Frameworks/ApplicationServices.framework/Frameworks \
	-I/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/PrivateHeaders \
	-F$(SYMROOTS) \
	$(NULL)

noinst_PROGRAMS = \
	make-mac-encodings \
	$(NULL)

make_mac_encodings_SOURCES = make-mac-encodings.c

make_mac_encodings_LDFLAGS = \
	-framework CoreFoundation \
	$(NULL)

mac-encodings.txt: make-mac-encodings
	./make-mac-encodings $@

KWQCharsetData.c: make-charset-table.pl character-sets.txt mac-encodings.txt
	./make-charset-table.pl character-sets.txt mac-encodings.txt KWQCharsetData.c 

BUILT_SOURCES = \
	make-mac-encodings \
	mac-encodings.txt \
	KWQCharsetData.c \
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
