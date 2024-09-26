#ifdef microhttpd_FOUND
#include "../rgb/gpio.h"
#include "../utils/utils.h"
#include "server.h"
#include <microhttpd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 3386

uint8_t pi;

static enum MHD_Result answer_to_connection(void *cls, struct MHD_Connection *connection, const char *url,
                                            const char *method, const char *version, const char *upload_data,
                                            size_t *upload_data_size, void **con_cls) {
    if (strcmp(method, "GET") != 0) {
        return MHD_NO;
    }

    const char *RED_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "R");
    const char *GREEN_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "G");
    const char *BLUE_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "B");
    const char *DURATION_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "DURATION");

    if (!RED_str || !GREEN_str || !BLUE_str || !DURATION_str) {
        return MHD_NO;
    }

    int red = atoi(RED_str);
    int green = atoi(GREEN_str);
    int blue = atoi(BLUE_str);
    int duration = atoi(DURATION_str);

    logger("HTTP: Received colors: R=%d, G=%d, B=%d, Duration=%d s\n", red, green, blue, duration);
    set_color_duration(pi, (struct Color){red, green, blue}, duration);
    int ret;
    struct MHD_Response *response;
    response = MHD_create_response_from_buffer(strlen("OK"), (void *)"OK", MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
}

void *http_server_thread(void *arg) {
    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL, &answer_to_connection, NULL, MHD_OPTION_END);
    if (NULL == daemon) {
        logger("Failed to start the HTTP server");
        return NULL;
    }

    logger("Started HTTP Server on port %d", PORT);

    while (!stop_server)
        ;

    MHD_stop_daemon(daemon);
    return NULL;
}

void start_http_server(uint8_t pi_) {
    pi = pi_;
    pthread_t server_thread;

    if (pthread_create(&server_thread, NULL, http_server_thread, NULL) != 0) {
        logger("Failed to create HTTP server thread");
        return;
    }

    pthread_detach(server_thread);
}

#endif
