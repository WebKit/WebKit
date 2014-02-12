/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "JSModule.h"
#import <JavaScriptCore/JavaScriptCore.h>

@implementation JSModule {
    JSManagedValue *_exports;
    NSString *_id;
    NSString *_filename;
    bool _loaded;
    __weak JSModule *_parent;
    NSMutableArray *_children;
}

// TODO: Get lib in the right place. Right now we copy it to the build results directory.
static NSDictionary *coreModules()
{
    static NSDictionary *modules = 0;
    if (!modules) {
        // Put location of built-in modules here.
        modules = @{
            //@"util": [libDir stringByAppendingPathComponent:@"util.js"],
        };
    }
    return modules;
}

static bool isCoreModule(NSString *moduleName)
{
    return !![coreModules() objectForKey:moduleName];
}

static Class classForModule(NSString *moduleName)
{
    static NSDictionary *moduleClasses = 0;
    if (!moduleClasses) {
        // Put classes for built-in modules here.
        moduleClasses = @{
            //@"util": [JSUtilModule class],
        };
    }
    
    if (isCoreModule(moduleName))
        return [moduleClasses objectForKey:moduleName];
    return [JSModule class];
}

static NSString *coreModuleFullPath(NSString *moduleName)
{
    assert(isCoreModule(moduleName));
    return [coreModules() objectForKey:moduleName];
}

static NSString *resolveModuleAsFile(NSString *modulePath)
{
    if ([[NSFileManager defaultManager] fileExistsAtPath:modulePath])
        return modulePath;
    
    if ([[NSFileManager defaultManager] fileExistsAtPath:[modulePath stringByAppendingPathExtension:@"js"]])
        return [modulePath stringByAppendingPathExtension:@"js"];
    
    return nil;
}

static NSString *resolveModuleAsDirectory(NSString *modulePath)
{
    if ([[NSFileManager defaultManager] fileExistsAtPath:[modulePath stringByAppendingPathComponent:@"package.json"]]) {
        NSString *path = [modulePath stringByAppendingString:@"package.json"];
        NSString *fileContents = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil];
        JSContext *tempContext = [[JSContext alloc] init];
        NSString *script = [NSString stringWithFormat:@"JSON.parse(%@)", fileContents];
        JSValue *result = [tempContext evaluateScript:script];
        if (![result[@"main"] isUndefined])
            return resolveModuleAsFile([result[@"main"] toString]);
    }
    
    if ([[NSFileManager defaultManager] fileExistsAtPath:[modulePath stringByAppendingPathComponent:@"index.js"]])
        return [modulePath stringByAppendingPathComponent:@"index.js"];
    
    return nil;
}

static NSArray *nodeModulePaths(NSString *start)
{
    NSArray *parts = [start pathComponents];
    NSUInteger root = [parts indexOfObject:@"node_modules"];
    if (root == NSNotFound)
        root = 0;
    
    NSUInteger i = [parts count] - 1;
    NSMutableArray *dirs = [[NSMutableArray alloc] init];
    while (i > root) {
        NSString *component = [parts objectAtIndex:i];
        if ([component isEqualToString:@"node_modules"]) {
            i -= 1;
            continue;
        }
        [dirs addObject:[NSString pathWithComponents:[parts subarrayWithRange:NSMakeRange(0, i)]]];
        i -= 1;
    }
    
    return dirs;
}

static NSString *resolveAsNodeModule(NSString *moduleName, NSString* start)
{
    NSArray *dirs = nodeModulePaths(start);
    for (NSUInteger i = 0; i < [dirs count]; i++) {
        NSString *dir = [dirs objectAtIndex:i];
        NSString *result = resolveModuleAsFile([NSString stringWithFormat:@"%@/%@", dir, moduleName]);
        if (result)
            return result;
        
        result = resolveModuleAsDirectory([NSString stringWithFormat:@"%@/%@", dir, moduleName]);
        if (result)
            return result;
    }
    return nil;
}

+ (NSString *)resolve:(NSString *)module atPath:(NSString *)path
{
    if (isCoreModule(module))
        return coreModuleFullPath(module);
    
    NSString *result;
    if ([module hasPrefix:@"./"] || [module hasPrefix:@"/"] || [module hasPrefix:@"../"]) {
        result = resolveModuleAsFile([NSString stringWithFormat:@"%@/%@", path, module]);
        
        if (result)
            return result;
        
        result = resolveModuleAsDirectory([NSString stringWithFormat:@"%@/%@", path, module]);
        if (result)
            return result;
    }
    
    return resolveAsNodeModule(module, [path stringByDeletingLastPathComponent]);
}

static NSMapTable *globalModuleCache()
{
    static NSMapTable *moduleCache = nil;
    if (!moduleCache)
        moduleCache = [NSMapTable strongToStrongObjectsMapTable];
    return moduleCache;
}

static bool isCached(NSString *fullModulePath)
{
    return !![globalModuleCache() objectForKey:fullModulePath];
}

static JSModule *cachedModule(NSString *fullModulePath)
{
    assert(isCached(fullModulePath));
    return [globalModuleCache() objectForKey:fullModulePath];
}

static void cacheModule(NSString *fullModulePath, JSModule *module)
{
    assert(!isCached(fullModulePath));
    [globalModuleCache() setObject:module forKey:fullModulePath];
}

static JSModule *currentLoadingModule = nil;

+ (JSModule *)require:(NSString *)module atPath:(NSString *)path
{
    return [JSModule require:module atPath:path inContext:[JSContext currentContext]];
}

+ (JSModule *)require:(NSString *)module atPath:(NSString *)path inContext:(JSContext *)context
{
    assert(context);
    NSString *fullModulePath = [JSModule resolve:module atPath:path];
    
    if (!fullModulePath)
        return nil;
    
    if (isCached(fullModulePath))
        return cachedModule(fullModulePath);
    
    Class moduleClass = classForModule(module);
    JSModule *newModule = [[moduleClass alloc] initWithId:fullModulePath filename:fullModulePath context:context];
    cacheModule(fullModulePath, newModule);
    
    NSString *script = [NSString stringWithContentsOfFile:fullModulePath encoding:NSUTF8StringEncoding error:nil];
    
    [newModule didStartLoading];
    
    JSValue *savedExports = context[@"exports"];
    context[@"exports"] = [newModule exports];

    JSValue *savedModule = context[@"module"];
    context[@"module"] = newModule;
    context[@"module"][@"exports"] = [newModule exports];
    
    JSValue *savedPlatformObject = context[@"platform"];
    context[@"platform"] = [newModule platformObjectInContext:context];
    
    // Load the module.
    [context evaluateScript:script];
    
    [context.virtualMachine removeManagedReference:newModule->_exports withOwner:self];
    newModule->_exports = [JSManagedValue managedValueWithValue:context[@"module"][@"exports"]];
    [context.virtualMachine addManagedReference:newModule->_exports withOwner:self];
    
    context[@"platform"] = savedPlatformObject;
    context[@"module"] = savedModule;
    context[@"exports"] = savedExports;

    [newModule didFinishLoading];

    return newModule;
}

- (JSValue *)platformObjectInContext:(JSContext *)context
{
    // Override this method in subclasses of JSModule to return a platform object
    // with whatever functionality required to implement that module.
    return [JSValue valueWithUndefinedInContext:context];
}

- (id)initWithId:(NSString *)myId filename:(NSString *)filename context:(JSContext *)context
{
    self = [super init];
    if (!self)
        return nil;
    
    _exports = [JSManagedValue managedValueWithValue:[JSValue valueWithNewObjectInContext:context]];
    [context.virtualMachine addManagedReference:_exports withOwner:self];
    _id = myId;
    _filename = filename;
    _loaded = false;
    _parent = nil;
    _children = [[NSMutableArray alloc] init];
    
    return self;
}

- (void)dealloc
{
    JSValue *exports = [_exports value];
    if (exports)
        [exports.context.virtualMachine removeManagedReference:_exports withOwner:self];
}

- (void)didStartLoading
{
    _parent = currentLoadingModule;
    if (currentLoadingModule)
        [currentLoadingModule->_children addObject:self];
    currentLoadingModule = self;
}

- (void)didFinishLoading
{
    _loaded = true;
    currentLoadingModule = _parent;
}

- (JSModule *)require:(NSString *)module
{
    // TODO
    return nil;
}

- (JSValue *)exports
{
    return [_exports value];
}

@end