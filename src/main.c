/**
 * @file main.c
 * @author Philip R. Simonson
 * @date 01/25/2020
 * @brief Simple text editor written in pure C.
 *************************************************************************
 */

#ifdef __linux
#define _GNU_SOURCE
#elif __unix >= 1
#if defined(__FreeBSD__) || defined(__OpenBSD__)
#define _BSD_SOURCE
#else
#define _GNU_SOURCE
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

/* Editor version */
#define PRSED_VERSION "1.0"
/* Control+k macro */
#define CTRL_KEY(k) ((k) & 0x1f)
/* Editor special keys */
enum {
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
	char *data;
} erow;
/* Editor config structure */
struct editor_config {
	int cx, cy;
	int screen_rows;
	int screen_cols;
	int num_rows;
	erow *row;
	struct termios orig_termios;
};
/* Editor config definition */
struct editor_config e;
/* Exit out of the program and report an error.
 */
void die(const char *msg)
{
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
/* Append row to string.
 */
static void editor_append_row(char *s, size_t len)
{
	int at = e.num_rows;
	erow *new;
	char *str;
	str = malloc(len+1);
	if(str == NULL) return;
	new = realloc(e.row, sizeof(erow)*(e.num_rows+1));
	if(new == NULL) return;
	e.row = new;
	e.row[at].size = len;
	e.row[at].data = str;
	memcpy(e.row[at].data, s, len);
	e.row[at].data[len] = '\0';
	e.num_rows++;
}
/* Open given 'filename' in editor.
 */
void editor_open(const char *filename)
{
	char *line = NULL;
	size_t line_cap = 0;
	ssize_t line_len;
	FILE *fp;

	fp = fopen(filename, "r");
	if(fp == NULL) die("editor_open()");
	while((line_len = getline(&line, &line_cap, fp)) > 0) {
		while(line_len > 0 && (line[line_len-1] == '\n' ||
				       line[line_len-1] == '\r'))
			line_len--;
		editor_append_row(line, line_len);
	}
	free(line);
	fclose(fp);
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
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}
/* Free append buffer.
 */
void ab_free(struct abuf *ab)
{
	free(ab->b);
}
/* Draw rows for editor.
 */
void editor_draw_rows(struct abuf *ab)
{
	int y;
	for(y = 0; y < e.screen_rows; y++) {
		if(y >= e.num_rows) {
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
			int len = e.row[y].size;
			if(len > e.screen_cols) len = e.screen_cols;
			ab_append(ab, e.row[y].data, len);
		}

		ab_append(ab, "\x1b[K", 3);
		if(y < e.screen_rows-1)
			ab_append(ab, "\r\n", 2);
	}
}
/* Clear the screen.
 */
void editor_refresh_screen()
{
	struct abuf ab = ABUF_INIT;
	char buf[32];
	ab_append(&ab, "\x1b[?25l", 6);
	ab_append(&ab, "\x1b[H", 3);
	editor_draw_rows(&ab);
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", e.cy+1, e.cx+1);
	ab_append(&ab, buf, strlen(buf));
	ab_append(&ab, "\x1b[?25h", 6);
	write(STDOUT_FILENO, ab.b, ab.len);
	ab_free(&ab);
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
/* Move the cursor with keys 'a', 'd', 'w', 's'.
 */
void editor_move_cursor(int key)
{
	switch(key) {
	case ARROW_LEFT:
		if(e.cx != 0) {
			e.cx--;
		}
	break;
	case ARROW_RIGHT:
		if(e.cx != e.screen_cols-1) {
			e.cx++;
		}
	break;
	case ARROW_UP:
		if(e.cy != 0) {
			e.cy--;
		}
	break;
	case ARROW_DOWN:
		if(e.cy != e.screen_rows-1) {
			e.cy++;
		}
	break;
	default:
	break;
	}
}
/* Process key presses from user.
 */
void editor_process_key() {
	int c = editor_read_key();

	switch(c) {
	case CTRL_KEY('q'):
		write(STDOUT_FILENO, "\x1b[2J", 4);
		write(STDOUT_FILENO, "\x1b[H", 3);
		exit(0);
	break;
	case HOME_KEY:
		e.cx = 0;
	break;
	case END_KEY:
		e.cx = e.screen_cols-1;
	break;
	case PAGE_UP:
	case PAGE_DOWN: {
		int times = e.screen_rows;
		while(times-- != 0)
			editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
	} break;
	case ARROW_UP:
	case ARROW_DOWN:
	case ARROW_LEFT:
	case ARROW_RIGHT:
		editor_move_cursor(c);
	break;
	default:
	break;
	}
}
/* Initialize the editor.
 */
void init_editor()
{
	/* setup variables for startup */
	e.cx = 0;
	e.cy = 0;
	e.num_rows = 0;
	e.row = NULL;

	if(get_window_size(&e.screen_rows, &e.screen_cols) < 0)
		die("get_window_size");
}
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
	while(1) {
		editor_refresh_screen();
		editor_process_key();
	}
	return 0;
}
