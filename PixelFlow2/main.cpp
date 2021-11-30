#include "pixelflow2.h"

int main(int argc, char *argv[]) {
    PixelFlow2 App1;
    App1.start();

    while(!App1.isStopped()) sleep(2);
    return 0;
}
