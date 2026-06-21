#ifndef _WIN32
#include "platform.h"
#endif

//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		cormanlisp_api.h
//		Contents:	Plain C API for Corman Lisp (replaces COM ICormanLisp).
//		            Linux port: clients link directly to libcormanlisp.so.
//

#ifndef CORMANLISP_API_H
#define CORMANLISP_API_H
#ifdef LINUX
#define CL_API __attribute__((visibility("default")))
#else
#define CL_API __declspec(dllexport)
#endif

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

	// ---- Client callbacks (replaces ICormanLispTextOutput/StatusMessage/Shutdown) ----

	typedef void (*CL_OutputText)(const char* text, long numChars);
	typedef void (*CL_SetMessage)(const char* text);
	typedef void (*CL_SetDefaultMessage)(void);
	typedef void (*CL_OpenEditWindow)(const char* file, void** wnd);
	typedef void (*CL_AddMenu)(const char* menuName);
	typedef void (*CL_AddMenuItem)(const char* menuName, const char* menuItem);
	typedef void (*CL_OpenURL)(const char* url, void** wnd);
	typedef void (*CL_ReplaceSelection)(const char* text, long numChars);
	typedef void (*CL_LispShutdown)(const char* text, long numChars);

	// All callbacks in one struct (passed to cl_initialize)
	struct CormanLispCallbacks
	{
		CL_OutputText output_text;
		CL_SetMessage set_message;
		CL_SetDefaultMessage set_default_message;
		CL_OpenEditWindow open_edit_window;
		CL_AddMenu add_menu;
		CL_AddMenuItem add_menu_item;
		CL_OpenURL open_url;
		CL_ReplaceSelection replace_selection;
		CL_LispShutdown lisp_shutdown;
	};

	// ---- Server API (replaces ICormanLisp) ----

	enum CormanLispClientType
	{
		CL_WIN_APP = 0,
		CL_CONSOLE = 1,
		CL_IDE = 2
	};

	// Initialize the Lisp runtime. callback may be NULL for headless.
	// Returns 0 on success, nonzero on failure.
	int cl_initialize(const struct CormanLispCallbacks* cb, const char* imageName, int clientType);

	// Extended initialize with heap size customization.
	int cl_initialize_ex(const struct CormanLispCallbacks* cb, const char* imageName, int clientType, int heapReserve,
						 int heapInitialSize, int ephemeralHeap1Size, int ephemeralHeap2Size);

	// Run the Lisp REPL loop. Returns when Lisp exits.
	void cl_run(void);

	// Process Lisp source text.
	void cl_process_source(const char* text, long numChars);

	// Get the number of active Lisp threads.
	long cl_get_num_threads(void);

	// Signal a user exception (Ctrl-C).
	void cl_user_exception(void);

	// Abort the current Lisp thread.
	void cl_abort_thread(void);

	// Direct call support (replaces ICormanLispDirectCall).
	// Bless a non-Lisp thread for direct Lisp calls.
	void cl_bless_thread(void);
	void cl_unbless_thread(void);

	// Get a compiled Lisp function pointer by name.
	int cl_get_function_address(const wchar_t* functionName, const wchar_t* packageName, void** funcptr);

	// Handle a structured exception from within Lisp code.
	int cl_handle_structured_exception(long exception, void* info, long* result);

	// User info queries.
	int cl_get_current_user_name(char* buf, size_t* len);
	int cl_get_current_user_profile_directory(char* buf, size_t* len);
	int cl_get_current_user_personal_directory(char* buf, size_t* len);

	// Get the number of times the image has been loaded.
	long cl_get_image_loads_count(void);

#ifdef __cplusplus
}
#endif

#endif // CORMANLISP_API_H
