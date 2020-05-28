/**
 * Serial port interface
 *
 * @file serialport.c
 * @author stfwi
 * @license Free, non-/commercial use granted.
 */
#include "serialport.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#ifdef __linux__
#include <sys/file.h>
#endif
 
/* Error helper macro */
#define ERR_S_NULLPOINTER "Null pointer given"
#define seterror(CODE, ...) {\
  port->_state.error_code = CODE; \
  snprintf(port->_state.error_string, sizeof(port->_state.error_string), __VA_ARGS__); \
  if(CODE!=0) return CODE; \
}
 
/* Used fields in port pointer */
#define _hnd (port->_state.handle)
#define _file (port->file)
#define _attr (port->_state.attr)
#define _attr_orig (port->_state.attr_original)
#define _mdlns (port->_state.mdlns)
#define _mdlns_orig (port->_state.mdlns_original)
#define _baudrate (port->baudrate)
#define _parity (port->parity)
#define _databits (port->databits)
#define _stopbits (port->stopbits)
#define _ign_ms (port->local)
#define _rts (port->set_rts)
#define _dtr (port->set_dtr)
#define _timeout_us (port->timeout_us)
#define _flush (port->flush)
#define _errc (port->_state.error_code)
#define _errs (port->_state.error_string)
 
 
/**
 * Allowed baud rates
 */
static const unsigned long serial_port_baudrates[] = {
  50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
  #ifdef B7200
  7200,
  #endif
  9600, 19200, 38400,
  #ifdef B14400
  14400,
  #endif
  #ifdef B28800
  28800,
  #endif
  57600,
  #ifdef B76800
  76800,
  #endif
  115200, 230400, 0
};
 
/**
 * Returns a list of valid baudrates. The end of the list is marked with the
 * value 0
 *
 * @return const int*
 */
const unsigned long* serial_port_get_valid_baudrates(void)
{
  return serial_port_baudrates;
}
 
/**
 * Initialises the structure variable, resets all settings and the state to
 * 0 or the defaults, respectively.
 * @param serial_port_t *port
 * @return serial_port_error_t
 */
serial_port_error_t serial_port_init(serial_port_t *port)
{
  if(!port) seterror(E_SERIAL_NULLPOINTER, ERR_S_NULLPOINTER);
  memset(port, 0, sizeof(serial_port_t));
  port->baudrate = 9600;
  port->parity = 'n';
  port->databits = 8;
  port->stopbits = 1;
  port->local = 1;
  port->timeout_us = -1;
  port->set_dtr = -1;
  port->set_rts = -1;
  return E_SERIAL_OK;
}
 
/**
 * Opens and applies the settings defined in port. Returns 0 on success,
 * or another serial_port_error_t.
 * @param serial_port_t *port
 * @return serial_port_error_t
 */
serial_port_error_t serial_port_open(serial_port_t *port)
{
  int n;
  if(!port) {
    seterror(E_SERIAL_NULLPOINTER, ERR_S_NULLPOINTER);
  } else if((_hnd=open(_file, O_RDWR|O_NDELAY|O_NOCTTY)) < 0) {
    if(strstr(strerror(errno), "denied")) {
      seterror(E_SERIAL_OPEN, "Failed to open port '%s' (NOTE: Are you in group 'dialout'?): %s.", _file, strerror(errno))
    } else {
      seterror(E_SERIAL_OPEN, "Failed to open port '%s': %s.", _file, strerror(errno))
    }
  } else if(flock(_hnd, LOCK_EX) < 0) {
    seterror(E_SERIAL_OPEN, "Port is already in use (didn't get exclusive lock): %s (Error=%s)", _file, strerror(errno))
  } else if(!isatty(_hnd)) {
    seterror(E_SERIAL_NOTTY, "Port is no TTY: %s]\n\n", _file);
  } else if(tcgetattr(_hnd, &_attr_orig) < 0) {
    seterror(E_SERIAL_GETATTR, "Failed to get port terminal attributes: %s", strerror(errno));
  } else if(ioctl(_hnd, TIOCMGET, &_mdlns_orig) < 0) {
    seterror(E_SERIAL_GETIOC, "Failed to get port io control: %s", strerror(errno));
  }
  _attr = _attr_orig;
  _mdlns = _mdlns_orig;
  _attr.c_iflag &= ~(IGNBRK|BRKINT|ICRNL|INLCR|PARMRK|IXON);
  _attr.c_cflag &= ~(ECHO|ECHONL|ICANON|IEXTEN|ISIG|ISTRIP|CRTSCTS);
  _attr.c_oflag &= ~(OCRNL|ONLCR|ONLRET|ONOCR|OFILL|OPOST);
  _attr.c_iflag = (_attr.c_iflag & ~(IGNPAR|INPCK)) | (_parity == 'n' ? IGNPAR : INPCK);
  _attr.c_cflag = (_attr.c_cflag & ~CSTOPB)| (_stopbits > 1 ? CSTOPB : 0);
  _attr.c_cflag = (_attr.c_cflag & ~(HUPCL|CLOCAL)) | CREAD | (_ign_ms ? CLOCAL : 0);
  _attr.c_cflag = (_attr.c_cflag & ~(PARENB))| (_parity != 'n' ? PARENB : 0);
  _attr.c_cflag = (_attr.c_cflag & ~(PARODD))| (_parity == 'o' ? PARODD : 0);
  switch(_databits) {
    case 5: n = CS5; break;
    case 6: n = CS6; break;
    case 7: n = CS7; break;
    case 8: n = CS8; break;
    default: n = CS8;
  }
  _attr.c_cflag = (_attr.c_cflag & ~CSIZE) | n;
  _attr.c_cc[VTIME] = 5;
  _attr.c_cc[VMIN]  = 1;
 
  speed_t br = B9600;
  switch(_baudrate) {
    case 50: br = B50; break;
    case 75: br = B75; break;
    case 110: br = B110; break;
    case 134: br = B134; break;
    case 150: br = B150; break;
    case 200: br = B200; break;
    case 300: br = B300; break;
    case 600: br = B600; break;
    case 1200: br = B1200; break;
    case 1800: br = B1800; break;
    case 2400: br = B2400; break;
    case 4800: br = B4800; break;
    #ifdef B7200
    case 7200: br = B7200; break;
    #endif
    case 9600: br = B9600; break;
    case 19200: br = B19200; break;
    case 38400: br = B38400; break;
    #ifdef B14400
    case 14400: br = B14400; break;
    #endif
    #ifdef B28800
    case 28800: br = B28800; break;
    #endif
    case 57600: br = B57600; break;
    #ifdef B76800
    case 76800: br = B76800; break;
    #endif
    case 115200: br = B115200; break;
    case 230400: br = B230400; break;
    case 0:
    default:
      seterror(E_SERIAL_SETATTR, "Invalid baud rate.");
  }
 
  cfsetispeed(&_attr, br);
  cfsetospeed(&_attr, br);
 
  if(_rts >=0) _mdlns = (_mdlns & ~(TIOCM_RTS)) | (_rts ? TIOCM_RTS : 0);
  if(_dtr >=0) _mdlns = (_mdlns & ~(TIOCM_DTR)) | (_dtr ? TIOCM_DTR : 0);
 
  cfmakeraw(&_attr);
 
  if(tcsetattr(_hnd, _flush ? TCSAFLUSH : TCSANOW, &_attr) < 0) {
    seterror(E_SERIAL_SETATTR, "Failed to set port terminal attributes.");
  } else if(ioctl(_hnd, TIOCMSET, &_mdlns) < 0) {
    seterror(E_SERIAL_SETIOC, "Failed to set port terminal attributes.");
  } else if((_timeout_us >=0) && (fcntl(_hnd, F_SETFL, O_NONBLOCK) < 0)) {
    seterror(E_SERIAL_SETNONBLOCK, "Failed to set port nonblocking.");
  }
 
  if(_flush) tcflush(_hnd, TCIOFLUSH);
  return E_SERIAL_OK;
}
 
/**
 * Close the port
 * serial_port_t *port
 * @return serial_port_error_t
 */
serial_port_error_t serial_port_close(serial_port_t *port)
{
  int i;
  if(!port) seterror(E_SERIAL_NULLPOINTER, ERR_S_NULLPOINTER);
  tcflush(_hnd, TCIOFLUSH);
  if(port->_state.handle < 0) return E_SERIAL_OK;
  for(i=0; i<sizeof((port->_state.attr_original)); i++) {
    if(((unsigned char*)&(port->_state.attr_original))[i]) {
      tcsetattr(port->_state.handle, TCSANOW, &(port->_state.attr_original));
      ioctl(_hnd, TIOCMSET, &_mdlns_orig);
      break;
    }
  }
  close(port->_state.handle);
  port->_state.handle = -1;
  return E_SERIAL_OK;
}
 
 
/**
 * Read from the port. Returns the number of bytes read or,
 * if <0 a serial_port_error_t.
 *
 * @param serial_port_t *port
 * @param char* buffer
 * @param unsigned sz
 * @return int
 */
int serial_port_read(serial_port_t *port, char* buffer, unsigned sz)
{
  if(!port || !buffer) seterror(E_SERIAL_NULLPOINTER, ERR_S_NULLPOINTER);
  long n = 0;
  if((n=read(_hnd, buffer, sz)) >= 0) {
    return n;
  } else if(errno == EWOULDBLOCK) {
    return 0;
  } else {
    seterror(E_SERIAL_READ, "Reading port failed: %s", strerror(errno));
  }
}
 
/**
 * Waits until something was received, all sent or an error occurred. The timeout
 * it specified using "port.timeout_us".
 * @param serial_port_t *port
 * @return
 */
int serial_port_wait_for_events(serial_port_t *port, int r, int w, int e)
{
  fd_set rfds, wfds,efds; struct timeval tv;
  long n = 1;
 
  if(!port) seterror(E_SERIAL_NULLPOINTER, ERR_S_NULLPOINTER);
  FD_ZERO(&rfds); FD_ZERO(&wfds); FD_ZERO(&efds);
 
  if(_timeout_us >=0) {
    tv.tv_sec  = _timeout_us / 1000000;
    tv.tv_usec = _timeout_us % 1000000;
  } else {
    tv.tv_sec  = _timeout_us = 0; // Just return immediately if something's there
    tv.tv_usec = _timeout_us = 0;
  }
 
  if(r) { FD_SET(_hnd, &rfds); n++; }
  if(w) { FD_SET(_hnd, &wfds); n++; }
  if(e) { FD_SET(_hnd, &efds); n++; }
 
  if((n=select(n, &rfds, &wfds, &efds, &tv)) < 0) {
    switch(n) {
      case EWOULDBLOCK: return 0;
      case EBADF: seterror(E_SERIAL_SELECT, "An invalid file descriptor was given in one of the sets"); break;
      case EINTR: return E_SERIAL_OK; // A signal was caught
      case EINVAL: seterror(E_SERIAL_SELECT, "Invalid file descriptor invalid timeout"); break;
      case ENOMEM: seterror(E_SERIAL_SELECT, "Unable to allocate memory for internal select() tables"); break;
      default: seterror(E_SERIAL_SELECT, "Waiting for i/o states failed (select()<0): %s", strerror(errno));
    }
  } else if(!n) {
    return 0;
  } else {
    return ((FD_ISSET(_hnd, &rfds)?1:0)<<0) | ((FD_ISSET(_hnd, &wfds)?1:0)<<1) | ((FD_ISSET(_hnd, &efds)?1:0)<<2);
  }
}
 
/**
 * Write sz bytes. Returns number of bytes written or <0 on error
 * @param serial_port_t *port
 * @param const char* buffer
 * @param unsigned sz
 * @return int
 */
int serial_port_write(serial_port_t *port, const char* buffer, unsigned sz)
{
  unsigned int n;
  if(!port || !buffer) seterror(E_SERIAL_NULLPOINTER, ERR_S_NULLPOINTER);
  if((n=write(_hnd, buffer, sz) < 0)) {
    seterror(E_SERIAL_WRITE, "Failed write to port: %s", strerror(errno));
  }
  return n;
}
 
/**
 * Clears RX and TX buffers ("flush"). It wait_until_all_output_sent==1, waits
 * until TX buffer is completely sent.
 * @param serial_port_t *port
 * @param int wait_until_all_output_sent
 */
int serial_port_clear_buffers(serial_port_t *port, int wait_until_all_output_sent)
{
  tcflush(_hnd, TCIFLUSH);
  if(wait_until_all_output_sent) tcdrain(_hnd);
  tcflush(_hnd, TCIOFLUSH);
  return E_SERIAL_OK;
}
 
/**
 * Returns 1 if set, 0 if not set <0 on error
 * @param serial_port_t *port
 * @return int
 */
int serial_port_get_cts(serial_port_t *port)
{
  if(!port) seterror(E_SERIAL_NULLPOINTER, ERR_S_NULLPOINTER);
  int m;
  if(ioctl(_hnd, TIOCMGET, &m) < 0) {
    seterror(E_SERIAL_IOCTL, "ioctl() failed for CTS: %s", strerror(errno));
  } else {
    return (m & TIOCM_CTS) ? 1 : 0;
  }
}
 
/**
 * Returns 1 if set, 0 if not set <0 on error
 * @param serial_port_t *port
 * @return int
 */
int serial_port_get_dsr(serial_port_t *port)
{
  if(!port) seterror(E_SERIAL_NULLPOINTER, ERR_S_NULLPOINTER);
  int m;
  if(ioctl(_hnd, TIOCMGET, &m) < 0) {
    seterror(E_SERIAL_IOCTL, "ioctl() failed for DSR: %s", strerror(errno));
  } else {
    return (m & TIOCM_DSR) ? 1 : 0;
  }
}
 
/**
 * Returns 1 if set, 0 if not set <0 on error
 * @param serial_port_t *port
 * @return int
 */
int serial_port_get_dtr(serial_port_t *port)
{
  if(!port) seterror(E_SERIAL_NULLPOINTER, ERR_S_NULLPOINTER);
  int m;
  if(ioctl(_hnd, TIOCMGET, &m) < 0) {
    seterror(E_SERIAL_IOCTL, "ioctl() failed for DTR: %s", strerror(errno));
  } else {
    return (m & TIOCM_DTR) ? 1 : 0;
  }
}
 
/**
 * Returns 1 if set, 0 if not set <0 on error
 * @param serial_port_t *port
 * @return int
 */
int serial_port_get_rts(serial_port_t *port)
{
  if(!port) seterror(E_SERIAL_NULLPOINTER, ERR_S_NULLPOINTER);
  int m;
  if(ioctl(_hnd, TIOCMGET, &m) < 0) {
    seterror(E_SERIAL_IOCTL, "ioctl() failed for RTS: %s", strerror(errno));
  } else {
    return (m & TIOCM_RTS) ? 1 : 0;
  }
}
 
/**
 * Returns 0 on success <0 on error
 * @param serial_port_t *port
 * @return int
 */
int serial_port_set_dtr(serial_port_t *port, int DTR)
{
  if(!port) seterror(E_SERIAL_NULLPOINTER, ERR_S_NULLPOINTER);
  int m;
  if(ioctl(_hnd, TIOCMGET, &m) < 0) {
    seterror(E_SERIAL_IOCTL, "ioctl() failed for set DTR: %s", strerror(errno));
  }
  m = (m & ~TIOCM_DTR) | (DTR ? TIOCM_DTR : 0);
  if(ioctl(_hnd, TIOCMSET, &m) < 0) {
    seterror(E_SERIAL_IOCTL, "ioctl() failed for set DTR: %s", strerror(errno));
  }
  return 0;
}
 
/**
 * Returns 0 on success <0 on error
 * @param serial_port_t *port
 * @return int
 */
int serial_port_set_rts(serial_port_t *port, int RTS)
{
  if(!port) seterror(E_SERIAL_NULLPOINTER, ERR_S_NULLPOINTER);
  int m;
  if(ioctl(_hnd, TIOCMGET, &m) < 0) {
    seterror(E_SERIAL_IOCTL, "ioctl() failed for set RTS: %s", strerror(errno));
  }
  m = (m & ~TIOCM_RTS) | (RTS ? TIOCM_RTS : 0);
  if(ioctl(_hnd, TIOCMSET, &m) < 0) {
    seterror(E_SERIAL_IOCTL, "ioctl() failed for set RTS: %s", strerror(errno));
  }
  return 0;
}
 
 
void serial_port_dump(serial_port_t *port)
{
  fprintf(stderr,
      "serial_port = {\n"
      "  file           = %s\n"
      "  baudrate       = %d\n"
      "  parity         = %c\n"
      "  databits       = %d\n"
      "  stopbits       = %d\n"
      "  local          = %d\n"
      "  flush          = %d\n"
      "  set_dtr        = %d\n"
      "  set_rts        = %d\n"
      "  timeout_us     = %lld\n"
      "  _state = {\n"
      "    error_code   = %d\n"
      "    error_string = %s\n"
      "    handle       = %d\n"
      "    mdlns        = %08xh\n"
      "    mdlns_orig   = %08xh\n"
      "    attr = {\n"
      "      c_iflag    = %08xh\n"
      "      c_oflag    = %08xh\n"
      "      c_cflag    = %08xh\n"
      "      c_lflag    = %08xh\n"
      "      c_cc       = (string)\n"
      "      c_ispeed   = %d\n"
      "      c_ospeed   = %d\n"
      "    }\n"
      "    attr_orig = {\n"
      "      c_iflag    = %08xh\n"
      "      c_oflag    = %08xh\n"
      "      c_cflag    = %08xh\n"
      "      c_lflag    = %08xh\n"
      "      c_cc       = (string)\n"
      "      c_ispeed   = %d\n"
      "      c_ospeed   = %d\n"
      "    }\n"
      "  }\n"
      "};\n",
      _file, _baudrate, _parity, _databits, _stopbits, _ign_ms, _flush, _dtr,
      _rts, _timeout_us, _errc, _errs, _hnd, _mdlns, _mdlns_orig,
      (unsigned)_attr.c_iflag, (unsigned)_attr.c_oflag, (unsigned)_attr.c_cflag,
      (unsigned)_attr.c_lflag, (unsigned)_attr.c_ispeed, (unsigned)_attr.c_ospeed,
      (unsigned)_attr_orig.c_iflag, (unsigned)_attr_orig.c_oflag, (unsigned)_attr_orig.c_cflag,
      (unsigned)_attr_orig.c_lflag, (unsigned)_attr_orig.c_ispeed, (unsigned)_attr_orig.c_ospeed
    );
}
