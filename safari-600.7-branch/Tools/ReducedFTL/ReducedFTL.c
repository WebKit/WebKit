/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

/*
 * Simple tool that takes some LLVM bitcode as input and "JITs" it, in the
 * same way that the FTL would use LLVM to JIT the IR that it generates.
 * This is meant for use as a reduction when communicating to LLVMers
 * about bugs, and for quick "what-if" testing to see how our optimization
 * pipeline performs. Because of its use as a reduction, this tool is
 * intentionally standalone and it would be great if it continues to fit
 * in one file.
 */

#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include <llvm-c/Analysis.h>
#include <llvm-c/BitReader.h>
#include <llvm-c/Core.h>
#include <llvm-c/Disassembler.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include <llvm-c/Transforms/Scalar.h>

static void usage()
{
    printf("Usage: ReducedFTL <file1> [<file2> ...]\n");
    printf("\n");
    printf("Options:\n");
    printf("--verbose        Display more information, including module dumps.\n");
    printf("--timing         Measure the time it takes to compile.\n");
    printf("--disassemble    Disassemble all of the generated code at the end.\n");
    printf("--mode <mode>    Set the optimization mode (either \"simple\" or \"opt\").\n");
    printf("--contexts <arg> Set the number of contexts (either \"one\" or \"many\").\n");
    printf("--loop           Keep recompiling forever. Useful when attaching a profiler.\n");
    printf("--fast-isel      Enable the \"fast\" instruction selector.\n");
    printf("--help           Print this message.\n");
    printf("\n");
    printf("Unless you specify one of --verbose, --timing, or --disassemble, you will\n");
    printf("not see any output.\n");
    exit(1);
}

static double currentTime()
{
    struct timeval now;
    gettimeofday(&now, 0);
    return now.tv_sec + now.tv_usec / 1000000.0;
}

struct MemorySection {
    uint8_t *start;
    size_t size;
    struct MemorySection *next;
};

static struct MemorySection* sectionHead;

static size_t roundUpSize(size_t size)
{
    size_t pageSize = getpagesize();
    
    return (size + pageSize - 1) & ~pageSize;
}

static uint8_t *mmAllocateCodeSection(
    void *opaqueState, uintptr_t size, unsigned alignment, unsigned sectionID)
{
    uint8_t *start = mmap(
        0,  roundUpSize(size),
        PROT_WRITE | PROT_READ | PROT_EXEC,
        MAP_ANON | MAP_PRIVATE, -1, 0);
    if (start == (uint8_t*)-1) {
        fprintf(stderr, "Unable to allocate %" PRIuPTR " bytes of executable memory.\n", size);
        exit(1);
    }
    
    struct MemorySection *section = malloc(sizeof(struct MemorySection));
    section->start = start;
    section->size = size;
    section->next = sectionHead;
    sectionHead = section;

    return start;
}

static uint8_t *mmAllocateDataSection(
    void *opaqueState, uintptr_t size, unsigned alignment, unsigned sectionID,
    LLVMBool isReadOnly)
{
    return mmAllocateCodeSection(opaqueState, size, alignment, sectionID);
}

static LLVMBool mmApplyPermissions(void *opaque, char **message)
{
    return 0;
}

static void mmDestroy(void *opaque)
{
}

static const char *symbolLookupCallback(
    void *opaque, uint64_t referenceValue, uint64_t *referenceType, uint64_t referencePC,
    const char **referenceName)
{
    static char symbolString[20];
    
    switch (*referenceType) {
    case LLVMDisassembler_ReferenceType_InOut_None:
        return 0;
    case LLVMDisassembler_ReferenceType_In_Branch:
        *referenceName = 0;
        *referenceType = LLVMDisassembler_ReferenceType_InOut_None;
        snprintf(
            symbolString, sizeof(symbolString), "0x%lx",
            (unsigned long)referenceValue);
        return symbolString;
    default:
        fprintf(stderr, "Unexpected reference type!\n");
        exit(1);
        return 0;
    }
}

void webkit_osr_exit()
{
    abort();
}

int main(int c, char **v)
{
    LLVMContextRef *contexts;
    LLVMModuleRef *modules;
    char *error;
    const char *mode = "opt";
    const char **filenames;
    unsigned numFiles;
    unsigned i;
    bool moreOptions;
    static int verboseFlag = 0;
    static int timingFlag = 0;
    static int disassembleFlag = 0;
    bool manyContexts = true;
    bool loop = false;
    double beforeAll;
    bool fastIsel = false;
    int jitOptLevel = 2;
    struct MemorySection *section;
    
    if (c == 1)
        usage();
    
    moreOptions = true;
    while (moreOptions) {
        static struct option longOptions[] = {
            {"verbose", no_argument, &verboseFlag, 1},
            {"timing", no_argument, &timingFlag, 1},
            {"disassemble", no_argument, &disassembleFlag, 1},
            {"mode", required_argument, 0, 0},
            {"contexts", required_argument, 0, 0},
            {"loop", no_argument, 0, 0},
            {"fast-isel", no_argument, 0, 0},
            {"jit-opt", required_argument, 0, 0},
            {"help", no_argument, 0, 0}
        };
        
        int optionIndex;
        int optionValue;
        
        optionValue = getopt_long(c, v, "", longOptions, &optionIndex);
        
        switch (optionValue) {
        case -1:
            moreOptions = false;
            break;
            
        case 0: {
            const char* thisOption = longOptions[optionIndex].name;
            if (!strcmp(thisOption, "help"))
                usage();
            if (!strcmp(thisOption, "loop"))
                loop = true;
            if (!strcmp(thisOption, "fast-isel"))
                fastIsel = true;
            if (!strcmp(thisOption, "contexts")) {
                if (!strcasecmp(optarg, "one"))
                    manyContexts = false;
                else if (!strcasecmp(optarg, "many"))
                    manyContexts = true;
                else {
                    fprintf(stderr, "Invalid argument for --contexts.\n");
                    exit(1);
                }
                break;
            }
            if (!strcmp(thisOption, "jit-opt")) {
                if (sscanf(optarg, "%d", &jitOptLevel) != 1) {
                    fprintf(stderr, "Invalid argument for --jit-opt.\n");
                    exit(1);
                }
                break;
            }
            if (!strcmp(thisOption, "mode")) {
                mode = strdup(optarg);
                break;
            }
            break;
        }
            
        case '?':
            exit(0);
            break;
            
        default:
            printf("optionValue = %d\n", optionValue);
            abort();
            break;
        }
    }
    
    LLVMLinkInMCJIT();
    LLVMInitializeNativeTarget();
    LLVMInitializeX86AsmPrinter();
    LLVMInitializeX86Disassembler();

    filenames = (const char **)(v + optind);
    numFiles = c - optind;
    
    if (!numFiles)
        return 0;
    
    do {
        contexts = malloc(sizeof(LLVMContextRef) * numFiles);
        modules = malloc(sizeof(LLVMModuleRef) * numFiles);
    
        if (manyContexts) {
            for (i = 0; i < numFiles; ++i)
                contexts[i] = LLVMContextCreate();
        } else {
            LLVMContextRef context = LLVMContextCreate();
            for (i = 0; i < numFiles; ++i)
                contexts[i] = context;
        }
    
        for (i = 0; i < numFiles; ++i) {
            LLVMMemoryBufferRef buffer;
            const char* filename = filenames[i];
        
            if (LLVMCreateMemoryBufferWithContentsOfFile(filename, &buffer, &error)) {
                fprintf(stderr, "Error reading file %s: %s\n", filename, error);
                exit(1);
            }
        
            if (LLVMParseBitcodeInContext(contexts[i], buffer, modules + i, &error)) {
                fprintf(stderr, "Error parsing file %s: %s\n", filename, error);
                exit(1);
            }
        
            LLVMDisposeMemoryBuffer(buffer);
        
            if (verboseFlag) {
                printf("Module #%u (%s) after parsing:\n", i, filename);
                LLVMDumpModule(modules[i]);
            }
        }

        if (verboseFlag)
            printf("Generating code for modules...\n");
    
        if (timingFlag)
            beforeAll = currentTime();
        for (i = 0; i < numFiles; ++i) {
            LLVMModuleRef module;
            LLVMExecutionEngineRef engine;
            struct LLVMMCJITCompilerOptions options;
            LLVMValueRef value;
            LLVMPassManagerRef functionPasses = 0;
            LLVMPassManagerRef modulePasses = 0;
        
            double before;
        
            if (timingFlag)
                before = currentTime();
        
            module = modules[i];

            LLVMInitializeMCJITCompilerOptions(&options, sizeof(options));
            options.OptLevel = jitOptLevel;
            options.EnableFastISel = fastIsel;
            options.MCJMM = LLVMCreateSimpleMCJITMemoryManager(
                0, mmAllocateCodeSection, mmAllocateDataSection, mmApplyPermissions, mmDestroy);
    
            if (LLVMCreateMCJITCompilerForModule(&engine, module, &options, sizeof(options), &error)) {
                fprintf(stderr, "Error building MCJIT: %s\n", error);
                exit(1);
            }
    
            if (!strcasecmp(mode, "simple")) {
                modulePasses = LLVMCreatePassManager();
                LLVMAddTargetData(LLVMGetExecutionEngineTargetData(engine), modulePasses);
                LLVMAddPromoteMemoryToRegisterPass(modulePasses);
                LLVMAddConstantPropagationPass(modulePasses);
                LLVMAddInstructionCombiningPass(modulePasses);
                LLVMAddBasicAliasAnalysisPass(modulePasses);
                LLVMAddTypeBasedAliasAnalysisPass(modulePasses);
                LLVMAddGVNPass(modulePasses);
                LLVMAddCFGSimplificationPass(modulePasses);
                LLVMRunPassManager(modulePasses, module);
            } else if (!strcasecmp(mode, "opt")) {
                LLVMPassManagerBuilderRef passBuilder;

                passBuilder = LLVMPassManagerBuilderCreate();
                LLVMPassManagerBuilderSetOptLevel(passBuilder, 2);
                LLVMPassManagerBuilderSetSizeLevel(passBuilder, 0);
        
                functionPasses = LLVMCreateFunctionPassManagerForModule(module);
                modulePasses = LLVMCreatePassManager();
        
                LLVMAddTargetData(LLVMGetExecutionEngineTargetData(engine), modulePasses);
        
                LLVMPassManagerBuilderPopulateFunctionPassManager(passBuilder, functionPasses);
                LLVMPassManagerBuilderPopulateModulePassManager(passBuilder, modulePasses);
        
                LLVMPassManagerBuilderDispose(passBuilder);
        
                LLVMInitializeFunctionPassManager(functionPasses);
                for (value = LLVMGetFirstFunction(module); value; value = LLVMGetNextFunction(value))
                    LLVMRunFunctionPassManager(functionPasses, value);
                LLVMFinalizeFunctionPassManager(functionPasses);
        
                LLVMRunPassManager(modulePasses, module);
            } else {
                fprintf(stderr, "Bad optimization mode: %s.\n", mode);
                fprintf(stderr, "Valid modes are: \"simple\" or \"opt\".\n");
                exit(1);
            }

            if (verboseFlag) {
                printf("Module #%d (%s) after optimization:\n", i, filenames[i]);
                LLVMDumpModule(module);
            }
    
            for (value = LLVMGetFirstFunction(module); value; value = LLVMGetNextFunction(value)) {
                if (LLVMIsDeclaration(value))
                    continue;
                LLVMGetPointerToGlobal(engine, value);
            }

            if (functionPasses)
                LLVMDisposePassManager(functionPasses);
            if (modulePasses)
                LLVMDisposePassManager(modulePasses);
    
            LLVMDisposeExecutionEngine(engine);
        
            if (timingFlag) {
                double after = currentTime();
                printf("Module #%d (%s) took %lf ms.\n", i, filenames[i], (after - before) * 1000);
            }
        }

        if (manyContexts) {
            for (i = 0; i < numFiles; ++i)
                LLVMContextDispose(contexts[i]);
        } else
            LLVMContextDispose(contexts[0]);
        
        if (timingFlag) {
            double after = currentTime();
            printf("Compilation took a total of %lf ms.\n", (after - beforeAll) * 1000);
        }
    
        if (disassembleFlag) {
            LLVMDisasmContextRef disassembler;
        
            disassembler = LLVMCreateDisasm("x86_64-apple-darwin", 0, 0, 0, symbolLookupCallback);
            if (!disassembler) {
                fprintf(stderr, "Error building disassembler.\n");
                exit(1);
            }
    
            for (section = sectionHead; section; section = section->next) {
                printf("Disassembly for section %p:\n", section);
        
                char pcString[20];
                char instructionString[1000];
                uint8_t *pc;
                uint8_t *end;
        
                pc = section->start;
                end = pc + section->size;
        
                while (pc < end) {
                    snprintf(
                        pcString, sizeof(pcString), "0x%lx",
                        (unsigned long)(uintptr_t)pc);
            
                    size_t instructionSize = LLVMDisasmInstruction(
                        disassembler, pc, end - pc, (uintptr_t)pc,
                        instructionString, sizeof(instructionString));
            
                    if (!instructionSize)
                        snprintf(instructionString, sizeof(instructionString), ".byte 0x%02x", *pc++);
                    else
                        pc += instructionSize;
            
                    printf("    %16s: %s\n", pcString, instructionString);
                }
            }
        }
        
        for (section = sectionHead; section;) {
            struct MemorySection* nextSection = section->next;
            
            munmap(section->start, roundUpSize(section->size));
            free(section);
            
            section = nextSection;
        }
        sectionHead = 0;
        
    } while (loop);
    
    return 0;
}

