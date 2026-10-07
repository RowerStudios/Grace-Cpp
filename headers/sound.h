#pragma once

// sound assets. the parser carries sound_2d nodes, this resolves them to files,
// checks they really are sound, and plays them through sdl3_mixer.

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <string>
#include <string_view>
#include <vector>

#include "scene_loader.h"

namespace grace {

enum class SoundFormat {
    Unknown,
    Wav,
    Mp3,
    Ogg,
    Flac,
};

// by file extension only
SoundFormat sound_format_from_path(std::string_view path);
const char* to_string(SoundFormat format);

// true when the extension is one we accept
bool is_supported_sound_path(std::string_view path);

// checks the extension, that the file exists, and that the header bytes match
// the format the extension claims. reason is filled in when it fails.
bool validate_sound_file(const std::string& path, std::string* reason);

// resolves a scene sound node to a file on disk. the reference is relative to
// the scene file, so parent."Sounds".bruh in scenes/main.gscn looks for
// scenes/Sounds/bruh. tries the working directory too, and every supported
// extension when the node left it off. returns an empty string when nothing
// matched.
std::string resolve_sound_file(const Node& node, std::string_view scene_path = {});

// checks every sound_2d node under the scene's main node.
// prints anything wrong to stderr and returns how many nodes were bad.
int validate_scene_sounds(const Scene& scene);

// owns the audio device and the mixer, one track per sound_2d node
class SoundPlayer {
public:
    ~SoundPlayer();

    // opens the audio device and mixer. safe to call twice
    bool open(std::string* reason = nullptr);
    void close();
    bool is_open() const { return m_mixer != nullptr; }

    // loads every sound_2d node in the scene. autoplay nodes start right away,
    // the rest sit loaded until played(). returns how many loaded.
    int load_scene(const Scene& scene);

    void play_all();
    void stop_all();

    int playing() const;

private:
    struct Entry {
        MIX_Track* track = nullptr;
        MIX_Audio* audio = nullptr;
        std::string name;
        bool autoplay = false;
    };

    MIX_Mixer* m_mixer = nullptr;
    SDL_AudioDeviceID m_device = 0;
    std::vector<Entry> m_entries;
};

}  // namespace grace