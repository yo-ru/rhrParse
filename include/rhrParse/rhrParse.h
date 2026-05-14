// rhrParse — Rhythia replay (.rhr) parser and encoder.
//
// Single-include public header. C++17.
//
// Usage:
//
//   #include <rhrParse/rhrParse.h>
//
//   std::vector<uint8_t> bytes = /* file contents */;
//   rhr::Replay replay;
//   if (rhr::Parse(bytes, replay)) {
//       printf("Player: %s\n", replay.scoreData.playerName.c_str());
//       printf("Frames: %zu\n", replay.frames.size());
//   }
//
//   std::vector<uint8_t> out = rhr::Encode(replay);
//
// License: MIT. See LICENSE.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rhr {

// One cursor sample. Rhythia writes one of these per game tick.
struct ReplayFrame
{
    int32_t  time;             // milliseconds since map start (float in versions < kVersionInt32Time)
    float    positionX;        // playfield-space x (note grid is centred on 0)
    float    positionY;        // playfield-space y
    float    health;           // 0..1, player health at this tick
    uint8_t  isImportantFrame; // non-zero if this tick is flagged important (e.g. note hit)

    // The wire format packs the frame as 4 four-byte fields followed by
    // 1 byte (no padding) — 17 bytes total. The in-memory struct may
    // have trailing alignment padding, which is why we don't memcpy the
    // whole frame at once.
    static constexpr std::size_t kWireSize = 4 * 4 + sizeof(uint8_t);
};
static_assert(ReplayFrame::kWireSize == 17, "ReplayFrame wire format must be 17 bytes");

// Per-run score record. Fields populated during decode depend on the
// replay's `version` (see kVersion* constants below); older replays
// receive sensible defaults for fields that didn't exist yet.
struct ScoreData
{
    int64_t      timestamp     = 0;     // .NET DateTime ticks (100ns since 0001-01-01 UTC)
    std::string  datePlayed;             // decoded local-time string "YYYY/MM/DD HH:MM:SS" (decode only)
    std::string  playerName;
    std::string  legacyMapId;
    int32_t      mapId         = 0;
    int32_t      startFrom     = 0;     // map seek-offset in milliseconds
    std::string  mode;
    bool         passed        = false;
    std::string  mods          = "[]";  // serialized mod array (game's format)
    bool         spin          = false;
    float        speed         = 0.0f;
    int64_t      totalScore    = 0;
    float        accuracy      = 0.0f;
    int32_t      hits          = 0;
    int32_t      misses        = 0;
    float        points        = 0.0f;
    int32_t      failTime      = -1;    // ms into the map where the player failed, or -1
    bool         failed        = false; // synthesised from failTime >= 0 (decode only)
};

struct Replay
{
    int32_t                  version = 0;
    ScoreData                scoreData;
    std::vector<ReplayFrame> frames;
};

// Version magics. The replay format is identified by a leading int32
// whose value is an integer-encoded date (YYYYMMDD).
//
// Decode handles each magic by:
//   < kVersionNegateY       : flips ReplayFrame.positionY on decode/encode
//                             (older builds stored Y with the opposite sign)
//   < kVersionExtendedFields: scoreData.{passed, mods, spin, speed,
//                             totalScore} aren't in the wire data; the
//                             decoder fills them with defaults and the
//                             encoder skips them
//   < kVersionFailTime      : scoreData.failTime isn't in the wire data;
//                             defaulted to -1 / failed=false
//   < kVersionInt32Time     : ReplayFrame.time is stored as float ms
//                             instead of int32 ms; the parser converts
//                             transparently on both decode and encode
constexpr int32_t kVersionNegateY        = 20260118;
constexpr int32_t kVersionExtendedFields = 20260125;
constexpr int32_t kVersionFailTime       = 20260222;
constexpr int32_t kVersionInt32Time      = 20260510;

// Decode `size` bytes at `data` into `out`. Returns false on any
// truncation or invalid length. On false return, `out` is left in an
// indeterminate state (may be partially populated).
bool Parse(const uint8_t* data, std::size_t size, Replay& out);

// Convenience overload.
bool Parse(const std::vector<uint8_t>& data, Replay& out);

// Encode `replay` into a fresh byte buffer. The output uses the wire
// format selected by `replay.version` — set it to the latest constant
// (kVersionInt32Time) for new files.
std::vector<uint8_t> Encode(const Replay& replay);

} // namespace rhr
