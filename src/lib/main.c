/**
 * Simple serial port terminal interface and configuration.
 *
 * Reads local stdin and forwards the characters (raw) to the
 * port. Reads the port and forwards to local stdout. Error
 * messages (and verbose messages) are printed to stderr.
 *
 * - Restores the port and terminal settings on exit.
 *
 * - Implemented exit signals: SIGTERM,SIGKILL,SIGINT,SIGHUP,
 *                             SIGABRT,SIGTRAP,SIGPIPE,SIGSYS
 *
 * - Defaults:
 *
 *   - Baudrate = 115200
 *   - Parity   = N
 *   - Databits = 8
 *   - Stopbits = 1
 *
 * @file main.c
 * @license GPL3
 * @author stfwi
 */
#include "serialport.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <termios.h>
#include <getopt.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <locale.h>
 
#define PRG_NAME ("serialport")
#define PRG_VERSION ("1.0")
#define debug(...) { if(is_debug) _debug(__VA_ARGS__); }
 
/**
 * Program exit codes
 */
enum exit_codes {
  EC_OK = 0, EC_ARG, EC_SIGNAL, EC_FATAL, EC_IO, EC_END
};
 
/**
 * Output colouring
 */
enum color_ids {
  COLOR_NO = 0, COLOR_OK, COLOR_ERR, COLOR_RX, COLOR_TX, COLOR_TS, COLOR_DBG, COLOR_END
};
 
const char *color_strings[] = {
  "\033[0m",    // reset
  "\033[0;32m", // green
  "\033[0;31m", // red
  "\033[0;34m", // RX: blue
  "\033[1;36m", // TX echo: light cyan
  "\033[0;35m", // Timestamp: purple
  "\033[0;37m", // Debug: light gray
  "\033[0m",    // --end--
};
 
/**
 * Globals
 */
int in_d = -1;                          // Input file descriptor (def. STDIN)
int out_d = -1;                         // Output file descriptor (def. STDOUT)
int in_is_tty = 0;                      // Local input is a TTY
int out_is_tty = 0;                     // Local output is a TTY
int in_eof = 0;                         // Local input is EOF (if no TTY)
int exit_counter = 0;                   //
struct termios stdios;                  // Local TTY settings
struct termios stdios_orig;             // Local TTY startup settings (for restore)
serial_port_t port;                     // Serial port
double timeout = NAN;                   // Inactivity timeout "counter"
int was_newline = 0;                    // Detection of <CR><LF> combinations
int was_cr = 0;
long long next_char_wait_count = 0;     // counter, send wait
unsigned long long lines_received  = 0; // statistics, counter, exit condition
unsigned long long bytes_rx = 0;        // statistics, counter
unsigned long long bytes_tx = 0;        // statistics, counter
struct { int p; char s[256]; int sz; } buffer;  // RX command buffer
int disconnect_cmd_len = 0;
enum color_ids color_last = COLOR_END;
 
/**
 * Globals (Config)
 */
char    portfile[256];
char    infile[256];            // maybe later overwritable, now stdin
char    outfile[256];           // maybe later overwritable, now stdout
char    disconnect_cmd[256];    // Exit on receiving this
int     baudrate        = 115200;
char    parity          = 'n';
int     stopbits        = 1;
int     databits        = 8;
int     is_debug        = 0;    // bool Debug messages, implicitly enable is_verbose
int     is_verbose      = 0;    // bool Verbose
int     is_local_echo   = 0;    // bool Echo sent chars to stdout
double  timeout_cfg     = NAN;  // Exit after inactivity of this in seconds
int     add_timestamps  = 0;    // bool
long    character_wait  = 0;    // us
long    start_wait      = 0;    // us
int     use_colors      = 0;    // bool
int     nonprintable    = 0;    // bool
int     max_lines       = -1;   // Exit after receiving max_lines
 
 
void eexit(int code, const char* err_msg, ...);
 
 
/**
 * Print usage and exit.
 *
 * @return void
 */
void print_usage(void)
{
  char placeholder[64];
  memset(placeholder, ' ', 64);
  placeholder[strlen(PRG_NAME)] = '\0';
  printf(
      "Usage: %s [options] <portfile> [BAUD=%d[PARITY=%c[DATABITS=%d[STOPBITS=%d]]]]\n"
      "\n"
      "Simple serial port TTY redirection program for (raw) microcontroller\n"
      "communication. Exit with CTRL-C.\n"
      "\n"
      "Options:\n\n"
      " -h, --help          Print this help\n"
      " -v, --verbose       Make the program more chatty (on stderr)\n"
      " -T, --timeout=N     Exit after nothing happened for N seconds\n"
      " -l, --lines=N       Exit after receiving N lines\n"
      " -d, --disconnect=S  Disconnect when receiving this (\\n etc allowed) \n"
      " -t, --timestamp     Add timestamp before each line\n"
      " -e, --local-echo    Print the sent characters locally\n"
      " -w, --char-wait=N   Wait N seconds before sending the next byte\n"
      " -W, --start-wait=N  Wait N seconds after opening port before doing anything\n"
      " -c, --color         Print with colors\n"
      " -n, --control-chars Print non-printable control characters\n"
      " -D, --debug         Print debug messages and dumps (stderr)\n"
      "\n"
      "Examples:\n\n"
      " %s /dev/ttyS               Connect with default values\n\n"
      " %s /dev/ttyS 115200        Set baud rate, rest default\n\n"
      " %s /dev/ttyS 9600N81       Set baud rate, parity, data bits, stop bits\n\n"
      " %s /dev/ttyS -T 0.5        Exit after 0.5s inactivity\n\n"
      " %s /dev/ttyS -d 'Bye\\\\r'   Exit when receiving 'Bye<CR>'\n\n"
      " %s /dev/ttyS -vect         Interactive mode with colors, local\n"
      " %s                         echo and timestamps for received lines\n\n"
      " echo 'hello!' | %s -W1.0 /dev/ttyS \n"
      " %s                         Wait 1s before sending the line 'hello!'\n\n"
      " cat cmds | %s -t -l 10 -T 0.5 /dev/ttyS | grep 'data'\n"
      " %s                         Send a file, fetch max 10 lines, quit after\n"
      " %s                         idle port for 0.5s and pipe to grep to fetch\n"
      " %s                         lines containing the text 'data'\n"
      "\n"
      "Exit codes (for shell programming):\n\n"
      " 0 = OK, no error\n"
      " 1 = Wrong argument, you mistyped something or the program can't handle it.\n"
      " 2 = Signal caught, e.g. CTRL-C, broken pipe, kill, etc.\n"
      " 3 = Fatal error. Something went wrong internally (memory, bugs(?) etc).\n"
      " 4 = I/O error, e.g. Disk full, USB-serial-converted disconnected ...\n"
      "\n"
      "(%s v%s, stfwi)\n",
      PRG_NAME, baudrate, parity, databits, stopbits,
      PRG_NAME, PRG_NAME, PRG_NAME, PRG_NAME, PRG_NAME, PRG_NAME, placeholder,
      PRG_NAME, placeholder, PRG_NAME, placeholder, placeholder, placeholder,
      PRG_NAME, PRG_VERSION);
  exit(1);
}
 
/**
 * Write a color definition to a defined
 *
 * @param enum colors color
 * @param int where 0=stdin 1=stderr
 */
void set_color(enum color_ids color, int where) {
  if(!use_colors || color >= COLOR_END || color < 0 || color == color_last) return;
  if(where) {
    fprintf(stderr, "%s", color_strings[color]);
  } else {
    if(write(out_d, color_strings[color], strlen(color_strings[color])) < 0) {
      eexit(EC_IO, "Writing to local output failed: %s", strerror(errno));
    }
  }
}
 
/**
 * Debug messages
 * @param const char *fmt
 * @param ...
 */
void _debug(const char* fmt, ...)
{
  char ss[256];
  int i;
  va_list args;
  va_start (args, fmt);
  vsnprintf (ss, sizeof(ss), fmt, args);
  va_end (args);
  for(i=strlen(ss)-1; i>=0; i--) {
    if(ss[i]=='\r' || ss[i]=='\n') ss[i]= '^';
  }
  if(!strlen(ss)) return;
  set_color(COLOR_DBG,1);
  fprintf(stderr, "%s", ss);
  set_color(COLOR_NO, 1);
  fprintf(stderr, "\n");
}
 
 
/**
 * Prints a nonprintable character to stdout
 * @param char c
 */
void print_nonprintable(char c)
{
  if(nonprintable && c < 32) {
    char es[5] = {0,0,0,0,0};
    snprintf(es, sizeof(es)-1, "%lc", 0x2400+c);
    if(write(out_d, es, strlen(es)) < 0) {
      eexit(EC_IO, "Writing to local output failed: %s", strerror(errno));
    }
  }
}
 
 
/**
 * Converts an argument to int with checks (numeric, integer, range min..max).
 * The range check can be ignored by setting min > max. Returns INVI on failure.
 * @param const char *s
 * @param int min
 * @param int max
 * @param double k
 * @return int
 */
int int_arg(const char *s, int min, int max, double k)
{
  #define INVI (INT_MAX-2)
  double d; int i;
  return (!s||isnan(d=atof(s)*k)||((i=(int)d)!=d)||(min<max&&(i<min||i>max)))?INVI:i;
}
 
 
/**
 * Converts an argument to double with check
 * The range check can be ignored by setting min > max. Returns NaN on failure.
 * @param const char *s
 * @param double min
 * @param double max
 * @return double
 */
double dbl_arg(const char *s, double min, double max)
{
  double d=atof(s);
  return (!s || isnan(d) || ((min<max) && (d<min||d>max)) ) ? NAN : d;
}
 
 
/**
 * Copies a string argument, where special character like \n \r \t are decoded.
 * @param char *v
 * @param const char *s
 * @param size_t n
 * @return char*
 */
char* str_arg(char *v, const char *s, size_t n)
{
  int i, j;
  memset(v,0,n);
  for(i=0, j=0; j<n && i<n && s[i]; i++, j++) {
    if(s[i] == '\\' && s[i+1] != '\0') {
      switch(s[++i]) {
        case 'r' : v[j] = '\r'; break;
        case 'n' : v[j] = '\n'; break;
        case 't' : v[j] = '\t'; break;
        case 'f' : v[j] = '\f'; break;
        case 'a' : v[j] = '\a'; break;
        case 'b' : v[j] = '\b'; break;
        case 'v' : v[j] = '\v'; break;
        case '\\': v[j] = '\\'; break;
        default:
          debug("str_arg() unrecognised character '\\%c'", s[i]);
      }
    } else {
      v[j] = s[i];
    }
  }
  if(j>=n || i>=n) {
    eexit(EC_FATAL, "Allocated memory not big enough to contain a string argument");
  }
  return v;
}
 
 
/**
 * Parses the command line arguments
 *
 * @param int argc
 * @param const char **argv
 * @return void
 */
void parse_args(int argc, char **argv)
{
  int n, c;
  debug("parse_args(%d, argv)", argc);
 
  const struct option long_options[] = {
      {"help", 0, 0, 0},
      {"verbose", 0, 0, 0},
      {"local-echo", 0, 0, 0},
      {"timeout", 1, 0, 0},
      {"debug", 0, 0, 0},
      {"disconnect", 1, 0, 0},
      {"timestamp", 0, 0, 0},
      {"color", 0, 0, 0},
      {"control-chars", 0, 0, 0},
      {"lines", 1, 0, 0},
      {"char-wait", 1, 0, 0},
      {"start-wait", 1, 0, 0},
      {NULL, 0, NULL, 0}
  };
 
  if(argc<2) print_usage();
 
  while ((c = getopt_long(argc, argv, "ncDhved:tT:l:w:W:", long_options, &n)) != -1) {
    //debug("parse_args() optindex=%d", optind);
    if(!c) {
      //debug("parse_args() longopt=%s", long_options[n].name);
      switch(n) {
        case 0  : c='h'; break;
        case 1  : c='v'; break;
        case 2  : c='e'; break;
        case 3  : c='T'; break;
        case 4  : c='D'; break;
        case 5  : c='d'; break;
        case 6  : c='t'; break;
        case 7  : c='c'; break;
        case 8  : c='n'; break;
        case 9  : c='l'; break;
        case 10 : c='w'; break;
        case 11 : c='W'; break;
        default: if(optind>0) eexit(EC_ARG, "Invalid option %s", argv[optind-1]);
      }
    }
    //debug("parse_args() shortopt=%c", c);
    switch (c) {
      case 'h': print_usage(); break;
      case 'v': is_verbose=1; break;
      case 'e': is_local_echo=1; break;
      case 'D': is_verbose=1; is_debug=1; break;
      case 't': add_timestamps = 1; break;
      case 'c': use_colors = 1; break;
      case 'n': nonprintable = 1; break;
      case 'T':
        if(isnan(timeout_cfg = dbl_arg(optarg, 0, 1000000))) {
          eexit(EC_ARG, "Invalid timeout (0 to 1e6) seconds: %s", optarg);
        }
        debug("parse_args() timeout=%f", timeout_cfg);
        break;
      case 'd':
        if(!optarg) {
          eexit(EC_ARG, "Disconnect command option (-d) needs a text argument");
        } else {
          str_arg(disconnect_cmd, optarg, sizeof(disconnect_cmd));
          debug("parse_args() disconnect=%s", disconnect_cmd);
          if(strlen(disconnect_cmd) >= buffer.sz) {
            eexit(EC_ARG, "Disconnect command is too long, sorry");
          }
        }
        break;
      case 'l':
        if((max_lines=int_arg(optarg, 0, 1000000, 1)) == INVI) {
          eexit(EC_ARG, "Invalid maximum lines to receive (0 to 1e6): %s", optarg);
        }
        debug("parse_args() max_lines=%d", max_lines);
        break;
      case 'w':
        if((character_wait=int_arg(optarg, 0, 100000000, 1e6)) == INVI) {
          eexit(EC_ARG, "Invalid maximum lines to receive (0 to 1e8): %s", optarg);
        }
        debug("parse_args() character_wait=%ld", character_wait);
        break;
      case 'W':
        if((start_wait=int_arg(optarg, 0, 100000000, 1e6)) == INVI) {
          eexit(EC_ARG, "Invalid initial wait time (0 to 1e8): %s", optarg);
        }
        debug("parse_args() start_wait=%ld", character_wait);
        break;
      default :
        eexit(EC_ARG, "Invalid option %s", argv[optind-1]);
    }
  }
 
  if(optind<1) optind = 1;
  argv += optind;
  argc -= optind;
 
  for(n=0; n<argc && argv[n]; n++) {
    char *p, *pp; p = argv[n];
    switch(n) {
      case 0: // port file
        strncpy(portfile, p, sizeof(portfile));
        portfile[sizeof(portfile)-1] = '\0';
        debug("parse_args() portfile=%s", portfile);
        if(!strlen(portfile)) eexit(EC_ARG, "No port given (/dev/...)");
        break;
      case 1:
        // Format 115200N81
        for(pp=p; *pp; pp++) {
          if(!isdigit(*pp)) {
            if((parity = tolower(*pp)) != 'n' && parity != 'e' && parity != 'o') {
              eexit(EC_ARG, "Invalid parity ('n','e','o'): %c", *pp);
            }
            debug("parse_args() parity=%c", parity);
            (*pp) = '\0'; // limit string for baud rate later
            if(*++pp) {
              if((databits = ((*pp)-'0')) != 7 && databits != 8) {
                eexit(EC_ARG, "Invalid data bits (7 or 8): %c", *pp);
              }
              debug("parse_args() databits=%d", databits);
            }
            if(*++pp) {
              if((stopbits = ((*pp)-'0')) != 1 && stopbits != 2) {
                eexit(EC_ARG, "Invalid stop bits (1 or 2): %c", *pp);
              }
              debug("parse_args() stopbits=%d", stopbits);
            }
          }
        }
        if((baudrate = atoi(p)) <= 0) {
          eexit(EC_ARG, "Invalid baud rate: %s", p);
        } else {
          debug("parse_args() baudrate=%d", baudrate);
          int baudrate_ok = 0;
          const unsigned long* r;
          for(r=serial_port_get_valid_baudrates(); r&&*r; r++) {
            if(*r == baudrate) baudrate_ok = 1;
            if(baudrate < *r) break;
          }
          if(!baudrate_ok) {
            eexit(EC_ARG, "Invalid baud rate: %d (Did you mean %d)", baudrate,
                (r&&*r?*r:0));
          }
        }
        break;
      default:
        ;
    }
  }
}
 
 
/**
 * Signal handler, exits with the error code arg signum
 *
 * @param int signum
 * @return void
 */
void signal_handler(int signum)
{
  debug("signal_handler(%d)", signum);
  int code = EC_SIGNAL;
  const char *signal_s;
  switch(signum) {
    case SIGTERM: signal_s = "kill signal"; break;
    case SIGKILL: signal_s = "hard kill signal"; break;
    case SIGINT: signal_s = "CTRL-C"; code=0; break;
    case SIGHUP: signal_s = "hangup signal (SIGHUP)"; break;
    case SIGABRT: signal_s = "error in program (SIGABRT)"; break;
    case SIGTRAP: signal_s = "debug trap (TRAP)"; break;
    case SIGPIPE: signal_s = "broken pipe"; break;
    case SIGSYS: signal_s = "Program error (bad system call)"; break;
    default: signal_s = "(unknown signal)";
  }
  eexit(code, "Exit due to signal: %s", signal_s);
}
 
 
 
 
/**
 * Prints an error message and exits.
 *
 * @param err_msg
 * @param ...
 * @return void
 */
void eexit(int code, const char* err_msg, ...)
{
  int i;
  debug("eexit(%d, %s)", code, err_msg?err_msg:"(no message)");
 
  // Close and restore
  serial_port_close(&port);
 
  // Print last lines ...
  set_color(code!=0 ? COLOR_ERR : COLOR_OK, 1);
  if(is_verbose || code != 0) {
    if(!err_msg) {
      fprintf(stderr, "\n[Unexpected error]");
    } else {
      char buffer[256];
      va_list args;
      va_start (args, err_msg);
      vsnprintf (buffer, sizeof(buffer), err_msg, args);
      va_end (args);
      buffer[sizeof(buffer)-1] = '\0';
      if(err_msg) fprintf(stderr, "\n[%s]", buffer);
    }
    if(is_verbose) {
      fprintf(stderr, "[E%d]\n[TX:%llu, RX:%llu, RX:%llu lines]", code, bytes_tx,
          bytes_rx, lines_received);
    }
    fprintf(stderr, "\n");
  }
  debug("eexit() EXIT");
  set_color(COLOR_NO, 0);
  set_color(COLOR_NO, 1);
 
  if(in_d >= 0) {
    for(i=0; i<sizeof(stdios_orig); i++) {
      if(((unsigned char*)&stdios_orig)[i]) {
        tcsetattr(in_d, TCSANOW, &stdios_orig);
        break;
      }
    }
    close(in_d);
    in_d = -1;
  }
  if(out_d >= 0) {
    close(out_d);
    out_d = -1;
  }
 
  exit(code);
}
 
 
 
int cycle(void);
 
/**
 * Main
 *
 * @param int argc
 * @param const char** argv
 * @return int
 */
int main(int argc, char** argv)
{
  // Init
  in_d = fileno(stdin);
  out_d = fileno(stdout);
  setlocale(LC_ALL, "");
 
  debug("main() Setup signals");
  // Signals
  signal(SIGTERM, signal_handler);
  signal(SIGKILL, signal_handler);
  signal(SIGINT, signal_handler);
  signal(SIGHUP, signal_handler);
  signal(SIGABRT, signal_handler);
  signal(SIGTRAP, signal_handler);
  signal(SIGPIPE, signal_handler);
  signal(SIGSYS, signal_handler);
 
  // INIT
  debug("main() Init variables");
  memset(&buffer, 0, sizeof(buffer)); buffer.sz = sizeof(buffer.s)-1;
  memset(disconnect_cmd, 0, sizeof(disconnect_cmd));
  memset(&stdios, 0, sizeof(stdios));
  memset(&stdios_orig, 0, sizeof(stdios_orig));
  memset(portfile, 0, sizeof(portfile));
  memset(infile, 0, sizeof(infile));
  memset(outfile, 0, sizeof(outfile));
  strcpy(infile, "/dev/stdin");
  strcpy(outfile, "/dev/stdout");
  parse_args(argc, argv);
 
  // Port settings
  debug("main() Init serial port");
  serial_port_init(&port);
  strncpy(port.file, portfile, sizeof(port.file));
  port.baudrate = baudrate;
  port.parity = parity;
  port.databits = databits;
  port.stopbits = stopbits;
  port.timeout_us = 1000;
  port.flush = 1;
 
  if(serial_port_open(&port) < 0) {
    eexit(EC_IO, port._state.error_string);
  }
 
  if(is_debug) {
    set_color(COLOR_DBG,1);
    serial_port_dump(&port);
    //set_color(COLOR_NO,1);
  }
 
  // Local TTY / files
  if(!strcmp(infile, "/dev/stdin")) {
    debug("main() Local input is STDIN");
    in_d = fileno(stdin);
  } else if((in_d=open(infile, O_RDONLY)) < 0) {
    eexit(EC_IO, "Failed to open input");
  }
  if(!strcmp(outfile, "/dev/stdout")) {
    debug("main() Local input is STDOUT");
    out_d = fileno(stdout);
  } else if((out_d=open(outfile, O_WRONLY)) < 0) {
    eexit(EC_IO, "Failed to open output");
  }
 
  fcntl(in_d, F_SETFL, O_NONBLOCK);
  fcntl(out_d, F_SETFL, O_NONBLOCK);
 
  if(isatty(in_d)) {
    in_is_tty = 1;
    tcgetattr(in_d, &stdios_orig);
    stdios = stdios_orig;
    stdios.c_lflag &= ~(ECHO|ICANON);
    stdios.c_cc[VTIME] = 0;
    stdios.c_cc[VMIN] = 1; // stfwi: double check
    debug("main() Local input is TTY");
  } else {
    in_is_tty = 0;
    debug("main() Local input is NO TTY");
  }
  tcsetattr(in_d, TCSANOW, &stdios);
 
  if(isatty(out_d)) {
    out_is_tty = 1;
    debug("main() Local output is TTY");
  } else {
    out_is_tty = 0;
    debug("main() Local output is NO TTY");
  }
 
  // Prepare
  debug("main() Prepare run");
  timeout = timeout_cfg;
  debug("main() Initial timeout: %f", timeout);
  was_newline = 1;
  lines_received = 0;
  bytes_tx = 0;
  bytes_tx = 0;
  next_char_wait_count = 0;
  disconnect_cmd_len = strlen(disconnect_cmd);
 
  if(start_wait > 0) {
    usleep(start_wait);
  }
 
  // RUN, abort with signals
  debug("main() Run ...");
  while(cycle() == 0);
  debug("main() ... run break.");
  eexit(0, "Bye");
  return 0;
}
 
/**
 * Called in a loop. Returns nonzero if the loop shall exit
 * @return int
 */
int cycle(void)
{
  fd_set rfds, wfds,efds;
  struct timeval tv;
  char s[256];
  int n,i;
 
  // Wait for events
  tv.tv_sec=0; tv.tv_usec=1000;
  FD_ZERO(&rfds); FD_ZERO(&wfds); FD_ZERO(&efds); n=0;
 
  if(timeout_cfg>0 && (timeout-= ((double)tv.tv_usec/(double)1000000))<=0) {
    eexit(0, "Timed out after %.1fs", timeout_cfg);
  }
 
  if(next_char_wait_count > 0) {
    next_char_wait_count -= tv.tv_usec;
  } else {
    if(!in_eof) {
      FD_SET(in_d, &rfds); n++;
    }
  }
 
  FD_SET(in_d, &efds); n++;
  if(out_is_tty) FD_SET(out_d, &efds); n++;
  FD_SET(port._state.handle, &efds); n++;
  FD_SET(port._state.handle, &rfds); n++;
 
  if((n=select((int)n+1, &rfds, &wfds, &efds, &tv)) < 0) switch(n) {
    case EBADF: eexit(EC_FATAL, "An invalid file descriptor was given in one of the sets"); break;
    case EINTR: debug("EINTR"); break; // A signal was caught
    case EINVAL: eexit(EC_FATAL, "Invalid file descriptor invalid timeout"); break;
    case ENOMEM: eexit(EC_FATAL, "Unable to allocate memory for internal select() tables"); break;
    default: eexit(EC_FATAL, "Waiting for i/o states failed (select()<0): %s", strerror(errno));
  }
 
  if(!n) {
    //return 0;
  } else {
    timeout = timeout_cfg;
  }
 
  //debug("[cycle(timeout=%.1f)]", timeout);
 
  if(FD_ISSET(out_d, &efds) || FD_ISSET(in_d, &efds) || FD_ISSET(port._state.handle, &efds)) {
    if(FD_ISSET(out_d, &efds)) eexit(EC_IO, "I/O local out error: %s", strerror(errno));
    if(FD_ISSET(in_d, &efds)) eexit(EC_IO, "I/O local in error: %s", strerror(errno));
    if(FD_ISSET(port._state.handle, &efds)) eexit(EC_IO, "I/O port error: %s", strerror(errno));
  }
 
  // Serial --> stdout
  if(FD_ISSET(port._state.handle, &rfds)) {
    if((n=serial_port_read(&port, s, sizeof(s)-1)) < 0) {
      eexit(EC_IO, port._state.error_string);
    }
 
    if(n > 0) {
      debug("[RX %d]", n);
      bytes_rx += n;
      set_color(COLOR_RX, 0);
      for(i=0; i<n; i++) {
        if(s[i] == '\r' || s[i] == '\n') {
          was_newline = 1;
          if(s[i] == '\r' || !was_cr) lines_received++;
          was_cr = (s[i] == '\r');
          if((lines_received >= max_lines) && max_lines>0) {
            if(write(out_d, "\n", 1) < 0) { // we add this to get a new line in the terminal
              eexit(EC_IO, "Writing to local output failed: %s", strerror(errno));
            }
            eexit(0, "Specified number of lines received");
          }
        } else if(was_newline) {
          was_newline = 0;
          if(add_timestamps) {
            char ts[64];
            struct timeval tod;
            gettimeofday(&tod, NULL);
            snprintf(ts, 64, "%.2f ", (double) time(NULL) + (double) tod.tv_usec * 1e-6);
            set_color(COLOR_TS, 0);
            if(write(out_d, ts, strlen(ts)) < 0) {
              eexit(EC_IO, "Writing to local output failed: %s", strerror(errno));
            }
            set_color(COLOR_RX, 0);
          }
        }
        print_nonprintable(s[i]);
        if(((!nonprintable) || (s[i]!='\r')) && (write(out_d, s+i, 1) < 0)) {
          eexit(EC_IO, "Writing to local output failed: %s", strerror(errno));
        }
      }
      //set_color(COLOR_NO, 0);
 
      // Disconnect command
      if(disconnect_cmd_len > 0) {
        int k, l;
        for(i=0; i<n; i++) {
          buffer.s[buffer.p] = s[i];
          if(++buffer.p >= buffer.sz) buffer.p = 0;
          if((k = buffer.p - disconnect_cmd_len)<0) k+= buffer.sz;
          l = 0;
          while(k != buffer.p) {
            if(buffer.s[k++] != disconnect_cmd[l++]) break;
            if(k >= buffer.sz) k = 0;
            if(l == disconnect_cmd_len) {
              debug("DETECTED '%s'", disconnect_cmd);
              eexit(0, "Exit due to disconnect command");
            }
          }
        }
      }
    }
  }
 
  // stdin --> Serial
  if(FD_ISSET(in_d, &rfds)) {
    next_char_wait_count = character_wait;
    if((n=read(in_d, s, character_wait>0 ? 1 : (sizeof(s)-1))) < 0 && errno != EWOULDBLOCK) {
      eexit(EC_IO, "Reading local input failed: %s", strerror(errno));
    }
    if(n > 0) {
      //s[n+1] = '\0';
      debug("[TX %d]", n);
      if(serial_port_write(&port, s, n) < 0) {
        eexit(EC_IO, port._state.error_string);
      }
      bytes_tx += n;
      if(is_local_echo) {
        set_color(COLOR_TX, 0);
        for(i=0; i<n; i++) {
          if(nonprintable && s[i] < 16) print_nonprintable(s[i]);
          if(!nonprintable || s[i]!='\r') {
            if(write(out_d, s+i, 1) < 0) {
              eexit(EC_IO, "Writing to local output failed: %s", strerror(errno));
            }
          }
        }
        //set_color(COLOR_NO, 0);
      }
    } else if(n==0 && !in_is_tty) {
      in_eof = 1;
      debug("[EOF IN]");
    }
  }
  return 0;
}
Serial port handling files
/**
 * Serial port interface
 *
 * Usage:
 *  serial_port_t port;
 *  serial_port_init(&port);
 *  strncpy(port.file, "/dev/ttyXXX", sizeof(port.file));
 *  port.baudrate = 115200;     or other valid rates
 *  port.parity = 'n';          or , 'e', or 'o'
 *  port.databits = 8           or 7 or 6 or 5
 *  port.stopbits = 1;          or 2
 *  port.read_timeout_us=1000;  or other timeouts or -1 for "read blocking"
 *  port.local = 1;             or 0 or -1 for 'don't change'.
 *  port.set_dtr = -1;          or 0 or 1. -1== don't change
 *  port.set_rts = -1;          or 0 or 1. -1== don't change
 *
 *  Meaning of port.local: "Ignore modem status lines"
 *  Meaning of set_dtr   : "Set DTR directly when opening"
 *  Meaning of set_rts   : "Set RTS directly when opening"
 *
 * @file serialport.h
 * @author stfwi
 * @license Free, non-/commercial use granted.
 */
#ifndef SERIALPORT_H
#define SERIALPORT_H
 
#include <termios.h>
#ifdef  __cplusplus
extern "C" {
#endif
 
/**
 * Errors returned by serial port functions
 */
typedef enum {
  E_SERIAL_OK = 0,
  E_SERIAL_NULLPOINTER = -1,
  E_SERIAL_OPEN = -2,
  E_SERIAL_NOTTY = -3,
  E_SERIAL_GETATTR = -4,
  E_SERIAL_SETATTR = -5,
  E_SERIAL_GETIOC = -6,
  E_SERIAL_SETIOC = -7,
  E_SERIAL_SETNONBLOCK = -8,
  E_SERIAL_SELECT = -9,
  E_SERIAL_READ = -10,
  E_SERIAL_BAUDRATE = -11,
  E_SERIAL_IOCTL = - 12,
  E_SERIAL_WRITE = -13
} serial_port_error_t;
 
 
/**
 * Port definition and state structure.
 */
typedef struct {
  char file[128];                   /* Port file to open, e.g. /dev/ttyUSB      */
  int  baudrate;                    /* A valid baud rate                        */
  char parity;                      /* Parity, either 'n', 'e', or 'o'          */
  char stopbits;                    /* Stop bits: 0=1bit, 1=2 bits              */
  char databits;                    /* Data bits 5 to 8                         */
  char local;                       /* 1= modem status lines are ignored        */
  char flush;                       /* 1= clear buffers after opening           */
  char set_dtr;                     /* Init DTR, 0=clear, 1=set, -1=dont change */
  char set_rts;                     /* Init RTS, 0=clear, 1=set, -1=dont change */
  long long timeout_us;             /* <0=read blocking, >=0: timeout in us     */
  struct {                          /* State structure (dynamically changed)    */
    serial_port_error_t error_code; /* Last error code                          */
    char error_string[128];         /* Last error string                        */
    int handle;                     /* Handle of the open port file             */
    struct termios attr;            /* Applied terminal io attributes           */
    struct termios attr_original;   /* Terminal io attributes before opening    */
    int mdlns;                      /* Modem line states                        */
    int mdlns_original;             /* Modem line states before opening         */
  } _state;
} serial_port_t;
 
 
 
/**
 * Returns a list of valid baudrates. The end of the list is marked with the
 * value 0
 *
 * @return const int*
 */
const unsigned long* serial_port_get_valid_baudrates(void);
 
/**
 * Initialises the structure variable, resets all settings and the state to
 * 0 or the defaults, respectively.
 * @param serial_port_t *port
 * @return serial_port_error_t
 */
serial_port_error_t serial_port_init(serial_port_t *port);
 
 
/**
 * Opens and applies the settings defined in port. Returns 0 on success,
 * or another serial_port_error_t.
 *
 * @param serial_port_t *port
 * @return serial_port_error_t
 */
serial_port_error_t serial_port_open(serial_port_t *port);
 
 
/**
 * Close the port
 *
 * serial_port_t *port
 * @return serial_port_error_t
 */
serial_port_error_t serial_port_close(serial_port_t *port);
 
/**
 * Wait until a port input/output/error flag is set, timout is port.timeout_us.
 * Arguments 'r', 'w' and 'e' are boolean flags what events shall be checked
 * (r=have received, w=send buffer empty, e=error somewhere).
 *
 * Return value: bit0: r is set, bit1: w is set, bit2: e is set.
 *
 * @param serial_port_t *port
 * @param int r
 * @param int w
 * @param int e
 * @return int
 */
int serial_port_wait_for_events(serial_port_t *port, int r, int w, int e);
 
/**
 * Read from the port. Returns the number of bytes read or,
 * if <0 a serial_port_error_t.
 *
 * @param serial_port_t *port
 * @param char* buffer
 * @param unsigned sz
 * @return int
 */
int serial_port_read(serial_port_t *port, char* buffer, unsigned sz);
 
/**
 * Write sz bytes. Returns number of bytes written or <0 on error
 *
 * @param serial_port_t *port
 * @param const char* buffer
 * @param unsigned sz
 * @return int
 */
int serial_port_write(serial_port_t *port, const char* buffer, unsigned sz);
 
/**
 * Clears RX and TX buffers ("flush"). It wait_until_all_output_sent==1, waits
 * until TX buffer is completely sent.
 * @param serial_port_t *port
 * @param int wait_until_all_output_sent
 */
int serial_port_clear_buffers(serial_port_t *port, int wait_until_all_output_sent);
 
/**
 * Returns 1 if set, 0 if not set <0 on error
 *
 * @param serial_port_t *port
 * @return int
 */
int serial_port_get_cts(serial_port_t *port);
 
/**
 * Returns 1 if set, 0 if not set <0 on error
 *
 * @param serial_port_t *port
 * @return int
 */
int serial_port_get_dsr(serial_port_t *port);
 
/**
 * Returns 1 if set, 0 if not set <0 on error
 *
 * @param serial_port_t *port
 * @return int
 */
int serial_port_get_dtr(serial_port_t *port);
 
/**
 * Returns 1 if set, 0 if not set <0 on error
 *
 * @param serial_port_t *port
 * @return int
 */
int serial_port_get_rts(serial_port_t *port);
 
/**
 * Returns 0 on success <0 on error
 *
 * @param serial_port_t *port
 * @return int
 */
int serial_port_set_dtr(serial_port_t *port, int DTR);
 
/**
 * Returns 0 on success <0 on error
 *
 * @param serial_port_t *port
 * @return int
 */
int serial_port_set_rts(serial_port_t *port, int RTS);
 
/**
 * Print port settings and state to stderr.
 * @param serial_port_t *port
 */
void serial_port_dump(serial_port_t *port);
 
#ifdef  __cplusplus
}
#endif
#endif
