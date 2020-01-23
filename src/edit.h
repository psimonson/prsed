/**
 * @file edit.h
 * @author Philip R. Simonson
 * @date 01/22/2020
 * @brief Simple text editor header file.
 ********************************************************************
 */

#ifndef EDIT_H
#define EDIT_H

/* Enable raw mode in terminal */
void enable_raw();
/* Initialise editor */
void init_editor();

/* Open file for reading/writing. */
void editor_open(const char *filename);
/* Refresh screen for editor. */
void editor_refresh_screen();
/* Process input from user. */
void editor_process_key();

#endif
