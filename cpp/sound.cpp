#include "sound.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
namespace grace {
namespace {
struct FormatEntry {
    const char* extension;
    SoundFormat format;
};

// the formats we accept. add here if a decoder ever shows up for it
constexpr FormatEntry kFormats[] = {
    {".wav", SoundFormat::Wav},
    {".mp3", SoundFormat::Mp3},
    {".ogg", SoundFormat::Ogg},
    {".flac", SoundFormat::Flac},
};

std::string lowercase(std::string_view value) {
    std::string out(value);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool ends_with(const std::string& text, const char* suffix) {
    size_t length = std::strlen(suffix);
    if (text.size() < length) return false;
    return text.compare(text.size() - length, length, suffix) == 0;
}

// the header bytes each format starts with. mp3 has no magic beyond a frame
// sync, so it gets a looser check
bool header_matches(SoundFormat format, const unsigned char* head, size_t length) {
    auto starts = [&](const char* magic, size_t size) {
        return length >= size && std::memcmp(head, magic, size) == 0;
    };

    switch (format) {
        case SoundFormat::Wav:
            return starts("RIFF", 4) && length >= 12 && std::memcmp(head + 8, "WAVE", 4) == 0;
        case SoundFormat::Ogg:
            return starts("OggS", 4);
        case SoundFormat::Flac:
            return starts("fLaC", 4);
        case SoundFormat::Mp3:
            if (starts("ID3", 3)) return true;
            // an mpeg frame starts with 11 set bits
            return length >= 2 && head[0] == 0xFF && (head[1] & 0xE0) == 0xE0;
        case SoundFormat::Unknown:
            return false;
    }
    return false;
}

void collect_sounds(const Node* node, std::vector<const Node*>& out) {
    if (node->is_sound()) out.push_back(node);
    for (const Node* child : node->children) {
        collect_sounds(child, out);
    }
}

}  // namespace

SoundFormat sound_format_from_path(std::string_view path) {
    std::string lower = lowercase(path);
    for (const FormatEntry& entry : kFormats) {
        if (ends_with(lower, entry.extension)) return entry.format;
    }
    return SoundFormat::Unknown;
}

const char* to_string(SoundFormat format) {
    switch (format) {
        case SoundFormat::Wav:  return "wav";
        case SoundFormat::Mp3:  return "mp3";
        case SoundFormat::Ogg:  return "ogg";
        case SoundFormat::Flac: return "flac";
        case SoundFormat::Unknown: return "unknown";
    }
    return "unknown";
}

bool is_supported_sound_path(std::string_view path) {
    return sound_format_from_path(path) != SoundFormat::Unknown;
}

bool validate_sound_file(const std::string& path, std::string* reason) {
    auto fail = [&](const std::string& message) {
        if (reason) *reason = message;
        return false;
    };

    if (path.empty()) return fail("no sound file given");

    SoundFormat format = sound_format_from_path(path);
    if (format == SoundFormat::Unknown) {
        return fail("unsupported sound format, expected wav, mp3, ogg or flac");
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) return fail("cannot open '" + path + "'");

    unsigned char head[16] = {};
    file.read(reinterpret_cast<char*>(head), sizeof(head));
    size_t length = static_cast<size_t>(file.gcount());

    if (length == 0) return fail("'" + path + "' is empty");
    if (!header_matches(format, head, length)) {
        return fail("'" + path + "' is not a valid " + to_string(format) + " file");
    }
    return true;
}

std::string resolve_sound_file(const Node& node, std::string_view scene_path) {
    if (node.sound_src.empty()) return {};

    // the asset reference is relative to the scene that declares it, so
    // scenes/main.gscn asking for parent."Sounds".bruh wants
    // scenes/Sounds/bruh.<ext>
    std::string scene_dir;
    size_t slash = scene_path.find_last_of('/');
    if (slash != std::string_view::npos) scene_dir = std::string(scene_path.substr(0, slash));

    // lowercase variant too, since the folder name is a guess either way
    std::string lowered = lowercase(node.sound_src);
    std::vector<std::string> bases;
    for (const std::string& candidate : {node.sound_src, lowered}) {
        for (const std::string& prefix : {scene_dir, std::string()}) {
            std::string base = prefix.empty() ? candidate : prefix + '/' + candidate;
            if (std::find(bases.begin(), bases.end(), base) == bases.end()) {
                bases.push_back(base);
            }
        }
    }

    // an explicit extension is honoured first, then every format we accept
    std::vector<std::string> suffixes;
    if (is_supported_sound_path(node.sound_src)) suffixes.emplace_back("");
    for (const FormatEntry& entry : kFormats) suffixes.push_back(entry.extension);

    for (const std::string& base : bases) {
        for (const std::string& suffix : suffixes) {
            std::string candidate = base + suffix;
            std::ifstream probe(candidate, std::ios::binary);
            if (probe) return candidate;
        }
    }
    return {};
}

int validate_scene_sounds(const Scene& scene) {
    const Node* entry = scene.main_node();
    if (!entry) return 0;

    std::vector<const Node*> sounds;
    collect_sounds(entry, sounds);

    int bad = 0;
    for (const Node* node : sounds) {
        if (node->sound_src.empty()) {
            std::fprintf(stderr, "sound '%s' has no file\n", node->name.c_str());
            bad++;
            continue;
        }

        std::string resolved = resolve_sound_file(*node, (scene.source_path.empty() ? scene_manager().path_for(scene.name) : scene.source_path));
        if (resolved.empty()) {
            std::fprintf(stderr, "sound '%s': no supported file for '%s'\n", node->name.c_str(),
                         node->sound_src.c_str());
            bad++;
            continue;
        }

        std::string reason;
        if (!validate_sound_file(resolved, &reason)) {
            std::fprintf(stderr, "sound '%s': %s\n", node->name.c_str(), reason.c_str());
            bad++;
            continue;
        }

        std::printf("sound '%s' ok: %s (%s)\n", node->name.c_str(), resolved.c_str(),
                    to_string(sound_format_from_path(resolved)));
    }
    return bad;
}

SoundPlayer::~SoundPlayer() { close(); }

bool SoundPlayer::open(std::string* reason) {
    if (m_mixer) return true;

    // sdl3 audio is independent of the qt window, they coexist fine
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        if (reason) *reason = std::string("SDL_Init(audio): ") + SDL_GetError();
        return false;
    }
    if (!MIX_Init()) {
        if (reason) *reason = std::string("MIX_Init: ") + SDL_GetError();
        return false;
    }

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;
    spec.freq = 48000;

    m_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!m_device) {
        if (reason) *reason = std::string("SDL_OpenAudioDevice: ") + SDL_GetError();
        MIX_Quit();
        return false;
    }

    m_mixer = MIX_CreateMixerDevice(m_device, nullptr);
    if (!m_mixer) {
        if (reason) *reason = std::string("MIX_CreateMixerDevice: ") + SDL_GetError();
        SDL_CloseAudioDevice(m_device);
        m_device = 0;
        MIX_Quit();
        return false;
    }
    return true;
}

void SoundPlayer::close() {
    stop_all();

    for (Entry& entry : m_entries) {
        if (entry.track) MIX_DestroyTrack(entry.track);
        if (entry.audio) MIX_DestroyAudio(entry.audio);
    }
    m_entries.clear();

    if (m_mixer) {
        MIX_DestroyMixer(m_mixer);
        m_mixer = nullptr;
    }
    if (m_device) {
        SDL_CloseAudioDevice(m_device);
        m_device = 0;
    }
    MIX_Quit();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

int SoundPlayer::load_scene(const Scene& scene) {
    if (!m_mixer) return 0;

    // a reload replaces everything the previous scene had playing
    for (Entry& entry : m_entries) {
        if (entry.track) MIX_DestroyTrack(entry.track);
        if (entry.audio) MIX_DestroyAudio(entry.audio);
    }
    m_entries.clear();

    const Node* entry_node = scene.main_node();
    if (!entry_node) return 0;

    std::vector<const Node*> sounds;
    collect_sounds(entry_node, sounds);

    int loaded = 0;
    for (const Node* node : sounds) {
        std::string path = resolve_sound_file(*node, (scene.source_path.empty() ? scene_manager().path_for(scene.name) : scene.source_path));
        if (path.empty()) continue;  // validate_scene_sounds already complained

        MIX_Audio* audio = MIX_LoadAudio(m_mixer, path.c_str(), true);
        if (!audio) {
            std::fprintf(stderr, "sound '%s': cannot load '%s': %s\n", node->name.c_str(),
                         path.c_str(), SDL_GetError());
            continue;
        }

        MIX_Track* track = MIX_CreateTrack(m_mixer);
        if (!track || !MIX_SetTrackAudio(track, audio)) {
            std::fprintf(stderr, "sound '%s': cannot create a track: %s\n", node->name.c_str(),
                         SDL_GetError());
            if (track) MIX_DestroyTrack(track);
            MIX_DestroyAudio(audio);
            continue;
        }

        // looped plays forever, which sdl_mixer spells as -1
        MIX_SetTrackLoops(track, node->looped ? -1 : 0);
        MIX_SetTrackGain(track, node->volume);

        // one ratio drives both pitch and tempo in sdl_mixer, so the two
        // node properties multiply together
        float ratio = node->pitch * node->speed;
        if (ratio > 0.0f) MIX_SetTrackFrequencyRatio(track, ratio);

        Entry entry;
        entry.track = track;
        entry.audio = audio;
        entry.name = node->name;
        entry.autoplay = node->autoplay;
        m_entries.push_back(entry);

        if (node->autoplay) MIX_PlayTrack(track, 0);

        std::printf("sound '%s' loaded: %s\n", node->name.c_str(), path.c_str());
        loaded++;
    }
    return loaded;
}

void SoundPlayer::play_all() {
    for (Entry& entry : m_entries) {
        if (entry.track) MIX_PlayTrack(entry.track, 0);
    }
}

void SoundPlayer::stop_all() {
    if (m_mixer) MIX_StopAllTracks(m_mixer, 0);
}

int SoundPlayer::playing() const {
    int count = 0;
    for (const Entry& entry : m_entries) {
        if (entry.track && MIX_TrackPlaying(entry.track)) count++;
    }
    return count;
}

}  // namespace grace
