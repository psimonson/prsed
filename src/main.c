/**
 * @file main.c
 * @author Philip R. Simonson
 * @date 01/22/2020
 * @brief Simple text editor written in pure C.
 *************************************************************************
 */

#include <stdio.h>
#include "edit.h"

/* Simple Text Editor (prsed).
 */
int main(int argc, char **argv)
{
	enable_raw();
	init_editor();
	if(argc > 2) {
		fprintf(stderr, "Usage: %s [filename.ext]\n", argv[0]);
		return 1;
	}
	if(argc == 2) {
		editor_open(argv[1]);
	}
	editor_set_status("HELP: Ctrl-Q = quit");
	while(1) {
		editor_refresh_screen();
		editor_process_key();
	}
	return 0;
}
