#include "music.h"

int main(int argc, char *argv[]) {
  Music App1;
  App1.start();

  while(!App1.isStopped()) sleep(1);
  return 0;
}
