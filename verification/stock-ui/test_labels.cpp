// Link with the SDK's lfo.cpp and its upstream lfo test doubles. This initial
// test runner finishes before the original regression fixture enters main.
#include "lfo.cpp.fixture"
#include "aux_lfo_common.hpp"
#include <cstring>
static u32 screen_mode, aux_names;
extern "C" u32 sdk_aux_lfo_screen_mode(u32 track) {
    require(track == expected_track);
    return screen_mode;
}
namespace opxy { namespace platform { namespace lfo {
const char* aux_name(u32 track, u32 group, u32 slot) {
    require(track == expected_track && group < 3 && slot < 4);
    ++aux_names;
    named_group = group;
    named_slot = slot;
    return "aux destination";
}
}}}
struct LabelSuite {
    LabelSuite() {
        for (u32 t : {0u, 8u, 10u, 11u, 14u, 15u, 16u}) {
            expected_track = t;
            for (u32 mode = 0; mode < aux_lfo::SourceCount; ++mode) {
                screen_mode = mode;
                const bool eligible = t == 11 || t == 14 || t == 15;
                const bool own = eligible && (mode == aux_lfo::Gyro || mode == aux_lfo::Mic ||
                                  (mode >= aux_lfo::Sine && mode <= aux_lfo::Random));
                for (u32 g : {0u, 1u, 2u, 3u, 0xffffffffu}) {
                    for (u32 s : {0u, 1u, 2u, 3u, 4u, 0xffffffffu}) {
                        const u32 before = aux_names;
                        const char* result = sdk_lfo_element_label(g, s, 1, 101);
                        if (!own) {
                            require(!std::strcmp(result, "slot name"));
                        } else if (g < 3 && s < 4) {
                            require(!std::strcmp(result, "aux destination"));
                            require(named_group == g && named_slot == s);
                        } else {
                            require(!std::strcmp(result, "-"));
                        }
                        require(aux_names == before + (own && g < 3 && s < 4 ? 1u : 0u));
                    }
                }
            }
        }
        screen_mode = 0;
        expected_track = 0;
    }
};
static LabelSuite test_labels;
