/*	
    WebHistoryList.m
    Copyright 2001, Apple, Inc. All rights reserved.
*/

#import "WebHistoryList.h"
#import "WebHistoryItem.h"
#import <WebFoundation/WebAssertions.h>

struct WebHistoryListNode
{
    unsigned hash;
    WebHistoryItem *entry;
    WebHistoryListNode *prev;
    WebHistoryListNode *next;
};

static WebHistoryListNode *newURIListNode(WebHistoryItem *entry)
{
    WebHistoryListNode *node;

    [entry retain];
    
    node = malloc(sizeof(WebHistoryListNode));
    node->hash = [entry hash];
    node->entry = entry;
    node->prev = nil;
    node->next = nil;
    
    return node;    
}

static void freeNode(WebHistoryListNode *node)
{
    // It is important to autorelase here rather than using 
    // a straight release since we often return an entry
    // as a result of an operation that causes a node
    // to be freed.
    [node->entry autorelease];
    free(node);    
}


@implementation WebHistoryList

-(id)init
{
    if (self != [super init])
    {
        return nil;
    }
    
    _head = _tail = nil;
    _count = 0;
    _maximumSize = -1;
    _allowsDuplicates = NO;
    
    return self;
}

-(void)dealloc
{
    WebHistoryListNode *curNode;
    WebHistoryListNode *delNode;

    curNode = _head;

    while (curNode) {
        delNode = curNode;
        curNode = curNode->next;
        freeNode(delNode);
    }

    [super dealloc];
}

-(BOOL)allowsDuplicates
{
    return _allowsDuplicates;
}

-(void)setAllowsDuplicates:(BOOL)allowsDuplicates
{
    _allowsDuplicates = allowsDuplicates;
}

-(int)count
{
    return _count;
}

-(int)maximumSize
{
    return _maximumSize;
}

-(void)setMaximumSize:(int)size
{
    ASSERT(size > 0 || size == -1);
    _maximumSize = size;
}

-(void)addEntry:(WebHistoryItem *)entry
{
    if (!_allowsDuplicates) {
        // search the list first and remove any entry with the same URL
        // having the same title does not count
        [self removeEntry:entry];
    }

    // make new entry and put it at the head of the list
    WebHistoryListNode *node = newURIListNode(entry);
    node->next = _head;
    _head = node;
    _count++;
    if (_count == 1) {
        _tail = _head;
    }
    
    if (_maximumSize != -1 && _count > _maximumSize) {
        // drop off the tail
        node = _tail;
        _tail = _tail->prev;
        _tail->next = nil;
        freeNode(node);
        _count--;
    }
}

-(void)removeEntry:(WebHistoryItem *)entry
{
    WebHistoryItem *removedEntry = nil;
    NSURL *URL = [entry URL];
    unsigned hash = [URL hash];

    WebHistoryListNode *node;
    for (node = _head; node != nil; node = node->next) {
        if (hash == node->hash && [URL isEqual:[node->entry URL]]) {
            _count--;
            removedEntry = node->entry;
            if (node == _head) {
                _head = node->next;
                if (_head) {
                    _head->prev = nil;
                }
            }
            else if (node == _tail) {
                _tail = node->prev;
                if (_tail) {
                    _tail->next = nil;
                }
            }
            else {
                node->prev->next = node->next;
                node->next->prev = node->prev;
            }
            freeNode(node);
            break;
        }
    }
}

-(WebHistoryItem *)entryForURL:(NSURL *)URL
{
    WebHistoryItem *foundEntry = nil;
    unsigned hash = [URL hash];

    WebHistoryListNode *node;
    for (node = _head; node != nil; node = node->next) {
        if (hash == node->hash && [URL isEqual:[node->entry URL]]) {
            foundEntry = node->entry;
            break;
        }
    }
    
    return foundEntry;
}

-(WebHistoryItem *)entryAtIndex:(int)index
{
    ASSERT(index >= 0 && index < _count);

    WebHistoryListNode *node = _head;
    int i;
    for (i = 0; i < index; i++) {
        node = node->next;
    }

    return node->entry;    
}

-(void)replaceEntryAtIndex:(int)index withEntry:(WebHistoryItem *)entry
{
    ASSERT(index >= 0 && index < _count);

    WebHistoryListNode *node = _head;
    int i;
    for (i = 0; i < index; i++) {
        node = node->next;
    }

    [node->entry autorelease];
    node->entry = [entry retain];
}

-(WebHistoryItem *)removeEntryAtIndex:(int)index
{
    ASSERT(index > 0 && index < _count);

    WebHistoryListNode *node = _head;
    int i;
    for (i = 0; i < index; i++) {
        node = node->next;
    }

    _count--;
    WebHistoryItem *removedEntry = node->entry;
    if (node == _head) {
        _head = node->next;
        if (_head) {
            _head->prev = nil;
        }
    }
    else if (node == _tail) {
        _tail = node->prev;
        if (_tail) {
            _tail->next = nil;
        }
    }
    else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }
    freeNode(node);

    return removedEntry;
}

-(void)removeEntriesToIndex:(int)index
{
    ASSERT(index > 0 && index < _count);

    WebHistoryListNode *node = _head;
    int i;
    for (i = 0; i < index; i++) {
        WebHistoryListNode *next = node->next;
        freeNode(node);
        _count--;
        node = next;
    }
    
    _head = node;
}

@end
