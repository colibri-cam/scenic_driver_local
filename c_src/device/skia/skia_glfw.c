#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "comms.h"
#include "device.h"
#include "fontstash.h"
#include "scenic_ops.h"
#include "skia_ctx.h"

extern device_info_t g_device_info;
extern device_opts_t g_opts;

typedef struct {
  GLFWwindow* window;
  GLuint texture;

  int window_width;
  int window_height;
  int framebuffer_width;
  int framebuffer_height;
  float ratio_x;
  float ratio_y;

  float last_x;
  float last_y;
} skia_glfw_t;

static skia_glfw_t g_skia_glfw = {0};

//------------------------------------------------------------------------------
// input callbacks
static void errorcb(int error, const char* desc)
{
  log_error("glfw: %s (%d)", desc, error);
}

static void reshape_framebuffer(GLFWwindow* window, int w, int h)
{
  (void)window;
  g_skia_glfw.framebuffer_width = w;
  g_skia_glfw.framebuffer_height = h;
}

static void reshape_window(GLFWwindow* window, int w, int h)
{
  (void)window;
  g_skia_glfw.window_width = w;
  g_skia_glfw.window_height = h;

  int fw, fh;
  glfwGetFramebufferSize(window, &fw, &fh);
  g_skia_glfw.framebuffer_width = fw;
  g_skia_glfw.framebuffer_height = fh;

  g_skia_glfw.ratio_x = (float)fw / (float)w;
  g_skia_glfw.ratio_y = (float)fh / (float)h;

  g_device_info.width = w;
  g_device_info.height = h;
  g_device_info.ratio = g_skia_glfw.ratio_x;

  send_reshape(w, h);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  (void)window;
  send_key(KEYMAP_GLFW, key, scancode, action, mods);
}

static void charmods_callback(GLFWwindow* window, unsigned int codepoint, int mods)
{
  (void)window;
  send_codepoint(KEYMAP_GLFW, codepoint, mods);
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
  (void)window;
  float x = (float)xpos;
  float y = (float)ypos;
  if ((g_skia_glfw.last_x != x) || (g_skia_glfw.last_y != y)) {
    send_cursor_pos(x, y);
    g_skia_glfw.last_x = x;
    g_skia_glfw.last_y = y;
  }
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
  double x, y;
  glfwGetCursorPos(window, &x, &y);
  send_mouse_button(KEYMAP_GLFW, button, action, mods, x, y);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
  double x, y;
  glfwGetCursorPos(window, &x, &y);
  send_scroll(xoffset, yoffset, x, y);
}

static void cursor_enter_callback(GLFWwindow* window, int entered)
{
  double x, y;
  glfwGetCursorPos(window, &x, &y);
  send_cursor_enter(entered, x, y);
}

static void window_close_callback(GLFWwindow* window)
{
  (void)window;
  send_close(0);
}

//------------------------------------------------------------------------------
static void destroy_glfw()
{
  if (g_skia_glfw.texture) {
    glDeleteTextures(1, &g_skia_glfw.texture);
    g_skia_glfw.texture = 0;
  }

  if (g_skia_glfw.window) {
    glfwDestroyWindow(g_skia_glfw.window);
    g_skia_glfw.window = NULL;
  }

  glfwTerminate();
}

static int init_glfw(const device_opts_t* p_opts)
{
  if (!glfwInit()) {
    log_error("skia-glfw: failed to init glfw");
    return -1;
  }

  glfwSetErrorCallback(errorcb);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  GLFWwindow* window = glfwCreateWindow(p_opts->width,
                                        p_opts->height,
                                        p_opts->title,
                                        NULL,
                                        NULL);
  if (!window) {
    log_error("skia-glfw: failed to create window");
    glfwTerminate();
    return -1;
  }

  g_skia_glfw.window = window;
  g_skia_glfw.last_x = -1.0f;
  g_skia_glfw.last_y = -1.0f;

  glfwMakeContextCurrent(window);
  glewExperimental = GL_TRUE;
  GLenum glew_ok = glewInit();
  if (glew_ok != GLEW_OK) {
    log_error("skia-glfw: glew init failed: %s", glewGetErrorString(glew_ok));
    destroy_glfw();
    return -1;
  }

  glfwSwapInterval(1);

  glfwSetKeyCallback(window, key_callback);
  glfwSetCharModsCallback(window, charmods_callback);
  glfwSetCursorPosCallback(window, cursor_pos_callback);
  glfwSetMouseButtonCallback(window, mouse_button_callback);
  glfwSetScrollCallback(window, scroll_callback);
  glfwSetCursorEnterCallback(window, cursor_enter_callback);
  glfwSetFramebufferSizeCallback(window, reshape_framebuffer);
  glfwSetWindowSizeCallback(window, reshape_window);
  glfwSetWindowCloseCallback(window, window_close_callback);

  glGenTextures(1, &g_skia_glfw.texture);
  glBindTexture(GL_TEXTURE_2D, g_skia_glfw.texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  int w, h, fw, fh;
  glfwGetWindowSize(window, &w, &h);
  glfwGetFramebufferSize(window, &fw, &fh);

  g_skia_glfw.window_width = w;
  g_skia_glfw.window_height = h;
  g_skia_glfw.framebuffer_width = fw;
  g_skia_glfw.framebuffer_height = fh;
  g_skia_glfw.ratio_x = (float)fw / (float)w;
  g_skia_glfw.ratio_y = (float)fh / (float)h;

  return 0;
}

static void blit_surface_to_glfw(scenic_skia_ctx_t* p_ctx)
{
  sk_pixmap_t* pixmap = sk_pixmap_new();
  if (!sk_surface_peek_pixels(p_ctx->surface, pixmap)) {
    log_error("skia-glfw: failed to peek pixels");
    sk_pixmap_destructor(pixmap);
    return;
  }

  sk_imageinfo_t info = {0};
  sk_pixmap_get_info(pixmap, &info);

  const void* pixels = sk_pixmap_get_writable_addr(pixmap);
  glBindTexture(GL_TEXTURE_2D, g_skia_glfw.texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA,
               info.width,
               info.height,
               0,
               GL_BGRA,
               GL_UNSIGNED_BYTE,
               pixels);

  glViewport(0, 0, g_skia_glfw.framebuffer_width, g_skia_glfw.framebuffer_height);
  glClear(GL_COLOR_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, (double)info.width, (double)info.height, 0.0, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glEnable(GL_TEXTURE_2D);
  glBegin(GL_TRIANGLE_STRIP);
  glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
  glTexCoord2f(1.0f, 0.0f); glVertex2f((float)info.width, 0.0f);
  glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, (float)info.height);
  glTexCoord2f(1.0f, 1.0f); glVertex2f((float)info.width, (float)info.height);
  glEnd();
  glDisable(GL_TEXTURE_2D);

  glfwSwapBuffers(g_skia_glfw.window);

  sk_pixmap_destructor(pixmap);
}

//------------------------------------------------------------------------------
int device_init(const device_opts_t* p_opts,
                device_info_t* p_info,
                driver_data_t* p_data)
{
  if (g_opts.debug_mode) {
    log_info("skia %s", __func__);
  }

  scenic_skia_ctx_t* p_ctx = scenic_skia_init(p_opts, p_info);
  if (!p_ctx) {
    log_error("skia-glfw: failed to init skia context");
    return -1;
  }

  if (init_glfw(p_opts) != 0) {
    scenic_skia_fini(p_ctx);
    return -1;
  }

  g_device_info.ratio = g_skia_glfw.ratio_x;
  p_data->v_ctx = p_ctx;

  return 0;
}

int device_close(device_info_t* p_info)
{
  if (g_opts.debug_mode) {
    log_info("skia %s", __func__);
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)p_info->v_ctx;
  scenic_skia_fini(p_ctx);

  destroy_glfw();
  return 0;
}

void device_poll()
{
  glfwPollEvents();
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
  blit_surface_to_glfw(p_ctx);
}

void device_loop(driver_data_t* p_data)
{
  scenic_loop(p_data);
}
