/*
 * Copyright (c) 2021 Thanos Makatos. All rights reserved.
 *
 * Author: Thanos Makatos <thanos@nutanix.com>
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of Nutanix nor the names of its contributors may be
 *        used to endorse or promote products derived from this software without
 *        specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *  DAMAGE.
 *
 */

#include <errno.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>


#include "co_ctx.h"

struct co_ctx {
	bool pending_callback;
	bool async_done;
	ucontext_t parent_ctx;
	ucontext_t child_ctx;
	int err;
	char child_ctx_stack[0x2000];
};

int co_ctx_size(void) {
	return sizeof(struct co_ctx);
}

void co_ctx_done(struct co_ctx *co_ctx, int err) {
	co_ctx->async_done = true;
	co_ctx->err = err;
}

static void co_ctx_yield(struct co_ctx *co_ctx) {
	co_ctx->async_done = false;
	swapcontext(&co_ctx->child_ctx, &co_ctx->parent_ctx);
}

int co_ctx_call_child(struct co_ctx *co_ctx, int (*fn)(void *), void *args) {
	int ret = fn(args);
	if (ret == -EBUSY) {
		co_ctx_yield(co_ctx);
		/* We'll return here when the parent calls swapcontext(&co_ctx->parent_ctx, &co_ctx->child_ctx),
		 * and it will only do that after the user has set async_done to true.
		 */
		ret = co_ctx->err;
	}
	return ret;
}

static void _co_ctx_call_parent(struct co_ctx *co_ctx, int (*fn)(void *), void *args) {
	co_ctx->pending_callback = true;
	co_ctx->err = fn(args);
	co_ctx->pending_callback = false;
}

int co_ctx_call_parent(struct co_ctx *co_ctx, int (*fn)(void *), void *args) {
	int ret = getcontext(&co_ctx->child_ctx);
	if (ret < -1) {
		return ret;
	}
	memset(&co_ctx->parent_ctx, 0, sizeof co_ctx->parent_ctx);
	co_ctx->child_ctx.uc_link = &co_ctx->parent_ctx;
	co_ctx->child_ctx.uc_stack.ss_sp = co_ctx->child_ctx_stack;
	co_ctx->child_ctx.uc_stack.ss_size = sizeof co_ctx->child_ctx_stack;
	makecontext(&co_ctx->child_ctx, (void (*) (void))_co_ctx_call_parent, 3, co_ctx, fn, args);
	swapcontext(&co_ctx->parent_ctx, &co_ctx->child_ctx);
	if (!co_ctx->pending_callback) {
		return co_ctx->err;
	}
	return -EBUSY;
}

int co_ctx_continue(struct co_ctx *co_ctx) {
	if (!co_ctx->pending_callback) {
		return 0;
	}
	if (!co_ctx->async_done) { /* previous async callback still running */
		printf("%s: previously async callback still running\n", __func__);
		return -EBUSY;
	}
	printf("%s: previously async callback finished\n", __func__);
	swapcontext(&co_ctx->parent_ctx, &co_ctx->child_ctx);
	if (co_ctx->pending_callback) { /* user started another async callback */
		printf("%s: another async callback started\n", __func__);
		return -EBUSY;
	}
	return 0;
}
