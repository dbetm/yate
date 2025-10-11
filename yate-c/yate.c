/*** includes ***/

// If your compiler complains about getline(), you may need to define a feature test macro
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>


/* defines */

#define YATE_VERSION "0.0.1"
#define KILO_TAB_STOP 4

/* The CTRL_KEY macro bitwise-ANDs a character with the value 00011111, in binary. 
(In C, you generally specify bitmasks using hexadecimal, since C doesn’t have binary literals)

It sets the upper 3 bits of the character to 0. This mirrors what the Ctrl key does in the terminal: 
it strips bits 5 and 6 from whatever key you press in combination with Ctrl, and sends that.
Example:
    'q' = 113 in decimal, 1110001 in binary
    113 & 31
    01110001 & 
    00011111
=   00010001
= 17 (which is the equivalent to Ctrl+q)
*/
#define CTRL_KEY(k) ((k) & 0x1f)

/* By setting the first constant in the enum to 1000, the rest of the constants get incrementing values 
of 1001, 1002, 1003, and so on. */
enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY, // the escape sequence: <esc>[3~
    HOME_KEY, // The Home key could be sent as <esc>[1~, <esc>[7~, <esc>[H, or <esc>OH
    END_KEY, // The End key could be sent as <esc>[4~, <esc>[8~, <esc>[F, or <esc>OF
    PAGE_UP, // escape sequence: <esc>[5~
    PAGE_DOWN // escape sequence: <esc>[6~
};


/*** data ***/
typedef struct errow { // editor row
    int size;
    int rsize; // size of the contents of render
    char *chars;
    char *render;
} erow;
struct editorConfig {
    int cx, cy; // horizontal coordinate and vertical coordinate
    int rx; // it'll be an index into the render field. If there are no tabs on the current line, then E.rx will be the same as E.cx. If there are tabs, then E.rx will be greater than E.cx
    int rowoff; // keep track of what row of the file the user is currently scrolled to
    int coloff; // keep track of what column of the file the user is currently scrolled to
    struct termios original_terminal;
    int screenrows;
    int screencols;
    int numrows;
    erow *row; // must be a pointer in order to save multiple line
    struct termios orig_termios;
};
struct editorConfig E;

/*** terminal ***/
void die(const char *s) {
    // reset screen
    write(STDOUT_FILENO, "\x1b[2J", 4); // clear scren
    write(STDERR_FILENO, "\x1b[H", 3); // relocate cursor position

    /*Most C library functions that fail will set the global errno variable to indicate what the error was. 
    perror() looks at the global errno variable and prints a descriptive error message for it.

    It also prints the string given to it before it prints the error message
    */
    perror(s);
    exit(1);
}


void disableRawMode() {
    // we want to disable raw mode when exiting the program in order to keep user with their Terminal "stable".
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.original_terminal) == -1) {
        die("tcsetattr");
    }
}


void enableRawMode() {
    /*By default your terminal starts in canonical mode, also called cooked mode.
    In this mode, keyboard input is only sent to your program when the user presses Enter.

    But it does not work well for programs with more complex user interfaces, like text editors. 
    We want to process each keypress as it comes in, so we can respond to it immediately. That's called
    the raw mode.
    */

    // Update terminal's attributes to turn off echoing
    /*The ECHO feature causes each key you type to be printed to the terminal, 
    so you can see what you’re typing. This is useful in canonical mode, but really gets 
    in the way when we are trying to carefully render a user interface in raw mode. 
    So we turn it off.

    There is an ICANON flag that allows us to turn off canonical mode. This means we will finally be 
    reading input byte-by-byte, instead of line-by-line.
    */

    if(tcgetattr(STDIN_FILENO, &E.original_terminal) == -1) die("tcsetattr");
    // restore to the original value
    atexit(disableRawMode);

    struct termios raw = E.original_terminal;
    // Update ECHO and CANONICAL mode flags
    /* We want too to handle some key combinations:
        1. Turn off ctrl+c (SIGINT) and ctrl+z (SIGTSTP) signals, which terminates or suspend the program.
        2. Turn off Ctrl-S and Ctrl-Q which are used for software flow control.
            Ctrl-S stops data from being transmitted to the terminal until you press Ctrl-Q.
            This originates in the days when you might want to pause the transmission of data to let
            a device like a printer catch up. We can use IXON flag.
        3. Disable Ctrl-V. When you type Ctrl-V, the terminal waits for you to type another character and
            then sends that character literally. For example, before we disabled Ctrl-C, you might’ve been
            able to type Ctrl-V and then Ctrl-C to input a 3 byte. We can turn off this feature using
            the IEXTEN flag.
        4. Fix CTRL-M. Ctrl-M is weird: it’s being read as 10, when we expect it to be read as 13, since it
            is the 13th letter of the alphabet, and Ctrl-J already produces a 10. What else produces 10?
            The Enter key does. ICRNL comes from <termios.h>. The I stands for “input flag”, CR stands for 
            “carriage return”, and NL stands for “new line”.
            It turns out that the terminal is helpfully translating any carriage returns (13, '\r') inputted
            by the user into newlines (10, '\n').
        5. Turn off all output processing. The terminal requires both of these characters in order to start a
            new line of text. The carriage return moves the cursor back to the beginning of the current line,
            and the newline moves the cursor down a line (these operations originated in the days of typewriters and teletypes).
            We will turn off all output processing features by turning off the OPOST flag. In practice,
            the "\n" to "\r\n" translation is likely the only output processing feature turned on by default.
        6. Miscellaneous flags. BRKINT, INPCK, ISTRIP, and CS8 all come from <termios.h>. This step probably
            won’t have any observable effect for you, because these flags are either already turned off,
            or they don’t really apply to modern terminal emulators.
            - When BRKINT is turned on, a break condition will cause a SIGINT signal to be sent to the program, like pressing Ctrl-C.
            - INPCK enables parity checking, which doesn’t seem to apply to modern terminal emulators.
            - ISTRIP causes the 8th bit of each input byte to be stripped, meaning it will set it to 0. This is probably already turned off.
            - CS8 is not a flag, it is a bit mask with multiple bits, which we set using the bitwise-OR (|) operator unlike all the flags we are turning off.
                It sets the character size (CS) to 8 bits per byte.
    */
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); // local flags
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP); // input flags
    raw.c_oflag &= ~(OPOST); // output flags
    raw.c_cflag |= (CS8); // control flags
    raw.c_cc[VMIN] = 0; // The VMIN value sets the minimum number of bytes of input needed before read() can return
    raw.c_cc[VTIME] = 1; // The VTIME value sets the maximum amount of time to wait before read() returns. It is in tenths of a second, or 100 milliseconds.

    if(tcsetattr(STDERR_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int editorReadKey() {
    /* Wait for one keypress, and return it */
    int nread;
    char c;

    while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if(nread == -1 && errno != EAGAIN) die("read");
    }

    //printf("'%c'", c);

    // process arrow keys
    if(c == '\x1b') {
        char seq[3];

        if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if(seq[0] == '[') {
            // mapping to be able to move the cursor with narrow keys
            if(seq[1] >= '0' && seq[1] <= '9') {
                if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if(seq[2] == '~') {
                    switch(seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            }
            else {
                switch(seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        }
        else if(seq[0] == 'O') {
            switch(seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return '\x1b';
    }

    return c;
}

int getCursorPosition(int *rows, int *cols) {
    char buffer[32];
    unsigned int i = 0;
    /* The n command (Device Status Report) can be used to query the terminal for status information.
    We want to give it an argument of 6 to ask for the cursor position. */
    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buffer) - 1) {
        if (read(STDIN_FILENO, &buffer[i], 1) != 1) break;
        if (buffer[i] == 'R') break; // the R is the final char of the cursor position report
        i++;
    }
    buffer[i] = '\0';
    /* Skip the first character in buffer by passing &buf[1] and avoid the terminal interpreting it as 
    an escape sequence.
    printf() expects strings to end with a 0 byte, so we make sure to assign '\0' to the final byte.
    */
    // printf("\r\n&buf[1]: '%s'\r\n", &buffer[1]);

    if (buffer[0] != '\x1b' || buffer[1] != '[') return -1; // make sure it responded with an escape sequence
    // get the position parsing the response
    if(sscanf(&buffer[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        // moving the cursor to the bottom-right (fallback method to get the window size)
        // C command is move cursor forward and B cmd is move the cursor down
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    }
    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** Row operations ***/
int editorRowCxToRx(erow *row, int cx) {
    int rx = 0;
    for(int j = 0; j < cx; j++) {
        if(row->chars[j] == '\t') {
            rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
        }
        rx++;
    }

    return rx;
}

void editorUpdateRow(erow *row) {
    int tabs = 0;
    int j;
    /* The maximum number of characters needed for each tab is 4. row->size already counts 1 for each tab, 
    so we multiply the number of tabs by 3 and add that to row->size to get the maximum amount of memory 
    we’ll need for the rendered row.
    */
    for(j = 0; j < row->size; j++) {
        if(row->chars[j] == '\t') tabs++;
    }

    free(row->render);
    row->render = malloc(row->size + tabs*(KILO_TAB_STOP-1) + 1);

    int idx = 0;
    // copy the from chars to render
    for(j = 0; j < row->size; j++) {
        if(row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while(idx % KILO_TAB_STOP != 0) row->render[idx++] = ' ';
        }
        else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void editorAppendRow(char *s, size_t len) {
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    int at = E.numrows;

    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1); // reserve the memory for the message
    memcpy(E.row[at].chars, s, len); // copy the message to chars
    E.row[at].chars[len] = '\0';
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);

    E.numrows++; // a line must be displayed now
}



/*** file I/O ***/
void editorOpen(char *filename) {
    FILE *fp = fopen(filename, "r");
    if(!fp) die("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    // getline() is useful for reading lines from a file when we don’t know how much memory to allocate
    // for each line. It takes care of memory management for you. First, we pass it a null line pointer
    // and a linecap (line capacity) of 0. That makes it allocate new memory for the next line it reads,
    // and set line to point to the memory, and set linecap to let you know how much memory it allocated.
    // Its return value is the length of the line it read, or -1 if it’s at the end of the file
    while((linelen = getline(&line, &linecap, fp)) != -1) {
        // strip off the newline or carriage return at the end of the line before copying it into our erow
        while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
            linelen--;
        }

        editorAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
}

/*** append buffer ***/
/* It would be better to do one big write(), to make sure the whole screen updates at once.
Otherwise there could be small unpredictable pauses between write()’s, which would cause an
annoying flicker effect.

We want to replace all our write() calls with code that appends the string to a buffer, 
and then write() this buffer out at the end.
*/
struct abuf {
    char* b;
    int len;
};
// An append buffer consists of a pointer to our buffer in memory, and a length
#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
    // make sure we allocate enough memory to hold the new string.
    char *new = realloc(ab->b, ab->len + len);

    if(new == NULL) return;
    /* copy the string s after the end of the current data in the buffer, and we update the pointer
    and length of the abuf to the new values */
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}

/** output ***/
void editorScroll() {
    E.rx = E.cx;
    if (E.cy < E.numrows) {
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
    }

    /* The first if statement checks if the cursor is above the visible window,
    and if so, scrolls up to where the cursor is. The second if statement checks if the cursor
    is past the bottom of the visible window, and contains slightly more complicated arithmetic 
    because E.rowoff refers to what’s at the top of the screen.
    */
    if(E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if(E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
    // horizontal scrolling
    if(E.rx < E.coloff) {
        E.coloff = E.rx;
    }
    if(E.rx >= E.coloff + E.screencols) {
        E.coloff = E.rx - E.screencols + 1;
    }
}

void editorDrawRows(struct abuf *ab) {
    int y;
    for(y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        if(filerow >= E.numrows) { // check whether we are currently drawing a row that is part of the text buffer
            if(E.numrows == 0 && y == E.screenrows / 3) {
                // write a WELCOME message
                char welcome[80];
                /*We use the welcome buffer and snprintf() to interpolate our YATE_VERSION string into 
                the welcome message*/
                int welcomelen = snprintf(welcome, sizeof(welcome), "Yate Editor -- version %s", YATE_VERSION);

                if(welcomelen > E.screencols) welcomelen = E.screencols;

                // center the message
                int padding = (E.screencols - welcomelen) / 2;
                if(padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while(padding--) abAppend(ab, " ", 1);

                abAppend(ab, welcome, welcomelen);
            }
            else {
                // write(STDOUT_FILENO, "~", 1); // write tilde for each visible row
                abAppend(ab, "~", 1);
            }
        }
        else {
            int len = E.row[filerow].rsize - E.coloff;
            if(len < 0) len = 0;
            if(len > E.screencols) len = E.screencols; // truncate the line if it's necessary
            abAppend(ab, &E.row[filerow].render[E.coloff], len);
        }
        /* The K command (Erase In Line) erases part of the current line. 
        Its argument is analogous to the J command’s argument: 2 erases the whole line, 
        1 erases the part of the line to the left of the cursor, 
        and 0 erases the part of the line to the right of the cursor (default)
        */
        abAppend(ab, "\x1b[K", 3); // clear each line as we redraw it
        // and for all except the last line, print \r\n
        if(y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}


void editorRefreshScreen() {
    editorScroll();
    /*The 4 in our write() call means we are writing 4 bytes out to the terminal. 
    The first byte is \x1b, which is the escape character, or 27 in decimal.

    We are writing an escape sequence to the terminal. Escape sequences always start with an escape character
    followed by a [ character. Escape sequences instruct the terminal to do various text formatting tasks, 
    such as coloring text, moving the cursor around, and clearing parts of the screen.

    We are using the J command (Erase In Display) to clear the screen. Escape sequence commands take arguments,
    which come before the command. In this case the argument is 2, which says to clear the entire screen. 
    <esc>[1J would clear the screen up to where the cursor is, and <esc>[0J would clear the screen from the 
    cursor up to the end of the screen.
    */
    // write(STDOUT_FILENO, "\x1b[2J", 4); // clear scren
    // write(STDERR_FILENO, "\x1b[H", 3); // relocate cursor at top, the default args are row and column 1 and 1

    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6); // hide cursor when repainting
    // abAppend(&ab, "\x1b[2J", 4); // don't clear full screen, instead clear each line as we redraw it
    abAppend(&ab, "\x1b[H", 3);
    editorDrawRows(&ab);

    // move the cursor to the position stored in E.cx and E.cy.
    char buf[32];
    // We changed the old H command into an H command with arguments, specifying the exact position 
    // we want the cursor to move to. We add 1 to (E.cy - offset) and (E.cx - offet) to convert from 0-indexed values to the 1-indexed 
    // values that the terminal uses.
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));

    // write(STDOUT_FILENO, "\x1b[H", 3);
    // abAppend(&ab, "\x1b[H", 4); // relocate cursor again
    abAppend(&ab, "\x1b[?25h", 6); // show cursor again

    // write the full buffer
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** input ***/
void editorMoveCursor(int key) {
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch (key) {
        case ARROW_LEFT:
            if(E.cx != 0) {
                E.cx--;
            }
            else if(E.cy > 0) {
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if(row && E.cx < row->size) {
                E.cx++;
            }
            else if(row && E.cx == row->size) {
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if(E.cy != 0) {
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if(E.cy < E.numrows) {
                E.cy++;
            }
            break;
    }

    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if(E.cx > rowlen) {
        E.cx = rowlen;
    }
}


void editorProcessKeypress() {
    /* waits for a keypress, and then handles it. */
    int c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            // reset screen
            write(STDOUT_FILENO, "\x1b[2J", 4); // clear scren
            write(STDERR_FILENO, "\x1b[H", 3); // relocate cursor position
            exit(0);
            break;
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            E.cx = E.screencols - 1;
            break;
        // If you’re on a laptop with an Fn key, you may be able to press Fn+↑ and Fn+↓ to simulate pressing 
        // the Page Up and Page Down keys.
        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = E.screenrows;
                while(times--) {
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}


/*** init ***/
void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.numrows = 0;
    E.rowoff = 0; // We initialize it to 0, which means we’ll be scrolled to the top of the file by default.
    E.coloff = 0; // same idea as the rowoff's initialization
    E.row = NULL;
    if(getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}


int main(int argc, char const *argv[]) {
    enableRawMode();
    initEditor();

    if(argc >= 2) {
        editorOpen(argv[1]);
    }

    char c;

    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
