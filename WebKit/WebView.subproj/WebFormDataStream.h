/*
    WebFormDataStream.h
    Copyright 2005 Apple Computer, Inc.
*/

@class NSArray;
@class NSMutableURLRequest;

void webSetHTTPBody(NSMutableURLRequest *request, NSArray *formData);
