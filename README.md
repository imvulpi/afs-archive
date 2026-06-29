# Adaptive File Stream (AFS)

AFS is a straightforward, byte-aligned streaming archive format. It is built to be a simple, lightweight alternative to older formats like TAR for streaming files over networks or installing software bundles.

The project is split into two parts:
1. **The Specification (`SPECIFICATION.md`):** The layout design document showing how the bytes are organized without using fixed-block padding.
2. **The Library (Upcoming):** The actual code used to pack and unpack the files. It is designed so you can toggle features on or off depending on what your application needs.

---

## The Main Idea

Most traditional archive formats force files into fixed blocks of a certain size (like 512 bytes) and fill the leftover space with empty null bytes. AFS drops the padding entirely. 

Instead, every file in the archive gets a simple 2-byte header bitmask. This bitmask tells the unpacker exactly what metadata is coming up (like file size, permissions, or timestamps). If a file doesn't need timestamps or special permissions, those fields are left out of the file stream entirely. You only use bytes for the data you actually need.

### Key Features
* **No Padding:** Stream entries sit right next to each other, byte-for-byte.
* **Optional Global Index:** You can include a list of file offsets at the very beginning of the archive. This lets multi-threaded tools unpack different files simultaneously without scanning the whole archive first.
* **Cross-Platform Attributes:** Windows file attributes and Unix permissions are handled in separate, non-overlapping parts of a single 4-byte block, making it easy to restore files correctly on either OS.

---

## The Library & Flexible Compiling

When the reference library is ready, it won't force you to compile a single, bulky binary. Instead, you can use build flags (like `#define` statements in C) to compile different versions of the packager and extractor based on your target system.

---

## 📄 Licenses

We use two different licenses to keep the file format rules completely open while making the library code as easy to reuse as possible.

* **The Specification (`SPECIFICATION.md`):** Licensed under **Creative Commons Attribution 4.0 ([CC-BY-4.0](https://creativecommons.org/licenses/by/4.0/))**. Anyone can build their own tools for `.afs` files or modify the spec, as long as you give credit to the original layout.
* **The Code / Libraries (Upcoming):** Licensed under the **[MIT License](https://opensource.org/licenses/MIT)**. You can drop this code into open-source projects or closed-source commercial apps without any restrictions or liabilities.

---

## 👥 Authors & Contributors

* **Vulpi** - Lead Architect & Maintainer
* **Quarkit Development Team** - Feedback and implementation testing

*For the exact details on the bitmasks, header fields, and extension structures, check out the [SPECIFICATION.md](./SPECIFICATION.md) file.*