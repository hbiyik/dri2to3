/*
 * Copyright (C) 2022 Icecream95 <ixn@disroot.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _GNU_SOURCE

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <drm.h>
#include <drm_mode.h>
#include <xcb/present.h>
#include <xcb/dri2.h>
#include <xcb/dri3.h>

#include "lib2to3.h"

#define DEVICE_NAME "/dev/dri/card0"

#define DLSYM(name) static typeof(name) *orig_##name; \
        if (!orig_##name) orig_##name = dlsym(RTLD_NEXT, #name)

#define RETURN(reply) do { \
                void *r = malloc(sizeof(reply)); \
                memcpy(r, &reply, sizeof(reply)); \
                if (e) *e = NULL; \
                return r; \
        } while (0)

#define RETURN_NULL() do { \
                if (e) *e = NULL; \
                return NULL; \
        } while (0)

static int drm_fd = -1;

xcb_dri2_connect_cookie_t
xcb_dri2_connect(xcb_connection_t *conn, xcb_window_t window, uint32_t driver_type)
{
        LOG("MY xcb_dri2_connect\n");

        lib2to3_init();

        xcb_generic_error_t *error;
        xcb_dri3_query_version_cookie_t ver_cookie =
                xcb_dri3_query_version(conn, 1, 2);

        xcb_dri3_query_version_reply_t *ver =
                xcb_dri3_query_version_reply(conn, ver_cookie, &error);

        LOG("DRI3 version: %i.%i\n", ver->major_version, ver->minor_version);

        free(ver);

        return (xcb_dri2_connect_cookie_t) { .sequence = 0 };
}

xcb_dri2_connect_reply_t *
xcb_dri2_connect_reply(xcb_connection_t *conn, xcb_dri2_connect_cookie_t cookie,
                       xcb_generic_error_t **e)
{
        LOG("MY xcb_dri2_connect_reply\n");

        struct {
                xcb_dri2_connect_reply_t reply;
                char device_name[sizeof(DEVICE_NAME)];
        } reply = {
                .reply = {
                        .response_type = XCB_DRI2_CONNECT,
                        .device_name_length = sizeof(DEVICE_NAME),
                },
                .device_name = DEVICE_NAME,
        };

        RETURN(reply);
}

xcb_dri2_authenticate_cookie_t
xcb_dri2_authenticate(xcb_connection_t *conn, xcb_window_t window, uint32_t magic)
{
        LOG("MY xcb_dri2_authenticate\n");
        return (xcb_dri2_authenticate_cookie_t) { .sequence = 0 };
}

xcb_dri2_authenticate_reply_t *
xcb_dri2_authenticate_reply(xcb_connection_t *conn, xcb_dri2_authenticate_cookie_t cookie,
                            xcb_generic_error_t **e)
{
        LOG("MY xcb_dri2_authenticate_reply\n");

        xcb_dri2_authenticate_reply_t reply = {
                .response_type = XCB_DRI2_AUTHENTICATE,
                .authenticated = 1,
        };

        RETURN(reply);
}

xcb_dri2_query_version_cookie_t
xcb_dri2_query_version(xcb_connection_t *conn, uint32_t major_version,
                       uint32_t minor_version)
{
        LOG("MY xcb_dri2_query_version %i.%i\n", major_version, minor_version);
        return (xcb_dri2_query_version_cookie_t) { .sequence = 0 };
}

xcb_dri2_query_version_reply_t *
xcb_dri2_query_version_reply(xcb_connection_t *conn,
                             xcb_dri2_query_version_cookie_t cookie,
                             xcb_generic_error_t **e)
{
        LOG("MY xcb_dri2_query_version_reply\n");

        xcb_dri2_query_version_reply_t reply = {
                .response_type = XCB_DRI2_QUERY_VERSION,
                .major_version = 1,
                .minor_version = 4,
        };

        RETURN(reply);
}

xcb_void_cookie_t
xcb_dri2_swap_interval(xcb_connection_t *conn, xcb_drawable_t drawable, uint32_t interval)
{
        LOG("MY xcb_dri2_swap_interval %i\n", interval);
        return (xcb_void_cookie_t) { .sequence = 0 };
}

xcb_void_cookie_t
xcb_dri2_create_drawable_checked(xcb_connection_t *conn, xcb_drawable_t drawable)
{
        LOG("MY xcb_dri2_create_drawable %i\n", drawable);

        xcb_generic_error_t *error;
        xcb_dri3_open_cookie_t open_cookie =
                xcb_dri3_open(conn, drawable, 0);

        xcb_dri3_open_reply_t *open =
                xcb_dri3_open_reply(conn, open_cookie, &error);

        LOG("DRI3 nfd %i\n", open->nfd);

        free(open);

        lib2to3_create_drawable(conn, drawable, drm_fd);

        return (xcb_void_cookie_t) { .sequence = 0 };
}

xcb_dri2_get_buffers_cookie_t
xcb_dri2_get_buffers(xcb_connection_t *conn, xcb_drawable_t drawable,
                     uint32_t count, uint32_t attachments_len,
                     const uint32_t *attachments)
{
        LOG("MY xcb_dri2_get_buffers %i: %i/%i: %x\n", drawable,
               count, attachments_len, attachments[0]);
        assert(count == 1);
        assert(count == attachments_len);
        assert(attachments[0] == XCB_DRI2_ATTACHMENT_BUFFER_BACK_LEFT);
        return (xcb_dri2_get_buffers_cookie_t) { .sequence = drawable };
}

xcb_dri2_get_buffers_reply_t *
xcb_dri2_get_buffers_reply(xcb_connection_t *conn, xcb_dri2_get_buffers_cookie_t cookie,
                           xcb_generic_error_t **e)
{
        LOG("MY xcb_dri2_get_buffers_reply %x\n", cookie.sequence);

        xcb_drawable_t drawable = cookie.sequence;

        struct drawable *d = lib2to3_get_drawable(conn, drawable);

        struct buffer *b = lib2to3_get_buffer(d);

        if (!b)
                RETURN_NULL();

        lib2to3_set_handle_size(b->handle, b->size);
        
        struct {
                xcb_dri2_get_buffers_reply_t reply;
                xcb_dri2_dri2_buffer_t buffer;
        } reply = {
                .reply = {
                        .response_type = XCB_DRI2_GET_BUFFERS,
                        .width = b->width,
                        .height = b->height,
                        .count = 1,
                },
                .buffer = {
                        .attachment = 1,
                        .name = b->handle,
                        .pitch = b->pitch,
                        .cpp = b->cpp,
                        .flags = 0,
                },
        };

        RETURN(reply);
}

xcb_dri2_swap_buffers_cookie_t
xcb_dri2_swap_buffers(xcb_connection_t *conn, xcb_drawable_t drawable,
                      uint32_t target_msc_hi, uint32_t target_msc_lo,
                      uint32_t divisor_hi, uint32_t divisor_lo,
                      uint32_t remainder_hi, uint32_t remainder_lo)
{
        LOG("MY xcb_dri2_swap_buffers\n");
        return (xcb_dri2_swap_buffers_cookie_t) { .sequence = drawable };
}

xcb_dri2_swap_buffers_reply_t *
xcb_dri2_swap_buffers_reply(xcb_connection_t *conn,
                            xcb_dri2_swap_buffers_cookie_t cookie,
                            xcb_generic_error_t **e)
{
        LOG("MY xcb_dri2_swap_buffers_reply\n");

        xcb_drawable_t drawable = cookie.sequence;

        struct drawable *d = lib2to3_get_drawable(conn, drawable);

        xcb_present_pixmap(conn, drawable, d->cur->pixmap, ++d->present_serial,
                           0, 0, 0, 0, 0, 0, 0, XCB_PRESENT_OPTION_ASYNC,
                           0, 0, 0, 0, NULL);

        lib2to3_drawable_swap(d);

        xcb_dri2_swap_buffers_reply_t reply = {
                .response_type = XCB_DRI2_SWAP_BUFFERS,
        };

        RETURN(reply);
}

xcb_void_cookie_t
xcb_dri2_destroy_drawable_checked(xcb_connection_t *conn, xcb_drawable_t drawable)
{
        LOG("MY xcb_dri2_destroy_drawable %x\n", drawable);
        lib2to3_free_drawable(conn, drawable);
        return (xcb_void_cookie_t) { .sequence = 0 };
}

int
ioctl(int fd, unsigned long request, ...)
{
        DLSYM(ioctl);

        va_list args;
        va_start(args, request);
        void *ptr = va_arg(args, void *);
        va_end(args);

        if (request == DRM_IOCTL_GEM_OPEN) {
                struct drm_gem_open *open = ptr;

                LOG("MY DRM_IOCTL_GEM_OPEN %i\n", open->name);

                open->handle = open->name;
                open->size = lib2to3_get_handle_size(open->name);

                return 0;
        } else if (request == DRM_IOCTL_GEM_CLOSE) {
                struct drm_gem_close *close = ptr;

                LOG("MY DRM_IOCTL_GEM_CLOSE %i\n", close->handle);

                if (!lib2to3_close_handle(fd, close->handle))
                        return 0;
        } else if (request == DRM_IOCTL_GET_MAGIC) {
                LOG("MY DRM_IOCTL_GET_MAGIC fd %i\n", fd);
                drm_fd = fd;
        }

        return orig_ioctl(fd, request, ptr);
}
