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

#ifndef __CO_CTX_H__
#define __CO_CTX_H__

#include <ucontext.h>

struct co_ctx;

/*
 * Returns size of struct co_ctx.
 */
int co_ctx_size(void);

/*
 * Calls a parent function (parent_fn). The parent function can then call child
 * functions via co_ctx_child_call. If a child functions returns -EBUSY then
 * co_ctx_parent_call also returns -EBUSY and the caller must execute
 * co_ctx_continue until it stops returning -EBUSY.
 */
int co_ctx_parent_call(struct co_ctx *co_ctx, int (*parent_fn)(void *), void *args);

/*
 * Calls a child function (child_fn). If the child function returns -EBUSY then
 * execution of the parent function is stopped until the child function calls
 * co_ctx_done. co_ctx_child_call must be called by a function started via
 * co_ctx_parent_call, othewise behavior is undefined.
 */
int co_ctx_child_call(struct co_ctx *co_ctx, int (*child_fn)(void *), void *args);

/*
 * Continues execution of a parent function.
 * Returns 0 if the parent function finished or if there is no parent function
 * running (therefore it's fine to unconditionally call this function).
 * Returns -EBUSY if a child function is not yet complete
 * (co_ctx_done has not been called).
 */
int co_ctx_continue(struct co_ctx *co_ctx);

/*
 * Finishes execution of a child function that previously returned -EBUSY.
 */
void co_ctx_done(struct co_ctx *co_ctx, int err);

#endif /* __CO_CTX_H__ */
