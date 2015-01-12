#include "txradar.hpp"

int main()
{
    txradar radar;
    radar.start(true, 1, target_connections);
    pause();
    return 0;
}

