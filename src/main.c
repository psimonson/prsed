#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/* Control+k macro */
#define CTRL_KEY(k) ((k) & 0x1f)
/* Editor config structure */
struct editor_config {
	int screen_rows;
	int screen_cols;
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
/* Read input from user.
 */
char editor_read_key()
{
	int nread;
	char c;
	while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if(nread < 0 && errno != EAGAIN) die("read");
	}
	return c;
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
/* Draw rows for editor.
 */
void editor_draw_rows()
{
	int y;
	for(y = 0; y < e.screen_rows; y++) {
		write(STDOUT_FILENO, "~", 1);

		if(y < e.screen_rows-1)
			write(STDOUT_FILENO, "\r\n", 2);
	}
}
/* Clear the screen.
 */
void editor_refresh_screen()
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	editor_draw_rows();
	write(STDOUT_FILENO, "\x1b[H", 3);
}
/* Process key presses from user.
 */
void editor_process_key() {
	char c = editor_read_key();

	switch(c) {
	case CTRL_KEY('q'):
		write(STDOUT_FILENO, "\x1b[2J", 4);
		write(STDOUT_FILENO, "\x1b[H", 3);
		exit(0);
	break;
	default:
	break;
	}
}
/* Initialize the editor.
 */
void init_editor()
{
	if(get_window_size(&e.screen_rows, &e.screen_cols) < 0)
		die("get_window_size");
}
/* Simple Text Editor (prsed).
 */
int main()
{
	enable_raw();
	init_editor();
	while(1) {
		editor_refresh_screen();
		editor_process_key();
	}
	return 0;
}
