/*
    WebNSCalendarDateExtras.h
    Private (SPI) header
    Copyright 2003, Apple, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>

@interface NSCalendarDate (WebNSCalendarDateExtras)
- (NSComparisonResult)_webkit_compareDay: (NSCalendarDate *)anotherDate;
@end
