# Repository-wide instructions

- Before concluding any change that touches this repository, run a compile check for the Skia GLFW backend:
  - `SCENIC_LOCAL_TARGET=skia-glfw mix compile` (set `SKIA_CFLAGS`/`SKIA_LDFLAGS` if they are not already available in the environment).
  - Record the result in your notes/tests, even if dependencies are missing.
