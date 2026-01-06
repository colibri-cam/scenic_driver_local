#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "comms.h"
#include "device.h"
#include "fontstash.h"
#include "scenic_ops.h"
#include "skia_ctx.h"

#define FB0_TIMEOUT 60 //seconds

typedef struct {
  int fd;

  union {
    uint8_t  *c;
    uint16_t *s;
    uint32_t *i;
  } rgb_buff;

  struct fb_var_screeninfo var;
  struct fb_fix_screeninfo fix;
} skia_fb_t;

static skia_fb_t g_skia_fb = {0};

extern device_info_t g_device_info;
extern device_opts_t g_opts;

static inline uint8_t to_8_color(uint8_t r, uint8_t g, uint8_t b)
{
  return ((((r >> 5) & 7) << 5) |
          (((g >> 5) & 7) << 2) |
          ((b >> 6) & 3));
}

static inline uint16_t to_15_color(uint8_t r, uint8_t g, uint8_t b)
{
  return ((((r >> 3) & 31) << 10) |
          (((g >> 3) & 31) << 5)  |
          ((b >> 3) & 31));
}

static inline uint16_t to_15_color_bgr(uint8_t r, uint8_t g, uint8_t b)
{
  return ((((b >> 3) & 31) << 10) |
          (((g >> 3) & 31) << 5)  |
          ((r >> 3) & 31));
}

static inline uint16_t to_16_color(uint8_t r, uint8_t g, uint8_t b)
{
  return ((((r >> 3) & 31) << 11) |
          (((g >> 2) & 63) << 5)  |
          ((b >> 3) & 31));
}

static void render_surface_to_fb(scenic_skia_ctx_t* p_ctx)
{
  sk_pixmap_t* pixmap = sk_pixmap_new();
  if (!sk_surface_peek_pixels(p_ctx->surface, pixmap)) {
    log_error("skia: failed to peek pixels");
    sk_pixmap_destructor(pixmap);
    return;
  }

  sk_imageinfo_t info = {0};
  sk_pixmap_get_info(pixmap, &info);

  const uint8_t* sk_buff = sk_pixmap_get_writable_addr(pixmap);
  size_t row_bytes = sk_pixmap_get_row_bytes(pixmap);

  uint32_t width = info.width;
  uint32_t height = info.height;

  bool is_bgr555 = ((g_skia_fb.var.red.offset == 0 &&
                     g_skia_fb.var.green.offset == 5 &&
                     g_skia_fb.var.blue.offset == 10))
                    ? true
                    : false;

  int cpp = 0;
  switch (g_skia_fb.var.bits_per_pixel)
  {
  case 8:
    cpp = 1;
    for (uint32_t y = 0; y < height; y++) {
      const uint8_t* row = sk_buff + (y * row_bytes);
      for (uint32_t x = 0, j = 0; x < width; x++, j += 4) {
        uint8_t b = row[j + 0];
        uint8_t g = row[j + 1];
        uint8_t r = row[j + 2];
        g_skia_fb.rgb_buff.c[y * width + x] = to_8_color(r, g, b);
      }
    }
    break;
  case 15:
    cpp = 2;
    for (uint32_t y = 0; y < height; y++) {
      const uint8_t* row = sk_buff + (y * row_bytes);
      for (uint32_t x = 0, j = 0; x < width; x++, j += 4) {
        uint8_t b = row[j + 0];
        uint8_t g = row[j + 1];
        uint8_t r = row[j + 2];
        g_skia_fb.rgb_buff.s[y * width + x] = is_bgr555
          ? to_15_color_bgr(r, g, b)
          : to_15_color(r, g, b);
      }
    }
    break;
  case 16:
    cpp = 2;
    for (uint32_t y = 0; y < height; y++) {
      const uint8_t* row = sk_buff + (y * row_bytes);
      for (uint32_t x = 0, j = 0; x < width; x++, j += 4) {
        uint8_t b = row[j + 0];
        uint8_t g = row[j + 1];
        uint8_t r = row[j + 2];
        g_skia_fb.rgb_buff.s[y * width + x] = is_bgr555
          ? to_15_color_bgr(r, g, b)
          : to_16_color(r, g, b);
      }
    }
    break;
  case 24:
    cpp = 3;
    for (uint32_t y = 0; y < height; y++) {
      const uint8_t* row = sk_buff + (y * row_bytes);
      for (uint32_t x = 0, j = 0; x < width; x++, j += 4) {
        size_t idx = (y * width + x) * 3;
        g_skia_fb.rgb_buff.c[idx + 0] = row[j + 0];
        g_skia_fb.rgb_buff.c[idx + 1] = row[j + 1];
        g_skia_fb.rgb_buff.c[idx + 2] = row[j + 2];
      }
    }
    break;
  case 32:
    cpp = 4;
    for (uint32_t y = 0; y < height; y++) {
      const uint8_t* row = sk_buff + (y * row_bytes);
      for (uint32_t x = 0, j = 0; x < width; x++, j += 4) {
        g_skia_fb.rgb_buff.i[y * width + x] = ((row[j + 2] << 16)) |
                                              (row[j + 1] << 8) |
                                              (row[j + 0]);
      }
    }
    break;
  }

  uint32_t x_stride = (g_skia_fb.fix.line_length * 8) / g_skia_fb.var.bits_per_pixel;

  uint32_t pic_xs = g_device_info.width;
  uint32_t pic_ys = g_device_info.height;
  uint32_t scr_xs = x_stride;
  uint32_t scr_ys = g_skia_fb.var.yres;

  uint32_t xc = (pic_xs > scr_xs) ? scr_xs : pic_xs;
  uint32_t yc = (pic_ys > scr_ys) ? scr_ys : pic_ys;

  uint32_t x_offs = (pic_xs < g_skia_fb.var.xres)
                    ? (g_skia_fb.var.xres - pic_xs) / 2
                    : 0;
  uint32_t y_offs = (pic_ys < g_skia_fb.var.yres)
                    ? (g_skia_fb.var.yres - pic_ys) / 2
                    : 0;

  size_t fb_size = scr_xs * scr_ys * cpp;
  uint8_t* fb = mmap(NULL, fb_size, PROT_WRITE | PROT_READ, MAP_SHARED, g_skia_fb.fd, 0);

  if (fb == MAP_FAILED) {
    log_error("skia: failed to mmap fb");
    sk_pixmap_destructor(pixmap);
    return;
  }

  uint8_t* p_fb = fb + (y_offs * scr_xs + x_offs) * cpp;
  uint8_t* p_image = g_skia_fb.rgb_buff.c;

  for (uint32_t i = 0; i < yc; i++, p_fb += scr_xs * cpp, p_image += pic_xs * cpp)
    memcpy(p_fb, p_image, xc * cpp);

  munmap(fb, fb_size);
  sk_pixmap_destructor(pixmap);
}

int device_init(const device_opts_t* p_opts,
                device_info_t* p_info,
                driver_data_t* p_data)
{
  if (g_opts.debug_mode) {
    log_info("skia %s", __func__);
  }

  scenic_skia_ctx_t* p_ctx = scenic_skia_init(p_opts, p_info);
  if (!p_ctx) {
    return -1;
  }

  p_info->v_ctx = p_ctx;

  time_t fb0_timer_start = time(NULL);
  while ((time(NULL) - fb0_timer_start) < FB0_TIMEOUT) {
    if ((g_skia_fb.fd = open(g_opts.fbdev, O_RDWR)) != -1) {
      break;
    }
    sched_yield();
  }

  if (g_skia_fb.fd == -1) {
    log_error("Failed to open device %s: %s", g_opts.fbdev, strerror(errno));
    return -1;
  }

  if (ioctl(g_skia_fb.fd, FBIOGET_VSCREENINFO, &g_skia_fb.var)) {
    log_error("Failed to get fb_var_screeninfo: %s", strerror(errno));
    return -1;
  }

  if (ioctl(g_skia_fb.fd, FBIOGET_FSCREENINFO, &g_skia_fb.fix)) {
    log_error("Failed to get fb_fix_screeninfo: %s", strerror(errno));
    return -1;
  }

  uint32_t width = p_info->width;
  uint32_t height = p_info->height;
  size_t pix_count = width * height;

  switch (g_skia_fb.var.bits_per_pixel)
  {
  case 8:
    g_skia_fb.rgb_buff.c = (uint8_t*)malloc(pix_count * sizeof(uint8_t));
    break;
  case 15:
  case 16:
    g_skia_fb.rgb_buff.c = (uint8_t*)malloc(pix_count * sizeof(uint16_t));
    break;
  case 24:
    g_skia_fb.rgb_buff.c = (uint8_t*)malloc(pix_count * 3 * sizeof(uint8_t));
    break;
  case 32:
    g_skia_fb.rgb_buff.c = (uint8_t*)malloc(pix_count * sizeof(uint32_t));
    break;
  default:
    log_error("skia: Unsupported video mode: %dbpp", g_skia_fb.var.bits_per_pixel);
    return -1;
  }

  return 0;
}

int device_close(device_info_t* p_info)
{
  if (g_opts.debug_mode) {
    log_info("skia %s", __func__);
  }

  close(g_skia_fb.fd);
  free(g_skia_fb.rgb_buff.c);

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)p_info->v_ctx;
  scenic_skia_fini(p_ctx);

  return 0;
}

void device_poll()
{
}

void device_begin_render(driver_data_t* p_data)
{
  if (g_opts.debug_mode) {
    log_info("skia %s", __func__);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)p_data->v_ctx;

  sk_color4f_t clear = skia_color_from_rgba(p_ctx->clear_color);
  sk_canvas_clear_color4f(p_ctx->canvas, clear);
  sk_path_reset(p_ctx->path);
}

void device_end_render(driver_data_t* p_data)
{
  if (g_opts.debug_mode) {
    log_info("skia %s", __func__);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)p_data->v_ctx;
  render_surface_to_fb(p_ctx);
}

void device_loop(driver_data_t* p_data)
{
  scenic_loop(p_data);
}

