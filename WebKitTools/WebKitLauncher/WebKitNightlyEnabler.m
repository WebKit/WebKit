/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Graham Dennis.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Cocoa/Cocoa.h>
#include <mach-o/dyld.h>
#include <dlfcn.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <string.h>

static void cleanUpAfterOurselves(void) __attribute__ ((constructor));

void *symbol_lookup(char *symbol);

void cleanUpAfterOurselves(void)
{
    char **args = *(char***)_NSGetArgv();
    char **procPath = symbol_lookup("___CFProcessPath");
    char *procPathBackup = *procPath;
    *procPath = args[0];
    CFBundleGetMainBundle();
    *procPath = procPathBackup;
    unsetenv("DYLD_INSERT_LIBRARIES");
    unsetenv("CFProcessPath");
}

#if __LP64__
#define LC_SEGMENT_COMMAND LC_SEGMENT_64
#define macho_header mach_header_64
#define macho_segment_command segment_command_64
#define macho_section section_64
#define getsectdatafromheader getsectdatafromheader_64
#define macho_nlist nlist_64
#else
#define LC_SEGMENT_COMMAND LC_SEGMENT
#define macho_header mach_header
#define macho_segment_command segment_command
#define macho_section section
#define macho_nlist nlist
#endif

void *GDSymbolLookup(const struct macho_header *header, const char *symbol);

void *symbol_lookup(char *symbol)
{
    int i;
    for(i=0;i<_dyld_image_count();i++)
    {    
        void *symbolResult = GDSymbolLookup((const struct macho_header*)_dyld_get_image_header(i), symbol);
        if (symbolResult)
            return symbolResult;
    }
    return NULL;
}

void *GDSymbolLookup(const struct macho_header *header, const char *symbol)
{
    if (!header || !symbol)
        return NULL;
    if ((header->magic != MH_MAGIC) && (header->magic != MH_MAGIC_64))
        return NULL;
    
    uint32_t currCommand;
    const struct load_command *loadCommand = (const struct load_command *)( ((void *)header) + sizeof(struct macho_header));
    const struct macho_segment_command *segCommand;
    
    const struct symtab_command *symtabCommand = NULL;
    const struct dysymtab_command *dysymtabCommand = NULL;
    const struct macho_segment_command *textSegment = NULL;
    const struct macho_segment_command *linkEditSegment = NULL;
    
    for (currCommand = 0; currCommand < header->ncmds; currCommand++)
    {
        switch (loadCommand->cmd)
        {
            case LC_SEGMENT_COMMAND:
                segCommand = (const struct macho_segment_command *)loadCommand;
                if (strcmp(segCommand->segname, "__TEXT")==0)
                    textSegment = segCommand;
                else if (strcmp(segCommand->segname, "__LINKEDIT")==0)
                    linkEditSegment = segCommand;
                    break;
                
            case LC_SYMTAB:
                symtabCommand = (const struct symtab_command *)loadCommand;
                break;
                
            case LC_DYSYMTAB:
                dysymtabCommand = (const struct dysymtab_command *)loadCommand;
                break;
        }
        
        loadCommand = (const struct load_command *)(((void*)loadCommand) + loadCommand->cmdsize);
    }
    if (textSegment==NULL || linkEditSegment==NULL || symtabCommand==NULL || dysymtabCommand==NULL) {
        return NULL;
    }
    
    typedef enum { Start = 0, LocalSymbols, ExternalSymbols, Done } SymbolSearchState;
    uint32_t currentSymbolIndex;
    uint32_t maximumSymbolIndex;
    SymbolSearchState state;
    
    for (state = Start + 1; state < Done; state++)
    {
        switch(state) {
            case LocalSymbols:
                currentSymbolIndex = dysymtabCommand->ilocalsym;
                maximumSymbolIndex = dysymtabCommand->ilocalsym + dysymtabCommand->nlocalsym;
                break;
                
            case ExternalSymbols:
                currentSymbolIndex = dysymtabCommand->iextdefsym;
                maximumSymbolIndex = dysymtabCommand->nextdefsym;
                break;
                
            default:
                return NULL;
        }
        for (; currentSymbolIndex < maximumSymbolIndex; currentSymbolIndex++)
        {
            const struct macho_nlist *symbolTableEntry;
            symbolTableEntry = (const struct macho_nlist *)(currentSymbolIndex*sizeof(struct macho_nlist)
                                                            + (ptrdiff_t)header + symtabCommand->symoff
                                                            + linkEditSegment->vmaddr - linkEditSegment->fileoff
                                                            - textSegment->vmaddr);
            int32_t stringTableIndex = symbolTableEntry->n_un.n_strx;
            if (stringTableIndex<0)
                continue;
            const char *stringTableEntry = (const char*)(stringTableIndex + (ptrdiff_t)header + symtabCommand->stroff
                                                         + linkEditSegment->vmaddr - linkEditSegment->fileoff
                                                         - textSegment->vmaddr);
            if (strcmp(symbol, stringTableEntry)==0) {
                return ((void*)header) - textSegment->vmaddr + symbolTableEntry->n_value;
            }
        }
        state++;
    }
    return NULL;
}


