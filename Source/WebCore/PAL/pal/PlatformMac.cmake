include(PlatformCocoa.cmake)

list(APPEND PAL_PUBLIC_HEADERS
    spi/mac/HIServicesSPI.h
    spi/mac/HIToolboxSPI.h
    spi/mac/LookupSPI.h
    spi/mac/NSAppearanceSPI.h
    spi/mac/NSApplicationSPI.h
    spi/mac/NSCellSPI.h
    spi/mac/NSColorSPI.h
    spi/mac/NSColorWellSPI.h
    spi/mac/NSEventSPI.h
    spi/mac/NSGraphicsSPI.h
    spi/mac/NSImageSPI.h
    spi/mac/NSImmediateActionGestureRecognizerSPI.h
    spi/mac/NSMenuSPI.h
    spi/mac/NSPasteboardSPI.h
    spi/mac/NSPopoverColorWellSPI.h
    spi/mac/NSPopoverSPI.h
    spi/mac/NSResponderSPI.h
    spi/mac/NSScrollViewSPI.h
    spi/mac/NSScrollerImpSPI.h
    spi/mac/NSScrollingInputFilterSPI.h
    spi/mac/NSScrollingMomentumCalculatorSPI.h
    spi/mac/NSSearchFieldCellSPI.h
    spi/mac/NSServicesRolloverButtonCellSPI.h
    spi/mac/NSSharingServicePickerSPI.h
    spi/mac/NSSharingServiceSPI.h
    spi/mac/NSSpellCheckerSPI.h
    spi/mac/NSTextFieldCellSPI.h
    spi/mac/NSTextFinderSPI.h
    spi/mac/NSTextInputContextSPI.h
    spi/mac/NSTextTableSPI.h
    spi/mac/NSUndoManagerSPI.h
    spi/mac/NSViewSPI.h
    spi/mac/NSWindowSPI.h
    spi/mac/PIPSPI.h
    spi/mac/QuarantineSPI.h
    spi/mac/TelephonyUtilitiesSPI.h

)

list(APPEND PAL_UNIFIED_SOURCE_LIST_FILES
    "SourcesMac.txt"
)
