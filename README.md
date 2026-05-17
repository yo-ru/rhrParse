# rhrParse

A small C++17 library that parses and encodes [Rhythia](https://rhythia.com) replay files.

- **Single header + one .cpp.** Drop into any project as a submodule.
- **No dependencies** beyond the C++17 standard library.
- **Round-trip-safe.** Bytes that parse will re-encode identically.
- **Forward + backward compatible.** Handles every replay format version, falling back gracefully on older files.

## File layout

```
include/rhrParse/rhrParse.h     # public API
src/rhrParse.cpp                # implementation
examples/dump.cpp               # print a replay's contents
examples/roundtrip.cpp          # parse + re-encode and verify
CMakeLists.txt                  # optional, for add_subdirectory() users
```

## Integration

### As a git submodule (recommended)

```sh
git submodule add https://github.com/yo-ru/rhrParse third_party/rhrParse
```

Then add `third_party/rhrParse/src/rhrParse.cpp` to your build's source list and put `third_party/rhrParse/include/` on the include path. That's it.

### Via CMake

```cmake
add_subdirectory(third_party/rhrParse)
target_link_libraries(my_app PRIVATE rhrParse::rhrParse)
```

### Plain build

```sh
g++ -std=c++17 -I include src/rhrParse.cpp my_code.cpp -o my_app
```

## Usage

```cpp
#include <rhrParse/rhrParse.h>
#include <fstream>
#include <iterator>

std::ifstream f("score.rhr", std::ios::binary);
std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());

rhr::Replay replay;
if (!rhr::Parse(bytes, replay)) {
    fprintf(stderr, "invalid replay\n");
    return 1;
}

printf("Player: %s\n", replay.scoreData.playerName.c_str());
printf("Score:  %lld\n", (long long)replay.scoreData.totalScore);
printf("Frames: %zu\n", replay.frames.size());

// Re-encode (for editing or relaying):
auto encoded = rhr::Encode(replay);
```

See [`examples/dump.cpp`](examples/dump.cpp) for a complete CLI utility.

## API

| Symbol | Purpose |
|---|---|
| `rhr::Replay` | Top-level record: `version`, `scoreData`, `frames`. |
| `rhr::ScoreData` | Per-run metadata (player, map, score, mods, etc.). |
| `rhr::ReplayFrame` | One cursor sample (`time`, `positionX/Y`, `health`, `isImportantFrame`). |
| `rhr::Parse(bytes, out)` | Decode bytes into a `Replay`. Returns `false` on truncation/invalid input. |
| `rhr::Encode(replay)` | Encode a `Replay` to `std::vector<uint8_t>` in the format chosen by `replay.version`. |
| `rhr::kVersionBeatmapHash` etc. | Format-version constants. Set `replay.version` to one of these when synthesising replays. |

All API surface is documented inline in [`rhrParse.h`](include/rhrParse/rhrParse.h).

## Wire format

Little-endian throughout. Strings use .NET's `Write7BitEncodedInt` length prefix (LEB128).

```
int32   version                     ; date-encoded YYYYMMDD
int64   timestamp                   ; .NET DateTime ticks
string  playerName
string  legacyMapId
int32   mapId
int32   startFrom
string  mode

; extended fields (version >= 20260125):
bool    passed
string  mods
bool    spin
float   speed
int64   totalScore

float   accuracy
int32   hits
int32   misses
float   points

; fail-time (version >= 20260222):
int32   failTime                    ; -1 if not failed

; beatmap-hash (version >= 20260517):
string  beatmapHash                 ; content hash of the played beatmap

int32   frameCount
frame[] frames                      ; frameCount * 17 bytes each
    int32  time                     ; older versions (< 20260510) store this as float
    float  positionX
    float  positionY                ; older versions (< 20260118) store this negated
    float  health
    uint8  isImportantFrame
```

## Versioning

| Magic | Effect on the wire |
|---|---|
| `< 20260118` | `positionY` is stored with the opposite sign. The parser handles this transparently on both decode and encode. |
| `< 20260125` | The extended-fields block (`passed`/`mods`/`spin`/`speed`/`totalScore`) isn't in the wire data; defaults are filled in. |
| `< 20260222` | `failTime` isn't in the wire data; defaults to `-1`. |
| `< 20260510` | Frame `time` is stored as `float` instead of `int32`. The parser converts transparently in both directions. |
| `< 20260517` | `beatmapHash` isn't in the wire data; defaults to the empty string. |

Encoding always writes the format implied by the `version` you set on the `Replay`. Use `rhr::kVersionBeatmapHash` for new files.

## License

[MIT](LICENSE). Use freely; attribution appreciated but not required.

## Credits

The Rhythia replay format was reverse-engineered from the official client. This library is an independent reimplementation; no game code is reused.
