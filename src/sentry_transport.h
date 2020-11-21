#ifndef SENTRY_TRANSPORT_H_INCLUDED
#define SENTRY_TRANSPORT_H_INCLUDED

#include "sentry_boot.h"

typedef struct sentry_dsn_s sentry_dsn_t;
typedef struct sentry_run_s sentry_run_t;
typedef struct sentry_rate_limiter_s sentry_rate_limiter_t;

/**
 * Sets the dump function of the transport.
 *
 * This function is called during a hard crash to dump any internal send queue
 * to disk, using `sentry__run_write_envelope`. The function runs inside a
 * signal handler, and appropriate restrictions apply.
 */
void sentry__transport_set_dump_func(sentry_transport_t *transport,
    size_t (*dump_func)(sentry_run_t *run, void *state));

/**
 * Submit the given envelope to the transport.
 */
void sentry__transport_send_envelope(
    sentry_transport_t *transport, sentry_envelope_t *envelope);

/**
 * Calls the transports startup hook.
 *
 * Returns 0 on success.
 */
int sentry__transport_startup(
    sentry_transport_t *transport, const sentry_options_t *options);

/**
 * Instructs the transport to shut down.
 *
 * Returns 0 on success.
 */
int sentry__transport_shutdown(sentry_transport_t *transport, uint64_t timeout);

/**
 * This will create a new platform specific HTTP transport.
 */
sentry_transport_t *sentry__transport_new_default(void);

/**
 * This function will instruct the platform specific transport to dump all the
 * envelopes in its send queue to disk.
 */
size_t sentry__transport_dump_queue(
    sentry_transport_t *transport, sentry_run_t *run);

#endif
