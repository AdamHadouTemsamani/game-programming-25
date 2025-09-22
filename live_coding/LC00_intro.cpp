#include <SDL3/SDL.h>

int main(void)
{
    SDL_Log("hello sdl!\n");

    SDL_Window* window = SDL_CreateWindow("LC00 - intro", 800, 600, 0);

    bool quit = false;

    while(!quit)
    {
        SDL_Event event;
        SDL_Event* event_adrress = &event;

        // NOTE: we may want to limit the amount of event that we process every frame to avoid stutter
        while(SDL_PollEvent(&event))
        {
            SDL_Log("event type: %d\n", event.type);
        }
    }

    return 0;
}