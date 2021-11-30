#include "picture.h"

int main(int argc, char *argv[]) {
    Picture App1(argc, argv);
    App1.start();

    while(!App1.isStopped()) sleep(2);
    return 0;
}
