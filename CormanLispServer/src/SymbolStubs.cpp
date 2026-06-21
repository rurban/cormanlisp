// Symbol stubs and C-linkage aliases for functions called from assembly.
// On Linux, C++ name mangling prevents asm from finding symbols.
// This file provides:
// 1. C-linkage aliases for functions already defined (ThreadQV, etc.)
// 2. Stub implementations for functions lost to #if 0 blocks

#include "Stdafx.h"
#include "Lisp.h"

// ---- C-linkage aliases for C++ functions called from inline asm ----

// These functions are defined in other .o files but their C++ mangled
// names aren't reachable from GCC inline asm (which uses unmangled names).
// Provide weak aliases from the asm-expected name to the mangled symbol.

extern "C"
{
	// From CormanLispServer.cpp (mangled _Z8ThreadQVv)
	extern LispObj* ThreadQV() __asm__("_Z8ThreadQVv");

	// From Gc.cpp
	extern void EnterGCCriticalSection() __asm__("_Z22EnterGCCriticalSectionv");
	extern LispObj AllocLocalCons() __asm__("_Z14AllocLocalConsv");
	extern LispObj LoadLocalHeap() __asm__("_Z13LoadLocalHeapv");
	extern void garbageCollect(long) __asm__("_Z14garbageCollectl");

	// From Lispfunc.cpp
	extern LispObj Plus(LispObj, ...) __asm__("_Z4Plusmz");
	extern LispObj Minus(LispObj, ...) __asm__("_Z5Minusmz");
	extern void checkFunction(LispObj) __asm__("_Z13checkFunctionm");

	// ---- Stubs for functions lost to #if 0 ----

	// AllocLargeVector was inside #if 0 in Gc.cpp
	LispObj AllocLargeVector(long)
	{
		return 0;
	}

	// WrongNumberOfArgs
	void WrongNumberOfArgs() {}

	// throwOSException / ThrowUserException — inside #if 0 in Lisp.cpp
	void throwOSException() {}
	void ThrowUserException() {}

	// LeaveGCCriticalSection — inside #if 0 in Gc.cpp
	void LeaveGCCriticalSection() {}

} // extern "C"
