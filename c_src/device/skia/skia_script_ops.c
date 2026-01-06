#include <math.h>
#include <sk_canvas.h>
#include <sk_font.h>
#include <sk_image.h>
#include <sk_paint.h>
#include <sk_path.h>
#include <sk_rrect.h>
#include <sk_shader.h>

#include "comms.h"
#include "font.h"
#include "image.h"
#include "script.h"
#include "script_ops.h"
#include "skia_ctx.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern device_opts_t g_opts;

static const char* log_prefix = "skia";

static sk_rect_t rect_from_origin(float w, float h)
{
  return (sk_rect_t){ .left = 0, .top = 0, .right = w, .bottom = h };
}

static void set_fill_shader(scenic_skia_ctx_t* p_ctx, sk_shader_t* shader)
{
  if (p_ctx->fill_shader) sk_shader_unref(p_ctx->fill_shader);
  p_ctx->fill_shader = shader;
  sk_paint_set_shader(p_ctx->paints.fill, shader);
}

static void set_stroke_shader(scenic_skia_ctx_t* p_ctx, sk_shader_t* shader)
{
  if (p_ctx->stroke_shader) sk_shader_unref(p_ctx->stroke_shader);
  p_ctx->stroke_shader = shader;
  sk_paint_set_shader(p_ctx->paints.stroke, shader);
}

static void set_fill_color(scenic_skia_ctx_t* p_ctx, color_rgba_t color)
{
  sk_color4f_t c = skia_color_from_rgba(color);
  set_fill_shader(p_ctx, NULL);
  sk_paint_set_color4f(p_ctx->paints.fill, &c, NULL);
}

static void set_stroke_color(scenic_skia_ctx_t* p_ctx, color_rgba_t color)
{
  sk_color4f_t c = skia_color_from_rgba(color);
  set_stroke_shader(p_ctx, NULL);
  sk_paint_set_color4f(p_ctx->paints.stroke, &c, NULL);
}

static void do_fill_stroke(scenic_skia_ctx_t* p_ctx, bool fill, bool stroke)
{
  if (fill) {
    sk_canvas_draw_path(p_ctx->canvas, p_ctx->path, p_ctx->paints.fill);
  }
  if (stroke) {
    sk_canvas_draw_path(p_ctx->canvas, p_ctx->path, p_ctx->paints.stroke);
  }
}

void script_ops_draw_line(void* v_ctx,
                          coordinates_t a,
                          coordinates_t b,
                          bool stroke)
{
  if (g_opts.debug_mode) {
    log_script_ops_draw_line(log_prefix, __func__, log_level_info,
                             a, b, stroke);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  if (stroke) {
    sk_canvas_draw_line(p_ctx->canvas, a.x, a.y, b.x, b.y, p_ctx->paints.stroke);
  }
}

void script_ops_draw_triangle(void* v_ctx,
                              coordinates_t a,
                              coordinates_t b,
                              coordinates_t c,
                              bool fill, bool stroke)
{
  if (g_opts.debug_mode) {
    log_script_ops_draw_triangle(log_prefix, __func__, log_level_info,
                                 a, b, c, fill, stroke);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_path_reset(p_ctx->path);
  sk_path_move_to(p_ctx->path, a.x, a.y);
  sk_path_line_to(p_ctx->path, b.x, b.y);
  sk_path_line_to(p_ctx->path, c.x, c.y);
  sk_path_close(p_ctx->path);
  do_fill_stroke(p_ctx, fill, stroke);
}

void script_ops_draw_quad(void* v_ctx,
                          coordinates_t a,
                          coordinates_t b,
                          coordinates_t c,
                          coordinates_t d,
                          bool fill, bool stroke)
{
  if (g_opts.debug_mode) {
    log_script_ops_draw_quad(log_prefix, __func__, log_level_info,
                             a, b, c, d, fill, stroke);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_path_reset(p_ctx->path);
  sk_path_move_to(p_ctx->path, a.x, a.y);
  sk_path_line_to(p_ctx->path, b.x, b.y);
  sk_path_line_to(p_ctx->path, c.x, c.y);
  sk_path_line_to(p_ctx->path, d.x, d.y);
  sk_path_close(p_ctx->path);
  do_fill_stroke(p_ctx, fill, stroke);
}

void script_ops_draw_rect(void* v_ctx,
                          float w,
                          float h,
                          bool fill, bool stroke)
{
  if (g_opts.debug_mode) {
    log_script_ops_draw_rect(log_prefix, __func__, log_level_info,
                             w, h, fill, stroke);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_rect_t rect = rect_from_origin(w, h);
  if (fill) sk_canvas_draw_rect(p_ctx->canvas, &rect, p_ctx->paints.fill);
  if (stroke) sk_canvas_draw_rect(p_ctx->canvas, &rect, p_ctx->paints.stroke);
}

void script_ops_draw_rrect(void* v_ctx,
                           float w,
                           float h,
                           float radius,
                           bool fill, bool stroke)
{
  if (g_opts.debug_mode) {
    log_script_ops_draw_rrect(log_prefix, __func__, log_level_info,
                              w, h, radius, fill, stroke);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_rect_t rect = rect_from_origin(w, h);
  if (fill) sk_canvas_draw_round_rect(p_ctx->canvas, &rect, radius, radius, p_ctx->paints.fill);
  if (stroke) sk_canvas_draw_round_rect(p_ctx->canvas, &rect, radius, radius, p_ctx->paints.stroke);
}

void script_ops_draw_rrectv(void* v_ctx,
                           float w,
                           float h,
                           float ulr,
                           float urr,
                           float lrr,
                           float llr,
                           bool fill, bool stroke)
{
  if (g_opts.debug_mode) {
    log_script_ops_draw_rrectv(log_prefix, __func__, log_level_info,
                               w, h, ulr, urr, lrr, llr, fill, stroke);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_rrect_t* rrect = sk_rrect_new();
  sk_rect_t rect = rect_from_origin(w, h);
  sk_vector_t radii[4] = {
    {.x = ulr, .y = ulr},
    {.x = urr, .y = urr},
    {.x = lrr, .y = lrr},
    {.x = llr, .y = llr},
  };
  sk_rrect_set_rect_radii(rrect, &rect, radii);
  if (fill) sk_canvas_draw_rrect(p_ctx->canvas, rrect, p_ctx->paints.fill);
  if (stroke) sk_canvas_draw_rrect(p_ctx->canvas, rrect, p_ctx->paints.stroke);
  sk_rrect_delete(rrect);
}

void script_ops_draw_arc(void* v_ctx,
                         float radius,
                         float radians,
                         bool fill, bool stroke)
{
  if (g_opts.debug_mode) {
    log_script_ops_draw_arc(log_prefix, __func__, log_level_info,
                            radius, radians, fill, stroke);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_path_reset(p_ctx->path);
  sk_path_move_to(p_ctx->path, radius, 0);
  sk_path_arc_to_with_points(p_ctx->path,
                             radius, 0,
                             radius * cosf(radians), radius * sinf(radians),
                             radius);
  if (fill || stroke) do_fill_stroke(p_ctx, fill, stroke);
}

void script_ops_draw_sector(void* v_ctx,
                            float radius,
                            float radians,
                            bool fill, bool stroke)
{
  if (g_opts.debug_mode) {
    log_script_ops_draw_sector(log_prefix, __func__, log_level_info,
                               radius, radians, fill, stroke);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_path_reset(p_ctx->path);
  sk_path_move_to(p_ctx->path, 0, 0);
  sk_path_line_to(p_ctx->path, radius, 0);
  sk_path_arc_to_with_points(p_ctx->path,
                             radius, 0,
                             radius * cosf(radians), radius * sinf(radians),
                             radius);
  sk_path_close(p_ctx->path);
  do_fill_stroke(p_ctx, fill, stroke);
}

void script_ops_draw_circle(void* v_ctx,
                            float radius,
                            bool fill, bool stroke)
{
  if (g_opts.debug_mode) {
    log_script_ops_draw_circle(log_prefix, __func__, log_level_info,
                               radius, fill, stroke);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  if (fill) sk_canvas_draw_circle(p_ctx->canvas, 0, 0, radius, p_ctx->paints.fill);
  if (stroke) sk_canvas_draw_circle(p_ctx->canvas, 0, 0, radius, p_ctx->paints.stroke);
}

void script_ops_draw_ellipse(void* v_ctx,
                             float radius0,
                             float radius1,
                             bool fill, bool stroke)
{
  if (g_opts.debug_mode) {
    log_script_ops_draw_ellipse(log_prefix, __func__, log_level_info,
                                radius0, radius1, fill, stroke);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_rect_t rect = { .left = -radius0, .top = -radius1, .right = radius0, .bottom = radius1 };
  if (fill) sk_canvas_draw_oval(p_ctx->canvas, &rect, p_ctx->paints.fill);
  if (stroke) sk_canvas_draw_oval(p_ctx->canvas, &rect, p_ctx->paints.stroke);
}

void script_ops_draw_text(void* v_ctx,
                          uint32_t size,
                          const char* text)
{
  if (g_opts.debug_mode) {
    log_script_ops_draw_text(log_prefix, __func__, log_level_info,
                             size, text);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_rect_t bounds = {0};
  sk_font_measure_text(p_ctx->active_font, text, size, UTF8_SK_TEXT_ENCODING, &bounds, p_ctx->paints.fill);

  sk_fontmetrics_t metrics = {0};
  sk_font_get_metrics(p_ctx->active_font, &metrics);

  float align_offset = 0;
  switch (p_ctx->text_align) {
  case TEXT_ALIGN_LEFT:
    align_offset = 0;
    break;
  case TEXT_ALIGN_CENTER:
    align_offset = -(bounds.right - bounds.left) / 2.0f;
    break;
  case TEXT_ALIGN_RIGHT:
    align_offset = -(bounds.right - bounds.left);
    break;
  }

  float base_offset = 0;
  switch (p_ctx->text_base) {
  case TEXT_BASE_TOP:
    base_offset = -metrics.fAscent;
    break;
  case TEXT_BASE_MIDDLE:
    base_offset = -((metrics.fDescent - metrics.fAscent) / 2.0f);
    break;
  case TEXT_BASE_ALPHABETIC:
    base_offset = 0;
    break;
  case TEXT_BASE_BOTTOM:
    base_offset = -metrics.fDescent;
    break;
  }

  sk_canvas_save(p_ctx->canvas);
  sk_canvas_translate(p_ctx->canvas, align_offset, base_offset);
  sk_canvas_draw_simple_text(p_ctx->canvas, text, size, UTF8_SK_TEXT_ENCODING,
                             0, 0, p_ctx->active_font, p_ctx->paints.fill);
  sk_canvas_restore(p_ctx->canvas);
}

static void draw_sprite(scenic_skia_ctx_t* p_ctx,
                        sk_image_t* image,
                        const sprite_t sprite)
{
  sk_rect_t src = {
    .left = sprite.sx,
    .top = sprite.sy,
    .right = sprite.sx + sprite.sw,
    .bottom = sprite.sy + sprite.sh,
  };
  sk_rect_t dst = {
    .left = sprite.dx,
    .top = sprite.dy,
    .right = sprite.dx + sprite.dw,
    .bottom = sprite.dy + sprite.dh,
  };

  sk_paint_t* paint = sk_paint_clone(p_ctx->paints.fill);
  uint8_t alpha = (uint8_t)(sprite.alpha * 255.0f);
  sk_color_t color = sk_color_set_argb(alpha, 255, 255, 255);
  sk_paint_set_color(paint, color);

  sk_canvas_draw_image_rect(p_ctx->canvas, image, &src, &dst, NULL, paint);
  sk_paint_delete(paint);
}

void script_ops_draw_sprites(void* v_ctx,
                             sid_t id,
                             uint32_t count,
                             const sprite_t* sprites)
{
  if (g_opts.debug_mode) {
    log_script_ops_draw_sprites(log_prefix, __func__, log_level_info,
                                id, count, sprites);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;

  image_t* p_image = get_image(id);
  if (!p_image) return;

  image_pattern_data_t* image_data = find_image_pattern(p_ctx, p_image->image_id);
  if (!image_data) return;

  for (uint32_t i = 0; i < count; i++) {
    draw_sprite(p_ctx, image_data->image, sprites[i]);
  }
}

void script_ops_draw_script(void* v_ctx, sid_t id)
{
  render_script(v_ctx, id);
}

void script_ops_begin_path(void* v_ctx)
{
  if (g_opts.debug_mode) {
    log_script_ops_begin_path(log_prefix, __func__, log_level_info);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_path_reset(p_ctx->path);
}

void script_ops_close_path(void* v_ctx)
{
  if (g_opts.debug_mode) {
    log_script_ops_close_path(log_prefix, __func__, log_level_info);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_path_close(p_ctx->path);
}

void script_ops_fill_path(void* v_ctx)
{
  if (g_opts.debug_mode) {
    log_script_ops_fill_path(log_prefix, __func__, log_level_info);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_canvas_draw_path(p_ctx->canvas, p_ctx->path, p_ctx->paints.fill);
}

void script_ops_stroke_path(void* v_ctx)
{
  if (g_opts.debug_mode) {
    log_script_ops_stroke_path(log_prefix, __func__, log_level_info);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_canvas_draw_path(p_ctx->canvas, p_ctx->path, p_ctx->paints.stroke);
}

void script_ops_move_to(void* v_ctx,
                        coordinates_t a)
{
  if (g_opts.debug_mode) {
    log_script_ops_move_to(log_prefix, __func__, log_level_info,
                           a);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_path_move_to(p_ctx->path, a.x, a.y);
}

void script_ops_line_to(void* v_ctx,
                        coordinates_t a)
{
  if (g_opts.debug_mode) {
    log_script_ops_line_to(log_prefix, __func__, log_level_info,
                           a);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_path_line_to(p_ctx->path, a.x, a.y);
}

void script_ops_arc_to(void* v_ctx,
                       coordinates_t a,
                       coordinates_t b,
                       float radius)
{
  if (g_opts.debug_mode) {
    log_script_ops_arc_to(log_prefix, __func__, log_level_info,
                          a, b, radius);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_path_arc_to_with_points(p_ctx->path, a.x, a.y, b.x, b.y, radius);
}

void script_ops_bezier_to(void* v_ctx,
                          coordinates_t c0,
                          coordinates_t c1,
                          coordinates_t a)
{
  if (g_opts.debug_mode) {
    log_script_ops_bezier_to(log_prefix, __func__, log_level_info,
                             c0, c1, a);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_path_cubic_to(p_ctx->path, c0.x, c0.y, c1.x, c1.y, a.x, a.y);
}

void script_ops_quadratic_to(void* v_ctx,
                             coordinates_t c,
                             coordinates_t a)
{
  if (g_opts.debug_mode) {
    log_script_ops_quadratic_to(log_prefix, __func__, log_level_info,
                                c, a);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_path_quad_to(p_ctx->path, c.x, c.y, a.x, a.y);
}

void script_ops_arc(void *v_ctx,
                    coordinates_t c,
                    float r,
                    float a0, float a1,
                    sweep_dir_t sweep_dir)
{
  if (g_opts.debug_mode) {
    log_script_ops_arc(log_prefix, __func__, log_level_info,
                                c, r, a0, a1, sweep_dir);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_rect_t oval = {
    .left = c.x - r,
    .top = c.y - r,
    .right = c.x + r,
    .bottom = c.y + r,
  };
  float sweep = (a1 - a0) * (180.0f / M_PI);
  if (sweep_dir == SWEEP_DIR_CCW) sweep = -sweep;
  sk_path_add_arc(p_ctx->path, &oval, a0 * (180.0f / M_PI), sweep);
}

void script_ops_push_state(void* v_ctx)
{
  if (g_opts.debug_mode) {
    log_script_ops_push_state(log_prefix, __func__, log_level_info);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;

  pattern_stack_push(p_ctx);
  sk_canvas_save(p_ctx->canvas);
}

void script_ops_pop_state(void* v_ctx)
{
  if (g_opts.debug_mode) {
    log_script_ops_pop_state(log_prefix, __func__, log_level_info);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;

  sk_canvas_restore(p_ctx->canvas);
  pattern_stack_pop(p_ctx);
}

void script_ops_scissor(void* v_ctx,
                        float w, float h)
{
  if (g_opts.debug_mode) {
    log_script_ops_scissor(log_prefix, __func__, log_level_info,
                           w, h);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_rect_t rect = rect_from_origin(w, h);
  sk_canvas_clip_rect_with_operation(p_ctx->canvas, &rect, INTERSECT_SK_CLIPOP, true);
}

void script_ops_transform(void* v_ctx,
                          float a, float b, float c, float d, float e, float f)
{
  if (g_opts.debug_mode) {
    log_script_ops_transform(log_prefix, __func__, log_level_info,
                             a, b, c, d, e, f);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_matrix44_t matrix = {
    .m00 = a, .m01 = c, .m02 = 0, .m03 = e,
    .m10 = b, .m11 = d, .m12 = 0, .m13 = f,
    .m20 = 0, .m21 = 0, .m22 = 1, .m23 = 0,
    .m30 = 0, .m31 = 0, .m32 = 0, .m33 = 1,
  };
  sk_canvas_concat(p_ctx->canvas, &matrix);
}

void script_ops_scale(void* v_ctx,
                      float x, float y)
{
  if (g_opts.debug_mode) {
    log_script_ops_scale(log_prefix, __func__, log_level_info,
                         x, y);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_canvas_scale(p_ctx->canvas, x, y);
}

void script_ops_rotate(void* v_ctx,
                       float radians)
{
  if (g_opts.debug_mode) {
    log_script_ops_rotate(log_prefix, __func__, log_level_info,
                          radians);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_canvas_rotate_radians(p_ctx->canvas, radians);
}

void script_ops_translate(void* v_ctx,
                          float x, float y)
{
  if (g_opts.debug_mode) {
    log_script_ops_translate(log_prefix, __func__, log_level_info,
                             x, y);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_canvas_translate(p_ctx->canvas, x, y);
}

void script_ops_fill_color(void* v_ctx,
                           color_rgba_t color)
{
  if (g_opts.debug_mode) {
    log_script_ops_fill_color(log_prefix, __func__, log_level_info,
                              color);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  set_fill_color(p_ctx, color);
}

void script_ops_fill_linear(void* v_ctx,
                            coordinates_t start, coordinates_t end,
                            color_rgba_t color_start, color_rgba_t color_end)
{
  if (g_opts.debug_mode) {
    log_script_ops_fill_linear(log_prefix, __func__, log_level_info,
                               start, end, color_start, color_end);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;

  sk_point_t pts[2] = {{start.x, start.y}, {end.x, end.y}};
  sk_color4f_t colors[2] = { skia_color_from_rgba(color_start), skia_color_from_rgba(color_end) };
  float positions[2] = {0.0f, 1.0f};
  sk_shader_t* shader = sk_shader_new_linear_gradient_color4f(pts, colors, NULL, positions, 2, CLAMP_SK_SHADER_TILEMODE, NULL);
  set_fill_shader(p_ctx, shader);
}

void script_ops_fill_radial(void* v_ctx,
                            coordinates_t center,
                            float inner_radius,
                            float outer_radius,
                            color_rgba_t color_start,
                            color_rgba_t color_end)
{
  if (g_opts.debug_mode) {
    log_script_ops_fill_radial(log_prefix, __func__, log_level_info,
                               center, inner_radius, outer_radius, color_start, color_end);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_point_t c = { center.x, center.y };
  sk_color4f_t colors[2] = { skia_color_from_rgba(color_start), skia_color_from_rgba(color_end) };
  float positions[2] = {0.0f, 1.0f};
  sk_shader_t* shader = sk_shader_new_two_point_conical_gradient_color4f(&c, inner_radius,
                                                                         &c, outer_radius,
                                                                         colors, NULL, positions, 2,
                                                                         CLAMP_SK_SHADER_TILEMODE, NULL);
  set_fill_shader(p_ctx, shader);
}

void script_ops_fill_image(void* v_ctx, sid_t id)
{
  if (g_opts.debug_mode) {
    log_script_ops_fill_image(log_prefix, __func__, log_level_info,
                              id);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;

  image_t* p_image = get_image(id);
  if (!p_image) return;

  image_pattern_data_t* image_data = find_image_pattern(p_ctx, p_image->image_id);
  if (!image_data) return;

  sk_shader_ref(image_data->shader);
  set_fill_shader(p_ctx, image_data->shader);
}

void script_ops_fill_stream(void* v_ctx,
                            sid_t id)
{
  if (g_opts.debug_mode) {
    log_script_ops_fill_stream(log_prefix, __func__, log_level_info,
                               id);
  }

  script_ops_fill_image(v_ctx, id);
}

void script_ops_stroke_width(void* v_ctx,
                             float w)
{
  if (g_opts.debug_mode) {
    log_script_ops_stroke_width(log_prefix, __func__, log_level_info,
                                w);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;

  sk_paint_set_stroke_width(p_ctx->paints.stroke, w);
}

void script_ops_stroke_color(void* v_ctx,
                             color_rgba_t color)
{
  if (g_opts.debug_mode) {
    log_script_ops_stroke_color(log_prefix, __func__, log_level_info,
                                color);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  set_stroke_color(p_ctx, color);
}

void script_ops_stroke_linear(void* v_ctx,
                              coordinates_t start, coordinates_t end,
                              color_rgba_t color_start, color_rgba_t color_end)
{
  if (g_opts.debug_mode) {
    log_script_ops_stroke_linear(log_prefix, __func__, log_level_info,
                                 start, end, color_start, color_end);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_point_t pts[2] = {{start.x, start.y}, {end.x, end.y}};
  sk_color4f_t colors[2] = { skia_color_from_rgba(color_start), skia_color_from_rgba(color_end) };
  float positions[2] = {0.0f, 1.0f};
  sk_shader_t* shader = sk_shader_new_linear_gradient_color4f(pts, colors, NULL, positions, 2, CLAMP_SK_SHADER_TILEMODE, NULL);
  set_stroke_shader(p_ctx, shader);
}

void script_ops_stroke_radial(void* v_ctx,
                              coordinates_t center,
                              float inner_radius,
                              float outer_radius,
                              color_rgba_t color_start,
                              color_rgba_t color_end)
{
  if (g_opts.debug_mode) {
    log_script_ops_stroke_radial(log_prefix, __func__, log_level_info,
                                 center, inner_radius, outer_radius, color_start, color_end);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_point_t c = { center.x, center.y };
  sk_color4f_t colors[2] = { skia_color_from_rgba(color_start), skia_color_from_rgba(color_end) };
  float positions[2] = {0.0f, 1.0f};
  sk_shader_t* shader = sk_shader_new_two_point_conical_gradient_color4f(&c, inner_radius,
                                                                         &c, outer_radius,
                                                                         colors, NULL, positions, 2,
                                                                         CLAMP_SK_SHADER_TILEMODE, NULL);
  set_stroke_shader(p_ctx, shader);
}

void script_ops_stroke_image(void* v_ctx, sid_t id)
{
  if (g_opts.debug_mode) {
    log_script_ops_stroke_image(log_prefix, __func__, log_level_info,
                                id);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;

  image_t* p_image = get_image(id);
  if (!p_image) return;

  image_pattern_data_t* image_data = find_image_pattern(p_ctx, p_image->image_id);
  if (!image_data) return;

  sk_shader_ref(image_data->shader);
  set_stroke_shader(p_ctx, image_data->shader);
}

void script_ops_stroke_stream(void* v_ctx,
                              sid_t id)
{
  if (g_opts.debug_mode) {
    log_script_ops_stroke_stream(log_prefix, __func__, log_level_info,
                                 id);
  }

  script_ops_stroke_image(v_ctx, id);
}

void script_ops_line_cap(void* v_ctx,
                         line_cap_t type)
{
  if (g_opts.debug_mode) {
    log_script_ops_line_cap(log_prefix, __func__, log_level_info,
                            type);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  switch(type) {
  case LINE_CAP_BUTT:
    sk_paint_set_stroke_cap(p_ctx->paints.stroke, BUTT_SK_STROKE_CAP);
    break;
  case LINE_CAP_ROUND:
    sk_paint_set_stroke_cap(p_ctx->paints.stroke, ROUND_SK_STROKE_CAP);
    break;
  case LINE_CAP_SQUARE:
    sk_paint_set_stroke_cap(p_ctx->paints.stroke, SQUARE_SK_STROKE_CAP);
    break;
  }
}

void script_ops_line_join(void* v_ctx,
                          line_join_t type)
{
  if (g_opts.debug_mode) {
    log_script_ops_line_join(log_prefix, __func__, log_level_info,
                             type);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  switch(type) {
  case LINE_JOIN_BEVEL:
    sk_paint_set_stroke_join(p_ctx->paints.stroke, BEVEL_SK_STROKE_JOIN);
    break;
  case LINE_JOIN_ROUND:
    sk_paint_set_stroke_join(p_ctx->paints.stroke, ROUND_SK_STROKE_JOIN);
    break;
  case LINE_JOIN_MITER:
    sk_paint_set_stroke_join(p_ctx->paints.stroke, MITER_SK_STROKE_JOIN);
    break;
  }
}

void script_ops_miter_limit(void* v_ctx,
                            uint32_t limit)
{
  if (g_opts.debug_mode) {
    log_script_ops_miter_limit(log_prefix, __func__, log_level_info,
                               limit);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  sk_paint_set_stroke_miter(p_ctx->paints.stroke, (float)limit);
}

void script_ops_font(void* v_ctx,
                     sid_t id)
{
  if (g_opts.debug_mode) {
    log_script_ops_font(log_prefix, __func__, log_level_info,
                        id);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  font_t* p_font = get_font(id);
  if (!p_font) return;

  font_data_t* font_data = find_font(p_ctx, p_font->font_id);
  if (!font_data) return;

  sk_font_set_typeface(p_ctx->active_font, font_data->typeface);
}

void script_ops_font_size(void* v_ctx,
                          float size)
{
  if (g_opts.debug_mode) {
    log_script_ops_font_size(log_prefix, __func__, log_level_info,
                             size);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  p_ctx->font_size = size;
  sk_font_set_size(p_ctx->active_font, size);
}

void script_ops_text_align(void* v_ctx,
                           text_align_t type)
{
  if (g_opts.debug_mode) {
    log_script_ops_text_align(log_prefix, __func__, log_level_info,
                              type);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  p_ctx->text_align = type;
}

void script_ops_text_base(void* v_ctx,
                          text_base_t type)
{
  if (g_opts.debug_mode) {
    log_script_ops_text_base(log_prefix, __func__, log_level_info,
                             type);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  p_ctx->text_base = type;
}

