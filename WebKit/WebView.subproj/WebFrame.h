/*	
        IFWebFrame.h
	    
	    Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/IFWebController.h>

@class IFWebDataSource;

@interface IFWebFrame : NSObject
{
@private
    id _framePrivate;
}

- initWithName: (NSString *)name view: view dataSource: (IFWebDataSource *)dataSource controller: (id <IFWebController>)controller;
- (NSString *)name;


- (void)setController: (id <IFWebController>)controller;
- (id <IFWebController>)controller;

- (void)setView: view;
- view;

/*
    Sets the frame's data source.  Note that the data source will be
    considered 'provisional' until it has been loaded, and at least
    ~some~ data has been received.
*/
- (void)setDataSource: (IFWebDataSource *)ds;
- (IFWebDataSource *)dataSource;

- (IFWebDataSource *)provisionalDataSource;

- (void)reset;

@end
