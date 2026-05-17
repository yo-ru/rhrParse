// rhrParse — Rhythia replay parser/encoder.
// See include/rhrParse/rhrParse.h for the public API.
// License: MIT.

#include "rhrParse/rhrParse.h"

#include <cstring>
#include <ctime>

namespace rhr {
namespace {

// .NET DateTime → UTC seconds since the Unix epoch.
//   Ticks are 100ns since 0001-01-01.
//   The Unix epoch (1970-01-01) sits at 621355968000000000 ticks.
constexpr int64_t kTicksPerSecond = 10'000'000LL;
constexpr int64_t kUnixEpochTicks = 621355968000000000LL;

// 7-bit LEB128 (a.k.a. .NET BinaryWriter `Write7BitEncodedInt`).
// Used for string-length prefixes. Sequence terminates on the first
// byte with the high bit clear; lower 7 bits of each byte form the
// little-endian payload.
constexpr uint8_t  kVarintContinueBit = 0x80;
constexpr uint8_t  kVarintPayloadMask = 0x7F;
constexpr uint32_t kMaxStringLen      = 1u << 24; // 16 MiB sanity cap

class Reader
{
public:
    Reader(const uint8_t* d, std::size_t s) : data(d), size(s) {}

    bool has(std::size_t n) const { return pos + n <= size; }

    uint8_t readU8()
    {
        if (!has(1)) { ok = false; return 0; }
        return data[pos++];
    }

    int32_t readInt32()
    {
        if (!has(4)) { ok = false; return 0; }
        int32_t v;
        std::memcpy(&v, data + pos, 4);
        pos += 4;
        return v;
    }

    int64_t readInt64()
    {
        if (!has(8)) { ok = false; return 0; }
        int64_t v;
        std::memcpy(&v, data + pos, 8);
        pos += 8;
        return v;
    }

    float readFloat()
    {
        if (!has(4)) { ok = false; return 0.0f; }
        float v;
        std::memcpy(&v, data + pos, 4);
        pos += 4;
        return v;
    }

    bool readBool() { return readU8() != 0; }

    std::string readString()
    {
        uint32_t len   = 0;
        int      shift = 0;
        while (true)
        {
            if (!has(1)) { ok = false; return {}; }
            uint8_t b = data[pos++];
            len |= static_cast<uint32_t>(b & kVarintPayloadMask) << shift;
            if ((b & kVarintContinueBit) == 0) break;
            shift += 7;
            if (shift >= 35) { ok = false; return {}; }
        }
        if (len > kMaxStringLen) { ok = false; return {}; }
        if (!has(len)) { ok = false; return {}; }
        std::string s(reinterpret_cast<const char*>(data + pos), len);
        pos += len;
        return s;
    }

    bool valid() const { return ok; }

private:
    const uint8_t* data;
    std::size_t    size;
    std::size_t    pos = 0;
    bool           ok  = true;
};

class Writer
{
public:
    void writeU8(uint8_t v) { buf.push_back(v); }

    void writeInt32(int32_t v)
    {
        const auto* p = reinterpret_cast<const uint8_t*>(&v);
        buf.insert(buf.end(), p, p + 4);
    }

    void writeInt64(int64_t v)
    {
        const auto* p = reinterpret_cast<const uint8_t*>(&v);
        buf.insert(buf.end(), p, p + 8);
    }

    void writeFloat(float v)
    {
        const auto* p = reinterpret_cast<const uint8_t*>(&v);
        buf.insert(buf.end(), p, p + 4);
    }

    void writeBool(bool v) { writeU8(v ? 1 : 0); }

    void writeString(const std::string& s)
    {
        auto v = static_cast<uint32_t>(s.size());
        while (v > kVarintPayloadMask)
        {
            buf.push_back(static_cast<uint8_t>((v & kVarintPayloadMask) | kVarintContinueBit));
            v >>= 7;
        }
        buf.push_back(static_cast<uint8_t>(v));
        buf.insert(buf.end(), s.begin(), s.end());
    }

    std::vector<uint8_t> take() { return std::move(buf); }

private:
    std::vector<uint8_t> buf;
};

// Pretty-print a .NET DateTime tick count as local time.
std::string formatDateTime(int64_t ticks)
{
    const int64_t unixSec = (ticks - kUnixEpochTicks) / kTicksPerSecond;
    if (unixSec <= 0) return {};
    auto t = static_cast<std::time_t>(unixSec);
    std::tm tm{};
#ifdef _WIN32
    if (localtime_s(&tm, &t) != 0) return {};
#else
    if (!localtime_r(&t, &tm)) return {};
#endif
    char buf[64];
    if (std::strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S", &tm) == 0) return {};
    return buf;
}

} // namespace

bool Parse(const std::vector<uint8_t>& data, Replay& out)
{
    return Parse(data.data(), data.size(), out);
}

bool Parse(const uint8_t* data, std::size_t size, Replay& out)
{
    if (!data || size < 4) return false;

    out    = {};
    Reader r(data, size);

    out.version = r.readInt32();
    auto& sd    = out.scoreData;

    sd.timestamp  = r.readInt64();
    sd.datePlayed = formatDateTime(sd.timestamp);
    sd.playerName = r.readString();
    sd.legacyMapId= r.readString();
    sd.mapId      = r.readInt32();
    sd.startFrom  = r.readInt32();
    sd.mode       = r.readString();

    if (out.version >= kVersionExtendedFields)
    {
        sd.passed     = r.readBool();
        sd.mods       = r.readString();
        sd.spin       = r.readBool();
        sd.speed      = r.readFloat();
        sd.totalScore = r.readInt64();
    }
    else
    {
        sd.passed     = true;
        sd.mods       = "[]";
        sd.spin       = false;
        sd.speed      = 0.0f;
        sd.totalScore = 0;
    }

    sd.accuracy = r.readFloat();
    sd.hits     = r.readInt32();
    sd.misses   = r.readInt32();
    sd.points   = r.readFloat();

    if (out.version >= kVersionFailTime)
    {
        sd.failTime = r.readInt32();
        sd.failed   = sd.failTime >= 0;
    }

    if (out.version >= kVersionBeatmapHash)
        sd.beatmapHash = r.readString();

    if (!r.valid()) return false;

    int32_t frameCount = r.readInt32();
    if (frameCount < 0) return false;
    if (!r.has(static_cast<std::size_t>(frameCount) * ReplayFrame::kWireSize)) return false;

    const bool int32Time = out.version >= kVersionInt32Time;
    out.frames.resize(static_cast<std::size_t>(frameCount));
    for (auto& f : out.frames)
    {
        f.time             = int32Time
                                ? r.readInt32()
                                : static_cast<int32_t>(r.readFloat());
        f.positionX        = r.readFloat();
        f.positionY        = r.readFloat();
        f.health           = r.readFloat();
        f.isImportantFrame = r.readU8();
    }

    if (!r.valid()) return false;

    if (out.version < kVersionNegateY)
    {
        for (auto& f : out.frames) f.positionY = -f.positionY;
    }

    return true;
}

std::vector<uint8_t> Encode(const Replay& replay)
{
    Writer w;

    w.writeInt32(replay.version);

    const auto& sd = replay.scoreData;
    w.writeInt64(sd.timestamp);
    w.writeString(sd.playerName);
    w.writeString(sd.legacyMapId);
    w.writeInt32(sd.mapId);
    w.writeInt32(sd.startFrom);
    w.writeString(sd.mode);

    if (replay.version >= kVersionExtendedFields)
    {
        w.writeBool(sd.passed);
        w.writeString(sd.mods);
        w.writeBool(sd.spin);
        w.writeFloat(sd.speed);
        w.writeInt64(sd.totalScore);
    }

    w.writeFloat(sd.accuracy);
    w.writeInt32(sd.hits);
    w.writeInt32(sd.misses);
    w.writeFloat(sd.points);

    if (replay.version >= kVersionFailTime)
        w.writeInt32(sd.failed ? sd.failTime : -1);

    if (replay.version >= kVersionBeatmapHash)
        w.writeString(sd.beatmapHash);

    w.writeInt32(static_cast<int32_t>(replay.frames.size()));

    const bool negateY   = replay.version < kVersionNegateY;
    const bool int32Time = replay.version >= kVersionInt32Time;
    for (const auto& f : replay.frames)
    {
        if (int32Time) w.writeInt32(f.time);
        else           w.writeFloat(static_cast<float>(f.time));
        w.writeFloat(f.positionX);
        w.writeFloat(negateY ? -f.positionY : f.positionY);
        w.writeFloat(f.health);
        w.writeU8(f.isImportantFrame);
    }

    return w.take();
}

} // namespace rhr
