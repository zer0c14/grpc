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

#include "src/cpp/server/server_rpc_handler.h"

#include <grpc/support/log.h>
#include "src/cpp/server/rpc_service_method.h"
#include <grpc++/async_server_context.h>

namespace grpc {

ServerRpcHandler::ServerRpcHandler(AsyncServerContext* server_context,
                                   RpcServiceMethod* method)
    : server_context_(server_context),
      method_(method) {
}

void ServerRpcHandler::StartRpc() {
  // Start the rpc on this dedicated completion queue.
  server_context_->Accept(cq_.cq());

  if (method_ == nullptr) {
    // Method not supported, finish the rpc with error.
    // TODO(rocking): do we need to call read to consume the request?
    FinishRpc(Status(StatusCode::UNIMPLEMENTED, "No such method."));
    return;
  }

  // Allocate request and response.
  std::unique_ptr<google::protobuf::Message> request(method_->AllocateRequestProto());
  std::unique_ptr<google::protobuf::Message> response(method_->AllocateResponseProto());

  // Read request
  server_context_->StartRead(request.get());
  auto type = WaitForNextEvent();
  GPR_ASSERT(type == CompletionQueue::SERVER_READ_OK);

  // Run the application's rpc handler
  MethodHandler* handler = method_->handler();
  Status status = handler->RunHandler(
      MethodHandler::HandlerParameter(request.get(), response.get()));

  if (status.IsOk()) {
    // Send the response if we get an ok status.
    server_context_->StartWrite(*response, 0);
    type = WaitForNextEvent();
    if (type != CompletionQueue::SERVER_WRITE_OK) {
      status = Status(StatusCode::INTERNAL, "Error writing response.");
    }
  }

  FinishRpc(status);
}

CompletionQueue::CompletionType ServerRpcHandler::WaitForNextEvent() {
  void* tag = nullptr;
  CompletionQueue::CompletionType type = cq_.Next(&tag);
  if (type != CompletionQueue::QUEUE_CLOSED &&
      type != CompletionQueue::RPC_END) {
    GPR_ASSERT(static_cast<AsyncServerContext*>(tag) == server_context_.get());
  }
  return type;
}

void ServerRpcHandler::FinishRpc(const Status& status) {
  server_context_->StartWriteStatus(status);
  CompletionQueue::CompletionType type = WaitForNextEvent();
  // TODO(rocking): do we care about this return type?

  type = WaitForNextEvent();
  GPR_ASSERT(type == CompletionQueue::RPC_END);

  cq_.Shutdown();
  type = WaitForNextEvent();
  GPR_ASSERT(type == CompletionQueue::QUEUE_CLOSED);
}

}  // namespace grpc