#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <uthash.h>

#include "minirev.h"

static void
usage(const char* pname)
{
  fprintf(stderr,
    "Usage: %s [-h] [-n <name>]\n"
    "  -n <name>:   Change the name of of the abtract unix domain socket. (%s)\n"
    "  -h:          Show help.\n",
    pname, DEFAULT_SOCKET_NAME
  );
}

static int
make_socket_non_blocking(int sfd)
{
  int flags, s;

  flags = fcntl(sfd, F_GETFL, 0);
  if (flags == -1)
  {
    perror("fcntl");
    return -1;
  }

  flags |= O_NONBLOCK;
  s = fcntl(sfd, F_SETFL, flags);
  if (s == -1)
  {
    perror("fcntl");
    return -1;
  }

  return 0;
}

static int
start_abstract_server(char* sockname)
{
  int sfd;
  struct sockaddr_un addr;

  sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sfd < 0)
  {
    perror("creating socket");
    return sfd;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(&addr.sun_path[1], sockname, strlen(sockname));

  if (bind(sfd, (struct sockaddr*) &addr,
    sizeof(sa_family_t) + strlen(sockname) + 1) < 0)
  {
    perror("binding socket");
    close(sfd);
    return -1;
  }

  return sfd;
}

static int
start_inet_server(int port)
{
  int sfd;
  struct sockaddr_in addr;
  int yes = 1;

  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd < 0)
  {
    perror("creating socket");
    return sfd;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0)
  {
    perror("setsockopt");
    close(sfd);
    return -1;
  }

  if (bind(sfd, (struct sockaddr*) &addr, sizeof(addr)) < 0)
  {
    perror("binding socket");
    close(sfd);
    return -1;
  }

  return sfd;
}

static int
pump(int fd, const void* buf, size_t count)
{
  int n = 0;

  do
  {
    n = write(fd, buf + n, count - n);

    if (n < 0)
    {
      if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
      {
        // @todo
      }

      perror("write");
    }
  }
  while (n < count);

  return n;
}

static void
insert_source(event_source_t** sources, event_source_t* source)
{
  D("%6d insert\n", source->fd);
  HASH_ADD_INT(*sources, fd, source);
}

static void delete_source(event_source_t**, event_source_t*);

static void
delete_sources(event_source_t** sources, event_source_type_t type, int port)
{
  event_source_t *cursor, *tmp;

  HASH_ITER(hh, *sources, cursor, tmp)
  {
    if (cursor->type == type && cursor->port == port)
    {
      delete_source(sources, cursor);
    }
  }
}

static void
delete_source(event_source_t** sources, event_source_t* source)
{
  if (source == NULL)
    return;

  D("%6d delete\n", source->fd);

  switch (source->type)
  {
  case CONTROL_CONNECTION:
    delete_sources(sources, FORWARD_SERVER, source->port);
    break;
  case FORWARD_SERVER:
    delete_sources(sources, FORWARD_CONNECTION, source->port);
    break;
  }

  HASH_DEL(*sources, source);
  close(source->fd);
  free(source);
}

static int
bind_control_server(event_source_t* source, int efd)
{
  D("%6d bind_control_server\n", source->fd);

  int sfd = start_inet_server(source->port);
  struct epoll_event ev;

  if (sfd < 0)
  {
    return sfd;
  }

  if (make_socket_non_blocking(sfd) < 0)
  {
    close(sfd);
    return -1;
  }

  if (listen(sfd, SOMAXCONN) < 0)
  {
    perror("listen");
    close(sfd);
    return -1;
  }

  ev.data.fd = sfd;
  ev.events = EPOLLIN | EPOLLET;

  if (epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &ev) < 0)
  {
    perror("epoll_ctl");
    close(sfd);
    return -1;
  }

  return sfd;
}

static int
handle_control_accept(event_source_t** sources, event_source_t* source, int efd)
{
  D("%6d handle_control_accept\n", source->fd);

  int infd;
  struct epoll_event event;
  event_source_t* asource;

  // We have a notification on the listening socket, which
  // means one or more incoming connections.
  for (;;)
  {
    infd = accept(source->fd, NULL, 0);

    if (infd < 0)
    {
      if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
      {
        // We have processed all incoming connections.
        break;
      }
      else
      {
        perror("accept");
        return infd;
      }
    }

    if (make_socket_non_blocking(infd) < 0)
    {
      // Disaster, never supposed to happen.
      return -1;
    }

    asource = make_event_source(infd, CONTROL_CONNECTION);
    asource->mplength = -HEADER_SIZE;
    insert_source(sources, asource);

    event.data.fd = infd;
    event.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event) < 0)
    {
      perror("epoll_ctl");
      abort();
    }
  }

  return 0;
}

static int
handle_control_read(event_source_t** sources, event_source_t* source, int efd)
{
  D("%6d handle_control\n", source->fd);

  ssize_t count, cursor;
  uint16_t fd, length;
  char buf[MAX_PACKET_SIZE];
  event_source_t* fsource;

  for (;;)
  {
    count = read(source->fd, buf, sizeof(buf));
    cursor = 0;

    D("%6d read %d bytes\n", source->fd, count);

    if (count < 0)
    {
      // If errno == EAGAIN, that means we have read all data.
      // Either way, we're done here.
      if (errno != EAGAIN)
      {
        perror("read");
        delete_source(sources, source);
        return count;
      }

      break;
    }

    if (count == 0)
    {
      // End of file. The remote has closed the connection.
      // @todo Make sure we leave no clients behind.
      delete_source(sources, source);
      break;
    }

    while (cursor < count)
    {
      D("%6d cursor %d, count %d\n", source->fd, cursor, count);

      // Do we need a header?
      if (source->mplength < 0)
      {
        // Still missing a header?
        if (count < -source->mplength)
        {
          D("%6d partial header\n", source->fd);

          // Save what we've received so far.
          memcpy(source->mpheader + HEADER_SIZE + source->mplength,
            buf + cursor, count);
          source->mplength += count;
          break;
        }

        // We've got a complete header, maybe more.
        memcpy(source->mpheader + HEADER_SIZE + source->mplength,
          buf + cursor, -source->mplength);

        cursor += -source->mplength;

        source->target = source->mpheader[0] + (source->mpheader[1] << 8);
        source->mplength = source->mpheader[2] + (source->mpheader[3] << 8);

        D("%6d header %d %d\n", source->fd, source->target, source->mplength);

        if (source->target == 0)
        {
          // In this case mplength is a port we're supposed to bind to.
          source->port = source->mplength;

          int ffd = bind_control_server(source, efd);

          if (ffd < 0)
          {
            delete_source(sources, source);
            return -1;
          }

          fsource = make_event_source(ffd, FORWARD_SERVER);
          fsource->target = source->fd;
          fsource->port = source->port;
          insert_source(sources, fsource);

          fprintf(stderr, "Forwarding port %d\n", source->port);

          source->mplength = -HEADER_SIZE;
        }
      }
      // Do we have a full data packet?
      else if (count - cursor >= source->mplength)
      {
        D("%6d full packet %d %d\n", source->fd, source->target, source->mplength);

        // We may have more bytes than we need.
        if (pump(source->target, buf + cursor, source->mplength) < 0)
        {
          delete_source(sources, source);
          return -1;
        }

        cursor += source->mplength;
        source->mplength = -HEADER_SIZE;
      }
      // We have a partial data packet.
      else
      {
        D("%6d partial packet %d %d\n", source->fd, source->target, source->mplength);

        // We have read less bytes than we need, just pump everything.
        if (pump(source->target, buf + cursor, count - cursor) < 0)
        {
          delete_source(sources, source);
          return -1;
        }

        source->mplength -= count - cursor;
        cursor += count - cursor;
      }
    }
  }

  return 0;
}

static int
handle_forward_accept(event_source_t** sources, event_source_t* source, int efd)
{
  D("%6d handle_forward_accept\n", source->fd);

  int infd;
  struct epoll_event event;
  event_source_t* asource;

  // We have a notification on the listening socket, which
  // means one or more incoming connections.
  for (;;)
  {
    infd = accept(source->fd, NULL, 0);

    if (infd < 0)
    {
      if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
      {
        // We have processed all incoming connections.
        break;
      }
      else
      {
        perror("accept");
        return infd;
      }
    }

    if (make_socket_non_blocking(infd) < 0)
    {
      // Disaster, never supposed to happen.
      return -1;
    }

    asource = make_event_source(infd, FORWARD_CONNECTION);
    asource->port = source->port;
    asource->target = source->target;
    insert_source(sources, asource);

    event.data.fd = infd;
    event.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event) < 0)
    {
      perror("epoll_ctl");
      return -1;
    }
  }

  return 0;
}

static int
handle_forward_read(event_source_t** sources, event_source_t* source)
{
  D("%6d handle_forward_read\n", source->fd);

  ssize_t count;
  char buf[HEADER_SIZE + MAX_PACKET_SIZE];

  for (;;)
  {
    count = read(source->fd, buf + HEADER_SIZE, sizeof(buf));

    if (count < 0)
    {
      // If errno == EAGAIN, that means we have read all data.
      // Either way, we're done here.
      if (errno != EAGAIN)
      {
        perror("read");
        delete_source(sources, source);
        return count;
      }

      break;
    }

    if (count == 0)
    {
      // End of file. The remote has closed the connection.
      delete_source(sources, source);
      break;
    }

    // Header
    buf[0] = (source->fd >> 8);
    buf[1] = (source->fd);
    buf[2] = (count >> 8);
    buf[3] = (count);

    D("%6d pump %d to %d\n", source->fd, HEADER_SIZE + count, source->target);

    if (pump(source->target, buf, HEADER_SIZE + count) < 0)
    {
      break;
    }
  }

  return 0;
}

int
main(int argc, char* argv[])
{
  const char* pname = argv[0];
  char* sockname = DEFAULT_SOCKET_NAME;
  int max_events = DEFAULT_MAX_EVENTS;
  struct sockaddr_un client_addr;
  int opt;
  int sfd, efd;
  struct epoll_event event;
  struct epoll_event* events;
  event_source_t* sources = NULL;
  event_source_t* source;
  event_source_t* main_source;

  while ((opt = getopt(argc, argv, "n:h")) != -1) {
    switch (opt) {
      case 'n':
        sockname = optarg;
        break;
      case '?':
        usage(pname);
        return EXIT_FAILURE;
      case 'h':
        usage(pname);
        return EXIT_SUCCESS;
    }
  }

  socklen_t client_addr_length = sizeof(client_addr);

  sfd = start_abstract_server(sockname);
  if (sfd < 0)
  {
    fprintf(stderr, "Unable to start server on %s\n", sockname);
    return EXIT_FAILURE;
  }

  if (make_socket_non_blocking(sfd) < 0)
  {
    return EXIT_FAILURE;
  }

  if (listen(sfd, SOMAXCONN) < 0)
  {
    perror("listen");
    return EXIT_FAILURE;
  }

  // Sadly epoll_create1() doesn't seem to be included in the NDK, or
  // at the very least not on the platform we're tied to.
  if ((efd = epoll_create(10)) < 0)
  {
    perror("epoll_create");
    return EXIT_FAILURE;
  }

  event.data.fd = sfd;
  event.events = EPOLLIN | EPOLLET;

  if (epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event) < 0)
  {
    perror("epoll_ctl");
    return EXIT_FAILURE;
  }

  events = calloc(max_events, sizeof(event));

  main_source = make_event_source(sfd, CONTROL_SERVER);
  insert_source(&sources, main_source);

  for (;;)
  {
    int n, i;

    // Since we're using Edge-Triggered mode (EPOLLET), epoll_wait()
    // won't notify us twice about the same event. We must make sure
    // that we always consume all events.
    n = epoll_wait(efd, events, max_events, -1);

    for (i = 0; i < n; ++i)
    {
      // Find the connection that caused the event, which may or
      // may not exist at this point.
      HASH_FIND_INT(sources, &(events[i].data.fd), source);

      if (UNLIKELY(source == NULL))
      {
        // Impossible.
        fprintf(stderr, "Received event for unmapped event source %d.\n",
          events[i].data.fd);
        close(events[i].data.fd);
        return EXIT_FAILURE;
      }

      if (UNLIKELY(events[i].events & EPOLLERR))
      {
        // An error occured on this fd.
	      fprintf(stderr, "epoll error\n");
	      delete_source(&sources, source);
        continue;
      }

      if (UNLIKELY(events[i].events & EPOLLHUP))
      {
        // Unexpected close of socket, can happen on Ctrl+C. Treat
        // as a normal case.
        delete_source(&sources, source);
        continue;
      }

      if (UNLIKELY(!(events[i].events & EPOLLIN)))
      {
        // The socket is not ready for reading, but we're not
        // listening for anything else. How did we get notified?
        // Assume some kind of an error.
        delete_source(&sources, source);
        continue;
      }

      switch (source->type)
      {
      case CONTROL_SERVER:
        handle_control_accept(&sources, source, efd);
        break;
      case FORWARD_SERVER:
        handle_forward_accept(&sources, source, efd);
        break;
      case CONTROL_CONNECTION:
        handle_control_read(&sources, source, efd);
        break;
      case FORWARD_CONNECTION:
        handle_forward_read(&sources, source);
        break;
      default:
        // Should be impossible unless there's a bug.
        fprintf(stderr, "Unknown event source type %d\n", source->type);
        return EXIT_FAILURE;
      }
    }
  }

  free(events);
  close(sfd);

  return EXIT_SUCCESS;
}
