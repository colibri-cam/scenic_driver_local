#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdbool.h>

#include <gr_context.h>

#include "comms.h"
#include "device.h"
#include "fontstash.h"
#include "scenic_ops.h"
#include "skia_ctx.h"

typedef struct {
  GLFWwindow* window;
  GLFWcursor* cursor;

  gr_direct_context_t* gr_context;
  const gr_glinterface_t* gl_interface;
  gr_backendrendertarget_t* render_target;
} skia_glfw_t;

static skia_glfw_t g_skia_glfw = {0};

extern device_info_t g_device_info;

static float g_last_x = -1.0f;
static float g_last_y = -1.0f;

static void destroy_gpu_objects()
{
  if (g_skia_glfw.render_target) {
    gr_backendrendertarget_delete(g_skia_glfw.render_target);
    g_skia_glfw.render_target = NULL;
  }

  if (g_skia_glfw.gr_context) {
    gr_direct_context_flush(g_skia_glfw.gr_context);
    gr_direct_context_release_resources_and_abandon_context(g_skia_glfw.gr_context);
    gr_recording_context_unref((gr_recording_context_t*)g_skia_glfw.gr_context);
    g_skia_glfw.gr_context = NULL;
  }

  if (g_skia_glfw.gl_interface) {
    gr_glinterface_unref(g_skia_glfw.gl_interface);
    g_skia_glfw.gl_interface = NULL;
  }
}

static sk_surface_t* create_skia_surface(int width, int height)
{
  GLint framebuffer = 0;
  GLint samples = 0;
  GLint stencil = 0;

  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
  glGetIntegerv(GL_SAMPLES, &samples);
  glGetIntegerv(GL_STENCIL_BITS, &stencil);

  if (g_skia_glfw.render_target) {
    gr_backendrendertarget_delete(g_skia_glfw.render_target);
    g_skia_glfw.render_target = NULL;
  }

  gr_gl_framebufferinfo_t fb_info = {
    .fFBOID = (unsigned int)framebuffer,
    .fFormat = GL_RGBA8,
    .fProtected = false,
  };

  g_skia_glfw.render_target = gr_backendrendertarget_new_gl(width,
                                                            height,
                                                            samples,
                                                            stencil,
                                                            &fb_info);
  if (!g_skia_glfw.render_target) {
    log_error("skia: failed to create backend render target");
    return NULL;
  }

  sk_surface_t* surface = sk_surface_new_backend_render_target(
    (gr_recording_context_t*)g_skia_glfw.gr_context,
    g_skia_glfw.render_target,
    BOTTOM_LEFT_GR_SURFACE_ORIGIN,
    RGBA_8888_SK_COLORTYPE,
    NULL,
    NULL);

  if (!surface) {
    log_error("skia: failed to create GPU surface");
    return NULL;
  }

  return surface;
}

static bool reshape_surface(GLFWwindow* window)
{
  int win_w = 0;
  int win_h = 0;
  int fb_w = 0;
  int fb_h = 0;

  glfwGetWindowSize(window, &win_w, &win_h);
  glfwGetFramebufferSize(window, &fb_w, &fb_h);

  if (fb_w <= 0 || fb_h <= 0) {
    return false;
  }

  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)g_device_info.v_ctx;
  sk_surface_t* surface = create_skia_surface(fb_w, fb_h);
  if (!surface || !p_ctx) {
    return false;
  }

  scenic_skia_replace_surface(p_ctx, surface);

  glViewport(0, 0, fb_w, fb_h);

  g_device_info.width = fb_w;
  g_device_info.height = fb_h;
  g_device_info.ratio = (win_w > 0) ? ((float)fb_w / (float)win_w) : 1.0f;

  send_reshape(win_w, win_h);

  return true;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  send_key(KEYMAP_GLFW, key, scancode, action, mods);
}

static void charmods_callback(GLFWwindow* window, unsigned int codepoint, int mods)
{
  send_codepoint(KEYMAP_GLFW, codepoint, mods);
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
  float x = xpos;
  float y = ypos;

  if ((g_last_x != x) || (g_last_y != y)) {
    send_cursor_pos(x, y);
    g_last_x = x;
    g_last_y = y;
  }
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
  double x = 0.0;
  double y = 0.0;
  glfwGetCursorPos(window, &x, &y);
  send_mouse_button(KEYMAP_GLFW, button, action, mods, x, y);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
  double x = 0.0;
  double y = 0.0;
  glfwGetCursorPos(window, &x, &y);
  send_scroll(xoffset, yoffset, x, y);
}

static void cursor_enter_callback(GLFWwindow* window, int entered)
{
  double x = 0.0;
  double y = 0.0;
  glfwGetCursorPos(window, &x, &y);
  send_cursor_enter(entered, x, y);
}

static void window_close_callback(GLFWwindow* window)
{
  send_close(0);
  glfwSetWindowShouldClose(window, false);
}

static void framebuffer_size_callback(GLFWwindow* window, int w, int h)
{
  (void)w;
  (void)h;
  reshape_surface(window);
}

static void window_size_callback(GLFWwindow* window, int w, int h)
{
  (void)w;
  (void)h;
  reshape_surface(window);
}

static int setup_window(GLFWwindow* window, const device_opts_t* p_opts)
{
  g_last_x = -1.0f;
  g_last_y = -1.0f;

  glfwMakeContextCurrent(window);

  glewExperimental = GL_TRUE;
  GLenum glew_err = glewInit();
  if (glew_err != GLEW_OK) {
    log_error("skia: glew initialization failed: %s", glewGetErrorString(glew_err));
    return -1;
  }

  glfwSwapInterval(1);

  g_skia_glfw.gl_interface = gr_glinterface_create_native_interface();
  if (!g_skia_glfw.gl_interface || !gr_glinterface_validate(g_skia_glfw.gl_interface)) {
    log_error("skia: failed to create GL interface");
    return -1;
  }

  g_skia_glfw.gr_context = gr_direct_context_make_gl(g_skia_glfw.gl_interface);
  if (!g_skia_glfw.gr_context) {
    log_error("skia: failed to create direct context");
    return -1;
  }

  if (!reshape_surface(window)) {
    return -1;
  }

  g_skia_glfw.cursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
  glfwSetCursor(window, g_skia_glfw.cursor);

  glfwSetKeyCallback(window, key_callback);
  glfwSetCharModsCallback(window, charmods_callback);
  glfwSetCursorPosCallback(window, cursor_pos_callback);
  glfwSetCursorEnterCallback(window, cursor_enter_callback);
  glfwSetMouseButtonCallback(window, mouse_button_callback);
  glfwSetScrollCallback(window, scroll_callback);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetWindowSizeCallback(window, window_size_callback);
  glfwSetWindowCloseCallback(window, window_close_callback);

  sk_color4f_t clear = skia_color_from_rgba(((scenic_skia_ctx_t*)g_device_info.v_ctx)->clear_color);
  glClearColor(clear.fR, clear.fG, clear.fB, clear.fA);

  if (!p_opts->resizable) {
    glfwSetWindowAttrib(window, GLFW_RESIZABLE, GLFW_FALSE);
  }

  return 0;
}

int device_init(const device_opts_t* p_opts,
                device_info_t* p_info,
                driver_data_t* p_data)
{
  if (!glfwInit()) {
    log_error("skia: unable to initialize GLFW");
    return -1;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
  glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

  GLFWwindow* window = glfwCreateWindow(p_opts->width,
                                        p_opts->height,
                                        p_opts->title,
                                        NULL,
                                        NULL);

  if (!window) {
    log_error("skia: unable to create GLFW window");
    glfwTerminate();
    return -1;
  }

  // Stub context to align with expected interface
  p_data->v_ctx = NULL;

  sk_surface_t* surface_placeholder = sk_surface_new_null(p_opts->width, p_opts->height);
  scenic_skia_ctx_t* p_ctx = scenic_skia_init_with_surface(p_opts, p_info, surface_placeholder);
  if (!p_ctx) {
    log_error("skia: failed to initialize Skia state");
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }

  g_device_info.v_ctx = p_ctx;

  if (setup_window(window, p_opts) != 0) {
    log_error("skia: window setup failed");
    scenic_skia_fini(p_ctx);
    destroy_gpu_objects();
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }

  g_skia_glfw.window = window;

  return 0;
}

int device_close(device_info_t* p_info)
{
  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)p_info->v_ctx;
  scenic_skia_fini(p_ctx);

  destroy_gpu_objects();

  if (g_skia_glfw.cursor) {
    glfwDestroyCursor(g_skia_glfw.cursor);
  }

  if (g_skia_glfw.window) {
    glfwDestroyWindow(g_skia_glfw.window);
  }

  glfwTerminate();

  return 0;
}

void device_poll()
{
  glfwPollEvents();
}

void device_begin_render(driver_data_t* p_data)
{
  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)p_data->v_ctx;

  sk_color4f_t clear = skia_color_from_rgba(p_ctx->clear_color);
  sk_canvas_clear_color4f(p_ctx->canvas, clear);
  sk_path_reset(p_ctx->path);
}

void device_end_render(driver_data_t* p_data)
{
  scenic_skia_ctx_t* p_ctx = (scenic_skia_ctx_t*)p_data->v_ctx;

  gr_direct_context_flush_surface(g_skia_glfw.gr_context, p_ctx->surface);
  glfwSwapBuffers(g_skia_glfw.window);
}

void device_loop(driver_data_t* p_data)
{
  scenic_loop(p_data);
}

