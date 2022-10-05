#include "tsf_ir.h"
#include "tsf_ir_different.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/time.h>
#ifdef TSF_BUILD_SYSTEM
#include "tsf.h"
#else
#include <tsf/tsf.h>
#endif

static void usage(void) {
    fprintf(stderr, "Usage: tsf_ir_speed [<count>]\n");
    exit(1);
}

static double milliTime(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000. + tv.tv_usec / 1000.;
}

#define TIMEIT(statement) do {                                          \
        double __ti_before = milliTime();                               \
        statement;                                                      \
        double __ti_after = milliTime();                                \
        printf("%s: %.3lf ms\n", #statement, __ti_after - __ti_before); \
    } while (0)

/* If the expression evaluates false then it assumes an error and exits. */
#define CT(exp) do {                                                    \
        if (!(exp)) {                                                   \
            fprintf(stderr, "%s:%d: %s: %s\n",                          \
                    __FILE__, __LINE__, #exp, tsf_get_error());         \
            exit(1);                                                    \
        }                                                               \
    } while (0)
#define CS(exp) do {                                                    \
        if (!(exp)) {                                                   \
            int myErrno = errno;                                        \
            fprintf(stderr, "%s:%d: %s: %s\n",                          \
                    __FILE__, __LINE__, #exp, strerror(myErrno));       \
            exit(1);                                                    \
        }                                                               \
    } while (0)

static void writeTest(const char *filename,
                      unsigned programSize, unsigned numPrograms,
                      tsf_zip_mode_t zipMode) {
    tsf_zip_wtr_attr_t zipAttributes;
    tsf_stream_file_output_t *out;
    unsigned programIndex;
    char buf[256];
    
    CT(tsf_zip_wtr_attr_get_for_mode(&zipAttributes, zipMode));
    CT(out = tsf_stream_file_output_open(filename, tsf_false, &zipAttributes));
    
    for (programIndex = 0; programIndex < numPrograms; ++programIndex) {
        const unsigned numDecls = 25;
        const unsigned numDefns = 25;
        
        Program_t *program;
        unsigned elementIndex;

        CS(program = tsf_region_create(sizeof(Program_t)));
        
        program->globals.len = numDecls + numDefns;
        CS(program->globals.data = tsf_region_alloc(
               program, sizeof(ProgramElement_t) * program->globals.len));
            
        for (elementIndex = 0; elementIndex < numDecls; ++elementIndex) {
            ProgramElement_t *element;
            ProcedureDecl_t *procedure;
                
            element = program->globals.data + elementIndex;
            element->value = ProgramElement__procedureDecl;
            procedure = &element->u.procedureDecl;
                
            snprintf(buf, sizeof(buf), "foo%u", elementIndex);
            procedure->name = tsf_region_strdup(program, buf);
        }
            
        for (elementIndex = numDecls;
             elementIndex < numDecls + numDefns;
             ++elementIndex) {
            ProgramElement_t *element;
            ProcedureDefn_t *procedure;
                
            unsigned numVariables = (programIndex % programSize) + 1;
            unsigned numInstructions = (programIndex % programSize) * 2 + 1;
            unsigned numDebugDatas = (numInstructions + 5) / 6;
            
            unsigned variableIndex;
            unsigned instructionIndex;
            unsigned debugDataIndex;
                
            element = program->globals.data + elementIndex;
            element->value = ProgramElement__procedureDefn;
            procedure = &element->u.procedureDefn;
                
            snprintf(buf, sizeof(buf), "bar%u", elementIndex);
            procedure->name = tsf_region_strdup(program, buf);
                
            procedure->variables.len = numVariables;
            CS(procedure->variables.data = tsf_region_alloc(
                   program, sizeof(VariableDecl_t) * numVariables));
                
            for (variableIndex = 0; variableIndex < numVariables; ++variableIndex) {
                VariableDecl_t *variable;
                    
                variable = procedure->variables.data + variableIndex;
                
                switch ((variableIndex + programIndex) % 3) {
                case 0:
                    variable->type = "foo";
                    break;
                case 1:
                    variable->type = "bar";
                    break;
                case 2:
                    variable->type = "baz";
                    break;
                default:
                    abort();
                    break;
                }
                
                snprintf(buf, sizeof(buf), "x%u", variableIndex);
                variable->name = tsf_region_strdup(program, buf);
            }
                
            procedure->code.len = numInstructions;
            CS(procedure->code.data = tsf_region_alloc(
                   program, sizeof(Instruction_t) * numInstructions));
                
            for (instructionIndex = 0;
                 instructionIndex < numInstructions;
                 ++instructionIndex) {
                Instruction_t *instruction;
                    
                instruction = procedure->code.data + instructionIndex;
                    
                switch ((instructionIndex + programIndex) % 8) {
                case Instruction__nop: {
                    instruction->value = Instruction__nop;
                    break;
                }
                        
                case Instruction__mov: {
                    instruction->value = Instruction__mov;
                    instruction->u.mov.dest =
                        (instructionIndex + programIndex) % numVariables;
                    instruction->u.mov.src =
                        (instructionIndex + programIndex + 1) % numVariables;
                    break;
                }
                        
                case Instruction__add: {
                    instruction->value = Instruction__add;
                    instruction->u.add.dest =
                        (instructionIndex + programIndex + 2) % numVariables;
                    instruction->u.add.src =
                        (instructionIndex + programIndex + 3) % numVariables;
                    break;
                }
                        
                case Instruction__alloc: {
                    instruction->value = Instruction__alloc;
                        
                    snprintf(buf, sizeof(buf), "t%u", instructionIndex + programIndex);
                    instruction->u.alloc.type = tsf_region_strdup(program, buf);
                    break;
                }
                        
                case Instruction__ret: {
                    instruction->value = Instruction__ret;
                    instruction->u.ret.src =
                        (instructionIndex + programIndex + 4) % numVariables;
                    break;
                }
                        
                case Instruction__jump: {
                    instruction->value = Instruction__jump;
                    instruction->u.jump.target =
                        (instructionIndex + programIndex) % numInstructions;
                    break;
                }
                        
                case Instruction__call: {
                    unsigned numArgs = (instructionIndex + programIndex) % 10;
                        
                    unsigned argIndex;
                        
                    instruction->value = Instruction__call;
                    instruction->u.call.dest =
                        (instructionIndex + programIndex + 7) % numVariables;
                    instruction->u.call.callee =
                        ((instructionIndex + programIndex) %
                         (numVariables + numDecls + numDefns)) -
                        numDecls - numDefns;
                        
                    instruction->u.call.args.len = numArgs;
                    CS(instruction->u.call.args.data = tsf_region_alloc(
                           program, sizeof(Operand_t) * numArgs));
                        
                    for (argIndex = 0; argIndex < numArgs; ++argIndex) {
                        instruction->u.call.args.data[argIndex] =
                            (instructionIndex + programIndex + argIndex) % numVariables;
                    }
                    break;
                }
                        
                case Instruction__branchZero: {
                    instruction->value = Instruction__branchZero;
                    instruction->u.branchZero.src =
                        (instructionIndex + programIndex + 5) % numVariables;
                    instruction->u.branchZero.target =
                        (instructionIndex + programIndex + 6) % numInstructions;
                    break;
                }
                        
                default:
                    abort();
                    break;
                }
            }
            
            procedure->debug.len = numDebugDatas;
            CS(procedure->debug.data = tsf_region_alloc(
                   program, sizeof(DebugData_t) * numDebugDatas));
            
            for (debugDataIndex = 0; debugDataIndex < numDebugDatas; ++debugDataIndex) {
                DebugData_t *debugData;
                    
                debugData = procedure->debug.data + debugDataIndex;
                debugData->startOffset = debugDataIndex;
                debugData->spanSize = 1;
                
                snprintf(buf, sizeof(buf), "debug%u", debugDataIndex);
                debugData->data = tsf_region_strdup(program, buf);
            }
        }
            
        Program__write(out, program);
        tsf_region_free(program);
    }
    
    CT(tsf_stream_file_output_close(out));
}

static void readTest(const char *filename, unsigned numPrograms) {
    tsf_stream_file_input_t *in;
    unsigned count;
    
    CT(in = tsf_stream_file_input_open(filename, NULL, NULL));
    
    count = 0;
    for (;;) {
        Program_t *program = Program__read(in);
        if (!program) {
            CT(tsf_get_error_code() == TSF_E_EOF);
            break;
        }
        
        count++;
        tsf_region_free(program);
    }
    
    if (count != numPrograms)
        abort();
    
    tsf_stream_file_input_close(in);
}

static void readMallocTest(const char *filename, unsigned numPrograms) {
    tsf_stream_file_input_t *in;
    unsigned count;
    
    CT(in = tsf_stream_file_input_open(filename, NULL, NULL));
    
    count = 0;
    for (;;) {
        Program_t program;
        if (!Program__read_into(in, &program)) {
            CT(tsf_get_error_code() == TSF_E_EOF);
            break;
        }
        
        count++;
        Program__destruct(&program);
    }
    
    if (count != numPrograms)
        abort();
    
    tsf_stream_file_input_close(in);
}

static void readConvertTest(const char *filename, unsigned numPrograms) {
    tsf_stream_file_input_t *in;
    unsigned count;
    
    CT(in = tsf_stream_file_input_open(filename, NULL, NULL));
    
    count = 0;
    for (;;) {
        DProgram_t *program = DProgram__read(in);
        if (!program) {
            CT(tsf_get_error_code() == TSF_E_EOF);
            break;
        }
        
        count++;
        tsf_region_free(program);
    }
    
    if (count != numPrograms)
        abort();
    
    tsf_stream_file_input_close(in);
}

int main(int c, char **v) {
    static const char *filename = "tsf_ir_speed_test_file.tsf";
    static const char *zipFilename = "tsf_ir_speed_test_zip_file.tsf";
    
    unsigned count;
    
    switch (c) {
    case 1:
        /* Use a small problem size suitable for regression testing. */
        count = 1000;
        break;
        
    case 2:
        if (sscanf(v[1], "%u", &count) != 1) {
            usage();
        }
        break;
        
    default:
        usage();
        return 1;
    }
    
    printf("Writing %u programs.\n", count);
    if (count != 10000)
        printf("WARNING: If you are benchmarking, please use count = 10000.\n");
    
    TIMEIT(writeTest(filename, 100, count, TSF_ZIP_NONE));
    TIMEIT(readTest(filename, count));
    TIMEIT(readMallocTest(filename, count));
    TIMEIT(readConvertTest(filename, count));
    
    if (tsf_zlib_supported()) {
        TIMEIT(writeTest(zipFilename, 100, count, TSF_ZIP_ZLIB));
        TIMEIT(readTest(zipFilename, count));
        TIMEIT(readMallocTest(filename, count));
        TIMEIT(readConvertTest(zipFilename, count));
    }
    
    /* We don't benchmark bzip2 because it's just too slow to be interesting. */
    
    return 0;
}

