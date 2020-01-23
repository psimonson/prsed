#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

/* Control+k macro */
#define CTRL_KEY(k) ((k) & 0x1f)
/* Editor config structure */
struct editor_config {
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
/* Draw rows for editor.
 */
void editor_draw_rows()
{
	int y;
	for(y = 0; y < 24; y++)
		write(STDOUT_FILENO, "~\r\n", 3);
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
/* Read input from user.
 */
static char editor_read_key()
{
	int nread;
	char c;
	while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if(nread < 0 && errno != EAGAIN) die("read");
	}
	return c;
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
/* Simple Text Editor (prsed).
 */
int main()
{
	enable_raw();
	while(1) {
		editor_refresh_screen();
		editor_process_key();
	}
	return 0;
}
