#ifdef libwebsockets_FOUND
#ifndef WS_H
#define WS_H

#include <libwebsockets.h>

extern volatile uint8_t pi;

int callback_websocket(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

static struct lws_protocols protocols[] = {
    {
        "piled",
        callback_websocket,
        0,
        0,
    },
    {NULL, NULL, 0, 0} // terminator
};

void ws_server_init(uint8_t pi_);

#endif
#endif