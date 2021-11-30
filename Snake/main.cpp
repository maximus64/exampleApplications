//matrix app
#include "snake.h"

int main(int argc, char *argv[]) {
  Snake App1;
  App1.start();

  while(!App1.isStopped()) sleep(2);
  return 0;
}
