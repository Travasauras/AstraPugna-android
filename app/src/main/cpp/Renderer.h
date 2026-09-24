#ifndef ANDROIDGLINVESTIGATIONS_RENDERER_H
#define ANDROIDGLINVESTIGATIONS_RENDERER_H

#include <EGL/egl.h>
#include <chrono>

#include "Draw2D.h"

struct android_app;
class Game;

/*!
 * Owns the EGL context for the current window and drives one Game frame per render() call.
 * The Game itself outlives the Renderer so the match survives the window being recreated.
 */
class Renderer {
public:
    Renderer(android_app *pApp, Game *game);
    ~Renderer();

    // Forwards queued touch events to the game. Clears the input queue.
    void handleInput();

    // Advances the simulation and draws a frame.
    void render();

private:
    void initRenderer();

    android_app *app_;
    Game *game_;
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLint width_ = 0;
    EGLint height_ = 0;
    Draw2D draw_;
    std::chrono::steady_clock::time_point lastFrame_;
    bool firstFrame_ = true;
};

#endif //ANDROIDGLINVESTIGATIONS_RENDERER_H
