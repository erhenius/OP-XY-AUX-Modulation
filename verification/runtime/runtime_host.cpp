// Tests the real AUX GUI service, audio evaluator and microphone observer.
// Only stock OS I/O, the browser surface and the transplanted native Duck kernel are doubled.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <project_companion.hpp>
#include <aux_tracks.cpp>
#include <aux_lfo_runtime.cpp>

extern "C" {
opxy::u32 sdk_control_kept_write(void*) __asm__("sdk_control_kept_write");
void sdk_control_kept_load(const char*) __asm__("sdk_control_kept_load");
opxy::u32 sdk_project_clear(opxy::u32,opxy::u32,opxy::u32) __asm__("sdk_project_clear");
}
const opxy::platform::CompanionMachine sdk_companion_machines[] = {{nullptr,nullptr,nullptr,false}};

namespace {
const u32 project_address = 0x90000000u;
u32 project = project_address, loading, tab, edit_counter;
u8 selected, dirty, playing[2];
alignas(4) u8 project_data[0xd7c + 16 * 0x53f44];
u32 pattern[16];
i32 gui[16][58], audio_copy[16][58];
const u32 tempo = 0x40000000u;
opxy::i16 sine[8192], tilt;
u32 capture[2], line_input, forwards, old_publishes, cv_outputs, cv_clears, cv_gates, writes, duck_calls;
browser::Callbacks pending;
i32 selected_by_browser = -1;
u32 initial_row;
const u32 turn_address = 0x82001000;
i32 turn_event[3];
i32 observed_type, observed_amount;

void check(bool ok, const char* reason) {
    if (!ok) { std::fprintf(stderr, "FAIL %s\n", reason); std::exit(1); }
}

u32 block_at(u32 track, u32 p) { return project + 0xd7c + track * 0x53f44 + p * 0x53f4; }
u32 host_block(u32 track) { return block_at(track, pattern[track]); }
u32 &cell(u32 track, u32 offset) { return *reinterpret_cast<u32*>(project_data + (host_block(track) - project + offset)); }
void enable(u32 track, bool on) { project_data[host_block(track) - project + 0x24] = on; }
void host_set_engine(u32 track, u32 engine) { cell(track, 0x18) = engine; }
int ready() { return 1; }
void set_value(u32, u32 track, u32 slot, i32 value) {
    cell(track, 0x3864 + slot * 4) = static_cast<u32>(gui[track][slot] = value);
}
void set_live(u32, u32 track, u32 slot, i32 value) {
    gui[track][slot] = value;
    ++writes;
}
void send_to_audio() {
    // One-way transfer. Until this point the audio role can see only its old values.
    std::memcpy(audio_copy, gui, sizeof(gui));
}
opxy::i16 motion() { return tilt; }
u32 source() { return line_input; }
void old_input(u32, const i32*) { ++forwards; }
void old_output(u32) { ++cv_outputs; }
void clear_cv() { ++cv_clears; }
void gate(u32) { ++cv_gates; }
void tab_change(u32, u32 next) { tab = next; }
void original_publish(u32,u32,i32,i32) { ++old_publishes; }
u32 telemetry(u32,u32) { return 1; }
u32 selected_for_page() { return selected; }
void stock_tick() {}
u32 no_engine(u32) { return 0; }
u32 no_shift() { return 0; }
const opxy::u16* project_tempo() { static const opxy::u16 bpm=1200; return &bpm; }
i32 host_shown(u32,u32 track,u32 slot,u32) { return gui[track][slot]; }
void reset_control(u32) {}
void control_callback(u32,const control::Input* input,control::Overlay*) {
    // A runtime transport mark belongs to the adapter only.
    for (u32 t : {11u,14u,15u}) check(!aux_lfo::tagged(input->values[t][57]),"machine sees stored mark, not transport tag");
}
u32 stock_clear(u32,u32,u32) { return 91; }
void* open_file(const char* path,const char* mode) { return std::fopen(path,mode); }
u32 read_file(void* data,u32 size,u32 count,void* f) {
    return static_cast<u32>(std::fread(data,size,count,static_cast<FILE*>(f)));
}
u32 write_file(const void* data,u32 size,u32 count,void* f) {
    return static_cast<u32>(std::fwrite(data,size,count,static_cast<FILE*>(f)));
}
int close_file(void* f) { return std::fclose(static_cast<FILE*>(f)); }
void save_companion() {
    FILE* file=std::fopen("song.oxy","wb"); check(file,"create private companion");
    std::fputs(R"({"format":"open-xy-project","version":1,"project":{"bytes":9,"crc32":"cbf43926"},"sections":{)",file);
    check(sdk_control_kept_write(file)>0,"real companion writer saves AUX modes");
    std::fputs(R"("other":{}}})",file); std::fclose(file);
    file=std::fopen("song.xy","wb"); check(file,"create project fixture");
    std::fputs("123456789",file); std::fclose(file);
}

i32 native_ui_words[8];
const LfoVector native_view = {0, native_ui_words, native_ui_words + 8, native_ui_words + 8};
const LfoVector* host_draw_input(u32) { return &native_view; }

void check_display(u32 track, u32 mode) {
    const i32 page = mode == aux_lfo::Gyro || mode == aux_lfo::Mic ? 29 :
                     mode == aux_lfo::Duck || mode == aux_lfo::DuckNote ? 30 :
                     mode == aux_lfo::Random ? 28 :
                     mode >= aux_lfo::Sine && mode <= aux_lfo::Rising ? 27 : -1;
    check(sdk_aux_page() == page, "selected family must activate its original screen");
    for (u32 g=0; g<3; ++g) {
        i32 before[8] = {0x10000000,0x20000000,static_cast<i32>(g*0x30000000u),
                        0x70000000,0x40000001,0x50000002,0x60000003,0x70000004};
        std::memcpy(native_ui_words,before,sizeof before);
        for (i32 candidate=27; candidate<=30; ++candidate) {
            const LfoVector* v = lfo_display_vector(track, candidate);
            if (candidate != page) {
                check(v == &native_view, "unrelated stock draw must keep its native vector");
                continue;
            }
            const i32* w = v->begin;
            check(v != &native_view && v->end - w == 8 && w[1] == before[1], "private eight-word display view");
            if (page == 30) {
                for (int i=0; i<4; ++i) check(w[i] == before[i], "Duck's live controls must match");
                check(w[4] == (mode == aux_lfo::DuckNote ? 2147483647 : 0), "Duck trigger icon");
            } else if (page == 29) {
                check((w[0] >> 29) == (mode == aux_lfo::Mic) && static_cast<u32>(w[2] >> 29) == g,
                      "Element source illustration and destination");
            } else {
                check(((static_cast<u32>(w[2]) >> 8)*6 >> 23) == 2*g+1, "free-running group encoding");
                if (page == 27) check(static_cast<u32>(w[4] >> 29) == mode - aux_lfo::Sine, "Value waveform");
                else check(w[7] == before[3] && w[3] == 0x40000000, "Random slot and neutral envelope");
            }
            check(!std::memcmp(native_ui_words,before,sizeof before), "instrument vector must be untouched");
        }
    }
}
bool open_mode(u32 track, u32 expected) {
    selected = static_cast<u8>(track);
    pending = {}; selected_by_browser = -1;
    sdk_aux_lfo_shift_m4();
    if (selected_by_browser < 0) return false;
    check(selected_by_browser == static_cast<i32>(track) &&
          initial_row == family_of(expected) && pending.confirm, "modal must open on selected track and current row");
    return true;
}
void choose(u32 track, u32 prior, u32 mode) {
    check(open_mode(track, prior), "Shift+M4 opens the modulation picker");
    pending.confirm(pending.context, family_of(mode));
    // The selector chooses a family. Variant changes are real M4 control events.
    const u32 k1 = 10;
    if (mode == aux_lfo::Mic || mode == aux_lfo::Gyro) {
        turn_event[1] = mode == aux_lfo::Mic ? 1 : -1;
        check(sdk_aux_lfo_turn(kAuxController, 0, turn_address) == 1, "Element K1 rotates source");
    }
    for (u32 n = 0; editable_modulation(track) != mode && n < 4; ++n)
        check(sdk_aux_key(kAuxController,&k1) == 2, "Duck type or Value shape changes on K1 click");
    check(editable_modulation(track)==mode, "confirmed variant matches audible mode");
    check_display(track,mode);
}
aux_lfo::Frame eval(u32 track, i32 depth=0x40000000, u32 slot=2) {
    aux_lfo::Frame f = {track, audio_copy[track], depth, 7654321, slot};
    aux_lfo::sdk_aux_lfo_evaluate(&f);
    return f;
}
void tag_is(u32 track,u32 mode) {
    check(aux_lfo::tagged(audio_copy[track][57]) &&
          (static_cast<u32>(audio_copy[track][57]) & 15u) == mode, "wrong mode on audio role");
}
}

extern "C" {
const ControlMachine sdk_control_machines[] = {
    {reinterpret_cast<const char*>(40), ready, reset_control, control_callback, "other", 0, nullptr, nullptr,
     true, 1u << 11, reinterpret_cast<const char*>(120), {1,2,3,4}, true},
    {reinterpret_cast<const char*>(41), ready, reset_control, control_callback, "morphcapture", 0, nullptr, nullptr,
     true, 1u << 11, reinterpret_cast<const char*>(121), {1,2,3,4}, true}
};
#ifdef QA_STANDALONE
const u32 sdk_control_machine_count = 0;
#else
const u32 sdk_control_machine_count = 2;
#endif
bool (*sdk_control_turn)(u32,u32,const i32*);
u32 sdk_benchmark_cycles() __asm__("sdk_benchmark_cycles");
u32 sdk_benchmark_cycles() { return 555; }
u32 sdk_companion_reserved;

u32 (*sdk_aux_engine)(u32);
bool (*sdk_aux_owns_page)(u32);
const SynthControls sdk_synth_controls[] = {{}};
const u32 sdk_synth_control_count=0;
u32 sdk_effect_turn(u32,u32,u32,u32) { return 1; }
}

extern "C" void sdk_aux_duck_kernel(i32* kernel, unsigned slot, const i32* words) {
    observed_type=words[28]; observed_amount=words[25];
    check(slot == 0, "kernel must use private storage index");
    ++duck_calls;
    kernel[0x88 / 4] = -(1 << 30);
}

namespace opxy { namespace platform { namespace stock_browser { namespace detail {
bool open(Session&, Row*, u32 count, const char* title, const char*const* names, u32 initial, Callbacks cb, i32 track) {
    const char* expected[] = {"stock","duck","element","random","value"};
    check(count == 5 && std::strcmp(title,"lfo") == 0, "picker has one row per LFO");
    for (u32 i = 0; i < count; ++i) check(!std::strcmp(names[i],expected[i]), "LFO type order");
    pending = cb; selected_by_browser = track; initial_row = initial; return true;
}
bool open_rows(Session&,Row*,u32,const char*,u32,Callbacks,i32,bool) { return false; }
bool name(String&,const char*) { return false; }
}}}}

opxy::platform::Address opxy::platform::host_stock_address(Address address) {
    if (address == 0x8d4d7de8) return reinterpret_cast<Address>(&host_draw_input);
    if (address == 0x8fed0348) return reinterpret_cast<Address>(&open_file);
    if (address == 0x8fed05bc) return reinterpret_cast<Address>(&read_file);
    if (address == 0x8fed0780) return reinterpret_cast<Address>(&write_file);
    if (address == 0x8fed00ae) return reinterpret_cast<Address>(&close_file);
    if (address == 0x8d588114) return reinterpret_cast<Address>(&stock_clear);
    static const u32 ticks=17;
    if (address == 0x8d580074u) return reinterpret_cast<Address>(&stock_tick);
    if (address == kTicks96) return reinterpret_cast<Address>(&ticks);
    if (address == 0x8d58781cu) return reinterpret_cast<Address>(&no_engine);
    if (address == 0x8d584210u) return reinterpret_cast<Address>(&project_tempo);
    if (address == 0x8d5ac9dcu) return reinterpret_cast<Address>(&host_shown);
    if (address == 0x8d51f184u) return reinterpret_cast<Address>(&no_shift);
    if (address == 0x8d4d6f14u) return reinterpret_cast<Address>(&selected_for_page);
    if (address == turn_address) return reinterpret_cast<Address>(turn_event);
    if (address == kSelectedTrack) return reinterpret_cast<Address>(&selected);
    if (address == kAuxTab) return reinterpret_cast<Address>(&tab);
    if (address == 0x840a0524) return reinterpret_cast<Address>(&project);
    if (address == 0x840a0528) return reinterpret_cast<Address>(&loading);
    if (address == 0x840a052c) return reinterpret_cast<Address>(&dirty);
    if (address == 0x840a0530) return reinterpret_cast<Address>(&edit_counter);
    if (address >= project && address < project + sizeof(project_data)) return reinterpret_cast<Address>(project_data) + address-project;
    if (address >= kPlaying && address < kPlaying+2) return reinterpret_cast<Address>(playing) + address-kPlaying;
    const u32 live_base = kProject + 0x4080;
    if (address >= live_base && address < live_base + sizeof(gui)) return reinterpret_cast<Address>(gui) + address-live_base;
    const u32 audio_base = 0x84eab39c;
    if (address >= audio_base && address < audio_base+sizeof(audio_copy)) return reinterpret_cast<Address>(audio_copy) + address-audio_base;
    if (address == 0x8d587708) return reinterpret_cast<Address>(&host_block);
    if (address == 0x8d5877e4) return reinterpret_cast<Address>(&host_set_engine);
    if (address == 0x8d5acf28) return reinterpret_cast<Address>(&set_value);
    if (address == 0x8d5acea8) return reinterpret_cast<Address>(&set_live);
    if (address == 0x8c08f74c) return reinterpret_cast<Address>(&tab_change);
    if (address == 0x8d4d68b8) return reinterpret_cast<Address>(&original_publish);
    if (address == 0x8d4d666a) return reinterpret_cast<Address>(&telemetry);
    if (address == 0x8d4db5bc) return reinterpret_cast<Address>(&motion);
    if (address == 0x840426f4) return reinterpret_cast<Address>(&tempo);
    if (address == 0x84048aa4) return reinterpret_cast<Address>(sine);
    if (address == 0x8d4db364) return reinterpret_cast<Address>(&source);
    if (address == 0x8d51a3c8) return reinterpret_cast<Address>(&old_input);
    if (address == 0x08034f68 || address == 0x08034f6c) return reinterpret_cast<Address>(capture) + address-0x08034f68;
    if (address == 0x8d50f1cc) return reinterpret_cast<Address>(&old_output);
    if (address == 0x8d5b59a0) return reinterpret_cast<Address>(&clear_cv);
    if (address == 0x8d5b5f78) return reinterpret_cast<Address>(&gate);
    std::fprintf(stderr,"unmapped virtual stock address %lx\n", static_cast<unsigned long>(address)); std::abort();
}

int main() {
    for (u32 i = 0; i < 8192; ++i) sine[i] = static_cast<opxy::i16>(std::lround(32767*std::sin(i*std::acos(-1.0)/4096)));
    for (u32 t : {11u,14u,15u}) {
        enable(t,true);
        cell(t,0x53f40) = 2;
        gui[t][3] = gui[t][41] = 0x60000000;
        gui[t][25] = 0x30000000;
    }
    sdk_aux_startup();
    const u32 k1 = 10, m4 = 9;
    for (u32 active_tab = 0; active_tab < 4; ++active_tab) {
        tab = active_tab;
        for (u32 send : {14u, 15u}) {
            selected = static_cast<u8>(send);
            selected_by_browser = -1;
            check(sdk_aux_key(kAuxController, &k1) == 0 && selected_by_browser < 0,
                  "K1 delegates to the normal AUX page");
            check(sdk_aux_key(kAuxController, &m4) == 0 && selected_by_browser < 0,
                  "plain M4 navigates normally");
            check(open_mode(send,0) && tab == active_tab, "Shift+M4 opens from every AUX page");
        }
    }
    check(!open_mode(10,0) && !open_mode(11,0), "unrelated AUX and plain CV retain stock Shift+M4");
    // Exercise the real entry used before the no-control-machines early return.
    choose(14,0,aux_lfo::Gyro); sdk_gui_tick(); send_to_audio();
    tag_is(14,aux_lfo::Gyro);
    choose(14,aux_lfo::Gyro,aux_lfo::Stock); sdk_gui_tick(); send_to_audio();
    check(!aux_lfo::tagged(audio_copy[14][57]),"standalone FX can return to stock before AUX-machine service");
    if (!sdk_control_machine_count) {
        choose(15,0,aux_lfo::Square); sdk_gui_tick(); send_to_audio();
        check(eval(15).wave==2147418112,"extension-only collection evaluates FX II");
        std::puts("PASS real GUI dispatcher, zero control machines, both sends");
        return 0;
    }
    const u32 edits_before_morph=edit_counter;
    take(11,1);
    check(!open_mode(11,0), "other shared-LFO machine cannot claim Morph mode");
    take(11,2);
    for (u32 active_tab = 0; active_tab < 4; ++active_tab) {
        tab = active_tab;
        check(open_mode(11,0), "Morph also opens Shift+M4 from each page");
        selected_by_browser = -1;
        const u32 result = sdk_aux_key(kAuxController, &k1);
        check(result == (tab == 3 ? 0u : 2u) && selected_by_browser < 0,
              "normal Morph K1 follows its machine page or shared M4");
    }
    choose(11,0,aux_lfo::Gyro); choose(14,0,aux_lfo::Random); choose(15,0,aux_lfo::Mic);
    check(dirty && edit_counter==edits_before_morph+4, "accepted edits must dirty the project exactly once");
    choose(14,aux_lfo::Random,aux_lfo::Random);
    check(edit_counter==edits_before_morph+4, "confirm current mode is a no-op");
    sdk_gui_tick();
    check(eval(11).wave==7654321 && eval(14).wave==7654321, "audio must wait for publication");
    send_to_audio();
    tag_is(11,aux_lfo::Gyro); tag_is(14,aux_lfo::Random); tag_is(15,aux_lfo::Mic);
    const u32 edits_after_sources = edit_counter;
    check(open_mode(15,aux_lfo::Mic), "selected Element family highlighted for A2 mic mode");
    pending.confirm(pending.context,ElementFamily);
    check(editable_modulation(15)==aux_lfo::Mic && edit_counter==edits_after_sources,
          "reconfirming Element retains mic and does not dirty project");
    pending.confirm(pending.context,FamilyCount);
    check(editable_modulation(15)==aux_lfo::Mic && edit_counter==edits_after_sources, "invalid family ignored");
    const u32 old_writes=writes;
    sdk_gui_tick(); check(writes==old_writes,"steady GUI tick must not repeatedly publish");
    tilt = 1234; check(eval(11).wave == -1234*65536, "gyro polarity through both roles");
    sdk_cv_output(0); sdk_cv_output(0);
    check(cv_gates==1 && cv_clears==1 && cv_outputs==0,"Morph tag keeps stock CV silenced");
    check(eval(14).wave == 0, "fresh Random starts at zero");
    i32 block[128]; std::fill(block,block+128,0x40000000);
    capture[0]=7; check(aux_lfo_mic_input_lane()==0,"native internal lane"); aux_lfo_observe_mic(14,block);
    check(eval(15).wave>0 && forwards==1,"live internal mic is consumed and native input still forwarded");
    ++capture[0]; check(eval(15).wave==0,"stale mic data clears");
    line_input=1; check(aux_lfo_mic_input_lane()==1,"native external lane"); aux_lfo_observe_mic(14,block); check(eval(15).wave==0 && forwards==2,"line input remains excluded");
    for (u32 i=0;i<3;++i) eval(14);
    const i32 continuing=eval(14).wave;
    check(continuing!=0,"independent FX I Random reaches first wrap");
    choose(15,aux_lfo::Mic,aux_lfo::Duck); sdk_gui_tick(); send_to_audio();
    selected = 15; selected_by_browser = -1;
    const u32 edits_pre_duck_click = edit_counter;
    check(sdk_aux_key(kAuxController,&k1)==2 && editable_modulation(15)==aux_lfo::DuckNote &&
          selected_by_browser<0 && edit_counter==edits_pre_duck_click+1,"Duck K1 switches to note with no browser");
    check(open_mode(15,aux_lfo::DuckNote),"Duck source represented by one row");
    pending.confirm(pending.context,DuckFamily);
    check(editable_modulation(15)==aux_lfo::DuckNote, "reselecting Duck retains note");
    check(sdk_aux_key(kAuxController,&k1)==2 && editable_modulation(15)==aux_lfo::Duck,"Duck K1 returns to audio");
    const aux_lfo::Frame duck=eval(15);
    check(duck.slot==41 && duck.wave<0 && duck.depth==2147483647 && duck_calls==1,"Duck FX controls route to return");
    check(observed_amount==0x30000000 && observed_type==0,"Duck audio uses unipolar raw control");
    check(eval(15,-0x40000000).wave==duck.wave,"Duck keeps attenuation with signed stock-depth input");
    audio_copy[15][25]=0; check(eval(15).wave==0,"Duck zero raw amount is clean");
    choose(11,aux_lfo::Gyro,aux_lfo::DuckNote); sdk_gui_tick(); send_to_audio();
    gui[11][3]=0x60000000; send_to_audio();
    check(eval(11).slot==3 && eval(11).wave<0 && observed_type==2147483647,"Morph note duck uses movement slot");
    for (u32 s=aux_lfo::Sine;s<=aux_lfo::Rising;++s) {
        choose(15,s==aux_lfo::Sine?aux_lfo::Duck:s-1,s); sdk_gui_tick(); send_to_audio();
        const i32 first=eval(15).wave;
        const i32 expected=(s==aux_lfo::Square || s==aux_lfo::Falling) ? 2147418112 : 0;
        check(first==expected,"shape change primes phase at zero");
    }
    check(eval(14).wave!=0,"changing a neighboring source must preserve active RNG progress");
    const i32 tag_before=audio_copy[14][57];
    enable(14,false); sdk_gui_tick(); send_to_audio();
    check(!aux_lfo::tagged(audio_copy[14][57]) && eval(14).wave==7654321,"disable returns stock words");
    enable(14,true); sdk_gui_tick(); send_to_audio();
    check(tag_before!=audio_copy[14][57] && eval(14).wave==0,"reenabling resets this voice");
    check(open_mode(14,aux_lfo::Random),"can open pending modal");
    pattern[14]=1; enable(14,true); pending.confirm(pending.context,ElementFamily);
    sdk_gui_tick(); send_to_audio();
    check(eval(14).wave==7654321 && open_mode(14,0),"stale modal cannot modify another pattern");
    choose(14,0,aux_lfo::Square); sdk_gui_tick(); send_to_audio();
    check(eval(14).wave==2147418112,"per-pattern mode selection works");
    pattern[14]=0; sdk_gui_tick(); send_to_audio();
    check(open_mode(14,aux_lfo::Random) && eval(14).wave==0,"returning to pattern restores mode with fresh phase");
    loading=1; pending.confirm(pending.context,ElementFamily); sdk_gui_tick(); send_to_audio();
    check(!open_mode(14,0) && eval(14).wave==7654321,"project load suppresses stale selections");
    loading=0; save_companion();
    check(sdk_project_clear(0,0,0)==91,"real project clear preserves stock return");
    sdk_gui_tick(); send_to_audio();
    check(open_mode(14,0) && eval(14).wave==7654321,"a new project restores stock");
    sdk_control_kept_load("song.xy"); sdk_gui_tick(); send_to_audio();
    check(open_mode(14,aux_lfo::Random) && eval(14).wave==0,"authentic project read restores selected mode with fresh phase");
    tag_is(11,aux_lfo::DuckNote); tag_is(15,aux_lfo::Rising);
    pattern[14]=1; sdk_gui_tick(); send_to_audio();
    check(open_mode(14,aux_lfo::Square),"noncurrent pattern survives save and load");
    // Same byte count, wrong checksum. Reject the companion as stock does.
    FILE* replaced=std::fopen("song.xy","wb"); check(replaced,"open changed project");
    std::fputs("123456780",replaced); std::fclose(replaced);
    sdk_control_kept_load("song.xy"); sdk_gui_tick(); send_to_audio();
    check(open_mode(14,0) && eval(15).wave==7654321,"mismatched companion restores stock");
    aux_lfo::Frame invalid={10,audio_copy[14],123,456,9};
    aux_lfo::sdk_aux_lfo_evaluate(&invalid);
    check(invalid.wave==456 && invalid.depth==123 && invalid.slot==9,"unrelated track passes through");
    for (i32 unrecognized : {aux_lfo::token(345,15), aux_lfo::token(1,0),-1}) {
        audio_copy[15][57]=unrecognized;
        const auto f=eval(15);
        check(f.wave==7654321 && f.depth==0x40000000 && f.slot==2,"unknown source passes through");
    }
    std::puts("PASS integrated GUI and audio roles, independent destinations, all sources, lifecycle, stock fallback");
}
