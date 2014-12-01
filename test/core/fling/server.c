/*
 *
 * Copyright 2014, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <grpc/grpc.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "test/core/util/grpc_profiler.h"
#include "test/core/util/test_config.h"
#include <grpc/support/alloc.h>
#include <grpc/support/cmdline.h>
#include <grpc/support/host_port.h>
#include <grpc/support/log.h>
#include <grpc/support/time.h>
#include "test/core/util/port.h"

static grpc_completion_queue *cq;
static grpc_server *server;
static int done = 0;

static const grpc_status status_ok = {GRPC_STATUS_OK, NULL};

typedef struct {
  gpr_refcount pending_ops;
  gpr_uint32 flags;
} call_state;

static void request_call() {
  call_state *s = gpr_malloc(sizeof(call_state));
  gpr_ref_init(&s->pending_ops, 2);
  grpc_server_request_call(server, s);
}

static void sigint_handler(int x) { done = 1; }

int main(int argc, char **argv) {
  grpc_event *ev;
  call_state *s;
  char *addr_buf = NULL;
  gpr_cmdline *cl;

  int secure = 0;
  char *addr = NULL;

  char *fake_argv[1];

  GPR_ASSERT(argc >= 1);
  fake_argv[0] = argv[0];
  grpc_test_init(1, fake_argv);

  grpc_init();
  srand(clock());

  cl = gpr_cmdline_create("fling server");
  gpr_cmdline_add_string(cl, "bind", "Bind host:port", &addr);
  gpr_cmdline_add_flag(cl, "secure", "Run with security?", &secure);
  gpr_cmdline_parse(cl, argc, argv);
  gpr_cmdline_destroy(cl);

  if (addr == NULL) {
    gpr_join_host_port(&addr_buf, "::", grpc_pick_unused_port_or_die());
    addr = addr_buf;
  }
  gpr_log(GPR_INFO, "creating server on: %s", addr);

  cq = grpc_completion_queue_create();
  server = grpc_server_create(cq, NULL);
  GPR_ASSERT(grpc_server_add_http2_port(server, addr));
  grpc_server_start(server);

  gpr_free(addr_buf);
  addr = addr_buf = NULL;

  request_call();

  grpc_profiler_start("server.prof");
  signal(SIGINT, sigint_handler);
  while (!done) {
    ev = grpc_completion_queue_next(
        cq, gpr_time_add(gpr_now(), gpr_time_from_micros(1000000)));
    if (!ev) continue;
    s = ev->tag;
    switch (ev->type) {
      case GRPC_SERVER_RPC_NEW:
        /* initial ops are already started in request_call */
        if (0 == strcmp(ev->data.server_rpc_new.method,
                        "/Reflector/reflectStream")) {
          s->flags = 0;
        } else {
          s->flags = GRPC_WRITE_BUFFER_HINT;
        }
        grpc_call_accept(ev->call, cq, s, s->flags);
        GPR_ASSERT(grpc_call_start_read(ev->call, s) == GRPC_CALL_OK);
        request_call();
        break;
      case GRPC_WRITE_ACCEPTED:
        GPR_ASSERT(ev->data.write_accepted == GRPC_OP_OK);
        GPR_ASSERT(grpc_call_start_read(ev->call, s) == GRPC_CALL_OK);
        break;
      case GRPC_READ:
        if (ev->data.read) {
          GPR_ASSERT(grpc_call_start_write(ev->call, ev->data.read, s,
                                           s->flags) == GRPC_CALL_OK);
        } else {
          GPR_ASSERT(grpc_call_start_write_status(ev->call, status_ok, s) ==
                     GRPC_CALL_OK);
        }
        break;
      case GRPC_FINISH_ACCEPTED:
      case GRPC_FINISHED:
        if (gpr_unref(&s->pending_ops)) {
          grpc_call_destroy(ev->call);
          gpr_free(s);
        }
        break;
      default:
        abort();
    }
    grpc_event_finish(ev);
  }
  grpc_profiler_stop();

  grpc_shutdown();
  return 0;
}