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

#ifndef LIB2TO3_INCLUDE_GUARD
#define LIB2TO3_INCLUDE_GUARD

#define MAX_BUFFERS 4

//#define LOG(...) fprintf(stderr, __VA_ARGS__)
#define LOG(...) do { break; fprintf(stderr, __VA_ARGS__); } while (0)

#include "list.h"

static pthread_mutex_t l = PTHREAD_MUTEX_INITIALIZER;
#define LOCK()   pthread_mutex_lock(&l)
#define UNLOCK() pthread_mutex_unlock(&l)

static bool init_done = false;
static struct list_head handle_list;
static struct list_head close_list;
static struct list_head drawable_list;

struct handle_link {
        struct list_head link;
        uint32_t handle;
        uint64_t size;
};

struct gem_close {
        struct list_head link;
        int drm_fd;
        uint32_t handle;
};

struct buffer {
        struct list_head link;
        xcb_pixmap_t pixmap;
        bool busy;
        bool dead;

        uint32_t handle;
        uint32_t pitch;
        uint32_t cpp;
        uint32_t width, height;
        uint64_t size;
};

struct drawable {
        struct list_head link;

        xcb_connection_t *conn;
        xcb_drawable_t drawable;

        int drm_fd;

        xcb_present_event_t eid;
        xcb_special_event_t *special_event;

        unsigned present_serial;
        unsigned sbc;

        unsigned num_buffers;

        struct buffer *cur;
        struct list_head buffers;
};

static inline void
lib2to3_init(void)
{
        LOCK();
        if (!init_done) {
                list_inithead(&handle_list);
                list_inithead(&close_list);
                list_inithead(&drawable_list);
                init_done = true;
        }
        UNLOCK();
}

static inline void
lib2to3_set_handle_size(uint32_t handle, uint64_t size)
{
        struct handle_link *link = malloc(sizeof(*link));
        *link = (struct handle_link) {
                .handle = handle,
                .size = size,
        };

        LOCK();
        list_add(&link->link, &handle_list);
        UNLOCK();
}

static inline uint64_t
lib2to3_get_handle_size(uint32_t handle)
{
        uint64_t size = 0;

        LOCK();
        list_for_each_entry_safe(struct handle_link, h, &handle_list, link) {
                if (h->handle == handle) {
                        size = h->size;
                        list_del(&h->link);
                        free(h);
                        break;
                }
        }
        UNLOCK();

        assert(size);

        return size;
}

static inline bool
lib2to3_close_del_locked(int drm_fd, uint32_t handle)
{
        list_for_each_entry_safe(struct gem_close, c, &close_list, link) {
                if ((c->drm_fd == drm_fd) && (c->handle == handle)) {
                        list_del(&c->link);
                        free(c);
                        return true;
                }
        }

        return false;
}

static inline bool
lib2to3_close_handle(int drm_fd, uint32_t handle)
{
        LOCK();
        if (lib2to3_close_del_locked(drm_fd, handle)) {
                UNLOCK();
                return true;
        }

        struct gem_close *c = malloc(sizeof(*c));
        *c = (struct gem_close) {
                .drm_fd = drm_fd,
                .handle = handle,
        };
        list_addtail(&c->link, &close_list);
        UNLOCK();

        return false;
}

static inline void
lib2to3_create_drawable(xcb_connection_t *conn, xcb_drawable_t drawable, int drm_fd)
{
        struct drawable *d = malloc(sizeof(*d));
        *d = (struct drawable) {
                .conn = conn,
                .drawable = drawable,
                .drm_fd = drm_fd,
        };
        list_inithead(&d->buffers);

        d->eid = xcb_generate_id(d->conn);

        xcb_present_select_input(d->conn, d->eid, d->drawable,  
                                 XCB_PRESENT_EVENT_MASK_CONFIGURE_NOTIFY |
                                 XCB_PRESENT_EVENT_MASK_IDLE_NOTIFY);    
        d->special_event =
                xcb_register_for_special_xge(d->conn, &xcb_present_id,
                                             d->eid, NULL);

        LOCK();
        list_add(&d->link, &drawable_list);
        UNLOCK();
}

static inline struct drawable *
lib2to3_get_drawable(xcb_connection_t *conn, xcb_drawable_t drawable)
{
        LOCK();
        list_for_each_entry(struct drawable, d, &drawable_list, link) {
                if ((d->conn == conn) && (d->drawable == drawable)) {
                        UNLOCK();
                        return d;
                }
        }
        UNLOCK();

        fprintf(stderr, "Could not find drawable %x!\n", drawable);
        return NULL;
}

static inline struct buffer *
lib2to3_create_buffer(struct drawable *d)
{
        xcb_generic_error_t *error;
        xcb_get_geometry_cookie_t geom_cookie =
                xcb_get_geometry(d->conn, d->drawable);

        xcb_get_geometry_reply_t *geom =
                xcb_get_geometry_reply(d->conn, geom_cookie, &error);

        if (!geom)
                return NULL;

        struct drm_mode_create_dumb create = {
                .width = geom->width,
                .height = geom->height,
                .bpp = 32,
        };
        ioctl(d->drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);

        struct drm_prime_handle handle = {
                .handle = create.handle,
        };
        ioctl(d->drm_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &handle);

        xcb_pixmap_t pixmap = xcb_generate_id(d->conn);

        xcb_dri3_pixmap_from_buffers(d->conn, pixmap, d->drawable, 1,
                                     create.width, create.height,
                                     create.pitch, 0,
                                     0, 0, 0, 0, 0, 0,
                                     geom->depth, 32, 0, &handle.fd);

        free(geom);

        struct buffer *b = malloc(sizeof(*b));
        *b = (struct buffer) {
                .pixmap = pixmap,
                .handle = create.handle,
                .pitch = create.pitch,
                .cpp = 4,
                .width = create.width,
                .height = create.height,
                .size = create.size,
        };

        ++d->num_buffers;

        return b;
}

static inline void
lib2to3_free_buffer(struct drawable *d, struct buffer *b)
{
        struct drm_gem_close close = {
                .handle = b->handle,
        };
        ioctl(d->drm_fd, DRM_IOCTL_GEM_CLOSE, &close);

        xcb_free_pixmap(d->conn, b->pixmap);

        free(b);

        --d->num_buffers;
}

static inline void
lib2to3_handle_present_event(struct drawable *d,
                             xcb_present_generic_event_t *ge)
{
        switch (ge->evtype) {
        case XCB_PRESENT_CONFIGURE_NOTIFY: {
                LOG("MY CONFIGURE_NOTIFY\n");

                list_for_each_entry(struct buffer, b, &d->buffers, link) {
                        b->dead = true;
                }
                break;
        }
        case XCB_PRESENT_EVENT_IDLE_NOTIFY: {
                LOG("MY IDLE_NOTIFY\n");

                xcb_present_idle_notify_event_t *ie = (void *) ge;

                list_for_each_entry(struct buffer, b, &d->buffers, link) {
                        if (b->pixmap == ie->pixmap)
                                b->busy = false;
                }
                break;
        }
        default:
                break;
        }

        list_for_each_entry_safe(struct buffer, b, &d->buffers, link) {
                if (b->dead && !b->busy) {
                        list_del(&b->link);
                        lib2to3_free_buffer(d, b);
                }
        }
        
        free(ge);
}

static inline void
lib2to3_flush_events(struct drawable *d)
{
        xcb_generic_event_t *ev;

        while ((ev = xcb_poll_for_special_event(d->conn,
                                                d->special_event)) != NULL) {
                xcb_present_generic_event_t *ge = (void *) ev;
                lib2to3_handle_present_event(d, ge);
        }
}

static inline bool
lib2to3_wait_for_event(struct drawable *d)
{
        xcb_generic_event_t *ev;
        ev = xcb_wait_for_special_event(d->conn, d->special_event);

        if (!ev)
                return false;

        xcb_present_generic_event_t *ge = (void *) ev;

        lib2to3_handle_present_event(d, ge);
        return true;
}

static inline struct buffer *
lib2to3_set_buffer(struct drawable *d, struct buffer *b, bool reused)
{
        if (reused) {
                LOCK();
                lib2to3_close_del_locked(d->drm_fd, b->handle);
                UNLOCK();
        }

        d->cur = b;
        return b;
}

static inline void
lib2to3_drawable_swap(struct drawable *d)
{
        d->cur->busy = true;

        list_addtail(&d->cur->link, &d->buffers);
        d->cur = NULL;
}

static inline struct buffer *
lib2to3_get_buffer(struct drawable *d)
{
        if (d->cur)
                return lib2to3_set_buffer(d, d->cur, true);

        lib2to3_flush_events(d);

        for (;;) {
                list_for_each_entry_safe(struct buffer, b, &d->buffers, link) {
                        if (!b->busy) {
                                list_del(&b->link);
                                return lib2to3_set_buffer(d, b, true);
                        }
                }

                if (d->num_buffers < MAX_BUFFERS) {
                        struct buffer *b = lib2to3_create_buffer(d);
                        return lib2to3_set_buffer(d, b, false);
                }

                bool ret = lib2to3_wait_for_event(d);
                if (!ret)
                        return NULL;
        }
}

static inline void
lib2to3_free_drawable(xcb_connection_t *conn, xcb_drawable_t drawable)
{
        LOCK();
        list_for_each_entry_safe(struct drawable, d, &drawable_list, link) {
                if ((d->conn == conn) && (d->drawable == drawable)) {
                        list_del(&d->link);
                        UNLOCK();

                        list_for_each_entry_safe(struct buffer, b,
                                                 &d->buffers, link) { 
                                list_del(&b->link);
                                lib2to3_free_buffer(d, b);
                        }

                        if (d->cur)
                                lib2to3_free_buffer(d, d->cur);

                        free(d);

                        return;
                }
        }
        UNLOCK();
}
#endif
