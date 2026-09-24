#include "Renderer.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <memory>

#include "AndroidOut.h"
#include "Game.h"

Renderer::Renderer(android_app *pApp, Game *game) : app_(pApp), game_(game) {
    initRenderer();
}

Renderer::~Renderer() {
    if (display_ != EGL_NO_DISPLAY) {
        // The context is still current on this thread, so GL objects can be released first.
        if (context_ != EGL_NO_CONTEXT) draw_.shutdown();
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}

void Renderer::initRenderer() {
    constexpr EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_NONE
    };

    auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    EGLint numConfigs = 0;
    eglChooseConfig(display, attribs, nullptr, 0, &numConfigs);
    std::unique_ptr<EGLConfig[]> configs(new EGLConfig[numConfigs]);
    eglChooseConfig(display, attribs, configs.get(), numConfigs, &numConfigs);

    // Prefer an exact RGB888 config, otherwise take the first one offered.
    EGLConfig config = numConfigs > 0 ? configs[0] : nullptr;
    for (int i = 0; i < numConfigs; ++i) {
        EGLint r, g, b;
        if (eglGetConfigAttrib(display, configs[i], EGL_RED_SIZE, &r) &&
            eglGetConfigAttrib(display, configs[i], EGL_GREEN_SIZE, &g) &&
            eglGetConfigAttrib(display, configs[i], EGL_BLUE_SIZE, &b) &&
            r == 8 && g == 8 && b == 8) {
            config = configs[i];
            break;
        }
    }

    EGLSurface surface = eglCreateWindowSurface(display, config, app_->window, nullptr);
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttribs);
    eglMakeCurrent(display, surface, surface, context);
    eglSwapInterval(display, 1);

    display_ = display;
    surface_ = surface;
    context_ = context;
    width_ = -1;
    height_ = -1;

    if (!draw_.init()) aout << "Failed to initialise 2D renderer" << std::endl;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::render() {
    EGLint width, height;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height);
    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        glViewport(0, 0, width, height);
    }

    auto now = std::chrono::steady_clock::now();
    float dt = firstFrame_ ? 0.f : std::chrono::duration<float>(now - lastFrame_).count();
    lastFrame_ = now;
    firstFrame_ = false;

    game_->update(dt);
    game_->render(draw_, width_, height_);
    eglSwapBuffers(display_, surface_);
}

void Renderer::handleInput() {
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) return;

    for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto &event = inputBuffer->motionEvents[i];
        auto action = event.action;
        auto pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        auto &pointer = event.pointers[pointerIndex];
        float x = GameActivityPointerAxes_getX(&pointer);
        float y = GameActivityPointerAxes_getY(&pointer);

        switch (action & AMOTION_EVENT_ACTION_MASK) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                game_->touchDown(pointer.id, x, y);
                break;
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                game_->touchUp(pointer.id, x, y);
                break;
            case AMOTION_EVENT_ACTION_CANCEL:
                game_->touchCancel();
                break;
            case AMOTION_EVENT_ACTION_MOVE:
                for (auto p = 0; p < event.pointerCount; p++) {
                    auto &ptr = event.pointers[p];
                    game_->touchMove(ptr.id, GameActivityPointerAxes_getX(&ptr),
                                     GameActivityPointerAxes_getY(&ptr));
                }
                break;
            case AMOTION_EVENT_ACTION_SCROLL:
                game_->scrollZoom(x, y, GameActivityPointerAxes_getAxisValue(
                        &pointer, AMOTION_EVENT_AXIS_VSCROLL));
                break;
            default:
                break;
        }
    }
    android_app_clear_motion_events(inputBuffer);
    android_app_clear_key_events(inputBuffer);
}
