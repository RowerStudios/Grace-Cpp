#pragma once

// .gscn scene loader
//
//     n1 = new_node("node")
//     n1.properties = {
//         n1.main = true
//         n1.name = "main_node"
//     }
//
//     n2 = new_node("text_2d")
//     n2.properties = {
//         n2.parent = n1
//         n2.pos = (50,50)
//         n2.name = "title"
//         n2.txt = "hi"
//     }
//
// parent is a variable reference, resolved after parsing so a child can be
// declared before its parent. 2d pos is normalized to a 0..100 axis, 50 is
// the centre. x/y are clamped.
//
// a scene file must declare exactly one node with main = true, that node is
// the entry point. change_scene_to("main") loads scenes/main.gscn.

#include <cctype>
#include <deque>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace grace {

struct Vec2 {
    float x = 0.0f, y = 0.0f;
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
};

inline constexpr float kAxisMin = 0.0f;
inline constexpr float kAxisMax = 100.0f;
inline constexpr float kAxisCenter = 50.0f;

inline constexpr const char* kSceneRoot = "scenes";

inline float clamp_axis(float v) {
    if (v < kAxisMin) return kAxisMin;
    if (v > kAxisMax) return kAxisMax;
    return v;
}

enum class NodeType {
    Node,
    Node2D,
    Node3D,
    Text2D,
    Sound2D,
};

inline const char* to_string(NodeType t) {
    switch (t) {
        case NodeType::Node:    return "node";
        case NodeType::Node2D:  return "2d_node";
        case NodeType::Node3D:  return "3d_node";
        case NodeType::Text2D:  return "text_2d";
        case NodeType::Sound2D: return "sound_2d";
    }
    return "node";
}

inline NodeType node_type_from_string(std::string_view s) {
    if (s == "node")     return NodeType::Node;
    if (s == "2d_node")  return NodeType::Node2D;
    if (s == "3d_node")  return NodeType::Node3D;
    if (s == "text_2d")  return NodeType::Text2D;
    if (s == "sound_2d") return NodeType::Sound2D;
    throw std::runtime_error("scene: unknown node type '" + std::string(s) + "'");
}

struct Node {
    NodeType type = NodeType::Node;
    std::string name;
    std::string var;  // the n1 in "n1 = new_node(...)"

    Node* parent = nullptr;
    std::vector<Node*> children;

    bool main = false;
    bool visible = true;

    Vec2 position2d{};
    float rotation2d = 0.0f;
    Vec2 scale2d{1.0f, 1.0f};
    int z_index = 0;

    Vec3 position3d{};
    Vec3 rotation3d{};
    Vec3 scale3d{1.0f, 1.0f, 1.0f};

    std::string text;
    float font_size = 32.0f;
    Vec3 text_color{1.0f, 1.0f, 1.0f};

    // sound_2d. nothing plays these yet, the parser just carries them
    std::string sound_ref;  // as written, e.g. parent."Sounds".bruh.mp3
    std::string sound_src;  // resolved to something openable, sounds/bruh.mp3
    bool autoplay = false;
    bool looped = false;
    float pitch = 1.0f;
    float speed = 1.0f;
    float volume = 1.0f;

    bool is_2d() const {
        return type == NodeType::Node2D || type == NodeType::Text2D;
    }
    bool is_3d() const { return type == NodeType::Node3D; }
    bool is_text() const { return type == NodeType::Text2D; }
    bool is_sound() const { return type == NodeType::Sound2D; }

    void set_position(float x, float y) {
        position2d.x = clamp_axis(x);
        position2d.y = clamp_axis(y);
    }

    Vec2 to_pixels(float viewport_width, float viewport_height) const {
        return {
            position2d.x / kAxisMax * viewport_width,
            position2d.y / kAxisMax * viewport_height,
        };
    }

    static Vec2 from_pixels(float px, float py, float viewport_width, float viewport_height) {
        return {
            clamp_axis(px / viewport_width * kAxisMax),
            clamp_axis(py / viewport_height * kAxisMax),
        };
    }

    Vec2 offset_from_center() const {
        return {position2d.x - kAxisCenter, position2d.y - kAxisCenter};
    }

    Node* find_child(std::string_view child_name) const {
        for (Node* c : children) {
            if (c->name == child_name) return c;
        }
        return nullptr;
    }
};

class Scene {
public:
    // deque, not vector: vector growth moves nodes and invalidates every Node*
    std::deque<Node> nodes;
    std::string name;
    std::string source_path;  // the file this came from, for asset lookups

    Node* new_node(std::string_view type_name, std::string name = {}) {
        NodeType type = node_type_from_string(type_name);
        if (name.empty()) {
            name = std::string(to_string(type)) + "_" + std::to_string(nodes.size());
        }
        if (find(name)) {
            throw std::runtime_error("scene: duplicate node name '" + name + "'");
        }
        nodes.emplace_back();
        Node& n = nodes.back();
        n.type = type;
        n.name = std::move(name);
        return &n;
    }

    Node* find(std::string_view name) const {
        for (const Node& n : nodes) {
            if (n.name == name) return const_cast<Node*>(&n);
        }
        return nullptr;
    }

    Node* find_var(std::string_view var) const {
        for (const Node& n : nodes) {
            if (n.var == var) return const_cast<Node*>(&n);
        }
        return nullptr;
    }

    Node* main_node() const {
        for (const Node& n : nodes) {
            if (n.main) return const_cast<Node*>(&n);
        }
        return nodes.empty() ? nullptr : const_cast<Node*>(&nodes.front());
    }

    size_t main_count() const {
        size_t count = 0;
        for (const Node& n : nodes) {
            if (n.main) count++;
        }
        return count;
    }

    // a scene must declare exactly one node with main = true
    Node* require_main_node() const {
        size_t count = main_count();
        if (count == 0) {
            throw std::runtime_error("scene '" + name +
                                     "': no node has main = true");
        }
        if (count > 1) {
            throw std::runtime_error("scene '" + name + "': " + std::to_string(count) +
                                     " nodes have main = true, expected 1");
        }
        return main_node();
    }

    void clear() { nodes.clear(); }

    void load_file(const std::string& path) {
        std::ifstream file(path);
        if (!file) {
            throw std::runtime_error("scene: cannot open '" + path + "'");
        }
        source_path = path;
        std::stringstream buffer;
        buffer << file.rdbuf();
        load_from_string(buffer.str(), path);
    }

    void load_from_string(const std::string& text, std::string_view source = "<memory>") {
        clear();

        std::vector<std::pair<Node*, std::string>> pending_links;

        std::istringstream stream(text);
        std::string line;
        int line_number = 0;
        std::string block_var;

        while (std::getline(stream, line)) {
            ++line_number;
            try {
                std::string s = strip_comment(line);
                if (s.empty()) continue;

                if (s == "}") {
                    block_var.clear();
                    continue;
                }
                if (ends_with_block_open(s)) {
                    block_var = trim(s.substr(0, s.size() - 1));
                    continue;
                }
                if (is_new_node(s)) {
                    declare(s, block_var);
                    continue;
                }

                size_t eq = s.find('=');
                if (eq == std::string::npos) {
                    throw std::runtime_error("expected an assignment");
                }

                std::string target = trim(s.substr(0, eq));
                std::string value = trim(s.substr(eq + 1));

                Node* node = nullptr;
                size_t dot = target.find('.');
                if (dot != std::string::npos) {
                    std::string var = trim(target.substr(0, dot));
                    std::string key = trim(target.substr(dot + 1));
                    node = find_var(var);
                    if (!node) node = resolve_block(block_var, var);
                    if (!node) {
                        throw std::runtime_error("unknown variable '" + var + "'");
                    }
                    apply_property(node, key, value, pending_links);
                } else if (!block_var.empty()) {
                    node = resolve_block(block_var, target);
                    if (!node) {
                        throw std::runtime_error("unknown variable '" + block_var + "'");
                    }
                    apply_property(node, target, value, pending_links);
                } else {
                    throw std::runtime_error("property outside a block needs 'var.key = value'");
                }
            } catch (const std::exception& e) {
                throw std::runtime_error(std::string(source) + ":" +
                                         std::to_string(line_number) + ": " + e.what());
            }
        }

        for (auto& [child, parent_var] : pending_links) {
            Node* parent = find_var(parent_var);
            if (!parent) {
                throw std::runtime_error("scene: '" + child->var + "' references unknown parent '" +
                                         parent_var + "'");
            }
            if (parent == child) {
                throw std::runtime_error("scene: '" + child->var + "' is its own parent");
            }
            parent->children.push_back(child);
            child->parent = parent;
        }
    }

    void save_file(const std::string& path) const {
        std::ofstream file(path);
        if (!file) {
            throw std::runtime_error("scene: cannot write '" + path + "'");
        }
        file << "# .gscn scene\n";
        for (size_t i = 0; i < nodes.size(); i++) {
            const Node& n = nodes[i];
            std::string var = n.var.empty() ? ("n" + std::to_string(i + 1)) : n.var;

            file << var << " = new_node(\"" << to_string(n.type) << "\")\n";
            file << var << ".properties = {\n";
            if (n.main) file << "    " << var << ".main = true\n";
            if (!n.visible) file << "    " << var << ".visible = false\n";
            file << "    " << var << ".name = \"" << n.name << "\"\n";
            if (n.parent) file << "    " << var << ".parent = " << n.parent->var << "\n";

            if (n.is_2d()) {
                file << "    " << var << ".pos = (" << n.position2d.x << "," << n.position2d.y
                     << ")\n";
                if (n.rotation2d != 0.0f) {
                    file << "    " << var << ".rotation = " << n.rotation2d << "\n";
                }
                if (n.scale2d.x != 1.0f || n.scale2d.y != 1.0f) {
                    file << "    " << var << ".scale = (" << n.scale2d.x << "," << n.scale2d.y
                         << ")\n";
                }
                if (n.z_index != 0) {
                    file << "    " << var << ".z_index = " << n.z_index << "\n";
                }
            }
            if (n.is_3d()) {
                file << "    " << var << ".position = (" << n.position3d.x << "," << n.position3d.y
                     << "," << n.position3d.z << ")\n";
                file << "    " << var << ".scale = (" << n.scale3d.x << "," << n.scale3d.y << ","
                     << n.scale3d.z << ")\n";
            }
            if (n.is_text()) {
                file << "    " << var << ".txt = \"" << n.text << "\"\n";
                if (n.font_size != 32.0f) {
                    file << "    " << var << ".font_size = " << n.font_size << "\n";
                }
            }
            if (n.is_sound()) {
                if (!n.sound_ref.empty()) {
                    file << "    " << var << ".sound = " << n.sound_ref << "\n";
                } else if (!n.sound_src.empty()) {
                    file << "    " << var << ".sound = \"" << n.sound_src << "\"\n";
                }
                if (n.autoplay) file << "    " << var << ".autoplay = true\n";
                if (n.looped) file << "    " << var << ".looped = true\n";
                if (n.pitch != 1.0f) file << "    " << var << ".pitch = " << n.pitch << "\n";
                if (n.speed != 1.0f) file << "    " << var << ".speed = " << n.speed << "\n";
                if (n.volume != 1.0f) file << "    " << var << ".volume = " << n.volume << "\n";
            }
            file << "}\n\n";
        }
    }

private:
    static std::string trim(std::string s) {
        size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return {};
        size_t last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, last - first + 1);
    }

    static std::string strip_comment(std::string_view line) {
        std::string out;
        bool in_quotes = false;
        for (char c : line) {
            if (c == '"') in_quotes = !in_quotes;
            if (c == '#' && !in_quotes) break;
            out += c;
        }
        return trim(out);
    }

    static bool is_new_node(const std::string& s) {
        size_t paren = s.find("new_node(");
        return s.find('=') != std::string::npos && paren != std::string::npos &&
               s.find('=') < paren;
    }

    static bool ends_with_block_open(const std::string& s) {
        return !s.empty() && s.back() == '{';
    }

    Node* resolve_block(const std::string& block_var, const std::string& var) {
        std::string v = var.empty() ? block_var : var;
        for (Node& n : nodes) {
            if (n.var == v) return &n;
        }
        return nullptr;
    }

    // "n1 = new_node("node")"
    void declare(const std::string& s, const std::string& block_var) {
        std::string var = trim(s.substr(0, s.find('=')));
        size_t open = s.find('(');
        size_t close = s.rfind(')');
        if (open == std::string::npos || close == std::string::npos || close < open) {
            throw std::runtime_error("malformed new_node call");
        }
        std::string type = trim(s.substr(open + 1, close - open - 1));
        if (type.size() >= 2 && type.front() == '"' && type.back() == '"') {
            type = type.substr(1, type.size() - 2);
        }

        Node* node = new_node(type);
        node->var = var;
        if (var.empty()) {
            node->var = block_var;
        }
    }

    static std::string unquote(std::string value) {
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            return value.substr(1, value.size() - 2);
        }
        return value;
    }

    // turns a roblox style asset reference into a path we can open.
    //   parent."Sounds".bruh      ->  sounds/bruh
    //   parent."Sounds".bruh.mp3  ->  sounds/bruh.mp3
    //   parent.bruh               ->  bruh
    //   sounds/bruh.mp3           ->  sounds/bruh.mp3  (already a path)
    // shape is root.folder.name[.ext], one folder level, the case is kept because
    // asset paths are case sensitive on linux
    static std::string parse_asset_path(const std::string& value) {
        std::string raw = unquote(value);
        if (raw.find('/') != std::string::npos || raw.find('.') == std::string::npos) {
            return raw;
        }

        std::vector<std::string> parts;
        std::string current;
        bool in_quotes = false;
        for (char c : raw) {
            if (c == '"') {
                in_quotes = !in_quotes;
                continue;
            }
            if (c == '.' && !in_quotes) {
                parts.push_back(current);
                current.clear();
                continue;
            }
            current += c;
        }
        parts.push_back(current);

        // drop the asset root if one was named
        if (!parts.empty() && (parts[0] == "parent" || parts[0] == "root")) {
            parts.erase(parts.begin());
        }
        if (parts.empty()) return raw;

        // one folder segment, everything after it is the filename
        if (parts.size() == 1) return parts[0];

        std::string folder = parts[0];

        std::string filename;
        for (size_t i = 1; i < parts.size(); i++) {
            if (i > 1) filename += '.';
            filename += parts[i];
        }

        return folder + '/' + filename;
    }

    static float to_float(const std::string& value) {
        try {
            return std::stof(value);
        } catch (const std::exception&) {
            throw std::runtime_error("'" + value + "' is not a number");
        }
    }

    static Vec3 parse_tuple(const std::string& value, size_t expected) {
        std::string inner = trim(value);
        if (inner.size() < 2 || inner.front() != '(' || inner.back() != ')') {
            throw std::runtime_error("expected a tuple like (1,2,3), got '" + value + "'");
        }
        inner = inner.substr(1, inner.size() - 2);

        std::vector<std::string> parts;
        std::stringstream ss(inner);
        std::string part;
        while (std::getline(ss, part, ',')) {
            parts.push_back(trim(part));
        }
        if (parts.size() != expected) {
            throw std::runtime_error("expected " + std::to_string(expected) +
                                     " numbers in '" + value + "'");
        }
        if (expected == 2) {
            return {to_float(parts[0]), to_float(parts[1]), 0.0f};
        }
        return {to_float(parts[0]), to_float(parts[1]), to_float(parts[2])};
    }

    static void apply_property(Node* n, const std::string& key, const std::string& raw,
                               std::vector<std::pair<Node*, std::string>>& pending_links) {
        std::string value = trim(raw);

        if (key == "main")         n->main = (value == "true");
        else if (key == "visible") n->visible = (value == "true");
        else if (key == "name")    n->name = unquote(value);
        else if (key == "parent")  pending_links.emplace_back(n, value);
        else if (key == "pos") {
            Vec3 v = parse_tuple(value, 2);
            n->position2d = {clamp_axis(v.x), clamp_axis(v.y)};
        }
        else if (key == "x")       n->position2d.x = clamp_axis(to_float(value));
        else if (key == "y")       n->position2d.y = clamp_axis(to_float(value));
        else if (key == "rotation") n->rotation2d = to_float(value);
        else if (key == "scale") {
            if (n->is_3d()) n->scale3d = parse_tuple(value, 3);
            else {
                Vec3 v = parse_tuple(value, 2);
                n->scale2d = {v.x, v.y};
            }
        }
        else if (key == "z_index") n->z_index = std::stoi(value);
        else if (key == "position")  n->position3d = parse_tuple(value, 3);
        else if (key == "rotation_3d") n->rotation3d = parse_tuple(value, 3);
        else if (key == "txt")     n->text = unquote(value);
        else if (key == "text")    n->text = unquote(value);
        else if (key == "font_size") n->font_size = to_float(value);
        else if (key == "text_color") n->text_color = parse_tuple(value, 3);
        else if (key == "sound" || key == "src" || key == "sound_src") {
            n->sound_ref = value;
            n->sound_src = parse_asset_path(value);
        }
        else if (key == "autoplay") n->autoplay = (value == "true");
        else if (key == "looped" || key == "loop") n->looped = (value == "true");
        else if (key == "pitch") n->pitch = to_float(value);
        else if (key == "speed") n->speed = to_float(value);
        else if (key == "volume") n->volume = to_float(value);
        else throw std::runtime_error("unknown property '" + key + "'");
    }
};

class SceneManager {
public:
    explicit SceneManager(std::string root = kSceneRoot) : root_(std::move(root)) {}

    // loads scenes/<name>.gscn and makes it the active scene. the file must
    // declare exactly one node with main = true, otherwise nothing changes
    void change_scene_to(std::string_view name) {
        Scene loaded;
        loaded.name = std::string(name);
        loaded.load_file(path_for(name));
        loaded.require_main_node();
        active_ = std::move(loaded);
    }

    Scene& current() { return active_; }
    Node* main() { return active_.require_main_node(); }
    bool has_scene() const { return !active_.nodes.empty(); }

    std::string path_for(std::string_view name) const {
        return root_ + "/" + std::string(name) + ".gscn";
    }

private:
    std::string root_;
    Scene active_;
};

inline SceneManager& scene_manager() {
    static SceneManager manager;
    return manager;
}

inline void change_scene_to(std::string_view name) { scene_manager().change_scene_to(name); }
inline Scene& current_scene() { return scene_manager().current(); }
inline Node* current_main() { return scene_manager().main(); }

}  // namespace grace