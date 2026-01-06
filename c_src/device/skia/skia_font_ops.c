#include <sk_data.h>
#include <sk_typeface.h>
#include <string.h>

#include "comms.h"
#include "font_ops.h"
#include "skia_ctx.h"

static int maxi(int a, int b) { return a > b ? a : b; }

static font_data_t* alloc_font_data(scenic_skia_ctx_t* p_ctx, sk_typeface_t* typeface)
{
  font_data_t* font = NULL;

  for (int i = 0; i < p_ctx->fonts_used; i++) {
    if (p_ctx->fonts[i].id == 0) {
      font = &p_ctx->fonts[i];
      break;
    }
  }

  if (!font) {
    if (p_ctx->fonts_used + 1 > p_ctx->fonts_count) {
      font_data_t* fonts;
      int fonts_count = maxi(p_ctx->fonts_count + 1, 4) + p_ctx->fonts_used / 2; // 1.5x over allocation
      fonts = (font_data_t*)realloc(p_ctx->fonts, sizeof(font_data_t) * fonts_count);
      if (!fonts) return NULL;
      p_ctx->fonts = fonts;
      p_ctx->fonts_count = fonts_count;
    }
    font = &p_ctx->fonts[p_ctx->fonts_used++];
  }

  memset(font, 0, sizeof(*font));
  font->id = ++p_ctx->highest_font_id;
  font->typeface = typeface;

  return font;
}

font_data_t* find_font(scenic_skia_ctx_t* p_ctx, int id);

int32_t font_ops_create(void* v_ctx, font_t* p_font, uint32_t size)
{
  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)v_ctx;

  sk_data_t* data = sk_data_new_with_copy(p_font->blob.p_data, size);
  sk_typeface_t* typeface = sk_typeface_new_from_data(data, 0);
  sk_data_unref(data);

  if (!typeface) {
    log_error("skia: failed to create typeface");
    return -1;
  }

  font_data_t* font_data = alloc_font_data(p_ctx, typeface);
  if (!font_data) {
    sk_typeface_unref(typeface);
    return -1;
  }

  return font_data->id;
}
