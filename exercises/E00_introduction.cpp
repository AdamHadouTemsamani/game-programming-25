#include <SDL3/SDL.h>
#include <stdbool.h>

typedef struct {
    SDL_FRect rect;
    SDL_Scancode up, down, left, right; // input keys
    SDL_Color color;
} Player;

static inline bool is_colliding(SDL_FRect *a, SDL_FRect *b)
{
	return !( (a->x + a->w) <= b->x ||
              (b->x + b->w) <= a->x ||
              (a->y + a->h) <= b->y ||
              (b->y + b->h) <= a->y );
}

int main(void)
{
    SDL_Log("hello sdl");

    float window_w = 800;
    float window_h = 600;
    int target_framerate_ns = 1000000000 / 60; // ~16.6 ms

    SDL_Window* window = SDL_CreateWindow("E00 - introduction", window_w, window_h, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);

    // zoom
    {
        float zoom = 1.0f;
        window_w /= zoom;
        window_h /= zoom;
        SDL_SetRenderScale(renderer, zoom, zoom);
    }

    bool quit = false;
    SDL_Time walltime_frame_beg;
    SDL_Time walltime_work_end;
    SDL_Time walltime_frame_end = 0;
    SDL_Time time_elapsed_frame = 0;
    SDL_Time time_elapsed_work;

    int delay_type = 0;
    float player_size = 40;
    float player_speed = 300; // pixels/sec

    // initialize players
    Player players[2];

    // Player 1
    players[0].rect = (SDL_FRect){ window_w/2 - 200, window_h/2,
                               player_size, player_size };
    players[0].up    = SDL_SCANCODE_W;
    players[0].down  = SDL_SCANCODE_S;
    players[0].left  = SDL_SCANCODE_A;
    players[0].right = SDL_SCANCODE_D;
    players[0].color = (SDL_Color){0x3C, 0x63, 0xFF, 0xFF};

    // Player 2
    players[1].rect = (SDL_FRect){ window_w/2 + 200, window_h/2,
                               player_size, player_size };
    players[1].up    = SDL_SCANCODE_UP;
    players[1].down  = SDL_SCANCODE_DOWN;
    players[1].left  = SDL_SCANCODE_LEFT;
    players[1].right = SDL_SCANCODE_RIGHT;
    players[1].color = (SDL_Color){0xFF, 0x63, 0x3C, 0xFF};

    // --- DEBUG: previous key state per-player (so we can log transitions only) ---
    bool prev_up[2] = {false, false};
    bool prev_down[2] = {false, false};
    bool prev_left[2] = {false, false};
    bool prev_right[2] = {false, false};
    // ---------------------------------------------------------------------------

    SDL_GetCurrentTime(&walltime_frame_beg);
    while(!quit)
    {
        // input
        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            switch(event.type)
            {
                case SDL_EVENT_QUIT:
                    quit = true;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if(event.key.key >= SDLK_0 && event.key.key < SDLK_5)
                        delay_type = event.key.key - SDLK_0;
                    break;
            }
        }

        // movement
        const uint8_t* state = (const uint8_t*)SDL_GetKeyboardState(NULL);
        float delta_seconds = (float)time_elapsed_frame / 1000000000.0f;

        for (int i = 0; i < 2; i++) {

            float dx = 0, dy = 0;
            if (state[players[i].up])    dy -= 1;
            if (state[players[i].down])  dy += 1;
            if (state[players[i].left])  dx -= 1;
            if (state[players[i].right]) dx += 1;

            if (dx != 0 && dy != 0) { // normalize diagonal
                dx *= 0.7071f;
                dy *= 0.7071f;
            }

            players[i].rect.x += dx * player_speed * delta_seconds;
            players[i].rect.y += dy * player_speed * delta_seconds;
        }

        // clear screen
        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
        SDL_RenderClear(renderer);

        // draw players
        for (int i = 0; i < 2; i++) {
            SDL_SetRenderDrawColor(renderer,
                                   players[i].color.r,
                                   players[i].color.g,
                                   players[i].color.b,
                                   players[i].color.a);
            SDL_RenderFillRect(renderer, &players[i].rect);
        }

        SDL_GetCurrentTime(&walltime_work_end);
        time_elapsed_work = walltime_work_end - walltime_frame_beg;

        // frame pacing
        if(target_framerate_ns > time_elapsed_work)
        {
            switch(delay_type)
            {
                case 0: { // busy wait
                    SDL_Time walltime_busywait = walltime_work_end;
                    while(walltime_busywait - walltime_frame_beg < target_framerate_ns)
                        SDL_GetCurrentTime(&walltime_busywait);
                    break;
                }
                case 1: { // simple delay (ms)
                    SDL_Delay((target_framerate_ns - time_elapsed_work) / 1000000);
                    break;
                }
                case 2: { // delay ns
                    SDL_DelayNS(target_framerate_ns - time_elapsed_work);
                    break;
                }
                case 3: { // precise delay
                    SDL_DelayPrecise(target_framerate_ns - time_elapsed_work);
                    break;
                }
                case 4: { // custom hybrid
                    SDL_DelayNS(target_framerate_ns - time_elapsed_work - 1000000);
                    SDL_Time walltime_busywait = walltime_work_end;
                    while(walltime_busywait - walltime_frame_beg < target_framerate_ns)
                        SDL_GetCurrentTime(&walltime_busywait);
                    break;
                }
            }
        }

        SDL_GetCurrentTime(&walltime_frame_end);
        time_elapsed_frame = walltime_frame_end - walltime_frame_beg;

        // debug text
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_RenderDebugTextFormat(renderer, 10.0f, 10.0f,
            "elapsed (frame): %9.6f ms", (float)time_elapsed_frame/(float)1000000);
        SDL_RenderDebugTextFormat(renderer, 10.0f, 20.0f,
            "elapsed (work) : %9.6f ms", (float)time_elapsed_work/(float)1000000);
        SDL_RenderDebugTextFormat(renderer, 10.0f, 30.0f,
            "delay type: %d (change with 0-4)", delay_type);

        // render
        SDL_RenderPresent(renderer);

        walltime_frame_beg = walltime_frame_end;
    }

    return 0;
}
