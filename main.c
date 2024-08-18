#include <stdio.h>
#include "server.h"

int main()
{
    if (start_server(3384) < 0) {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }
    return 0;
}
