cmake_minimum_required(VERSION 3.10)

# CMAKE_HOST_SYSTEM_NAME is available before project(); WIN32/MSVC are not
# (they're only reliable once project() has run the compiler/platform checks).
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    # Set 32-bit flags BEFORE project() so CMake detects the correct ABI.
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS}    -m32")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32")
endif()

project(CormanLisp LANGUAGES C CXX)

# Target: 32-bit x86. Linux via GCC -m32; Windows via the MSVC Win32 toolset
# (selected with `cmake -A Win32`, so no -m32 equivalent flag is needed there).
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -fno-omit-frame-pointer -fno-strict-aliasing -fno-PIE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fno-strict-aliasing -fpermissive -ffixed-esi -ffixed-edi -fno-PIE")
endif()

# ---- Include directories ----
set(CORMAN_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/CormanLispServer/include
)
if(NOT WIN32)
    # include/posix holds synthetic stand-ins for Win32/CRT headers
    # (windows.h, ole2.h, process.h, sys/stat.h, ...) that didn't exist
    # before the Linux port and must stay invisible to MSVC, which needs
    # the real SDK/CRT headers of the same name.
    list(APPEND CORMAN_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include/posix)
endif()

# ---- zlib (bundled static library) ----
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "Skip zlib tests" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "Skip zlib shared library" FORCE)
add_subdirectory(zlib)
set_target_properties(zlibstatic PROPERTIES OUTPUT_NAME z)
if(NOT WIN32)
    target_compile_options(zlibstatic PRIVATE -m32)
endif()

# ---- distorm (static library) ----
set(DISTORM_DIR ${CMAKE_SOURCE_DIR}/CormanLispServer/distorm)
set(DISTORM_SRCS
    ${DISTORM_DIR}/src/decoder.c
    ${DISTORM_DIR}/src/distorm.c
    ${DISTORM_DIR}/src/instructions.c
    ${DISTORM_DIR}/src/insts.c
    ${DISTORM_DIR}/src/mnemonics.c
    ${DISTORM_DIR}/src/operands.c
    ${DISTORM_DIR}/src/prefix.c
    ${DISTORM_DIR}/src/textdefs.c
    ${DISTORM_DIR}/src/wstring.c
)
add_library(distorm STATIC ${DISTORM_SRCS})
target_include_directories(distorm PUBLIC
    ${DISTORM_DIR}/include
    ${DISTORM_DIR}/src
)
if(WIN32)
    target_compile_options(distorm PRIVATE /W0)
else()
    target_compile_options(distorm PRIVATE -m32 -w)
endif()

# ---- CormanLisp library ----
# Sources common to both platforms.
set(CORMAN_SERVER_SRCS
    CormanLispServer/src/CharBuf.cpp
    CormanLispServer/src/Compx86.cpp
    CormanLispServer/src/CormanLispServer.cpp
    CormanLispServer/src/Gc.cpp
    CormanLispServer/src/Lisp.cpp
    CormanLispServer/src/Lispfunc.cpp
    CormanLispServer/src/LispMath.cpp
    CormanLispServer/src/LispThreadQueue.cpp
    CormanLispServer/src/MemoryReport.cpp
    CormanLispServer/src/Read.cpp
    CormanLispServer/src/Stdafx.cpp
    CormanLispServer/src/ThreadClasses.cpp
    CormanLispServer/src/Unassemble.cpp
    CormanLispServer/src/UserInfo.cpp
    CormanLispServer/src/Write.cpp
)

if(WIN32)
    # Windows keeps the original COM IPC layer: clconsole/clboot LoadLibrary
    # this DLL by name and call DllGetClassObject directly (no registry
    # registration needed).
    list(APPEND CORMAN_SERVER_SRCS
        CormanLispServer/src/CoCormanLisp.cpp
        CormanLispServer/src/CoConnectionPoint.cpp
        CormanLispServer/src/CoEnumConnectionPoints.cpp
        CormanLispServer/src/CoEnumConnections.cpp
    )
    add_library(cormanlisp SHARED ${CORMAN_SERVER_SRCS} CormanLispServer/CormanLispServer.def)
    set_target_properties(cormanlisp PROPERTIES OUTPUT_NAME CormanLispServer)
    target_compile_definitions(cormanlisp PRIVATE WIN32 NDEBUG _WINDOWS _CormanLispServer _CRT_SECURE_NO_WARNINGS)
else()
    # Linux links clconsole/clboot directly against a static library through
    # the cl_* C API instead of COM.
    list(APPEND CORMAN_SERVER_SRCS CormanLispServer/src/CoCormanLispStubs.cpp)
    add_library(cormanlisp STATIC ${CORMAN_SERVER_SRCS})
    target_compile_definitions(cormanlisp PRIVATE LINUX _CormanLispServer)
target_compile_definitions(cormanlisp PRIVATE $<$<CONFIG:Debug>:_DEBUG>)
endif()

# GC verification and per-collection tracing without the per-call debug
# noise of a full _DEBUG build: defines _DEBUG for Gc.cpp only.
option(GC_DEBUG "Enable GC heap verification (_DEBUG in Gc.cpp only)" OFF)
if(GC_DEBUG)
    set_source_files_properties(CormanLispServer/src/Gc.cpp
        PROPERTIES COMPILE_DEFINITIONS _DEBUG)
endif()

target_include_directories(cormanlisp PRIVATE
    ${CORMAN_INCLUDE_DIRS}
    ${DISTORM_DIR}/include
    ${DISTORM_DIR}/src
    ${CMAKE_BINARY_DIR}/zlib
)
target_link_libraries(cormanlisp PRIVATE zlibstatic distorm)
if(WIN32)
    target_link_libraries(cormanlisp PRIVATE ws2_32 userenv)
else()
    target_link_libraries(cormanlisp PRIVATE pthread dl m)
endif()

# ---- clconsole (console REPL) ----
add_executable(clconsole clconsole/clconsole.cpp)
target_include_directories(clconsole PRIVATE ${CORMAN_INCLUDE_DIRS})

if(WIN32)
    # clconsole never links the DLL's import lib: it LoadLibrary()s
    # CormanLispServer.dll itself at runtime and calls DllGetClassObject
    # directly (see clconsole.cpp). add_dependencies just orders the build
    # so the DLL exists next to clconsole.exe when it's run.
    add_dependencies(clconsole cormanlisp)
else()
    # Naked/inline-asm functions in the static library jmp/call into these
    # symbols by their unmangled name; --defsym aliases the GNU-ld symbol to
    # its real (mangled) C++ name so the raw asm can reach it.
    set(CORMAN_DEFSYM_LINK_FLAGS "-Wl,--defsym=ThreadQV=_Z8ThreadQVv -Wl,--defsym=EnterGCCriticalSection=_Z22EnterGCCriticalSectionv -Wl,--defsym=LeaveGCCriticalSection=_Z22LeaveGCCriticalSectionv -Wl,--defsym=AllocLocalCons=_Z14AllocLocalConsv -Wl,--defsym=LoadLocalHeap=_Z13LoadLocalHeapv -Wl,--defsym=garbageCollect=_Z14garbageCollectl -Wl,--defsym=ThrowUserException=_Z18ThrowUserExceptionv -Wl,--defsym=Plus=_Z4Plusmz -Wl,--defsym=Minus=_Z5Minusmz -Wl,--defsym=checkFunction=_Z13checkFunctionm -no-pie")
    target_link_libraries(clconsole PRIVATE cormanlisp ${CORMAN_DEFSYM_LINK_FLAGS})
endif()

# ---- Lisp image (built after clconsole via makeimg.sh) ----
if(NOT WIN32)
    set(CORMANLISP_IMG ${CMAKE_SOURCE_DIR}/CormanLisp.img)
    add_custom_command(
        OUTPUT ${CORMANLISP_IMG}
        COMMAND ${CMAKE_SOURCE_DIR}/makeimg.sh || (echo "NOTE: CormanLisp.img not built yet -- compiler bootstrap incomplete" && touch ${CORMANLISP_IMG}.pending)
        DEPENDS clconsole ${CMAKE_SOURCE_DIR}/Sys/compile-sys.lisp
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Building CormanLisp.img (may fail during bootstrap)")
    add_custom_target(cormanlisp_img DEPENDS ${CORMANLISP_IMG})
endif()

# ---- clboot (minimal bootstrap) ----
add_executable(clboot clboot/clboot.cpp)
target_include_directories(clboot PRIVATE ${CORMAN_INCLUDE_DIRS})

if(WIN32)
    # Same dynamic-loading relationship as clconsole; see above.
    add_dependencies(clboot cormanlisp)
else()
    target_compile_definitions(clboot PRIVATE LINUX)
    target_link_libraries(clboot PRIVATE cormanlisp ${CORMAN_DEFSYM_LINK_FLAGS} -no-pie)
endif()

# ---- Testing ----
enable_testing()
if(WIN32)
    add_test(NAME test-all
        COMMAND clconsole --batch -image ${CMAKE_SOURCE_DIR}/CormanLisp.img -execute ${CMAKE_SOURCE_DIR}/test/test-all.lisp
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
    add_test(NAME test-all
        COMMAND sh -c "cd ${CMAKE_SOURCE_DIR} && if [ -f CormanLisp.img ] || ./makeimg.sh; then ${CMAKE_BINARY_DIR}/clconsole --batch -image ${CMAKE_SOURCE_DIR}/CormanLisp.img -execute ${CMAKE_SOURCE_DIR}/test/test-all.lisp; else echo 'SKIP: CormanLisp.img not built'; exit 125; fi"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
set_tests_properties(test-all PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED")

# ---- Linting ----
find_program(PRE_COMMIT pre-commit)
if(PRE_COMMIT)
    add_custom_target(lint
        COMMAND ${PRE_COMMIT} run --all-files
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running pre-commit lint on all files")
else()
    add_custom_target(lint
        COMMAND ${CMAKE_COMMAND} -E echo "pre-commit not found. Install: pip install pre-commit"
        VERBATIM)
endif()
