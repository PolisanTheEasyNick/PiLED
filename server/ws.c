#ifdef libwebsockets_FOUND
#include "ws.h"
#include "../globals/globals.h"
#include "../rgb/gpio.h"
#include "../utils/utils.h"
#include "server.h"
#include <pthread.h>

int callback_websocket(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {

    if (is_suspended) {
        return 0;
    }

    switch (reason) {
    case LWS_CALLBACK_RECEIVE: {
        // expecting the input in the format: "RED,GREEN,BLUE,DURATION"
        char *msg = (char *)in;
        char *token;
        int red, green, blue, duration;

        token = strtok(msg, ",");
        if (token)
            red = atoi(token);
        token = strtok(NULL, ",");
        if (token)
            green = atoi(token);
        token = strtok(NULL, ",");
        if (token)
            blue = atoi(token);
        token = strtok(NULL, ",");
        if (token)
            duration = atoi(token);

        set_color_duration(pi, (struct Color){red, green, blue}, duration);
    }
    default:
        break;
    }
    return 0;
}

void *event_loop(void *arg) {
    struct lws_context *context = (struct lws_context *)arg;

    while (!stop_server) {
        lws_service(context, 0);
    }

    return NULL;
}

void ws_server_init(uint8_t pi_) {
    pi = pi_;
    struct lws_context_creation_info info;
    struct lws_context *context;

    memset(&info, 0, sizeof(info));
    info.port = 3385;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

    lws_set_log_level(0, NULL);

    context = lws_create_context(&info);
    if (!context) {
        logger_debug(WS, "lws create context failed\n");
        return;
    }

    pthread_t thread;
    if (pthread_create(&thread, NULL, event_loop, context) != 0) {
        logger_debug(WS, "Failed to create event loop thread\n");
        lws_context_destroy(context);
        return;
    }
    logger(WS, "Started WS server on port 3385 successfully!");

    pthread_detach(thread);
}

#endif