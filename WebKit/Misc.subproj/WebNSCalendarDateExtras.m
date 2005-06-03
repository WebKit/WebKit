/*
    WebNSCalendarDateExtras.m
    Private (SPI) header
    Copyright 2005, Apple, Inc. All rights reserved.
 */

#import <WebKit/WebNSCalendarDateExtras.h>

#import <WebKit/WebAssertions.h>

@implementation NSCalendarDate (WebNSCalendarDateExtras)

- (NSComparisonResult) _webkit_compareDay: (NSCalendarDate *)anotherDate
{
    int year, yearOther, month, monthOther, day, dayOther;
    
    if (self == anotherDate) {
        return NSOrderedSame;
    }

    year = [self yearOfCommonEra];
    yearOther = [anotherDate yearOfCommonEra];
    if (year < yearOther) {
        return NSOrderedAscending;
    }
    if (year > yearOther) {
        return NSOrderedDescending;
    }

    month = [self monthOfYear];
    monthOther = [anotherDate monthOfYear];
    if (month < monthOther) {
        return NSOrderedAscending;
    }
    if (month > monthOther) {
        return NSOrderedDescending;
    }

    day = [self dayOfMonth];
    dayOther = [anotherDate dayOfMonth];
    if (day < dayOther) {
        return NSOrderedAscending;
    }
    if (day > dayOther) {
        return NSOrderedDescending;
    }

    return NSOrderedSame;
}

@end

