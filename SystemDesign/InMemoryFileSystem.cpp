/*
 * In-Memory File System - Interview-Style Low-Level Design (LLD)
 *
 * Question:
 * Design and implement an In-Memory File System (similar to Unix VFS / LeetCode 588)
 * that supports:
 *   1. Hierarchical tree structure of Directories and Files using the Composite Pattern.
 *   2. Path operations with absolute path parsing (supporting ".", "..", and "//"):
 *      - mkdir(path): Create directories recursively (mkdir -p semantics).
 *      - writeFile(path, content) / appendToFile(path, content): Create or update files.
 *      - readFile(path): Read file contents.
 *      - ls(path): List sorted directory entries, or return the filename if path is a file.
 *      - remove(path): Delete a file or recursively delete a directory.
 *      - move(src, dst): Move/rename files or directories (with cycle prevention).
 *      - find(startPath, query): Recursively search matching paths in a subtree.
 *      - getSize(path): Compute file size or recursive directory size polymorphically.
 */

#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// -------------------------------------------------------------
// Composite Component: FileSystemNode
// -------------------------------------------------------------
class FileSystemNode {
public:
    explicit FileSystemNode(string name) : name_(move(name)) {}
    virtual ~FileSystemNode() = default;

    const string& getName() const { return name_; }
    void setName(string newName) { name_ = move(newName); }

    virtual bool isDirectory() const = 0;
    virtual size_t getSize() const = 0;

private:
    string name_;
};

// -------------------------------------------------------------
// Leaf Node: FileNode
// -------------------------------------------------------------
class FileNode : public FileSystemNode {
public:
    explicit FileNode(string name, string content = "")
        : FileSystemNode(move(name)), content_(move(content)) {}

    bool isDirectory() const override { return false; }

    size_t getSize() const override { return content_.size(); }

    const string& getContent() const { return content_; }

    void setContent(string content) { content_ = move(content); }

    void appendContent(const string& extra) { content_ += extra; }

private:
    string content_;
};

// -------------------------------------------------------------
// Composite Node: DirectoryNode
// -------------------------------------------------------------
class DirectoryNode : public FileSystemNode {
public:
    explicit DirectoryNode(string name) : FileSystemNode(move(name)) {}

    bool isDirectory() const override { return true; }

    size_t getSize() const override {
        size_t total = 0;
        for (const auto& [childName, childNode] : children_) {
            total += childNode->getSize();
        }
        return total;
    }

    shared_ptr<FileSystemNode> getChild(const string& name) const {
        auto it = children_.find(name);
        return (it != children_.end()) ? it->second : nullptr;
    }

    bool hasChild(const string& name) const {
        return children_.find(name) != children_.end();
    }

    void setChild(const string& name, shared_ptr<FileSystemNode> node) {
        children_[name] = move(node);
    }

    bool removeChild(const string& name) {
        return children_.erase(name) > 0;
    }

    const map<string, shared_ptr<FileSystemNode>>& getChildren() const {
        return children_;
    }

private:
    // Ordered map guarantees lexicographically sorted directory listings
    map<string, shared_ptr<FileSystemNode>> children_;
};

// -------------------------------------------------------------
// In-Memory File System Service
// -------------------------------------------------------------
class FileSystem {
public:
    FileSystem() : root_(make_shared<DirectoryNode>("/")) {}

    // Creates directory (and missing parent directories, like `mkdir -p`).
    bool mkdir(const string& path) {
        vector<string> parts;
        if (!parseAbsolutePath(path, parts)) {
            cout << "[mkdir] Invalid path: '" << path << "'. Must be a valid absolute path.\n";
            return false;
        }
        if (parts.empty()) {
            return true; // Root always exists
        }

        auto targetDir = traverseDirectories(parts, /*createMissing=*/true);
        if (!targetDir) {
            cout << "[mkdir] Failed for '" << path << "': a file exists along the path.\n";
            return false;
        }
        return true;
    }

    // Overwrites or creates a file at filePath.
    bool writeFile(const string& filePath, const string& content, bool createParents = false) {
        return upsertFile(filePath, content, /*append=*/false, createParents);
    }

    // Appends content to an existing file or creates a new file at filePath.
    bool appendToFile(const string& filePath, const string& content, bool createParents = false) {
        return upsertFile(filePath, content, /*append=*/true, createParents);
    }

    // Reads content of a file. Returns nullopt if path does not exist or is a directory.
    optional<string> readFile(const string& filePath) const {
        auto node = resolveNode(filePath);
        if (!node) {
            cout << "[readFile] Path not found: '" << filePath << "'.\n";
            return nullopt;
        }
        if (node->isDirectory()) {
            cout << "[readFile] Cannot read directory as file: '" << filePath << "'.\n";
            return nullopt;
        }
        return static_pointer_cast<FileNode>(node)->getContent();
    }

    // Lists sorted child names if path is a directory, or {filename} if path is a file.
    vector<string> ls(const string& path) const {
        auto node = resolveNode(path);
        if (!node) {
            cout << "[ls] Path not found: '" << path << "'.\n";
            return {};
        }

        if (!node->isDirectory()) {
            return {node->getName()};
        }

        vector<string> entries;
        auto dir = static_pointer_cast<DirectoryNode>(node);
        entries.reserve(dir->getChildren().size());
        for (const auto& [name, child] : dir->getChildren()) {
            entries.push_back(name);
        }
        return entries;
    }

    // Deletes a file or recursively deletes a directory (root "/" cannot be deleted).
    bool remove(const string& path) {
        string targetName;
        auto parent = resolveParentDirectory(path, targetName, /*createMissing=*/false);
        if (!parent || targetName.empty()) {
            cout << "[remove] Cannot remove '" << path << "'.\n";
            return false;
        }

        if (!parent->removeChild(targetName)) {
            cout << "[remove] Path not found: '" << path << "'.\n";
            return false;
        }
        return true;
    }

    // Moves or renames a file/directory from srcPath to dstPath.
    bool move(const string& srcPath, const string& dstPath) {
        string srcName;
        auto srcParent = resolveParentDirectory(srcPath, srcName, /*createMissing=*/false);
        if (!srcParent || srcName.empty() || !srcParent->hasChild(srcName)) {
            cout << "[move] Source not found or invalid: '" << srcPath << "'.\n";
            return false;
        }

        auto srcNode = srcParent->getChild(srcName);

        // If dstPath resolves to an existing directory, move inside it preserving srcName
        auto existingDst = resolveNode(dstPath);
        shared_ptr<DirectoryNode> dstParent;
        string dstName;

        if (existingDst && existingDst->isDirectory()) {
            dstParent = static_pointer_cast<DirectoryNode>(existingDst);
            dstName = srcName;
        } else {
            dstParent = resolveParentDirectory(dstPath, dstName, /*createMissing=*/false);
            if (!dstParent || dstName.empty()) {
                cout << "[move] Invalid destination path: '" << dstPath << "'.\n";
                return false;
            }
        }

        // Prevent moving a directory into itself or any of its descendants
        if (srcNode->isDirectory() && isSubtreeOrSame(srcNode, dstParent)) {
            cout << "[move] Cannot move directory '" << srcPath << "' into itself or a subdirectory.\n";
            return false;
        }

        if (dstParent->hasChild(dstName)) {
            cout << "[move] Destination already exists: '" << dstPath << "'.\n";
            return false;
        }

        srcParent->removeChild(srcName);
        srcNode->setName(dstName);
        dstParent->setChild(dstName, srcNode);
        return true;
    }

    // Recursively finds all nodes whose names contain `query` under startPath.
    vector<string> find(const string& startPath, const string& query) const {
        vector<string> parts;
        if (!parseAbsolutePath(startPath, parts)) {
            cout << "[find] Invalid start path: '" << startPath << "'.\n";
            return {};
        }

        auto startNode = resolveNodeFromParts(parts);
        if (!startNode) {
            cout << "[find] Start path not found: '" << startPath << "'.\n";
            return {};
        }

        string normalizedStart = "/";
        for (size_t i = 0; i < parts.size(); ++i) {
            normalizedStart += parts[i];
            if (i + 1 < parts.size()) normalizedStart += "/";
        }

        vector<string> matches;
        collectMatches(startNode, normalizedStart, query, matches);
        return matches;
    }

    // Returns total byte size of a file or recursive sum of a directory subtree.
    optional<size_t> getSize(const string& path) const {
        auto node = resolveNode(path);
        if (!node) {
            cout << "[getSize] Path not found: '" << path << "'.\n";
            return nullopt;
        }
        return node->getSize();
    }

    // Displays an indented visual tree starting from `path`.
    void printTree(const string& path = "/") const {
        auto node = resolveNode(path);
        if (!node) {
            cout << "[printTree] Path not found: '" << path << "'.\n";
            return;
        }
        cout << "\n--- File System Tree (" << path << ") ---\n";
        printSubtree(node, 0);
        cout << "-----------------------------------------\n";
    }

private:
    bool parseAbsolutePath(const string& path, vector<string>& parts) const {
        if (path.empty() || path[0] != '/') {
            return false;
        }

        parts.clear();
        stringstream ss(path);
        string token;
        while (getline(ss, token, '/')) {
            if (token.empty() || token == ".") {
                continue;
            }
            if (token == "..") {
                if (parts.empty()) {
                    return false; // Cannot traverse above root "/"
                }
                parts.pop_back();
            } else {
                parts.push_back(token);
            }
        }
        return true;
    }

    shared_ptr<DirectoryNode> traverseDirectories(const vector<string>& dirParts, bool createMissing) const {
        auto curr = root_;
        for (const string& segment : dirParts) {
            auto child = curr->getChild(segment);
            if (!child) {
                if (!createMissing) {
                    return nullptr;
                }
                auto newDir = make_shared<DirectoryNode>(segment);
                curr->setChild(segment, newDir);
                curr = newDir;
            } else {
                if (!child->isDirectory()) {
                    return nullptr; // Path component is a file, not a directory
                }
                curr = static_pointer_cast<DirectoryNode>(child);
            }
        }
        return curr;
    }

    shared_ptr<FileSystemNode> resolveNodeFromParts(const vector<string>& parts) const {
        if (parts.empty()) {
            return root_;
        }
        shared_ptr<FileSystemNode> curr = root_;
        for (const string& segment : parts) {
            if (!curr->isDirectory()) {
                return nullptr;
            }
            curr = static_pointer_cast<DirectoryNode>(curr)->getChild(segment);
            if (!curr) {
                return nullptr;
            }
        }
        return curr;
    }

    shared_ptr<FileSystemNode> resolveNode(const string& path) const {
        vector<string> parts;
        if (!parseAbsolutePath(path, parts)) {
            return nullptr;
        }
        return resolveNodeFromParts(parts);
    }

    shared_ptr<DirectoryNode> resolveParentDirectory(
        const string& path, string& targetName, bool createMissing) const {
        vector<string> parts;
        if (!parseAbsolutePath(path, parts) || parts.empty()) {
            targetName.clear();
            return nullptr;
        }
        targetName = parts.back();
        parts.pop_back();
        return traverseDirectories(parts, createMissing);
    }

    bool upsertFile(const string& filePath, const string& content, bool append, bool createParents) {
        string fileName;
        auto parent = resolveParentDirectory(filePath, fileName, createParents);
        if (!parent || fileName.empty()) {
            cout << "[writeFile] Invalid or missing parent path for '" << filePath << "'.\n";
            return false;
        }

        auto existing = parent->getChild(fileName);
        if (existing) {
            if (existing->isDirectory()) {
                cout << "[writeFile] Cannot write: '" << filePath << "' is a directory.\n";
                return false;
            }
            auto file = static_pointer_cast<FileNode>(existing);
            if (append) {
                file->appendContent(content);
            } else {
                file->setContent(content);
            }
            return true;
        }

        parent->setChild(fileName, make_shared<FileNode>(fileName, content));
        return true;
    }

    bool isSubtreeOrSame(const shared_ptr<FileSystemNode>& ancestor,
                         const shared_ptr<DirectoryNode>& candidate) const {
        if (ancestor.get() == candidate.get()) {
            return true;
        }
        if (!ancestor->isDirectory()) {
            return false;
        }
        auto dir = static_pointer_cast<DirectoryNode>(ancestor);
        for (const auto& [name, child] : dir->getChildren()) {
            if (child->isDirectory() && isSubtreeOrSame(child, candidate)) {
                return true;
            }
        }
        return false;
    }

    void collectMatches(const shared_ptr<FileSystemNode>& node,
                        const string& currentPath,
                        const string& query,
                        vector<string>& matches) const {
        if (currentPath != "/" && node->getName().find(query) != string::npos) {
            matches.push_back(currentPath);
        }
        if (!node->isDirectory()) {
            return;
        }
        auto dir = static_pointer_cast<DirectoryNode>(node);
        for (const auto& [name, child] : dir->getChildren()) {
            string childPath = (currentPath == "/") ? ("/" + name) : (currentPath + "/" + name);
            collectMatches(child, childPath, query, matches);
        }
    }

    void printSubtree(const shared_ptr<FileSystemNode>& node, int depth) const {
        string indent(depth * 2, ' ');
        if (node->isDirectory()) {
            cout << indent << node->getName()
                 << (node->getName() == "/" ? "" : "/")
                 << " (" << node->getSize() << " bytes)\n";
            auto dir = static_pointer_cast<DirectoryNode>(node);
            for (const auto& [name, child] : dir->getChildren()) {
                printSubtree(child, depth + 1);
            }
        } else {
            cout << indent << node->getName() << " (" << node->getSize() << " bytes)\n";
        }
    }

    shared_ptr<DirectoryNode> root_;
};

// -------------------------------------------------------------
// Helper to print vector<string> cleanly
// -------------------------------------------------------------
void printList(const string& label, const vector<string>& items) {
    cout << label << ": [";
    for (size_t i = 0; i < items.size(); ++i) {
        cout << items[i] << (i + 1 < items.size() ? ", " : "");
    }
    cout << "]\n";
}

// -------------------------------------------------------------
// Driver Demo
// -------------------------------------------------------------
int main() {
    FileSystem fs;

    cout << "====================================================\n";
    cout << "       In-Memory File System - System Design        \n";
    cout << "====================================================\n\n";

    // 1. Directory creation (mkdir -p) & File writing
    cout << "--- Test 1: Creating Directories & Writing Files ---\n";
    fs.mkdir("/home/alice/docs");
    fs.mkdir("/home/alice/projects/cpp");
    fs.mkdir("/var/log");

    fs.writeFile("/home/alice/docs/notes.txt", "System Design Notes: ");
    fs.appendToFile("/home/alice/docs/notes.txt", "Composite Pattern.");
    fs.writeFile("/home/alice/projects/cpp/main.cpp", "int main() { return 0; }");
    fs.writeFile("/var/log/app.log", "INFO: Server started\n");

    cout << "Read /home/alice/docs/notes.txt: "
         << fs.readFile("/home/alice/docs/notes.txt").value_or("<error>") << "\n";

    // 2. Listing & Composite sizes
    cout << "\n--- Test 2: ls (Sorted) & Recursive Directory Sizes ---\n";
    printList("ls /home/alice", fs.ls("/home/alice"));
    printList("ls /home/alice/docs/notes.txt (file)", fs.ls("/home/alice/docs/notes.txt"));
    cout << "Size of /home/alice: " << fs.getSize("/home/alice").value_or(0) << " bytes\n";
    cout << "Size of /: " << fs.getSize("/").value_or(0) << " bytes\n";

    fs.printTree("/");

    // 3. Search & Path Normalization (".", "..", "//")
    cout << "\n--- Test 3: Search & Path Normalization ---\n";
    printList("find '.txt' under /", fs.find("/", ".txt"));
    printList("find 'main' under /home", fs.find("/home", "main"));
    cout << "Read via normalized path (/home/alice/projects/../docs//notes.txt): "
         << fs.readFile("/home/alice/projects/../docs//notes.txt").value_or("<error>") << "\n";

    // 4. Move / Rename & Cycle Prevention
    cout << "\n--- Test 4: Move, Rename & Edge Cases ---\n";
    fs.move("/home/alice/docs/notes.txt", "/home/alice/projects/readme.md");
    fs.move("/home/alice", "/home/alice/projects"); // Should fail (cycle check)
    fs.writeFile("/missing/dir/file.txt", "fail"); // Should fail (missing parent)
    fs.mkdir("/home/alice/projects/readme.md/sub"); // Should fail (file in path)
    fs.remove("/"); // Should fail (cannot delete root)

    fs.printTree("/");

    // 5. Recursive Deletion
    cout << "\n--- Test 5: Recursive Delete ---\n";
    cout << "Removing /home/alice/projects: "
         << (fs.remove("/home/alice/projects") ? "Success" : "Failed") << "\n";
    cout << "Total size after delete: " << fs.getSize("/").value_or(0) << " bytes\n";
    fs.printTree("/");

    return 0;
}
