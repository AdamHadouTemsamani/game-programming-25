#ifndef ITU_LIB_IMGUI_HPP
#define ITU_LIB_IMGUI_HPP

#ifndef ITU_UNITY_BUILD
#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl3.h>
#include <imgui/imgui_impl_sdlrenderer3.h>
#endif

void itu_lib_imgui_setup(SDL_Window* window, SDLContext* context);

// default imgui event handler. Call this before doing your own processing of the SDL event
// NOTE: a return value of `true` is a suggestion to skip processing of the event for the application
bool itu_lib_imgui_process_sdl_event(SDL_Event* event);
void itu_lib_imgui_frame_begin();
void itu_lib_imgui_frame_end(SDLContext* context);

inline void itu_lib_imgui_setup(SDL_Window* window, SDLContext* context, bool intercept_keyboard)
{
	IMGUI_CHECKVERSION();

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	if(intercept_keyboard)
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

	// defaults to dark style (easier on both eyes and environment)
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL3_InitForSDLRenderer(window, context->renderer);
	ImGui_ImplSDLRenderer3_Init(context->renderer);
}

inline bool itu_lib_imgui_process_sdl_event(SDL_Event* event)
{
	ImGui_ImplSDL3_ProcessEvent(event);

	// NOTE: if we are interacting with an imgui widget, we don't want the game to do weird stuff in the meantime
	//       (ie, tab to navigate controls would reset the game state, not great)
	if(event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP)
	{
		ImGuiIO& io = ImGui::GetIO();
		return io.WantCaptureKeyboard;
	}

	return false;
}

inline void itu_lib_imgui_frame_begin()
{
	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}

inline void itu_lib_imgui_frame_end(SDLContext* context)
{
	ImGuiIO& io = ImGui::GetIO();

	// NOTE: imgui HAS to render at whatever resolution it desires, otherwise input will be messed up
	SDL_SetRenderScale(context->renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), context->renderer);

	// reset rendering scale to what the application wants
	SDL_SetRenderScale(context->renderer, context->zoom, context->zoom);
}
#endif // ITU_LIB_IMGUI_HPP