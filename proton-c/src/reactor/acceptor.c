/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#include <proton/io.h>
#include <proton/sasl.h>
#include <proton/selector.h>
#include <proton/transport.h>
#include "reactor.h"
#include "selectable.h"

pn_selectable_t *pn_reactor_selectable_transport(pn_reactor_t *reactor, pn_socket_t sock, pn_transport_t *transport);

PN_HANDLE(PNI_ACCEPTOR_HANDLER)

void pni_acceptor_readable(pn_selectable_t *sel) {
  pn_reactor_t *reactor = (pn_reactor_t *) pni_selectable_get_context(sel);
  char name[1024];
  pn_socket_t sock = pn_accept(pn_reactor_io(reactor), pn_selectable_get_fd(sel), name, 1024);
  pn_handler_t *handler = (pn_handler_t *) pn_record_get(pn_selectable_attachments(sel), PNI_ACCEPTOR_HANDLER);
  if (!handler) { handler = pn_reactor_get_handler(reactor); }
  pn_connection_t *conn = pn_reactor_connection(reactor, handler);
  pn_transport_t *trans = pn_transport();
  pn_transport_set_server(trans);
  pn_sasl_t *sasl = pn_sasl(trans);
  pn_sasl_allow_skip(sasl, true);
  pn_sasl_mechanisms(sasl, "ANONYMOUS");
  pn_sasl_done(sasl, PN_SASL_OK);
  pn_transport_bind(trans, conn);
  pn_reactor_selectable_transport(reactor, sock, trans);
  pn_decref(trans);
}

void pni_acceptor_finalize(pn_selectable_t *sel) {
  pn_reactor_t *reactor = (pn_reactor_t *) pni_selectable_get_context(sel);
  if (pn_selectable_get_fd(sel) != PN_INVALID_SOCKET) {
    pn_close(pn_reactor_io(reactor), pn_selectable_get_fd(sel));
  }
}

pn_acceptor_t *pn_reactor_acceptor(pn_reactor_t *reactor, const char *host, const char *port, pn_handler_t *handler) {
  pn_socket_t socket = pn_listen(pn_reactor_io(reactor), host, port);
  if (socket == PN_INVALID_SOCKET) {
    return NULL;
  }
  pn_selectable_t *sel = pn_reactor_selectable(reactor);
  pn_selectable_set_fd(sel, socket);
  pn_selectable_on_readable(sel, pni_acceptor_readable);
  pn_selectable_on_finalize(sel, pni_acceptor_finalize);
  pni_record_init_reactor(pn_selectable_attachments(sel), reactor);
  pn_record_t *record = pn_selectable_attachments(sel);
  pn_record_def(record, PNI_ACCEPTOR_HANDLER, PN_OBJECT);
  pn_record_set(record, PNI_ACCEPTOR_HANDLER, handler);
  pn_selectable_set_reading(sel, true);
  pn_reactor_update(reactor, sel);
  return (pn_acceptor_t *) sel;
}

void pn_acceptor_close(pn_acceptor_t *acceptor) {
  pn_selectable_t *sel = (pn_selectable_t *) acceptor;
  if (!pn_selectable_is_terminal(sel)) {
    pn_reactor_t *reactor = (pn_reactor_t *) pni_selectable_get_context(sel);
    pn_socket_t socket = pn_selectable_get_fd(sel);
    pn_close(pn_reactor_io(reactor), socket);
    pn_selectable_set_fd(sel, PN_INVALID_SOCKET);
    pn_selectable_terminate(sel);
    pn_reactor_update(reactor, sel);
  }
}
