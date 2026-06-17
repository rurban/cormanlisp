//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		clboot.cpp
//		Contents:	Corman Lisp bootstrap client — Linux port.
//		            Replaces COM with direct API linking.
//		            Used for building the Lisp image file.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cormanlisp_api.h"

struct CormanLispCallbacks g_callbacks;

static void output_text(const char* text, long numChars)
{
	fwrite(text, 1, numChars, stdout);
}

int main(int argc, char* argv[])
{
	(void)argc; (void)argv;

	g_callbacks.output_text         = output_text;
	g_callbacks.set_message         = NULL;
	g_callbacks.set_default_message = NULL;
	g_callbacks.open_edit_window    = NULL;
	g_callbacks.add_menu            = NULL;
	g_callbacks.add_menu_item       = NULL;
	g_callbacks.open_url            = NULL;
	g_callbacks.replace_selection   = NULL;
	g_callbacks.lisp_shutdown       = NULL;

	if (cl_initialize(&g_callbacks, NULL, CL_CONSOLE) != 0) {
		fprintf(stderr, "Failed to initialize Corman Lisp\n");
		return 1;
	}

	cl_run();
	return 0;
}
