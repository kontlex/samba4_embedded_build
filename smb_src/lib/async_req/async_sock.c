/*
   Unix SMB/CIFS implementation.
   async socket syscalls
   Copyright (C) Volker Lendecke 2008

     ** NOTE! The following LGPL license applies to the async_sock
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "replace.h"
#include "system/network.h"
#include "system/filesys.h"
#include <talloc.h>
#include <tevent.h>
#include "lib/async_req/async_sock.h"

/* Note: lib/util/ is currently GPL */
#include "lib/util/tevent_unix.h"
#include "lib/util/samba_util.h"

struct async_connect_state {
	int fd;
	struct tevent_fd *fde;
	int result;
	long old_sockflags;
	socklen_t address_len;
	struct sockaddr_storage address;

	void (*before_connect)(void *private_data);
	void (*after_connect)(void *private_data);
	void *private_data;
};

static void async_connect_cleanup(struct tevent_req *req,
				  enum tevent_req_state req_state);
static void async_connect_connected(struct tevent_context *ev,
				    struct tevent_fd *fde, uint16_t flags,
				    void *priv);

/**
 * @brief async version of connect(2)
 * @param[in] mem_ctx	The memory context to hang the result off
 * @param[in] ev	The event context to work from
 * @param[in] fd	The socket to recv from
 * @param[in] address	Where to connect?
 * @param[in] address_len Length of *address
 * @retval The async request
 *
 * This function sets the socket into non-blocking state to be able to call
 * connect in an async state. This will be reset when the request is finished.
 */

struct tevent_req *async_connect_send(
	TALLOC_CTX *mem_ctx, struct tevent_context *ev, int fd,
	const struct sockaddr *address, socklen_t address_len,
	void (*before_connect)(void *private_data),
	void (*after_connect)(void *private_data),
	void *private_data)
{
	struct tevent_req *req;
	struct async_connect_state *state;

	req = tevent_req_create(mem_ctx, &state, struct async_connect_state);
	if (req == NULL) {
		return NULL;
	}

	/**
	 * We have to set the socket to nonblocking for async connect(2). Keep
	 * the old sockflags around.
	 */

	state->fd = fd;
	state->before_connect = before_connect;
	state->after_connect = after_connect;
	state->private_data = private_data;

	state->old_sockflags = fcntl(fd, F_GETFL, 0);
	if (state->old_sockflags == -1) {
		tevent_req_error(req, errno);
		return tevent_req_post(req, ev);
	}

	tevent_req_set_cleanup_fn(req, async_connect_cleanup);

	state->address_len = address_len;
	if (address_len > sizeof(state->address)) {
		tevent_req_error(req, EINVAL);
		return tevent_req_post(req, ev);
	}
	memcpy(&state->address, address, address_len);

	set_blocking(fd, false);

	if (state->before_connect != NULL) {
		state->before_connect(state->private_data);
	}

	state->result = connect(fd, address, address_len);

	if (state->after_connect != NULL) {
		state->after_connect(state->private_data);
	}

	if (state->result == 0) {
		tevent_req_done(req);
		return tevent_req_post(req, ev);
	}

	/**
	 * A number of error messages show that something good is progressing
	 * and that we have to wait for readability.
	 *
	 * If none of them are present, bail out.
	 */

	if (!(errno == EINPROGRESS || errno == EALREADY ||
#ifdef EISCONN
	      errno == EISCONN ||
#endif
	      errno == EAGAIN || errno == EINTR)) {
		tevent_req_error(req, errno);
		return tevent_req_post(req, ev);
	}

	state->fde = tevent_add_fd(ev, state, fd,
				   TEVENT_FD_READ | TEVENT_FD_WRITE,
				   async_connect_connected, req);
	if (state->fde == NULL) {
		tevent_req_error(req, ENOMEM);
		return tevent_req_post(req, ev);
	}
	return req;
}

static void async_connect_cleanup(struct tevent_req *req,
				  enum tevent_req_state req_state)
{
	struct async_connect_state *state =
		tevent_req_data(req, struct async_connect_state);

	TALLOC_FREE(state->fde);
	if (state->fd != -1) {
		fcntl(state->fd, F_SETFL, state->old_sockflags);
		state->fd = -1;
	}
}

/**
 * fde event handler for connect(2)
 * @param[in] ev	The event context that sent us here
 * @param[in] fde	The file descriptor event associated with the connect
 * @param[in] flags	Indicate read/writeability of the socket
 * @param[in] priv	private data, "struct async_req *" in this case
 */

static void async_connect_connected(struct tevent_context *ev,
				    struct tevent_fd *fde, uint16_t flags,
				    void *priv)
{
	struct tevent_req *req = talloc_get_type_abort(
		priv, struct tevent_req);
	struct async_connect_state *state =
		tevent_req_data(req, struct async_connect_state);
	int ret;

	if (state->before_connect != NULL) {
		state->before_connect(state->private_data);
	}

	ret = connect(state->fd, (struct sockaddr *)(void *)&state->address,
		      state->address_len);

	if (state->after_connect != NULL) {
		state->after_connect(state->private_data);
	}

	if (ret == 0) {
		tevent_req_done(req);
		return;
	}
	if (errno == EINPROGRESS) {
		/* Try again later, leave the fde around */
		return;
	}
	tevent_req_error(req, errno);
	return;
}

int async_connect_recv(struct tevent_req *req, int *perrno)
{
	int err = tevent_req_simple_recv_unix(req);

	if (err != 0) {
		*perrno = err;
		return -1;
	}

	return 0;
}

struct writev_state {
	struct tevent_context *ev;
	int fd;
	struct tevent_fd *fde;
	struct iovec *iov;
	int count;
	size_t total_size;
	uint16_t flags;
	bool err_on_readability;
};

static void writev_cleanup(struct tevent_req *req,
			   enum tevent_req_state req_state);
static void writev_trigger(struct tevent_req *req, void *private_data);
static void writev_handler(struct tevent_context *ev, struct tevent_fd *fde,
			   uint16_t flags, void *private_data);

struct tevent_req *writev_send(TALLOC_CTX *mem_ctx, struct tevent_context *ev,
			       struct tevent_queue *queue, int fd,
			       bool err_on_readability,
			       struct iovec *iov, int count)
{
	struct tevent_req *req;
	struct writev_state *state;

	req = tevent_req_create(mem_ctx, &state, struct writev_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->fd = fd;
	state->total_size = 0;
	state->count = count;
	state->iov = (struct iovec *)talloc_memdup(
		state, iov, sizeof(struct iovec) * count);
	if (tevent_req_nomem(state->iov, req)) {
		return tevent_req_post(req, ev);
	}
	state->flags = TEVENT_FD_WRITE|TEVENT_FD_READ;
	state->err_on_readability = err_on_readability;

	tevent_req_set_cleanup_fn(req, writev_cleanup);

	if (queue == NULL) {
		state->fde = tevent_add_fd(state->ev, state, state->fd,
				    state->flags, writev_handler, req);
		if (tevent_req_nomem(state->fde, req)) {
			return tevent_req_post(req, ev);
		}
		return req;
	}

	if (!tevent_queue_add(queue, ev, req, writev_trigger, NULL)) {
		tevent_req_nomem(NULL, req);
		return tevent_req_post(req, ev);
	}
	return req;
}

static void writev_cleanup(struct tevent_req *req,
			   enum tevent_req_state req_state)
{
	struct writev_state *state = tevent_req_data(req, struct writev_state);

	TALLOC_FREE(state->fde);
}

static void writev_trigger(struct tevent_req *req, void *private_data)
{
	struct writev_state *state = tevent_req_data(req, struct writev_state);

	state->fde = tevent_add_fd(state->ev, state, state->fd, state->flags,
			    writev_handler, req);
	if (tevent_req_nomem(state->fde, req)) {
		return;
	}
}

static void writev_handler(struct tevent_context *ev, struct tevent_fd *fde,
			   uint16_t flags, void *private_data)
{
	struct tevent_req *req = talloc_get_type_abort(
		private_data, struct tevent_req);
	struct writev_state *state =
		tevent_req_data(req, struct writev_state);
	size_t to_write, written;
	int i;

	to_write = 0;

	if ((state->flags & TEVENT_FD_READ) && (flags & TEVENT_FD_READ)) {
		int ret, value;

		if (state->err_on_readability) {
			/* Readable and the caller wants an error on read. */
			tevent_req_error(req, EPIPE);
			return;
		}

		/* Might be an error. Check if there are bytes to read */
		ret = ioctl(state->fd, FIONREAD, &value);
		/* FIXME - should we also check
		   for ret == 0 and value == 0 here ? */
		if (ret == -1) {
			/* There's an error. */
			tevent_req_error(req, EPIPE);
			return;
		}
		/* A request for TEVENT_FD_READ will succeed from now and
		   forevermore until the bytes are read so if there was
		   an error we'll wait until we do read, then get it in
		   the read callback function. Until then, remove TEVENT_FD_READ
		   from the flags we're waiting for. */
		state->flags &= ~TEVENT_FD_READ;
		TEVENT_FD_NOT_READABLE(fde);

		/* If not writable, we're done. */
		if (!(flags & TEVENT_FD_WRITE)) {
			return;
		}
	}

	for (i=0; i<state->count; i++) {
		to_write += state->iov[i].iov_len;
	}

	written = writev(state->fd, state->iov, state->count);
	if ((written == -1) && (errno == EINTR)) {
		/* retry */
		return;
	}
	if (written == -1) {
		tevent_req_error(req, errno);
		return;
	}
	if (written == 0) {
		tevent_req_error(req, EPIPE);
		return;
	}
	state->total_size += written;

	if (written == to_write) {
		tevent_req_done(req);
		return;
	}

	/*
	 * We've written less than we were asked to, drop stuff from
	 * state->iov.
	 */

	while (written > 0) {
		if (written < state->iov[0].iov_len) {
			state->iov[0].iov_base =
				(char *)state->iov[0].iov_base + written;
			state->iov[0].iov_len -= written;
			break;
		}
		written -= state->iov[0].iov_len;
		state->iov += 1;
		state->count -= 1;
	}
}

ssize_t writev_recv(struct tevent_req *req, int *perrno)
{
	struct writev_state *state =
		tevent_req_data(req, struct writev_state);
	ssize_t ret;

	if (tevent_req_is_unix_error(req, perrno)) {
		tevent_req_received(req);
		return -1;
	}
	ret = state->total_size;
	tevent_req_received(req);
	return ret;
}

struct read_packet_state {
	int fd;
	struct tevent_fd *fde;
	uint8_t *buf;
	size_t nread;
	ssize_t (*more)(uint8_t *buf, size_t buflen, void *private_data);
	void *private_data;
};

static void read_packet_cleanup(struct tevent_req *req,
				 enum tevent_req_state req_state);
static void read_packet_handler(struct tevent_context *ev,
				struct tevent_fd *fde,
				uint16_t flags, void *private_data);

struct tevent_req *read_packet_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    int fd, size_t initial,
				    ssize_t (*more)(uint8_t *buf,
						    size_t buflen,
						    void *private_data),
				    void *private_data)
{
	struct tevent_req *req;
	struct read_packet_state *state;

	req = tevent_req_create(mem_ctx, &state, struct read_packet_state);
	if (req == NULL) {
		return NULL;
	}
	state->fd = fd;
	state->nread = 0;
	state->more = more;
	state->private_data = private_data;

	tevent_req_set_cleanup_fn(req, read_packet_cleanup);

	state->buf = talloc_array(state, uint8_t, initial);
	if (tevent_req_nomem(state->buf, req)) {
		return tevent_req_post(req, ev);
	}

	state->fde = tevent_add_fd(ev, state, fd,
				   TEVENT_FD_READ, read_packet_handler,
				   req);
	if (tevent_req_nomem(state->fde, req)) {
		return tevent_req_post(req, ev);
	}
	return req;
}

static void read_packet_cleanup(struct tevent_req *req,
			   enum tevent_req_state req_state)
{
	struct read_packet_state *state =
		tevent_req_data(req, struct read_packet_state);

	TALLOC_FREE(state->fde);
}

static void read_packet_handler(struct tevent_context *ev,
				struct tevent_fd *fde,
				uint16_t flags, void *private_data)
{
	struct tevent_req *req = talloc_get_type_abort(
		private_data, struct tevent_req);
	struct read_packet_state *state =
		tevent_req_data(req, struct read_packet_state);
	size_t total = talloc_get_size(state->buf);
	ssize_t nread, more;
	uint8_t *tmp;

	nread = recv(state->fd, state->buf+state->nread, total-state->nread,
		     0);
	if ((nread == -1) && (errno == ENOTSOCK)) {
		nread = read(state->fd, state->buf+state->nread,
			     total-state->nread);
	}
	if ((nread == -1) && (errno == EINTR)) {
		/* retry */
		return;
	}
	if (nread == -1) {
		tevent_req_error(req, errno);
		return;
	}
	if (nread == 0) {
		tevent_req_error(req, EPIPE);
		return;
	}

	state->nread += nread;
	if (state->nread < total) {
		/* Come back later */
		return;
	}

	/*
	 * We got what was initially requested. See if "more" asks for -- more.
	 */
	if (state->more == NULL) {
		/* Nobody to ask, this is a async read_data */
		tevent_req_done(req);
		return;
	}

	more = state->more(state->buf, total, state->private_data);
	if (more == -1) {
		/* We got an invalid packet, tell the caller */
		tevent_req_error(req, EIO);
		return;
	}
	if (more == 0) {
		/* We're done, full packet received */
		tevent_req_done(req);
		return;
	}

	if (total + more < total) {
		tevent_req_error(req, EMSGSIZE);
		return;
	}

	tmp = talloc_realloc(state, state->buf, uint8_t, total+more);
	if (tevent_req_nomem(tmp, req)) {
		return;
	}
	state->buf = tmp;
}

ssize_t read_packet_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			 uint8_t **pbuf, int *perrno)
{
	struct read_packet_state *state =
		tevent_req_data(req, struct read_packet_state);

	if (tevent_req_is_unix_error(req, perrno)) {
		tevent_req_received(req);
		return -1;
	}
	*pbuf = talloc_move(mem_ctx, &state->buf);
	tevent_req_received(req);
	return talloc_get_size(*pbuf);
}

struct wait_for_read_state {
	struct tevent_fd *fde;
};

static void wait_for_read_cleanup(struct tevent_req *req,
				  enum tevent_req_state req_state);
static void wait_for_read_done(struct tevent_context *ev,
			       struct tevent_fd *fde,
			       uint16_t flags,
			       void *private_data);

struct tevent_req *wait_for_read_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      int fd)
{
	struct tevent_req *req;
	struct wait_for_read_state *state;

	req = tevent_req_create(mem_ctx, &state, struct wait_for_read_state);
	if (req == NULL) {
		return NULL;
	}

	tevent_req_set_cleanup_fn(req, wait_for_read_cleanup);

	state->fde = tevent_add_fd(ev, state, fd, TEVENT_FD_READ,
				   wait_for_read_done, req);
	if (tevent_req_nomem(state->fde, req)) {
		return tevent_req_post(req, ev);
	}
	return req;
}

static void wait_for_read_cleanup(struct tevent_req *req,
				  enum tevent_req_state req_state)
{
	struct wait_for_read_state *state =
		tevent_req_data(req, struct wait_for_read_state);

	TALLOC_FREE(state->fde);
}

static void wait_for_read_done(struct tevent_context *ev,
			       struct tevent_fd *fde,
			       uint16_t flags,
			       void *private_data)
{
	struct tevent_req *req = talloc_get_type_abort(
		private_data, struct tevent_req);

	if (flags & TEVENT_FD_READ) {
		tevent_req_done(req);
	}
}

bool wait_for_read_recv(struct tevent_req *req, int *perr)
{
	int err = tevent_req_simple_recv_unix(req);

	if (err != 0) {
		*perr = err;
		return false;
	}

	return true;
}
