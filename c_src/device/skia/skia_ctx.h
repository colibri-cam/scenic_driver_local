#pragma once

#include <stdbool.h>
#include <sk_canvas.h>
#include <sk_font.h>
#include <sk_image.h>
#include <sk_matrix.h>
#include <sk_paint.h>
#include <sk_path.h>
#include <sk_pixmap.h>
#include <sk_rrect.h>
#include <sk_shader.h>
#include <sk_surface.h>
#include "script_ops.h"

typedef struct {
  sk_paint_t* fill;
  sk_paint_t* stroke;
} paint_pair_t;

typedef struct pattern_stack_t_ {
  paint_pair_t paints;
  sk_shader_t* fill_shader;
  sk_shader_t* stroke_shader;
  text_align_t text_align;
  text_base_t text_base;
  struct pattern_stack_t_* next;
} pattern_stack_t;

typedef struct {
  int id;
  sk_image_t* image;
  sk_shader_t* shader;
} image_pattern_data_t;

typedef struct {
  int id;
  sk_typeface_t* typeface;
} font_data_t;

typedef struct {
  color_rgba_t clear_color;
  float font_size;
  text_align_t text_align;
  text_base_t text_base;
  float ratio;
  bool antialias;

  sk_surface_t* surface;
  sk_canvas_t* canvas;
  sk_path_t* path;
  paint_pair_t paints;
  sk_shader_t* fill_shader;
  sk_shader_t* stroke_shader;
  pattern_stack_t* pattern_stack_head;

  int images_count;
  int images_used;
  int highest_image_id;
  image_pattern_data_t* images;

  int fonts_count;
  int fonts_used;
  int highest_font_id;
  font_data_t* fonts;
  sk_font_t* active_font;
} scenic_skia_ctx_t;

scenic_skia_ctx_t* scenic_skia_init(const device_opts_t* p_opts,
                                    device_info_t* p_info);
void scenic_skia_fini(scenic_skia_ctx_t* p_ctx);

void pattern_stack_push(scenic_skia_ctx_t* p_ctx);
void pattern_stack_pop(scenic_skia_ctx_t* p_ctx);

image_pattern_data_t* find_image_pattern(scenic_skia_ctx_t* p_ctx, int id);
font_data_t* find_font(scenic_skia_ctx_t* p_ctx, int id);
sk_color4f_t skia_color_from_rgba(color_rgba_t color);
