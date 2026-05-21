#include "nwawe/runtime.hpp"

#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

#ifdef NWAWE_USE_SDL3
#include <SDL3/SDL.h>
#endif

namespace {

std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void appendCharacter(std::string& text, char character) {
    if (character >= 32 || character == '\n' || character == '\t') {
        text.push_back(character);
    }
}

std::string joinLines(const std::vector<std::string>& lines) {
    std::string result;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index > 0) {
            result.push_back('\n');
        }
        result += lines[index];
    }
    return result;
}

} // namespace

#ifdef NWAWE_USE_SDL3
void drawMultiline(SDL_Renderer* renderer, int x, int y, const std::string& text) {
    int lineY = y;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        SDL_RenderDebugText(renderer, static_cast<float>(x), static_cast<float>(lineY), line.c_str());
        lineY += 14;
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
}

int main(int argc, char** argv) {
    std::string source;
    if (argc > 1) {
        source = readFile(argv[1]);
    }
    if (source.empty()) {
        source = "print<Hello World>\n";
    }

    std::string output;
    std::string error;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("nwawe", 1280, 840, SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_StartTextInput(window);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_TEXT_INPUT:
                    if (event.text.text[0] != '\0') {
                        for (const char* cursor = event.text.text; *cursor != '\0'; ++cursor) {
                            appendCharacter(source, *cursor);
                        }
                    }
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (event.key.key == SDLK_ESCAPE) {
                        running = false;
                    } else if (event.key.key == SDLK_BACKSPACE && !source.empty()) {
                        source.pop_back();
                    } else if (event.key.key == SDLK_RETURN) {
                        source.push_back('\n');
                    } else if (event.key.key == SDLK_F5) {
                        std::ostringstream capture;
                        try {
                            nwawe::runSource(source, std::cin, capture);
                            output = capture.str();
                            error.clear();
                        } catch (const std::exception& exception) {
                            error = exception.what();
                        }
                    }
                    break;
            }
        }

        int width = 1280;
        int height = 840;
        SDL_GetWindowSizeInPixels(window, &width, &height);

        SDL_SetRenderDrawColor(renderer, 246, 247, 251, 255);
        SDL_RenderClear(renderer);

        SDL_FRect editor = {24.0f, 92.0f, static_cast<float>(width) * 0.58f - 36.0f, static_cast<float>(height) - 116.0f};
        SDL_FRect console = {editor.x + editor.w + 24.0f, 92.0f, static_cast<float>(width) - (editor.x + editor.w + 48.0f), static_cast<float>(height) - 116.0f};

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &editor);
        SDL_RenderFillRect(renderer, &console);
        SDL_SetRenderDrawColor(renderer, 208, 214, 223, 255);
        SDL_RenderRect(renderer, &editor);
        SDL_RenderRect(renderer, &console);

        SDL_RenderDebugText(renderer, 24.0f, 24.0f, "nwawe");
        SDL_RenderDebugText(renderer, 24.0f, 48.0f, "F5 runs the script. Backspace deletes the last character.");
        SDL_RenderDebugText(renderer, editor.x + 12.0f, editor.y + 12.0f, "Editor");
        SDL_RenderDebugText(renderer, console.x + 12.0f, console.y + 12.0f, "Output");

        drawMultiline(renderer, static_cast<int>(editor.x + 12.0f), static_cast<int>(editor.y + 30.0f), source);
        drawMultiline(renderer, static_cast<int>(console.x + 12.0f), static_cast<int>(console.y + 30.0f), output);
        if (!error.empty()) {
            SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
            drawMultiline(renderer, static_cast<int>(console.x + 12.0f), static_cast<int>(console.y + 220.0f), error);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_StopTextInput(window);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
#else
int main() {
    return 1;
}
#endif
