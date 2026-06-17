//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		clconsole.cpp
//		Contents:	Corman Lisp console application client — Linux port.
//		            Replaces COM with direct API linking.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <termios.h>

#include "cormanlisp_api.h"

// ---- Terminal input (replaces conio.h) ----

static struct termios g_orig_termios;
static int g_term_raw = 0;

static void term_enable_raw(void)
{
	if (g_term_raw) return;
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
	if (!g_term_raw) return;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
	g_term_raw = 0;
}

static char* getConsoleText(void)
{
	static char buf[4096];
	int pos = 0;
	int c;
	term_enable_raw();
	while (pos < (int)sizeof(buf) - 1) {
		c = getchar();
		if (c == EOF) break;
		if (c == '\r' || c == '\n') {
			buf[pos++] = '\n';
			putchar('\n');
			break;
		}
		if (c == 127 || c == '\b') {
			if (pos > 0) { pos--; putchar('\b'); putchar(' '); putchar('\b'); }
			continue;
		}
		buf[pos++] = c;
		putchar(c);
	}
	buf[pos] = 0;
	return buf;
}

// ---- Callbacks (replaces COM ConsoleCormanLispClient) ----

static void output_text(const char* text, long numChars)
{
	fwrite(text, 1, numChars, stdout);
	fflush(stdout);
}

static void set_message(const char* text)
{
	fprintf(stderr, "\r[%s]\n", text);
}

static void set_default_message(void)
{
}

static void open_edit_window(const char* file, void** wnd)
{
	(void)file; (void)wnd;
}

static void add_menu(const char* menuName)
{
	(void)menuName;
}

static void add_menu_item(const char* menuName, const char* menuItem)
{
	(void)menuName; (void)menuItem;
}

static void open_url(const char* url, void** wnd)
{
	(void)url; (void)wnd;
}

static void replace_selection(const char* text, long numChars)
{
	(void)text; (void)numChars;
}

static void lisp_shutdown(const char* text, long numChars)
{
	if (text && numChars > 0)
		fwrite(text, 1, numChars, stderr);
	fprintf(stderr, "\nLisp has shut down.\n");
}

static struct CormanLispCallbacks g_callbacks = {
	output_text,
	set_message,
	set_default_message,
	open_edit_window,
	add_menu,
	add_menu_item,
	open_url,
	replace_selection,
	lisp_shutdown
};

// ---- Main ----

static const char* g_image_name = NULL;
static const char* g_exec_file = NULL;

static void load_file(const char* filename)
{
	FILE* f = fopen(filename, "r");
	if (!f) {
		fprintf(stderr, "Cannot open file: %s\n", filename);
		return;
	}
	char buf[4096];
	while (fgets(buf, sizeof(buf), f)) {
		cl_process_source(buf, strlen(buf));
	}
	fclose(f);
}

int main(int argc, char* argv[])
{
	int argidx = 1;

	while (argidx < argc) {
		if (!strcmp(argv[argidx], "-image") && argidx + 1 < argc) {
			g_image_name = argv[++argidx];
		} else if (!strcmp(argv[argidx], "-execute") && argidx + 1 < argc) {
			g_exec_file = argv[++argidx];
		} else {
			fprintf(stderr, "Usage: %s [-image <path>] [-execute <file>]\n", argv[0]);
			return 1;
		}
		argidx++;
	}

	// Initialize Lisp
	int ret = cl_initialize(&g_callbacks, g_image_name, CL_CONSOLE);
	if (ret != 0) {
		fprintf(stderr, "Failed to initialize Corman Lisp.\n");
		return 1;
	}

	// Execute a file if specified
	if (g_exec_file) {
		load_file(g_exec_file);
	}

	// REPL loop
	printf("Corman Lisp (Linux port)\n");
	printf("Type :quit to exit.\n");

	int quitting = 0;
	while (!quitting) {
		char* input = getConsoleText();
		if (strlen(input) == 0) continue;

		if (!strcmp(input, ":quit\n")) break;
		if (!strcmp(input, ":q\n")) break;

		if (!quitting)
			cl_process_source(input, strlen(input));
	}

	term_restore();
	return 0;
}
