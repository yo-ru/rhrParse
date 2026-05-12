// rhrroundtrip — read a replay, encode it back, verify the bytes match.
// Useful sanity check after format changes.
//
//   rhrroundtrip path/to/replay.rhr

#include <rhrParse/rhrParse.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: rhrroundtrip <replay-file>\n");
        return 2;
    }

    std::ifstream f(argv[1], std::ios::binary);
    if (!f)
    {
        std::fprintf(stderr, "rhrroundtrip: cannot open '%s'\n", argv[1]);
        return 1;
    }
    std::vector<uint8_t> original((std::istreambuf_iterator<char>(f)),
                                   std::istreambuf_iterator<char>());

    rhr::Replay replay;
    if (!rhr::Parse(original, replay))
    {
        std::fprintf(stderr, "rhrroundtrip: parse failed\n");
        return 1;
    }

    auto encoded = rhr::Encode(replay);

    if (encoded.size() != original.size())
    {
        std::fprintf(stderr,
            "rhrroundtrip: SIZE MISMATCH (original=%zu, encoded=%zu)\n",
            original.size(), encoded.size());
        return 1;
    }

    for (std::size_t i = 0; i < original.size(); ++i)
    {
        if (original[i] != encoded[i])
        {
            std::fprintf(stderr,
                "rhrroundtrip: byte mismatch at offset %zu (orig=%02X new=%02X)\n",
                i, original[i], encoded[i]);
            return 1;
        }
    }

    std::printf("rhrroundtrip: OK (%zu bytes)\n", encoded.size());
    return 0;
}
