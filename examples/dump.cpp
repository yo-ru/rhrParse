// rhrdump — print a summary of a Rhythia replay file.
//
//   rhrdump path/to/replay.rhr

#include <rhrParse/rhrParse.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: rhrdump <replay-file>\n");
        return 2;
    }

    std::ifstream f(argv[1], std::ios::binary);
    if (!f)
    {
        std::fprintf(stderr, "rhrdump: cannot open '%s'\n", argv[1]);
        return 1;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());

    rhr::Replay replay;
    if (!rhr::Parse(bytes, replay))
    {
        std::fprintf(stderr, "rhrdump: parse failed (truncated or invalid)\n");
        return 1;
    }

    const auto& sd = replay.scoreData;
    std::printf("Version:     %d\n",   replay.version);
    std::printf("Played at:   %s\n",   sd.datePlayed.c_str());
    std::printf("Player:      %s\n",   sd.playerName.c_str());
    std::printf("Map:         %s (id %d)\n", sd.legacyMapId.c_str(), sd.mapId);
    if (!sd.beatmapHash.empty())
        std::printf("Beatmap hash:%s\n", sd.beatmapHash.c_str());
    std::printf("Mode:        %s\n",   sd.mode.c_str());
    std::printf("Mods:        %s\n",   sd.mods.c_str());
    std::printf("Speed:       %.2fx\n", sd.speed);
    std::printf("Score:       %lld\n", static_cast<long long>(sd.totalScore));
    std::printf("Accuracy:    %.2f%%\n", sd.accuracy);
    std::printf("Hits/Misses: %d / %d\n", sd.hits, sd.misses);
    std::printf("Passed:      %s\n", sd.passed ? "yes" : "no");
    if (sd.failed)
        std::printf("Failed at:   %d ms\n", sd.failTime);
    std::printf("Frames:      %zu\n", replay.frames.size());

    return 0;
}
