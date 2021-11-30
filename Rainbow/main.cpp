#include "rainbow.h"

int main(int argc, char *argv[]) {
    Rainbow App1;
    App1.start();

    while(!App1.isStopped()) sleep(2);
    return 0;
}
