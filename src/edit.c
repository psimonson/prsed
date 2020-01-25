/**
 * @file edit.c
 * @author Philip R. Simonson
 * @date 01/22/2020
 * @brief Simple text editor main file for PRS Edit.
 ************************************************************************
 */

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#define _BSD_SOURCE
#else
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "edit.h"

/* Defines to convert integers into strings */
#define VAR(x) #x
#define STR(x) VAR(x)
/* Editor version */
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define PRSED_VERSION STR(VERSION_MAJOR) "." STR(VERSION_MINOR)
/* Editor tab stop */
#define PRSED_TAB_STOP 4
/* Editor key presses required to quit */
#define PRSED_QUIT_TIMES 3
/* Editor foreground/background colors */
#define PRSED_FOREGROUND 33
#define PRSED_BACKGROUND 30
#define PRSED_DEFAULT_COLOR "\x1b[" STR(PRSED_BACKGROUND) \
	";" STR(PRSED_FOREGROUND) "m"
/* Control+key macro */
#define CTRL_KEY(k) ((k) & 0x1f)
/* Editor special keys */
enum {
	BACKSPACE = 127,
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};
/* Editor row structure */
typedef struct erow {
	int size;
	int rsize;
	char *data;
	char *render;
} erow;
/* Editor config structure */
struct editor_config {
	int cx, cy;
	int rx;
	int row_off;
	int col_off;
	int screen_rows;
	int screen_cols;
	int num_rows;
	erow *row;
	int dirty;
	int csize;
	char *cdata;
	char *filename;
	char status[80];
	time_t status_time;
	struct termios orig_termios;
};
/* Editor config definition */
struct editor_config e;
/* Exit out of the program and report an error.
 */
void die(const char *msg)
{
	write(STDOUT_FILENO, "\x1b[m", 3);
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	perror(msg);
	exit(1);
}
/* Disable raw mode.
 */
static void disable_raw()
{
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &e.orig_termios) < 0)
		die("tcsetattr");
}
/* Enable raw mode.
 */
void enable_raw()
{
	struct termios raw;
	if(tcgetattr(STDIN_FILENO, &e.orig_termios) < 0)
		die("tcgetattr");
	atexit(disable_raw);
	/* setup raw mode */
	raw = e.orig_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0)
		die("tcsetattr");
}
/* Gets the current position of the cursor.
 */
int get_cursor_pos(int *rows, int *cols)
{
	unsigned int i = 0;
	char buf[32];
	if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
	printf("\r\n");
	while(i < sizeof(buf)-1) {
		if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if(buf[i] == 'R') break;
		i++;
	}
	buf[i] = '\0';
	if(buf[0] != '\x1b' || buf[1] != '[') return -1;
	if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
	return 0;
}
/* Gets the size of the window.
 */
int get_window_size(int *rows, int *cols)
{
	struct winsize ws;
	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0 || ws.ws_col == 0) {
		if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
			return -1;
		return get_cursor_pos(rows, cols);
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}
/* Update the rendered string.
 */
void editor_update_row(erow *row)
{
	int i, idx = 0, tabs = 0;
	for(i = 0; i < row->size; i++)
		if(row->data[i] == '\t') tabs++;
	free(row->render);
	row->render = malloc(row->size+tabs*(PRSED_TAB_STOP-1)+1);
	for(i = 0; i < row->size; i++) {
		if(row->data[i] == '\t') {
			row->render[idx++] = ' ';
			while((idx % PRSED_TAB_STOP) != 0)
				row->render[idx++] = ' ';
		} else {
			row->render[idx++] = row->data[i];
		}
	}
	row->render[idx] = '\0';
	row->rsize = idx;
}
/* Append row to string.
 */
void editor_insert_row(int at, const char *s, size_t len)
{
	char *data;
	erow *row;
	if(at < 0 || at > e.num_rows) return;
	data = malloc(len+1);
	if(data == NULL) return;
	row = realloc(e.row, sizeof(erow)*(e.num_rows+1));
	if(row == NULL) { free(data); return; }
	e.row = row;
	memmove(&e.row[at+1], &e.row[at], sizeof(erow)*(e.num_rows-at));
	e.row[at].size = len;
	e.row[at].data = data;
	memcpy(e.row[at].data, s, len);
	e.row[at].data[len] = '\0';
	e.row[at].rsize = 0;
	e.row[at].render = NULL;
	editor_update_row(&e.row[at]);
	e.num_rows++;
	e.dirty = 1;
}
/* Insert character at given position in row.
 */
void editor_row_insert_char(erow *row, int at, int c)
{
	char *data;
	if(at < 0 || at > row->size) at = row->size;
	data = realloc(row->data, row->size+2);
	if(data == NULL) return;
	row->data = data;
	memmove(&row->data[at+1], &row->data[at], row->size-at+1);
	row->size++;
	row->data[at] = c;
	editor_update_row(row);
	e.dirty = 1;
}
/* Delete character at given position.
 */
void editor_row_delete_char(erow *row, int at)
{
	if(at < 0 || at >= row->size) return;
	memmove(&row->data[at], &row->data[at+1], row->size-at);
	row->size--;
	editor_update_row(row);
	e.dirty = 1;
}
/* Convert rows into one long string.
 */
char *editor_rows_to_string(int *buflen)
{
	int i, total_len = 0;
	char *buf, *p;
	for(i = 0; i < e.num_rows; i++)
		total_len += e.row[i].size+1;
	if(buflen != NULL) *buflen = total_len;
	buf = malloc(total_len);
	p = &buf[0];
	for(i = 0; i < e.num_rows; i++) {
		memcpy(p, e.row[i].data, e.row[i].size);
		p += e.row[i].size;
		*p = '\n';
		p++;
	}
	return buf;
}
/* Open given 'filename' in editor.
 */
void editor_open(const char *filename)
{
	char *line = NULL;
	size_t line_cap = 0;
	ssize_t line_len;
	FILE *fp;
	free(e.filename);
	e.filename = strdup(filename);
	fp = fopen(filename, "r");
	if(fp == NULL) die("editor_open()");
	while((line_len = getline(&line, &line_cap, fp)) > 0) {
		while(line_len > 0 && (line[line_len-1] == '\n' ||
				       line[line_len-1] == '\r'))
			line_len--;
		editor_insert_row(e.num_rows, line, line_len);
	}
	free(line);
	fclose(fp);
	e.dirty = 0;
}
/* Save text file to disk.
 */
void editor_save()
{
	char *buf;
	int len, fd;
	if(e.filename == NULL) {
		e.filename = editor_prompt("Save as (ESC to cancel): %s");
		if(e.filename == NULL) {
			editor_set_status("Save aborted!");
			return;
		}
	}
	buf = editor_rows_to_string(&len);
	if(buf == NULL) return;
	fd = open(e.filename, O_RDWR | O_CREAT, 0644);
	if(fd != -1) {
		if(ftruncate(fd, len) != -1) {
			if(write(fd, buf, len) == len) {
				close(fd);
				free(buf);
				e.dirty = 0;
				editor_set_status("%d bytes written to disk.",
					len);
				return;
			}
		}
		close(fd);
	}
	free(buf);
	editor_set_status("Can't save! I/O error: %s", strerror(errno));
}
/* Search for string in current text.
 */
void editor_search()
{
	extern int editor_row_rx_to_cx(erow *row, int rx);
	char *query = editor_prompt("Search (ESC to cancel): %s");
	if(query == NULL) return;
	{
		int i;
		for(i = 0; i < e.num_rows; i++) {
			erow *row = &e.row[i];
			char *match = strstr(row->render, query);
			if(match != NULL) {
				e.cy = i;
				e.cx = editor_row_rx_to_cx(row, match-row->render);
				e.row_off = e.num_rows;
				break;
			}
		}
	}
}
/* Free row after deletion.
 */
void editor_free_row(erow *row)
{
	free(row->render);
	free(row->data);
}
/* Delete row from buffer.
 */
void editor_delete_row(int at)
{
	if(at < 0 || at >= e.num_rows) return;
	editor_free_row(&e.row[at]);
	memmove(&e.row[at], &e.row[at+1], sizeof(erow)*(e.num_rows-at-1));
	e.num_rows--;
	e.dirty = 1;
}
/* Editor free last line that was deleted.
 */
void editor_delete()
{
	if(e.cdata == NULL) return;
	free(e.cdata);
	e.cdata = NULL;
	e.csize = 0;
}
/* Editor insert line from buffer.
 */
void editor_insert()
{
	if(e.cdata == NULL) return;
	editor_insert_row(e.cy, e.cdata, e.csize);
	editor_delete();
}
/* Append a string to the end of a row.
 */
void editor_row_append_string(erow *row, char *s, size_t len)
{
	char *p;
	p = realloc(row->data, row->size+len+1);
	if(p == NULL) return;
	row->data = p;
	memcpy(&row->data[row->size], s, len);
	row->size += len;
	row->data[row->size] = '\0';
	editor_update_row(row);
	e.dirty = 1;
}
/* Structure for append buffer. */
struct abuf {
	char *b;
	int len;
};
/* Initial append buffer */
#define ABUF_INIT	{NULL, 0}
/* Create/Append to append buffer.
 */
void ab_append(struct abuf *ab, const char *s, int len)
{
	char *new = realloc(ab->b, ab->len+len);
	if(new == NULL) return;
	ab->b = new;
	memcpy(&ab->b[ab->len], s, len);
	ab->len += len;
}
/* Free append buffer.
 */
void ab_free(struct abuf *ab)
{
	free(ab->b);
}
/* Insert character into row at index.
 */
void editor_insert_char(int c)
{
	if(e.cy == e.num_rows) {
		editor_insert_row(e.num_rows, "", 0);
	}
	editor_row_insert_char(&e.row[e.cy], e.cx, c);
	e.cx++;
}
/* Insert a new line.
 */
void editor_insert_line()
{
	if(e.cx == 0) {
		editor_insert_row(e.cy, "", 0);
	} else {
		erow *row = &e.row[e.cy];
		editor_insert_row(e.cy+1, &row->data[e.cx], row->size-e.cx);
		row = &e.row[e.cy];
		row->size = e.cx;
		row->data[row->size] = '\0';
		editor_update_row(row);
	}
	e.cy++;
	e.cx = 0;
}
/* Delete character from row at index.
 */
void editor_delete_char()
{
	if(e.cy == e.num_rows) return;
	if(e.cx == 0 && e.cy == 0) return;
	erow *row = &e.row[e.cy];
	if(e.cx > 0) {
		editor_row_delete_char(row, e.cx-1);
		e.cx--;
	} else {
		e.cx = e.row[e.cy-1].size;
		editor_row_append_string(&e.row[e.cy-1], row->data, row->size);
		editor_delete_row(e.cy);
		e.cy--;
	}
}
/* Draw rows for editor.
 */
void editor_draw_rows(struct abuf *ab)
{
	int y;
	for(y = 0; y < e.screen_rows; y++) {
		int file_row = y+e.row_off;
		if(file_row >= e.num_rows) {
			if(e.num_rows == 0 && y == e.screen_rows/3) {
				char welcome[80];
				int welcome_len;
				int padding;
				welcome_len = snprintf(welcome, sizeof(welcome),
				    "PRS Edit -- Version %s", PRSED_VERSION);
				if(welcome_len > e.screen_cols)
					welcome_len = e.screen_cols;
				padding = (e.screen_cols-welcome_len)/2;
				if(padding != 0) {
					ab_append(ab, "~", 1);
					padding--;
				}
				while(padding-- != 0) ab_append(ab, " ", 1);
				ab_append(ab, welcome, welcome_len);
			} else {
				ab_append(ab, "~", 1);
			}
		} else {
			int len = e.row[file_row].rsize-e.col_off;
			if(len < 0) len = 0;
			if(len > e.screen_cols) len = e.screen_cols;
			ab_append(ab, &e.row[file_row].render[e.col_off], len);
		}

		ab_append(ab, "\x1b[K", 3);
		ab_append(ab, "\r\n", 2);
	}
}
/* Calculate render index from character index.
 */
int editor_row_cx_to_rx(erow *row, int cx)
{
	int i, rx = 0;
	for(i = 0; i < cx; i++) {
		if(row->data[i] == '\t') {
			rx += (PRSED_TAB_STOP - 1)-(rx % PRSED_TAB_STOP);
		}
		rx++;
	}
	return rx;
}
/* Calculate render index to character index.
 */
int editor_row_rx_to_cx(erow *row, int rx)
{
	int cx, cur_rx = 0;
	for(cx = 0; cx < row->size; cx++) {
		if(row->data[cx] == '\t') {
			cur_rx += (PRSED_TAB_STOP - 1)-(cur_rx % PRSED_TAB_STOP);
		}
		cur_rx++;
		if(cur_rx > rx) return cx;
	}
	return cx;
}
/* Scroll the editor screen through the file.
 */
void editor_scroll()
{
	/* Handle tab stops */
	e.rx = 0;
	if(e.cy < e.num_rows) {
		e.rx = editor_row_cx_to_rx(&e.row[e.cy], e.cx);
	}
	/* vertical scrolling */
	if(e.cy < e.row_off) {
		e.row_off = e.cy;
	}
	if(e.cy >= e.row_off+e.screen_rows) {
		e.row_off = e.cy-e.screen_rows+1;
	}
	/* horizontal scrolling */
	if(e.cx < e.col_off) {
		e.col_off = e.cx;
	}
	if(e.cx >= e.col_off+e.screen_cols) {
		e.col_off = e.cx-e.screen_cols+1;
	}
}
/* Draw a status bar at the bottom of the screen.
 */
void editor_draw_status(struct abuf *ab)
{
	char status[80], rstatus[80];
	int len = 0, rlen = 0;
	ab_append(ab, "\x1b[7m", 4);
	len = snprintf(status, sizeof(status), "[%.20s]%s - %d lines",
	  e.filename ? e.filename : "No Name",
	  e.dirty ? " (modified)" : "", e.num_rows);
	rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
		(e.num_rows > 0) ? e.cy+1 : e.cy, e.num_rows);
	if(len > e.screen_cols) len = e.screen_cols;
	ab_append(ab, status, len);
	while(len < e.screen_cols) {
		if(e.screen_cols-len == rlen) {
			ab_append(ab, rstatus, rlen);
			break;
		} else {
			ab_append(ab, " ", 1);
			len++;
		}
	}
	ab_append(ab, "\x1b[m", 3);
	ab_append(ab, "\r\n", 2);
}
/* Draws the message bar on the screen.
 */
void editor_draw_message(struct abuf *ab)
{
	int len = strlen(e.status);
	ab_append(ab, PRSED_DEFAULT_COLOR, strlen(PRSED_DEFAULT_COLOR));
	ab_append(ab, "\x1b[K", 3);
	if(len > e.screen_cols) len = e.screen_cols;
	if(len > 0 && time(NULL)-e.status_time < 5) {
		ab_append(ab, e.status, len);
		while(len < e.screen_cols) {
			ab_append(ab, " ", 1);
			len++;
		}
		ab_append(ab, "\x1b[m", 3);
	}
}
/* Clear the screen.
 */
void editor_refresh_screen()
{
	struct abuf ab = ABUF_INIT;
	char buf[32];
	editor_scroll();
	ab_append(&ab, PRSED_DEFAULT_COLOR, strlen(PRSED_DEFAULT_COLOR));
	ab_append(&ab, "\x1b[?25l", 6);
	ab_append(&ab, "\x1b[H", 3);
	editor_draw_rows(&ab);
	editor_draw_status(&ab);
	editor_draw_message(&ab);
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
			(e.cy-e.row_off)+1, (e.rx-e.col_off)+1);
	ab_append(&ab, buf, strlen(buf));
	ab_append(&ab, "\x1b[?25h", 6);
	write(STDOUT_FILENO, ab.b, ab.len);
	ab_free(&ab);
}
/* Draw a status bar to display common hot keys.
 */
void editor_set_status(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(e.status, sizeof(e.status), fmt, ap);
	va_end(ap);
	e.status_time = time(NULL);
}
/* Read input from user.
 */
int editor_read_key()
{
	int nread;
	char c;
	while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if(nread < 0 && errno != EAGAIN) die("read");
	}
	if(c == '\x1b') {
		char seq[3];

		if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

		if(seq[0] == '[') {
			if(seq[1] >= '0' && seq[1] <= '9') {
				if(read(STDIN_FILENO, &seq[2], 1) != 1)
					return '\x1b';
				if(seq[2] == '~') {
					switch(seq[1]) {
					case '1': return HOME_KEY;
					case '3': return DEL_KEY;
					case '4': return END_KEY;
					case '5': return PAGE_UP;
					case '6': return PAGE_DOWN;
					case '7': return HOME_KEY;
					case '8': return END_KEY;
					default: break;
					}
				}
			} else {
				switch(seq[1]) {
				case 'A': return ARROW_UP;
				case 'B': return ARROW_DOWN;
				case 'C': return ARROW_RIGHT;
				case 'D': return ARROW_LEFT;
				case 'H': return HOME_KEY;
				case 'F': return END_KEY;
				default: break;
				}
			}
		} else if(seq[0] == 'O') {
			switch(seq[1]) {
			case 'H': return HOME_KEY;
			case 'F': return END_KEY;
			default: break;
			}
		}
		return '\x1b';
	} else {
		return c;
	}
}
/* Prompt user for input.
 */
char *editor_prompt(const char *msg)
{
#define MAXBUF 128
	static char buf[MAXBUF];
	size_t i = 0;
	int c;
	do {
		editor_set_status(msg, buf);
		editor_refresh_screen();

		c = editor_read_key();
		if(c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
			if(i != 0) buf[--i] = '\0';
		} else if(c == '\x1b') {
			editor_set_status("", 0);
			return NULL;
		} else if(c == '\r') {
			if(i != 0) {
				editor_set_status("", 0);
				return &buf[0];
			}
		} else if(!iscntrl(c) && c < 128) {
			if(i < MAXBUF-1) {
				buf[i++] = c;
				buf[i] = '\0';
			}
		}
	} while(i < MAXBUF-1);
	return &buf[0];
#undef MAXBUF
}
/* Move the cursor with keys 'a', 'd', 'w', 's'.
 */
void editor_move_cursor(int key)
{
	erow *row = (e.cy >= e.num_rows) ? NULL : &e.row[e.cy];

	switch(key) {
	case ARROW_LEFT:
		if(e.cx != 0) {
			e.cx--;
		} else if(e.cy > 0) {
			e.cy--;
			e.cx = e.row[e.cy].size;
		}
	break;
	case ARROW_RIGHT:
		if(row && e.cx < row->size) {
			e.cx++;
		} else if(row && e.cx == row->size) {
			e.cy++;
			e.cx = 0;
		}
	break;
	case ARROW_UP:
		if(e.cy != 0) {
			e.cy--;
		}
	break;
	case ARROW_DOWN:
		if(e.cy < e.num_rows) {
			e.cy++;
		}
	break;
	default:
	break;
	}

	row = (e.cy >= e.num_rows) ? NULL : &e.row[e.cy];
	if(row != NULL && e.cx > row->size) {
		e.cx = row->size;
	}
}
/* Process key presses from user.
 */
void editor_process_key() {
	static int quit_times = PRSED_QUIT_TIMES;
	int c = editor_read_key();
	void reset_editor(void);

	switch(c) {
	case '\r':
		editor_insert_line();
	break;
	case CTRL_KEY('q'):
		if(e.dirty && quit_times > 0) {
			editor_set_status(
			    "WARNING!!! File has unsaved changes. "
			    "Press Ctrl-Q %d more times to quit.", quit_times);
			quit_times--;
			return;
		}
		write(STDOUT_FILENO, "\x1b[m", 3);
		write(STDOUT_FILENO, "\x1b[2J", 4);
		write(STDOUT_FILENO, "\x1b[H", 3);
		editor_delete();
		exit(0);
	break;
	case CTRL_KEY('s'):
		editor_save();
	break;
	case CTRL_KEY('f'):
		editor_search();
	break;
	case CTRL_KEY('u'):
		editor_insert();
	break;
	case CTRL_KEY('k'):
		if(e.num_rows > 0 && e.num_rows > e.cy && e.row[e.cy].data != NULL) {
			free(e.cdata);
			e.cdata = strdup(e.row[e.cy].data);
			e.csize = strlen(e.cdata);
			editor_delete_row(e.cy);
		}
	break;
	case CTRL_KEY('n'):
		reset_editor();
	break;
	case CTRL_KEY('o'): {
		char *filename = editor_prompt("File name (ESC to cancel): %s");
		if(filename == NULL) break;
		reset_editor();
		editor_open(filename);
	} break;
	case HOME_KEY:
		e.cx = 0;
	break;
	case END_KEY:
		if(e.cy < e.num_rows)
			e.cx = e.row[e.cy].size;
	break;
	case BACKSPACE:
	case CTRL_KEY('h'):
	case DEL_KEY:
		if(c == DEL_KEY) editor_move_cursor(ARROW_RIGHT);
		editor_delete_char();
	break;
	case PAGE_UP:
	case PAGE_DOWN: {
		int times;

		if(c == PAGE_UP) {
			e.cy = e.row_off;
		} else if(c == PAGE_DOWN) {
			e.cy = e.row_off+e.screen_rows-1;
			if(e.cy > e.num_rows) e.cy = e.num_rows;
		}

		times = e.screen_rows;
		while(times-- != 0)
			editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
	} break;
	case ARROW_UP:
	case ARROW_DOWN:
	case ARROW_LEFT:
	case ARROW_RIGHT:
		editor_move_cursor(c);
	break;
	case CTRL_KEY('l'):
	case '\x1b':
	break;
	default:
		editor_insert_char(c);
	break;
	}

	quit_times = PRSED_QUIT_TIMES;
}
/* Initialize the editor.
 */
void init_editor()
{
	/* setup variables for startup */
	e.cx = 0;
	e.cy = 0;
	e.rx = 0;
	e.row_off = 0;
	e.col_off = 0;
	e.num_rows = 0;
	e.row = NULL;
	e.dirty = 0;
	e.csize = 0;
	e.cdata = NULL;
	e.filename = NULL;
	e.status[0] = '\0';
	e.status_time = 0;

	if(get_window_size(&e.screen_rows, &e.screen_cols) < 0)
		die("get_window_size");
	e.screen_rows -= 2;
}
/* Reset editor free all data and re-initialize.
 */
void reset_editor()
{
	int i;
	for(i = 0; i < e.num_rows; i++) {
		editor_free_row(&e.row[i]);
	}
	free(e.row);
	free(e.cdata);
	free(e.filename);
	init_editor();
}
