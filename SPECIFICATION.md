# Adaptive File Stream (AFS) Architecture Specification

## Abstract

Traditional archive formats, such as TAR, were originally architected for sequential tape drives, relying on a fixed-block structure (typically 512 bytes) and mandatory padding to align data. While highly reliable for legacy systems, this approach introduces predictable structural overhead and requires constant stream alignment padding.

The Adaptive File Stream (AFS) is a compact, byte-aligned streaming format engineered specifically for high-efficiency asset delivery and modern deployment frameworks like Quarkit. AFS replaces fixed-block structures with a dynamic, linear pipeline where each file entry handles its own metadata footprint via a bitmask. If optional attributes - such as timestamps, advanced permissions, or inline extensions - are not required for a deployment, they are completely omitted from the stream. This optimization minimizes archive bloat, eliminates unnecessary padding, and allows for direct, single-pass linear parsing that maximizes disk I/O throughput.

---

## 1. Introduction

When engineering asset distribution networks or installer frameworks, file format design directly impacts decompression speed, storage footprint, and network transit costs. AFS removes structural overhead by treating the archive as a continuous, byte-exact sequence of data.

### Core Architecture Principles:

* **Metadata Optionality**: Fields can be optional which is beneficial for size. For example, directories naturally don't need payload size headers, and files without strict security requirements drop permission masks entirely. The stream layout differs dynamically on a per-entry basis.
* **Single-Pass Processing**: The format ensures that the extractor can process the stream sequentially from start to finish without requiring backward seek operations. This design simplifies client implementation and optimizes CPU cache utility.
* **Cross-Platform Mapping**: Rather than forcing abstraction layers or complex emulation for security, AFS separates Unix permission bits and Windows file attributes into distinct, non-overlapping regions within a unified 4-byte attribute pool. This ensures high-fidelity restoration on both POSIX and Win32 target environments.
* **Decentralized Extensions**: Metadata extensions can be housed entirely within individual file entries rather than declared in a global extension block. This allows chunks of the stream to be sliced, moved, or processed as independent, self-contained units.
* **Centralized Extensions**: Instead of repeating configurations or extension data, centralized extensions can be used once in the global extension block of the header. For example in case of an Index Extension in multi-threaded extractions.
* **Compression Efficiency:**: By avoiding artificial padding blocks between file payloads, AFS maintains high data density. This prevents general-purpose compression algorithms (such as Zstd, LZMA, or Deflate) from wasting dictionary space on predictable blocks of null bytes, resulting in better overall compression ratios.

---

## 2. Global Header & Stream Structure

An AFS archive begins with a lightweight, fixed-size 8-byte Global Header that establishes file identity and basic configuration states. Based on Bit 1 of the Global Flags the Header can extend to house global extensions, which are predictable in size and can be ommited.

**Note: The format uses Little Endian for byte order.**
Because most desktops, laptops, smartphones and modern processor run natively on Litte Endian.

### 2.1 Fixed Header Layout

| Magic Signature | Format Version | Global Flags | Conditional Global Extension Block
| :--- | :--- | :--- | :--- |
| 4 Bytes | 2 Bytes | 2 Bytes | Varies |

**_Where:_**

1. **Magic Signature** (4 Bytes - char[4]): The constant ASCII string ADFS (0x41 0x44 0x46 0x53). This uniquely identifies the stream as an Adaptive File Stream archive.
2. **Format Version** (2 Bytes - uint16_t): The revision of the specification used to package the archive (e.g., 1 for v1.0). This ensures backward compatibility if structural bit alignments shift in future major revisions, and helps understand the features available in the specified version of the format.
3. **Global Flags** (2 Bytes - uint16_t): Global state settings applied to the entire stream processing lifecycle:
    * Bit 0 (0x0001): Archive contains global extensions.
    * Bits 1-15: Reserved for future global parameters.

### 2.2 Global Extension Block 

If Bit 0 (Has Global Extensions) of the Global Flags is set to 1, the stream sequentially injects a global extension wrapper before any file entry metadata begins. This layout uses the exact same self-contained architecture found in entry-level extensions (Section 5).

Specific extension format:

<table>
  <thead>
    <tr>
      <th style="width: 20%; text-align: left;">Loop Controller</th>
      <th style="width: 2%;"></th>
      <th colspan="3" style="width: 78%; text-align: left;">Extension Data</th>
    </tr>
    <tr>
      <th style="text-align: left;">Extension Count</th>
      <th></th>
      <th style="text-align: left;">Extension ID</th>
      <th style="text-align: left;">Extension Size</th>
      <th style="text-align: left;">Extension Data</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>2 Bytes (Only Start)</td>
      <td></td>
      <td>2 Bytes</td>
      <td>4 Byte</td>
      <td>Varies (Extension Size)</td>
    </tr>
  </tbody>
</table>

Data Structure Note:
 - Loop Controller: Appears exactly once at the start of the block.
 - Extension Data: This entire segment repeats sequentially based on the value defined in the Extension Count field.

Explanation of fields:

| Field Name | Data Type | Description |
| :--- | :--- | :--- |
| Extension ID   | uint16_t  | The identifier for the block logic. **IDs 0x0000 to 0x7FFF are reserved** for format standardization. IDs 0x8000 to 0xFFFF are open for vendor-specific custom properties. |
| Extension Size | uint32_t  | The exact byte length of the trailing payload data. This allows safe stream skipping, which is useful in cases where extractor can't handle the extension. |
| Extension Data | uint8_t[] | The raw configuration space. For custom or third-party IDs, developers can prepend a 4-byte magic ASCII string keyword to guarantee unique execution contexts. |

### 2.3 Adaptive Entry Layout

The data elements of an individual stream entry must appear in the exact sequential order defined below. Elements marked as optional are completely omitted from the byte stream if format calls for it or their corresponding activation bits are set to `0` in the Entry Bitmask.

| Field Order | Data Type | Field Name | Description |
| :--- | :--- | :--- | :--- |
| 1        | `uint16_t`   | Entry Bitmask | 2-byte field defining entry type and active metadata fields. |
| 2        | `char[]`     | Path String   | The destination path, matching standard UTF-8/ASCII strings, terminated by a null-byte `\0. |
| 3 (Opt)* | `uint32_t`   | File Size     | 4-byte unsigned integer indicating the total size of the raw data payload in bytes. |
| 4 (Opt)  | `uint32_t`   | Permissions   | 4-byte field containing crossplatform OS attributes (e.g., mode bits / attributes). |
| 5 (Opt)  | `uint8_t`    | Date Sub-Mask | 1-byte field defining which specific timestamp fields follow immediately after. |
| 6 (Opt)  | `uint32_t[]` | Timestamps    | Sequentially ordered 4-byte Unix timestamps (seconds since Jan 1, 1970) activated by the Date Sub-Mask. |
| 6 (Opt)  | `Extension*` | Extensions    | The inline extensions block which structure is defined in section 5. |
| 7 (Opt)  | `uint8[]`    | Payload Data  | The raw binary contents of the file, or target path string for hard/symbolic links. Length determined by Field 3 or implicitly based on entry type. |

**Notes:** 
 - `3. File Size` - Whether File Size is available is implicitly based on the entry type, which is specified in **_Section 6_**
 - `6. Extensions` - Extension layout is specified in **_5 Inline Extension Block Specification_**

## 3. Entry Metadata & Entry Bitmask 

Immediately following the Global Header, the stream transitions into a continuous sequence of file entries. Each entry begins with a 2-byte Primary Entry Bitmask (uint16_t) that dictates the structural schema of the fields following it.

### 3.1 Entry Bitmask (2 Bytes)

_> Releated to 2.2 Adaptive Entry Layout - 1st Field_

The first 16 bits of the entry are evaluated as follows.

| Bit | Hex Mask | Field Name | Functional Meaning / Layout Impact when Set (`1`) |
| :--- | :--- | :--- | :--- |
| **Bits 0-3** | `0x000F` | Entry Type | Maps entry types (Regular File, Directory, Symlink, etc.). _See Explanation Below_. |
| **Bit 4** | `0x0010` | Has Permissions | Injects the 4-byte (`uint32_t`) Cross-Platform Permission & Attribute Mask. |
| **Bit 5** | `0x0020` | Has Dates | Gateway bit; injects a 1-byte Date Sub-Mask followed by active 4-byte Unix timestamp(s). |
| **Bit 6** | `0x0040` | Has Custom Extensions | Appends a self-contained Extension Block directly into this entry's metadata layout. |
| **Bits 7–15** |  -  | Reserved | Reserved for future format architecture revisions. Must be initialized to `0`. |

* **Bits 0–3: Entry Type (`0x0F`)**
  Indicates the physical operating system filesystem node type. 
  * `0000` (`0`) - **Regular File**: Payload field contains raw binary file contents.
  * `0001` (`1`) - **Hard Link**: Payload field contains the null-terminated target path string.
  * `0010` (`2`) - **Symbolic Link**: Payload field contains the null-terminated symlink path string.
  * `0011` (`3`) - **Character Special**: Hardware device node configuration.
  * `0100` (`4`) - **Block Special**: Hardware block device configuration.
  * `0101` (`5`) - **Directory**: Represents a directory structure. Payload field is empty (0 bytes).
  * `0110` (`6`) - **FIFO**: Named pipe pipeline configuration.
  * `0111` (`7`) - **Contiguous File**: Pre-allocated storage block target.
  * `1000` to `1111` (`8–15`) - **Reserved**: Allocations reserved for future standardization layers.
* **Bit 4: Has Permissions (`0x20`)**
  * `0` = Field 4 is omitted entirely.
  * `1` = Field 4 (`uint16_t`) is present.
* **Bit 5: Has Dates (`0x40`)**
  * `0` = Field 5 and Field 6 are omitted entirely.
  * `1` = Field 5 (`uint8_t` Date Sub-Mask) is present in the stream.
* **Bit 6: Has Custom Extensions (`0x80`)**
  * `0` = No custom developer extension blocks are attached to this entry.
  * `1` = Activates the localized custom metadata payloads registered in the Global Header.
* **Bits 7–15: Reserved**
  Reserved exclusively for future core format layout upgrades. Must be written as `0` in compliance with Version 1 parsers.

---

## 4. Sequential Layout Fields

Following the Entry Bitmask, fields are evaluated and processed linearly in the exact sequence outlined below and in section _2.2 Adaptive Entry Layout_. If a field is noted as conditional, it appears if and only if its triggering bit in the primary bitmask is active.

### 4.1 Path String Format

The Path String represents the relative destination path of the file entry within the target deployment directory. It is written immediately following the entry bitmask.

 - **Universal Separator**: Paths must exclusively use the forward slash (/) as the directory hierarchy separator. Backslashes (\\) are prohibited to ensure the standard.
 - **Null-Termination**: The string must be terminated with a single null byte (\0 / 0x00).
 - **No Length Header**: This is not a length prefixed string, stream must be read until a null terminator (\0 / 0x00).
 - **Relative Constraints**: Paths must be relative to the root extraction directory. Absolute paths and directory traversal tokens (../) should be avoided.

 **Note:** Extractors that are publically available should especially follow the _**Relative Constraints**_, because they don't control the input (AFS archive) that is given to the program.

### 4.2 File Size (Conditional):

**Trigger:** Implicitly required if Entry Type is 0000 (Regular File) or 0111 (Contiguous File).

A 4-byte (uint32_t) unsigned integer field defining the exact byte length of the upcoming raw binary file payload. For all other Entry Types, this field is completely absent from the stream.

**Note:** Other cases where it's ommited are specified in **_Section 6._**

### 4.3 Permissions & Attributes Mask (Conditional)

**Trigger:** Active when Bit 4 (Has Permissions) of the entry bitmask is set to 1.

A 4-byte (uint32_t) field explicitly decoupling standard POSIX permission masks from native Windows file attributes and Linux kernel-level attributes, guaranteeing exact replication states across target architectures.

| Bit | Hex Mask | Flag Name | Operating System Context & Extractor Actions |
| :--- | :--- | :--- | :--- |
| **Bit 0** | `0x00000001` | Owner Read | POSIX: Maps to `S_IRUSR`. |
| **Bit 1** | `0x00000002` | Owner Write | POSIX: Maps to `S_IWUSR`. |
| **Bit 2** | `0x00000004` | Owner Exec | POSIX: Maps to `S_IXUSR`. Ignored on Windows. |
| **Bit 3** | `0x00000008` | Group Read | POSIX: Maps to `S_IRGRP`. Ignored on Windows. |
| **Bit 4** | `0x00000010` | Group Write | POSIX: Maps to `S_IWGRP`. Ignored on Windows. |
| **Bit 5** | `0x00000020` | Group Exec | POSIX: Maps to `S_IXGRP`. Ignored on Windows. |
| **Bit 6** | `0x00000040` | Other Read | POSIX: Maps to `S_IROTH`. Ignored on Windows. |
| **Bit 7** | `0x00000080` | Other Write | POSIX: Maps to `S_IWOTH`. Ignored on Windows. |
| **Bit 8** | `0x00000100` | Other Exec | POSIX: Maps to `S_IXOTH`. Ignored on Windows. |
| **Bits 9–15** |  -  | Reserved | Reserved for future revisions |
| **Bit 16** | `0x00010000` | Win Hidden | Windows: Applies `FILE_ATTRIBUTE_HIDDEN`. Ignored on Linux. |
| **Bit 17** | `0x00020000` | Win System | Windows: Applies `FILE_ATTRIBUTE_SYSTEM`. Ignored on Linux. |
| **Bit 18** | `0x00040000` | Win Archive | Windows: Applies `FILE_ATTRIBUTE_ARCHIVE`. Ignored on Linux. |
| **Bit 19** | `0x00080000` | Win Encrypt | Windows: Applies `FILE_ATTRIBUTE_ENCRYPTED`. Ignored on Linux. |
| **Bit 20** | `0x00100000` | Win Readonly | Windows: Applies `FILE_ATTRIBUTE_READONLY`. Ignored on Linux. |
| **Bits 21–23** |  -  | Reserved | Reserved for future revisions. |
| **Bit 24** | `0x01000000` | Linux Append | Linux: Sets `FS_APPEND_FL` (`chattr +a`). Requires root/CAP_LINUX_IMMUTABLE. |
| **Bit 25** | `0x02000000` | Linux Immut | Linux: Sets `FS_IMMUTABLE_FL` (`chattr +i`). Overrides standard write locks. |
| **Bit 26** | `0x04000000` | Linux NoDump | Linux: Sets `FS_NODUMP_FL` (`chattr +d`). |
| **Bit 27** | `0x08000000` | Linux Sync | Linux: Sets `FS_SYNC_FL` (`chattr +s`). Forces synchronous I/O. |
| **Bit 28** | `0x10000000` | Linux UnDel | Linux: Sets `FS_UNRM_FL` (`chattr +u`). |
| **Bits 29–31** |  -  | Reserved | Reserved for future revisions. |

### 4.4 Dates (Conditional)

**Trigger:** Active when Bit 5 (Has Dates) of the entry bitmask is set to 1.

When active the dates bitmask is present in the entry, this mask determines which 4-byte (`uint32_t`) integers follow it, in strict sequential bit order:

* **Bit 0 (`0x01`): Modification Time (`mtime`)** - Injects 4-byte Unix timestamp if `1`.
* **Bit 1 (`0x02`): Access Time (`atime`)** - Injects 4-byte Unix timestamp if `1`.
* **Bit 2 (`0x04`): Creation/Birth Time (`btime`)** - Injects 4-byte Unix timestamp if `1`.
* **Bits 3–7 (`0x08 - 0x80`): Reserved** - Reserved for sub-second precision layers or future definitions.

Setting any of the bits to a value of `1`, means following the bitmask the date will be present in the order above. 
**Important:** Setting **Bit 5 (Has Dates)** to 1 means at least one timestamp exists.

### 4.5 Specific Dates (Conditional)

Following the dates bitmask specified in _4.4 Dates (Conditional)_ a list of Unix timestamps (seconds since Jan 1, 1970) in uint32_t is present in order defined in the mentioned section. The extractors that can't handle specific dates should ommit them by moving 4 Bytes forward for each 1 in a dates bitmask for a date they can't handle. 

**Note:** With future reserved fields the size should be **uint32_t** to maintain forward compatibility, parsers should move forward by 4 Bytes for the reserved fields to maintain forward compability. The bytes that are reserved should be set to 0 as they're not defined in this revision.

## 5. Inline Extension Block Specification

When Bit 6 (Has Inline Extensions) of the Entry Bitmask is set to 1, a self-contained extension wrapper is injected into the entry's metadata stream. This layout eliminates the need for a global archive registry header, allowing individual entries to carry modular, custom, or platform-specific metadata packets independently.

### 5.1 Extension Block Layout

The block begins with a 2-byte extension count field, followed sequentially by individual extension payloads.

This is the exact same format as with Global Extensions in _Section 2.2_

Specific extension format:

<table>
  <thead>
    <tr>
      <th style="width: 20%; text-align: left;">Loop Controller</th>
      <th style="width: 2%;"></th>
      <th colspan="3" style="width: 78%; text-align: left;">Extension Data</th>
    </tr>
    <tr>
      <th style="text-align: left;">Extension Count</th>
      <th></th>
      <th style="text-align: left;">Extension ID</th>
      <th style="text-align: left;">Extension Size</th>
      <th style="text-align: left;">Extension Data</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>2 Bytes (Only Start)</td>
      <td></td>
      <td>2 Bytes</td>
      <td>4 Byte</td>
      <td>Varies (Extension Size)</td>
    </tr>
  </tbody>
</table>

Data Structure Note:
 - Loop Controller: Appears exactly once at the start of the block.
 - Extension Data: This entire segment repeats sequentially based on the value defined in the Extension Count field.

Explanation of fields:

| Field Name | Data Type | Description |
| :--- | :--- | :--- |
| Extension ID   | uint16_t  | The identifier for the block logic. IDs 0x0000 to 0x7FFF are reserved for format standardization. IDs 0x8000 to 0xFFFF are open for vendor-specific custom properties. |
| Extension Size | uint32_t  | The exact byte length of the trailing payload data. This allows safe stream skipping, which is useful in cases where extractor can't handle the extension. |
| Extension Data | uint8_t[] | The raw configuration space. For custom or third-party IDs, developers can prepend a 4-byte magic ASCII string keyword to guarantee unique execution contexts. |

### 5.2 Blind Skipping Strategy (Forward Compatibility)

This decentralized approach ensures that lightweight or older client extractors do not crash when encountering advanced archives. Because every extension explicitly passes its own Extension Size (uint32_t), an unfamiliar parser can read the ID, realize it is unsupported, and safely execute a forward seek or stream skip instruction.

## 6. Entry Payload Data Specification

The Entry Payload Data block represents the actual asset payload or other data (like paths in symbolic links). It is positioned at the absolute end of the entry's metadata layout. Once an extractor completes processing the entry bitmask and any active conditional fields (Permissions, Timestamps, or Inline Extensions), the stream enters the payload boundary.

Parsers must dynamically adjust how they read and advance through this space by referencing the structural properties established in the table below:

| Binary Type | Dec | Entry Type | File Size Field Present? | Payload Format Structure |
| :--- | :--- | :--- | :--- | :--- |
| `0000` | `0` | **Regular File** | **Yes** (`uint32_t`) | **Regular Payload:** Content boundary is defined exactly by the File Size field. Can contain arbitrary binary data. |
| `0001` | `1` | **Hard Link** | *Omitted* | **Self-Terminating Path:** Consists entirely of a null-terminated (`\0`) target path string. |
| `0010` | `2` | **Symbolic Link** | *Omitted* | **Self-Terminating Path:** Consists entirely of a null-terminated (`\0`) symlink path string. |
| `0011` | `3` | **Character Special**| *Omitted* | **Self-Terminating String:** Hardware node configuration details encoded as a null-terminated string. |
| `0100` | `4` | **Block Special** | *Omitted* | **Self-Terminating String:** Hardware block configuration details encoded as a null-terminated string. |
| `0101` | `5` | **Directory** | *Omitted* | **Empty Payload:** Standard directory container marker. Allocates exactly 0 bytes in the stream. |
| `0110` | `6` | **FIFO** | *Omitted* | **Self-Terminating String:** Named pipe configurations use a simple null-terminated string. |
| `0111` | `7` | **Contiguous File** | **Yes** (`uint32_t`) | **Regular Payload:** Pre-allocated continuous storage block target; bounds defined explicitly by the File Size field. |
| `1000`–`1111`| `8-15`| **Reserved** | *Fallback Mode* | **Default:** Future specifications should default to a null-terminated format *__unless__* the payload handles arbitrary binary data where a true null byte (`0x00`) can natively occur. |


### 6.1 Payload Processing Modes

Depending on the entry type evaluation defined in the table above, the extractor handles the incoming payload stream in one of three programmatic ways:

1. **Length-Bounded Streams** (Regular / Contiguous Files)
For Entries that pass an explicit size descriptor, the extractor can initialize a read buffer matching the 4-byte uint32_t File Size value. The stream is read sequentially for exactly that number of bytes.
    - **Data Safety**: Because the boundary is strictly numeric, the binary contents can safely contain arbitrary data, nested null bytes (0x00), or execution code without risk of truncating early.
    - **Stream Synchronization**: Once the specified byte count is read, the stream pointer is positioned at the first byte of the next entry's Primary Entry Bitmask.

2. **Null-Terminating Strings** (Links / Device Configurations)
For Entries that omit the File Size field but require a destination parameter (such as Hard Links, Symbolic Links, or Special Entries), the payload functions as a continuous text stream.
    - **Extraction Action**: The extractor reads bytes one by one, writing them out as the target link string, until it intercepts a true null terminator (\0 / 0x00).
    - **Stream Synchronization**: The null byte acts as both the payload boundary and the synchronization marker. The immediate next byte belongs to the subsequent file entry.

3. **Empty Payloads** (Directories)
For entries designated as Directories, the format enforces zero allocation overhead.
    - **Extraction Action**: The payload block drops to exactly 0 bytes.
    - **Stream Synchronization**: No reading action is performed. The parser immediately transitions to processing the next block header in the stream.

### 6.2 Stream Pipeline Visualization

The following diagram illustrates how the layout footprint changes sequentially based on the payload profiles determined by the Entry Type:

```
Regular File Pipeline:
[Bitmask (2B)] ──> [Path\0] ──> [File Size (4B)] ──> [Optional Fields] ──> [Raw Binary Payload (N Bytes)] ──> [Next Bitmask...]

Symbolic Link Pipeline:
[Bitmask (2B)] ──> [Path\0] ───────────────────────> [Optional Fields] ──> [Target Path String\0] ─────────> [Next Bitmask...]

Directory Pipeline:
[Bitmask (2B)] ──> [Path\0] ───────────────────────> [Optional Fields] ──> (0-Byte Empty Space) ──────────> [Next Bitmask...]
```

---

## 7. Implementation & Adaptability Guideline

The architectural defining characteristic of the Adaptive File Stream (AFS) is asymmetric complexity. The format is designed to be highly adaptive, granting complete flexibility to both the packer and the extractor based on the operational constraints of their execution environments.

### 7.1 Extractor Scaling Profiles

Unlike rigid traditional archive parsers, an AFS extractor is explicitly permitted to drop or ignore feature layers it does not require or cannot support due to environmental limitations (such as minimal storage stubs, bootloaders, or restricted user-space installers).

Because every optional metadata structure is guarded by a dedicated bit in the Entry Bitmask and explicitly defines its own size boundaries, client extractors can be scaled across various footprint profiles:

1. **The Minimalist Profile (e.g., Bootstrappers / Embedded Stubs):**
   * **Behavior:** The extractor completely ignores Bits 4, 5, and 6. It only reads the type bits to handle the payload state and the Path string. 
   * **Implementation Action:** If Bit 4 (`Has Permissions`) is `1`, the parser blindly advances the stream pointer by 4 bytes. If Bit 6 (`Has Inline Extensions`) is `1`, it reads the extension sizes and loops past them without allocating memory or executing logic. 
   * **Benefit:** Allows for ultra-compact extractor binaries (under a few kilobytes) ideal for space-constrained environments.

2. **The Standard Profile (e.g., Application Installers / User-Space Tools):**
   * **Behavior:** Processes file locations and universal attributes. It honors the `Win Readonly` flag or standard owner-write permission stripping but ignores advanced kernel configurations like Linux `chattr` flags or custom extension blocks.
   * **Implementation Action:** Evaluates Byte 0 of the permissions mask and ignores Byte 3. Skips unknown inline extension IDs using the blind-skipping strategy.

3. **The High-Fidelity Profile (e.g., System Backups / Root Provisioning Frameworks):**
   * **Behavior:** Full-spectrum enforcement. The extractor parses all 4 bytes of the permissions mask (including `chattr` capabilities), restores microsecond timestamp precision, and maps custom inline extensions (such as cryptographic signatures or advanced ACLs).

### 7.2 Forward & Backward Compatibility Lifecycle

The core layout of the AFS stream is designed to be highly durable. However, versioning and extensions should adapt naturally as the ecosystem grows.

* **Version Bumps & Standardization:** The `Format Version` field in the Global Header should be incremented when new, official extensions are integrated into the core specification standard. This alerts extractors to the baseline feature set used to pack the archive.
* **The Extension Recommendations:** Any feature requirement that modifies or expands metadata per file should be implemented via the Inline Extension Block (Section 5) or Global Extension Block (Section 2.2) rather than by altering the base structure sequence or reclaiming reserved bits haphazardly. Because older parsers safely use the blind-skipping strategy on unknown Extension IDs, archives utilizing newer or vendor-specific extensions remain fully forward-compatible with legacy, lightweight extractors.
* **Forward Compatibility** The extractors or packagers can be fully forward compatible by understanding the structure and prematurely introducing blind-skipping for reserved bytes in the format when bits of one are set to 1. This ensures that even with next revisions the old extractors or packagers still work as intended. For example: the dates bitmask specifies that if a bit is set to 1 a 4 byte uint32_t date will follow after the bitmask, a program can stay forward compatible by ensuring it skips forward for bits even if they're reserved.

### 7.3 Parallel Extraction & The Index Extension Pattern

Because entries are self-contained and explicitly declare their own boundaries, the format naturally supports concurrent execution. To maximize multi-threaded efficiency without forcing specific pipeline architectures, the format can utilize an **Index Extension**.

An index extension acts as a centralized directory map included in global extension data block within the stream. This block contains a sequential array of byte offsets mapping out exactly where every subsequent file entry begins. More on the Index Extension on Section 8.1.

### 7.4 Developer Autonomy Statement

While this specification outlines the precise bit-alignments and layout structures required to maintain stream validity, **the physical implementation strategy remains entirely at the discretion of the developer creating the packaging or extraction software.** Architects are free to optimize their processing loops for their specific hardware paradigms - whether that involves asynchronous multi-threaded payload streaming, zero-copy memory-mapped file allocations, or strictly sequential stack-allocated stream reads. The format guarantees that as long as the parser respects the linear byte boundaries dictated by the active bitmasks, the stream will remain fully synchronized and cross-compatible across all compliance tiers.

## 8. Standardized Extensions

To maintain interoperability between different packaging tools and extractors, this section defines the official, standardized Extension IDs. Both Global Extensions (Section 2.2) and Inline Extensions (Section 5) utilize this registry to ensure consistent behavior across platform ecosystems.

### 8.1 The Index Extension Map (Extension ID: 0x0001)

The Index Extension is a highly optimized directory map designed primarily as a **Global Extension** to enable high-performance parallel extraction. By packing raw stream offsets into dynamically sized integers, this extension allows multi-threaded extraction engines to partition and seek through the archive with minimal metadata overhead.

#### Internal Data Structure Layout

| Byte Offset | Field Name | Data Type | Functional Description |
| :--- | :--- | :--- | :--- |
| `0x00` | **Total Entry Count** | `uint32_t` | Specifies the exact number of indexed file entries present in the trailing array map. |
| `0x04` | **Offset Width Flag** | `uint8_t` | Dictates the integer byte-width used for every element in the Index Array. (See Width Mapping Table). |
| `0x05` | **Index Offset Array** | `Variable[]` | A continuous block of sequentially packed integers representing stream offsets. Maps perfectly 1:1 to the sequential file entries in the archive. |

#### Offset Width Mapping
The **Offset Width** (`uint8_t`) configures the parser's array loop stepping size. Packers choose the smallest width capable of holding the absolute maximum byte offset of the archive stream:

| Flag Value | Integer Type | Element Width | Maximum Addressable Archive Size |
| :--- | :--- | :--- | :--- |
| `0x02` | `uint16_t` | 2 Bytes | 65.5 Kilobytes |
| `0x04` | `uint32_t` | 4 Bytes | 4.2 Gigabytes |
| `0x08` | `uint64_t` | 8 Bytes | 16.7 Exabytes (Unbounded / Large Deployments) |

> A 1-byte (`uint8_t`/`0x01`) width is explicitly omitted from this mapping. Archives under 255 bytes are structurally too small to derive any performance benefit from multi-threaded indexing; the CPU overhead required to initialize worker threads exceeds the total processing time of a direct, single-threaded extract.

#### Operational Impact

* **High-Density Parallelism:** Because the Index Array is a homogeneous list of fixed-size integers, a worker thread can calculate the exact memory address of any file's offset using simple math: `offset_address = 0x05 + (target_index * width)`.

> In theory, an index could utilize a per-entry bitmask or prefix flag to dynamically define a variable integer width for each individual offset. While this approach might shave a few bytes off the first handful of small-offset entries, it destroys the mathematical predictability of the index stream. Forcing variable-width fields prevents extractors from calculating individual entry boundaries using simple pointer arithmetic, turning random-access lookups into slow, sequential bit-scanning operations. Maintaining a uniform, global width across the entire index array guarantees absolute compatibility with multi-threaded, lock-free parallel extraction architectures.

## Document Metadata & Contributors

### Authors
* **Vulpi** - Lead Architect & Specification Maintainer

### Acknowledgments & Contributors
* **Quarkit Development Team** - For feedback and initial reference implementation testing.

### 10.3 Revision History

| Version | Date | Description | Authors |
| :--- | :--- | :--- | :--- |
| `v1.0.0` | June 2026 | Initial public specification release. Consolidated dynamic bitmasking layout, Global Extension architectures, and standardized Index Maps (`0x0001`). | Core Working Group |

---

> **Copyright Notice & License** > This specification is licensed under the Creative Commons Attribution 4.0 International License (CC-BY-4.0). You are free to share, copy, modify, and distribute this format, provided appropriate credit is given to the original authors.