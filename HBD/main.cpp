#include "HBD.h"

int main(int argc, char *argv[]) {
  HBD App1;
  App1.start();

  while(!App1.isStopped()) sleep(1);
  return 0;
}
