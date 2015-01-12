#include "txradar.hpp"

int main()
{
    txradar radar;
    radar.start(true, 1, target_connections);
    radar.keep_pushing_count();
    return 0;
}

