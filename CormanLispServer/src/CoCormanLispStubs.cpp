// CoCormanLisp stubs — provides symbols normally in CoCormanLisp.cpp

#include "Stdafx.h"
#include "CormanLispServer.h"
#include "Lisp.h"

// Globals normally defined in CoCormanLisp.cpp
char LispImageName[260] = "";
int CormanLispClientType = 1;
const int LispImageNameMax = 260;
CoCormanLisp* CormanLispServer = NULL;


// CoCormanLisp method stubs (STDMETHODIMP = HRESULT return)
CoCormanLisp::CoCormanLisp() {}
CoCormanLisp::~CoCormanLisp() {}

HRESULT CoCormanLisp::QueryInterface(REFIID, void**) { return E_NOINTERFACE; }
ULONG  CoCormanLisp::AddRef()  { return 1; }
ULONG  CoCormanLisp::Release() { return 1; }

HRESULT CoCormanLisp::Initialize(IUnknown*, const char*, int) { return S_OK; }
HRESULT CoCormanLisp::Run(HANDLE*) { return S_OK; }
HRESULT CoCormanLisp::ProcessSource(char*, long) { return S_OK; }
HRESULT CoCormanLisp::GetNumThreads(long* num) { *num = 0; return S_OK; }
HRESULT CoCormanLisp::UserException() { return S_OK; }
HRESULT CoCormanLisp::AbortThread() { return S_OK; }
HRESULT CoCormanLisp::InitializeEx(IUnknown*, const char*, int, int, int, int, int) { return S_OK; }
HRESULT CoCormanLisp::GetCurrentUserName(char* buf, size_t* len) {
    const char* u = getenv("USER"); if (!u) u = "unknown";
    if (buf) strncpy(buf, u, *len); *len = strlen(u) + 1; return S_OK;
}
HRESULT CoCormanLisp::GetCurrentUserProfileDirectory(char* buf, size_t* len) {
    const char* h = getenv("HOME"); if (!h) h = "/tmp";
    if (buf) strncpy(buf, h, *len); *len = strlen(h) + 1; return S_OK;
}
HRESULT CoCormanLisp::GetCurrentUserPersonalDirectory(char* buf, size_t* len) {
    return GetCurrentUserProfileDirectory(buf, len);
}
HRESULT CoCormanLisp::GetImageLoadsCount(LONG* count) { *count = 0; return S_OK; }
HRESULT CoCormanLisp::EnumConnectionPoints(IEnumConnectionPoints**) { return 0x80004001L; }
HRESULT CoCormanLisp::FindConnectionPoint(REFIID, IConnectionPoint**) { return 0x80004001L; }
HRESULT CoCormanLisp::SetMessage(const char*) { return S_OK; }
HRESULT CoCormanLisp::GetMessage(char*, long) { return S_OK; }
HRESULT CoCormanLisp::SetDefaultMessage() { return S_OK; }
HRESULT CoCormanLisp::OutputText(const char*, long) { return S_OK; }
HRESULT CoCormanLisp::GetAppInstance(HINSTANCE*) { return 0x80004001L; }
HRESULT CoCormanLisp::GetAppMainWindow(HWND*) { return 0x80004001L; }
HRESULT CoCormanLisp::OpenEditWindow(char*, HWND*) { return S_OK; }
HRESULT CoCormanLisp::OpenURL(char*, HWND*) { return S_OK; }
HRESULT CoCormanLisp::AddMenu(char*) { return S_OK; }
HRESULT CoCormanLisp::AddMenuItem(char*, char*) { return S_OK; }
HRESULT CoCormanLisp::ReplaceSelection(char*, long) { return S_OK; }
HRESULT CoCormanLisp::BlessThread() { return S_OK; }
HRESULT CoCormanLisp::UnblessThread() { return S_OK; }
HRESULT CoCormanLisp::GetFunctionAddress(wchar_t*, wchar_t*, void**) { return 0x80004001L; }
HRESULT CoCormanLisp::HandleStructuredException(long, LPEXCEPTION_POINTERS, long*) { return S_OK; }
HRESULT CoCormanLisp::LispShutdown(const char*, long) { return S_OK; }

// Missing internal functions — C linkage for asm callers
// LeaveGCCriticalSection defined in Gc.cpp (may conflict if un-stubbed)
extern "C" {
LispObj AllocLargeVector(long) { return 0; }
void WrongNumberOfArgs() {}
void throwOSException() {}
}
// These are declared as C++ in Lisp.h, must keep C++ linkage

