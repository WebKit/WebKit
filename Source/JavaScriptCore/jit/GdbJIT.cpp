/*
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
 * Copyright 2010 the V8 project authors. All rights reserved.
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
#include "config.h"
#include "GdbJIT.h"

#include <wtf/TZoneMallocInlines.h>

#if ENABLE(ASSEMBLER)
#if OS(DARWIN) || OS(LINUX)

#include "CallFrame.h"
#include "CallFrameInlines.h"
#include "Options.h"
#include "ProfilerSupport.h"
#include <array>
#include <fcntl.h>
#include <mutex>
#include <sys/stat.h>
#include <sys/types.h>
#include <wtf/DataLog.h>
#include <wtf/MonotonicTime.h>
#include <wtf/PageBlock.h>
#include <wtf/ProcessID.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/StringPrintStream.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

// Binary GDB JIT Interface as described in
//   http://sourceware.org/gdb/onlinedocs/gdb/Declarations.html
extern "C" {
enum JITAction {
    NoAction = 0,
    RegisterFunction = 1,
    UnregisterFunction = 2
};

struct JITCodeEntry {
    JITCodeEntry* next { };
    JITCodeEntry* prev { };
    uint8_t* symfileAddr { };
    uint64_t symfileSize { };
};

struct JITDescriptor {
    uint32_t version { };
    uint32_t actionFlag { };
    JITCodeEntry* relevantEntry { };
    JITCodeEntry* firstEntry { };
};

// GDB will place breakpoint into this function.
// To prevent GCC from inlining or removing it we place noinline attribute
// and inline assembler statement inside.
static REFERENCED_FROM_ASM NEVER_INLINE void __jit_debug_register_code()
{
    __asm__("");
}

// GDB will inspect contents of this descriptor.
// Static initialization is necessary to prevent GDB from seeing
// uninitialized descriptor.
static REFERENCED_FROM_ASM JITDescriptor __jit_debug_descriptor = { 1, 0, nullptr, nullptr };

} // extern "C"

namespace JSC {

namespace GdbJITInternal {
static constexpr bool verbose = false;
} // namespace GdbJITInternal

WTF_MAKE_TZONE_ALLOCATED_IMPL(GdbJIT);

GdbJIT& GdbJIT::singleton()
{
    static LazyNeverDestroyed<GdbJIT> logger;
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        logger.construct();
    });
    return logger.get();
}

#if OS(DARWIN)
class MachO;
class MachOSection;
using DebugObject = MachO;
using DebugSection = MachOSection;
#else
class ELF;
class ELFSection;
class ELFStringTable;
using DebugObject = ELF;
using DebugSection = ELFSection;
#endif

template<typename V>
static inline void writeUnalignedValue(uint8_t* p, V value)
{
    memcpy(p, &value, sizeof(V));
}

class Writer : public RefCountedAndCanMakeWeakPtr<Writer> {
public:
    static Ref<Writer> create(DebugObject* obj)
    {
        return adoptRef(*new Writer(obj));
    }

    uintptr_t position() const { return m_position; }

    template<typename T>
    class Slot {
    public:
        Slot(WeakPtr<Writer> writer, uintptr_t offset)
            : m_writer(writer)
            , m_offset(offset)
        {
        }

        T* operator->() { return m_writer->template rawSlotAt<T>(m_offset); }
        T operator*() { return *m_writer->template rawSlotAt<T>(m_offset); }

        void set(const T& value)
        {
            writeUnalignedValue(m_writer->template addressAt<T>(m_offset), value);
        }

        Slot<T> at(int i) { return Slot<T>(m_writer, m_offset + sizeof(T) * i); }

    private:
        WeakPtr<Writer> m_writer;
        uintptr_t m_offset;
    };

    template<typename T>
    void write(const T& val)
    {
        ensure(m_position + sizeof(T));
        writeUnalignedValue(addressAt<T>(m_position), val);
        m_position += sizeof(T);
    }

    template<typename T>
    Slot<T> slotAt(uintptr_t offset) LIFETIME_BOUND
    {
        ensure(offset + sizeof(T));
        return Slot<T>(*this, offset);
    }

    template<typename T>
    Slot<T> createSlotHere() LIFETIME_BOUND
    {
        return createSlotsHere<T>(1);
    }

    template<typename T>
    Slot<T> createSlotsHere(uint32_t count) LIFETIME_BOUND
    {
        uintptr_t slotPosition = m_position;
        m_position += sizeof(T) * count;
        ensure(m_position);
        return slotAt<T>(slotPosition);
    }

    void ensure(uintptr_t pos)
    {
        if (m_buffer.size() < pos)
            m_buffer.grow(pos);
    }

    DebugObject* debugObject() { return m_debugObject; }

    uint8_t* buffer() LIFETIME_BOUND { return &m_buffer[0]; }

    void align(uintptr_t align)
    {
        uintptr_t delta = m_position % align;
        if (!delta)
            return;
        uintptr_t padding = align - delta;
        ensure(m_position += padding);
        ASSERT(m_position % align == 0);
    }

    void writeULEB128(uintptr_t value)
    {
        do {
            uint8_t byte = value & 0x7F;
            value >>= 7;
            if (value)
                byte |= 0x80;
            write<uint8_t>(byte);
        } while (value);
    }

    void writeSLEB128(intptr_t value)
    {
        bool more = true;
        while (more) {
            int8_t byte = value & 0x7F;
            bool byteSign = byte & 0x40;
            value >>= 7;

            if ((!value && !byteSign) || (value == -1 && byteSign))
                more = false;
            else
                byte |= 0x80;

            write<int8_t>(byte);
        }
    }

    void writeString(const CString& str)
    {
        for (auto c : str.span())
            write<char>(c);
        write<char>('\0');
    }

private:
    template<typename T>
    friend class Slot;

    template<typename T>
    uint8_t* addressAt(uintptr_t offset) LIFETIME_BOUND
    {
        ASSERT(offset < m_buffer.size() && offset + sizeof(T) <= m_buffer.size());
        return &m_buffer[offset];
    }

    template<typename T>
    T* rawSlotAt(uintptr_t offset) LIFETIME_BOUND
    {
        ASSERT(offset < m_buffer.size() && offset + sizeof(T) <= m_buffer.size());
        return reinterpret_cast<T*>(&m_buffer[offset]);
    }

    Writer(DebugObject* debugObject)
        : m_debugObject(debugObject)
        , m_position(0)
    {
    }

    DebugObject* m_debugObject;
    uintptr_t m_position;
    Vector<uint8_t> m_buffer;
};

class CodeDescription : public RefCounted<CodeDescription> {
public:
    const CString& name() const LIFETIME_BOUND { return m_name; }

    const void* codeStart() const { return reinterpret_cast<const void*>(m_codeRegion.data()); }

    const void* codeEnd() const { return reinterpret_cast<const void*>(m_codeRegion.data() + m_codeRegion.size()); }

    uintptr_t codeSize() const { return m_codeRegion.size(); }

    std::span<const uint8_t> region() { return m_codeRegion; }

    static Ref<CodeDescription> create(const CString& name, std::span<const uint8_t> region)
    {
        return adoptRef(*new CodeDescription(name, region));
    }

private:
    CodeDescription(const CString& name, std::span<const uint8_t> region)
        : m_name(name)
        , m_codeRegion(region)
    {
    }

    CString m_name;
    std::span<const uint8_t> m_codeRegion;
};

static JITCodeEntry* createCodeEntry(uint8_t* symfileAddr, uintptr_t symfileSize)
{
    // It is easiest to just leak this.
    auto* entry = static_cast<JITCodeEntry*>(malloc(sizeof(JITCodeEntry) + symfileSize));

    entry->symfileAddr = reinterpret_cast<uint8_t*>(entry + 1);
    entry->symfileSize = symfileSize;
    memcpy(entry->symfileAddr, symfileAddr, symfileSize);

    entry->prev = entry->next = nullptr;

    return entry;
}

static void registerCodeEntry(JITCodeEntry* entry)
{
    entry->next = __jit_debug_descriptor.firstEntry;
    if (entry->next)
        entry->next->prev = entry;
    __jit_debug_descriptor.firstEntry = __jit_debug_descriptor.relevantEntry = entry;

    __jit_debug_descriptor.actionFlag = RegisterFunction;
    __jit_debug_register_code();
}

static void unregisterCodeEntry(JITCodeEntry* entry)
{
    if (entry->prev)
        entry->prev->next = entry->next;
    else
        __jit_debug_descriptor.firstEntry = entry->next;

    if (entry->next)
        entry->next->prev = entry->prev;

    __jit_debug_descriptor.relevantEntry = entry;
    __jit_debug_descriptor.actionFlag = UnregisterFunction;
    __jit_debug_register_code();
}

template <typename THeader>
class DebugSectionBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(DebugSectionBase);

public:
    virtual ~DebugSectionBase() = default;

    virtual void writeBody(Writer::Slot<THeader> header, Ref<Writer> writer)
    {
        uint64_t start = writer->position();
        if (writeBodyInternal(writer)) {
            header->offset = static_cast<uint32_t>(start);
            uint64_t end = writer->position();
            header->size = std::max(end - start, static_cast<uint64_t>(header->size));
        }
    }

    virtual bool writeBodyInternal(Ref<Writer>) { return false; }

    using Header = THeader;
};

struct MachOSectionHeader {
    char sectname[16];
    char segname[16];
    uint64_t addr;
    uint64_t size;
    uint32_t offset;
    uint32_t align;
    uint32_t reloff;
    uint32_t nreloc;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
} __attribute__((packed,aligned(1)));

class MachOSection : public DebugSectionBase<MachOSectionHeader> {
public:
    enum Type {
        Regular = 0x0u,
        AttrCoalesced = 0xBu,
        AttrSomeInstructions = 0x400u,
        AttrDebug = 0x02000000u,
        AttrPureInstructions = 0x80000000u
    };

    MachOSection(const CString& name, const CString& segment, uint32_t align, const void* addr, size_t size, uint32_t flags)
        : m_name(name)
        , m_segment(segment)
        , m_align(align)
        , m_addr(addr)
        , m_size(size)
        , m_flags(flags)
    {
        if (m_align) {
            ASSERT(WTF::isPowerOfTwo(align));
            m_align = WTF::fastLog2(align);
        }
    }

    ~MachOSection() override = default;

    virtual void populateHeader(Writer::Slot<Header> header)
    {
        header->addr = reinterpret_cast<uintptr_t>(m_addr);
        header->size = m_size;
        header->offset = 0;
        header->align = m_align;
        header->reloff = 0;
        header->nreloc = 0;
        header->flags = m_flags;
        header->reserved1 = 0;
        header->reserved2 = 0;
        header->reserved3 = 0;
        memset(header->sectname, 0, sizeof(header->sectname));
        memset(header->segname, 0, sizeof(header->segname));
        ASSERT(m_name.length() < sizeof(header->sectname));
        ASSERT(m_segment.length() < sizeof(header->segname));
        strncpy(header->sectname, m_name.data(), sizeof(header->sectname));
        strncpy(header->segname, m_segment.data(), sizeof(header->segname));
    }

    const void* addr() const { return m_addr; }
    size_t size() const { return m_size; }

private:
    CString m_name;
    CString m_segment;
    uint32_t m_align;
    const void* m_addr;
    size_t m_size;
    uint32_t m_flags;
};

struct ELFsectionHeader {
    uint32_t name;
    uint32_t type;
    uintptr_t flags;
    const void* address;
    uintptr_t offset;
    uintptr_t size;
    uint32_t link;
    uint32_t info;
    uintptr_t alignment;
    uintptr_t entrySize;
} __attribute__((packed,aligned(1)));

#if OS(LINUX)
class ELFSection : public DebugSectionBase<ELFsectionHeader> {
public:
    enum Type {
        TypeNull = 0,
        TypeProgBits = 1,
        TypeSymTab = 2,
        TypeStrTab = 3,
        TypeRela = 4,
        TypeHash = 5,
        TypeDynamic = 6,
        TypeNote = 7,
        TypeNoBits = 8,
        TypeRel = 9,
        TypeShLib = 10,
        TypeDynSym = 11,
        TypeLoProc = 0x70000000,
        TypeX86_64Unwind = 0x70000001,
        TypeHiProc = 0x7FFFFFFF,
        TypeLoUser = 0x80000000,
        TypeHiUser = 0xFFFFFFFF
    };

    enum Flags {
        FlagWrite = 1,
        FlagAlloc = 2,
        FlagExec = 4
    };

    enum SpecialIndexes {
        IndexAbsolute = 0xFFF1
    };

    ELFSection(const CString& name, Type type, uintptr_t align)
        : m_type(type)
        , m_name(name)
        , m_align(align)
    {
    }

    ~ELFSection() override = default;

    void populateHeader(Writer::Slot<Header>, ELFStringTable* strtab);

    void writeBody(Writer::Slot<Header> header, Ref<Writer> writer) override
    {
        uintptr_t start = writer->position();
        if (writeBodyInternal(writer)) {
            uintptr_t end = writer->position();
            header->offset = start;
            header->size = end - start;
        }
    }

    bool writeBodyInternal(Ref<Writer>) override { return false; }

    uint16_t index() const { return m_index; }
    void setIndex(uint16_t index) { m_index = index; }

    const Type m_type;

protected:
    virtual void populateHeader(Writer::Slot<Header> header)
    {
        header->flags = 0;
        header->address = 0;
        header->offset = 0;
        header->size = 0;
        header->link = 0;
        header->info = 0;
        header->entrySize = 0;
    }

private:
    CString m_name;
    uintptr_t m_align;
    uint16_t m_index;
};
#endif // OS(LINUX)

#if OS(DARWIN)
class MachOTextSection : public MachOSection {
public:
    MachOTextSection(uint32_t align, const void* codeAddr, uintptr_t codeSize)
        : MachOSection("__text", "__TEXT", align, codeAddr, codeSize, MachOSection::Regular | MachOSection::AttrSomeInstructions | MachOSection::AttrPureInstructions)
        , m_codeAddr(reinterpret_cast<const uint64_t*>(codeAddr))
        , m_codeSize(codeSize)
    {
    }

    bool writeBodyInternal(Ref<Writer> writer) override
    {
        for (auto* ptr = m_codeAddr; ptr < m_codeAddr + m_codeSize / sizeof(uint64_t); ++ptr)
            writer->write<uint64_t>(*ptr);
        return true;
    }

private:
    const uint64_t* m_codeAddr;
    const size_t m_codeSize;
};
#endif // OS(DARWIN)

#if OS(LINUX)
class FullHeaderELFSection : public ELFSection {
public:
    FullHeaderELFSection(const CString& name, Type type, uintptr_t align, const void* addr, uintptr_t offset, uintptr_t size, uintptr_t flags)
        : ELFSection(name, type, align)
        , m_addr(addr)
        , m_offset(offset)
        , m_size(size)
        , m_flags(flags)
    {
    }

protected:
    void populateHeader(Writer::Slot<Header> header) override
    {
        ELFSection::populateHeader(header);
        header->address = m_addr;
        header->offset = m_offset;
        header->size = m_size;
        header->flags = m_flags;
    }

private:
    const void* m_addr;
    uintptr_t m_offset;
    uintptr_t m_size;
    uintptr_t m_flags;
};

class ELFStringTable : public ELFSection {
public:
    explicit ELFStringTable(const CString& name)
        : ELFSection(name, TypeStrTab, 1)
        , m_writer(nullptr)
        , m_offset(0)
        , m_size(0)
    {
    }

    uintptr_t add(const CString& str)
    {
        if (!str.length())
            return 0;

        uintptr_t offset = m_size;
        writeString(str);
        return offset;
    }

    void attachWriter(Ref<Writer> w)
    {
        m_writer = w.ptr();
        m_offset = m_writer->position();

        // First entry in the string table should be an empty string.
        writeString("");
    }

    void detachWriter()
    {
        m_writer = nullptr;
    }

    void writeBody(Writer::Slot<Header> header, Ref<Writer>) override
    {
        ASSERT(!m_writer);
        header->offset = m_offset;
        header->size = m_size;
    }

private:
    void writeString(const CString& str)
    {
        for (auto c : str.span())
            m_writer->write(c);
        m_writer->write('\0');
        m_size += str.length() + 1;
    }

    RefPtr<Writer> m_writer;
    uintptr_t m_offset;
    uintptr_t m_size;
};

void ELFSection::populateHeader(Writer::Slot<ELFSection::Header> header, ELFStringTable* strtab)
{
    header->name = static_cast<uint32_t>(strtab->add(m_name));
    header->type = m_type;
    header->alignment = m_align;
    populateHeader(header);
}
#endif // OS(LINUX)

#if OS(DARWIN)
class MachO {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(MachO);

public:
    size_t addSection(std::unique_ptr<MachOSection> section)
    {
        m_sections.append(WTFMove(section));
        return m_sections.size() - 1;
    }

    void write(Ref<Writer> w, const CString& name, uintptr_t codeStart, uintptr_t)
    {
        Writer::Slot<MachOHeader> header = writeHeader(w);
        uintptr_t loadCommandStart = w->position();

        Vector<Writer::Slot<MachOSegmentCommand>> segmentCommands;
        Vector<Writer::Slot<MachOSection::Header>> sectionHeadersForSegments;

        for (auto& section : m_sections) {
            segmentCommands.append(writeSegmentCommand(w, reinterpret_cast<uintptr_t>(section->addr()), section->size()));
            sectionHeadersForSegments.append(w->createSlotHere<MachOSection::Header>());
        }

        auto symtabCmd = writeSymtabCommand(w, name);
        header->sizeOfCommands = static_cast<uint32_t>(w->position() - loadCommandStart);

        for (unsigned i = 0; i < m_sections.size(); ++i) {
            auto segmentCmd = segmentCommands[i];
            auto sectionHeader = sectionHeadersForSegments[i];
            segmentCmd->fileOff = w->position();
            m_sections[i]->populateHeader(sectionHeader);
            m_sections[i]->writeBody(sectionHeader, w);
            segmentCmd->fileSize = w->position() - segmentCmd->fileOff;
            segmentCmd->vmSize = sectionHeader->size;
        }

        writeNList(w, symtabCmd, codeStart);
        writeStringTable(w, name, symtabCmd);
    }

private:
    struct MachOHeader {
        uint32_t magic;
        uint32_t cpuType;
        uint32_t cpuSubtype;
        uint32_t fileType;
        uint32_t numCommands;
        uint32_t sizeOfCommands;
        uint32_t flags;
#if USE(JSVALUE64)
        uint32_t reserved;
#endif
    } __attribute__((packed,aligned(1)));

    struct MachOSegmentCommand {
        uint32_t cmd;
        uint32_t cmdSize;
        char segname[16];
        uint64_t vmAddr;
        uint64_t vmSize;
        uint64_t fileOff;
        uint64_t fileSize;
        uint32_t maxProt;
        uint32_t initProt;
        uint32_t numSects;
        uint32_t flags;
    } __attribute__((packed,aligned(1)));

    struct MachOSymtabCommand {
        uint32_t cmd;
        uint32_t cmdSize;
        uint32_t symFileOff;
        uint32_t numSyms;
        uint32_t strFileOff;
        uint32_t stringBytes;
    } __attribute__((packed,aligned(1)));

    enum MachOLoadCommandCmd {
        LCSegment32 = 0x00000001u,
        LCSymTab = 0x00000002u,
        LCSegment64 = 0x00000019u,
    };

    enum NListType {
        NAbs = 0x2,
        NSect = 0xe,
    };

    enum NListDescription {
        ReferenceFlagDefined = 0x2,
    };

    struct __attribute__((packed)) NList64 {
        uint32_t index;
        uint8_t type;
        uint8_t section;
        uint16_t desc;
        uint64_t value;
    };

    Writer::Slot<MachOHeader> writeHeader(Ref<Writer> writer)
    {
        ASSERT(!writer->position());
        Writer::Slot<MachOHeader> header = writer->createSlotHere<MachOHeader>();
#if CPU(ARM64)
        header->magic = 0xFEEDFACFu;
        header->cpuType = 0x0000000C | 0x01000000; // ARM | 64-bit ABI
        header->cpuSubtype = 0x00000000; // All
        header->reserved = 0;
#elif CPU(X86_64)
        header->magic = 0xFEEDFACFu;
        header->cpuType = 7 | 0x01000000; // i386 | 64-bit ABI
        header->cpuSubtype = 3;
        header->reserved = 0;
#else
#error Unsupported target architecture.
#endif
        header->fileType = 0x1; // MH_OBJECT
        header->numCommands = 3;
        header->sizeOfCommands = 0;
        header->flags = 0;
        return header;
    }

    Writer::Slot<MachOSegmentCommand> writeSegmentCommand(Ref<Writer> writer, uintptr_t codeStart, uintptr_t codeSize)
    {
        auto cmd = writer->createSlotHere<MachOSegmentCommand>();
        cmd->cmd = LCSegment64;
        cmd->vmAddr = codeStart;
        cmd->vmSize = codeSize;
        cmd->fileOff = 0;
        cmd->fileSize = 0;
        cmd->maxProt = 7;
        cmd->initProt = 7;
        cmd->flags = 0;
        cmd->numSects = 1;
        memset(cmd->segname, 0, 16);
        cmd->cmdSize = sizeof(MachOSegmentCommand) + sizeof(MachOSection::Header) * cmd->numSects;
        return cmd;
    }

    Writer::Slot<MachOSymtabCommand> writeSymtabCommand(Ref<Writer> writer, const CString& name)
    {
        auto cmd = writer->createSlotHere<MachOSymtabCommand>();
        cmd->cmd = LCSymTab;
        cmd->symFileOff = 0;
        cmd->numSyms = 1;
        cmd->strFileOff = 0;
        cmd->stringBytes = name.length() + 1;
        cmd->cmdSize = sizeof(MachOSymtabCommand);
        return cmd;
    }

    void writeNList(Ref<Writer> writer, Writer::Slot<MachOSymtabCommand> cmd, uintptr_t codeStart)
    {
        cmd->symFileOff = writer->position();
        auto slot = writer->createSlotHere<NList64>();
        slot->index = 1;
        slot->type = NSect;
        slot->section = 2;
        slot->desc = 0;
        slot->value = codeStart;
    }

    void writeStringTable(Ref<Writer> writer, const CString& name, Writer::Slot<MachOSymtabCommand> cmd)
    {
        cmd->strFileOff = writer->position();
        writer->write<char>('\0'); // Index 0
        for (auto c : name.span())
            writer->write<char>(c);
        writer->write<char>('\0');
    }

    Vector<std::unique_ptr<MachOSection>> m_sections { };
};
#endif // OS(DARWIN)

#if OS(LINUX)
class ELF {
public:
    explicit ELF()
    {
        m_sections.append(WTF::makeUnique<ELFSection>("", ELFSection::TypeNull, 0));
        m_sections.append(WTF::makeUnique<ELFStringTable>(".shstrtab"));
    }

    void write(Ref<Writer> writer)
    {
        writeHeader(writer);
        writeSectionTable(writer);
        writeSections(writer);
    }

    ELFSection* SectionAt(uint32_t index) { return m_sections[index].get(); }

    size_t addSection(std::unique_ptr<ELFSection> section)
    {
        section->setIndex(m_sections.size());
        m_sections.append(WTFMove(section));
        return m_sections.size() - 1;
    }

private:
    struct ELFHeader {
        uint8_t ident[16];
        uint16_t type;
        uint16_t machine;
        uint32_t version;
        uintptr_t entry;
        uintptr_t phtOffset;
        uintptr_t shtOffset;
        uint32_t flags;
        uint16_t headerSize;
        uint16_t phtEntrySize;
        uint16_t phtEntryNum;
        uint16_t shtEntrySize;
        uint16_t shtEntryNum;
        uint16_t shtStrTabIndex;
    } __attribute__((packed,aligned(1)));

    void writeHeader(Ref<Writer> writer)
    {
        ASSERT(!writer->position());
        Writer::Slot<ELFHeader> header = writer->createSlotHere<ELFHeader>();
#if CPU(ARM_THUMB2)
        const uint8_t ident[16] = {
            0x7F, 'E', 'L', 'F', 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0
        };
#elif CPU(X86_64) || CPU(ARM64)
        const uint8_t ident[16] = {
            0x7F, 'E', 'L', 'F', 2, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0
        };
#else
#error Unsupported target architecture.
#endif
        memcpy(header->ident, ident, 16);
        header->type = 1;
#if CPU(X86_64)
        // Processor identification value for x64 is 62 as defined in
        // System V ABI, AMD64 Supplement
        // http://www.x86-64.org/documentation/abi.pdf
        header->machine = 62;
#elif CPU(ARM_THUMB2)
        // Set to EM_ARM, defined as 40, in "ARM ELF File Format" at
        // infocenter.arm.com/help/topic/com.arm.doc.dui0101a/DUI0101A_Elf.pdf
        header->machine = 40;
#elif CPU(ARM64)
        // AARCH64
        header->machine = 0xB7;
#else
#error Unsupported target architecture.
#endif
        header->version = 1;
        header->entry = 0;
        header->phtOffset = 0;
        header->shtOffset = sizeof(ELFHeader); // Section table follows header.
        header->flags = 0;
        header->headerSize = sizeof(ELFHeader);
        header->phtEntrySize = 0;
        header->phtEntryNum = 0;
        header->shtEntrySize = sizeof(ELFSection::Header);
        header->shtEntryNum = m_sections.size();
        header->shtStrTabIndex = 1;
    }

    void writeSectionTable(Ref<Writer> writer)
    {
        // Section headers table immediately follows file header.
        ASSERT(writer->position() == sizeof(ELFHeader));

        Writer::Slot<ELFSection::Header> headers = writer->createSlotsHere<ELFSection::Header>(
            static_cast<uint32_t>(m_sections.size()));

        // String table for section table is the first section.
        ELFStringTable* strtab = static_cast<ELFStringTable*>(SectionAt(1));
        ASSERT(strtab->m_type == ELFSection::TypeStrTab);
        strtab->attachWriter(writer);
        uint32_t index = 0;
        for (auto& section : m_sections) {
            section->populateHeader(headers.at(index), strtab);
            index++;
        }
        strtab->detachWriter();
    }

    void writeSections(Ref<Writer> writer)
    {
        Writer::Slot<ELFSection::Header> headers = writer->slotAt<ELFSection::Header>(sizeof(ELFHeader));

        uint32_t index = 0;
        for (auto& section : m_sections) {
            section->writeBody(headers.at(index), writer);
            index++;
        }
    }

    Vector<std::unique_ptr<ELFSection>> m_sections;
};

class ELFSymbol {
public:
    enum Type {
        TypeNone = 0,
        TypeObject = 1,
        TypeFunction = 2,
        TypeSection = 3,
        TypeFile = 4,
        TypeLoProc = 13,
        TypeHiProc = 15
    };

    enum Binding {
        BindLocal = 0,
        BindGlobal = 1,
        BindWeak = 2,
        BindLoProc = 13,
        BindHiProc = 15
    };

    ELFSymbol(const CString& name, uintptr_t value, uintptr_t size, Binding binding, Type type, uint16_t section)
        : m_name(name)
        , m_value(value)
        , m_size(size)
        , m_info((binding << 4) | type)
        , m_other(0)
        , m_section(section)
    {
    }

    Binding binding() const { return static_cast<Binding>(m_info >> 4); }

#if CPU(ARM_THUMB2)
    struct SerializedLayout {
        SerializedLayout(uint32_t name, uintptr_t value, uintptr_t size, Binding binding, Type type, uint16_t section)
            : m_name(name)
            , m_value(value)
            , m_size(size)
            , m_info((binding << 4) | type)
            , m_other(0)
            , m_section(section)
        {
        }

        uint32_t m_name;
        uintptr_t m_value;
        uintptr_t m_size;
        uint8_t m_info;
        uint8_t m_other;
        uint16_t m_section;
    } __attribute__((packed,aligned(1)));
#elif CPU(X86_64) || CPU(ARM64)
    struct SerializedLayout {
        SerializedLayout(uint32_t name, uintptr_t value, uintptr_t size, Binding binding, Type type, uint16_t section)
            : m_name(name)
            , m_info((binding << 4) | type)
            , m_other(0)
            , m_section(section)
            , m_value(value)
            , m_size(size)
        {
        }

        uint32_t m_name;
        uint8_t m_info;
        uint8_t m_other;
        uint16_t m_section;
        uintptr_t m_value;
        uintptr_t m_size;
    } __attribute__((packed,aligned(1)));
#endif

    void write(Writer::Slot<SerializedLayout> slot, ELFStringTable* table) const
    {
        // Convert symbol names from strings to indexes in the string table.
        slot->m_name = static_cast<uint32_t>(table->add(m_name));
        slot->m_value = m_value;
        slot->m_size = m_size;
        slot->m_info = m_info;
        slot->m_other = m_other;
        slot->m_section = m_section;
    }

private:
    CString m_name;
    uintptr_t m_value;
    uintptr_t m_size;
    uint8_t m_info;
    uint8_t m_other;
    uint16_t m_section;
};

class ELFSymbolTable : public ELFSection {
public:
    ELFSymbolTable(const CString& name)
        : ELFSection(name, TypeSymTab, sizeof(uintptr_t))
    {
    }

    void writeBody(Writer::Slot<Header> header, Ref<Writer> writer) override
    {
        writer->align(header->alignment);
        size_t totalSymbols = m_locals.size() + m_globals.size() + 1;
        header->offset = writer->position();

        Writer::Slot<ELFSymbol::SerializedLayout> symbols = writer->createSlotsHere<ELFSymbol::SerializedLayout>(
            static_cast<uint32_t>(totalSymbols));

        header->size = writer->position() - header->offset;

        // String table for this symbol table should follow it in the section table.
        ELFStringTable* strtab = static_cast<ELFStringTable*>(writer->debugObject()->SectionAt(index() + 1));
        ASSERT(strtab->m_type == ELFSection::TypeStrTab);
        strtab->attachWriter(writer);
        symbols.at(0).set(ELFSymbol::SerializedLayout(0, 0, 0, ELFSymbol::BindLocal, ELFSymbol::TypeNone, 0));
        writeSymbolsList(&m_locals, symbols.at(1), strtab);
        writeSymbolsList(&m_globals, symbols.at(static_cast<uint32_t>(m_locals.size() + 1)), strtab);
        strtab->detachWriter();
    }

    void add(const ELFSymbol& symbol)
    {
        if (symbol.binding() == ELFSymbol::BindLocal)
            m_locals.append(symbol);
        else
            m_globals.append(symbol);
    }

protected:
    void populateHeader(Writer::Slot<Header> header) override
    {
        ELFSection::populateHeader(header);
        // We are assuming that string table will follow symbol table.
        header->link = index() + 1;
        header->info = static_cast<uint32_t>(m_locals.size() + 1);
        header->entrySize = sizeof(ELFSymbol::SerializedLayout);
    }

private:
    void writeSymbolsList(const Vector<ELFSymbol>* src,
        Writer::Slot<ELFSymbol::SerializedLayout> dst,
        ELFStringTable* strtab)
    {
        int i = 0;
        for (const ELFSymbol& symbol : *src)
            symbol.write(dst.at(i++), strtab);
    }

    Vector<ELFSymbol> m_locals;
    Vector<ELFSymbol> m_globals;
};

static void createSymbolsTable(Ref<CodeDescription> desc, ELF* elf, size_t textSectionIndex)
{
    auto symtab = WTF::makeUnique<ELFSymbolTable>(".symtab");
    auto strtab = WTF::makeUnique<ELFStringTable>(".strtab");

    symtab->add(ELFSymbol("JSC Code", 0, 0, ELFSymbol::BindLocal,
        ELFSymbol::TypeFile, ELFSection::IndexAbsolute));

    symtab->add(ELFSymbol(desc->name(), 0, desc->codeSize(),
        ELFSymbol::BindGlobal, ELFSymbol::TypeFunction, textSectionIndex));

    // Symbol table should be followed by the linked string table.
    elf->addSection(WTFMove(symtab));
    elf->addSection(WTFMove(strtab));
}
#endif // OS(LINUX)

class UnwindInfoSection : public DebugSection {
public:
    explicit UnwindInfoSection(Ref<CodeDescription>);
    bool writeBodyInternal(Ref<Writer>) override;

    int writeCIE(Ref<Writer>, uint32_t debugSectionStart);
    Writer::Slot<int32_t> writeFDE(Ref<Writer>);
    void writeFDEState(Ref<Writer>);
    void WriteLength(Ref<Writer>, Writer::Slot<uint32_t>* lengthSlot, int initialPosition);

private:
    const Ref<CodeDescription> m_desc;

    // DWARF3 Specification, Table 7.23
    enum CFIInstructions {
        AdvanceLoc = 0x40,
        Offset = 0x80,
        Restore = 0xC0,
        Nop = 0x00,
        SetLoc = 0x01,
        AdvanceLoc1 = 0x02,
        AdvanceLoc2 = 0x03,
        AdvanceLoc4 = 0x04,
        OffsetExtended = 0x05,
        RestoreExtended = 0x06,
        Undefined = 0x07,
        SameValue = 0x08,
        Register = 0x09,
        RememberState = 0x0A,
        RestoreState = 0x0B,
        DefCFA = 0x0C,
        DefCFARegister = 0x0D,
        DefCFAOffset = 0x0E,
        DefCFAExpression = 0x0F,
        Expression = 0x10,
        OffsetExtendedSF = 0x11,
        DefCFASF = 0x12,
        DefCFAOffsetSF = 0x13,
        ValOffset = 0x14,
        ValOffsetSF = 0x15,
        ValExpression = 0x16
    };

    // System V ABI, AMD64 Supplement, Version 0.99.5, Figure 3.36
    enum RegisterMapping {
    // Only the relevant ones have been added to reduce clutter.
#if CPU(X86_64)
        RegisterFP = 6,
        RegisterLR = 16,
#elif CPU(ARM64)
        RegisterFP = 29,
        RegisterLR = 30,
#else
        RegisterFP = 7,
        RegisterLR = 14,
#endif
    };

    enum CFIConstants : uint32_t {
        CIEID = UINT32_MAX,
        CIEVersion = 4,
        CodeAlignFactor = 1,
        DataAlignFactor = 1,
        ReturnAddressRegister = RegisterLR
    };
};

void UnwindInfoSection::WriteLength(Ref<Writer> writer, Writer::Slot<uint32_t>* lengthSlot, int initialPosition)
{
    uint32_t align = (writer->position() - initialPosition) % sizeof(void*);

    if (align) {
        for (uint32_t i = 0; i < (sizeof(void*) - align); i++)
            writer->write<uint8_t>(Nop);
    }

    ASSERT((writer->position() - initialPosition) % sizeof(void*) == 0);
    lengthSlot->set(static_cast<uint32_t>(writer->position() - initialPosition));
}

UnwindInfoSection::UnwindInfoSection(Ref<CodeDescription> desc)
#if OS(LINUX)
    : ELFSection(".debug_frame", TypeProgBits, 1)
#elif OS(DARWIN)
    : MachOSection("__debug_frame", "__TEXT", sizeof(uintptr_t), 0, 0, MachOSection::Regular)
#else
#error "Unsupported platform"
#endif
    , m_desc(desc)
{
}

int UnwindInfoSection::writeCIE(Ref<Writer> writer, uint32_t)
{
    auto ciePosition = static_cast<uint32_t>(writer->position());
    auto cieLengthSlot = writer->createSlotHere<uint32_t>();

    writer->write<uint32_t>(CIEID);
    writer->write<uint8_t>(CIEVersion);
    writer->write<uint8_t>(0); // Null augmentation string.
    writer->write<uint8_t>(sizeof(uintptr_t)); // Address size
    writer->write<uint8_t>(0); // Segment size
    writer->writeULEB128(CodeAlignFactor);
    writer->writeSLEB128(DataAlignFactor);
    writer->writeULEB128(ReturnAddressRegister);

    WriteLength(writer, &cieLengthSlot, ciePosition + sizeof(*cieLengthSlot));

    return ciePosition;
}

Writer::Slot<int32_t> UnwindInfoSection::writeFDE(Ref<Writer> writer)
{
    int fdePosition = static_cast<uint32_t>(writer->position());
    auto fdeLengthSlot = writer->createSlotHere<uint32_t>();
    auto ciePointerSlot = writer->createSlotHere<int32_t>();

    writer->write<uintptr_t>(reinterpret_cast<uintptr_t>(m_desc->codeStart()));
    writer->write<uintptr_t>(reinterpret_cast<uintptr_t>(m_desc->codeSize()));

    writeFDEState(writer);

    WriteLength(writer, &fdeLengthSlot, fdePosition + sizeof(*fdeLengthSlot));
    return ciePointerSlot;
}

// You can read an example unwind section from GCC:
// readelf --debug-dump=frames ./test
// Or:
// llvm-dwarfdump -a "/tmp/jit-8113659Thunk: CallTrampoline.o"
// Also, try adding `log enable lldb unwind` to your .lldbinit if you debug with lldb,
// or `set debug frame on` and `set debug jit on` for gdb.
// https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf
// https://github.com/ARM-software/abi-aa/blob/main/aadwarf64/aadwarf64.rst#dwarf-register-names
// https://dwarfstd.org/doc/DWARF5.pdf
/*
CFA means canonical frame address. These bytecodes define CFA in terms of other registers,
or other registers in terms of CFA.

In the CIE:
DW_CFA_def_cfa: r31 (sp) ofs 0

00000000004007a4 <main>:
4007a4: d100c3ff  sub sp, sp, #0x30 (decimal 48)

DW_CFA_def_cfa_offset: 48

4007a8: a9027bfd  stp fp, lr, [sp, #32]
4007ac: 910083fd  add fp, sp, #0x20

DW_CFA_def_cfa: r29 (fp) ofs 16
DW_CFA_offset: r30 (lr) at cfa-8
DW_CFA_offset: r29 (fp) at cfa-16

<snip>

DW_CFA_def_cfa: r31 (sp) ofs 48

400840: a9427bfd  ldp fp, lr, [sp, #32]
400844: 9100c3ff  add sp, sp, #0x30

DW_CFA_def_cfa_offset: 0
DW_CFA_restore: r30 (lr)
DW_CFA_restore: r29 (fp)
400848: d65f03c0  ret

The generated table:

0x4007a4: CFA=WSP
0x4007a8: CFA=WSP+48
0x4007b0: CFA=W29+16: W29=[CFA-16], W30=[CFA-8]
0x400840: CFA=WSP+48: W29=[CFA-16], W30=[CFA-8]
0x400848: CFA=WSP
*/

void UnwindInfoSection::writeFDEState(Ref<Writer> writer)
{
    // The first state, just after the control has been transferred to the the
    // function.
    // Since we just want simple unwinding to work, we will just use this blanket rule to
    // force the unwidner to check fp. It won't be accurate, but it should be good enough for
    // basic debugging.
    // Consider when we are after these instructions:
    // stp fp, lr
    // mov fp, sp
    // Then:
    // CFA = fp (current) + 8
    // fp (previous value) = *(CFA - 16); lr / Return Address (saved) = *(CFA - 8);
    // Start with a bogus rule. LLDB is off by one some times, and this seemed to fix it for some reason.
    writer->write<uint8_t>(DefCFASF);
    writer->writeULEB128(RegisterFP);
    writer->writeSLEB128(0);
    writer->write<uint8_t>(AdvanceLoc1);
    writer->write<uint8_t>(is32Bit() ? 2 : 4);
    writer->write<uint8_t>(DefCFASF);
    writer->writeULEB128(RegisterFP);
    writer->writeSLEB128(static_cast<int32_t>(sizeof(CallerFrameAndPC)));
    writer->write<uint8_t>(OffsetExtendedSF);
    writer->writeULEB128(RegisterLR);
    writer->writeSLEB128(-static_cast<int32_t>(sizeof(uintptr_t)));
    writer->write<uint8_t>(OffsetExtendedSF);
    writer->writeULEB128(RegisterFP);
    writer->writeSLEB128(-2 * static_cast<int32_t>(sizeof(uintptr_t)));
}

bool UnwindInfoSection::writeBodyInternal(Ref<Writer> writer)
{
    uint32_t debugSectionStart = writer->position();
    // This is a throw-away; The CIE must come first for LLDB to be happy, but
    // the offset can't be 0 according to the spec / gdb.
    writeCIE(writer, debugSectionStart);
    auto ciePosition = writeCIE(writer, debugSectionStart);
    auto ciePointer = writeFDE(writer);
    ciePointer.set(ciePosition - debugSectionStart);
    return true;
}

static JITCodeEntry* createELFObject(Ref<CodeDescription> desc)
{
    constexpr int codeAlignment = 4;
#if OS(DARWIN)
    MachO machO;
    auto writer = Writer::create(&machO);

    if constexpr (isARM64())
        machO.addSection(WTF::makeUnique<UnwindInfoSection>(desc));
    machO.addSection(WTF::makeUnique<MachOTextSection>(codeAlignment, desc->codeStart(), desc->codeSize()));

    machO.write(writer, desc->name(), reinterpret_cast<uintptr_t>(desc->codeStart()), desc->codeSize());
#else
    ELF elf;
    auto writer = Writer::create(&elf);

    size_t textSectionIndex = elf.addSection(WTF::makeUnique<FullHeaderELFSection>(
        ".text", ELFSection::TypeNoBits, codeAlignment, desc->codeStart(), 0,
        desc->codeSize(), ELFSection::FlagAlloc | ELFSection::FlagExec));

    createSymbolsTable(desc, &elf, textSectionIndex);
    if constexpr (isARM64() || is32Bit())
        elf.addSection(WTF::makeUnique<UnwindInfoSection>(desc));

    elf.write(writer);
#endif

    return createCodeEntry(writer->buffer(), writer->position());
}

static std::optional<std::pair<GdbJITCodeMap::iterator, GdbJITCodeMap::iterator>>
getOverlappingRegions(GdbJITCodeMap& map, const std::span<const uint8_t> region)
{
    ASSERT(region.data() < region.data() + region.size());

    if (map.empty())
        return std::nullopt;

    // Find the first overlapping entry.

    // If successful, points to the first element not less than `region`. The
    // returned iterator has the key in `first` and the value in `second`.
    auto it = map.lower_bound(region);
    auto startIt = it;

    if (it == map.end()) {
        startIt = map.begin();
        // Find the first overlapping entry.
        for (; startIt != map.end(); ++startIt) {
            if (startIt->first.data() + startIt->first.size() > region.data())
                break;
        }
    } else if (it != map.begin()) {
        for (--it; it != map.begin(); --it) {
            if (it->first.data() + it->first.size() <= region.data())
                break;
            startIt = it;
        }
        if (it == map.begin() && it->first.data() + it->first.size() > region.data())
            startIt = it;
    }

    if (startIt == map.end())
        return std::nullopt;

    // Find the first non-overlapping entry after `region`.

    const auto endIt = map.lower_bound({ region.data() + region.size(), 0 });

    // Return a range containing intersecting regions.

    if (std::distance(startIt, endIt) < 1)
        return std::nullopt; // No overlapping entries.

    return { { startIt, endIt } };
}

static void removeJITCodeEntries(GdbJITCodeMap& map, const std::span<const uint8_t> region)
{
    if (auto overlap = getOverlappingRegions(map, region)) {
        auto startIt = overlap->first;
        auto endIt = overlap->second;
        for (auto it = startIt; it != endIt; ++it)
            unregisterCodeEntry(it->second);

        map.erase(startIt, endIt);
    }
}

// Insert the entry into the map and register it with GDB.
static void addJITCodeEntry(GdbJITCodeMap& map, std::span<const uint8_t> region,
    JITCodeEntry* entry, bool shouldDump, const CString& nameHint)
{
    static int fileNum = 0;
    if (shouldDump) {
        StringPrintStream filename;
        if (auto* optionalDirectory = Options::jitDumpDirectory())
            filename.print(optionalDirectory);
        else
            filename.print("/tmp");
        filename.print("/jit-", getCurrentProcessID(), fileNum++, nameHint, ".o");
        auto fd = open(filename.toCString().data(), O_CREAT | O_TRUNC | O_RDWR, 0666);
        RELEASE_ASSERT(fd != -1);
        auto file = fdopen(fd, "wb");
        RELEASE_ASSERT(file);

        fwrite(entry->symfileAddr, entry->symfileSize, 1, file);
        fflush(file);
        dataLogLnIf(GdbJITInternal::verbose, "GDBInfo dumped: ", nameHint, " ", RawPointer(region.data()), "-", RawPointer(region.data() + region.size()), " ", region.size(), " ", filename.toCString().data());
    }

    auto result = map.emplace(region, entry);
    ASSERT_UNUSED(result, result.second); // Insertion happened.

    registerCodeEntry(entry);
}

void GdbJIT::log(const CString& name, MacroAssemblerCodeRef<LinkBufferPtrTag> code)
{
    if (!Options::useGdbJITInfo())
        return;
    GdbJIT& logger = singleton();
    size_t size = code.size();
    auto* executableAddress = code.code().untaggedPtr<const uint8_t*>();
    if (!size) {
        dataLogLnIf(GdbJITInternal::verbose, "0 size record ", name, " ", RawPointer(executableAddress));
        return;
    }

    Locker locker { logger.m_lock };

    auto region = unsafeMakeSpan(executableAddress, size);
    removeJITCodeEntries(logger.m_map, region);
    auto* entry = createELFObject(CodeDescription::create(name, region));
    bool shouldDump = false;
    addJITCodeEntry(logger.m_map, region, entry, shouldDump, name);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#else

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GdbJIT);

GdbJIT& GdbJIT::singleton()
{
    static LazyNeverDestroyed<GdbJIT> logger;
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        logger.construct();
    });
    return logger.get();
}

void GdbJIT::log(const CString&, MacroAssemblerCodeRef<LinkBufferPtrTag>) { }

} // namespace JSC

#endif // OS(DARWIN) || OS(LINUX)
#endif // ENABLE(ASSEMBLER)
