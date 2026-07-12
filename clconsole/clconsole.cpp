//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		clconsole.cpp
//		Contents:	Corman Lisp console application client.
//			        Portable between Windows (_WIN32) and Linux.
//		Author:		Roger Corman
//		Created:	3/6/98

#ifdef _WIN32

#include <wtypes.h>
#include <string.h>
#include <stdio.h>
#include <ocidl.h>
#include <initguid.h>
#include <conio.h>
#include <fcntl.h>
#include <io.h>

#include "clsids.h"
#include "ErrorMessage.h"
#include "ICormanLisp.h"

#else // LINUX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <termios.h>

#include "cormanlisp_api.h"

#endif

static char gModuleName[MAX_PATH];
static char gImageName[MAX_PATH];

#ifdef _WIN32
const char* consoleAppName = "CLCONSOLE.EXE";
bool isTemplateApp = false;
const char SERVER_TITLE[] = "CormanLispServer.dll";
char LispServerPath[MAX_PATH + 1 + sizeof(SERVER_TITLE)];
typedef HRESULT(WINAPI* GETCLASSOBJECTFUNC)(REFCLSID rclsid, REFIID riid, void** ppv);
static HINSTANCE getLocalCormanLispServer();
static IClassFactory* getCormanLispClassFactory();
static IClassFactory* getCormanLispRegisteredClassFactory();
#else
const char* consoleAppName = "clconsole";
bool isTemplateApp = false;
#endif

static void outputConsoleText(const char* text);
static char* getConsoleText();
static void LoadFile(const char* filename);

// ---- Linux termios input ----
#ifdef LINUX
static struct termios g_orig_termios;
static int g_term_raw = 0;

static void term_enable_raw(void)
{
	if (g_term_raw)
		return;
	tcgetattr(STDIN_FILENO, &g_orig_termios);
	struct termios raw = g_orig_termios;
	raw.c_lflag &= ~(ECHO | ICANON);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
	g_term_raw = 1;
}

static void term_restore(void)
{
	if (!g_term_raw)
		return;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
	g_term_raw = 0;
}
#endif

// ---- Windows COM client classes ----
#ifdef _WIN32

class ConsoleCormanLispClient : public ICormanLispStatusMessage
{
public:
	ConsoleCormanLispClient();
	~ConsoleCormanLispClient();

	// IUnknown methods
	STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// ICormanLisp methods
	STDMETHODIMP OutputText(const char* text, long numChars);
	STDMETHODIMP SetMessage(const char* text);
	STDMETHODIMP GetMessage(char* text, long maxMessageLength);
	STDMETHODIMP SetDefaultMessage();
	STDMETHODIMP GetAppInstance(HINSTANCE* appInstance);
	STDMETHODIMP GetAppMainWindow(HWND* appMainWindow);
	STDMETHODIMP OpenEditWindow(char* file, HWND* wnd);
	STDMETHODIMP AddMenu(char* menuName);
	STDMETHODIMP AddMenuItem(char* menuName, char* menuItem);
	STDMETHODIMP OpenURL(char* file, HWND* wnd);
	STDMETHODIMP ReplaceSelection(const char* text, long numChars);

	// Helper functions
	STDMETHODIMP Connect(IConnectionPoint* pConnectionPoint);
	STDMETHODIMP Disconnect(IConnectionPoint* pConnectionPoint);

private:
	long m_cRef;
	DWORD m_dwCookie;
};

class ConsoleCormanLispShutdownClient : public ICormanLispShutdown
{
public:
	ConsoleCormanLispShutdownClient();
	~ConsoleCormanLispShutdownClient();

	// IUnknown methods
	STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// ICormanLispShutdown methods
	STDMETHODIMP LispShutdown(const char* text, long numChars);

	// Helper functions
	STDMETHODIMP Connect(IConnectionPoint* pConnectionPoint);
	STDMETHODIMP Disconnect(IConnectionPoint* pConnectionPoint);

private:
	long m_cRef;
	DWORD m_dwCookie;
};

typedef void(WINAPI* LOADLIBRARYFUNC)();

ICormanLisp* pCormanLisp = 0;

#else // LINUX

// Linux callbacks (replaces COM)
static void output_text(const char* text, long numChars)
{
	fwrite(text, 1, numChars, stdout);
	fflush(stdout);
}

static void set_message(const char* text)
{
	fprintf(stderr, "\r[%s]\n", text);
}

static void set_default_message(void) {}

static void open_edit_window(const char* file, void** wnd)
{
	(void)file;
	(void)wnd;
}

static void add_menu(const char* menuName)
{
	(void)menuName;
}

static void add_menu_item(const char* menuName, const char* menuItem)
{
	(void)menuName;
	(void)menuItem;
}

static void open_url(const char* url, void** wnd)
{
	(void)url;
	(void)wnd;
}

static void replace_selection(const char* text, long numChars)
{
	(void)text;
	(void)numChars;
}

static void lisp_shutdown(const char* text, long numChars)
{
	if (text && numChars > 0)
		fwrite(text, 1, numChars, stderr);
	fprintf(stderr, "\nLisp has shut down.\n");
}

static struct CormanLispCallbacks g_callbacks = {output_text,	   set_message,		  set_default_message,
												 open_edit_window, add_menu,		  add_menu_item,
												 open_url,		   replace_selection, lisp_shutdown};

#endif // _WIN32/LINUX

char* execFile = 0;

static int g_batch_mode = 0;
static const char* g_image_name = NULL;
static const char* g_exec_file = NULL;

#ifdef _WIN32
int mainx(int argc, char* argv[])
{
	IConnectionPoint* pConnectionPoint = 0;
	IConnectionPoint* pShutdownConnectionPoint = 0;
	HRESULT hr = E_FAIL;

	CoInitialize(0);

	//  Get the CormanLisp class factory
	IClassFactory* pcf = getCormanLispClassFactory();
	IUnknown* pUnk = 0;
	hr = pcf->CreateInstance(0, IID_IUnknown, (void**)&pUnk);
	if (FAILED(hr))
	{
		ErrorMessage(__TEXT("QueryInterface() did not return IID_IUnknown"), hr);
		return FALSE;
	}
	pcf->Release();
	pcf = 0;

	hr = pUnk->QueryInterface(IID_ICormanLisp, (void**)&pCormanLisp);
	if (FAILED(hr))
	{
		ErrorMessage(__TEXT("QueryInterface() did not return IID_ICormanLisp"), hr);
		return FALSE;
	}

	// Connect the ICormanLispClient Sink
	IConnectionPointContainer* pConnectionPointContainer = 0;
	hr = pCormanLisp->QueryInterface(IID_IConnectionPointContainer, (void**)&pConnectionPointContainer);

	if (FAILED(hr))
	{
		ErrorMessage(__TEXT("QueryInterface() did not return IID_IConnectionPointContainer"), hr);
		return FALSE;
	}

	hr = pConnectionPointContainer->FindConnectionPoint(IID_ICormanLispTextOutput, &pConnectionPoint);
	if (FAILED(hr))
	{
		pConnectionPointContainer->Release();
		ErrorMessage(__TEXT("FindConnectionPoint() did not return IID_ICormanLispStatusMessage"), hr);
		return FALSE;
	}

	// make an instance of our outgoing class interface
	ConsoleCormanLispClient* pCormanLispClient = new ConsoleCormanLispClient;
	pCormanLispClient->AddRef();

	hr = pCormanLispClient->Connect(pConnectionPoint);
	if (FAILED(hr))
	{
		pCormanLispClient->Release();
		pConnectionPoint->Release();
		fprintf(stderr, "Connection failed");
	}

	hr = pConnectionPointContainer->FindConnectionPoint(IID_ICormanLispShutdown, &pShutdownConnectionPoint);
	pConnectionPointContainer->Release();
	if (FAILED(hr))
	{
		ErrorMessage(__TEXT("FindConnectionPoint() did not return IID_ICormanLispShutdowne"), hr);
		return FALSE;
	}

	// make an instance of our outgoing shutdown class interface
	ConsoleCormanLispShutdownClient* pCormanLispShutdownClient = new ConsoleCormanLispShutdownClient;
	pCormanLispShutdownClient->AddRef();

	hr = pCormanLispShutdownClient->Connect(pShutdownConnectionPoint);
	if (FAILED(hr))
	{
		pCormanLispShutdownClient->Release();
		pShutdownConnectionPoint->Release();
		fprintf(stderr, "Connection failed");
	}

	pCormanLisp->Initialize(pCormanLispClient, gImageName, CONSOLE_CLIENT);
	HANDLE thread = 0;
	pCormanLisp->Run(&thread);

	long numThreads = 0;
	char* input = 0;
	bool quitting = false;
	if (execFile)
	{
		LoadFile(execFile);
		char exitCommand[] = "(win:ExitProcess 0)\n";
		pCormanLisp->ProcessSource(exitCommand, sizeof(exitCommand));
	}
	while (1)
	{
		if (!quitting)
			input = getConsoleText();
		if (strlen(input) > 0)
		{
			if (!_stricmp(input, ":quit\r\n"))
				break;
			if (!quitting)
				pCormanLisp->ProcessSource(input, strlen(input));
		}
		else if (!quitting)
		{
			char exitCommand[] = "(win:ExitProcess 0) \n #< expression not finished > \n (win:ExitProcess 0)\n";
			quitting = true;
			pCormanLisp->ProcessSource(exitCommand, sizeof(exitCommand));
			Sleep(1000);
		}
	}
	if (pCormanLisp)
		pCormanLisp->Release();
	pCormanLisp = 0;
	CoUninitialize();
	return 0;
}

//
//	If a CormanLispServer.dll exists in the same directory as
//	the calling application, load it and return a handle to it.
//
static HINSTANCE getLocalCormanLispServer()
{
	DWORD chars = GetModuleFileName(0, LispServerPath, sizeof(LispServerPath));
	int index = chars - 1;
	while (index >= 0 && LispServerPath[index] != '\\')
		index--;
	LispServerPath[index] = 0; // get rid of file name, just leave the path
	if (chars > 0)
		strcat_s(LispServerPath, sizeof(LispServerPath), "\\");
	strcat_s(LispServerPath, sizeof(LispServerPath), SERVER_TITLE);

	return LoadLibrary(LispServerPath);
}

static IClassFactory* getCormanLispClassFactory()
{
	IClassFactory* pcf = 0;
	HINSTANCE module = getLocalCormanLispServer();
	if (module)
	{
		FARPROC proc = GetProcAddress(module, "DllGetClassObject");
		if (!proc)
		{
			ErrorMessage("Could not find DllGetClassObject() in CormanLispServer.dll");
			return 0;
		}
		GETCLASSOBJECTFUNC GetClassObjectFunc = (GETCLASSOBJECTFUNC)proc;

		HRESULT hr = GetClassObjectFunc(CLSID_CormanLisp, IID_IClassFactory, (void**)&pcf);

		if (FAILED(hr))
		{
			ErrorMessage(__TEXT("CoGetClassObject"), hr);
			return 0;
		}
	}
	if (!pcf)
		pcf = getCormanLispRegisteredClassFactory();
	return pcf;
}

static IClassFactory* getCormanLispRegisteredClassFactory()
{
	IClassFactory* pcf = 0;

	// see if the class is registered
	HRESULT hr = CoGetClassObject(CLSID_CormanLisp, CLSCTX_INPROC_SERVER, 0, IID_IClassFactory, (void**)&pcf);
	if (FAILED(hr))
	{
		if (hr == REGDB_E_CLASSNOTREG)
		{
			// the server was not registered, so see if we can
			// find it and register it now
			HINSTANCE plserver = LoadLibrary(SERVER_TITLE);
			if (!plserver)
			{
				ErrorMessage("Could not load CormanLispServer.dll");
				return 0;
			}
			FARPROC proc = GetProcAddress(plserver, "DllRegisterServer");
			if (!proc)
			{
				ErrorMessage("Could not find DllRegisterServer() in CormanLispServer.dll");
				return 0;
			}
			LOADLIBRARYFUNC func = (LOADLIBRARYFUNC)proc;
			func(); // calls DllRegisterServer()

			// now try again
			hr = CoGetClassObject(CLSID_CormanLisp, CLSCTX_INPROC_SERVER, 0, IID_IClassFactory, (void**)&pcf);

			if (FAILED(hr))
			{
				ErrorMessage(__TEXT("CoGetClassObject"), hr);
				return FALSE;
			}
		}
		else
		{
			ErrorMessage(__TEXT("CoGetClassObject"), hr);
			return FALSE;
		}
	}
	return pcf;
}

long numThreads = 0;

BOOL __stdcall HandlerRoutine(DWORD dwCtrlType)
{
	BOOL handled = FALSE;
	switch (dwCtrlType)
	{
		case CTRL_C_EVENT:
			if (pCormanLisp)
			{
				pCormanLisp->GetNumThreads(&numThreads);
				if (numThreads > 0)
					pCormanLisp->AbortThread();
			}
			handled = TRUE;
			break;
		case CTRL_BREAK_EVENT:
			if (pCormanLisp)
			{
				pCormanLisp->GetNumThreads(&numThreads);
				if (numThreads > 0)
					pCormanLisp->AbortThread();
			}
			handled = TRUE;
			break;
		case CTRL_CLOSE_EVENT: break;
		case CTRL_LOGOFF_EVENT: break;
		case CTRL_SHUTDOWN_EVENT: break;
	}
	return handled;
}

int main(int argc, char* argv[])
{
	// use binary mode
	int result = _setmode(_fileno(stdout), _O_BINARY);
	result = _setmode(_fileno(stdin), _O_BINARY);

	BOOL ret = SetConsoleCtrlHandler(HandlerRoutine, TRUE);
	SetConsoleTitle("Corman Lisp Console");
#ifdef SET_CONSOLE_SCREEN_BUFFER_SIZE
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD coord;
	coord.X = 100;
	coord.Y = 100; // default console size if 100 x 100
	SetConsoleScreenBufferSize(handle, coord);
#endif // SET_CONSOLE_SCREEN_BUFFER_SIZE
	GetModuleFileName(0, gModuleName, sizeof(gModuleName));

	isTemplateApp = _stricmp(argv[0], "clconsole") != 0; // see if this app is being used
														 // as a template

	char* imageName = 0;
	for (int i = 1; i < argc; i++)
	{
		if (!_stricmp(argv[i], "-image") && (i + 1) < argc)
		{
			imageName = argv[i + 1];
		}
		if (!_stricmp(argv[i], "-execute") && (i + 1) < argc)
		{
			execFile = argv[i + 1];
		}
	}
	if (imageName)
		strcpy_s(gImageName, sizeof(gImageName), imageName);
	else
	{
		unsigned int moduleNameLength = strlen(gModuleName);
		if (moduleNameLength >= strlen(consoleAppName) &&
			!_stricmp(gModuleName + (moduleNameLength - strlen(consoleAppName)), consoleAppName))
		{
			strcpy_s(gImageName, sizeof(gImageName), gModuleName);
			strcpy_s(gImageName + (moduleNameLength - strlen(consoleAppName)), sizeof(gImageName), "CormanLisp.img");
		}
		else
		{
			strcpy_s(gImageName, sizeof(gImageName), gModuleName);
		}
	}
	static char szAppName[] = "CormanLispConsole";
	mainx(0, 0);
	return 0;
}

//	If an entire line has been entered, return it.
//	Otherwise return an empty string.
//
char ConsoleTextBuf[4096];
static char* s = ConsoleTextBuf;

static void outputConsoleText(const char* text)
{
	const char* p = text;
	while (*p)
	{
		putchar(*p++);
	}
}

static char* getConsoleText()
{
	while (1)
	{
		int c = getchar();

		if (c == EOF)
		{
		}
		else if (c == 10)
		{
			*s++ = (char)c;
			*s++ = 0;
			s = ConsoleTextBuf;
			return s;
		}
		else
			*s++ = (char)c;
	}
}

//////////////////////////////////////////////////////////////////////
// ctor and dtor
ConsoleCormanLispClient::ConsoleCormanLispClient()
{
	m_cRef = 0;
	m_dwCookie = (unsigned long)-1;
}

ConsoleCormanLispClient::~ConsoleCormanLispClient() {}

//////////////////////////////////////////////////////////////////////
// IUnknown interfaces

STDMETHODIMP ConsoleCormanLispClient::QueryInterface(REFIID riid, void** ppv)
{
	if (riid == IID_IUnknown)
		*ppv = (ICormanLisp*)this;
	else if (riid == IID_ICormanLispTextOutput)
		*ppv = (ICormanLispTextOutput*)this;
	else if (riid == IID_ICormanLispStatusMessage)
		*ppv = (ICormanLispStatusMessage*)this;
	else
		*ppv = 0;
	if (*ppv)
		((IUnknown*)*ppv)->AddRef();
	return *ppv ? S_OK : E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ConsoleCormanLispClient::AddRef()
{
	return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) ConsoleCormanLispClient::Release()
{
	if (InterlockedDecrement(&m_cRef) != 0)
		return m_cRef;
	delete this;
	return 0;
}

STDMETHODIMP ConsoleCormanLispClient::OutputText(const char* text, long numBytes)
{
	for (int i = 0; i < numBytes; i++)
	{
		putchar(text[i]);
	}
	fflush(stdout);
	return S_OK;
}

STDMETHODIMP ConsoleCormanLispClient::SetMessage(const char* /*message*/)
{
	return S_OK;
}

STDMETHODIMP ConsoleCormanLispClient::GetMessage(char* /*text*/, long /*maxMessageLength*/)
{
	return S_OK;
}

STDMETHODIMP ConsoleCormanLispClient::SetDefaultMessage()
{
	return S_OK;
}

STDMETHODIMP ConsoleCormanLispClient::GetAppInstance(HINSTANCE* appInstance)
{
	*appInstance = 0;
	return S_OK;
}

STDMETHODIMP ConsoleCormanLispClient::GetAppMainWindow(HWND* appMainWindow)
{
	*appMainWindow = 0;
	return S_OK;
}

STDMETHODIMP ConsoleCormanLispClient::Connect(IConnectionPoint* pConnectionPoint)
{
	return pConnectionPoint->Advise((ICormanLispStatusMessage*)this, &m_dwCookie);
}

STDMETHODIMP ConsoleCormanLispClient::Disconnect(IConnectionPoint* pConnectionPoint)
{
	return pConnectionPoint->Unadvise(m_dwCookie);
}

STDMETHODIMP ConsoleCormanLispClient::OpenEditWindow(char* /*file*/, HWND* /*wnd*/)
{
	return E_FAIL;
}

STDMETHODIMP ConsoleCormanLispClient::OpenURL(char* file, HWND* /*wnd*/)
{
	return (ShellExecuteA(NULL, "open", file, NULL, NULL, SW_SHOWNORMAL) > (HINSTANCE)32 ? S_OK : E_FAIL);
}

STDMETHODIMP ConsoleCormanLispClient::AddMenu(char* /*menuName*/)
{
	return E_FAIL;
}

STDMETHODIMP ConsoleCormanLispClient::AddMenuItem(char* /*menuName*/, char* /*menuItem*/)
{
	return E_FAIL;
}

STDMETHODIMP ConsoleCormanLispClient::ReplaceSelection(const char* /*text*/, long /*numChars*/)
{
	return E_FAIL;
}

BYTE* MapFile(const char* path, DWORD* length)
{
	HANDLE hfile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hfile == INVALID_HANDLE_VALUE)
		return 0;
	*length = GetFileSize(hfile, 0);
	if (*length == 0xffffffff)
	{
		CloseHandle(hfile);
		return 0;
	}

	HANDLE hfilemap = CreateFileMapping(hfile, 0, PAGE_READONLY, 0, 0, 0);

	CloseHandle(hfile);
	if (!hfilemap)
		return 0;

	BYTE* pbFile = (BYTE*)MapViewOfFile(hfilemap, FILE_MAP_READ, 0, 0, 0);
	CloseHandle(hfilemap);
	return pbFile;
}

void UnmapFile(BYTE* mapping)
{
	UnmapViewOfFile(mapping);
}

static void LoadFile(const char* filename)
{
	const char* ext = filename + strlen(filename);
	while (ext > filename && *ext != '.')
		ext--;
	if (*ext == '.' && !_stricmp(ext, ".fasl"))
	{
		// load a compiled file
		char command[512];
		strcpy_s(command, sizeof(command), "(common-lisp:load #P\"");
		strcat_s(command, sizeof(command), filename);
		strcat_s(command, sizeof(command), "\")");
		pCormanLisp->ProcessSource(command, strlen(command));
		return;
	}

	// load the file into memory
	DWORD fileSize;
	BYTE* data = MapFile(filename, &fileSize);

	if (!data)
	{
		fprintf(stderr, "Error: Could not open file %s.\n", filename);
		return;
	}

	// process code in the file
	pCormanLisp->ProcessSource((char*)data, fileSize);

	UnmapFile(data);
}

//////////////////////////////////////////////////////////////////////
// ctor and dtor
ConsoleCormanLispShutdownClient::ConsoleCormanLispShutdownClient()
{
	m_cRef = 0;
	m_dwCookie = (unsigned long)-1;
}

ConsoleCormanLispShutdownClient::~ConsoleCormanLispShutdownClient() {}

//////////////////////////////////////////////////////////////////////
// IUnknown interfaces

STDMETHODIMP ConsoleCormanLispShutdownClient::QueryInterface(REFIID riid, void** ppv)
{
	if (riid == IID_IUnknown)
		*ppv = this;
	else if (riid == IID_ICormanLispShutdown)
		*ppv = (ICormanLispShutdown*)this;
	else
		*ppv = 0;
	if (*ppv)
		((IUnknown*)*ppv)->AddRef();
	return *ppv ? S_OK : E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ConsoleCormanLispShutdownClient::AddRef()
{
	return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) ConsoleCormanLispShutdownClient::Release()
{
	if (InterlockedDecrement(&m_cRef) != 0)
		return m_cRef;
	delete this;
	return 0;
}

STDMETHODIMP ConsoleCormanLispShutdownClient::LispShutdown(const char* text, long numBytes)
{
	for (int i = 0; i < numBytes; i++)
	{
		putchar(text[i]);
	}
	fflush(stdout);
	exit(0);
	return S_OK;
}

STDMETHODIMP ConsoleCormanLispShutdownClient::Connect(IConnectionPoint* pConnectionPoint)
{
	return pConnectionPoint->Advise((ICormanLispStatusMessage*)this, &m_dwCookie);
}

STDMETHODIMP ConsoleCormanLispShutdownClient::Disconnect(IConnectionPoint* pConnectionPoint)
{
	return pConnectionPoint->Unadvise(m_dwCookie);
}

#else // LINUX — simplified main using direct C API

static void LoadFile(const char* filename)
{
	FILE* f = fopen(filename, "r");
	if (!f)
	{
		fprintf(stderr, "Cannot open file: %s\n", filename);
		return;
	}
	char buf[4096];
	while (fgets(buf, sizeof(buf), f))
	{
		cl_process_source(buf, strlen(buf));
	}
	fclose(f);
}

static char* getConsoleText(void)
{
	static char buf[4096];
	int pos = 0;
	int c;
	term_enable_raw();
	while (pos < (int)sizeof(buf) - 1)
	{
		c = getchar();
		if (c == EOF)
			break;
		if (c == '\r' || c == '\n')
		{
			buf[pos++] = '\n';
			putchar('\n');
			break;
		}
		if (c == 127 || c == '\b')
		{
			if (pos > 0)
			{
				pos--;
				putchar('\b');
				putchar(' ');
				putchar('\b');
			}
			continue;
		}
		buf[pos++] = c;
		putchar(c);
	}
	buf[pos] = 0;
	return buf;
}

static const char* find_default_image(const char* argv0)
{
	static char imgpath[MAX_PATH];
	char exepath[MAX_PATH];
	char* dir;

	// Resolve the executable's real path to get its directory.
	if (realpath(argv0, exepath) == NULL)
	{
		// realpath failed — try argv0 directly (may be relative).
		strncpy(exepath, argv0, MAX_PATH - 1);
		exepath[MAX_PATH - 1] = 0;
	}

	// Strip the executable name to get the directory.
	dir = strrchr(exepath, '/');
	if (dir)
		*dir = 0;
	else
		strcpy(exepath, "."); // argv0 had no slash — use cwd

	// Try executable's directory first, then cwd.
	snprintf(imgpath, MAX_PATH, "%s/CormanLisp.img", exepath);
	if (access(imgpath, R_OK) == 0)
		return imgpath;

	// Also try CormanLisp.img in cwd (for build-tree runs where
	// the image lives in the repo root, not under build/).
	if (access("CormanLisp.img", R_OK) == 0)
		return "CormanLisp.img";

	return NULL;
}

int main(int argc, char* argv[])
{
	int argidx = 1;
	const char* g_save_image = NULL;

	while (argidx < argc)
	{
		if (!strcmp(argv[argidx], "--help") || !strcmp(argv[argidx], "-help") || !strcmp(argv[argidx], "-h"))
		{
			printf("Usage: clconsole [OPTIONS]\n\n");
			printf("Corman Common Lisp console REPL (Linux port).\n\n");
			printf("Options:\n");
			printf("  --batch, -batch      Run in batch mode (process input, then exit)\n");
			printf("  -execute FILE         Load and evaluate FILE before starting REPL\n");
			printf("  -image FILE           Load Lisp image FILE on startup\n");
			printf("  --save-image FILE     Save heap to FILE after batch execution\n");
			printf("  --help, -help, -h     Show this help and exit\n");
			return 0;
		}
		else if (!strcmp(argv[argidx], "--batch") || !strcmp(argv[argidx], "-batch"))
		{
			g_batch_mode = 1;
		}
		else if (!strcmp(argv[argidx], "--save-image") && argidx + 1 < argc)
		{
			g_save_image = argv[++argidx];
		}
		else if ((!_stricmp(argv[argidx], "-image") || !strcmp(argv[argidx], "-image")) && argidx + 1 < argc)
		{
			g_image_name = argv[++argidx];
		}
		else if ((!_stricmp(argv[argidx], "-execute") || !strcmp(argv[argidx], "-execute")) && argidx + 1 < argc)
		{
			g_exec_file = argv[++argidx];
		}
		argidx++;
	}

	// Fallback: if no -image was given, search for CormanLisp.img next
	// to the executable (mirrors the Windows GetModuleFileName fallback).
	if (g_image_name == NULL)
		g_image_name = find_default_image(argv[0]);

	int ret = cl_initialize(&g_callbacks, g_image_name, CL_CONSOLE);
	if (ret != 0)
	{
		fprintf(stderr, "Failed to initialize Corman Lisp.\n");
		return 1;
	}

	if (g_exec_file)
	{
		LoadFile(g_exec_file);
	}

	// In batch mode, read remaining stdin and feed to Lisp
	if (g_batch_mode && !g_exec_file)
	{
		char buf[4096];
		while (fgets(buf, sizeof(buf), stdin))
			cl_process_source(buf, strlen(buf));
	}

	extern bool g_batch_input_done;
	g_batch_input_done = true;

	if (g_batch_mode)
	{
		cl_run();
		if (g_save_image)
		{
			fprintf(stderr, "Saving image to %s...\n", g_save_image);
			cl_save_image(g_save_image);
			fprintf(stderr, "Image saved.\n");
		}
		return 0;
	}

	printf("Corman Lisp (Linux port)\n");
	printf("Type :quit to exit.\n");

	int quitting = 0;
	while (!quitting)
	{
		char* input = getConsoleText();
		if (strlen(input) == 0)
			continue;

		if (!strcmp(input, ":quit\n") || !_stricmp(input, ":quit\n"))
			break;
		if (!strcmp(input, ":q\n") || !_stricmp(input, ":q\n"))
			break;

		if (!quitting)
			cl_process_source(input, strlen(input));
	}

	term_restore();
	return 0;
}
#endif // _WIN32/LINUX
