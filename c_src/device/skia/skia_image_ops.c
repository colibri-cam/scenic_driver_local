#include <sk_image.h>
#include <stdlib.h>
#include <string.h>

#include "comms.h"
#include "image_ops.h"
#include "skia_ctx.h"

static int maxi(int a, int b) { return a > b ? a : b; }

static image_pattern_data_t* alloc_image_pattern(scenic_skia_ctx_t* p_ctx)
{
  image_pattern_data_t* ipd = NULL;

  for (int i = 0; i < p_ctx->images_used; i++) {
    if (p_ctx->images[i].id == 0) {
      ipd = &p_ctx->images[i];
      break;
    }
  }

  if (ipd == NULL) {
    if (p_ctx->images_used + 1 > p_ctx->images_count) {
      image_pattern_data_t* images;
      int images_count = maxi(p_ctx->images_count + 1, 4) + p_ctx->images_used / 2; // 1.5x over allocation
      images = (image_pattern_data_t*)realloc(p_ctx->images, sizeof(image_pattern_data_t) * images_count);
      if (images == NULL) return NULL;
      p_ctx->images = images;
      p_ctx->images_count = images_count;
    }
    ipd = &p_ctx->images[p_ctx->images_used++];
  }

  memset(ipd, 0, sizeof(*ipd));
  ipd->id = ++p_ctx->highest_image_id;

  return ipd;
}

image_pattern_data_t* find_image_pattern(scenic_skia_ctx_t* p_ctx, int id);

static void set_image_shader(image_pattern_data_t* image_data)
{
  if (image_data->shader) {
    sk_shader_unref(image_data->shader);
  }

  image_data->shader = sk_image_make_shader(image_data->image,
                                            REPEAT_SK_SHADER_TILEMODE,
                                            REPEAT_SK_SHADER_TILEMODE,
                                            NULL,
                                            NULL);
}

int32_t image_ops_create(void* v_ctx,
                         uint32_t width, uint32_t height,
                         void* p_pixels)
{
  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;

  sk_imageinfo_t info = {
    .colorspace = NULL,
    .width = (int32_t)width,
    .height = (int32_t)height,
    .colorType = RGBA_8888_SK_COLORTYPE,
    .alphaType = PREMUL_SK_ALPHATYPE,
  };

  size_t row_bytes = width * 4;
  sk_image_t* image = sk_image_new_raster_copy(&info, p_pixels, row_bytes);
  if (!image) return 0;

  image_pattern_data_t* image_data = alloc_image_pattern(p_ctx);
  if (!image_data) {
    sk_image_unref(image);
    return 0;
  }

  image_data->image = image;
  set_image_shader(image_data);

  return image_data->id;
}

void image_ops_update(void* v_ctx, int32_t image_id, void* p_pixels)
{
  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  image_pattern_data_t* image_data = find_image_pattern(p_ctx, image_id);
  if (!image_data) return;

  int width = sk_image_get_width(image_data->image);
  int height = sk_image_get_height(image_data->image);

  sk_imageinfo_t info = {
    .colorspace = NULL,
    .width = width,
    .height = height,
    .colorType = RGBA_8888_SK_COLORTYPE,
    .alphaType = PREMUL_SK_ALPHATYPE,
  };

  sk_image_t* image = sk_image_new_raster_copy(&info, p_pixels, width * 4);
  if (!image) return;

  sk_image_unref(image_data->image);
  image_data->image = image;
  set_image_shader(image_data);
}

void image_ops_delete(void* v_ctx, int32_t image_id)
{
  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;
  image_pattern_data_t* image_data = find_image_pattern(p_ctx, image_id);
  if (!image_data) return;

  if (image_data->image) sk_image_unref(image_data->image);
  if (image_data->shader) sk_shader_unref(image_data->shader);
  memset(image_data, 0, sizeof(*image_data));
}
