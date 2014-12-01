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

#ifndef __GRPCPP_CLIENT_CONTEXT_H__
#define __GRPCPP_CLIENT_CONTEXT_H__

#include <chrono>
#include <string>
#include <vector>

#include <grpc++/config.h>

using std::chrono::system_clock;

struct grpc_call;
struct grpc_completion_queue;

namespace grpc {

class ClientContext {
 public:
  ClientContext();
  ~ClientContext();

  void AddMetadata(const grpc::string &meta_key,
                   const grpc::string &meta_value);

  void set_absolute_deadline(const system_clock::time_point &deadline);
  system_clock::time_point absolute_deadline();

  void StartCancel();

 private:
  // Disallow copy and assign.
  ClientContext(const ClientContext &);
  ClientContext &operator=(const ClientContext &);

  friend class Channel;
  friend class StreamContext;

  grpc_call *call() { return call_; }
  void set_call(grpc_call *call) { call_ = call; }

  grpc_completion_queue *cq() { return cq_; }
  void set_cq(grpc_completion_queue *cq) { cq_ = cq; }

  grpc_call *call_;
  grpc_completion_queue *cq_;
  system_clock::time_point absolute_deadline_;
  std::vector<std::pair<grpc::string, grpc::string> > metadata_;
};

}  // namespace grpc

#endif  // __GRPCPP_CLIENT_CONTEXT_H__