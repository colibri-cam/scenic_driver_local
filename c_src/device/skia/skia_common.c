#include <stdlib.h>
#include <string.h>

#include "device.h"
#include "scenic_types.h"
#include "skia_ctx.h"

extern device_info_t g_device_info;

sk_color4f_t skia_color_from_rgba(color_rgba_t color)
{
  return (sk_color4f_t){
    .fR = color.red / 255.0f,
    .fG = color.green / 255.0f,
    .fB = color.blue / 255.0f,
    .fA = color.alpha / 255.0f,
  };
}

static paint_pair_t init_paints(bool antialias)
{
  paint_pair_t paints = {0};

  paints.fill = sk_paint_new();
  sk_paint_set_antialias(paints.fill, antialias);
  sk_paint_set_style(paints.fill, FILL_SK_PAINT_STYLE);
  sk_paint_set_color(paints.fill, sk_color_set_argb(255, 255, 255, 255));

  paints.stroke = sk_paint_new();
  sk_paint_set_antialias(paints.stroke, antialias);
  sk_paint_set_style(paints.stroke, STROKE_SK_PAINT_STYLE);
  sk_paint_set_color(paints.stroke, sk_color_set_argb(255, 255, 255, 255));
  sk_paint_set_stroke_width(paints.stroke, 1.0f);

  return paints;
}

static void delete_paints(paint_pair_t* paints)
{
  if (paints->fill) sk_paint_delete(paints->fill);
  if (paints->stroke) sk_paint_delete(paints->stroke);
}

scenic_skia_ctx_t* scenic_skia_init(const device_opts_t* p_opts,
                                    device_info_t* p_info)
{
  sk_imageinfo_t info = {
    .colorspace = NULL,
    .width = p_info->width,
    .height = p_info->height,
    .colorType = BGRA_8888_SK_COLORTYPE,
    .alphaType = PREMUL_SK_ALPHATYPE,
  };

  sk_surface_t* surface = sk_surface_new_raster(&info, 0, NULL);
  if (!surface) {
    return NULL;
  }

  return scenic_skia_init_with_surface(p_opts, p_info, surface);
}

static scenic_skia_ctx_t* scenic_skia_alloc(const device_opts_t* p_opts,
                                            device_info_t* p_info)
{
  scenic_skia_ctx_t* p_ctx = calloc(1, sizeof(scenic_skia_ctx_t));
  if (!p_ctx) return NULL;

  p_ctx->ratio = 1.0f;
  p_ctx->antialias = p_opts->antialias;
  p_ctx->font_size = 10.0f;
  p_ctx->text_align = TEXT_ALIGN_LEFT;
  p_ctx->text_base = TEXT_BASE_ALPHABETIC;

  p_ctx->clear_color = (color_rgba_t){
    .red = 0.0f,
    .green = 0.0f,
    .blue = 0.0f,
    .alpha = 1.0f
  };

  p_info->width = p_opts->width;
  p_info->height = p_opts->height;
  p_info->v_ctx = p_ctx;

  p_ctx->path = sk_path_new();
  p_ctx->paints = init_paints(p_ctx->antialias);

  p_ctx->active_font = sk_font_new();
  sk_font_set_size(p_ctx->active_font, p_ctx->font_size);

  return p_ctx;
}

scenic_skia_ctx_t* scenic_skia_init_with_surface(const device_opts_t* p_opts,
                                                 device_info_t* p_info,
                                                 sk_surface_t* surface)
{
  scenic_skia_ctx_t* p_ctx = scenic_skia_alloc(p_opts, p_info);
  if (!p_ctx) {
    if (surface) sk_surface_unref(surface);
    return NULL;
  }

  if (!surface) {
    scenic_skia_fini(p_ctx);
    return NULL;
  }

  scenic_skia_replace_surface(p_ctx, surface);

  return p_ctx;
}

void scenic_skia_replace_surface(scenic_skia_ctx_t* p_ctx,
                                 sk_surface_t* surface)
{
  if (!p_ctx || !surface) return;

  if (p_ctx->surface) sk_surface_unref(p_ctx->surface);

  p_ctx->surface = surface;
  p_ctx->canvas = sk_surface_get_canvas(surface);
}

void scenic_skia_fini(scenic_skia_ctx_t* p_ctx)
{
  if (!p_ctx) return;

  if (p_ctx->fill_shader) sk_shader_unref(p_ctx->fill_shader);
  if (p_ctx->stroke_shader) sk_shader_unref(p_ctx->stroke_shader);

  for (int i = 0; i < p_ctx->images_used; i++) {
    if (p_ctx->images[i].shader) sk_shader_unref(p_ctx->images[i].shader);
    if (p_ctx->images[i].image) sk_image_unref(p_ctx->images[i].image);
  }
  free(p_ctx->images);

  for (int i = 0; i < p_ctx->fonts_used; i++) {
    if (p_ctx->fonts[i].typeface) sk_typeface_unref(p_ctx->fonts[i].typeface);
  }
  free(p_ctx->fonts);

  while (p_ctx->pattern_stack_head) {
    pattern_stack_pop(p_ctx);
  }

  if (p_ctx->active_font) sk_font_delete(p_ctx->active_font);
  if (p_ctx->path) sk_path_delete(p_ctx->path);

  delete_paints(&p_ctx->paints);

  if (p_ctx->surface) sk_surface_unref(p_ctx->surface);

  free(p_ctx);
}

void pattern_stack_push(scenic_skia_ctx_t* p_ctx)
{
  pattern_stack_t* node = calloc(1, sizeof(pattern_stack_t));
  if (!node) return;

  node->paints.fill = sk_paint_clone(p_ctx->paints.fill);
  node->paints.stroke = sk_paint_clone(p_ctx->paints.stroke);
  if (p_ctx->fill_shader) sk_shader_ref(p_ctx->fill_shader);
  if (p_ctx->stroke_shader) sk_shader_ref(p_ctx->stroke_shader);
  node->fill_shader = p_ctx->fill_shader;
  node->stroke_shader = p_ctx->stroke_shader;
  node->text_align = p_ctx->text_align;
  node->text_base = p_ctx->text_base;

  node->next = p_ctx->pattern_stack_head;
  p_ctx->pattern_stack_head = node;
}

void pattern_stack_pop(scenic_skia_ctx_t* p_ctx)
{
  pattern_stack_t* node = p_ctx->pattern_stack_head;
  if (!node) {
    log_error("pattern stack underflow");
    return;
  }

  delete_paints(&p_ctx->paints);
  p_ctx->paints.fill = node->paints.fill;
  p_ctx->paints.stroke = node->paints.stroke;

  if (p_ctx->fill_shader) sk_shader_unref(p_ctx->fill_shader);
  if (p_ctx->stroke_shader) sk_shader_unref(p_ctx->stroke_shader);
  p_ctx->fill_shader = node->fill_shader;
  p_ctx->stroke_shader = node->stroke_shader;

  p_ctx->text_align = node->text_align;
  p_ctx->text_base = node->text_base;

  p_ctx->pattern_stack_head = node->next;
  free(node);
}

void device_begin_cursor_render(driver_data_t* p_data)
{
  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)p_data->v_ctx;
  sk_canvas_translate(p_ctx->canvas, p_data->cursor_pos[0], p_data->cursor_pos[1]);
}

void device_clear_color(float red, float green, float blue, float alpha)
{
  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)g_device_info.v_ctx;
  p_ctx->clear_color = (color_rgba_t){
    .red = red,
    .green = green,
    .blue = blue,
    .alpha = alpha,
  };
}

char* device_gl_error()
{
  return NULL;
}

image_pattern_data_t* find_image_pattern(scenic_skia_ctx_t* p_ctx, int id)
{
  for (int i = 0; i < p_ctx->images_used; i++) {
    if (p_ctx->images[i].id == id) {
      return &p_ctx->images[i];
    }
  }
  return NULL;
}

font_data_t* find_font(scenic_skia_ctx_t* p_ctx, int id)
{
  for (int i = 0; i < p_ctx->fonts_used; i++) {
    if (p_ctx->fonts[i].id == id) {
      return &p_ctx->fonts[i];
    }
  }
  return NULL;
}
