#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

static struct termios orig_term;

/* Disable raw mode.
 */
void disable_raw()
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
}
/* Enable raw mode.
 */
void enable_raw()
{
	struct termios raw;
	tcgetattr(STDIN_FILENO, &orig_term);
	atexit(disable_raw);

	/* setup raw mode */
	raw = orig_term;
	raw.c_iflag &= ~(IXON);
	raw.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
/* Simple Text Editor (prsed).
 */
int main()
{
	char c;
	enable_raw();
	while(read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
		if(iscntrl(c)) {
			printf("%x\n", c);
		} else {
			printf("%x (%c)\n", c, c);
		}
	}
	return 0;
}
