//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		platform.h
//		Contents:	OS abstraction layer for Corman Lisp Linux port.

#ifndef PLATFORM_H
#define PLATFORM_H

#if defined(__linux__) || defined(__unix__)
  #define LINUX 1
#else
  #define LINUX 0
#endif

#if LINUX

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <climits>
#include <stdlib.h>
#include <climits>
#include <cctype>
#include <setjmp.h>

// ---- Win32 type aliases ----
#include <sys/time.h>
typedef int                 BOOL;
typedef unsigned long       DWORD;
typedef unsigned long       ULONG;
typedef long                LONG;
#include <dlfcn.h>
typedef long*               LPLONG;
typedef const char*         LPCTSTR;
typedef void*               HANDLE;
typedef void*               HINSTANCE;
typedef void*               HWND;
typedef void*               LPSECURITY_ATTRIBUTES;
typedef void*               LPVOID;
typedef unsigned int        UINT;
typedef char                TCHAR;
typedef wchar_t             WCHAR;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef long long           __int64;

#define FALSE  0
#define TRUE   1
#define INFINITE  ((DWORD)-1)
#define WAIT_OBJECT_0  0
#define WAIT_TIMEOUT   0x00000102L
#define MAX_PATH 260
#define ERROR_SUCCESS 0L

// ---- MSVC keyword stubs ----
#define __declspec(x)
#define interface struct
#define __stdcall
#define __cdecl
#define FAR
#define PASCAL
#define _stdcall
#define __try       if (1)
#define __except(x) else
#define __finally
#define __leave     break
#define __asm __asm_ignored

// ---- String stubs ----
#define strcpy_s(dst, sz, src)  strncpy(dst, src, sz)
#define strcat_s(dst, sz, src)  strncat(dst, src, sz)
#define sprintf_s               snprintf
#define lstrlen                 strlen
#define __TEXT(x)               x
#define _TRUNCATE               ((size_t)-1)

// ---- Safe string functions ----
inline int strncpy_s(char* dst, size_t sz, const char* src, size_t cnt) {
    size_t n = cnt < sz ? cnt : sz - 1;
    strncpy(dst, src, n); dst[n] = 0; return 0;
}
#define fopen_s(ppf, path, mode) ((*(ppf) = fopen(path, mode)) ? 0 : errno)

// ---- Misc stubs ----
// CloseHandle at line 98
inline DWORD GetModuleFileName(void*, char* buf, DWORD sz) { buf[0]=0; return 0; }
#define MessageBeep(x)          ((void)0)
#define OutputDebugString(s)    fprintf(stderr, "%s", s)
#define Sleep(ms)               usleep((ms) * 1000)
#define GetLastError()          (errno)
#define SetLastError(x)         (errno = (x))
inline int CloseHandle(HANDLE) { return 0; }  // return int not void for Lispfunc.cpp
#define _MAX_PATH MAX_PATH
inline size_t wcslen(const wchar_t* s) { size_t n = 0; while (s[n]) n++; return n; }

// ---- Interlocked / atomics ----
inline LONG InterlockedIncrement(volatile LONG* p) { return __sync_add_and_fetch(p, 1); }
inline LONG InterlockedDecrement(volatile LONG* p) { return __sync_sub_and_fetch(p, 1); }
inline LONG InterlockedExchange(volatile LONG* p, LONG v) { return __sync_lock_test_and_set(p, v); }
inline LONG InterlockedCompareExchange(volatile LONG* p, LONG ex, LONG comp) {
    return __sync_val_compare_and_swap(p, comp, ex);
}
// Performance counter
typedef union _LARGE_INTEGER { __int64 QuadPart; struct { DWORD LowPart; LONG HighPart; }; } LARGE_INTEGER;
inline int QueryPerformanceCounter(LARGE_INTEGER* lp) { *lp = LARGE_INTEGER{0}; return 1; }

// ---- min/max ----
// Additional stubs for Lispfunc.cpp
inline int QueryPerformanceFrequency(LARGE_INTEGER* lp) { *lp = LARGE_INTEGER{1000000}; return 1; }
inline DWORD GetTickCount(void) { struct timeval tv; gettimeofday(&tv, NULL); return tv.tv_sec*1000 + tv.tv_usec/1000; }
#define PAGE_GUARD 0x100
inline int SetCurrentDirectoryA(const char* dir) { return chdir(dir) == 0; }
// strncat_s
inline int strncat_s(char* dst, size_t sz, const char* src, size_t cnt) {
    size_t dlen = strlen(dst); size_t n = cnt < (sz - dlen - 1) ? cnt : (sz - dlen - 1);
    strncat(dst, src, n); return 0;
}

// DL library stubs for Lispfunc.cpp FFI (Phase 9: replace with dlopen/dlsym)
#define HMODULE HINSTANCE
typedef void (__stdcall *FARPROC)();
inline HMODULE LoadLibrary(const char* name) { return (HMODULE)dlopen(name, RTLD_LAZY); }
inline HMODULE LoadLibraryA(const char* name) { return LoadLibrary(name); }
inline FARPROC GetProcAddress(HMODULE mod, const char* name) { return (FARPROC)dlsym((void*)mod, name); }
inline int FreeLibrary(HMODULE mod) { return dlclose((void*)mod) == 0; }

// Time/date stubs
struct _timeb { time_t time; unsigned short millitm; short timezone, dstflag; };
inline void _ftime_s(struct _timeb* tb) { struct timeval tv; gettimeofday(&tv, NULL); tb->time = tv.tv_sec; tb->millitm = tv.tv_usec/1000; }

struct SYSTEMTIME { unsigned short wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; };
struct FILETIME { unsigned long dwLowDateTime, dwHighDateTime; };
inline void GetSystemTime(SYSTEMTIME* st) { time_t t = time(NULL); struct tm* tm = gmtime(&t); st->wYear = tm->tm_year+1900; st->wMonth = tm->tm_mon+1; st->wDay = tm->tm_mday; st->wDayOfWeek = tm->tm_wday; st->wHour = tm->tm_hour; st->wMinute = tm->tm_min; st->wSecond = tm->tm_sec; st->wMilliseconds = 0; }
inline void GetLocalTime(SYSTEMTIME* st) { time_t t = time(NULL); struct tm* tm = localtime(&t); st->wYear = tm->tm_year+1900; st->wMonth = tm->tm_mon+1; st->wDay = tm->tm_mday; st->wDayOfWeek = tm->tm_wday; st->wHour = tm->tm_hour; st->wMinute = tm->tm_min; st->wSecond = tm->tm_sec; st->wMilliseconds = 0; }
inline int SystemTimeToFileTime(const SYSTEMTIME*, FILETIME*) { return 0; }
inline int FileTimeToSystemTime(const FILETIME*, SYSTEMTIME*) { return 0; }

struct TIME_ZONE_INFORMATION { LONG Bias; WCHAR StandardName[32]; SYSTEMTIME StandardDate; LONG StandardBias; WCHAR DaylightName[32]; SYSTEMTIME DaylightDate; LONG DaylightBias; };
#define TIME_ZONE_ID_DAYLIGHT 2
inline DWORD GetTimeZoneInformation(TIME_ZONE_INFORMATION* tzi) { tzi->Bias = 0; return 0; }

// alloca
#include <alloca.h>
#define _alloca alloca
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

// ---- TLS ----
inline DWORD TlsAlloc() { pthread_key_t k; pthread_key_create(&k, NULL); return (DWORD)k; }
inline void  TlsSetValue(DWORD k, void* v) { pthread_setspecific((pthread_key_t)k, v); }
inline void* TlsGetValue(DWORD k) { return pthread_getspecific((pthread_key_t)k); }
inline void  TlsFree(DWORD k) { pthread_key_delete((pthread_key_t)k); }

// ---- Memory management ----
#define HEAP_ZERO_MEMORY 8
inline void* HeapReAlloc(HANDLE, DWORD, void* ptr, size_t sz) { return realloc(ptr, sz); }

#define address_to_page(addr)   (((unsigned long)(addr)) >> 12)
#define page_to_address(page)   ((void*)(((unsigned long)(page)) << 12))
#define page_address(page)      page_to_address(page)
#define page_offset(addr)       ((((unsigned long)(addr)) << 20) >> 23)

#define PAGE_NOACCESS           PROT_NONE
#define PAGE_READONLY           PROT_READ
#define PAGE_READWRITE          (PROT_READ | PROT_WRITE)
#define PAGE_EXECUTE            PROT_EXEC
#define PAGE_EXECUTE_READ       (PROT_READ | PROT_EXEC)
#define PAGE_EXECUTE_READWRITE  (PROT_READ | PROT_WRITE | PROT_EXEC)

#define MEM_RESERVE   0x00002000
#define MEM_COMMIT    0x00001000
#define MEM_RELEASE   0x00008000
#define MEM_FREE      0x00010000
#define MEM_DECOMMIT  0x00004000

inline LPVOID VirtualAlloc(LPVOID addr, size_t size, DWORD, DWORD protect) {
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    if (addr) flags |= MAP_FIXED;
    void* p = mmap(addr, size, protect, flags, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
}
inline int VirtualFree(LPVOID addr, size_t size, DWORD) { return munmap(addr, size) == 0; }
inline int VirtualProtect(LPVOID addr, size_t size, DWORD np, DWORD*) { return mprotect(addr, size, np) == 0; }
#define FlushInstructionCache(p,a,s) 1

#define GetProcessHeap()        ((HANDLE)1)
#define HeapAlloc(h,f,s)        malloc(s)
#define HeapFree(h,f,p)         free(p)
#define HeapCreate(f,i,m)       ((HANDLE)1)
#define HeapDestroy(h)          ((void)0)

struct MEMORY_BASIC_INFORMATION {
    void*  BaseAddress; void* AllocationBase; DWORD AllocationProtect;
    size_t RegionSize; DWORD State, Protect, Type;
};
inline size_t VirtualQuery(void* addr, MEMORY_BASIC_INFORMATION* info, size_t sz) {
    memset(info, 0, sz); return sz;
}

// ---- Thread creation ----
typedef unsigned(__stdcall *PTHREAD_START)(void*);
struct _threadex_arg { PTHREAD_START fn; void* arg; };
inline HANDLE _beginthreadex(void*, unsigned, PTHREAD_START fn, void* arg, unsigned, unsigned* id) {
    _threadex_arg* ta = new _threadex_arg{fn, arg};
    pthread_t* pt = new pthread_t;
    pthread_create(pt, NULL, [](void* p) -> void* {
        auto* a = (_threadex_arg*)p; a->fn(a->arg); delete a; return NULL;
    }, ta);
    if (id) *id = (unsigned)(unsigned long)*pt;
    return (HANDLE)pt;
}
#define CREATE_SUSPENDED         4
#define THREAD_PRIORITY_NORMAL   0
inline void   SetThreadPriority(HANDLE, int) {}
inline DWORD  ResumeThread(HANDLE)         { return 0; }
inline DWORD  SuspendThread(HANDLE)         { return 0; }
inline void   _endthreadex(unsigned)       {}
inline DWORD  GetCurrentThreadId()         { return (DWORD)(unsigned long)pthread_self(); }
inline HANDLE GetCurrentProcess()          { return (HANDLE)1; }
inline HANDLE GetCurrentThread()           { return (HANDLE)pthread_self(); }
inline int    DuplicateHandle(HANDLE, HANDLE, HANDLE, HANDLE*, DWORD, int, DWORD) { return 1; }
#define DUPLICATE_SAME_ACCESS 2
inline DWORD  GetCurrentProcessId()        { return (DWORD)getpid(); }

// ---- Wait / sync stubs ----
inline DWORD  WaitForSingleObject(HANDLE, DWORD) { return WAIT_OBJECT_0; }
inline HANDLE CreateMutex(void*, int, const char*) { return (HANDLE)1; }

// ---- COM stubs (Phase 7) ----
typedef long HRESULT;
#define S_OK                      ((HRESULT)0L)
#define E_FAIL                    ((HRESULT)0x80004005L)
#define E_NOINTERFACE             ((HRESULT)0x80004002L)
#define E_OUTOFMEMORY             ((HRESULT)0x8007000EL)
#define CLASS_E_NOAGGREGATION     ((HRESULT)0x80040110L)
#define CLASS_E_CLASSNOTAVAILABLE ((HRESULT)0x80040111L)
#define REGDB_E_CLASSNOTREG       ((HRESULT)0x80040154L)
#define S_FALSE                   ((HRESULT)1L)
#define CONNECT_E_ADVISELIMIT     ((HRESULT)0x80040202L)
#define CONNECT_E_CANNOTCONNECT   ((HRESULT)0x80040203L)
#define CONNECT_E_NOCONNECTION    ((HRESULT)0x80040204L)
#define E_POINTER                 ((HRESULT)0x80004003L)
#define FAILED(hr)                ((hr) < 0)

#define STDMETHOD(m)              virtual HRESULT m
#define STDMETHOD_(type,m)        virtual type m
#define STDMETHODIMP              HRESULT
#define STDMETHODIMP_(type)       type
#define PURE                      = 0
#define THIS_
#define THIS
#define STDAPI                    extern "C" HRESULT
#define STDMETHODCALLTYPE
#define WINAPI

// GUID
typedef struct { unsigned long Data1; unsigned short Data2; unsigned short Data3; unsigned char Data4[8]; } GUID;
typedef GUID IID, CLSID, REFIID, REFCLSID;
#define DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
    const GUID name = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }
inline bool operator==(const GUID& a, const GUID& b) { return memcmp(&a, &b, sizeof(GUID)) == 0; }
inline bool operator!=(const GUID& a, const GUID& b) { return !(a == b); }

// IUnknown
struct IUnknown {
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) PURE;
    STDMETHOD_(unsigned long, AddRef)(void) PURE;
    STDMETHOD_(unsigned long, Release)(void) PURE;
};
struct IClassFactory : public IUnknown {
    STDMETHOD(CreateInstance)(IUnknown* pUnkOuter, REFIID riid, void** ppv) PURE;
    STDMETHOD(LockServer)(int bLock) PURE;
};
struct IConnectionPoint : public IUnknown {};
struct IConnectionPointContainer : public IUnknown {
    STDMETHOD(FindConnectionPoint)(REFIID riid, IConnectionPoint** ppCP) PURE;
};
struct IEnumConnectionPoints : public IUnknown {};
struct IEnumConnections : public IUnknown {};

inline const GUID& __iid_iunknown() {
    static GUID g = {0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
    return g;
}
inline const GUID& __iid_iclassfactory() {
    static GUID g = {0x00000001,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
    return g;
}
#define IID_IUnknown        __iid_iunknown()
#define IID_IClassFactory   __iid_iclassfactory()

// ---- SEH stubs (Phase 6) ----
typedef struct _EXCEPTION_RECORD {
    unsigned long ExceptionCode, ExceptionFlags;
    struct _EXCEPTION_RECORD* ExceptionRecord;
    void* ExceptionAddress;
    unsigned long NumberParameters;
    unsigned long ExceptionInformation[15];
} EXCEPTION_RECORD;
typedef struct _CONTEXT {
    unsigned long ContextFlags, Eip, Esp, Ebp, Edi, Esi, Ebx, Edx, Ecx, Eax;
} CONTEXT;
typedef struct _EXCEPTION_POINTERS { EXCEPTION_RECORD* ExceptionRecord; CONTEXT* ContextRecord; } EXCEPTION_POINTERS;
typedef EXCEPTION_POINTERS* LPEXCEPTION_POINTERS;
typedef int EXCEPTION_DISPOSITION;

#define GetExceptionCode()          0
#define GetExceptionInformation()   ((LPEXCEPTION_POINTERS)0)
#define CONTEXT_FULL                0x10007
#define CONTEXT_CONTROL             0x10001
#define CONTEXT_INTEGER             0x10002
#define EXCEPTION_EXECUTE_HANDLER   1
#define CREATE_ALWAYS 2
#define HWND_DESKTOP ((HWND)0)
#define MB_OK 0
#define MB_SETFOREGROUND 0x10000
#define MB_YESNO 4
#define IDNO 7
inline int MessageBox(HWND, const char*, const char*, unsigned) { return IDNO; }
inline void ExitThread(DWORD) {}
inline void ExitProcess(unsigned) {}
#define FORMAT_MESSAGE_FROM_SYSTEM 0x1000
inline DWORD FormatMessage(DWORD, void*, DWORD, DWORD, char*, DWORD, void*) { return 0; }
#define lstrcpy strcpy
#define wsprintf sprintf
#define EXCEPTION_CONTINUE_SEARCH   0
#define EXCEPTION_CONTINUE_EXECUTION (-1)
#define ExceptionContinueExecution EXCEPTION_CONTINUE_EXECUTION
#define ExceptionContinueSearch    EXCEPTION_CONTINUE_SEARCH
#define CONTROL_C_EXIT              0xC000013AL
inline void GetThreadContext(HANDLE, CONTEXT*) {}
inline void SetThreadContext(HANDLE, CONTEXT*) {}

// ---- Registry stubs ----
#define HKEY_CLASSES_ROOT     ((void*)0x80000000)
#define REG_SZ                1
#define CLSCTX_INPROC_SERVER  1
inline LONG RegSetValue(void*, const char*, DWORD, const char*, DWORD) { return ERROR_SUCCESS; }
inline LONG RegDeleteKey(void*, const char*) { return ERROR_SUCCESS; }

// DLL entry points
#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0
#define DLL_THREAD_ATTACH  2
#define DLL_THREAD_DETACH  3

// ---- OS stubs ----
struct OSVERSIONINFO {
    DWORD dwOSVersionInfoSize, dwMajorVersion, dwMinorVersion, dwBuildNumber, dwPlatformId;
    char  szCSDVersion[128];
};

// ---- Socket stubs ----
struct WSADATA {
    unsigned short wVersion, wHighVersion;
    char szDescription[257], szSystemStatus[129];
    unsigned short iMaxSockets, iMaxUdpDg;
    char* lpVendorInfo;
};
#define MAKEWORD(a,b) ((unsigned short)(((unsigned char)(a))|((unsigned short)((unsigned char)(b)))<<8))
inline int WSAStartup(unsigned short, WSADATA*) { return 0; }
#define EXCEPTION_ARRAY_BOUNDS_EXCEEDED     0xC000008CLU
#define EXCEPTION_BREAKPOINT                0x80000003LU
#define EXCEPTION_DATATYPE_MISALIGNMENT     0x80000002LU
#define EXCEPTION_FLT_DENORMAL_OPERAND      0xC000008DLU
#define EXCEPTION_FLT_DIVIDE_BY_ZERO        0xC000008ELU
#define EXCEPTION_FLT_INEXACT_RESULT        0xC000008FLU
#define EXCEPTION_FLT_INVALID_OPERATION     0xC0000090LU
#define EXCEPTION_FLT_OVERFLOW              0xC0000091LU
#define EXCEPTION_FLT_STACK_CHECK           0xC0000092LU
#define EXCEPTION_FLT_UNDERFLOW             0xC0000093LU
#define EXCEPTION_ILLEGAL_INSTRUCTION       0xC000001DLU
#define EXCEPTION_IN_PAGE_ERROR             0xC0000006LU
#define EXCEPTION_INVALID_DISPOSITION       0xC0000026LU
#define EXCEPTION_NONCONTINUABLE_EXCEPTION  0xC0000025LU
#define EXCEPTION_PRIV_INSTRUCTION          0xC0000096LU
#define EXCEPTION_SINGLE_STEP               0x80000004LU
inline int WSACleanup() { return 0; }

// ---- COM init stubs ----
inline void CoInitialize(void*) {}
inline void CoUninitialize() {}

// ---- CRITICAL_SECTION (Win32) stub ----
struct CRITICAL_SECTION_STUB { char _[40]; };
#define CRITICAL_SECTION CRITICAL_SECTION_STUB
inline void InitializeCriticalSection(void*) {}
inline void DeleteCriticalSection(void*) {}
inline void EnterCriticalSection(void*) {}
inline void LeaveCriticalSection(void*) {}

// ---- File I/O stubs ----
#define GENERIC_READ             0x80000000LU
#define GENERIC_WRITE            0x40000000L
#define FILE_SHARE_READ          1
#define OPEN_EXISTING            3
#define FILE_ATTRIBUTE_NORMAL    0x80
#define INVALID_HANDLE_VALUE     ((HANDLE)-1)
#define FILE_MAP_READ            4
inline HANDLE CreateFile(const char*, DWORD, DWORD, void*, DWORD, DWORD, HANDLE) {
    return INVALID_HANDLE_VALUE;
}
inline DWORD GetFileSize(HANDLE, DWORD*) { return 0; }
inline HANDLE CreateFileMapping(HANDLE, void*, DWORD, DWORD, DWORD, const char*) { return INVALID_HANDLE_VALUE; }
inline void* MapViewOfFile(HANDLE, DWORD, DWORD, DWORD, size_t) { return NULL; }
inline void FlushViewOfFile(void*, size_t) {}
inline void UnmapViewOfFile(void*) {}

// ---- PE header structs ----
#define IMAGE_DOS_SIGNATURE 0x5A4D
struct IMAGE_DOS_HEADER {
    unsigned short e_magic, e_cblp, e_cp, e_crlc, e_cparhdr, e_minalloc, e_maxalloc, e_ss, e_sp;
    unsigned short e_csum, e_ip, e_cs, e_lfarlc, e_ovno, e_res[4], e_oemid, e_oeminfo, e_res2[10];
    long e_lfanew;
};
struct IMAGE_FILE_HEADER {
    unsigned short Machine, NumberOfSections;
    unsigned long TimeDateStamp, PointerToSymbolTable, NumberOfSymbols;
    unsigned short SizeOfOptionalHeader, Characteristics;
};
struct IMAGE_OPTIONAL_HEADER {
    unsigned short Magic;
    unsigned char MajorLinkerVersion, MinorLinkerVersion;
    unsigned long SizeOfCode, SizeOfInitializedData, SizeOfUninitializedData;
    unsigned long AddressOfEntryPoint, BaseOfCode, BaseOfData, ImageBase;
    unsigned long SectionAlignment, FileAlignment;
    unsigned short MajorOperatingSystemVersion, MinorOperatingSystemVersion;
    unsigned short MajorImageVersion, MinorImageVersion;
    unsigned short MajorSubsystemVersion, MinorSubsystemVersion;
    unsigned long Win32VersionValue, SizeOfImage, SizeOfHeaders, CheckSum;
    unsigned short Subsystem, DllCharacteristics;
    unsigned long SizeOfStackReserve, SizeOfStackCommit, SizeOfHeapReserve, SizeOfHeapCommit;
    unsigned long LoaderFlags, NumberOfRvaAndSizes;
};
struct IMAGE_SECTION_HEADER {
    char Name[8];
    union { unsigned long PhysicalAddress, VirtualSize; } Misc;
    unsigned long VirtualAddress, SizeOfRawData, PointerToRawData;
    unsigned long PointerToRelocations, PointerToLinenumbers;
    unsigned short NumberOfRelocations, NumberOfLinenumbers;
    unsigned long Characteristics;
};

// ---- Exception codes ----
#define EXCEPTION_ACCESS_VIOLATION    0xC0000005LU
#define EXCEPTION_INT_DIVIDE_BY_ZERO  0xC0000094LU
#define EXCEPTION_STACK_OVERFLOW      0xC00000FDLU

#endif // LINUX
#endif // PLATFORM_H
