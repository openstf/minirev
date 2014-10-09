#ifndef MINIREV_H
#define MINIREV_H

#include <uthash.h>

#define VERSION 1
#define DEFAULT_SOCKET_NAME "minirev"
#define DEFAULT_MAX_EVENTS 64
#define MAX_PACKET_SIZE 0xFFFF
#define HEADER_SIZE 4

#define LIKELY(x)       __builtin_expect((x),1)
#define UNLIKELY(x)     __builtin_expect((x),0)

//#define D(...)          fprintf(stderr, __VA_ARGS__)
#define D(...)

typedef enum {
  CONTROL_SERVER      = 1,
  CONTROL_CONNECTION  = 2,
  FORWARD_SERVER      = 3,
  FORWARD_CONNECTION  = 4,
} event_source_type_t;

typedef struct
{
  // The fd, also works as a unique key for uthash.
  int fd;
  // Type.
  event_source_type_t type;
  // Source port if applicable. Most types have it.
  int port;
  // Target if applicable.
  int target;
  // The multiplex current header buffer. This could be removed with trickery.
  char mpheader[HEADER_SIZE];
  // How many bytes we still need to get for the current target. If negative,
  // we're waiting for a header.
  int mplength;
  // Multiplex target.
  int mptarget;
  // Hash handle for uthash.
  UT_hash_handle hh;
} event_source_t;

event_source_t* make_event_source(int fd, event_source_type_t type)
{
  event_source_t* source = calloc(1, sizeof(event_source_t));
  source->fd = fd;
  source->type = type;
  return source;
}

#endif
