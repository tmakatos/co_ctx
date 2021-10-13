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

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>

#include "co_ctx.h"

struct vfu_ctx {
	/* TODO more private stuff */
	struct co_ctx *co_ctx;
};

/* Device callbacks */

static int dma_unmap(void *args) {
	/*
  	 * Pretend that there's pending I/O and we need to wait for it to finish
  	 * before unmapping the DMA region.
 	 */
	return -EBUSY;
}

static int reset(void *args) {
	/*
	 * Pretend that the device needs to do a lot of stuff and can't immediately
	 * complete the reset.
	 */
	return -EBUSY;
}

static int migration_state_transition(void *args) {
	/*
	 * Device immediately transitioned to specified migration state.
	 */
	return 0;
}

static int vfu_handle_stuff(struct vfu_ctx *vfu_ctx) {

	int ret;

	printf("%s: DMA unmap a region\n", __func__);
	ret = co_ctx_call_child(vfu_ctx->co_ctx, dma_unmap, NULL);
	if (ret < 0) {
		printf("%s:%d child failed: %s\n", __func__, __LINE__, strerror(-ret));
		return ret;
	}

	printf("%s: DMA unmap another region\n", __func__);
	ret = co_ctx_call_child(vfu_ctx->co_ctx, dma_unmap, NULL);
	if (ret < 0) {
		printf("%s:%d child failed: %s\n", __func__, __LINE__, strerror(-ret));
		return ret;
	}

	printf("%s: reset\n", __func__);
	ret = co_ctx_call_child(vfu_ctx->co_ctx, reset, NULL);
	if (ret < 0) {
		printf("%s:%d child failed: %s\n", __func__, __LINE__, strerror(-ret));
		return ret;
	}
	
	/* migration state */
	printf("%s: migrate\n", __func__);
	ret = migration_state_transition(NULL);
	assert(ret == 0);

	return 0;
}

static int vfu_handle_migration_state_transition(struct vfu_ctx *vfu_ctx) {
	printf("%s: migration\n", __func__);
	int ret = co_ctx_call_child(vfu_ctx->co_ctx, migration_state_transition, NULL);
	if (ret < 0) {
		printf("%s:%d child failed: %s\n", __func__, __LINE__, strerror(-ret));
		return ret;
	}
	return 0;
}

/* Return next request to be processed */
static int get_next_request(void)
{
	static int i;
	return i++;
}

static int vfu_run_ctx(struct vfu_ctx *vfu_ctx) {

	int ret = 0;
	static bool must_continue;

	if (must_continue) { /* It's safe to call co_ctx_continue unconditionally. */
		ret = co_ctx_continue(vfu_ctx->co_ctx);
		if (ret < 0) {
			return ret;		
		}
		must_continue = false;
	}

	ret = get_next_request();

	printf("%s: work %d\n", __func__, ret);

	switch (ret) {
		case 0:
			ret = co_ctx_call_parent(vfu_ctx->co_ctx, (int (*) (void *))vfu_handle_stuff, vfu_ctx);
			break;
		case 2:
			ret = co_ctx_call_parent(vfu_ctx->co_ctx, (int (*) (void *))vfu_handle_migration_state_transition, vfu_ctx);
			break;
		default: /* handle other request types */
			ret = 0;
			printf("did some other work\n");
	}
	if (ret == -EBUSY) {
		must_continue = true;
	}
	return ret;
}

int main(void) {

	int ret;

	struct vfu_ctx vfu_ctx = { };

	vfu_ctx.co_ctx = calloc(1, co_ctx_size());
	assert(vfu_ctx.co_ctx != NULL);

	/* user callback for DMA unmap is async */
	ret = vfu_run_ctx(&vfu_ctx);
	assert(ret == -EBUSY);

	/* user callback for DMA map is not done yet */
	ret = vfu_run_ctx(&vfu_ctx);
	assert(ret == -EBUSY);

	/* user callback for DMA unmap is done */
	co_ctx_done(vfu_ctx.co_ctx, 0);

	ret = vfu_run_ctx(&vfu_ctx);
	assert(ret == -EBUSY);

	/* user callback for DMA unmap is not done yet */
	ret = vfu_run_ctx(&vfu_ctx);
	assert(ret == -EBUSY);

	/* still not done */
	ret = vfu_run_ctx(&vfu_ctx);
	assert(ret == -EBUSY);

	/* user callback for DMA unmap is done */
	co_ctx_done(vfu_ctx.co_ctx, 0);

	ret = vfu_run_ctx(&vfu_ctx);
	assert(ret == -EBUSY);
	co_ctx_done(vfu_ctx.co_ctx, 0);

	/* some completely unrelated work is executed */
	ret = vfu_run_ctx(&vfu_ctx);
	assert(ret == 0);

	/* migration state transition is executed */
	ret = vfu_run_ctx(&vfu_ctx);
	assert(ret == 0);

	return 0;
}
