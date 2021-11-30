#include "Blackout3D.h"


int main(int argc, char *argv[]) {
    Blackout3D App1;
    App1.start();

    while(!App1.isStopped()) sleep(1);
    return 0;
}
