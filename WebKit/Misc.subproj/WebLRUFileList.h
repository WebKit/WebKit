/*	
    WebLRUFileList.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <CoreFoundation/CoreFoundation.h>

typedef struct WebLRUFileList WebLRUFileList;

WebLRUFileList *WebLRUFileListCreate(void);
void WebLRUFileListRelease(WebLRUFileList *list);

int WebLRUFileListRebuildFileDataUsingRootDirectory(WebLRUFileList *list, const char *path);

unsigned int WebLRUFileListRemoveFileWithPath(WebLRUFileList *list, const char *path);
void WebLRUFileListTouchFileWithPath(WebLRUFileList *list, const char *path);
void WebLRUFileListSetFileData(WebLRUFileList *list, const char *path, unsigned long fileSize, CFTimeInterval time);

Boolean WebLRUFileListGetPathOfOldestFile(WebLRUFileList *list, char *buffer, unsigned int length);
unsigned int WebLRUFileListRemoveOldestFileFromList(WebLRUFileList *list);

Boolean WebLRUFileListContainsItem(WebLRUFileList *list, const char *path);
unsigned int WebLRUFileListGetFileSize(WebLRUFileList *list, const char *path);
unsigned int WebLRUFileListCountItems(WebLRUFileList *list);
unsigned int WebLRUFileListGetTotalSize(WebLRUFileList *list);
void WebLRUFileListRemoveAllFilesFromList(WebLRUFileList *list);

NSString *WebLRUFileListDescription(WebLRUFileList *list);
