#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_CHILDREN 128

struct Child {
  char *name;
  char *cmd;
  pid_t pid;
  // TODO: add respawn counter/timer
};

struct Children {
  struct Child children[MAX_CHILDREN];
  int num_children;
};

void establish_handler() {
  // TODO: implement
}

struct Children* parse_config() {
  struct Children *chlds = malloc(sizeof(struct Children));

  struct Child chld1 = {.name = "true", .cmd = "/bin/true"};
  struct Child chld2 = {.name = "false", .cmd = "/bin/false"};
  chlds->children[0] = chld1;
  chlds->num_children++;
  chlds->children[1] = chld2;
  chlds->num_children++;

  // TODO: actual parsing

  return chlds;
}

void spawn_child(struct Child *chld) {
  // TODO: implement
}

void respawn(int signal) {
  int status;
  pid_t child_pid;

  while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
    // TODO: Lookup the pid and start the dead child again.
    // TODO: Possibly add some data about respawns and add a backoff algorithm.
  }
}

int main(int argc, char *argv[]) {
  // TODO:  establish_handler()

  struct Children *chlds = parse_config();

  for (int i = 0; i < chlds->num_children; i++) {
    struct Child chld = chlds->children[i];
    printf("%d %s %s\n", i, chld.name, chld.cmd);
    // TODO: spawn_child(child)
  }

  return 0;
}
