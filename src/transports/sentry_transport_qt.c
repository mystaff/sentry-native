#include "sentry_alloc.h"
#include "sentry_core.h"
#include "sentry_database.h"
#include "sentry_envelope.h"
#include "sentry_options.h"
#include "sentry_ratelimiter.h"
#include "sentry_string.h"
#include "sentry_sync.h"
#include "sentry_transport.h"
#include "sentry_utils.h"

#include <stdlib.h>
#include <string.h>

typedef struct qt_transport_state_s {
    sentry_dsn_t *dsn;
    sentry_rate_limiter_t *ratelimiter;
    void (*func)(const char *url, const char *body, const long bodyLen,
        const char *headers, void *data, void *state);
    void *data;
    bool debug;
} qt_transport_state_t;

static qt_transport_state_t *
sentry__qt_transport_state_new(void)
{
    qt_transport_state_t *state = SENTRY_MAKE(qt_transport_state_t);
    if (!state) {
        return NULL;
    }
    memset(state, 0, sizeof(qt_transport_state_t));

    state->ratelimiter = sentry__rate_limiter_new();

    return state;
}

static void
sentry__qt_transport_state_free(void *_state)
{
    qt_transport_state_t *state = _state;
    sentry__dsn_decref(state->dsn);
    sentry__rate_limiter_free(state->ratelimiter);
    sentry_free(state);
}

static int
sentry__qt_transport_start(
    const sentry_options_t *options, void *transport_state)
{
    qt_transport_state_t *state = (qt_transport_state_t *)transport_state;

    state->dsn = sentry__dsn_incref(options->dsn);
    state->debug = options->debug;

    return 0;
}

static int
sentry__qt_transport_shutdown(
    uint64_t UNUSED(timeout), void *UNUSED(transport_state))
{
    return 0;
}

static void
sentry__qt_transport_send_envelope(sentry_envelope_t *envelope, void *_state)
{
    qt_transport_state_t *state = (qt_transport_state_t *)_state;

    sentry_prepared_http_request_t *req = sentry__prepare_http_request(
        envelope, state->dsn, state->ratelimiter);
    if (!req) {
        sentry_envelope_free(envelope);
        return;
    }

    char buf[2048];
    buf[0] = '\0';
    int written = 0;
    for (size_t i = 0; i < req->headers_len && written < 2048; i++) {
        written += snprintf(buf + written, sizeof(buf) - written, "%s:%s",
            req->headers[i].key, req->headers[i].value);
        SENTRY_DEBUGF("sentry__qt_transport_send_envelope: written %s - %d",
            buf, written);
        if (written > 2048) {
            SENTRY_WARN("sentry__qt_transport_send_envelope: header buffer "
                        "size exceeded");
        }
    }

    state->func(req->url, req->body, req->body_len, buf, state->data, state);

    sentry__prepared_http_request_free(req);
    sentry_envelope_free(envelope);
}

void
sentry_qt_transport_process_response(
    void *_state, char *rate_limits, char *retry_after)
{
    qt_transport_state_t *state = (qt_transport_state_t *)_state;

    if (rate_limits) {
        sentry__rate_limiter_update_from_header(
            state->ratelimiter, rate_limits);
    } else if (retry_after) {
        sentry__rate_limiter_update_from_http_retry_after(
            state->ratelimiter, retry_after);
    }
}

sentry_transport_t *
sentry_new_qt_transport(
    void (*func)(const char *url, const char *body, const long bodyLen,
        const char *headers, void *data, void *state),
    void *data)
{
    SENTRY_DEBUG("initializing qt transport");
    qt_transport_state_t *state = sentry__qt_transport_state_new();
    if (!state) {
        return NULL;
    }
    state->func = func;
    state->data = data;

    sentry_transport_t *transport
        = sentry_transport_new(sentry__qt_transport_send_envelope);
    if (!transport) {
        sentry__qt_transport_state_free(state);
        return NULL;
    }
    sentry_transport_set_state(transport, state);
    sentry_transport_set_free_func(transport, sentry__qt_transport_state_free);
    sentry_transport_set_startup_func(transport, sentry__qt_transport_start);
    sentry_transport_set_shutdown_func(
        transport, sentry__qt_transport_shutdown);

    return transport;
}
