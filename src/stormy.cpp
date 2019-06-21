/*
    stormy.cpp version - 17 (2019-06-15)

    Copyright (C) 2019 Justas Dabrila (justasdabrila@gmail.com)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

extern "C" {
    #include "stdint.h"
    #include "stdlib.h"
    #include "stdio.h"
    #include "math.h"
}
#include <cstring> // NOTE(justas): std::size_t

#define TAU 6.28318530717958647692528
#define SQRT_2 1.414213562373095
#define SQRT_2_OVER_2 (1.414213562373095 * .5)
#define SQRT_3 1.73205080756887729352
#define DEG_TO_RAD (TAU/360.0)
#define RAD_TO_DEG (360.0/TAU)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float f32;
typedef double f64;

typedef bool b32;

#define intern static
#define extern_c extern "C"
#define baked static const 

#define assert_message(__EXPRESSION, __MESSAGE) \
    assert_function(__EXPRESSION, __MESSAGE, __FILE__, __LINE__, __FUNCTION__)

#define assert_no_message(__EXPRESSION) \
    assert_function(__EXPRESSION, 0, __FILE__, __LINE__, __FUNCTION__)

#define assert_macro_pick_overload(__ARG1, __ARG2, __ARG3, ...) __ARG3

#if defined(_WIN32)
    #define IS_WINDOWS 1
    #include "windows.h"

    #define force_export __declspec(dllexport)

    // NOTE(justas): this doesn't do anyhting?????
    #define BREAKPOINT DebugBreak();

#elif defined(__GNUC__)
    #define IS_LINUX 1

    extern "C" {
        #include <unistd.h> // NOTE(justas): getpagesize
        #include <sys/mman.h> // NOTE(justas): mmap
        #include <sys/stat.h>  // NOTE(justas): stat
        #include <fcntl.h> // NOTE(justas): open
        //#include "execinfo.h"
    }
    #include <cerrno>

    #define force_export __attribute__ ((visibility ("default")))
    #define BREAKPOINT asm("int3");

#endif

#define struct_packed(__defn) struct __attribute__((packed)) __defn
#define force_inline inline __attribute__((always_inline)) 

#define assert(...) assert_macro_pick_overload(__VA_ARGS__, assert_message, assert_no_message)(__VA_ARGS__)

#define KILOBYTES(num) num * 1024
#define MEGABYTES(num) KILOBYTES(num) * 1024
#define GIGABYTES(num) MEGABYTES(num) * 1024

#if !defined(ARRAY_SIZE)
    #define ARRAY_SIZE(__what) (sizeof(__what) / sizeof(*__what))
#endif

// TODO(justas): also "watermark" should be renamed to size because it doesn't actually hold the
// over-all largest size of the storage unit

#define For(__what) for(auto * it : __what) 
#define ForRange(__i, __start_inclusive, __end_exclusive) for(s64 __i = __start_inclusive; __i < __end_exclusive; __i++)

// NOTE(justas): provides iterators over a { T * values; s64 num_values } array structure
template<typename T>
struct WeakArrayWrapper {
    s64 size;
    T * storage;

    struct Iterator {
        WeakArrayWrapper * array;
        s64 index;

        Iterator operator++() {
            this->index++;
            return *this;
        }

        b32 operator!=(const Iterator & rhs) const {
            return !(this->index == rhs.index && this->array == rhs.array);
        }

        T * operator*() {
            return this->array->storage + this->index;
        }
    };

    Iterator begin() {
        Iterator iter;
        iter.array = this;
        iter.index = 0;
        return iter;
    }

    Iterator end() {
        Iterator iter;
        iter.array = this;
        iter.index = size;
        return iter;
    }
};

template<typename T>
intern force_inline 
WeakArrayWrapper<T> make_weak_array_wrapper(
        T * storage, 
        s64 num_elements
) {
    WeakArrayWrapper<T> ret;

    ret.size = num_elements;
    ret.storage = storage;

    return ret;
}

#define ForWeak( __array, __max) For(make_weak_array_wrapper(__array, __max))

struct String {
    s64 length;
    const char * str;
};

intern force_inline constexpr
s64 string_length(const char* string) {
    s64 ret = 0;

    while(*string != '\0') {
        ret++;
        string++;
    }

    return ret;
}

intern constexpr
String make_string(const char *cstring, s64 length = -1) {
    String ret = {};

    if(length == -1) {
        length = string_length(cstring);
    }

    ret.length = length;
    ret.str = cstring;

    return ret;
}

constexpr intern String empty_string = make_string("");

struct Memory_Allocation {
    void * data;
    s64 length;
};

intern Memory_Allocation plat_mem_allocate(s64 num_bytes);
intern void plat_mem_free(Memory_Allocation mem);


intern
void assert_function(b32 expression, String msg, const char * file, s64 line, const char * function) {
    if(expression) {
        return;
    }

    printf("Assertion failed!\n");
    printf("--> at: '%s:%lld'\n", file, line);
    printf("--> in: '%s'\n", function);

    if(msg.length > 0) {
        printf("  Message:\n");
        printf("    %.*s\n", msg.length, msg.str); 
    }

#if defined(IS_WINDOWS)

    auto fmt =
        "Assertion Failed!\n"
        "\n"
        "%.*s\n"
        "\n"
        "at: %s:%lld\n"
        "in: %s";

    auto msg_size = string_length(fmt) + string_length(file) + msg.length + string_length(function) + 32; // + 32 arbitrary pad
    auto mem = plat_mem_allocate(msg_size);

    snprintf((char*)mem.data, mem.length, fmt, 
        msg.length, msg.str, file, line, function
    );

    MessageBoxA(NULL, (const char*)mem.data, "Error", MB_DEFAULT_DESKTOP_ONLY);

    plat_mem_free(mem);
    
#endif

#if defined(DEVELOPER)
#if defined(IS_LINUX)

#if 0
    printf("Backtrace:\n");

    void * trace_buffer[256];
    auto num_entries = backtrace(trace_buffer, ARRAY_SIZE(trace_buffer));
    auto symbols = backtrace_symbols(trace_buffer, num_entries);

    ForRange(index, 0, num_entries) {
        printf("\t%s\n", symbols[index]);
    }
    free(symbols);
#endif

    BREAKPOINT;
#endif
    exit(1);
#endif
}

intern force_inline
void assert_function(b32 expression, const char* message, const char * file, s64 line, const char * function) {
    assert_function(expression, message ? make_string(message) : empty_string, file, line, function);
}

// NOTE(justas): jblow's defer
#define MACRO_CONCAT_INTERNAL(x,y) x##y
#define MACRO_CONCAT(x,y) MACRO_CONCAT_INTERNAL(x,y)
 
template<typename T>
struct DeferExitScope {
    T lambda;

    DeferExitScope(T l) : lambda(l) {}

    ~DeferExitScope() { lambda(); }
    DeferExitScope(const DeferExitScope &);

private:
    DeferExitScope& operator = (const DeferExitScope &);
};
 
class DeferExitScopeHelp {
public:
    template<typename T>
    DeferExitScope<T> operator+(T t){ return t;}
};
 
#define defer const auto & MACRO_CONCAT(defer__, __LINE__) = DeferExitScopeHelp() + [&]()

// NOTE(justas): underscore is required for clang
intern force_inline constexpr
String operator "" _S(const char * cstring, std::size_t length) {
    return make_string(cstring, length);
}

intern force_inline
b32 char_is_uppercase(char c) {
    return (u8)c >= 65 && 90 >= (u8)c;
}

intern force_inline
b32 char_is_lowercase(char c) {
    return (u8)c >= 97 && 122 >= (u8)c;
}

intern
char char_to_lowercase_unchecked(char c) {
    return (u8)c + 0x20;
}

intern force_inline
char char_to_uppercase_unchecked(char c) {
    return (u8)c - 0x20;
}

intern force_inline
char char_to_lowercase_force(char c) {
    if(char_is_uppercase(c)) {
        return char_to_lowercase_unchecked(c);
    }
    return c;
}

intern
b32 string_equals(String a, String b, b32 case_sensitive = true) {
    if(a.length != b.length) {
        return false;
    }
    if(a.length == 0) {
        return true;
    }

    for(s32 cursor_index = 0;
            cursor_index < a.length;
            cursor_index++) {
        auto a_char = a.str[cursor_index];
        auto b_char = b.str[cursor_index];

        if(!case_sensitive) {
            if(char_is_uppercase(a_char)) {
                a_char = char_to_lowercase_unchecked(a_char);
            }
            if(char_is_uppercase(b_char)) {
                b_char = char_to_lowercase_unchecked(b_char);
            }
        }

        if(a_char != b_char) {
            return false;
        }
    }

    return true;
}

intern force_inline
b32 string_equals_case_sensitive(String a, String b) {
    return string_equals(a, b, true);
}


intern force_inline
b32 string_equals_case_insensitive(String a, String b) {
    return string_equals(a, b, false);
}

intern
b32 is_char_equals_case_generic(
        char a, 
        char b, 
        b32 case_sensitive
) {
    if(case_sensitive) {
        return a == b;
    }

    if(char_is_uppercase(a)) {
        a = char_to_lowercase_unchecked(a);
    }
    if(char_is_uppercase(b)) {
        b = char_to_lowercase_unchecked(b);
    }

    return a == b;
}

intern
s64 string_index_of_base(
        String target, 
        String contains, 
        s64 start_index,
        s64 max,
        s64 direction,
        b32 case_sensitive
) {
    if(contains.length > target.length) {
        return -1;
    }

    if(contains.length == 0 || target.length == 0) {
        return -1;
    }


    for(auto target_start_cursor = start_index;
            target_start_cursor != max;
            target_start_cursor += direction
       ) {
        s32 compare_cursor = 0;

        while(true) {
            // NOTE(justas): we've walked the whole of 'contains', thus it exists in 'target'
            if(compare_cursor >= contains.length) {
                return target_start_cursor;
            }

            // NOTE(justas): we've walked the whole of 'target', thus contains doesn't exist in 'target'
            if(compare_cursor >= target.length) {
                break;
            }

            char target_char    = *(target.str    + compare_cursor + target_start_cursor);
            char contains_char  = *(contains.str  + compare_cursor);

            if(!is_char_equals_case_generic(target_char, contains_char, case_sensitive)) {
                break;
            }
            
            ++compare_cursor;
        }
    }

    return -1;
}

intern force_inline
s64 string_index_of(
        String target, 
        String contains, 
        b32 case_sensitive = true
) {
    return string_index_of_base(target, contains, 0, target.length, 1, case_sensitive);
}

intern force_inline
s64 string_last_index_of(
        String target, 
        String contains, 
        b32 case_sensitive = true
) {
    return string_index_of_base(target, contains, target.length - 1, 0, -1, case_sensitive);
}

intern
b32 string_contains_case_generic(
        String target,
        String contains,
        b32 case_sensitive
) {
    if(contains.length == 0 || target.length == 0) {
        return false;
    }

    return string_index_of(target, contains, case_sensitive) >= 0;
}

intern
b32 string_contains_case_generic(
        String target,
        char contains,
        b32 case_sensitive
) {
    ForRange(index, 0 , target.length) {
        auto c = target.str[index];
        if(is_char_equals_case_generic(c, contains, case_sensitive)) {
            return true;
        }
    }
    return false;
}

template<typename T>
intern force_inline
b32 string_contains_case_sensitive(
        String target, 
        T contains
) {
    return string_contains_case_generic(target, contains, true);
}

template<typename T>
intern force_inline
b32 string_contains_case_insensitive(
        String target, 
        T contains
) {
    return string_contains_case_generic(target, contains, false);
}

template<typename T>
intern force_inline
b32 string_contains_any_of_case_generic(
        String target, 
        const T * strings, 
        s64 num_strings,
        b32 case_sensitive
) {
    ForRange(index, 0, num_strings) {
        if(string_contains_case_generic(target, strings[index], case_sensitive)) {
            return true;
        }
    }

    return false;
}

intern
u8* copy_bytes(u8 *destination, const u8 *source, s64 num_bytes) {
    return (u8*)memcpy(destination, source, num_bytes);
}

intern
void set_bytes(u8 *destination, u8 pattern, s64 num_bytes) {
    memset((void*)destination, (int)pattern, (size_t)num_bytes);
}

struct Allocate_String_Result {
    Memory_Allocation mem;
    String string;
};

intern force_inline
b32 allocation_equals(const Memory_Allocation * a, const Memory_Allocation * b) {
    return a->data == b->data && a->length == b->length;
}

#if defined(IS_WINDOWS)

    baked String PLAT_SEPARATOR = "\\";

    intern
    Memory_Allocation plat_mem_allocate(s64 num_bytes) {
        assert(num_bytes > 0, "plat_mem_allocate allocation was less than or equal to 0 bytes");
        Memory_Allocation ret;

        assert(num_bytes >= 0);
        if(num_bytes == 0) {
            ret.length = num_bytes;
            ret.data = 0;
            return ret;
        }

        auto ptr = VirtualAlloc(0, num_bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        ret.data = ptr;
        ret.length = num_bytes;

        return ret;
    }

    intern 
    void plat_mem_free(Memory_Allocation mem) {
        b32 status = VirtualFree(mem.data, 0, MEM_RELEASE);
        assert(status);
    }

#elif defined(IS_LINUX)
    baked String PLAT_SEPARATOR = "/"_S;

    template<typename T>
    intern force_inline
    T atomic_compare_and_swap(
            T * data, 
            T old_value, 
            T new_value 
    ) {
        return __sync_val_compare_and_swap(data, old_value, new_value);
    }

    template<typename T>
    intern force_inline
    b32 atomic_compare_and_swap_bool(
            T * data, 
            T old_value, 
            T new_value 
    ) {
        return __sync_bool_compare_and_swap(data, old_value, new_value);
    }

    template<typename T>
    intern force_inline
    T atomic_fetch(T * data) {
        return __atomic_load_n(data, __ATOMIC_SEQ_CST);
    }

    template<typename T>
    intern force_inline
    void atomic_store(T * data, T value) {
        __atomic_store_n(data, value, __ATOMIC_SEQ_CST);
    }

    intern force_inline
    void memory_barrier() {
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
    }

    template<typename T>
    intern force_inline
    T atomic_swap(T * data, T new_value) {
        return __atomic_exchange_n(data, new_value, __ATOMIC_SEQ_CST);
    }
    
    intern
    Memory_Allocation plat_mem_allocate(s64 num_bytes) {
        assert(num_bytes > 0, "plat_mem_allocate allocation was less than or equal to 0 bytes");

        {
            static auto page_size = getpagesize();

            if(page_size > num_bytes) {
                num_bytes = page_size;
            }
        }

        void * data = mmap(0, (size_t)num_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0 ,0);

        if(data == MAP_FAILED) {
            printf("plat_mem_allocate: failed to mmap %d bytes (errno is: %d).", num_bytes, errno);
            assert(false);
        }

        Memory_Allocation mem;

        mem.data = data;
        mem.length = num_bytes;

        return mem;
    }

    intern 
    void plat_mem_free(Memory_Allocation mem) {
        s32 status = munmap(mem.data, (size_t)mem.length);

        if(status == -1) {
            printf("plat_mem_free: failed to munmap %d bytes for address %p. Errno is %d\n", mem.length, mem.data, errno);
            assert(false);
        }
    }

#endif


baked Memory_Allocation null_page = {0,0};

struct Memory_Allocator;
intern Memory_Allocation memory_allocator_allocate(Memory_Allocator * generic_allocator, s64 num_bytes, const char * reason);

intern void memory_allocator_free(Memory_Allocator * generic_allocator, Memory_Allocation allocation);

intern Memory_Allocation memory_allocator_reallocate(Memory_Allocator * void_allocator, Memory_Allocation allocation, s64 new_size, const char* reason);

struct Read_File_Result {
    Memory_Allocation mem;
    s64 size = 0;
    b32 did_succeed = false;

    const char *log;
    
    String as_string;
};

intern
const char * copy_and_null_terminate_string(String str, 
                                            Memory_Allocator * allocator, 
                                            const char * reason,
                                            Memory_Allocation * out_alloc = 0
) {
    auto mem = memory_allocator_allocate(allocator, str.length + 1, reason);
    if(out_alloc) {
        *out_alloc = mem;
    }

    auto bytes = (u8*)mem.data;
    copy_bytes(bytes, (u8*)str.str, str.length);

    bytes[str.length] = '\0';

    return (const char*)mem.data;
}



#if defined(IS_WINDOWS) 

    template<typename T>
    intern force_inline
    T atomic_compare_and_swap(
            T * data, 
            T old_value, 
            T new_value 
    ) {
        return __sync_val_compare_and_swap(data, old_value, new_value);
    }

    template<typename T>
    intern force_inline
    b32 atomic_compare_and_swap_bool(
            T * data, 
            T old_value, 
            T new_value 
    ) {
        return __sync_bool_compare_and_swap(data, old_value, new_value);
    }

    template<typename T>
    intern force_inline
    T atomic_fetch(T * data) {
        return __atomic_load_n(data, __ATOMIC_SEQ_CST);
    }

    template<typename T>
    intern force_inline
    void atomic_store(T * data, T value) {
        __atomic_store_n(data, value, __ATOMIC_SEQ_CST);
    }

    intern force_inline
    void memory_barrier() {
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
    }

    template<typename T>
    intern force_inline
    T atomic_swap(T * data, T new_value) {
        return __atomic_exchange_n(data, new_value, __ATOMIC_SEQ_CST);
    }


    intern
    void plat_sleep(f64 seconds) {
        Sleep((DWORD)(seconds * 1000));
    }
 
    intern
    b32 plat_fs_write_entire_file(const char * dir, const void * buf, s64 num_bytes) {
        // TODO(justas): MAX_PATH chars max...
        auto handle = CreateFileA(dir, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        if(handle == INVALID_HANDLE_VALUE) {
            return false;
        }
        defer { CloseHandle(handle); };

        auto status = WriteFile(handle, buf, num_bytes, 0, 0);
        if(!status) {
            return false;
        }

        return true;
    }

    intern
    Read_File_Result plat_fs_read_entire_file(const char * dir, Memory_Allocator * persistent_allocator) {
        Read_File_Result ret;
        ret.did_succeed = false;
        
        // TODO(justas): MAX_PATH chars max...
        auto handle = CreateFileA(dir, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        if(handle == INVALID_HANDLE_VALUE) {
            ret.log = "failed to open file";
            return ret;
        }
        defer { CloseHandle(handle); };

        LARGE_INTEGER size_win;
        auto success =GetFileSizeEx(handle, &size_win);
        if(!success) {
            ret.log = "failed to query file size";
            return ret;
        }

        s64 size = size_win.QuadPart;
        if(size == 0) {
            ret.size = 0;
            ret.mem = null_page;
            ret.did_succeed = true;
            ret.as_string = empty_string;
            return ret;
        }
        auto mem = memory_allocator_allocate(persistent_allocator, size, "plat_fs_read_entire_file buffer");

        success = ReadFile(handle, mem.data, size, 0, 0);
        if(!success) {
            ret.log = "failed to read file";
            return ret;
        }

        ret.size = size;
        ret.mem = mem;
        ret.did_succeed = true;
        ret.as_string = make_string((char*)mem.data, size);

        return ret;
    }

    intern force_inline
    u64 plat_get_file_modification_time(const char * dir) {
        // TODO(justas): MAX_PATH chars max...
        auto handle = CreateFileA(dir, GENERIC_READ, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        if(handle == INVALID_HANDLE_VALUE) {
            return 0;
        }
        defer { CloseHandle(handle); };

        FILETIME time;
        auto success = GetFileTime(handle, 0, 0, &time);
        if(!success) {
            return 0;
        }

        return ((time.dwHighDateTime << 16) << 16) | time.dwLowDateTime;
    }

#elif defined(IS_LINUX) 
    intern
    void plat_sleep(f64 seconds) {
        // NOTE(justas): microseconds
        usleep((useconds_t)(seconds * 1000000));
    }

    intern
    b32 plat_fs_write_entire_file(const char * dir, const void * buf, s64 num_bytes) {

        s32 handle = open(dir, O_WRONLY | O_TRUNC | O_CREAT, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

        if(handle == -1) {
            return false;
        }

        write(handle, buf, num_bytes);
        close(handle);

        return true;
    }

    intern
    Read_File_Result plat_fs_read_entire_file(const char * dir, Memory_Allocator * persistent_allocator) {
        Read_File_Result ret = {};

        FILE * f = fopen(dir, "r");

        if(!f) {
            ret.log = "fopen failed";
            ret.did_succeed = false;

            return ret;
        }

        fseek(f, 0, SEEK_END);
        s64 size= ftell(f);
        fseek(f, 0, SEEK_SET);

        if(size == 0) {
            ret.did_succeed = true;
            ret.size = 0;
            ret.mem = null_page;
            ret.as_string = empty_string;

            return ret;
        }

        auto mem = memory_allocator_allocate(persistent_allocator, size, "plat_fs_read_entire_file read_buffer");

        fread(mem.data, 1, size, f);
        fclose(f);

        ret.did_succeed = true;
        ret.size = size;
        ret.mem = mem;
        ret.as_string = make_string((char*)mem.data, size);

        return ret;
    }

    intern force_inline
    u64 plat_get_file_modification_time(const char * dir) {
        struct stat attribs;
        stat(dir, &attribs);
        return attribs.st_mtim.tv_sec;
    }

#endif

struct AtomicSpinlock {
    s32 * _lock;

    AtomicSpinlock(s32 * lock) : _lock(lock) {
        while(true) {
            if(atomic_compare_and_swap_bool(lock, 0, 1)) {
                break;
            }
        }
    }

    ~AtomicSpinlock() {
        atomic_store(_lock, 0);
    }
};


intern force_inline
u64 plat_get_file_modification_time(String file, Memory_Allocator * temp_allocator) {
    auto cstring = copy_and_null_terminate_string(file, temp_allocator, "get file mod time cstring");
    return plat_get_file_modification_time(cstring);
}

intern
void string_to_lowercase(String text) {
    for(s64 char_index = 0;
            char_index < text.length;
            char_index++) {

        u8 c = (u8)text.str[char_index];
        if(char_is_uppercase(c)) {
            *(char*)(text.str + char_index) = char_to_lowercase_unchecked((char)c);
        }
    }
}

intern
void string_to_uppercase(String text) {
    for(s64 char_index = 0;
            char_index < text.length;
            char_index++) {

        u8 c = (u8)text.str[char_index];
        if(char_is_lowercase(c)) {
            *(char*)(text.str + char_index) = char_to_uppercase_unchecked((char)c);
        }
    }
}

intern force_inline
Allocate_String_Result make_string(Memory_Allocator * allocator, s64 length, const char * reason) {
    Allocate_String_Result ret;

    auto mem = memory_allocator_allocate(allocator, length, reason);

    ret.mem = mem;
    ret.string.str = (char*)mem.data;
    ret.string.length = length;

    return ret;
}

intern force_inline
Allocate_String_Result make_string_copy(String source, Memory_Allocator * allocator) {

    auto result = make_string(allocator, source.length, "make_string_copy");
    copy_bytes((u8*)result.string.str, (u8*)source.str, source.length);

    return result;
}

intern
Allocate_String_Result make_string_copy(const char *cstring, Memory_Allocator * allocator, s64 length = -1) {

    if(length == -1) {
        length = string_length(cstring);
    }

    assert(length > 0, "make_string_copy length was under or equal to zero!");

    String text;
    text.str = cstring;
    text.length = length;

    return make_string_copy(text, allocator);
}

intern force_inline
String make_string_copy_temporary(String source, Memory_Allocator * temp_allocator) {
    return make_string_copy(source, temp_allocator).string;
}

intern force_inline
String make_string_copy_temporary(const char * cstring, Memory_Allocator * temp_allocator) {
    return make_string_copy(cstring, temp_allocator).string;
}

struct v2_u16 {
    u16 x;
    u16 y;
};

intern force_inline
v2_u16 make_vector_u16(u16 x, u16 y) {
    v2_u16 ret;

    ret.x = x;
    ret.y = y;

    return ret;
}

struct v2_s64 {
    union {
        struct {
            s64 x;
            s64 y;
        };

        s64 nth[2];
    };
};

intern force_inline
v2_s64 make_vector_s64(s64 x, s64 y) {
    v2_s64 ret;
    ret.x = x;
    ret.y = y;
    return ret;
}


struct v2_s32 {
    union {
        struct {
            s32 x;
            s32 y;
        };

        s32 nth[2];
    };

    v2_s32 & operator -=(const v2_s32 & rhs) {
        this->x -= rhs.x;
        this->y -= rhs.y;
        return *this;
    }

    v2_s32 & operator +=(const v2_s32 & rhs) {
        this->x += rhs.x;
        this->y += rhs.y;
        return *this;
    }
};

struct v2_u32 {
    union {
        struct {
            u32 x;
            u32 y;
        };

        u32 nth[2];
    };

    v2_u32 & operator -=(const v2_u32 & rhs) {
        this->x -= rhs.x;
        this->y -= rhs.y;
        return *this;
    }

    v2_u32 & operator +=(const v2_u32 & rhs) {
        this->x += rhs.x;
        this->y += rhs.y;
        return *this;
    }

};

intern force_inline
v2_u32 make_vector_u32(u32 x, u32 y) {
    v2_u32 ret;

    ret.x = x;
    ret.y = y;

    return ret;
}

b32 operator ==(const v2_u32 &lhs, const v2_u32 &rhs) {
    return lhs.x == rhs.x 
        && lhs.y == rhs.y;
}

v2_u32 operator -(const v2_u32 &lhs, const v2_u32 &rhs) {
    v2_u32 ret;

    ret.x = lhs.x - rhs.x;
    ret.y = lhs.y - rhs.y;

    return ret;
}

v2_u32 operator +(const v2_u32 &lhs, const v2_u32 &rhs) {
    v2_u32 ret;

    ret.x = lhs.x + rhs.x;
    ret.y = lhs.y + rhs.y;

    return ret;
}

b32 operator ==(const v2_s32 &lhs, const v2_s32 &rhs) {
    return lhs.x == rhs.x 
        && lhs.y == rhs.y;
}

v2_s32 operator -(const v2_s32 &lhs, const v2_s32 &rhs) {
    v2_s32 ret;

    ret.x = lhs.x - rhs.x;
    ret.y = lhs.y - rhs.y;

    return ret;
}

v2_s32 operator +(const v2_s32 &lhs, const v2_s32 &rhs) {
    v2_s32 ret;

    ret.x = lhs.x + rhs.x;
    ret.y = lhs.y + rhs.y;

    return ret;
}

struct v2_f64 {
    f64 x;
    f64 y;

    v2_f64 & operator *=(const v2_f64 & rhs) {
        this->x *= rhs.x;
        this->y *= rhs.y;
        return *this;
    }

    v2_f64 & operator *=(const f64 & rhs) {
        this->x *= rhs;
        this->y *= rhs;
        return *this;
    }

    v2_f64 & operator +=(const v2_f64 & rhs) {
        this->x += rhs.x;
        this->y += rhs.y;
        return *this;
    }

    v2_f64 & operator -=(const v2_f64 & rhs) {
        this->x -= rhs.x;
        this->y -= rhs.y;
        return *this;
    }

    v2_f64 & operator -=(const s32 & rhs) {
        this->x -= (f64)rhs;
        this->y -= (f64)rhs;
        return *this;
    }

    v2_f64 & operator /=(const v2_f64 & rhs) {
        this->x /= rhs.x;
        this->y /= rhs.y;
        return *this;
    }

    v2_f64 & operator /=(const s32 & rhs) {
        this->x /= (f64)rhs;
        this->y /= (f64)rhs;
        return *this;
    }


    v2_f64 operator-() const {
        v2_f64 ret;

        ret.x = -this->x;
        ret.y = -this->y;

        return ret;
    }
};

intern force_inline
v2_f64 make_vector_f64(v2_s64 v) {
    v2_f64 ret;
    ret.x = v.x;
    ret.y = v.y;
    return ret;
}

intern
v2_f64 operator*(const v2_f64 &lhs, const f64 &rhs) {
    v2_f64 ret;

    ret.x = lhs.x * rhs;
    ret.y = lhs.y * rhs;

    return ret;
}

intern
v2_f64 operator*(const v2_f64 &lhs, const v2_f64 &rhs) {
    v2_f64 ret;

    ret.x = lhs.x * rhs.x;
    ret.y = lhs.y * rhs.y;

    return ret;
}

intern
v2_f64 operator/(const v2_f64 &lhs, const v2_f64 &rhs) {
    v2_f64 ret;

    ret.x = lhs.x / rhs.x;
    ret.y = lhs.y / rhs.y;

    return ret;
}

intern
v2_f64 operator/(const v2_f64 &lhs, const f64 &rhs) {
    v2_f64 ret;

    ret.x = lhs.x / rhs;
    ret.y = lhs.y / rhs;

    return ret;
}


intern
v2_f64 operator/(const f64 &lhs, const v2_f64 &rhs) {
    v2_f64 ret;

    ret.x = lhs / rhs.x;
    ret.y = lhs / rhs.y;

    return ret;
}



b32 operator ==(const v2_f64 &lhs, const v2_f64 &rhs) {
    return lhs.x == rhs.x 
        && lhs.y == rhs.y;
}

v2_f64 operator -(const v2_f64 &lhs, const v2_f64 &rhs) {
    v2_f64 ret;

    ret.x = lhs.x - rhs.x;
    ret.y = lhs.y - rhs.y;

    return ret;
}

v2_f64 operator +(const v2_f64 &lhs, const v2_f64 &rhs) {
    v2_f64 ret;

    ret.x = lhs.x + rhs.x;
    ret.y = lhs.y + rhs.y;

    return ret;
}

v2_f64 operator +(const v2_f64 &lhs, const f64 &rhs) {
    v2_f64 ret;

    ret.x = lhs.x + rhs;
    ret.y = lhs.y + rhs;

    return ret;
}

v2_f64 operator -(const v2_f64 &lhs, const f64 &rhs) {
    v2_f64 ret;

    ret.x = lhs.x - rhs;
    ret.y = lhs.y - rhs;

    return ret;
}

intern force_inline constexpr
v2_f64 make_vector_f64(f64 x, f64 y) {
    v2_f64 ret = {};

    ret.x = x;
    ret.y = y;

    return ret;
}

intern force_inline
v2_f64 make_vector_f64(v2_s32 v) {
    v2_f64 ret;

    ret.x = (f64)v.x;
    ret.y = (f64)v.y;

    return ret;
}

intern force_inline
v2_s32 make_vector_s32(s32 x, s32 y) {
    v2_s32 ret;

    ret.x = x;
    ret.y = y;

    return ret;
}

intern force_inline
v2_s32 _v2s32(s32 x, s32 y) {
    return make_vector_s32(x,y);
}


intern force_inline
v2_s32 make_vector_s32(v2_s32 v) {
    v2_s32 ret;

    ret.x = v.x,
    ret.y = v.y;

    return ret;
}

intern force_inline
v2_s32 make_vector_s32(v2_f64 v) {
    v2_s32 ret;

    ret.x = (s32)v.x,
    ret.y = (s32)v.y;

    return ret;
}

template<typename T> intern force_inline T MAX(T a, T b) { return a > b ? a : b; }
template<typename T> intern force_inline T MIN(T a, T b) { return a > b ? b : a; }

struct v3 {
    f32 x;
    f32 y;
    f32 z;
};

v3 operator -(const v3 & lhs, const v3 & rhs) {
    v3 ret;

    ret.x = lhs.x - rhs.x;
    ret.y = lhs.y - rhs.y;
    ret.z = lhs.z - rhs.z;

    return ret;
}

v3 operator +(const v3 & lhs, const v3 & rhs) {
    v3 ret;

    ret.x = lhs.x + rhs.x;
    ret.y = lhs.y + rhs.y;
    ret.z = lhs.z + rhs.z;

    return ret;
}


v3 operator *(const v3 & lhs, const f32 & rhs) {
    v3 ret;

    ret.x = lhs.x * rhs;
    ret.y = lhs.y * rhs;
    ret.z = lhs.z * rhs;

    return ret;
}

struct v3_f64 {
    union {
        struct {
            f64 x;
            f64 y;
            f64 z;
        };

        struct {
            v2_f64 xy;
        };

        struct {
            f64 __unused;
            v2_f64 yz;
        };

        f64 nth[3];
    };
    
    v3_f64& operator *=(const f64 &rhs) {
        this->x *= rhs;
        this->y *= rhs;
        this->z *= rhs;

        return *this;
    }

    v3_f64& operator /=(const f64 &rhs) {
        this->x /= rhs;
        this->y /= rhs;
        this->z /= rhs;

        return *this;
    }

    v3_f64 operator-() {
        v3_f64 ret;

        ret.x = -this->x;
        ret.y = -this->y;
        ret.z = -this->z;

        return ret;
    }

    v3_f64& operator +=(const f64 &rhs) {
        this->x += rhs;
        this->y += rhs;
        this->z += rhs;

        return *this;
    }
};

intern
v3_f64 operator*(const v3_f64 &lhs, const f64 &rhs) {
    v3_f64 ret;

    ret.x = lhs.x * rhs;
    ret.y = lhs.y * rhs;
    ret.z = lhs.z * rhs;

    return ret;
}

intern
v3_f64 operator/(const v3_f64 &lhs, const f64 &rhs) {
    v3_f64 ret;

    ret.x = lhs.x / rhs;
    ret.y = lhs.y / rhs;
    ret.z = lhs.z / rhs;

    return ret;
}

intern
v3_f64 operator-(const v3_f64 & lhs, const v3_f64 & rhs) {
    v3_f64 ret;

    ret.x = lhs.x - rhs.x;
    ret.y = lhs.y - rhs.y;
    ret.z = lhs.z - rhs.z;

    return ret;
}

intern
v3_f64 operator+(const v3_f64 & lhs, const v3_f64 & rhs) {
    v3_f64 ret;

    ret.x = lhs.x + rhs.x;
    ret.y = lhs.y + rhs.y;
    ret.z = lhs.z + rhs.z;

    return ret;
}



intern force_inline
v3_f64 make_vector_f64(f64 x, f64 y, f64 z) {
    v3_f64 ret;

    ret.x = x;
    ret.y = y;
    ret.z = z;

    return ret;
}

intern force_inline
v3_f64 make_vector_f64(v2_f64 v, f64 z) {
    v3_f64 ret;

    ret.x = v.x;
    ret.y = v.y;
    ret.z = z;

    return ret;
}

struct v2 {
    f32 x;
    f32 y;

    v2& operator *=(const f32 &rhs) {
        this->x *= rhs;
        this->y *= rhs;
        return *this;
    }

    v2& operator *=(const v2 &rhs) {
        this->x *= rhs.x;
        this->y *= rhs.y;
        return *this;
    }


    v2& operator /=(const f32 &rhs) {
        this->x /= rhs;
        this->y /= rhs;
        return *this;
    }

    v2& operator /=(const v2&rhs) {
        this->x /= rhs.x;
        this->y /= rhs.y;
        return *this;
    }


    v2& operator -=(const v2 &rhs) {
        this->x -= rhs.x;
        this->y -= rhs.y;

        return *this;
    }

    v2& operator +=(const v2 &rhs) {
        this->x += rhs.x;
        this->y += rhs.y;

        return *this;
    }


    v2 operator-() {
        v2 ret;

        ret.x = -this->x;
        ret.y = -this->y;

        return ret;
    }
};

intern force_inline
v2 make_vector_f32(f32 x, f32 y) {
    v2 ret;
    ret.x = x;
    ret.y = y;
    return ret;
}


intern force_inline constexpr
v3_f64 make_vector_f64(v2 v, f64 z) {
    v3_f64 ret = {};

    ret.x = v.x;
    ret.y = v.y;
    ret.z = z;

    return ret;
}

intern
v2 perspective_project(v3 v) {
    v2 ret;

    ret.x = v.x / v.z;
    ret.y = v.y / v.z;

    return ret;
}

intern
v2_f64 perspective_project(v3_f64 v) {
    v2_f64 ret;

    ret.x = v.x / v.z;
    ret.y = v.y / v.z;

    return ret;
}

intern
v3 perspective_unproject(v2  v, f32 z) {
    v3 ret;

    ret.x = v.x * z;
    ret.y = v.y * z;
    ret.z = z;

    return ret;
}

intern
v3_f64 perspective_unproject(v2_f64  v, f64 z) {
    v3_f64 ret;

    ret.x = v.x * z;
    ret.y = v.y * z;
    ret.z = z;

    return ret;
}

struct v4 {
    f32 x;
    f32 y;
    f32 z;
    f32 w;
};

intern
v2 operator/(const f32 &lhs, const v2 &rhs) {
    v2 ret;

    ret.x = lhs / rhs.x;
    ret.y = lhs / rhs.y;

    return ret;
}

intern
v2 operator+(const v2 &lhs, const v2 &rhs) {
    v2 ret;

    ret.x = lhs.x + rhs.x;
    ret.y = lhs.y + rhs.y;

    return ret;
}

intern
v2 operator*(const v2 &lhs, const f32 &rhs) {
    v2 ret;

    ret.x = lhs.x * rhs;
    ret.y = lhs.y * rhs;

    return ret;
}

intern
v2 operator*(const v2 &lhs, const v2 &rhs) {
    v2 ret;

    ret.x = lhs.x * rhs.x;
    ret.y = lhs.y * rhs.y;

    return ret;
}

intern
f32 ceil_f32(f32 f) {
    return ceilf(f);
}

intern
f64 ceil_f64(f64 f) {
    return ceil(f);
}

template<typename TNum>
intern force_inline
void clamp(TNum *val, TNum min, TNum max) {
    if(min > *val) {
        *val = min;
    }
    else if(*val > max){
        *val = max;
    }
}

struct v4_f64 {

    union {
        struct { 
            f64 x;
            f64 y;
            f64 z;
            f64 w;
        };

        struct { 
            f64 r;
            f64 g;
            f64 b;
            f64 a;
        };

        v3_f64 xyz;
        v3_f64 rgb;

        v2_f64 rg;
        v2_f64 xy;

        f64 nth[4];
    };

    v4_f64& operator +=(const f64 &rhs) {
        this->x += rhs;
        this->y += rhs;
        this->z += rhs;
        this->w += rhs;

        return *this;
    }

    v4_f64 & operator *=(const v4_f64 & rhs) {
        this->x *= rhs.x;
        this->y *= rhs.y;
        this->z *= rhs.z;
        this->w *= rhs.w;

        return *this;
    }
};

intern force_inline
v4 v4_f64_to_v4_f32(v4_f64 v) {
    v4 ret;

    ret.x = v.x;
    ret.y = v.y;
    ret.z = v.z;
    ret.w = v.w;

    return ret;
}

intern force_inline
v4_f64 make_color(v3_f64 color, f64 alpha) {
    v4_f64 ret;

    ret.xyz = color;
    ret.a = alpha;

    return ret;
}

intern force_inline
v4_f64 make_color(v4_f64 color, f64 alpha) {
    v4_f64 ret;

    ret = color;
    ret.a = alpha;

    return ret;
}


intern force_inline
f32 sin_f32(f32 val) {
    return sinf(val);
}

intern force_inline
f32 cos_f32(f32 val) {
    return cosf(val);
}

struct Sin_Cos_f32 {
    f32 sin;
    f32 cos;
};

intern
Sin_Cos_f32 sin_cos_f32(f32 val) {
    Sin_Cos_f32 ret;

    ret.sin = sin_f32(val);
    ret.cos = cos_f32(val);

    return ret;
}

intern
f64 sin_f64(f64 val) {
    return sin(val);
}

intern
f64 cos_f64(f64 val) {
    return cos(val);
}

struct Sin_Cos_f64 {
    f64 sin;
    f64 cos;
};

intern
Sin_Cos_f64 sin_cos_f64(f64 val) {
    Sin_Cos_f64 ret;

    ret.sin = sin_f64(val);
    ret.cos = cos_f64(val);

    return ret;
}

intern
f64 sqrt_f64(f64 f) {
    return sqrt(f);
}

intern
f32 sqrt_f32(f32 f) {
    return sqrtf(f);
}

intern force_inline
f64 cos_to_sin_f64(f64 cos) {
    return sqrt_f64(1 - (cos * cos));
}

intern force_inline
f64 sin_to_cos_f64(f64 sin) {
    return sqrt_f64(1 - (sin * sin));
}


intern
Sin_Cos_f64 sin_cos_f64_from_cos(f64 angle_cos) {
    Sin_Cos_f64 ret;

    ret.cos = angle_cos;
    ret.sin = cos_to_sin_f64(angle_cos);

    return ret;
}

intern force_inline
v3 make_vector(f32 x, f32 y, f32 z) {
    v3 ret;

    ret.x = x;
    ret.y = y;
    ret.z = z;

    return ret;
}

intern force_inline
v2 make_vector(f32 x, f32 y) {
    v2 ret;

    ret.x = x;
    ret.y = y;

    return ret;
}

intern force_inline
v4 make_vector(s32 x, s32 y, s32 z, s32 w) {
    v4 ret;

    ret.x = (f32)x;
    ret.y = (f32)y;
    ret.z = (f32)z;
    ret.w = (f32)w;

    return ret;
}

intern force_inline
v4 make_vector(f32 x, f32 y, f32 z, f32 w) {
    v4 ret;

    ret.x = x;
    ret.y = y;
    ret.z = z;
    ret.w = w;

    return ret;
}

intern force_inline
v3 make_vector(v2 a, f32 z) {
    v3 ret;

    ret.x = a.x;
    ret.y = a.y;
    ret.z = z;

    return ret;
}

intern force_inline
f64 dot_product(v2_f64 a, v2_f64 b) {
    return a.x * b.x + a.y * b.y;
}

intern force_inline
f64 dot_product(v3_f64 a, v3_f64 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

intern force_inline
f32 dot_product(v2 a, v2 b) {
    return a.x * b.x + a.y * b.y;
}

intern force_inline
f32 dot_product(v3 a, v3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

struct m2_f64 {
    v2_f64 a;
    v2_f64 b;
};


// NOTE(justas): column order
struct m3_f64 {
    v3_f64 a;
    v3_f64 b;
    v3_f64 c;
};

intern force_inline
m3_f64 make_matrix_identity_m3_f64() {
    m3_f64 ret;

    ret.a = make_vector_f64(1, 0, 0);
    ret.b = make_vector_f64(0, 1, 0);
    ret.c = make_vector_f64(0, 0, 1);

    return ret;
}

intern force_inline
m2_f64 make_matrix_identity_m2_f64() {
    m2_f64 ret;

    ret.a = make_vector_f64(1, 0);
    ret.b = make_vector_f64(0, 1);

    return ret;
}


intern force_inline
m3_f64 matrix_transpose(const m3_f64 * matrix) {
    m3_f64 ret;

    ret.a = make_vector_f64(matrix->a.x, matrix->b.x, matrix->c.x);
    ret.b = make_vector_f64(matrix->a.y, matrix->b.y, matrix->c.y);
    ret.c = make_vector_f64(matrix->a.z, matrix->b.z, matrix->c.z);

    return ret;
}

intern force_inline
m2_f64 matrix_transpose(const m2_f64 * matrix) {
    m2_f64 ret;

    ret.a = make_vector_f64(matrix->a.x, matrix->b.x);
    ret.b = make_vector_f64(matrix->a.y, matrix->b.y);

    return ret;
}

intern force_inline
f64 cross_product(v2_f64 a, v2_f64 b) {
    return a.x * b.y 
         - a.y * b.x;
}

intern force_inline
v3_f64 cross_product(v3_f64 a, v3_f64 b) {
    v3_f64 ret;

    ret.x = (a.y * b.z) - (a.z * b.y);
    ret.y = (a.z * b.x) - (a.x * b.z);
    ret.z = (a.x * b.y) - (a.y * b.x);

    return ret;
}

intern force_inline
f64 matrix_determinant(
        m2_f64 * m, 
        b32 * out_is_valid_det
) {
    auto det = cross_product(m->a, m->b);
    if(det == 0) {
        *out_is_valid_det = false;
        return 0;
    }
    *out_is_valid_det = true;
    return det;
}
intern force_inline
m2_f64 matrix_inverse(m2_f64 * m, b32 * out_is_valid_det) {

    auto det = matrix_determinant(m, out_is_valid_det);

    if(!(*out_is_valid_det)) {
        return make_matrix_identity_m2_f64();
    }

    if(det == -1.0 || det == 1.0) {
        return matrix_transpose(m);
    }

    m2_f64 ret;

    ret.a = make_vector_f64(m->b.y / det, -m->a.y / det);
    ret.b = make_vector_f64(-m->b.x / det, m->a.x / det);

    return ret;
}

intern force_inline
v3_f64 make_vector(v2_f64 p, f64 z) {
    v3_f64 ret;

    ret.x = p.x;
    ret.y = p.y;
    ret.z = z;

    return ret;
}

intern force_inline
m2_f64 make_matrix_f64(v2_f64 a, v2_f64 b) {
    m2_f64 ret;

    ret.a = a;
    ret.b = b;

    return ret;
}

intern force_inline
m3_f64 make_matrix_f64(v3_f64 a, v3_f64 b, v3_f64 c) {
    m3_f64 ret;

    ret.a = a;
    ret.b = b;
    ret.c = c;

    return ret;
}

intern force_inline
m3_f64 matrix_inverse(m3_f64 * m, b32 * out_is_valid_det) {
    v3_f64 a = cross_product(m->b, m->c);
    
    f64 det = dot_product(m->a, a);

    if(det == 0) {
        *out_is_valid_det = false;
        return make_matrix_identity_m3_f64();
    }

    if(det == -1.0 || det == 1.0) {
        *out_is_valid_det = true;
        return matrix_transpose(m);
    }

    m3_f64 ret;

    ret.a = a;
    ret.b = cross_product(m->c, m->a);
    ret.c = cross_product(m->a, m->b);

    auto inverse_det = 1.0 / det;

    ret = matrix_transpose(&ret);

    ret.a *= inverse_det;
    ret.b *= inverse_det;
    ret.c *= inverse_det;
    *out_is_valid_det = true;

    return ret;
}

intern force_inline
m3_f64 make_matrix_translate_m3_f64(v2_f64 translation) {
    m3_f64 ret = make_matrix_identity_m3_f64();

    ret.c.x = translation.x;
    ret.c.y = translation.y;

    return ret;
}

intern force_inline
m2_f64 make_matrix_translate_1d_m2_f64(f64 translation) {
    m2_f64 ret = make_matrix_identity_m2_f64();

    ret.b.x = translation;

    return ret;
}

intern force_inline
m2_f64 make_matrix_scale_1d_m2_f64(f64 scale) {
    m2_f64 ret = make_matrix_identity_m2_f64();

    ret.a.x = scale;

    return ret;
}

intern force_inline
m3_f64 make_matrix_scale_m3_f64(v2_f64 scale) {
    m3_f64 ret = make_matrix_identity_m3_f64();

    ret.a.x = scale.x;
    ret.b.y = scale.y;

    return ret;
}

intern force_inline
m2_f64 make_matrix_scale_m2_f64(v2_f64 scale) {
    m2_f64 ret = make_matrix_identity_m2_f64();

    ret.a.x = scale.x;
    ret.b.y = scale.y;

    return ret;
}

intern force_inline
m3_f64 make_matrix_scale_m3_f64(f64 scale) {
    return make_matrix_scale_m3_f64(make_vector_f64(scale, scale));
}

intern force_inline
m3_f64 make_matrix_rotate_m3_f64(Sin_Cos_f64 rotation) {
    m3_f64 ret = make_matrix_identity_m3_f64();

    ret.a.x = rotation.cos;
    ret.a.y = rotation.sin;

    ret.b.x = -rotation.sin;
    ret.b.y =  rotation.cos;

    return ret;
}

intern force_inline
v3_f64 operator * (const m3_f64 & lhs, const v3_f64 & rhs) {
    v3_f64 ret;
    m3_f64 transposed_lhs = matrix_transpose(&lhs);

    ret.x = dot_product(transposed_lhs.a, rhs);
    ret.y = dot_product(transposed_lhs.b, rhs);
    ret.z = dot_product(transposed_lhs.c, rhs);
    
    return ret;
}

intern force_inline
v2_f64 operator * (const m2_f64 & lhs, const v2_f64 & rhs) {
    v2_f64 ret;
    m2_f64 transposed_lhs = matrix_transpose(&lhs);

    ret.x = dot_product(transposed_lhs.a, rhs);
    ret.y = dot_product(transposed_lhs.b, rhs);
    
    return ret;
}


intern force_inline
v2_f64 operator * (const m3_f64 & lhs, const v2_f64 & rhs) {
    v2_f64 ret;
    m3_f64 transposed_lhs = matrix_transpose(&lhs);

    auto upgraded_rhs = make_vector_f64(rhs, 1.0);

    ret.x = dot_product(transposed_lhs.a, upgraded_rhs);
    ret.y = dot_product(transposed_lhs.b, upgraded_rhs);
    
    return ret;
}

intern force_inline
m3_f64 operator * (const m3_f64 & lhs, const m3_f64 & rhs) {
    m3_f64 ret;
    m3_f64 transposed_lhs = matrix_transpose(&lhs);

    ret.a.x = dot_product(transposed_lhs.a, rhs.a);
    ret.b.x = dot_product(transposed_lhs.a, rhs.b);
    ret.c.x = dot_product(transposed_lhs.a, rhs.c);

    ret.a.y = dot_product(transposed_lhs.b, rhs.a);
    ret.b.y = dot_product(transposed_lhs.b, rhs.b);
    ret.c.y = dot_product(transposed_lhs.b, rhs.c);

    ret.a.z = dot_product(transposed_lhs.c, rhs.a);
    ret.b.z = dot_product(transposed_lhs.c, rhs.b);
    ret.c.z = dot_product(transposed_lhs.c, rhs.c);

    return ret;
}

intern force_inline
m2_f64 operator * (const m2_f64 & lhs, const m2_f64 & rhs) {
    m2_f64 ret;
    m2_f64 transposed_lhs = matrix_transpose(&lhs);

    ret.a.x = dot_product(transposed_lhs.a, rhs.a);
    ret.b.x = dot_product(transposed_lhs.a, rhs.b);

    ret.a.y = dot_product(transposed_lhs.b, rhs.a);
    ret.b.y = dot_product(transposed_lhs.b, rhs.b);

    return ret;
}

intern
Sin_Cos_f64 sin_cos_f64_invert(Sin_Cos_f64 * rot) {
    /*
     * NOTE(justas):
     * sin (–θ) = –sin θ
     * cos (–θ) = cos θ
     */
    Sin_Cos_f64 ret;

    ret.sin = -rot->sin;
    ret.cos = rot->cos;

    return ret;
}

intern force_inline
m3_f64 make_matrix_rotate_around_m3_f64(Sin_Cos_f64 rotation, v2_f64 origin) {
    m3_f64 ret = make_matrix_translate_m3_f64(origin)
        * make_matrix_rotate_m3_f64(rotation)
        * make_matrix_translate_m3_f64(-origin)
    ;

    return ret;
}

intern force_inline
m3_f64 operator*(const m3_f64 & rhs, const f64 & lhs) {
    m3_f64 ret;

    ret.a = rhs.a * lhs;
    ret.b = rhs.b * lhs;
    ret.c = rhs.c * lhs;

    return ret;
}

intern force_inline
m3_f64 operator+(const m3_f64 & rhs, const m3_f64 & lhs) {
    m3_f64 ret;

    ret.a = rhs.a + lhs.a;
    ret.b = rhs.b + lhs.b;
    ret.c = rhs.c + lhs.c;

    return ret;
}

intern force_inline
m3_f64 make_matrix_rotate_x_axis_m3_f64(Sin_Cos_f64 rot) {
    m3_f64 ret;

    ret.a = make_vector_f64(1, 0, 0);
    ret.b = make_vector_f64(0, rot.cos, -rot.sin);
    ret.c = make_vector_f64(0, rot.sin, rot.cos);
    

    return ret;
}

intern force_inline
m3_f64 make_matrix_rotate_y_axis_m3_f64(Sin_Cos_f64 rot) {
    m3_f64 ret;

    ret.a = make_vector_f64(rot.cos, 0, rot.sin);
    ret.b = make_vector_f64(0, 1, 0);
    ret.c = make_vector_f64(-rot.sin, 0, rot.cos);
    

    return ret;
}

intern force_inline
m3_f64 make_matrix_rotate_z_axis_m3_f64(Sin_Cos_f64 rot) {
    m3_f64 ret;

    ret.a = make_vector_f64(rot.cos, -rot.sin, 0);
    ret.b = make_vector_f64(rot.sin, rot.cos, 0);
    ret.c = make_vector_f64(0, 0, 1);

    return ret;
}



intern force_inline
m3_f64 make_matrix_scale_around_m3_f64(v2_f64 scale, v2_f64 origin) {
    m3_f64 ret = make_matrix_translate_m3_f64(origin)
        * make_matrix_scale_m3_f64(scale)
        * make_matrix_translate_m3_f64(-origin)
    ;

    return ret;
}

// NOTE(justas): column order
struct m3 {
    v3 a;
    v3 b;
    v3 c;
};

intern force_inline
m3 make_matrix_identity_m3() {
    m3 ret;

    ret.a = make_vector(1, 0, 0);
    ret.b = make_vector(0, 1, 0);
    ret.c = make_vector(0, 0, 1);

    return ret;
}

intern force_inline
m3 matrix_transpose(const m3 * m) {
    m3 ret;

    ret.a = make_vector(m->a.x, m->b.x, m->c.x);
    ret.b = make_vector(m->a.y, m->b.y, m->c.y);
    ret.c = make_vector(m->a.z, m->b.z, m->c.z);

    return ret;
}

intern force_inline
v2 operator * (const m3 & lhs, const v2 & rhs) {
    v2 ret;
    m3 transposed_lhs = matrix_transpose(&lhs);

    auto upgraded_rhs = make_vector(rhs, 1.0);

    ret.x = dot_product(transposed_lhs.a, upgraded_rhs);
    ret.y = dot_product(transposed_lhs.b, upgraded_rhs);
    
    return ret;
}


intern force_inline
m3 make_matrix_translate_m3(v2 translation) {
    m3 ret = make_matrix_identity_m3();

    ret.c.x = translation.x;
    ret.c.y = translation.y;

    return ret;
}

intern force_inline
m3 make_matrix_scale_m3(v2 scale) {
    m3 ret = make_matrix_identity_m3();

    ret.a.x = scale.x;
    ret.b.y = scale.y;

    return ret;
}

intern force_inline
m3 make_matrix_rotate_m3(Sin_Cos_f32 rotation) {
    m3 ret = make_matrix_identity_m3();

    ret.a.x = rotation.cos;
    ret.a.y = rotation.sin;

    ret.b.x = -rotation.sin;
    ret.b.y =  rotation.cos;

    return ret;
}

intern force_inline
v3 operator * (const m3 & lhs, const v3 & rhs) {
    v3 ret;
    m3 transposed = matrix_transpose(&lhs);

    ret.x = dot_product(transposed.a, rhs);
    ret.y = dot_product(transposed.b, rhs);
    ret.z = dot_product(transposed.c, rhs);

    return ret;
}

intern force_inline
m3 operator * (const m3 & lhs, const m3 & rhs) {
    m3 ret;
    m3 transposed_lhs = matrix_transpose(&lhs);

    ret.a.x = dot_product(transposed_lhs.a, rhs.a);
    ret.b.x = dot_product(transposed_lhs.a, rhs.b);
    ret.c.x = dot_product(transposed_lhs.a, rhs.c);

    ret.a.y = dot_product(transposed_lhs.b, rhs.a);
    ret.b.y = dot_product(transposed_lhs.b, rhs.b);
    ret.c.y = dot_product(transposed_lhs.b, rhs.c);

    ret.a.z = dot_product(transposed_lhs.c, rhs.a);
    ret.b.z = dot_product(transposed_lhs.c, rhs.b);
    ret.c.z = dot_product(transposed_lhs.c, rhs.c);

    return ret;
}

struct m4 {
    v4 a;
    v4 b;
    v4 c;
    v4 d;
};

intern
m4 make_matrix_identity_m4() {
    m4 ret;

    ret.a = make_vector(1, 0, 0, 0);
    ret.b = make_vector(0, 1, 0, 0);
    ret.c = make_vector(0, 0, 1, 0);
    ret.d = make_vector(0, 0, 0, 1);

    return ret;
}

intern
m4 make_orthographic_projection(f32 width, f32 height, f32 near_plane, f32 far_plane) {
    m4 ret = make_matrix_identity_m4();

    ret.a.x = 2.0f / width; 
    ret.b.y = 2.0f / height; 
    // NOTE(justas): originally (far - plane) but MSVC REALLY doesn't like that for some magical fucking reason???
    ret.c.z = 1.0f / (far_plane - near_plane); 
    ret.d.x = -1.0f;
    ret.d.y = -1.0f;

    return ret;
}

intern
f32 modulo_f32_round_zero(f32 dividend, f32 divisor) {
    return fmod(dividend, divisor);
}

intern
f64 modulo_f64_round_zero(f64 dividend, f64 divisor) {
    return fmod(dividend, divisor);
}

intern
f64 modulo_f64_round_negative_infinity(f64 dividend, f64 divisor) {
    f64 fract = dividend / divisor;

    // NOTE(justas): round here
    if(0 > fract) {
        fract -= 1.0f;
    }

    return dividend - (divisor * (s32)fract);
}

// NOTE(justas): returns either 1 or -1, no zeros.
intern force_inline
f64 get_sign_f64(f64 val) {
    baked s64 shift = 63;
    baked f64 signs[] = {1.0, -1.0};

    u64 reinterp = *(u64*)(void*)&val;
    
    u8 index = ((reinterp & ((u64)1 << shift)) >> shift);
    return signs[index];
}

intern force_inline
s64 get_sign(s64 val) {
    if(val >= 0) {
        return 1;
    }
    return -1;
}

intern
f32 absolute_f32(f32 val) {
    return fabs(val);
}

intern
f64 absolute_f64(f64 val) {
    return fabs(val);
}

intern
v2_f64 absolute_f64(v2_f64 val) {
    v2_f64 ret;

    ret.x = absolute_f64(val.x);
    ret.y = absolute_f64(val.y);

    return ret;
}

intern
s32 absolute_s32(s32 val) {
    return (s32)abs(val);
}

intern
s64 absolute_s64(s64 val) {
    return (s64)abs(val);
}


/*
 *  NOTE(justas): 
 *  3.1 -> 3
 *  2.6 -> 3
 *  2.5 -> 3
 *
 *  2.4 -> 2
 *  3.5 -> 4
 *  3.6 -> 4
 *
 *  -3.1 -> -3
 *  -3.6 -> -3
 *  -2.5 -> -3
 *  etc
 */
intern force_inline
f64 round_towards_nearest_integer_f64(f64 val) {
    f64 sign = get_sign_f64(val);
    return sign * ((int)(absolute_f64(val) + 0.5));
}

/*
intern force_inline
f64 round_toward_zero_f64(f64 val) {
    return get_sign_f64(val) * floor_f64(absolute_f64(val));
}
*/

struct rect_f64 {
    v2_f64 position; // bottom-left
    v2_f64 extents; // position + extents = top right
};

intern
rect_f64 make_rect_f64 (v2_f64 position, v2_f64 extents) {
    rect_f64 ret;

    ret.position = position;
    ret.extents = extents;

    return ret;
}

intern
rect_f64 make_rect_f64(f64 pos_x, f64 pos_y, f64 ext_x, f64 ext_y) {
    rect_f64 ret;

    ret.position.x = pos_x;
    ret.position.y = pos_y;

    ret.extents.x = ext_x;
    ret.extents.y = ext_y;

    return ret;
}


intern
v2_f64 rect_bottom_left_f64(rect_f64 r ) {
    return r.position;
}

intern
v2_f64 rect_bottom_right_f64(rect_f64 r) {
    return make_vector_f64(r.position.x + r.extents.x, r.position.y);
}

intern
v2_f64 rect_top_left_f64(rect_f64 r) {
    return make_vector_f64(r.position.x, r.position.y + r.extents.y);
}

intern
v2_f64 rect_top_right_f64(rect_f64 r) {
    return r.position + r.extents;
}

intern force_inline
v2_f64 rect_center_f64(rect_f64 r) {
    return r.position + r.extents * 0.5;
}

intern force_inline
rect_f64 rect_scale_with_origin_as_center(rect_f64 bb, f64 scalar) {
    bb.position += bb.extents * (1 - scalar) * .5;
    bb.extents *= scalar;
    return bb;
}

intern force_inline
v2_f64 rect_halfway_left(rect_f64 r) {
    return rect_bottom_left_f64(r) + make_vector_f64(0, r.extents.y) * .5;
}

intern force_inline
v2_f64 rect_halfway_right(rect_f64 r) {
    return rect_bottom_right_f64(r) + make_vector_f64(0, r.extents.y) * .5;
}

intern force_inline
rect_f64 rect_convert_between_bottom_left_and_ui(rect_f64 r) {
    rect_f64 ret;

    ret.position = make_vector_f64(r.position.x, -r.position.y - r.extents.y);
    ret.extents = r.extents;

    return ret;
}

intern force_inline
v2_f64 v2_f64_convert_between_bottom_left_and_ui(v2_f64 p) {
    return make_vector_f64(p.x, -p.y);
}


force_inline
b32 is_power_of_two(s64 val) {
    if(val <=  0) { 
        return false;
    }

    return ((val - 1) & val) == 0;
}

intern
f32 floor_f32(f32 f) {
    return floorf(f);
}

intern
f64 floor_f64(f64 f) {
    return floor(f);
}

template<typename T>
intern force_inline
T bit_set(T val, T nth_bit) {
    return val | (1 << nth_bit);
}

template<typename T>
intern force_inline
T bit_clear(T val, T nth_bit) {
    return val & ~(1 << nth_bit);
}


template<typename T>
intern force_inline
b32 bit_is_set(T val, T nth_bit) {
    return (val & (1 << nth_bit)) != 0;
}

template<typename T>
intern force_inline
T bit_set_state(T val, T nth_bit, b32 true_if_on) {
    if(true_if_on) {
        return bit_set(val, nth_bit);
    }
    else {
        return bit_clear(val, nth_bit);
    }
}

template<typename T>
intern force_inline
T bit_set_state_mask(T val, T mask, b32 true_if_on) {
    if(true_if_on) {
        return val | mask;
    }
    else {
        return val & ~mask;
    }
}


intern
s32 next_power_of_two(s32 v) {
    // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2

    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

template<typename TNum>
intern
TNum get_min_max_delta(TNum val, TNum min, TNum max) {
    if(min > val) {
        return val - min;
    }
    else if(val > max) {
        return val - max;
    }

    return 0;
}

intern force_inline
v2 v2_f64_to_v2_f32(v2_f64 v) {
    v2 ret;
    ret.x = (f32)v.x;
    ret.y = (f32)v.y;
    return ret;
}

intern force_inline
v2_f64 v2_f32_to_v2_f64(v2 v) {
    return make_vector_f64(v.x, v.y);
}

v2 operator -(const v2 &lhs, const v2 &rhs) {
    v2 ret;

    ret.x = lhs.x - rhs.x;
    ret.y = lhs.y - rhs.y;

    return ret;
}

intern
f64 lerp_f64(f64 start, f64 time, f64 end) {
    return (end - start) * time + start;
}

intern
v2_f64 vec_lerp(v2_f64 start, f64 time, v2_f64 end) {
    v2_f64 ret;

    ret.x = lerp_f64(start.x, time, end.x);
    ret.y = lerp_f64(start.y, time, end.y);

    return ret;
}

intern
v3_f64 vec_lerp(v3_f64 start, f64 time, v3_f64 end) {
    v3_f64 ret;

    ret.x = lerp_f64(start.x, time, end.x);
    ret.y = lerp_f64(start.y, time, end.y);
    ret.z = lerp_f64(start.z, time, end.z);

    return ret;
}


intern
f32 vec_length(v2 p) {
    return sqrt_f32(dot_product(p, p));
}

intern
f64 vec_length(v2_f64 p) {
    return sqrt_f64(dot_product(p, p));
}

intern
f32 vec_length(v3 p) {
    return sqrt_f32(dot_product(p, p));
}

intern
f64 vec_length(v3_f64 p) {
    return sqrt_f64(dot_product(p, p));
}

intern
v2 normalize(v2 p) {
    v2 ret;
    f32 length = vec_length(p);

    ret.x = p.x / length;
    ret.y = p.y / length;

    return ret;
}

intern
v3 normalize(v3 p) {
    v3 ret;
    f32 length = vec_length(p);

    ret.x = p.x / length;
    ret.y = p.y / length;
    ret.z = p.z / length;

    return ret;
}

intern
v3_f64 normalize(v3_f64 p) {
    v3_f64 ret;
    f64 length = vec_length(p);

    ret.x = p.x / length;
    ret.y = p.y / length;
    ret.z = p.z / length;

    return ret;
}

intern
v2_f64 normalize(v2_f64 p) {
    v2_f64 ret;
    f64 length = vec_length(p);

    ret.x = p.x / length;
    ret.y = p.y / length;

    return ret;
}

intern force_inline
v2_f64 v2_rotate_90(v2_f64 p) {
    v2_f64 ret;

    ret.x = -p.y;
    ret.y = p.x;

    return ret;
}

intern
v2_f64 v2_rotate(v2_f64 p, f64 sin, f64 cos) {
    v2_f64 ret;

    ret.x = p.x * cos - p.y * sin;
    ret.y = p.x * sin + p.y * cos;

    return ret;
}

intern
v2_f64 v2_rotate(v2_f64 p, Sin_Cos_f64 sc) {
    return v2_rotate(p, sc.sin, sc.cos);
}

intern force_inline
v2_f64 v2_rotate_45(v2_f64 p) {
    return v2_rotate(p, 0.70710678118, 0.70710678118);
}

intern force_inline
v2_f64 v2_rotate_135(v2_f64 p) {
    return v2_rotate(p, 0.70710678118, -0.70710678118);
}

intern force_inline
v2_f64 v2_rotate_180(v2_f64 p) {
    return -p;
}

intern force_inline
v2_f64 v2_rotate_225(v2_f64 p) {
    return v2_rotate_45(-p);
}

intern force_inline
v2_f64 v2_rotate_270(v2_f64 p) {
    return v2_rotate_90(-p);
}

intern force_inline
v2_f64 v2_rotate_315(v2_f64 p) {
    return v2_rotate_135(-p);
}

intern force_inline
v2_f64 v2_rotate_degrees(v2_f64 p, f64 angle_degrees) {
    auto rotation = sin_cos_f64(angle_degrees * DEG_TO_RAD);

    return v2_rotate(p, rotation);
}

intern force_inline
v2_f64 v2_rotate_radians(v2_f64 p, f64 angle_rad) {
    return v2_rotate(p, sin_cos_f64(angle_rad));
}

baked v3_f64 zero_vector3_f64 = make_vector_f64(0.0, 0.0, 0);
baked v3_f64 x_axis3_f64 = make_vector_f64(1.0, 0.0, 0);
baked v3_f64 y_axis3_f64 = make_vector_f64(0.0, 1.0, 0);
baked v3_f64 z_axis3_f64 = make_vector_f64(0.0, 0.0, 1);

baked v2_f64 zero_vector_f64 = make_vector_f64(0.0, 0.0);
baked v2_f64 half_vector_f64 = make_vector_f64(0.5, 0.5);
baked v2_f64 unit_vector_f64 = make_vector_f64(1,1);
baked v2_f64 x_axis_f64 = make_vector_f64(1,0);
baked v2_f64 y_axis_f64 = make_vector_f64(0,1);

baked v2 zero_vector_f32 = make_vector(0.0f, 0.0f);
baked v2 half_vector_f32 = make_vector(0.5f, 0.5f);
baked v2 unit_vector_f32 = make_vector(1,1);
baked v2 x_axis_f32 = make_vector(1,0);
baked v2 y_axis_f32 = make_vector(0,1);


baked v2_s32 zero_vector_s32 = make_vector_s32(0, 0);

baked rect_f64 unit_rect_f64 = make_rect_f64(make_vector_f64(0, 0), make_vector_f64(1, 1));
baked rect_f64 zero_rect_f64 = make_rect_f64(make_vector_f64(0, 0), make_vector_f64(0, 0));

intern
Allocate_String_Result string_concat(String * strings, s64 num_strings, Memory_Allocator * allocator) {
    s64 length = 0;

    for(s32 string_index = 0;
            string_index < num_strings;
            string_index++) {

        String * txt = strings + string_index;

        length += txt->length;
    }

    auto mem = memory_allocator_allocate(allocator, length, "string_concat return buffer");
    u8 * write_cursor = (u8*)mem.data;

    for(s32 string_index = 0;
            string_index < num_strings;
            string_index++) {

        String * txt = strings + string_index;

        copy_bytes(write_cursor, (u8*)txt->str, txt->length);
        write_cursor += txt->length;
    }

    Allocate_String_Result ret;

    ret.mem = mem;
    ret.string= make_string((char*)mem.data, length);

    return ret;
}

intern
Allocate_String_Result string_concat(String a, String b, Memory_Allocator * allocator) {
    String arr[] = {a, b};
    return string_concat(arr, ARRAY_SIZE(arr), allocator);
}

intern force_inline
String string_concat_temp(String * strings, s64 num_strings, Memory_Allocator * temp_allocator) {
    return string_concat(strings, num_strings, temp_allocator).string;
}

intern force_inline
String string_concat_temp(String a, String b, Memory_Allocator * temp_allocator) {
    return string_concat(a, b, temp_allocator).string;
}


intern
b32 is_point_in_rect_f64(rect_f64 r, v2_f64 p) {
    b32 x_ok = p.x > r.position.x && (r.position.x + r.extents.x) > p.x;
    b32 y_ok = p.y > r.position.y && (r.position.y + r.extents.y) > p.y;

    return x_ok && y_ok;
}

intern force_inline
f64 v2_get_min(v2_f64 v) {
    return MIN(v.x, v.y);
}

intern force_inline
f64 v2_get_max(v2_f64 v) {
    return MAX(v.x, v.y);
}

intern force_inline
s32 log2_s32(s32 v) {
    return (s32)log2f((f32)v);
}

#define S64_POSITIVE_MAXIMUM +9,223,372,036,854,775,807
#define S64_NEGATIVE_MAXIMUM −9,223,372,036,854,775,808 
#define S8_POSITIVE_MAXIMUM 126
#define S8_NEGATIVE_MAXIMUM -127

#define F64_POSITIVE_MAXIMUM 1.7976931348623158e+308
#define F64_NEGATIVE_MAXIMUM -1.7976931348623158e+308
#define F64_POSITIVE_EPSILON 2.2204460492503131e-016

intern force_inline
b32 char_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

intern force_inline
b32 char_is_number(char c) {
    return (u8)c >= 48 && 57 >= (u8)c;
}

intern force_inline
b32 char_is_number_in_hex(char c) {
    return ((u8)c >= 65 && (u8)c <= 70) // NOTE(justas): uppercase
        || ((u8)c >= 97 && (u8)c <= 102); // NOTE(justas): lowercase
}

intern force_inline
s64 pow_s64(s64 val, s64 power) {
    return (s64)pow(val, power);
}

intern force_inline
f64 log10_f64(f64 val) {
    return log10(val);
}

intern force_inline
s32 char_to_number_unchecked(char c) {
    return (u8)c - 48;
}

intern force_inline
char number_to_char_unchecked(s64 val) {
    return (char)(val + 48);
}

intern force_inline
s32 lower_char_to_number_hex_unchecked(char c) {
    return (u8)c - 97 + 10;
}

struct Interp_Context {
    String error;
    Memory_Allocator * temp_allocator;
    Memory_Allocator * lifetime_allocator;

    s64 line;
    s64 column;
};

intern force_inline
Interp_Context make_interp_context(
        Memory_Allocator * temp_allocator, 
        Memory_Allocator * lifetime_allocator
) {
    Interp_Context ret;

    ret.error = empty_string;
    ret.temp_allocator = temp_allocator;
    ret.lifetime_allocator = lifetime_allocator;

    return ret;
}

template<typename T>
struct Tokenizer {
    T * data;
    s64 num_elements;
    s64 current_index;
    b32 is_eof;
    
    s32 direction;

    s64 line;
    s64 column;
};

template<typename T>
intern force_inline
Tokenizer<T> make_tokenizer(T * data, s64 num_elements) {
    assert(num_elements >= 0);

    Tokenizer<T> ret;

    ret.data = data;
    ret.num_elements = num_elements;
    ret.direction = 1;
    ret.current_index = 0;
    ret.is_eof = false;
    ret.line = 1;
    ret.column = 0;

    return ret;
}

intern force_inline
Tokenizer<char> make_tokenizer(String data) {
    return make_tokenizer((char*)data.str, data.length);
}

intern force_inline
Tokenizer<char> make_tokenizer_backwards(String data) {
    auto ret = make_tokenizer(data);

    ret.current_index = data.length - 1;
    ret.direction = -1;

    return ret;
}

template<typename T>
intern force_inline
b32 tokenizer_can_lookahead(Tokenizer<T> * tkn, s64 num_elements) {
    auto abs_idx = tkn->current_index + num_elements * tkn->direction;

    return tkn->num_elements > abs_idx && 0 <= abs_idx;
}

template<typename T>
intern force_inline
b32 tokenizer_is_eof(Tokenizer<T> * tkn) {
    return tkn->is_eof;
}

template<typename T>
intern force_inline
T * tokenizer_get_current(Tokenizer<T> * tkn) {
    assert(!tokenizer_is_eof(tkn));

    return tkn->data + tkn->current_index;
}

template<typename T>
intern force_inline
void tokenizer_mark_eof_if_needed(Tokenizer<T> * tkn) {
    if(0 > tkn->current_index || tkn->num_elements <= tkn->current_index) {
        tkn->is_eof = true;
    }
}

template<typename T>
intern force_inline
void tokenizer_consume(Tokenizer<T> * tkn) {
    assert(!tokenizer_is_eof(tkn));

    tkn->current_index += tkn->direction;

    tokenizer_mark_eof_if_needed(tkn);
}

template<typename T>
intern force_inline
T * tokenizer_lookahead_as_ptr(Tokenizer<T> * tkn, s64 num) {
    assert(tokenizer_can_lookahead(tkn, num));

    return tkn->data + (tkn->current_index + num * tkn->direction);
}

template<typename T>
intern force_inline
T tokenizer_lookahead(Tokenizer<T> * tkn, s64 num) {
    assert(tokenizer_can_lookahead(tkn, num));

    return tokenizer_lookahead_as_ptr(tkn, num);
}

template<typename T>
intern force_inline
T * tokenizer_try_lookahead(Tokenizer<T> * tkn, s64 num) {
    if(!tokenizer_can_lookahead(tkn, num)) {
        return 0;
    }

    return tokenizer_lookahead_as_ptr(tkn, num);
}
intern force_inline
void tokenizer_consume(Tokenizer<char> * tkn) {
    assert(!tokenizer_is_eof(tkn));

    if(tkn->direction == -1) {
        auto * c = tokenizer_try_lookahead(tkn, 1);
        if(c && *c == '\n') {
            tkn->line--;
            tkn->column = 0;
        }
        else {
            tkn->column--;
        }
    }
    else {
        char c = *tokenizer_get_current(tkn);
        if(c == '\n') {
            tkn->line++;
            tkn->column = 0;
        }
        else {
            tkn->column++;
        }
    }

    tkn->current_index += tkn->direction;
    tokenizer_mark_eof_if_needed(tkn);
}

intern force_inline
void tokenizer_skip_whitespace(Tokenizer<char> * tkn) {
    while(!tokenizer_is_eof(tkn)) {
        char c = *tokenizer_get_current(tkn);

        if(!char_is_space(c)) {
            break;
        }

        tokenizer_consume(tkn);
    }
}


intern force_inline
String tokenizer_get_empty_string_at_current_location(Tokenizer<char> * tkn) {
    return make_string(
        tkn->data + tkn->current_index,
        0
    );
}

intern force_inline constexpr
u64 hash_fnv(String str, u64 hash = 14695981039346656037UL)
{
    for(s64 i = 0; i < str.length; i++) {
        hash = hash ^ str.str[i];
        hash = hash * 1099511628211UL;
    }
    return hash;
}

template<typename Token, typename UntilFx, typename Context = void>
intern 
String tokenizer_read_until(
        Tokenizer<Token> * tkn,
        UntilFx && should_break,
        Context * ctx = 0
) {
    assert(tkn->direction == 1);

    auto text = tokenizer_get_empty_string_at_current_location(tkn);

    while(!tokenizer_is_eof(tkn)) {
        char c = *tokenizer_get_current(tkn);

        if(should_break(c, ctx)) {
            break;
        }

        ++text.length;

        tokenizer_consume(tkn);
    }

    return text;
}

enum NUMBER_TOKENIZER_TOKEN_ {
    NUMBER_TOKENIZER_TOKEN_SIGN,
    NUMBER_TOKENIZER_TOKEN_CHARACTER,
    NUMBER_TOKENIZER_TOKEN_EOF
};

struct Number_tokenizer_Token {
    NUMBER_TOKENIZER_TOKEN_ type;

    union {
        char character;
        b32 is_positive;
    };
};

intern force_inline
b32 tokenize_number_parser(Interp_Context * interp, Tokenizer<char> * tkn, Number_tokenizer_Token * token) {

    while(true) {
        tokenizer_skip_whitespace(tkn);

        if(tokenizer_is_eof(tkn)) {
            token->type = NUMBER_TOKENIZER_TOKEN_EOF;
            return true;
        }

        char c = *tokenizer_get_current(tkn);
        
        if(c == '+') {
            tokenizer_consume(tkn);
            token->type = NUMBER_TOKENIZER_TOKEN_SIGN;
            token->is_positive = true;
            return true;
        }
        else if(c == '-') {
            tokenizer_consume(tkn);
            token->type = NUMBER_TOKENIZER_TOKEN_SIGN;
            token->is_positive = false;
            return true;
        }
        else {
            tokenizer_consume(tkn);
            token->type = NUMBER_TOKENIZER_TOKEN_CHARACTER;
            token->character = c;
            return true;
        }
    }

    assert(false);
}

template<typename ...TArgs>
intern force_inline
void interp_report_error(Interp_Context * interp, const char * fmt, TArgs... args) {
    interp->error = format_temp_string(interp->temp_allocator, fmt, args...);
}

template<typename TLineColumnSource, typename ...TArgs>
intern force_inline
void interp_report_error(TLineColumnSource * source, Interp_Context * interp, const char * fmt, TArgs... args) {
    interp->error = format_temp_string(interp->temp_allocator, fmt, args...);
    interp->line = source->line;
    interp->column = source->column;
}

intern 
b32 try_read_number_base_10(Interp_Context * interp, String word, s64 * out_number) {
    auto tokenizer_no_ptr = make_tokenizer_backwards(word);
    auto * tkn = &tokenizer_no_ptr;

    s64 nth_number = 0;
    s64 accum = 0;

    enum STAGE_ {
        STAGE_NUMBER,
        STAGE_SIGN,
        STAGE_DONE,
    };

    auto stage = STAGE_NUMBER;

    Number_tokenizer_Token token;
    Number_tokenizer_Token token_next;
    auto status = tokenize_number_parser(interp, tkn, &token);
    auto status_next = tokenize_number_parser(interp, tkn, &token_next);

    if(!status) {
        return false;
    }

    do {
        if(!status_next) {
            return false;
        }
        
        if(stage == STAGE_NUMBER) {
            if(token.type == NUMBER_TOKENIZER_TOKEN_CHARACTER) {
                if(char_is_number(token.character)) {
                    s32 number = char_to_number_unchecked(token.character);
                    accum += number * pow_s64(10, nth_number);
                    nth_number++;

                    if(token_next.type == NUMBER_TOKENIZER_TOKEN_SIGN) {
                        stage = STAGE_SIGN;
                    }
                }
                else {
                    interp_report_error(interp, "unexpected character '%c' while parsing number\n", token.character);
                    return false;
                }
            }
            else if(token.type == NUMBER_TOKENIZER_TOKEN_EOF) {
                stage = STAGE_DONE;
            }
            else {
                interp_report_error(interp, "unexpected token '%d' while parsing number\n", token.type);
                return false;
            }
        }
        else if(stage == STAGE_SIGN) {
            if(token.type == NUMBER_TOKENIZER_TOKEN_SIGN) {
                if(!token.is_positive) {
                    accum *= -1;
                }

                if(token_next.type == NUMBER_TOKENIZER_TOKEN_EOF) {
                    stage = STAGE_DONE;
                }
                else {
                    interp_report_error(interp, "found trailing tokens when parsing number\n");
                    return false;
                }
            }
            else {
                interp_report_error(interp, "unexpected token '%d' while parsing number sign\n", token.type);
                return false;
            }
        }
        else {
            interp_report_error(interp, "internal error: unexpected number parser state\n");
            return false;
        }

        if(stage == STAGE_DONE) {
            *out_number = accum;
            return true;
        }

        token = token_next;
        status = status_next ;

        status_next = tokenize_number_parser(interp, tkn, &token_next);

    }while(true);
}

intern
b32 string_parse_s64(String str, s64 * out_val, Memory_Allocator * temp_allocator) {
    if(str.length <= 0) {
        return false;
    }

    auto interp = make_interp_context(temp_allocator, temp_allocator);
    return try_read_number_base_10(&interp, str, out_val);
}

intern
b32 string_parse_f64(
        String str, 
        f64 * out_val,
        Memory_Allocator * temp_allocator
) {
    auto cstdlib_is_dumb = copy_and_null_terminate_string(str, temp_allocator, "string_parse_f64 buf");
    f64 buf;

    auto expect_1 = sscanf(cstdlib_is_dumb, "%f", &buf);
    if(expect_1 != 1) {
        return false;
    }

    *out_val = buf;

    return true;
}

intern String path_to_run_tree = "../run_tree/"_S;

intern force_inline
String make_temp_path_to_file_in_run_tree(String relative_path, 
                                              Memory_Allocator * temp_allocator
) {
    // TODO(justas): compensate for the CWD being any possible dir :Win32
    // :RunTreeCwd
    // possibly https://stackoverflow.com/questions/933850/how-do-i-find-the-location-of-the-executable-in-c

    String strs[] = {
        path_to_run_tree,
        relative_path
    };

    return string_concat(strs, ARRAY_SIZE(strs), temp_allocator).string;
}

intern
b32 is_point_in_circle(v2_f64 * point, v2_f64 * circle_origin, f64 circle_radius_max, f64 circle_radius_min = 0.0f) {
    v2_f64 origin_to_point = *point - *circle_origin;
    f64 otp_length_sq = dot_product(origin_to_point, origin_to_point);

    circle_radius_max *= circle_radius_max;

    if(otp_length_sq > circle_radius_max) {
        return false;
    }

    if(circle_radius_min > 0) {
        circle_radius_min *= circle_radius_min;

        if(circle_radius_min > otp_length_sq) {
            return false;
        }
    }

    return true;
}
intern force_inline
f64 atan2_f64(f64 y, f64 x) {
    return (f64)atan2(y, x);
}

intern force_inline
f64 atan2_f64(v2_f64 v) {
    return (f64)atan2(v.y, v.x);
}

intern force_inline
f64 tan_f64(f64 v) {
    return (f64)tan(v);
}

intern force_inline
f64 atan_f64(f64 v) {
    return (f64)atan(v);
}



intern
v3 v3_f64_to_v3_f32(v3_f64 v) {
    v3 ret;

    ret.x = (f32)v.x;
    ret.y = (f32)v.y;
    ret.z = (f32)v.z;

    return ret;
}


intern force_inline
m3 m3_f64_to_m3_f32(m3_f64 * m) {
    m3 ret;

    ret.a = v3_f64_to_v3_f32(m->a);
    ret.b = v3_f64_to_v3_f32(m->b);
    ret.c = v3_f64_to_v3_f32(m->c);

    return ret;
}


template<typename T>
struct Stack {
    T * storage;

    Memory_Allocation mem;
    Memory_Allocator * allocator;
    
    s64 starting_size;
    s64 watermark;

    b32 log_realloc;

    String name;

    struct Iterator {
        s64 index;
        Stack<T> * stack;

        Iterator operator ++() {
            this->index++;
            return *this;
        }

        b32 operator !=(const Iterator & rhs) const {
            return !(this->index == rhs.index && this->stack == rhs.stack);
        }

        T * operator *() {
            return this->stack->storage + this->index;
        }
    };

    Iterator begin() {
        Iterator iter;
        iter.index = 0;
        iter.stack = this;
        return iter;
    }

    Iterator end() {
        Iterator iter;
        iter.index = watermark;
        iter.stack = this;
        return iter;
    }
};

template<typename T>
intern force_inline
s64 stack_get_num_bytes_used(Stack<T> * stack) {
    return stack->watermark * sizeof(T);
}

template<typename T>
intern force_inline
Stack<T> make_stack_empty() {
    Stack<T> ret;

    ret.storage = 0;
    ret.mem = null_page;
    ret.allocator = 0;
    ret.starting_size = 0;
    ret.watermark = 0;
    ret.name = empty_string;

    return ret;
}

template<typename T>
intern
Stack<T> make_stack(s64 num_starting_elements, 
                                    Memory_Allocator * allocator, 
                                    String name,
                                    b32 log_reallocation = false
) {
    assert(num_starting_elements > 0);

    Stack<T> ret;
    ret.allocator = allocator;
    ret.log_realloc = log_reallocation;

    auto num_bytes = sizeof(T) * num_starting_elements;
    auto mem = memory_allocator_allocate(allocator, num_bytes, name.str);

    ret.name = name;
    ret.storage = (T*)mem.data;
    ret.starting_size = mem.length;
    ret.mem = mem;
    ret.watermark = 0;

    return ret;
}

template<typename T>
intern force_inline
T * stack_get_at_index(Stack<T> * stack, s64 index) {
    if(0 > index || index >= stack->watermark) {
        return 0;
    }

    return stack->storage + index;
}

template<typename T>
intern force_inline
T * stack_peek(Stack<T> * stack) {
    if(stack->watermark == 0) {
        return 0;
    }

    return stack->storage + (stack->watermark - 1);
}

template<typename T>
intern
T * stack_push(Stack<T> * stack, s64 * opt_out_index = 0) {
    s64 num_new_bytes = (1 + stack->watermark) * sizeof(T);

    if(num_new_bytes > stack->mem.length) {

        s64 realloc_size = stack->mem.length + stack->starting_size;

        if(stack->log_realloc) {
            printf("stack: '%.*s' ran ouf of memory! Current size is: %d, new size is %d.\n",
                stack->name.length, stack->name.str, stack->mem.length, realloc_size
            );
        }

        auto new_mem = memory_allocator_reallocate(
            stack->allocator, stack->mem, realloc_size, "dynamic stack realloc"
        );
        stack->storage = (T*)new_mem.data;
        stack->mem = new_mem;
    }

    T * ret = stack->storage + stack->watermark;

    if(opt_out_index) {
        *opt_out_index = stack->watermark;
    }

    ++stack->watermark;

    return ret;
}

template<typename T>
intern
T * stack_pop(Stack<T> * stack) {
    if(stack->watermark <= 0) {
        return 0;
    }

    --stack->watermark;
    T * ret = stack->storage + stack->watermark;

    return ret;
}

template<typename T>
intern
T * stack_get_top(Stack<T> * stack) {
    if(stack->watermark <= 0) {
        return 0;
    }

    return stack->storage + stack->watermark - 1;
}

template<typename T>
intern force_inline
void stack_clear(Stack<T> * stack) {
    stack->watermark = 0;
}

intern force_inline
f32 asin_f32(f32 s) {
    return (f32)asinf((f64)s);
}

intern force_inline
f64 asin_f64(f64 s) {
    return asin(s);
}


intern force_inline
f32 acos_f32(f32 c) {
    return (f32)acosf((f64)c);
}

intern force_inline
f64 acos_f64(f64 c) {
    return (f64)acos(c);
}

// NOTE(justas): designed for overlapping dest & source ranges
intern
void move_bytes(
        u8 * destination, 
        const u8 * source, 
        s64 num_bytes
) {
    if(num_bytes <= 0) {
        return;
    }

    if(destination == source) {
        return;
    }

    // NOTE(justas): overlapping, i.e:
    // destination: .........[--------------321]..............
    //      source: ...............[--------------321]........              
    //
    // TODO(justas): @Speed
    if(destination < source && (destination + num_bytes) > source) {
        for(auto byte_index = (num_bytes - 1);
                byte_index >= 0;
                byte_index--
        ) {
            destination[byte_index] = source[byte_index];
        }
    }
    else {
        copy_bytes(destination, source, num_bytes);
    }
}

template<typename T>
struct Array {
    T * storage;

    Memory_Allocation page;
    Memory_Allocator * allocator;
    s64 num_starting_elements;
    String name;

    s64 watermark;

    b32 log_reallocation;
    b32 do_resizing;

    struct Iterator {
        s64 index;
        Array<T> * array;

        Iterator operator++() {
            this->index++;
            return *this;
        }

        b32 operator !=(const Iterator & rhs) const {
            return !(this->index == rhs.index && this->array == rhs.array);
        }

        T * operator *() {
            return this->array->storage + this->index;
        }
    };

    Iterator begin() {
        Iterator iter = {};
        iter.index = 0;
        iter.array = this;
        return iter;
    }

    Iterator end() {
        Iterator iter = {};
        iter.index = this->watermark;
        iter.array = this;
        return iter;
    }
};

template<typename T>
intern
void array_free(Array<T> * arr) {
    if(arr->storage == 0) {
        return;
    }

    if(allocation_equals(&arr->page, &null_page)) {
        return;
    }

    assert(arr->allocator);

    memory_allocator_free(arr->allocator, arr->page);

    arr->storage = 0;
    arr->page = null_page;
    arr->watermark = 0;
}

#define Foreach_Array(__array, __index) Foreach_Max((__array)->watermark, __index)

template<typename T>
intern force_inline
void array_clear(Array<T> * arr) {
    arr->watermark = 0;
}

template<typename T>
intern force_inline
Tokenizer<T> make_tokenizer(Array<T> * arr) {
    return make_tokenizer(arr->storage, arr->watermark);
}

template<typename T>
intern force_inline
Array<T> make_array(s64 num_starting_elements, 
                    Memory_Allocator * allocator, 
                    String name, 
                    b32 log_rellocation = false
) {
    Array<T> ret;

    ret.name = name;
    ret.allocator = allocator;

    if(num_starting_elements > 0) {
        ret.num_starting_elements = num_starting_elements;
        ret.page = memory_allocator_allocate(allocator, sizeof(T) * num_starting_elements, "make_array");
        ret.storage = (T*)ret.page.data;
    }
    else {
        ret.storage = 0;
        ret.page = null_page;
        ret.num_starting_elements = 32;
    }

    ret.do_resizing = true;
    ret.watermark = 0;
    ret.log_reallocation = log_rellocation;

    return ret;
}

template<typename T>
intern force_inline
Array<T> make_array_without_resizes(
        s64 num_starting_elements, 
        Memory_Allocator * allocator, 
        String name
        ) {
    auto arr = make_array<T>(num_starting_elements, allocator, name, false);
    arr.do_resizing = false;
    return arr;
}

template<typename T>
intern force_inline
Array<T> make_empty_unresizable_array(String name) {
    return make_array_without_resizes<T>(0, 0, name);
}

template<typename T>
intern force_inline
b32 array_is_last_index(Array<T> * arr, s64 index) {
    return arr->watermark <= index + 1;
}

template<typename T>
intern
void array_reserve(Array<T> * arr, s32 num_elements) {

    s64 new_required_size = (arr->watermark + num_elements) * sizeof(T);

    if(new_required_size > arr->page.length) {

        if(!arr->do_resizing) {
            assert(false, "unresizable array ran out of memory!");
            return;
        }

        s64 new_desired_size_in_bytes = arr->page.length + (sizeof(T) * arr->num_starting_elements) + (num_elements * sizeof(T));

        Memory_Allocation new_page;
        if(allocation_equals(&arr->page, &null_page)) {
            new_page = memory_allocator_allocate(arr->allocator, new_desired_size_in_bytes, "Array<T> rellocation (array initial size was 0 so this is the first allocation actually");
        }
        else {
            new_page = memory_allocator_reallocate(arr->allocator, arr->page, new_desired_size_in_bytes, "Array<T> reallocation");
        }
            
        if(arr->log_reallocation) {
            printf("array named '%.*s' needs reallocation (sizes are in bytes):\n", 
                arr->name.length, arr->name.str
            );

            printf("            old size: %d\n", arr->page.length);
            printf("    new desired size: %d\n", new_desired_size_in_bytes);
            printf("       new page size: %d\n", new_page.length);
        }

        arr->storage = (T*)new_page.data;
        arr->page = new_page;
    }
}

template<typename T>
intern 
T * array_append(Array<T> * arr, s64 * opt_out_index = 0) {

    array_reserve(arr, 1);

    T * ret = arr->storage + arr->watermark;

    if(opt_out_index) {
        *opt_out_index = arr->watermark;
    }

    ++(arr->watermark);

    return ret;
}

template<typename T>
intern 
T * array_insert_before(Array<T> * arr, s64 index) {

    array_reserve(arr, 1);

    T * slot = arr->storage + index;

    s64 num_bytes_to_move = (arr->watermark - index) * sizeof(T);

    if(num_bytes_to_move > 0) {
        u8 * move_start = (u8*)(arr->storage + index);
        u8 * move_destination = (u8*)(arr->storage + index + 1);

        move_bytes(move_destination, move_start, num_bytes_to_move);
    }

    ++(arr->watermark);

    return slot;
}

template<typename T>
intern 
void array_concat_generic(
        Array<T> * into, 
        s64 num_elements, 
        const u8 * copy_source,
        s64 element_size
) {
    array_reserve(into, num_elements);

    u8 * copy_destination = (u8*)(into->storage + into->watermark);
    s64 num_bytes = num_elements * element_size;

    copy_bytes(copy_destination, copy_source, num_bytes);

    into->watermark += num_elements;
}


template<typename T>
intern 
void array_concat(Array<T> * into, Array<T> * from) {
    array_concat_generic(into, from->watermark, (u8*)from->storage, sizeof(T));
}

intern
void array_concat(Array<char> * into, String from) {
    array_concat_generic(into, from.length, (u8*)from.str, sizeof(char));
}

template<typename T>
intern 
void array_remove(Array<T> * arr, s64 at_index) {
    if(0 > at_index || at_index >= arr->watermark) {
        return;
    }

    if(arr->watermark - 1 == at_index) {
        T * ret = arr->storage + arr->watermark - 1;
        --(arr->watermark);
        return;
    }

    s64 num_bytes_to_move = sizeof(T) * (arr->watermark - at_index - 1);

    if(num_bytes_to_move <= 0) {
        assert(false, "array_remove: num_bytes_to_move <= 0");
        return;
    }

    u8 * rebase_onto = (u8*)(arr->storage + at_index);
    u8 * rebase_from = (u8*)(arr->storage + at_index + 1);

    copy_bytes(rebase_onto, rebase_from, num_bytes_to_move);

    --(arr->watermark);
}


template<typename T>
intern force_inline
s64 array_get_size(Array<T> * arr) {
    return arr->watermark;
}

template<typename T>
intern force_inline
b32 array_is_index_in_bounds(Array<T> * arr, s64 index) {
    return !(0 > index || arr->watermark <= index);
}

template<typename T>
intern 
T * array_get_at_index_unchecked(Array<T> * arr, s64 index) {
    if(!array_is_index_in_bounds(arr, index)) {
        return 0;
    }

    return arr->storage + index;
}

template<typename T>
intern force_inline
T * array_get_with_bounds_check(Array<T> * arr, s64 index) {
    assert(array_is_index_in_bounds(arr, index), "out of bounds Array<T> access");

    return arr->storage + index;
}

template<typename T>
intern force_inline
b32 string_contains_any_of_case_sensitive(
        String target, 
        Array<T> * strings
) {
    return string_contains_any_of_case_generic(target, strings->storage, strings->watermark, true);
}

template<typename T>
intern force_inline
b32 string_contains_any_of_case_insensitive(
        String target, 
        Array<T> * strings
) {
    return string_contains_any_of_case_generic(target, strings->storage, strings->watermark, false);
}

intern force_inline
v2 just_x(v2 v) {
    v2 ret;

    ret.x = v.x;
    ret.y = 0;

    return ret;
}

intern force_inline
v2_f64 just_x(v2_f64 v) {
    v2_f64 ret;

    ret.x = v.x;
    ret.y = 0;

    return ret;
}

intern force_inline
v2 just_y(v2 v) {
    v2 ret;

    ret.x = 0;
    ret.y = v.y;

    return ret;
}

intern force_inline
v2_s32 just_x(v2_s32 v) {
    v2_s32 ret;

    ret.x = v.x;
    ret.y = 0;

    return ret;
}

intern force_inline
v2_s32 just_y(v2_s32 v) {
    v2_s32 ret;

    ret.x = 0;
    ret.y = v.y;

    return ret;
}

intern force_inline
v2_f64 just_y(v2_f64 v) {
    v2_f64 ret;

    ret.x = 0;
    ret.y = v.y;

    return ret;
}

template<typename T>
intern force_inline
b32 is_in_range_inclusive_exact(T min, T val, T max) {
    if(min > val) return false;
    if(val > max) return false;

    return true;
}

template<typename T>
intern force_inline
T * array_get_with_bounds_check(T * array, s64 array_length, s64 index) {
    assert(index >= 0);
    assert(array_length > index);

    return array + index;
}

intern force_inline
v2_f64 project_point_onto_line(v2_f64 * point, const v2_f64 * direction) {
    return *direction * dot_product(*point, *direction);
}

struct Raycast_Result_v3_f64 {
    v3_f64 hit;
    b32 did_hit;
};

intern
v3_f64 vec_project(v3_f64 vec, v3_f64 onto) {

    f64 v_length = vec_length(vec);
    if(v_length == 0) return zero_vector3_f64;

    v3_f64 vec_norm = vec / v_length;
    v3_f64 onto_norm = normalize(onto);

    return onto_norm * v_length * dot_product(vec_norm, onto_norm);
}

intern
v2_f64 vec_project(v2_f64 vec, v2_f64 onto) {

    f64 v_length = vec_length(vec);
    if(v_length == 0) return zero_vector_f64;

    v2_f64 vec_norm = vec / v_length;
    v2_f64 onto_norm = normalize(onto);

    return onto_norm * v_length * dot_product(vec_norm, onto_norm);
}

intern 
Raycast_Result_v3_f64 ray_versus_plane(
        v3_f64 ray_origin, 
        v3_f64 ray_direction, 
        v3_f64 plane_normal, 
        f64 plane_d
) {
    Raycast_Result_v3_f64 ret;

    f64 divisor = dot_product(plane_normal, ray_direction);
    if(absolute_f64(divisor) <= 0.000001) {
        ret.did_hit = false;
        return ret;
    }

    f64 dividend = plane_d - dot_product(plane_normal, ray_origin);

    f64 plane_hit_time = dividend / divisor;

    if(plane_hit_time < 0) {
        ret.did_hit = false;
        return ret;
    }

    ret.hit = ray_origin + ray_direction * plane_hit_time;
    ret.did_hit = true;
    return ret;
}

intern force_inline
b32 are_barycentric_coordinates_degenerate(v3_f64 barycentric) {

         if(!is_in_range_inclusive_exact(0.0, barycentric.x, 1.0)) return true; 
    else if(!is_in_range_inclusive_exact(0.0, barycentric.y, 1.0)) return true; 
    else if(!is_in_range_inclusive_exact(0.0, barycentric.z, 1.0)) return true; 

    return false;
}


intern
Raycast_Result_v3_f64 calculate_barycentric_coordinates(const v3_f64 * vertices, const u16 * indices, v3_f64 point) {
    Raycast_Result_v3_f64 ret;
    
    m3_f64 tri_matrix = make_matrix_f64(
        vertices[indices[0]],
        vertices[indices[1]],
        vertices[indices[2]]
    );

    b32 is_valid_inverse;
    m3_f64 inverse_of_tri_matrix= matrix_inverse(&tri_matrix, &is_valid_inverse);
    if(!is_valid_inverse) {
        ret.did_hit = false;
        return ret;
    }

    ret.did_hit = true;
    ret.hit = inverse_of_tri_matrix * point;

    return ret;
}

intern
Raycast_Result_v3_f64 ray_versus_triangle(v3_f64 ray_origin, v3_f64 ray_direction, v3_f64 * triangle_vertices, u16 * indices) {
    Raycast_Result_v3_f64 ret;

    // NOTE(justas): build plane out of triangle
    v3_f64 normal = normalize(cross_product(
        triangle_vertices[indices[0]] - triangle_vertices[indices[1]], 
        triangle_vertices[indices[0]] - triangle_vertices[indices[2]]
    ));

    f64 plane_d = dot_product(normal, triangle_vertices[indices[0]]);

    auto plane_raycast = ray_versus_plane(ray_origin, ray_direction, normal, plane_d);
    if(!plane_raycast.did_hit) {
        ret.did_hit = false;
        return ret;
    }

    auto barycentric_raycast = calculate_barycentric_coordinates(triangle_vertices, indices, plane_raycast.hit);
    if(!barycentric_raycast.did_hit) {
        ret.did_hit = false;
        return ret;
    }

    if(are_barycentric_coordinates_degenerate(barycentric_raycast.hit)) {
        ret.did_hit = false;
        return ret;
    }
   
    ret.did_hit = true;
    ret.hit = plane_raycast.hit;

    return ret;
}

// NOTE(justas): graphics gems vol 1, pg 304
intern
Raycast_Result_v3_f64 calculate_ray_intersection(v3_f64 first_origin, v3_f64 first_direction,
                                v3_f64 second_origin, v3_f64 second_direction) {

    Raycast_Result_v3_f64 ret;

    v3_f64 crossed_dirs = cross_product(first_direction, second_direction);
    f64 divisor = dot_product(crossed_dirs, crossed_dirs);

    if(divisor == 0) {
        ret.did_hit = false;
        return ret;
    }

    f64 det = dot_product(
            second_origin - first_origin, 
            cross_product(second_direction, crossed_dirs)
    );

    f64 time = det / divisor;

    ret.did_hit = true;
    ret.hit = first_origin + first_direction * time;

    return ret;
}

struct Raycast_Result_v2_f64 {
    v2_f64 hit;
    b32 did_hit;
};

intern
Raycast_Result_v2_f64 calculate_ray_intersection_time(v2_f64 first_start, 
                                                      v2_f64 first_end,
                                                      v2_f64 second_start, 
                                                      v2_f64 second_end
) {
    Raycast_Result_v2_f64 ret;

    v2_f64 a = first_end - first_start;
    v2_f64 b = second_end - second_start;
    v2_f64 c = second_start - first_start;

    m2_f64 mat = make_matrix_f64(a, b);
    b32 is_valid_inverse;
    m2_f64 inverse = matrix_inverse(&mat, &is_valid_inverse);

    if(!is_valid_inverse) {
        ret.did_hit = false;
        return ret;
    }

    v2_f64 time = inverse * c;

    // NOTE(justas): 
    //  |  tx | = inverse(| a b  |) * c
    //  | -ty |
    //  where tx is for a and ty is for b

    ret.did_hit = true;
    ret.hit = time;

    return ret;
}

intern
Raycast_Result_v2_f64 calculate_ray_intersection(v2_f64 first_start, 
                                                 v2_f64 first_end,
                                                 v2_f64 second_start, 
                                                 v2_f64 second_end
) {

    auto result = calculate_ray_intersection_time(first_start, first_end, second_start, second_end);
    if(!result.did_hit) {
        return result;
    }

    result.hit = vec_lerp(first_start, result.hit.x, first_end);

    return result;
}

intern
v2_f64 project_point_onto_line_segment(v2_f64 point, v2_f64 a, v2_f64 b) {
    v2_f64 ab = b - a;
    v2_f64 local_point = point - a;

    f64 ab_length = vec_length(ab);
    v2_f64 ab_unit = ab / ab_length;

    f64 point_in_a_b_segment = dot_product(local_point, ab_unit);

    if(0 > point_in_a_b_segment) {
        return a;
    }

    if(point_in_a_b_segment > ab_length) {
        return b;
    }

    return a + ab_unit * point_in_a_b_segment;
}

intern
v2_f64 raycast_against_line_soup(v2_f64 * ray, 
                                 v2_f64 * lines, 
                                 const v2_u16 * indices, 
                                 s32 num_lines
) {

    b32 is_smallest_initialized = false;
    f64 smallest_projected_distance_sq_so_far;
    v2_f64 hit;

    for(s32 line_index = 0;
            line_index < num_lines;
            line_index++) {

        const v2_u16 * vertex_indices = indices + line_index;
        v2_f64 * start = lines + vertex_indices->x;
        v2_f64 * end = lines + vertex_indices->y;

        v2_f64 projected = project_point_onto_line_segment(*ray, *start, *end);
        v2_f64 ray_to_projected = projected - *ray;
        f64 rtp_length_squared = dot_product(ray_to_projected, ray_to_projected);

        if(!is_smallest_initialized || smallest_projected_distance_sq_so_far > rtp_length_squared) {
            is_smallest_initialized = true;
            smallest_projected_distance_sq_so_far = rtp_length_squared;
            hit = projected;
        }
    }

    return hit;
}

// NOTE(justas): doesn't check if the point is in the tri, only fails if the vertices dont form a triangle
intern
Raycast_Result_v3_f64 calculate_barycentric_coordinates(const v2_f64 * vertices, const u16 * indices, v2_f64 point) {
    Raycast_Result_v3_f64 ret;

    m2_f64 matrix = make_matrix_f64(
        vertices[indices[0]] - vertices[indices[2]],
        vertices[indices[1]] - vertices[indices[2]]
    );

    b32 is_valid;
    m2_f64 inverse_matrix = matrix_inverse(&matrix, &is_valid);

    if(!is_valid) {
        ret.did_hit = false;
        return ret;
    }

    v2_f64 first_two = inverse_matrix * (point - vertices[indices[2]]);

    ret.did_hit = true;
    ret.hit = make_vector_f64(first_two.x, first_two.y, 1.0 - first_two.x - first_two.y);

    return ret;
}

intern force_inline
v4_f64 operator*(const v4_f64 & lhs, const f64 & rhs) {
    v4_f64 ret;

    ret.x = lhs.x * rhs;
    ret.y = lhs.y * rhs;
    ret.z = lhs.z * rhs;
    ret.w = lhs.w * rhs;

    return ret;
}

v4_f64 operator +(const v4_f64 & lhs, const v4_f64 & rhs) {
    v4_f64 ret;

    ret.x = lhs.x + rhs.x;
    ret.y = lhs.y + rhs.y;
    ret.z = lhs.z + rhs.z;
    ret.w = lhs.w + rhs.w;

    return ret;
}

intern force_inline
v4_f64 make_vector_f64(f64 x, f64 y, f64 z, f64 w) {
    v4_f64 ret;

    ret.x = x;
    ret.y = y;
    ret.z = z;
    ret.w = w;

    return ret;
}

struct hsva_f64 {
    f64 hue;
    f64 saturation;
    f64 value;
    f64 alpha;
};

intern force_inline
hsva_f64 make_hsva_f64(f64 hue, f64 saturation, f64 value, f64 alpha) {
    hsva_f64 ret;

    ret.hue = hue;
    ret.saturation = saturation;
    ret.value = value;
    ret.alpha = alpha;

    return ret;
}

// NOTE(justas): 
// https://en.wikipedia.org/wiki/HSL_and_HSV#Conversion_RGB_to_HSL/HSV_used_commonly_in_software_programming
intern
hsva_f64 rgba_f64_to_hsva_f64(v4_f64 col) {
    hsva_f64 ret;

    f64 hue = 0;

    f64 max = MAX(col.r, MAX(col.g, col.b));
    f64 min = MIN(col.r, MIN(col.g, col.b));

    f64 max_min_delta = max - min;
    if(max_min_delta != 0) {

        if(max == col.r) {
            hue = 60.0 * (col.g - col.b) / max_min_delta;
        }
        else if(max == col.g) {
            hue = 60.0 * (2.0 + (col.b - col.r) / max_min_delta);
        }
        else if(max == col.b) {
            hue = 60.0 * (4.0 + (col.r - col.g) / max_min_delta);
        }

        if(hue < 0) {
            hue += 360.0;
        }
    }
    else {
        hue = 0;
    }

    ret.hue = hue;
    ret.saturation = max != 0 ? max_min_delta / max : 0;
    ret.value = max;
    ret.alpha = col.a;
    return ret;
}

intern
v4_f64 hsva_f64_to_rgba_f64(hsva_f64 data) {
    v4_f64 col;

    f64 hue_slice = data.hue / 60.0;
    f64 c = data.value * data.saturation; // NOTE(justas): chroma
    f64 x = c * (1.0 - absolute_f64(fmod(hue_slice, 2.0) - 1.0));

         if(0 <= hue_slice && hue_slice <= 1) col = make_vector_f64(c, x, 0, data.alpha);
    else if(1 <  hue_slice && hue_slice <= 2) col = make_vector_f64(x, c, 0, data.alpha);
    else if(2 <  hue_slice && hue_slice <= 3) col = make_vector_f64(0, c, x, data.alpha);
    else if(3 <  hue_slice && hue_slice <= 4) col = make_vector_f64(0, x, c, data.alpha);
    else if(4 <  hue_slice && hue_slice <= 5) col = make_vector_f64(x, 0, c, data.alpha);
    else if(5 <  hue_slice && hue_slice <= 6) col = make_vector_f64(c, 0, x, data.alpha);

    col.xyz += data.value - c;

    return col;
}

intern
b32 is_memory_equal(void * a, s64 a_length, void * b, s64 b_length) {
    if(a_length != b_length) {
        return false;
    }
    if(a_length == 0) {
        return false;
    }

    for(s64 i = 0; i < a_length; i++) {
        if(((u8*)a)[i] != ((u8*)b)[i]) {
            return false;
        }
    }

    return true;
}

template<typename T>
intern force_inline
b32 is_memory_equal_generic(T * a, T * b) {
    return is_memory_equal(a, sizeof(*a), b, sizeof(*b));
}

intern
rect_f64 rect_reanchor_f64(rect_f64 r, v2_f64 anchor) {
    rect_f64 ret;

    ret.position = r.position - r.extents * anchor;
    ret.extents = r.extents;

    return ret;
}

template<typename... TArgs>
intern force_inline
Allocate_String_Result format_string(Memory_Allocator * allocator, s64 size, const char * format, TArgs... args) {
    auto page = memory_allocator_allocate(allocator, size, "format_temp_string");
    auto data = (char*)page.data;

    auto num_chars = snprintf(data, size, format, args...);

    Allocate_String_Result ret;
    ret.string = make_string(data, num_chars);
    ret.mem = page;
    return ret;
}

template<typename... TArgs>
intern force_inline
String format_temp_string(Memory_Allocator * allocator, s64 size, const char * format, TArgs... args) {
    return format_string(allocator, size, format, args...).string;
}

template<typename... TArgs>
intern force_inline
String format_temp_string(Memory_Allocator * allocator, const char * format, TArgs... args) {

    // NOTE(justas): we've got a bunch of bytes floating around so lets spare some
    s64 size = (sizeof...(TArgs) * 128) + string_length(format) * 2;
    return format_temp_string(allocator, size, format, args...);
}

template<typename... TArgs>
intern force_inline
String tprint(Memory_Allocator * allocator, const char * format, TArgs... args) {
    return format_temp_string(allocator, format, args...);
}

struct v4_u8 {
    union {
        struct {
            u8 x;
            u8 y;
            u8 z;
            u8 w;
        };

        struct {
            u8 r;
            u8 g;
            u8 b;
            u8 a;
        };

        u8 nth[4];
    };
};

intern force_inline
u8 rgba_f64_to_rgba_255_ceil(f64 f) {
    s32 val = (s32)ceil_f64(f * 255.0);
    clamp(&val, 0, 255);
    return val;
}

intern force_inline
v4_u8 rgba_f64_to_rgba_255_ceil(v4_f64 f) {
    v4_u8 ret;

    ret.x = rgba_f64_to_rgba_255_ceil(f.x);
    ret.y = rgba_f64_to_rgba_255_ceil(f.y);
    ret.z = rgba_f64_to_rgba_255_ceil(f.z);
    ret.w = rgba_f64_to_rgba_255_ceil(f.w);

    return ret;
}


intern force_inline
f64 rgba_255_to_rgba_f64(u8 v) {
    return (f64)v / 255.0;
}

intern force_inline
v4_f64 rgba_255_to_rgba_f64(v4_u8 v) {
    v4_f64 ret;

    ret.x = rgba_255_to_rgba_f64(v.x);
    ret.y = rgba_255_to_rgba_f64(v.y);
    ret.z = rgba_255_to_rgba_f64(v.z);
    ret.w = rgba_255_to_rgba_f64(v.w);

    return ret;
}

intern force_inline
v4_f64 make_color(u8 r, u8 g, u8 b, u8 a) {
    v4_f64 ret;

    ret.r = rgba_255_to_rgba_f64(r);
    ret.g = rgba_255_to_rgba_f64(g);
    ret.b = rgba_255_to_rgba_f64(b);
    ret.a = rgba_255_to_rgba_f64(a);

    return ret;
}

baked v4_f64 color_white = make_color(255,255,255,255);
baked v4_f64 color_black = make_color(0,0,0,255);
baked v4_f64 color_red = make_color(255,0,0,255);
baked v4_f64 color_green = make_color(0,255,0,255);
baked v4_f64 color_yellow = make_color(255,255,0,255);
baked v4_f64 color_orange = make_color(255, 128, 0, 255);
baked v4_f64 color_blue = make_color(0,0,255,255);
baked v4_f64 color_none = make_color(0, 0, 0, 0);
baked Sin_Cos_f64 sin_cos_f64_zero_deg = {0,1};

intern force_inline
v4_u8 make_vector_u8(u8 x, u8 y, u8 z, u8 w) {
    v4_u8 ret;

    ret.x = x;
    ret.y = y;
    ret.z = z;
    ret.w = w;

    return ret;
}

intern force_inline
b32 plat_fs_write_entire_file(String dir, 
                              const void * buf, 
                              s64 num_bytes, 
                              Memory_Allocator * temp_allocator
) {

    auto cstring_dir = copy_and_null_terminate_string(dir, temp_allocator, "plat_fs_write_entire_file cstring");
    b32 ret = plat_fs_write_entire_file(cstring_dir, buf, num_bytes);

    return ret;
}

intern force_inline
Read_File_Result plat_fs_read_entire_file(String dir, 
                                          Memory_Allocator * persistent_allocator, 
                                          Memory_Allocator * temp_allocator
) {

    auto cstring_dir = copy_and_null_terminate_string(dir, temp_allocator, "plat_fs_read_entire_file cstring");
    auto ret = plat_fs_read_entire_file(cstring_dir, persistent_allocator);
    return ret;
}

intern force_inline
v4_f64 mix_color(v4_f64 c, f64 alpha) {
    c.a *= alpha;
    return c;
}

intern force_inline
v4_f64 invert_color(v4_f64 c) {
    c.r = 1.0 - c.r;
    c.g = 1.0 - c.g;
    c.b = 1.0 - c.b;

    return c;
}

struct String_Builder {
    Memory_Allocator * allocator;
    Memory_Allocation mem;
    s64 starting_size;

    char * chars;
    s64 cursor_index;
};

intern force_inline
String_Builder make_string_builder(s64 starting_size, Memory_Allocator * allocator, const char * reason) {
    String_Builder ret;

    auto mem = memory_allocator_allocate(allocator, starting_size, reason);

    ret.allocator = allocator;
    ret.mem = mem;
    ret.starting_size = starting_size;
    ret.chars = (char*)mem.data;
    ret.cursor_index = 0;

    return ret;
}

intern force_inline
void string_builder_append(String string, String_Builder * builder) {
    s64 new_top = builder->cursor_index + string.length;
    
    if(new_top > builder->mem.length) {

        auto new_size = builder->mem.length + builder->starting_size + string.length;

        auto new_page = memory_allocator_reallocate(
            builder->allocator, builder->mem, new_size, "string_builder_append realloc"
        );

        builder->mem = new_page;
        builder->chars = (char*)new_page.data;
    }

    u8 * start = (u8*)(builder->chars + builder->cursor_index);
    copy_bytes(start, (u8*)string.str, string.length);

    builder->cursor_index = new_top;
}

intern force_inline
String string_builder_finish(String_Builder * builder) {
    return make_string(builder->chars, builder->cursor_index);
}

intern force_inline
void string_builder_append_bool(b32 val, String_Builder * builder) {
    if(val) {
        string_builder_append("true"_S, builder);
    }
    else {
        string_builder_append("false"_S, builder);
    }
}


intern force_inline
void string_builder_append(s64 number, String_Builder * builder) {
    if(number == 0) {
        string_builder_append("0"_S, builder);
        return;
    }

    auto sign = get_sign(number);
    number = absolute_s64(number);

    // NOTE(justas): we reverse the integer and print it backwards but it represents the "number" integer, which
    // isn't backwards. The backwardness cancels out basically.
    // TODO(justas): @BUG: might overflow if the number is too big
    s64 number_reversed = 0;
    s64 trailing_zeroes = 0;
    b32 increment_trailing_zeros = true;
    {
        s64 nth_power = log10_f64((f64)number);

        while(number != 0) {
            auto remainder = number % 10;
            number /= 10;

            if(increment_trailing_zeros && remainder == 0) {
                ++trailing_zeroes;
            }
            else {
                increment_trailing_zeros = false;
                number_reversed += remainder * pow_s64(10, nth_power);
            }
            --nth_power;
        }
    }
    {  
        if(sign == -1) {
            string_builder_append("-"_S, builder);
        }

        while(number_reversed != 0) {
            auto remainder = number_reversed % 10;
            number_reversed /= 10;

            char c = number_to_char_unchecked(remainder);
            string_builder_append(make_string(&c, 1), builder);
        }

        for(s32 i = 0; i < trailing_zeroes; i++) {
            string_builder_append("0"_S, builder);
        }
    }
}

intern force_inline
String allocate_temp_string(Memory_Allocator * allocator, s64 length, const char * reason) {
    String ret;

    auto page = memory_allocator_allocate(allocator, length, reason);
    ret.str = (char*)page.data;
    ret.length = length;

    return ret;
}

struct v4_s8 {
    union {
        struct {
            s8 x;
            s8 y;
            s8 z;
            s8 w;
        };

        s8 nth[4];
    };
};

struct v3_s8 {
    union {
        struct {
            s8 x;
            s8 y;
            s8 z;
        };

        s8 nth[3];
    };
};


intern force_inline
v4_s8 make_vector_s8(s8 x, s8 y, s8 z, s8 w) {
    v4_s8 ret;

    ret.x = x;
    ret.y = y;
    ret.z = z;
    ret.w = w;

    return ret;
}

intern force_inline
v3_s8 make_vector_s8(s8 x, s8 y, s8 z) {
    v3_s8 ret;

    ret.x = x;
    ret.y = y;
    ret.z = z;

    return ret;
}

v3_s8 operator -(const v3_s8 &lhs, const v3_s8 &rhs) {
    v3_s8 ret;

    ret.x = lhs.x - rhs.x;
    ret.y = lhs.y - rhs.y;
    ret.z = lhs.z - rhs.z;

    return ret;
}

intern force_inline
b32 solve_2nd_degree_polynomial(f64 a_term, f64 b_term, f64 c_term, f64 * out_values) {
    if(a_term == 0) {
        return false;
    }

    f64 discriminant = (b_term * b_term) - (a_term * c_term * 4.0);

    if(discriminant < 0) {
        return false;
    }

    f64 discriminant_sqrt = sqrt_f64(discriminant);
    f64 twice_a = a_term * 2.0;

    out_values[0] = (-b_term + discriminant_sqrt) / twice_a;
    out_values[1] = (-b_term - discriminant_sqrt) / twice_a;

    return out_values;
}

intern
Raycast_Result_v3_f64 raycast_against_sphere(v3_f64 ray_origin, 
                                            v3_f64 ray_direction,
                              v3_f64 sphere_origin,
                              f64 sphere_radius
) {
    Raycast_Result_v3_f64 ret;

    f64 times[2];

    v3_f64 local_origin = ray_origin - sphere_origin;

    f64 a_term = dot_product(ray_direction, ray_direction);
    f64 b_term = 2.0 * dot_product(ray_direction, local_origin);
    f64 c_term = dot_product(local_origin, local_origin) - sphere_radius * sphere_radius;

    b32 did_hit = solve_2nd_degree_polynomial(a_term, b_term, c_term, times);
    if(!did_hit) {
        ret.did_hit = false;
        return ret;
    }

    f64 time = MIN(times[0], times[1]);

    ret.did_hit = true;
    ret.hit = ray_origin + ray_direction * time;

    return ret;
}

template<typename T>
struct Spherical_Coordinate_Generic {
    T azimuth;
    T polar;
};

typedef Spherical_Coordinate_Generic<f64> Spherical_Coordinate_f64;

intern force_inline
Spherical_Coordinate_f64 make_Spherical_Coordinate_f64(f64 azimuth , f64 polar) {
    Spherical_Coordinate_f64 ret;
    ret.azimuth = azimuth;
    ret.polar = polar;

    return ret;
}

intern force_inline
Spherical_Coordinate_f64 make_spherical_coordinate_degrees_f64(f64 azimuth , f64 polar) {
    return make_Spherical_Coordinate_f64(azimuth * DEG_TO_RAD, polar * DEG_TO_RAD);
}

template<typename T>
intern force_inline
Spherical_Coordinate_Generic<T>
operator + (
    const Spherical_Coordinate_Generic<T> & lhs, 
    const Spherical_Coordinate_Generic<T> & rhs
) {
    Spherical_Coordinate_Generic<T> ret;

    ret.azimuth = lhs.azimuth+ rhs.azimuth;
    ret.polar = lhs.polar + rhs.polar;

    return ret;
}

intern force_inline
v3_f64 spherical_to_cartesian(f64 radius, Spherical_Coordinate_f64 sphere) {
    auto azimuth = sin_cos_f64(sphere.azimuth);
    auto polar = sin_cos_f64(sphere.polar);

    return make_vector_f64(
        polar.sin * azimuth.cos,
        polar.sin * azimuth.sin,
        polar.cos
    ) * radius;
}

intern force_inline
Spherical_Coordinate_f64 cartesian_to_spherical(f64 radius, v3_f64 pos) {
    Spherical_Coordinate_f64 ret;

    ret.azimuth = atan2_f64(pos.y, pos.x);
    ret.polar = acos_f64(pos.z / radius);

    return ret;
}

template<typename T>
struct Double_Buffer {
    T first;
    T second;
    b32 is_first;
};

template<typename T>
intern force_inline
Double_Buffer<T> make_double_buffer(T first, T second) {
    Double_Buffer<T> ret;

    ret.first = first;
    ret.second = second;

    ret.is_first = true;

    return ret;
}

template<typename T>
intern force_inline
T * double_buffer_get_current(Double_Buffer<T> * db) {
    if(db->is_first) {
        return &db->first;
    }
    return &db->second;
}

template<typename T>
intern force_inline
T * double_buffer_get_other(Double_Buffer<T> * db) {
    if(db->is_first) {
        return &db->second;
    }
    return &db->first;
}

template<typename T>
intern force_inline
T * double_buffer_swap_and_return_current(Double_Buffer<T> * db) {
    db->is_first = !db->is_first;
    return double_buffer_get_current(db);
}

intern
void convert_color_float_to_255(void * in_void, void * out_void) {
    auto * in = (v4_f64*)in_void;
    *((v4_u8*)out_void) = rgba_f64_to_rgba_255_ceil(*in);
}

intern
void convert_color_255_to_float(void * in_void, void * out_void) {
    auto * in = (v4_u8*)in_void;
    *((v4_f64*)out_void) = rgba_255_to_rgba_f64(*in);
}

intern
void convert_v2_s32_to_v2_f64(void * in_void, void * out_void) {
    auto * in = (v2_s32*)in_void;
    *((v2_f64*)out_void) = make_vector_f64(in->x, in->y);
}

intern
void convert_v2_f64_to_v2_s32(void * in_void, void * out_void) {
    auto * in = (v2_f64*)in_void;
    *((v2_s32*)out_void) = make_vector_s32(in->x, in->y);
}

intern
f64 find_min(f64 * vals, s64 num_vals, s64 * opt_out_index) {
    b32 is_init = false;
    f64 min_value = 0;
    s32 min_index = 0;

    ForRange(index, 0, num_vals) {
        auto val = vals[index];

        if(!is_init || min_value > val) {
            is_init = true;
            min_value = val;
            min_index = index;
        }
    }

    if(opt_out_index) {
       *opt_out_index = min_index;
    }

    return min_value;
}

template<typename T>
struct Optional {
    b32 has_value;
    T value;
};

template<typename T>
intern force_inline
Optional<T> opt_some(T val) {
    Optional<T> ret;
    ret.has_value = true;
    ret.value = val;
    return ret;
}


template<typename T>
Optional<T> opt_none() {
    Optional<T> ret;
    ret.has_value = false;
    return ret;
}

baked u64 TABLE_ENTRY_PROTECTION_COOKIE = 0x1234567812345678;

template<typename T>
struct Table_Entry {
    T value;

    u64 protection_cookie_top;
    b32 is_occupied;
    u64 hash;
    u64 protection_cookie_bottom;
};

template<typename T>
intern force_inline
void table_entry_verify_cookie(Table_Entry<T> * entry) {
#if defined(DEVELOPER)
    assert(entry->protection_cookie_top == TABLE_ENTRY_PROTECTION_COOKIE);
    assert(entry->protection_cookie_bottom == TABLE_ENTRY_PROTECTION_COOKIE);
#endif
}

template<typename T>
struct Table {
    Table_Entry<T> * storage;
    
    Memory_Allocation page;
    Memory_Allocator * allocator;
    s64 num_starting_elements;
    s64 max_storage_elements;
    String name;

    s64 watermark;

    struct Iterator {
        s64 index;
        Table<T> * table;

        void find_next_slot() {
            while(true) {
                if(table->max_storage_elements <= this->index) {
                    break;
                }

                auto * slot = table->storage + this->index;
                if(slot->is_occupied) {
                    break;
                }
                this->index++;
            }
        }

        Iterator operator ++() {
            this->index++;
            this->find_next_slot();
            return *this;
        }

        b32 operator !=(const Iterator & rhs) const {
            return !(this->index == rhs.index && this->table == rhs.table);
        }

        Table_Entry<T> * operator *() {
            return this->table->storage + index;
        }
    };

    Iterator begin() {
        Iterator iter = {};
        iter.index = 0;
        iter.table = this;
        iter.find_next_slot();
        return iter;
    }

    Iterator end() {
        Iterator iter = {};
        iter.index = this->max_storage_elements;
        iter.table = this;
        return iter;
    }
};

template<typename T>
intern force_inline
void table_free(Table<T> * table) {

    if(!allocation_equals(&table->page, &null_page)) {
        memory_allocator_free(table->allocator, table->page);
    }

    table->storage = 0;
    table->page = null_page;
    table->max_storage_elements = 0;
}

template<typename T>
intern force_inline
s64 table_calc_max_elements(Table<T> * table, Memory_Allocation page) {
    return (s64)(page.length / sizeof(*table->storage));
}

template<typename T>
intern force_inline
Table<T> make_table(
        s64 num_starting_elements, 
        Memory_Allocator * allocator,
        String name
) {
    Table<T> ret;

    assert(num_starting_elements > 0);

    ret.num_starting_elements = num_starting_elements;
    ret.name = name;
    ret.allocator = allocator;
    ret.watermark = 0;

    auto page = memory_allocator_allocate(allocator, sizeof(*ret.storage) * num_starting_elements, "make_table");
    set_bytes((u8*)page.data, 0, page.length);
    
    ret.page = page;
    ret.storage = (Table_Entry<T>*)page.data;
    ret.max_storage_elements = table_calc_max_elements(&ret, page);

    return ret;
}

template<typename T>
intern
T * table_insert_or_get_without_resizing(
        Table_Entry<T> * storage, 
        s64 max_elements, 
        u64 hash, 
        b32 * did_insert
) {
    // NOTE(justas): @COPY-PASTE :TableStorageIterator
    auto index = hash % max_elements;

    T * ret = 0;

    while(true) {
        auto * entry = storage + index;

        if(entry->is_occupied) {
            table_entry_verify_cookie(entry);

            if(entry->hash == hash) {
                table_entry_verify_cookie(entry);

                ret = &entry->value;
                *did_insert = false;
                break;
            }

            index = (index + 1) % max_elements;
        }
        else {
            entry->is_occupied = true;
            entry->hash = hash;
            entry->protection_cookie_top = TABLE_ENTRY_PROTECTION_COOKIE;
            entry->protection_cookie_bottom = TABLE_ENTRY_PROTECTION_COOKIE;
            ret = &entry->value;
            *did_insert = true;

            break;
        }
    }

    return ret;
}

template<typename T>
intern
T * table_insert(
        Table<T> * table, 
        u64 hash,
        b32 * opt_out_did_insert = 0
) {
    if(table->watermark >= table->max_storage_elements) {
        auto new_page_size = table->num_starting_elements * sizeof(*table->storage) + table->page.length;

        auto new_page = memory_allocator_allocate(table->allocator, new_page_size, "table_insert");
        set_bytes((u8*)new_page.data, 0, new_page.length);
        auto new_max_elements = table_calc_max_elements(table, new_page);

        auto new_storage = (Table_Entry<T>*)new_page.data;

        for(s64 entry_index = 0;
                entry_index < table->max_storage_elements;
                entry_index++) {

            auto * old_entry = table->storage + entry_index;

            if(!old_entry->is_occupied) {
                continue;
            }

            table_entry_verify_cookie(old_entry);

            b32 did_insert = false;
            *table_insert_or_get_without_resizing(new_storage, new_max_elements, old_entry->hash, &did_insert) = old_entry->value;
            assert(did_insert);
        }

        memory_allocator_free(table->allocator, table->page);

        table->page = new_page;
        table->storage = new_storage;
        table->max_storage_elements = new_max_elements;
    }

    b32 did_insert;
    T * ret = table_insert_or_get_without_resizing(table->storage, table->max_storage_elements, hash, &did_insert);

    if(did_insert) {
        if(opt_out_did_insert) {
            *opt_out_did_insert = true;
        }
        table->watermark++;
    }

    return ret;
}

template<typename T>
intern
T * table_walk_storage(Table<T> * table, u64 hash, b32 should_remove) {
    // NOTE(justas): @COPY-PASTE :TableStorageIterator
    auto index = hash % table->max_storage_elements;
    auto starting_index = index;

    T * ret = 0;

    while(true) {
        auto * entry = table->storage + index;

        if(entry->is_occupied && entry->hash == hash) {
            table_entry_verify_cookie(entry);
            if(should_remove) {
                entry->is_occupied = false;
                table->watermark--;
            }
            ret = &entry->value;
            break;
        }
        else {
            index = (index + 1) % table->max_storage_elements;

            if(index == starting_index) {
                break;
            }
        }
    }

    return ret;
}

template<typename T>
intern 
T * table_remove(Table<T> * data, u64 hash) {
    return table_walk_storage(data, hash, true);
}

template<typename T>
intern 
T * table_get(Table<T> * data, u64 hash) {
    return table_walk_storage(data, hash, false);
}

template<typename T>
intern 
T * table_insert(Table<T> * table, String key, b32 * opt_out_did_insert = 0) {
    return table_insert(table, hash_fnv(key), opt_out_did_insert);
}

template<typename T>
intern 
T * table_remove(Table<T> * data, String key) {
    return table_walk_storage(data, hash_fnv(key), true);
}

template<typename T>
intern 
T * table_get(Table<T> * data, String key) {
    return table_walk_storage(data, hash_fnv(key), false);
}

intern force_inline
rect_f64 _r(v2_f64 pos, v2_f64 ext) {
    return make_rect_f64(pos, ext);
}

intern force_inline
rect_f64 _r(f64 x, f64 y, f64 ext_x, f64 ext_y) {
    return make_rect_f64(x,y,ext_x,ext_y);
}

intern force_inline
v2_f64 _v2(f64 x, f64 y) {
    return make_vector_f64(x,y);
}


intern force_inline
f64 smoothstep(f64 x) {
    return x * x * (3.0 - 2.0 * x);
}

intern
v2_f64 slerp_f64(v2_f64 dir_a, f64 time, v2_f64 dir_b) {
    auto angle = acos_f64(dot_product(dir_a, dir_b));
    auto orthonormalized_dir_b = dir_b - (dir_a * dot_product(dir_a, dir_b));

    auto trig = sin_cos_f64(time * angle);
    return (dir_a * trig.cos) + (orthonormalized_dir_b * trig.sin);
}

intern force_inline
v2_f64 evaluate_quadratic_bezier(
        v2_f64 * p,
        f64 t
) {
    auto inverse_t = (1.0 - t);
    return p[1] + ((p[0] - p[1]) * inverse_t * inverse_t) + ((p[2] - p[1]) * t * t);
}

intern force_inline
v2_f64 evaluate_quadratic_bezier_tangent(
        v2_f64 * p,
        f64 t
) {
    return (p[1] - p[0]) * 2.0 * (1.0 - t) + (p[2] - p[1]) * 2.0 * t;
}

intern force_inline
v2_f64 evaulate_the_parallel_curve_of_a_quadratic_bezier(
        v2_f64 * p,
        f64 t,
        f64 parallel_curve_distance
) {
    auto bezier_point = evaluate_quadratic_bezier(p, t);

    if(parallel_curve_distance != 0) {
        auto tangent = evaluate_quadratic_bezier_tangent(p, t);
        auto normal = v2_rotate_90(normalize(tangent));
        return bezier_point + (normal * parallel_curve_distance);
    }

    return bezier_point;
}

intern force_inline
v2_f64 evaluate_cubic_bezier(
        v2_f64 * p,
        f64 t
) {
    auto inverse_t = (1.0 - t);
    return (evaluate_quadratic_bezier(p, t) * inverse_t) + (evaluate_quadratic_bezier(p + 1, t) * t);
}

intern force_inline
v2_f64 evaluate_cubic_bezier_tangent(
        v2_f64 * p,
        f64 t
) {
    auto inverse_t = (1.0 - t);
    return ((p[1] - p[0]) * 3 * inverse_t * inverse_t) + ((p[2] - p[1]) * 6 * t * inverse_t) + ((p[3] - p[2]) * 3 * t * t);
}

intern force_inline
v2_f64 evaulate_the_parallel_curve_of_a_cubic_bezier(
        v2_f64 * p,
        f64 t,
        f64 parallel_curve_distance
) {
    auto bezier_point = evaluate_cubic_bezier(p, t);

    if(parallel_curve_distance != 0) {
        auto tangent = evaluate_cubic_bezier_tangent(p, t);
        auto normal = v2_rotate_90(normalize(tangent));
        return bezier_point + (normal * parallel_curve_distance);
    }

    return bezier_point;
}


intern force_inline
f64 matrix_apply_m2_f64_transforms_to_1d(m2_f64 matrix, f64 val) {
    return (matrix * make_vector_f64(val, 0)).x;
}

struct Unicode_Interpret_Result {
    u32 code_point;
    s8 num_bytes;
};

intern
Unicode_Interpret_Result text_interpret_this_char_as_unicode(String text, s64 char_index) {
    Unicode_Interpret_Result ret;

    u8 first_byte = (u8)text.str[char_index];
    auto first_header = first_byte & 0b11111000;

    b32 has_second_byte = false;
    b32 has_third_byte = false;
    b32 has_fourth_byte = false;

    u8 second_byte;
    u8 third_byte;
    u8 fourth_byte;
    
    if(text.length > char_index + 1) {
        second_byte = (u8)text.str[char_index + 1];
        has_second_byte = true;
    }

    if(text.length > char_index + 2) {
        third_byte = (u8)text.str[char_index + 2];
        has_third_byte = true;
    }

    if(text.length > char_index + 3) {
        fourth_byte = (u8)text.str[char_index + 3];
        has_fourth_byte = true;
    }

    if(((first_byte & 0b11111000) == 0b11110000) && has_second_byte && has_third_byte && has_fourth_byte) {
        ret.num_bytes = 4;
        ret.code_point = (u32)(((first_byte & 0b00000111) << 18) | ((second_byte & 0b00111111) << 12) | ((third_byte & 0b00111111) << 6) | (third_byte & 0b00111111));
    }
    else if(((first_byte & 0b11110000) == 0b11100000) && has_second_byte && has_third_byte) {
        ret.num_bytes = 3;
        ret.code_point = (u32)(((first_byte & 0b00001111) << 12) | ((second_byte & 0b00111111) << 6) | (third_byte & 0b00111111));
    }
    else if(((first_byte & 0b11100000) == 0b11000000) && has_second_byte)  {
        ret.num_bytes = 2;
        ret.code_point = (u32)(((first_byte & 0b0001111) << 6) | (second_byte & 0b00111111));
    }
    else {
        ret.num_bytes = 1;
        ret.code_point = (u32)(first_byte  & 0b01111111);
    }
    return ret;
}

template<typename ScoreType, typename T, typename ScoreFx, typename ScoreCompareFx>
intern
s64 find_element_with_best_score_in_array(
        T * array,
        s64 num_elements,
        ScoreFx & score_fx,
        ScoreCompareFx & compare_fx
) {
    ScoreType best_score = {};
    auto has_score = false;
    s64 best_index = 0;

    ForRange(index, 0, num_elements) {
        ScoreType score = score_fx(array, index);

        if(!has_score || compare_fx(score, best_score)) {
            has_score = true;
            best_index = index;
            best_score = score;
        }
    }

    return best_index;
}

struct Aspect_And_Conversion_Data {
    v2_f64 aspect_ratio;
    f64 pixel_to_screen;
};

intern
Aspect_And_Conversion_Data calculate_aspect_and_pixel_conversion_data(v2_f64 screen_size) {
    Aspect_And_Conversion_Data ret = {};

    if(screen_size.x > screen_size.y) {
        // x/y = 1/a
        // a = y/x
        ret.aspect_ratio = make_vector_f64(1, screen_size.y / screen_size.x);
    }
    else {
        // x/y = a/1
        // a = x/y
        ret.aspect_ratio = make_vector_f64(screen_size.x / screen_size.y, 1);
    }
    ret.pixel_to_screen = 1.0 / MAX(screen_size.x, screen_size.y);

    return ret;
};

template<typename T, typename IsEqualFx>
intern
b32 array_elements_equal(
        T * arr_1,
        s64 arr_1_size,
        T * arr_2,
        s64 arr_2_size,
        IsEqualFx & is_equal_fx
) {
    if(arr_1_size != arr_2_size) {
        return false;
    }

    ForRange(arr_1_index, 0, arr_1_size) {
        ForRange(arr_2_index, 0, arr_2_size) {
            if(!is_equal_fx(arr_1 + arr_1_index, arr_2 + arr_2_index)) {
                return false;
            }
        }
    }

    return true;
}

struct Memory_Allocator_Arena {
    void * storage;
    s64 top;
    Memory_Allocation page;
    Memory_Allocation last_allocation;
};

struct Memory_Allocator_Tracked {
    Table<Memory_Allocation> allocations;
    Memory_Allocator * allocator;
};

enum MEMORY_ALLOCATOR_TYPE_ {
    MEMORY_ALLOCATOR_TYPE_ARENA,
    MEMORY_ALLOCATOR_TYPE_PAGE,
    MEMORY_ALLOCATOR_TYPE_TRACKED,
    MEMORY_ALLOCATOR_TYPE_MALLOC,
};

struct Memory_Allocator {
    MEMORY_ALLOCATOR_TYPE_ type;

    union {
        Memory_Allocator_Arena arena;
        Memory_Allocator_Tracked tracked;
    };
};

intern force_inline
Memory_Allocator make_tracked_memory_allocator(Memory_Allocator * allocator) {
    Memory_Allocator ret;

    ret.type = MEMORY_ALLOCATOR_TYPE_TRACKED;
    ret.tracked.allocations = make_table<Memory_Allocation>(16, allocator, "tracked alloc storage"_S);
    ret.tracked.allocator = allocator;

    return ret;
}

intern force_inline
void tracked_memory_allocator_clear_all_allocations(Memory_Allocator * allocator) {
    assert(allocator->type == MEMORY_ALLOCATOR_TYPE_TRACKED);

    For(allocator->tracked.allocations) {
        memory_allocator_free(allocator->tracked.allocator, it->value);
    }
}

intern force_inline
void free_tracked_memory_allocator(Memory_Allocator * allocator) {
    tracked_memory_allocator_clear_all_allocations(allocator);
    table_free(&allocator->tracked.allocations);
}

intern force_inline
Memory_Allocator make_page_memory_allocator() {
    Memory_Allocator ret;

    ret.type = MEMORY_ALLOCATOR_TYPE_PAGE;

    return ret;
}

intern force_inline
Memory_Allocator make_malloc_memory_allocator() {
    Memory_Allocator ret;

    ret.type = MEMORY_ALLOCATOR_TYPE_MALLOC;

    return ret;
}


intern force_inline
void memory_allocator_arena_reset(Memory_Allocator * allocator) {
    assert(allocator->type == MEMORY_ALLOCATOR_TYPE_ARENA);

    allocator->arena.top = 0;
    allocator->arena.last_allocation = null_page;
}

intern force_inline
Memory_Allocator make_arena_memory_allocator(Memory_Allocation page) {
    Memory_Allocator ret;

    ret.type = MEMORY_ALLOCATOR_TYPE_ARENA;
    ret.arena.top = 0;
    ret.arena.page = page;
    ret.arena.storage = ret.arena.page.data;
    ret.arena.last_allocation = null_page;

    return ret;
}

intern force_inline
Memory_Allocator make_arena_memory_allocator_dynamically_allocated(s64 size) {
    return make_arena_memory_allocator(plat_mem_allocate(size));
}

intern
Memory_Allocation memory_allocator_allocate(
        Memory_Allocator * generic_allocator, 
        s64 num_bytes, 
        const char * reason
) {

    switch(generic_allocator->type) {
        case MEMORY_ALLOCATOR_TYPE_MALLOC: {
            Memory_Allocation page = {};
            page.length = num_bytes;
            page.data = malloc(num_bytes);
            return page;
        }
        case MEMORY_ALLOCATOR_TYPE_TRACKED: {
            auto * alloc = &generic_allocator->tracked;
            auto page = memory_allocator_allocate(alloc->allocator, num_bytes, reason);

            *table_insert(&alloc->allocations, (u64)page.data) = page;

            return page;
        }
        case MEMORY_ALLOCATOR_TYPE_ARENA: {
            auto * allocator = &generic_allocator->arena;
            s64 new_top = allocator->top + num_bytes;

            if(new_top > allocator->page.length) {
                printf("arena allocator ran out of memory. allocation reason: '%s'\n", reason);
                assert(false);
            }

            Memory_Allocation ret;

            ret.length = num_bytes;
            ret.data = (u8*)allocator->storage + allocator->top;

            allocator->top = new_top;
            allocator->last_allocation = ret;

            return ret;
        }
        case MEMORY_ALLOCATOR_TYPE_PAGE: {
            return plat_mem_allocate(num_bytes);
        }
        default: {
            assert(false, "allocate: unknown allocator type");
        }
    }

    return null_page;
}

intern
void memory_allocator_free(
        Memory_Allocator * generic_allocator, 
        Memory_Allocation allocation
) {
    if(allocation_equals(&allocation, &null_page)) {
        return;
    }

    switch(generic_allocator->type) {
        case MEMORY_ALLOCATOR_TYPE_MALLOC: {
            free(allocation.data);
            break;
        }
        case MEMORY_ALLOCATOR_TYPE_TRACKED: {

            auto * alloc = &generic_allocator->tracked;

            auto key = (u64)allocation.data;

            if(!table_get(&alloc->allocations, key)) {
                break;
            }

            table_remove(&alloc->allocations, key);

            memory_allocator_free(alloc->allocator, allocation);

            break;
        }
        case MEMORY_ALLOCATOR_TYPE_ARENA: {
            auto * allocator = &generic_allocator->arena;

            if(allocation_equals(&allocator->last_allocation, &allocation)) {
                allocator->top -= allocation.length;
                allocator->last_allocation = null_page;
            }
            break;
        }
        case MEMORY_ALLOCATOR_TYPE_PAGE: {
            plat_mem_free(allocation);
            break;
        }
        default: {
            assert(false, "free: unknown allocator type");
        }
    }
}

intern
Memory_Allocation memory_allocator_reallocate(
        Memory_Allocator * void_allocator, 
        Memory_Allocation allocation, 
        s64 new_size, 
        const char* reason
) {
    if(allocation.length >= new_size) {
        return allocation;
    }

    auto new_allocation = memory_allocator_allocate(void_allocator, new_size, reason);
    if(allocation.data == 0) {
        return new_allocation;
    }

    copy_bytes((u8*)new_allocation.data, (u8*)allocation.data, allocation.length);

    memory_allocator_free(void_allocator, allocation);

    return new_allocation;
}

intern
String string_trim_in_place(String str) {
    String ret = str;

    ForRange(index, 0, str.length) {
        if(char_is_space(str.str[index])) {
            ret.str++;
            ret.length--;
        }
        else {
            break;
        }
    }

    for(auto index = ret.length - 1; index >= 0; index--) {
        if(char_is_space(ret.str[index])) {
            ret.length--;
        } else {
            break;
        }
    }
    return ret;
}

#if defined(IS_LINUX) 
    #include <time.h>

    typedef timespec Plat_High_Frequency_Time;

    intern force_inline
    timespec plat_get_high_frequency_time() {
        timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        return t;
    }

    intern force_inline
    f64 plat_get_time_delta_in_seconds(timespec a, timespec b) {

        auto seconds = a.tv_sec - b.tv_sec;
        auto nanoseconds = 0;

        // NOTE(justas): avoids overflow issues when we increment a second
        if(b.tv_nsec > a.tv_nsec) {
            baked u64 billion = 1000000000; // billion nanoseconds = 1 second
            u64 needed_till_billion = billion - b.tv_nsec;

            nanoseconds = a.tv_nsec + needed_till_billion;

            --seconds;
        }
        else {
            nanoseconds = a.tv_nsec - b.tv_nsec;

        }

        return (f64)seconds + (nanoseconds * 0.000000001);
    }


    intern
    void open_process_or_url(
            String path,
            Memory_Allocator * temp_alloc
    ) {
        auto builder = make_string_builder(path.length + 16, temp_alloc, "open_process_or_url builder");
        string_builder_append("xdg-open "_S, &builder);
        string_builder_append(path, &builder);
        string_builder_append("\0"_S, &builder);
        auto arg = string_builder_finish(&builder);

        system(arg.str);
    }

#elif defined(IS_WINDOWS)
    intern
    void open_process_or_url(
            String path,
            Memory_Allocator * temp_alloc
    ) {
        auto builder = make_string_builder(path.length + 16, temp_alloc, "open_process_or_url builder");
        string_builder_append("open"_S, &builder);
        string_builder_append(path, &builder);
        string_builder_append("\0"_S, &builder);
        auto arg = string_builder_finish(&builder);

        system(arg.str);
    }

#endif

intern
f64 polygon_area(
        v2_f64 * polygon, 
        s64 num_vertices
) {
    f64 accum = 0;
    ForRange(index, 0, num_vertices) {
        auto start = polygon[index];
        auto end = polygon[(index + 1) % num_vertices];
        
        m2_f64 mat;
        mat.a = start;
        mat.b = end;

        b32 is_valid;
        accum += matrix_determinant(&mat, &is_valid);
    }
    return absolute_f64(accum * .5);
}

#include <iterator>

template<typename T, s64 sz>
struct Static_Array {
    T storage[sz];

    force_inline constexpr
    T & operator[](s64 index) {
        return storage[index];
    }

    intern constexpr force_inline 
    s64 size() {
        return sz;
    }
    
    struct Iter {
        T * storage;
        s64 index;

        Iter operator++() {
            index++;
            return *this;
        }

        b32 operator!=(const Iter & other) const {
            return !(other.storage == storage && other.index == index);
        }

        T * operator *() {
            return storage + index;
        }
    };

    force_inline constexpr
    Iter begin() {
        Iter iter = {};
        iter.storage = storage;
        iter.index = 0;
        return iter;
    }

    force_inline constexpr
    Iter end() {
        Iter iter = {};
        iter.storage = storage;
        iter.index = sz;
        return iter;
    }

    force_inline constexpr
    T * operator +(s64 i) {
        return storage + i;
    }
};

intern force_inline
b32 is_infinity(f64 v) {
    return isinf(v);
}

intern force_inline
b32 is_nan(f64 v) {
    return isnan(v);
}

intern force_inline
String path_get_directory(String filename) {
    auto idx = string_last_index_of(filename, PLAT_SEPARATOR);
    if(idx == -1) {
        return filename;
    }

    filename.length = idx + 1;
    return filename;
}

intern force_inline
String path_get_filename(String filename) {
    auto idx = string_last_index_of(filename, PLAT_SEPARATOR);
    if(idx == -1) {
        return filename;
    }

    auto stride = idx + 1;
    filename.str += stride;
    filename.length -= stride;
    return filename;
}

template<typename Type, template<typename> typename Container, typename PredicateFx>
auto * container_first(
        Container<Type> * container,
        PredicateFx && predicate
) {
    for(auto it = container->begin(); it != container->end(); ++it) {
        auto * val = *it;
        if(predicate(val)) {
            return val;
        }
    }
    return (Type*)0;
}

intern force_inline
v2_f64 make_vector_f64(v2_u32 v) {
    v2_f64 ret;

    ret.x = v.x;
    ret.y = v.y;

    return ret;
}

#if defined (TESTING)

intern Memory_Allocator global_test_allocator = make_page_memory_allocator();
intern Memory_Allocator global_test_temp_allocator = make_arena_memory_allocator(
    memory_allocator_allocate(&global_test_allocator, MEGABYTES(2), "test temp allocator")
);

#define TEST(name) \
    intern void name##_test(); \
\
    struct name##_struct { \
        name##_struct() { \
            printf("running: '%s'\n", #name); \
            name##_test(); \
            memory_allocator_arena_reset(&global_test_temp_allocator); \
        } \
    } \
\
    intern name##_global = {}; \
    intern void name##_test()

TEST(table) {
    auto table = make_table<s32>(4, &global_test_allocator, "test"_S);
    assert(table_get(&table, "one"_S) == 0);
    assert(table_get(&table, "two"_S) == 0);

    *table_insert(&table, "one"_S) = 1;

    assert(*table_get(&table, "one"_S) == 1);
    assert(table_get(&table, "two"_S) == 0);

    s32 iters = 2048;
    for(s32 i = 0; i < iters; i++) {
        auto str = format_string(&global_test_allocator, 32, "%d", i);
        defer { memory_allocator_free(&global_test_allocator, str.mem); };

        assert(table_get(&table, str.string) == 0);
        *table_insert(&table, str.string) = i;
        assert(*table_get(&table, str.string) == i);

    }

    for(s32 i = 0; i < iters; i++) {
        auto str = format_string(&global_test_allocator, 32, "%d", i);
        defer { memory_allocator_free(&global_test_allocator, str.mem); };

        assert(*table_get(&table, str.string) == i);
    }
}

intern force_inline
void test_string_builder_append_integer(s64 in, String wants) {
    auto builder = make_string_builder(32, &global_test_allocator, "test");

    string_builder_append(in, &builder);
    auto got = string_builder_finish(&builder);

    assert(string_equals_case_sensitive(got, wants));

    memory_allocator_free(builder.allocator, builder.mem);
}

TEST(string_builder_append_integer) {
    test_string_builder_append_integer(123, "123"_S);
    test_string_builder_append_integer(1, "1"_S);
    test_string_builder_append_integer(12, "12"_S);
    test_string_builder_append_integer(120, "120"_S);
    test_string_builder_append_integer(1020, "1020"_S);
    test_string_builder_append_integer(-1020, "-1020"_S);
    test_string_builder_append_integer(+1020, "1020"_S);
    test_string_builder_append_integer(1020001, "1020001"_S);
    test_string_builder_append_integer(102000, "102000"_S);
}

TEST(string_builder_small_strings) {
    auto b = make_string_builder(4, &global_test_allocator, "a");

    string_builder_append("longish string"_S, &b);
    string_builder_append("a"_S, &b);
    string_builder_append("b"_S, &b);

    auto done = string_builder_finish(&b);
    assert(string_equals_case_sensitive(done, "longish stringab"_S));
}

intern force_inline
void test_string_parse_s64(const char * c, b32 expect_b, s64 expect) {
    s64 out_val = 0;
    b32 success = string_parse_s64(make_string(c), &out_val, &global_test_allocator);

    if(expect_b != success) {
        printf("(retval) expected %d got %d: %s %lld\n", expect_b, success, c, expect);
        assert(false);
    }

    if(success) {
        if(out_val != expect) {
            printf("(out_val) expected %lld got %lld: %s\n", expect, out_val, c);
            assert(false);
        }
    }
}

TEST(string_parse_s64) {
    test_string_parse_s64("123", true, 123);
    test_string_parse_s64("1230", true, 1230);
    test_string_parse_s64("12300", true, 12300);
    test_string_parse_s64("123000", true, 123000);
    test_string_parse_s64("1230001", true, 1230001);
    test_string_parse_s64("123000100", true, 123000100);
    test_string_parse_s64("00123000100", true, 123000100);
    test_string_parse_s64("00000000123000100", true, 123000100);
    test_string_parse_s64("+123", true, 123);
    test_string_parse_s64("-123", true, -123);
    test_string_parse_s64("  -123", true, -123);
    test_string_parse_s64("a  -123", false, 123);
    test_string_parse_s64("a  +123", false, 123);
    test_string_parse_s64("  +123", true, 123);
    test_string_parse_s64("  +123  ", true, 123);
    test_string_parse_s64("  +123a", false, 123);
    test_string_parse_s64("  +123a  ", false, 123);
    test_string_parse_s64("  +123  a  ", false,123);
    test_string_parse_s64("  +123  a", false,123);
    test_string_parse_s64("  +123  -", false,123);
    test_string_parse_s64("  +123  - ", false,123);
    test_string_parse_s64("  +123+", false,123);
    test_string_parse_s64("  +123 + ", false,123);
    test_string_parse_s64("  +123 + ", false,123);
    test_string_parse_s64("1-1", false,123);
    test_string_parse_s64("1-1", false,123);
    test_string_parse_s64("1 - 1", false,123);
    test_string_parse_s64(" 1-1", false,123);
    test_string_parse_s64(" 1 - 1 ", false,123);
    test_string_parse_s64(" 1- 1 ", false,123);
    test_string_parse_s64("a 1 -1b c", false,123);
    test_string_parse_s64("a 1 -1b ", false,123);
    test_string_parse_s64("a 1 -1 a", false,123);
    test_string_parse_s64("1 -1 a", false,123);
    test_string_parse_s64("1a", false,123);
    test_string_parse_s64("a1", false,123);
    test_string_parse_s64("1", true,1);
}


TEST(array_operations) {
    auto arr = make_array<s64>(8, &global_test_allocator, "test array"_S);

    {
        assert(array_get_size(&arr) == 0);

        array_remove(&arr, 0);
        assert(array_get_size(&arr) == 0);

        array_remove(&arr, 1);
        assert(array_get_size(&arr) == 0);

        array_remove(&arr, 5);
        assert(array_get_size(&arr) == 0);

        array_remove(&arr, -1);
        assert(array_get_size(&arr) == 0);

        array_remove(&arr, -10);
        assert(array_get_size(&arr) == 0);
    }

    {
        assert(array_get_at_index_unchecked(&arr, 0) == 0);
        assert(array_get_at_index_unchecked(&arr, 1) == 0);
        assert(array_get_at_index_unchecked(&arr, 10) == 0);
        assert(array_get_at_index_unchecked(&arr, -1) == 0);
        assert(array_get_at_index_unchecked(&arr, -10) == 0);
    }

    {
        *array_append(&arr) = 5;
        assert(array_get_size(&arr) == 1);
        assert(*array_get_at_index_unchecked(&arr, 0) == 5);
        assert(array_get_at_index_unchecked(&arr, 1) == 0);
        assert(array_get_at_index_unchecked(&arr, -1) == 0);
    }

    {
        *array_append(&arr) = 6;
        assert(array_get_size(&arr) == 2);
        assert(*array_get_at_index_unchecked(&arr, 0) == 5);
        assert(*array_get_at_index_unchecked(&arr, 1) == 6);
    }

    {
        *array_append(&arr) = 7;
        assert(array_get_size(&arr) == 3);
        array_remove(&arr, 1);
        assert(array_get_size(&arr) == 2);
        assert(*array_get_at_index_unchecked(&arr, 0) == 5);
        assert(*array_get_at_index_unchecked(&arr, 1) == 7);
    }

    {
        array_remove(&arr, 0);
        assert(array_get_size(&arr) == 1);
        assert(*array_get_at_index_unchecked(&arr, 0) == 7);
    }

    {
        array_remove(&arr, 1);
        array_remove(&arr, -1);
        array_remove(&arr, 10);
        assert(array_get_size(&arr) == 1);
        array_remove(&arr, 0);
        assert(array_get_size(&arr) == 0);
    }

    {
        for(s32 i = 0; i < 1024; i++) {
            *array_append(&arr) = i;
        }

        assert(array_get_size(&arr) == 1024);

        for(s32 i = 0; i < 512; i++) {
            array_remove(&arr, 0);
        }

        assert(array_get_size(&arr) == 512);

        for(s32 i = 0; i < 512; i++) {
            assert(*array_get_at_index_unchecked(&arr, i) == i + 512);
        }
    }
}

#endif

