# Linux Port of Corman Lisp

## Context

Corman Lisp is a Windows-only, 32-bit x86 Common Lisp implementation (~27,000 lines C++, ~6970 lines x86 compiler). The user wants it running on Linux. This is a from-scratch port: every subsystem touches Windows APIs or inline MSVC x86 assembly. No existing Linux build files, no `#ifdef` platform branches, no abstraction layer — the code is Windows through and through.

## Approach

The port is architecturally feasible but **extremely labor-intensive** — months of work by someone who understands both x86 assembly and Lisp runtime internals. Below is a phased approach ordered by dependency, with the rationale that phases 1-4 must complete before anything works, phase 5 is the bulk, and phases 6-8 are polish.

### Phase 1: Build system (CMake + GCC/Clang)

**Replace** all `.vcxproj`/`.sln` with a single top-level `CMakeLists.txt`.

- Create `CMakeLists.txt` at repo root. Build targets:
  - `cormanlisp` — shared library (`libcormanlisp.so`) from `CormanLispServer/src/*.cpp` + `zlib/*.c` + `distorm/src/*.c`
  - `clconsole` — console REPL executable from `clconsole/clconsole.cpp`
  - `clboot` — minimal bootstrap executable (no GUI) from `clboot/clboot.cpp`
- Compiler: GCC or Clang, C++14. Enable `-fpermissive` for MSVC-isms during initial port.
- Convert: all IDE projects (`CormanLispIDE/`) to QT5.
- Drop: installer (`installer/`), MSI, WiX, all `.bat` files, `src_vc15.sln`, DLL template (`dlltemplate/`), DLL client (`dllclient/`), AutoCad library (`Libraries/acad/`). (#ifdef _WIN32)
- `makeimg.bat` → shell script `makeimg.sh` that invokes the bootstrapped console app.
- Copy: `zlib/CMakeLists.txt` exists for the bundled zlib — integrate it as a static library target, not a separate build.

### Phase 2: Threading primitives (replace Win32 → pthreads)

File: `include/ThreadClasses.h` + `CormanLispServer/src/ThreadClasses.cpp`

- Replace `#include <windows.h>` with `#include <pthread.h>` (or a new `platform.h` that selects the backend).
- Map each class:
  - `CriticalSection` → `pthread_mutex_t` (non-recursive by default; if code relies on recursive, use `PTHREAD_MUTEX_RECURSIVE`).
  - `PLEvent` (manual-reset + auto-reset) → `pthread_cond_t` + `pthread_mutex_t` + a boolean flag.
  - `PLSemaphore` → POSIX semaphore (`sem_t`) or a condvar-based semaphore.
  - `PLSingleLock` → RAII wrapper, same API.
- Remove `HANDLE`, `DWORD`, `LPCTSTR`, `LPSECURITY_ATTRIBUTES`.
- `HANDLE m_hObject` → `int m_hObject` (fd or internal index), or the raw pthread types.

### Phase 3: Thread-local storage (replace `__declspec(thread)` + `fs` register → pthread TLS)

Files: `CormanLispServer/include/Lisp.h`, `CormanLispServer/src/Lisp.cpp`, `CormanLispServer/src/CormanLispServer.cpp`

- `__declspec(thread)` → `__thread` (GCC) or `thread_local` (C++11 — prefer this since we set C++14).
- The `fs:0018h` register access in `SETUP_LISP_CALL` for getting the thread's QV array: this is part of inline assembly macros used to access the thread's Quick Vector. Replace the entire macro pair (`SETUP_LISP_CALL`/`END_LISP_CALL`) with C code using `pthread_getspecific()` or direct `thread_local` variable access.
- `TlsAlloc`/`TlsGetValue`/`TlsSetValue`/`TlsFree` → `pthread_key_create`/`pthread_getspecific`/`pthread_setspecific`/`pthread_key_delete`.
- `ThreadQV()` — the two assembly variants (NTONLY vs WIN98ONLY in `CormanLispServer.cpp`) both read `fs` register. Replace with a single portable implementation using `pthread_getspecific(Thread_Index)`.

### Phase 4: Memory management (replace VirtualAlloc/VirtualProtect → mmap/mprotect)

File: `CormanLispServer/src/Gc.cpp` — the single hardest file.

- `VirtualAlloc(…, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE)` → `mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)`.
- `VirtualFree` → `munmap`.
- `VirtualProtect(…, PAGE_READWRITE)` → `mprotect(…, PROT_READ|PROT_WRITE)`.
- `VirtualProtect(…, PAGE_READONLY)` → `mprotect(…, PROT_READ)`.
- Write barrier: the GC uses SEH `__try`/`__except` to catch access violations on write-protected pages. On Linux: install a `SIGSEGV` signal handler via `sigaction` with `SA_SIGINFO`. In the handler, inspect `siginfo_t->si_addr` to determine the faulting page, mark it dirty in the page table, `mprotect` it to read-write, and return. This is a direct 1:1 mapping of the SEH pattern.
- `SuspendThread`/`ResumeThread` for stack scanning during GC → `pthread_kill(thread, SIGUSR1)` to pause threads, or use a cooperative safepoint mechanism (threads check a flag). The existing code already uses `GetThreadContext` to read `EIP`/`ESP`/`EBP`; on Linux, use `signal.h` + `ucontext_t` to read registers from the signal handler frame. **Decision**: use signals for simplicity — the GC already suspends all other threads, so a signal-based approach maps directly.
- `FlushInstructionCache` → `__builtin___clear_cache` (GCC) or no-op on x86 Linux (self-modifying code on x86 doesn't need explicit cache flush).
- `GetProcessHeap`/`HeapAlloc` → `malloc`/`free` (used for non-GC allocations like symbol tables).
- `#define address_to_page(addr) (((unsigned long)addr) >> 12)` — unchanged, since Linux also uses 4KB pages by default, but verify at runtime via `sysconf(_SC_PAGESIZE)`.

### Phase 5: Replace all inline x86 assembly with C or GCC inline asm

Files: **all** `CormanLispServer/src/*.cpp` files, `CormanLispServer/include/Lisp.h`, `CormanLispServer/include/Compx86.h`.

This is the bulk of the work. Strategy: replace MSVC `__asm { … }` blocks with GCC extended asm (`asm volatile(… : … : … : "memory", …)`). Key hotspots:

- **`__declspec(naked)` functions** (~75 functions in `Lisp.cpp`, `Gc.cpp`, `Lispfunc.cpp`, `LispMath.cpp`): MSVC naked functions have no prolog/epilog — the function body is raw assembly that ends with `ret`. On GCC, use `__attribute__((naked))` with the same constraint.

- **`SETUP_LISP_CALL(numargs)` / `END_LISP_CALL()` macros** (`Lisp.h` ~L1169-1180): These push/pop callee-save registers and set up the thread's QV pointer from `fs:0018h`. Replace with C code using `thread_local` or `pthread_getspecific`. The register save/restore is handled by the compiler's normal calling convention — the assembly was needed because MSVC didn't reserve enough registers for the Lisp calling convention, but with GCC `-ffixed-esi -ffixed-edi` (or similar) we can reserve the registers the Lisp runtime needs.

- **`LispCall0` through `LispCall8`** (`Lisp.cpp`): Naked functions that marshal C arguments into Lisp args (in ESI/EDI registers) and call into compiled Lisp code. Replace with GCC inline asm that loads values from the stack into the expected registers, then issues `call *%eax` (or whatever register holds the function pointer).

- **Fixnum overflow detection** (`LispMath.cpp` ~L615-617, ~L858-860, etc.): `__asm add …, __asm jo do_bignum` → use GCC `__builtin_add_overflow()` / `__builtin_saddl_overflow()` for signed 32-bit. Same for sub, mul. This is actually **simpler** on GCC — no inline asm needed.

- **`rdtsc`** (`Lispfunc.cpp` ~L893): `#define rdtsc _emit 0x0f __asm _emit 0x31` → `__builtin_ia32_rdtsc()` or `asm volatile("rdtsc" : "=a"(low), "=d"(high))`.

- **`Plus_EAX_EDX` / `Minus_EAX_EDX`** (`Lispfunc.cpp` ~L3460): Naked assembly functions that implement `+` and `-` for two args inline. Replace with C implementations that use `__builtin_add_overflow`.

- **`genericThunkFunc`** (`Lispfunc.cpp`): Machine-code-patching function that creates FFI thunks at runtime. The thunk is x86 code bytes patched into a buffer — this is architecture-specific but not OS-specific; the same x86 code works on Linux. However, the buffer allocation must use `mmap` with `PROT_READ|PROT_WRITE|PROT_EXEC` instead of `VirtualAlloc` with `PAGE_EXECUTE_READWRITE`.

- **`ThrowUserException` / `throwOSException`** (`Lisp.cpp`): Assembly jumps into the SEH handler. Replace with `longjmp` or a signal-based mechanism that raises `SIGUSR2`.

- **GC atomic sections** (`Gc.cpp` ~L594-780, ~L1005-1103): Assembly blocks that push/pop registers, enter/leave critical sections, and scan the stack. Port to GCC inline asm with explicit clobber lists. The assembly here is largely stack-frame setup and function calls to C code — much of it can become plain C with the compiler managing registers.

- **Thread pool suspend/resume** (`LispThreadQueue.cpp`): Assembly to read `EBP` for stack start detection. Replace with `__builtin_frame_address(0)` or `ucontext_t` from signal handler.

- **`Compx86.h` code generation macros** (~717 lines): These emit x86 opcode bytes — they are architecture-specific (x86) but not OS-specific. Keep them as-is; they work on Linux since `unsigned char*` buffers with `memcpy` of opcodes are portable.

### Phase 6: Exception handling (replace SEH → POSIX signals)

Files: `CormanLispServer/src/Lisp.cpp` (the `handleStructuredException` function, `throwOSException`), `CormanLispServer/src/Gc.cpp` (write barrier `__try`/`__except`)

- `__try`/`__except` blocks → `sigsetjmp`/`siglongjmp` + signal handlers.
- The write barrier in `Gc.cpp` catches `EXCEPTION_ACCESS_VIOLATION`: install `SIGSEGV` handler that checks if the faulting address is within a GC heap page. If yes, mark dirty and return (after `mprotect` to read-write). If no, re-raise or call the previous handler.
- `EXCEPTION_INT_DIVIDE_BY_ZERO` → `SIGFPE`.
- `EXCEPTION_STACK_OVERFLOW` → alternate signal stack via `sigaltstack` + `SIGSEGV` handler.
- `handleStructuredException` currently maps EXCEPTION codes to Lisp condition symbols. Replace the filter expression with `sigsetjmp` before the protected block and a check after.

### Phase 7: Replace COM IPC → direct function calls or Unix domain sockets

Files: `CormanLispServer/src/CormanLispServer.cpp`, `CormanLispServer/src/CoCormanLisp.cpp`, `CormanLispServer/src/CoConnectionPoint.cpp`, `CormanLispServer/src/CoEnumConnectionPoints.cpp`, `CormanLispServer/src/CoEnumConnections.cpp`, `clconsole/clconsole.cpp`, `clboot/clboot.cpp`, `clbootapp/clbootapp.cpp`, `clconsoleapp/clconsoleapp.cpp`

The COM layer is used for inter-process communication between the Lisp kernel (DLL) and the client (console/boot/IDE apps). Since Linux has no COM:

- **Decision: use direct linking** — clients link `libcormanlisp.so` directly and call into it via a plain C API instead of COM QueryInterface/AddRef/Release. This is the simplest approach and matches how most language runtimes work (Python, Ruby, etc.).
- Replace `ICormanLisp` COM interface with a struct of function pointers or a plain C header declaring exported functions.
- Replace `ICormanLispStatusMessage` (text output callback) with a function pointer passed during initialization.
- Replace `IClassFactory`/`CoCreateInstance`/`CoGetClassObject` with direct `dlopen`/`dlsym` or static linking.
- Drop all COM boilerplate files: `CoCormanLisp.cpp`, `CoConnectionPoint.cpp`, `CoEnumConnectionPoints.cpp`, `CoEnumConnections.cpp`.
- Drop `CormanLispServer.def` (DLL exports file) — use `__attribute__((visibility("default")))` on the public API.
- `DllMain` → use `__attribute__((constructor))` and `__attribute__((destructor))` functions for init/cleanup.
- `WSAStartup`/`WSACleanup` → drop (sockets work without init on Linux); keep the socket code itself (Lisp-level socket functions).
- Registry-based COM registration (`RegSetValue`, etc.) → drop entirely.

### Phase 8: Client applications

- **`clconsole`** — the console REPL (`clconsole/clconsole.cpp`, 725 lines). Rewrite to remove COM: link directly to `libcormanlisp.so`, replace COM method calls with direct function calls. Replace `conio.h` / `_getch` with termios-based raw terminal input or use `readline` for the REPL. Replace `COORD`/`SetConsoleCursorPosition` console tricks with ANSI escape sequences or just drop them.
- **`clboot`** — minimal bootstrap (`clboot/clboot.cpp`): same treatment. This is used to build the image file.
- **Drop**: `clconsoleapp` (GUI console app using Win32 windows), `clbootapp` (GUI boot app), the entire `CormanLispIDE/` directory (MFC-based IDE — 18+ source files, complete rewrite needed for Linux).
- The IDE functionality (editor, inspector, debugger) is left as future work. The console REPL is sufficient for a working Lisp.

### Phase 9: Lisp-level changes

Files in `Sys/`, `Modules/`

- `Sys/ffi.lisp`: The FFI mechanism loads Windows DLLs by name. Change to use `dlopen`/`dlsym` on Linux. The `DEFINE-FOREIGN-FUNCTION` macro currently generates `LoadLibrary`/`GetProcAddress` calls; replace with `dlopen`/`dlsym`.
- `Modules/win32-symbols.lisp` (4367 lines, ~3300 Win32 function bindings): Replace with Linux system call and libc bindings (`Modules/linux-symbols.lisp`). This is a large mechanical task but not difficult — each binding is a one-liner. Prioritize the core set (file I/O, process, socket, time) and leave the rest for later.
- `Sys/kernel-asm.lisp`: Inline assembly definitions for type checks — review for x86-specific bits that need GCC asm syntax equivalents in the compiler.
- `makeimg.sh`: Replace `makeimg.bat` — invoke `clconsole` to load and compile all `.lisp` files, then dump the image. The build order is defined in `CormanLispImage/CormanLispImage.vcxproj` (the `ImageBuild` target).

### Phase 10: Compiler code generation (x86 → x86-64 future-proofing)

File: `CormanLispServer/src/Compx86.cpp` (6970 lines)

- The x86 code generator remains x86-only for the initial port (Linux x86 32-bit).
- `LispObj` is `unsigned long` (32-bit) — the entire type system (29-bit fixnums, 3-bit tags) assumes 32-bit pointers. Moving to 64-bit would require changing `LispObj` to `unsigned long long`, adjusting tag bits, doubling Cons cell size (8→16 bytes), and rewriting all allocation code. **Not in scope for this port** — target 32-bit x86 Linux (`-m32`).
- Build the entire project as 32-bit: `-m32` flag, link against 32-bit libraries. Most Linux distros still ship 32-bit compatibility libs.

## Critical files & anchors

|File|Anchor|Why|
|---|---|---|
|`CormanLispServer/src/Gc.cpp`|`VirtualAlloc`, `VirtualProtect`, `__asm`, `__try`/`__except`|Hardest single file; GC page protection + stack scanning + assembly sections|
|`CormanLispServer/include/Lisp.h`|`SETUP_LISP_CALL`/`END_LISP_CALL` macros, `__declspec(thread)`|Core macros used everywhere; must be replaced before anything compiles|
|`CormanLispServer/src/Lisp.cpp`|`LispCall0`-`LispCall8`, `handleStructuredException`, `ThrowUserException`|Naked functions + exception handling|
|`CormanLispServer/src/CormanLispServer.cpp`|`DllMain`, `ThreadQV`, COM class factory|Entry point + TLS + COM replacement|
|`CormanLispServer/src/Compx86.cpp`|Code generation macros, `FlushInstructionCache`|Compiler emits x86 opcodes; mostly portable but needs exec-memory allocation|
|`include/ThreadClasses.h`|`CriticalSection`, `PLEvent`, `PLSingleLock`|Threading primitives used in GC and runtime|
|`clconsole/clconsole.cpp`|COM client code, `ICormanLispStatusMessage`|Console REPL; model for porting clients|
|`Sys/ffi.lisp`|`LoadLibrary`, `GetProcAddress`|Lisp-side FFI; must call `dlopen`/`dlsym` instead|

## Verification

After each phase, verify the build compiles. Full integration test only after all phases complete:

1. **Phase 1-3**: `cmake -B build -DCMAKE_CXX_FLAGS="-m32" && cmake --build build` compiles without errors.
2. **Phase 4**: GC unit test — allocate 100K cons cells, force a GC, verify no crash.
3. **Phase 5**: Run `(+ 1 2)` → 3 in the REPL (exercises fixnum arithmetic, LispCallN, QV setup).
4. **Phase 6**: `(/ 1 0)` → signals a `DIVISION-BY-ZERO` condition instead of crashing.
5. **Phase 7-8**: `clconsole` starts, shows REPL prompt, accepts input.
6. **Phase 9-10**: `makeimg.sh` produces `CormanLisp.img` from all `.lisp` sources. Loading the image and running `(format t "Hello, Linux~%")` prints to stdout.
7. **Smoke test**: Run `test/ansi-chapter-2.lisp` through the bootstrapped REPL — basic Lisp forms (CONS, CAR, CDR, LAMBDA, LET) should pass.

## Assumptions & contingencies

- **Target is 32-bit x86 Linux** (not x86-64). If 32-bit multilib is unavailable on the build machine, install `gcc-multilib` / `glibc-devel.i686`. If the user wants 64-bit, this plan doesn't cover it — that requires changing the entire type system.
- **No IDE port**. The MFC IDE is Windows-only. A future Qt/GTK IDE is out of scope. The console REPL is the deliverable.
- **COM replacement is direct linking**, not Unix domain sockets. If inter-process isolation is needed, it's future work.
- **The x86 code generator stays x86**. No ARM or x86-64 native code generation. An interpreter fallback could be added later for non-x86 platforms.
- **If `pthread_kill` + signal-based GC suspension proves unreliable** (threads in signal-unsafe code), fall back to cooperative safepoints: threads check a `volatile int gc_pending` flag in the Lisp function prolog and yield.
- **If GCC `__attribute__((naked))` doesn't work identically to MSVC** (GCC's naked functions can't reference function arguments by name), rewrite the ~75 naked functions as plain functions with inline asm for the critical sections, letting GCC handle the prolog/epilog.
