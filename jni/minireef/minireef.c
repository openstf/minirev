#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_SOCKET_PATH "/data/local/tmp/minireef.sock"

static void usage(const char* pname)
{
  fprintf(stderr,
    "Usage: %s [-h] [-s <socket>]\n"
    "  -s <socket>: Start a unix domain socket at the given path. (%s)\n"
    "  -h:          Show help.\n",
    pname, DEFAULT_SOCKET_PATH
  );
}

int main(int argc, char* argv[])
{
  const char* pname = argv[0];
  char* socket = DEFAULT_SOCKET_PATH;

  int opt;
  while ((opt = getopt(argc, argv, "s:h")) != -1) {
    switch (opt) {
      case 's':
        socket = optarg;
        break;
      case '?':
        usage(pname);
        return EXIT_FAILURE;
      case 'h':
        usage(pname);
        return EXIT_SUCCESS;
    }
  }

  return EXIT_SUCCESS;
}
