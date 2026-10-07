#include <glad/gl.h>
#include <SDL3/SDL.h>
#include <iostream>
#include <SDL3/SDL_main.h>
using namespace std;

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    // init sdl3.
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }
    // request a GL 3.3 core context.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    // the window is the os fb, it does not know anything about gl yet
    SDL_Window* window = SDL_CreateWindow(
        "Grace Cpp", 1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }
    // the context bruh. mainn btw
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // hand over the process addr to gl. main point is that it should HAPPEN after context is loaded..
    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        cerr << "gladLoadGL failed\n";
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // sanity check that we really got 3.3 core and not a compat profile
    cout << "GL_VERSION: " << glGetString(GL_VERSION) << '\n'
         << "GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << '\n'
         << "RENDERER: " << glGetString(GL_RENDERER) << '\n';

    // set the viewport up front, a resizable window's drawable can be smaller
    // than the window until the first real resize event
    int width = 0, height = 0;
    SDL_GetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {  // input stuff
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) running = false;
            // resize pixels on change in window scale
            // which is what the viewport wants (matters on HiDPI/fractional scaling)
            if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                glViewport(0, 0, e.window.data1, e.window.data2);
            }
        }
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(window);  // puts your finished frame on screen
        SDL_Delay(16);              // capping fps to 60 lol
    }
    SDL_GL_DestroyContext(context);  // destroy context
    SDL_DestroyWindow(window);       // and now the window
    SDL_Quit(); // quit sdl after gl ended
    return 0;
}